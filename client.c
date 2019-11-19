//
//  main.c
//  server
//
//  Created by bbn on 2019/11/18.
//  Copyright © 2019 bbn. All rights reserved.
//

#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include "backTCP.h"
#include "helper.h"

#define SERVPORT 12345

int sockfd, len;
int curr_start; // window start, start from 0
int curr_end;   // window end, start from 0
int seq_to;     // pointer to package to send
int seq;        // the seq of the current package, start from 1
int pkg_point;  // the time-out pointer or the sequence to find
char buff_send[64]; // data buff to send
int isRound;    // is round
int pkg_seq_point;  
unsigned long bytes_read; // records the bytes read by fread
pbpkg pkgBuf[MAX_SEQ_NUM];   // package buff, 32


pbpkg newPacket(int s_port, int d_port, int seq, int ack, int dat_off, int w_size, int flag, char* dat);    // package maker
void catch_alarm(int sig);      // timeout handler
void start_timer(void);
void stop_timer(void);

int main(int argc, const char * argv[]) {
    // first, socket initialization
    struct sockaddr_in servaddr, cli;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) { // error
        kpanic("client: Failed to create the socket.\n");
    } else {
        printf("client: Successfully create the socket.\n");
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign ip and port
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    servaddr.sin_port = htons(SERVPORT);

    // connect the client socket to server
    if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        kpanic("client: Connection with the server failed.\n");
    } else {
        printf("client: Connected to the server.\n");
    }

    // accepted the client, open the file to send
    // note that if this failed, close the socket and quit
    FILE* fptr = fopen("input.bin", "rb"); // write, binary
    if(fptr == NULL) {
        close(sockfd);
    }
    
    // that's it, let's start the transition
    // set alarm, we need a timer which should be set up by setitimer
    // FIXME: TO MANY USELESS THINGS

    signal(SIGALRM, catch_alarm);
    // initialize the window start, window end, package_point
    pkg_point = 0;
    curr_start = 0;
    curr_end = curr_start + WIN_SIZE;   // window size: 8
    seq_to = 0;
    seq = 0;
    isRound = 0;
    int isFull = 0;
    int end_of_file = 0;
    
    pkg_point = 0;  // the first package to send in index 0
    pkg_seq_point = 0;

    while(1) {  // infinity loop, only breaks if the file reaches its end
        // first, prepare data to the full window

        while ((seq < curr_end) || (curr_end < curr_start && seq < curr_end + MAX_SEQ_NUM)) {  // this means the window is not full, not reaching the end, fill the window?
                                     // FIXME: not right judgement
            bytes_read = fread(buff_send, 1, 64, fptr);  // copies at most 64 bytes data to buff_send
            printf("DEBUG: seq = %d, curr_start = %d, current_point = %d, curr_end = %d\n", seq, curr_start, pkg_point, curr_end);
            printf("DEBUG: not hitting the end, read %lu bytes, making package.\n", bytes_read);
            if(bytes_read != 0) {
                printf("DEBUG: Send...\n");
                pkgBuf[seq] = newPacket(SERVPORT, 12345, seq, 0, (int)bytes_read, WIN_SIZE, 0, buff_send);
                write(sockfd, pkgBuf[seq], sizeof(bpkg));
            } else {
                end_of_file = 1;
                // set curr_end
                curr_end = (seq + 1) % MAX_SEQ_NUM;
                printf("DEBUG: end of file! don't add curr_end\n");
                break;
            }
            if(seq == curr_start) {
                // start timer here
                // sent first package
                pkg_point = seq;    // set point
                start_timer();
            }
            seq = (seq + 1) % MAX_SEQ_NUM;
        }

        // receive package
        char recv_buffer[sizeof(bpkg)];
        pbpkg revPkg;
        int revPkgLen;
        printf("DEBUG: expected ACK: %d\n", pkg_point);
        if((revPkgLen = read(sockfd, recv_buffer, sizeof(bpkg)) > 0)) {
            printf("DEBUG: sender: received package.\n");
            revPkg = (pbpkg)recv_buffer;
            int curr_ack = revPkg->btcpHeader.btcp_ack;
            if((curr_ack >= curr_start && curr_ack < curr_end) || (curr_end < curr_start && (curr_ack >= curr_start || curr_ack < curr_end))) {
                // right ack, move the window
                // restart timers
                printf("DEBUG: sender: ack = %d, right\n", revPkg->btcpHeader.btcp_ack);
                curr_start = (curr_ack + 1) % MAX_SEQ_NUM;
                if(end_of_file == 0) {
                    curr_end = (curr_start + WIN_SIZE + 1) % MAX_SEQ_NUM;
                }
                stop_timer();
                start_timer();  // restart timer
                printf("DEBUG: reset timer\n");
                pkg_point = curr_start;
                if (pkg_point == curr_end - 1 && end_of_file) {
                    break;
                }
            } else {
                // this means wrong ack, drop
                // don't do anything?
                printf("DEBUG: sender: ack = %d, wrong.\n", revPkg->btcpHeader.btcp_ack);
            }
        }

    }

    close(sockfd);
    fclose(fptr);

    return 0;
}

// timer
void start_timer(void) {
    // set timer to 10ms and start it
    struct itimerval t;
    t.it_interval.tv_sec = 1;
    t.it_interval.tv_usec = 10;
    t.it_value.tv_sec = 1;
    t.it_value.tv_usec = 10;
    
    setitimer(ITIMER_REAL, &t, NULL);
    
}

void stop_timer(void) {
    // stop the timer
    struct itimerval t;
    t.it_interval.tv_sec = 0;
    t.it_interval.tv_usec = 0;
    t.it_value.tv_usec = 0;
    t.it_value.tv_usec = 0;
    
    setitimer(ITIMER_REAL, &t, NULL);
}

pbpkg newPacket(int s_port, int d_port, int seq, int ack, int dat_off, int w_size, int flag, char* dat) {
    // fill in a packet
    pbpkg p = malloc(sizeof(bpkg));
    
    p->btcpHeader.btcp_sport = s_port;
    p->btcpHeader.btcp_dport = d_port;
    p->btcpHeader.btcp_seq = seq;
    p->btcpHeader.btcp_ack = ack;
    p->btcpHeader.data_off = dat_off;
    p->btcpHeader.win_size = w_size;
    p->btcpHeader.flag = flag;
    
    for(int i = 0; i < dat_off; ++ i) {
        p->payload[i] = dat[i];
    }
    
    return  p;
}

void catch_alarm(int sig) {
    // this is the timeout handler
    // timeout, so we resend all packages from the timeouted package.
    
    printf("Timed Out for package %d\n", pkg_point);
    int cnt = 0;
    for(int i = pkg_seq_point; cnt < WIN_SIZE; ++ cnt) {
        pkgBuf[i]->btcpHeader.flag = 1;
        write(sockfd, pkgBuf[i], sizeof(bpkg));
        i = (i + 1) % MAX_SEQ_NUM;
        if(i == curr_end) {
            return;
        }
    }
}
