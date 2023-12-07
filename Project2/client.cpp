#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>
#include <algorithm>

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
    ssize_t bytes_read;
    ssize_t bytes_sent;
    ssize_t bytes_received;

    // BATCH: get total file len, file remainder size
    long int len;
    fseek(fp,0,SEEK_END);
    len = ftell(fp);
    if(len==0)
    {
        return 0;
    }

    long int last_packet_len = len % PAYLOAD_SIZE;
    long int num_packets = len/PAYLOAD_SIZE + (len % PAYLOAD_SIZE != 0);
    printf("last packet len: %d, num of packets:%d\n", last_packet_len, num_packets);

    fclose(fp);
    fp = fopen(filename, "rb");
    
    struct packet packets_window[num_packets];
    while((bytes_read = fread(buffer, 1, PAYLOAD_SIZE, fp))> 0){
        if (bytes_read < PAYLOAD_SIZE){
            last = 1;
        }

        build_packet(&pkt, seq_num, ack_num, last, ack, bytes_read, (const char*) buffer);
        packets_window[seq_num] = pkt;
        seq_num += 1;
    }   

    fclose(fp);


/* CONGESTION CONTROL*/
    // set dup ack for fast retransmit
    int dupCount = 0;
    int last_received_ack = -1;
    seq_num = 0;
    // printf("current seq: %d, start_window: %d, end_window: %d\n", seq_num, start_seq, end_seq);

    // set count for number of packets received for congestion avoidance
    int pack_recv = 0;
    struct packet curr_pkt;
    double startTime;
    enum state STATE = SLOW_START;
    int cwnd = 1;
    int ssthresh = 6;
    int valid_size = 0;
    unsigned short start_seq = 0;
    unsigned short end_seq = cwnd - 1;
    bool empty = false;


    

    if (seq_num >= start_seq && seq_num <= end_seq){
        for(int i = seq_num; i <= end_seq; i ++ ){
            curr_pkt = packets_window[i];
            startTime = setStartTime();
            curr_pkt.starttime = startTime;
            // change_packet_start_time(&curr_pkt, startTime);
            bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
            printf("regular packet %d sent\n", curr_pkt.seqnum);
            packets_window[i] = curr_pkt;
        }
        seq_num = end_seq + 1;
    }

    fcntl(listen_sockfd, F_SETFL, O_NONBLOCK);
    int timeout_count = 0;
    while(1){
        if (last_received_ack >= num_packets-1) {
            break;
        }
        if(timeout(packets_window[start_seq].starttime))
        {
            pack_recv = 0;
            ssthresh = std::max(cwnd/2, 2);
            cwnd = 1;
            // retransmit lost packet
            curr_pkt = packets_window[start_seq];
            // reset timer
            startTime = setStartTime();
            curr_pkt.starttime = startTime;
            // change_packet_start_time(&curr_pkt, startTime);
            bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
            printf("timeout! lost packet %d sent\n", curr_pkt.seqnum);
            packets_window[start_seq] = curr_pkt;
            // change the current window range
            end_seq = start_seq;
        
            STATE = SLOW_START;
        }
        if (bytes_received = recvfrom(listen_sockfd, (void *) &ack_pkt, sizeof(ack_pkt), 0, (struct sockaddr*)&client_addr, &addr_size)>0){
            printf("last ack:%d curr ack:%d starting seq:%d\n", last_received_ack, ack_pkt.acknum, start_seq);
            if(ack_pkt.acknum==last_received_ack)
            {
                dupCount += 1;
                printf("dup ack!!!! %d\n",dupCount);

            } else {
                dupCount = 0;
                // fast recovery on unacked pkt
                if(STATE==FAST_RECOVERY)
                {
                    STATE = SLOW_START;
                    cwnd = ssthresh;
                }
            }

            last_received_ack = ack_pkt.acknum;

            // if three duplicated acks, go to fast retransimit
            if(dupCount==3)
            {
                curr_pkt = packets_window[last_received_ack];

                // reset the timer and resend 
                startTime = setStartTime();
                curr_pkt.starttime = startTime;
                // change_packet_start_time(&curr_pkt, startTime);
                bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                packets_window[last_received_ack] = curr_pkt;

                // reset ssthresh and cwnd
                ssthresh = std::max(cwnd/2, 2);
                cwnd = ssthresh + 3;

                STATE = FAST_RECOVERY;
                pack_recv = 0;
            }

            

            if(STATE==FAST_RECOVERY)
            {
                cwnd += 1;

                // change the window range
                start_seq = last_received_ack;
                end_seq = start_seq + (cwnd - 1);
                if (end_seq >= num_packets){
                    end_seq = num_packets - 1;
                }

                if (seq_num >= start_seq && seq_num <= end_seq){
                    for(int i = seq_num; i <= end_seq; i ++ ){
                        curr_pkt = packets_window[i];
                        startTime = setStartTime();
                        curr_pkt.starttime = startTime;
                        // change_packet_start_time(&curr_pkt, startTime);
                        bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                        packets_window[i] = curr_pkt;

                        printf("regular packet %d sent\n", curr_pkt.seqnum);
                        printf("state: FAST RECOVERY curr cwnd: %d seqnum:%d startseq:%d\n", cwnd, seq_num, start_seq);
                    }
                    seq_num = end_seq + 1;
                }
                
            }
            else if(ack_pkt.acknum > start_seq) {

                if(STATE == SLOW_START)
                {
                    cwnd += 1;
                    
                    // TODO: create 2 packets and send + reset timer
                    start_seq = ack_pkt.acknum;
                    end_seq = start_seq + (cwnd - 1);
                    if (end_seq >= num_packets){
                        end_seq = num_packets - 1;
                    }
                    printf("state: SLOW START curr cwnd: %d seq num:%d start:%d end:%d\n", cwnd, seq_num,start_seq,end_seq);
                    if (seq_num >= start_seq && seq_num <= end_seq){
                        for(int i = seq_num; i <= end_seq; i ++ ){
                            curr_pkt = packets_window[i];
                            startTime = setStartTime();
                            curr_pkt.starttime = startTime;
                            // change_packet_start_time(&curr_pkt, startTime);
                            bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                            printf("regular packet %d sent\n", curr_pkt.seqnum);
                            packets_window[i] = curr_pkt;
                            // printf("this pkt start at %d\n", packets_window[i].starttime);
                        }
                        seq_num = end_seq + 1;
                    }
                    // end of TODO
                    if(cwnd>ssthresh)
                    {
                        pack_recv = 0;
                        STATE = CONGESTION_AVOIDANCE;
                        printf("entering congestion avoidance\n");
                    }
                }
                else if(STATE == CONGESTION_AVOIDANCE)
                {
                    // cwnd +=1 for every RTT
                    // when to reset pack_recv?
                    pack_recv += 1;
                    if(pack_recv==cwnd)
                    {
                        cwnd += 1;
                        pack_recv = 0;
                        // TODO: create packet and send + reset timer
                    }
                    printf("state: CONGESTION AVOIDANCE curr cwnd: %d pack recv:%d\n", cwnd, pack_recv);

                    // TODO: create packet and send + reset timer
                    start_seq = ack_pkt.acknum;
                    end_seq = start_seq + (cwnd - 1);
                    if (end_seq >= num_packets){
                        end_seq = num_packets - 1;
                    }
                    
                    if (seq_num >= start_seq && seq_num <= end_seq){
                        for(int i = seq_num; i <= end_seq; i ++ ){
                            curr_pkt = packets_window[i];
                            startTime = setStartTime();
                            curr_pkt.starttime = startTime;
                            // change_packet_start_time(&curr_pkt, startTime);
                            bytes_sent = sendto(send_sockfd, (void *) &curr_pkt, sizeof(curr_pkt), 0, (struct sockaddr*)&server_addr_to, sizeof(server_addr_to));
                            printf("regular packet %d sent\n", curr_pkt.seqnum);
                            packets_window[i] = curr_pkt;
                            // printf("this pkt start at %d\n", packets_window[i].starttime); 
                        }
                        seq_num = end_seq + 1;
                    }
                }
                
            } 
        }
        
    }


    return 0;

}

