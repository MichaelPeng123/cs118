#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <cmath>

#include "utils.h"


int main(int argc, char *argv[]) {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in client_addr, server_addr_to, server_addr_from;
    socklen_t addr_size = sizeof(server_addr_to);
    struct timeval tv;
    struct packet pkt;
    struct packet ack_pkt;
    char buffer[PAYLOAD_SIZE];
    unsigned short ack_num = 0;
    char last = 0;
    char ack = 0;

    int ssthresh = SSTHRESH_INIT;
    int cwnd = CWND_INIT;
    int num_dupes = 0;
    int new_ssthresh;

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

    fseek(fp, 0, SEEK_END);
    int file_size = ftell(fp);
    rewind(fp);

    struct packet client_buffer[file_size / PAYLOAD_SIZE + 1];
    tv.tv_sec = 0;
    tv.tv_usec = 200000;

    int read_pkt = 0;
    while (read_pkt < file_size / PAYLOAD_SIZE + 1) {
        int bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp);
        if (bytes_read < PAYLOAD_SIZE) {
            last = 1;
            buffer[bytes_read] = '\0';
        }
        build_packet(&pkt, read_pkt, ack_num, last, ack, bytes_read, buffer);
        if (last) {
            memcpy(pkt.payload, buffer, PAYLOAD_SIZE);
        }
        client_buffer[read_pkt] = pkt;
        read_pkt++;
    }

    while (true) {
        // send packets
        int send_pkt = ack_num;
        while (send_pkt < ack_num + cwnd && send_pkt < file_size / PAYLOAD_SIZE + 1) {
            printf("Sending packet %d\n", send_pkt);
            if (sendto(send_sockfd, &client_buffer[send_pkt], sizeof(client_buffer[send_pkt]), 0, (struct sockaddr *)&server_addr_to, addr_size) < 0) {
                perror("Error sending packet");
                fclose(fp);
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            }
            if (client_buffer[send_pkt].last == 1) {
                break;
            }
            send_pkt++;
        }

        // set timeout
        if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv) < 0) {
            perror("Error setting timeout");
            fclose(fp);
            close(listen_sockfd);
            close(send_sockfd);
            return 1;
        }
        // handle acks and duplicates
        while (true) {
            printf("ssthresh = %d, cwnd = %d\n", ssthresh, cwnd);
            if (recvfrom(listen_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&server_addr_from, &addr_size) <= 0) {
                // timeout, reset to slow start
                printf("Timeout waiting for: %d\n", ack_num);
                new_ssthresh = std::floor(cwnd / 2);
                if (new_ssthresh < 2) {
                    ssthresh = 2;
                } else {
                    ssthresh = new_ssthresh;
                }
                cwnd = 1;
                num_dupes = 0;
                
                // resend packet
                printf("Resending packet %d\n", ack_num);
                if (ack_num <= 0) {
                    if (sendto(send_sockfd, &client_buffer[0], sizeof(client_buffer[0]), 0, (struct sockaddr *)&server_addr_to, addr_size) < 0) {
                        fclose(fp);
                        perror("Error sending packet");
                        close(listen_sockfd);
                        close(send_sockfd);
                        return 1;
                    }
                } else {
                    if (sendto(send_sockfd, &client_buffer[ack_num - 1], sizeof(client_buffer[ack_num - 1]), 0, (struct sockaddr *)&server_addr_to, addr_size) < 0) {
                        fclose(fp);
                        perror("Error sending packet");
                        close(listen_sockfd);
                        close(send_sockfd);
                        return 1;
                    }
                }
                break;
            } else {
                if (ack_pkt.last == 1) {
                    fclose(fp);
                    close(listen_sockfd);
                    close(send_sockfd);
                    return 0;
                }
                if (ack_pkt.acknum >= ack_num) { 
                    // new ack received
                    ack_num = ack_pkt.acknum + 1;
                    num_dupes = 0;
                    if (cwnd > ssthresh) {
                        // congestion avoidance
                        cwnd += std::floor(1.0 / cwnd);
                    } else {
                        // slow start
                        cwnd *= 2;
                    }
                    break;
                } else {
                    // duplicate ack received
                    num_dupes++;
                    if (num_dupes == 3) {
                        // move to fast recovery
                        new_ssthresh = std::floor(cwnd / 2);
                        if (new_ssthresh < 2) {
                            ssthresh = 2;
                        } else {
                            ssthresh = new_ssthresh;
                        }
                        cwnd = std::floor(ssthresh + 3);
                        num_dupes = 0;

                        // resend packet
                        if (sendto(send_sockfd, &client_buffer[ack_pkt.acknum - 1], sizeof(client_buffer[ack_pkt.acknum - 1]), 0, (struct sockaddr *)&server_addr_to, addr_size) < 0) {
                            perror("Error sending packet");
                            fclose(fp);
                            close(listen_sockfd);
                            close(send_sockfd);
                            return 1;
                        }

                        if (setsockopt(listen_sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv) < 0) {
                            perror("Error setting timeout");
                            fclose(fp);
                            close(listen_sockfd);
                            close(send_sockfd);
                            return 1;
                        }
                    } else if (num_dupes > 3) {
                        // fast recovery -> fast recovery
                        cwnd++;
                    }
                }
            }
        }
    }
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}