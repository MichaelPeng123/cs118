#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    struct packet buffer;
    struct packet ack_pkt;
    socklen_t addr_size = sizeof(client_addr_from);
    int expected_seq_num = 0;

    // Create a UDP socket for sending
    send_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (send_sockfd < 0) {
        perror("Could not create send socket");
        return 1;
    }

    // Create a UDP socket for listening
    listen_sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sockfd < 0) {
        perror("Could not create listen socket");
        return 1;
    }

    // Configure the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // Bind the listen socket to the server address
    if (bind(listen_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(listen_sockfd);
        return 1;
    }

    // Configure the client address structure to which we will send ACKs
    memset(&client_addr_to, 0, sizeof(client_addr_to));
    client_addr_to.sin_family = AF_INET;
    client_addr_to.sin_addr.s_addr = inet_addr(LOCAL_HOST);
    client_addr_to.sin_port = htons(CLIENT_PORT_TO);

    // Open the target file for writing (always write to output.txt)
    FILE *fp = fopen("output.txt", "wb");
    struct packet server_buffer[BUFFER_SIZE];

    // TODO: Receive file from the client and save it as output.txt
    while (true) {
        if (recvfrom(listen_sockfd, &buffer, sizeof(buffer), 0, (struct sockaddr *)&client_addr_from, &addr_size) == -1) {
            perror("Error retrieving packet");
            fclose(fp);
            close(listen_sockfd);
            close(send_sockfd);
            return 1;
        }
        // printf("expecting packet %d, received packet %d\n", expected_seq_num, buffer.seqnum);

        server_buffer[buffer.seqnum] = buffer;
        if (expected_seq_num == buffer.seqnum) {
            while (server_buffer[expected_seq_num].length > 0) {
                // printf("Writing packet %d\n", expected_seq_num);
                fwrite(server_buffer[expected_seq_num].payload, 1, server_buffer[expected_seq_num].length, fp);
                if (server_buffer[expected_seq_num].last) {
                    fclose(fp);
                    close(listen_sockfd);
                    close(send_sockfd);
                    return 0;
                }
                expected_seq_num++;
            }
            build_packet(&ack_pkt, 0, expected_seq_num - 1, buffer.last, 1, 0, "");
            // printf("Sending ACK %d\n", expected_seq_num - 1);
            if (sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size) < 0) {
                fclose(fp);
                perror("Error sending packet");
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            }
        } else {
            build_packet(&ack_pkt, 0, expected_seq_num, buffer.last, 1, 0, "");
            // printf("Sending retransmit ACK %d\n", expected_seq_num);
            if (sendto(send_sockfd, &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr *)&client_addr_to, addr_size) < 0) {
                fclose(fp);
                perror("Error sending packet");
                close(listen_sockfd);
                close(send_sockfd);
                return 1;
            }
        }
    }
    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
