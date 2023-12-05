#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short seq_num = 0;
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    // read filename from command line argument
    if (argc != 2) {
        printf("Usage: ./client <filename>\n");
        return 1;
    }
    char *filename = argv[1];

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Configure the server address structure to which we will send data
    memset(&server_addr_to, 0, sizeof(server_addr_to));
    server_addr_to.sin_family = AF_INET;
    server_addr_to.sin_port = htons(SERVER_PORT_TO);
    server_addr_to.sin_addr.s_addr = inet_addr(SERVER_IP);

    // Configure the client address structure
    memset(&client_addr, 0, sizeof(client_addr));
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(CLIENT_PORT);
    client_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the client address
    if (bind(listen_sockfd, (struct sockaddr *)&client_addr, sizeof(client_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Open file for reading
    FILE *fp = fopen(filename, "rb");
    if (fp == NULL) {
        perror("Error opening file");
        close(listen_sockfd);
        close(send_sockfd);
        return 1;
    }

    // TODO: Read from file, and initiate reliable data transfer to the server
    int window_size = WINDOW_SIZE;
    struct packet window[window_size];
    int window_base = 0;
    int next_seq_num = 0;

    while (1) {
        // Read data from the file
        int bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
        if (bytes_read <= 0) {
            break; // End of file
        }

        // Create a packet
        build_packet(&pkt, next_seq_num, 0, (bytes_read == 0), 0, bytes_read, buffer);

        // Send the packet
        sendto(send_sockfd, &pkt, sizeof(pkt), 0, (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));

        // Move the window
        window_base++;
        next_seq_num = (next_seq_num + 1) % MAX_SEQUENCE;

        // Receive acknowledgments and update the window
        int recv_len = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size);
        if (recv_len < 0) {
            perror("Receive error");
            break;
        }

        // Receive acknowledgments and update the window
        int ack_received = 0;
        while (1) {
            // Check for timeouts
            fd_set readfds;
            FD_ZERO(&readfds);
            FD_SET(listen_sockfd, &readfds);

            struct timeval timeout;
            timeout.tv_sec = TIMEOUT;
            timeout.tv_usec = 0;

            if (select(listen_sockfd + 1, &readfds, NULL, NULL, &timeout) == 0) {
                // Timeout occurred, retransmit the packets in the window
                for (int i = window_base; i < window_base + window_size; i++) {
                    int index = i % window_size;
                    if (window[index].seqnum != -1) {
                        // Retransmit the packet
                        sendto(send_sockfd, &window[index], sizeof(window[index]), 0,
                               (struct sockaddr *)&server_addr_to, sizeof(server_addr_to));
                        printSend(&window[index], 1);  // Log the retransmission
                    }
                }
            }

            // Receive acknowledgment
            recv_len = recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0,
                                (struct sockaddr *)&server_addr_from, &addr_size);
            if (recv_len < 0) {
                perror("Receive error");
                break;
            }

            // Process the acknowledgment
            int acked_seq_num = ack_pkt.acknum;
            if (acked_seq_num >= window_base && acked_seq_num < window_base + window_size) {
                int index = acked_seq_num % window_size;
                // Mark the corresponding packet as acknowledged
                window[index].seqnum = -1;

                // Move the window base
                while (window_base < window_base + window_size && window[window_base % window_size].seqnum == -1) {
                    window_base++;
                }

                ack_received = 1;
            }
        }

        // Check if the last packet has been acknowledged
        if (window_base > next_seq_num) {
            break;
        }
    }
 
    
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}

