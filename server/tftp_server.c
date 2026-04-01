/*
Name    : Sakthivel Arumugam
Date    : 21/02/2026
Title   : TFTP using UDP protocl
*/

#include "tftp.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>

// Function prototype for handle_client
void handle_client(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet);
void receive_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, char *filename, uint16_t t_mode);
void send_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, char *filename, uint16_t t_mode);

int main() {
    int sock_fd;
    struct sockaddr_in server_info, client_addr;
    socklen_t client_len = sizeof(client_addr);
    tftp_packet packet;

    // 1. Create UDP Socket
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_fd == -1) {
        perror("Socket creation failed");
        return -1;
    }

    // 2. Initialize Server Address
    memset(&server_info, 0, sizeof(server_info));
    server_info.sin_family = AF_INET;
    server_info.sin_port = htons(PORT); // Port 6969 from tftp.h
    server_info.sin_addr.s_addr = "127.0.0.1";  // normal loopback ip address

    // 3. Bind Socket to Port
    if (bind(sock_fd, (struct sockaddr *)&server_info, sizeof(server_info)) < 0) {
        perror("Bind failed");
        close(sock_fd);
        return -1;
    }

    printf("TFTP Server listening on port %d...\n", PORT);

    // 4. Main Server Loop
    while (1) {
        client_len = sizeof(client_addr);
        memset(&packet, 0, sizeof(packet));

        // Receive Request (RRQ or WRQ)
        int n = recvfrom(sock_fd, &packet, sizeof(packet), 0, (struct sockaddr *)&client_addr, &client_len);
        
        if (n < 0) {
            perror("Receive failed");
            continue;
        }

        // Process the client request
        handle_client(sock_fd, client_addr, client_len, &packet);
    }

    close(sock_fd);
    return 0;
}

void handle_client(int sock_fd, struct sockaddr_in client_addr, socklen_t client_len, tftp_packet *packet) 
{
    uint16_t opcode = ntohs(packet->opcode);
    char *filename = packet->body.request.filename;
    
    // Capture the mode sent by the client
    uint16_t t_mode = ntohs(packet->body.request.transmission_mode);

    if (opcode == RRQ) {
        send_file(sock_fd, client_addr, client_len, filename, t_mode);
    } 
    else if (opcode == WRQ) {
        receive_file(sock_fd, client_addr, client_len, filename, t_mode);
    }
}

void receive_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, char *filename, uint16_t t_mode) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1) { perror("File open failed"); return; }
    int expected_packet_size = (t_mode == OCTAL) ? 5 : 516; 
    tftp_packet ack_pkt, data_pkt;
    int expected_block = 1;
    int n_received;

    // 1. Send Initial ACK 0 to confirm WRQ
    ack_pkt.opcode = htons(ACK);
    ack_pkt.body.ack_packet.block_number = htons(0);
    sendto(sockfd, &ack_pkt, 4, 0, (struct sockaddr *)&client_addr, client_len);

    // 2. Receive Data Loop
    do {
        n_received = recvfrom(sockfd, &data_pkt, sizeof(data_pkt), 0, (struct sockaddr *)&client_addr, &client_len);
        
        if (n_received >= 4 && ntohs(data_pkt.opcode) == DATA) {
            uint16_t block = ntohs(data_pkt.body.data_packet.block_number);
            
            if (block == expected_block) {
                write(fd, data_pkt.body.data_packet.data, n_received - 4);
                
                // Send ACK for this block
                ack_pkt.body.ack_packet.block_number = htons(block);
                sendto(sockfd, &ack_pkt, 4, 0, (struct sockaddr *)&client_addr, client_len);
                expected_block++;
            }
        }
    } while (n_received == expected_packet_size);  // Continue if full packet received

    printf("File %s received successfully.\n", filename);
    close(fd);
}

void send_file(int sockfd, struct sockaddr_in client_addr, socklen_t client_len, char *filename, uint16_t t_mode) {
    int fd = open(filename, O_RDONLY);
    int read_limit = (t_mode == OCTAL) ? 1 : 512;
    if (fd == -1) {
        tftp_packet err;
        err.opcode = htons(ERROR);
        err.body.error_packet.error_code = htons(1); // 1 = File not found
        strcpy(err.body.error_packet.error_msg, "File not found");
        sendto(sockfd, &err, strlen(err.body.error_packet.error_msg) + 5, 0, 
            (struct sockaddr *)&client_addr, client_len);
        return; // Exit function so server doesn't hang
    }

    tftp_packet data_pkt, ack_pkt;
    int n_read, block = 1;

    // 1. Send ACK 0 to confirm RRQ exists
    ack_pkt.opcode = htons(ACK);
    ack_pkt.body.ack_packet.block_number = htons(0);
    sendto(sockfd, &ack_pkt, 4, 0, (struct sockaddr *)&client_addr, client_len);

    // 2. Wait for Client's "Ready" ACK 0
    recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr, &client_len);

    // 3. Send Data Loop
    do {
        n_read = read(fd, data_pkt.body.data_packet.data, 512);
        data_pkt.opcode = htons(DATA);
        data_pkt.body.data_packet.block_number = htons(block);

        int acked = 0;
        while (!acked) {
            sendto(sockfd, &data_pkt, n_read + 4, 0, (struct sockaddr *)&client_addr, client_len);
            
            int r = recvfrom(sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr, &client_len);
            if (r >= 4 && ntohs(ack_pkt.opcode) == ACK && ntohs(ack_pkt.body.ack_packet.block_number) == block) {
                acked = 1;
                block++;
            }
        }
    } while (n_read == read_limit);

    printf("File %s sent successfully.\n", filename);
    close(fd);
}
