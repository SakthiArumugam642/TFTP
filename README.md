**TFTP Server (UDP Protocol)
Overview**
The server acts as the central hub, listening on port 6969. It manages concurrent-like requests for reading and writing files. It uses a custom tftp_packet structure to ensure data is packed correctly for network transmission.

**Server Features**
Loopback Binding: Configured to run on 127.0.0.1 for local testing.
Request Handling: Automatically identifies if a client wants to GET (RRQ) or PUT (WRQ) a file.
Reliability: Implements a Wait-for-ACK mechanism. It won't send the next data block until the current one is acknowledged by the client.
Error Reporting: Sends formal TFTP Error Packets (OpCode 5) if a requested file is missing.
Mode Awareness: Adapts block sizes based on whether the client selects OCTAL (1-byte) or NORMAL (512-byte) mode.

**File Transfer Logic**
**Receive File (WRQ):**
Sends ACK 0 to signal it's ready to receive.
Writes incoming data blocks to a local file.
Sends an ACK for every block number received.
**Send File (RRQ):**
Verifies file existence; sends an error packet if not found.
Sends ACK 0, then waits for a "Ready" ACK from the client.
Streams the file in blocks until the final packet (size < 512 bytes) is sent.

**Project Architecture Summary**
The project consists of three main files:
File	Role
**tftp.h**	The Blueprint: Defines OpCodes (RRQ, WRQ, etc.), transfer modes, and the packed tftp_packet structure shared by both sides.
**client.c**	The User Interface: Provides the menu-driven system for users to connect and initiate transfers.
**server.c**	The File Host: Sits in a continuous loop (while(1)) waiting for incoming UDP packets to process.

**Compilation Tip**
To run the full system, open two terminal windows.
Terminal 1 (Server): gcc server.c -o server && ./server
Terminal 2 (Client): gcc client.c -o client -lm && ./client
