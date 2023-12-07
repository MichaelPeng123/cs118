#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils.h"

int main() {
    int listen_sockfd, send_sockfd;
    struct sockaddr_in server_addr, client_addr_from, client_addr_to;
    // struct packet buffer;
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

    // TODO: Receive file from the client and save it as output.txt
    struct packet cur_packet;

    if (!fp) {
        // Handle error
        return EXIT_FAILURE;
    }

    int recv_len;
    int expected_seq = 0;
    int window_size = 5;
    int max_window = 3000;
    struct packet buffer[max_window];
    // keep track of completed packets
    bool done[max_window];
    // use this to send the ack numbers back
    struct packet filler_pkt;

    while (1) {
        struct packet pkt;
        if (recvfrom(listen_sockfd, (void *) &pkt, sizeof(pkt), 0, (struct sockaddr*)&server_addr, &addr_size) > 0) {
            // if seq number is prev_ack's next or starting new file
            int received_seq = pkt.seqnum;
            if (received_seq != expected_seq) {
                if (pkt.seqnum > expected_seq){
                    done[pkt.seqnum] = 1;
                    buffer[pkt.seqnum] = pkt;
                }
                // send previous ack number back 
                filler_pkt.acknum = expected_seq;
                printf("wrong seq received -- expected: %d received: %d\n", expected_seq, pkt.seqnum);
                sendto(send_sockfd, &filler_pkt, sizeof(filler_pkt), 0, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));

            }
            else {
                printf("correct seq received -- value: %d\n", pkt.seqnum);
                int increment_prog = expected_seq;
                done[increment_prog] = 1;
                buffer[increment_prog] = pkt;
                // move forward received values
                while (increment_prog < max_window){
                    if (done[increment_prog] != 1) break;
                    cur_packet = buffer[increment_prog];
                    fwrite(cur_packet.payload, 1, cur_packet.length, fp);
                    if(cur_packet.last)
                    {
                        filler_pkt.last= 1;
                        sendto(send_sockfd, &filler_pkt, sizeof(filler_pkt), 0, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));
                        fclose(fp);
                        close(listen_sockfd);
                        close(send_sockfd);
                        return EXIT_SUCCESS;
                    }
                    increment_prog += 1;
                }
                expected_seq = increment_prog;
                filler_pkt.acknum = increment_prog;
                sendto(send_sockfd, &filler_pkt, sizeof(filler_pkt), 0, (struct sockaddr *)&client_addr_to, sizeof(client_addr_to));
                printf("ack sent: %d\n", filler_pkt.acknum);
            }
        }
    }   

    fclose(fp);
    close(listen_sockfd);
    close(send_sockfd);
    return 0;
}
