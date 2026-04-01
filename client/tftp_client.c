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
#include <math.h>
#include <fcntl.h>

typedef struct {
    int sock_fd;
    struct sockaddr_in server_info;
    char server_ip[16]; 
} tftp_client_t;

char ip_address[30];
int port;
char filename[20];
int put_fd;
int put_size = 512;
tftp_mode mode = NORMAL;

void connect_to_server(tftp_client_t *client, char *ip, int port);
void put_file(tftp_client_t *client, char *filename);
void get_file(tftp_client_t *client, char *filename);
int process_command(tftp_client_t *client, char *command);
int send_request(tftp_client_t *client, char *filename, int opcode);
int receive_request(tftp_client_t *client, char *filename, int opcode);

int main() {

    char command[256];
    tftp_client_t client;
    memset(&client, 0, sizeof(client));  // Initialize client structure
    // Main loop for command-line interface
    while (1) {
        int opt;
        printf("1.Connect\n2.put\n3.Get\n4.Mode\n5.Exit\nPlease Select the Option: ");
        scanf(" %d",&opt);
        getchar();
        switch(opt){
            case 1:
                printf("Enter the IP address: ");
                scanf(" %[^\n]",ip_address);
                printf("Enter the port number: ");
                scanf("%d",&port);
                getchar();
                int flag = 0;
                // ip address validation
                int dot_cnt = 0;
                for(int i = 0 ; ip_address[i]; i++){
                    if(ip_address[i] == '.') dot_cnt++;
                }
                if(dot_cnt > 3 || dot_cnt < 3){
                    flag = 1;
                    printf("Invalid IP address\n");
                    continue;
                }
                // port no. validation
                if(port > 65535 || port < 1024){
                    flag = 1;
                    printf("Invalid port number\nTry Again\n");
                    continue;
                }
                if(!flag) connect_to_server(&client,ip_address,port);
                break;
            
            case 2:
                printf("Enter the file name to transfer: ");
                fgets(filename,20,stdin);
                filename[strcspn(filename,"\n")] = 0;
                if(process_command(&client,filename) == -1){
                    printf("Error: File does not exists\n");
                    continue;
                }
                if(send_request(&client,filename,WRQ)) put_file(&client,filename);
                else printf("Server denied the write(WRQ) request\nTry Again\n");
                break;
            case 3:
                printf("Enter the file name to transfer: ");
                fgets(filename,20,stdin);
                filename[strcspn(filename,"\n")] = 0;
                if(receive_request(&client,filename,RRQ)) get_file(&client,filename);
                else printf("Server denied the read(RRQ) request\nTry Again\n");
                break;
            case 4:
                printf("1.OCTAL\n2.NORMAL\n3.NETASCII\nEnter the mode: ");
                int op;
                scanf("%d",&op);
                if(op < 0 || op > 4){
                    printf("Invalid option, by default NORMAL mode has been given\n");
                    continue;
                }
                switch(op){
                    case 1:
                        mode = OCTAL;
                        put_size = 1;
                        printf("OCTAL mode has been selected\n");
                        break;
                    case 2:
                        mode = NORMAL;
                        put_size = 512;
                        printf("NORMAL mode has been selected\n");
                        break;
                    case 3:
                        mode = NETASCII;
                        put_size = 1;
                        printf("NETASCII mode has been selected\n");
                        break;
                } 
                break; 
            case 5:
                printf("Exiting...\n");
                exit(0);
        }
        
    }
    return 0;
}

// Function to process commands
int process_command(tftp_client_t *client, char *command) {
   put_fd = open(command, O_RDONLY);
   return put_fd;
}

// This function is to initialize socket with given server IP, no packets sent to server in this function
void connect_to_server(tftp_client_t *client, char *ip, int port){
    // Create UDP socket
    client->sock_fd = socket(AF_INET,SOCK_DGRAM,0);
        if(client->sock_fd == -1){
                perror("socket");
                return;
        }
    memset(&client->server_info, 0, sizeof(client->server_info));
    client->server_info.sin_family = AF_INET;
    client->server_info.sin_port = htons(port);
    client->server_info.sin_addr.s_addr = inet_addr(ip);
    
    strcpy(client->server_ip, ip);
    printf("Connection initialized to %s:%d\n", ip, port);
}
void put_file(tftp_client_t *client, char *filename) {
    // this function is for sending the file to server by selected mode no. of bytes
    tftp_packet put;
    int block = 1;
    socklen_t addr_len;
    int finished = 0;

    unsigned char char_in;
    int bytes_in_packet;

    printf("Starting transfer: %s\n", filename);

    do {
        memset(&put, 0, sizeof(put));
        put.opcode = htons(DATA);
        put.body.data_packet.block_number = htons(block);
        bytes_in_packet = 0;
        while (bytes_in_packet < 512) {
            int r = read(put_fd, &char_in, 1);
            if (r <= 0) {
                finished = 1;
                break;
            }
            if (mode == NETASCII && char_in == '\n') {
                put.body.data_packet.data[bytes_in_packet++] = '\r';
                if (bytes_in_packet == 512) {
                    lseek(put_fd, -1, SEEK_CUR); 
                    break;
                }
            }
            put.body.data_packet.data[bytes_in_packet++] = char_in;
            if (mode == OCTAL) break;
        }
        int acked = 0;
        while (!acked) {  // do until receiving ack from the servrer
            addr_len = sizeof(client->server_info);
            sendto(client->sock_fd, &put, bytes_in_packet + 4, 0, 
                   (struct sockaddr *)&client->server_info, addr_len);

            char ack_buf[18];
            int r_recv = recvfrom(client->sock_fd, ack_buf, sizeof(ack_buf), 0, (struct sockaddr *)&client->server_info, &addr_len);
            
            if (r_recv >= 4) {
                tftp_packet *ack_pkt = (tftp_packet *)ack_buf;
                if (ntohs(ack_pkt->opcode) == ACK && ntohs(ack_pkt->body.ack_packet.block_number) == block) {
                    acked = 1;
                    block++;
                }
            }
        }
    } while (!finished || bytes_in_packet == (mode == OCTAL ? 1 : 512));

    printf("Transfer of %s finished.\n", filename);
    close(put_fd);
}

void get_file(tftp_client_t *client, char *filename) {
    // this function is for receiving the file from server
    int get_fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (get_fd == -1) { perror("Failed to create local file"); return; }

    tftp_packet data_pkt;
    tftp_packet ack_pkt;
    socklen_t addr_len;
    int expected_block = 1;
    int n_received;

    do {
        addr_len = sizeof(client->server_info);
        n_received = recvfrom(client->sock_fd, &data_pkt, sizeof(data_pkt), 0, 
                              (struct sockaddr *)&client->server_info, &addr_len);

        if (n_received >= 4 && ntohs(data_pkt.opcode) == DATA) {
            int current_block = ntohs(data_pkt.body.data_packet.block_number);

            if (current_block == expected_block) {
                write(get_fd, data_pkt.body.data_packet.data, n_received - 4);
                ack_pkt.opcode = htons(ACK);
                ack_pkt.body.ack_packet.block_number = htons(current_block);
                sendto(client->sock_fd, &ack_pkt, 4, 0, (struct sockaddr *)&client->server_info, addr_len);
                
                expected_block++;
            }
        }
    } while (n_received == 516);

    printf("Download of %s finished.\n", filename);
    close(get_fd);
}

int send_request(tftp_client_t *client, char *filename, int opcode)
{
    // this function is for sending the write request to the server
    tftp_packet wr;
    socklen_t addr_len = sizeof(client->server_info);
    memset(&wr, 0, sizeof(wr));
    wr.opcode = htons(opcode);
    strncpy(wr.body.request.filename, filename, 255);
    wr.body.request.transmission_mode = htons((uint16_t)mode);
    sendto(client->sock_fd,&wr,sizeof(wr),0, (struct sockaddr *)&client->server_info, sizeof(client->server_info));
    char ack[18];
    int n = recvfrom(client->sock_fd,ack,18,0,(struct sockaddr *)&client->server_info,  &addr_len);
    if (n < 0) return 0;
    tftp_packet *ack_ptr = (tftp_packet *)ack;
    return (ntohs(ack_ptr->opcode) == ACK && ntohs(ack_ptr->body.ack_packet.block_number) == 0);
}
int receive_request(tftp_client_t *client, char *filename, int opcode) {
    // this function is for sending the rrq to the server
    tftp_packet rr;
    socklen_t addr_len = sizeof(client->server_info);
    memset(&rr, 0, sizeof(rr));
    rr.opcode = htons(RRQ);
    strncpy(rr.body.request.filename, filename, strlen(filename)+1);
    rr.body.request.transmission_mode = htons((uint16_t)mode);
    sendto(client->sock_fd, &rr, sizeof(rr), 0, (struct sockaddr *)&client->server_info, addr_len);
    char ack_buf[18];
    int n = recvfrom(client->sock_fd, ack_buf, 18, 0, (struct sockaddr *)&client->server_info, &addr_len);

    if (n >= 4) {
        tftp_packet *pkt = (tftp_packet *)ack_buf;
        if (ntohs(pkt->opcode) == ACK && ntohs(pkt->body.ack_packet.block_number) == 0) {
            tftp_packet start_ack;
            start_ack.opcode = htons(ACK);
            start_ack.body.ack_packet.block_number = htons(0);
            sendto(client->sock_fd, &start_ack, 4, 0, (struct sockaddr *)&client->server_info, addr_len);
            return 1; // Success
        }
    }
    return 0; // File not found or server denied
}
