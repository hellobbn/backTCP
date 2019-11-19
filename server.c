//
//  main.c
//  client
//
//  Created by bbn on 2019/11/18.
//  Copyright © 2019 bbn. All rights reserved.
//

// includes here
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <strings.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFER_MAX 512  // max buffer size
#define PORT 12345  // port to connect
#define PSIZE 64
#define SEQ_MAX 32

// backTCP structure
typedef uint8_t tcp_seq;

// header
struct btcphdr {
    uint8_t btcp_sport;     // Source Port
    uint8_t btcp_dport;     // Destination Port
    tcp_seq btcp_seq;
    tcp_seq btcp_ack;
    uint8_t data_off;
    uint8_t win_size;
    uint8_t flag;
};
typedef struct btcphdr header;
typedef struct btcphdr* pheader;

// package
struct btcpPkg {
    header btcpHeader;
    char payload[64];       // at most 64 byte data
};

typedef struct btcpPkg bpkg;
typedef struct btcpPkg* pbpkg;

// some definition
#define MAX_BUF_SIZE 256
#define TIMEOUT 10  // 10ms
#define WIN_SIZE 8 // window size
#define MAX_SEQ_NUM 32  // seq number
/**
 * panic:
 * error and exit when error occurs
 * @param msg the error message
 */
void panic(char* msg) {
    fprintf(stderr, "ERROR: %s", msg);
    exit(1);
}

/**
 * newPackage:
 * creates a new backTCP package;
 * @param sport source port
 * @param dport dest port
 * @param seq sequence number
 * @param ack ack number
 * @param off data offset
 * @param size window size
 * @param flag retransmission or nor
 * @param data the payload
 * @return the pointer to the package
 * The data is then freed
 */
pbpkg newPackage(int sport, int dport, int seq, int ack, int off, int size, int flag, char* data) {
    pbpkg p = malloc(sizeof(bpkg));
    p->btcpHeader.btcp_dport = dport;
    p->btcpHeader.btcp_sport = sport;
    p->btcpHeader.btcp_ack = ack;
    p->btcpHeader.btcp_seq = seq;
    p->btcpHeader.data_off = off;
    p->btcpHeader.win_size = size;
    p->btcpHeader.flag = flag;
    
    // copy payload
    for(int i = 0; i < off; ++ i) {
        p->payload[i] = data[i];
    }
    
    free(data);
    return p;
}

// client main
int main(void) {
    // first, socket initialization
    int sockfd, connfd;
    // struct sockaddr_in servaddr, cli;

    // make a output file
    FILE* fptr = fopen("output.txt", "wb");

    if(fptr == NULL) {
        return 1;
    }

    // // socket create and verification
    // sockfd = socket(AF_INET, SOCK_STREAM, 0);
    // if(sockfd == -1) { // error
    //     panic("client: Failed to create the socket.\n");
    // } else {
    //     printf("client: Successfully create the socket.\n");
    // }
    // bzero(&servaddr, sizeof(servaddr));

    // // assign ip and port
    // servaddr.sin_family = AF_INET;
    // servaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    // servaddr.sin_port = htons(PORT);

    // // connect the client socket to server
    // if (connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
    //     panic("client: Connection with the server failed.\n");
    // } else {
    //     printf("client: Connected to the server.\n");
    // }


    // socket creation and verification

    struct sockaddr_in servaddr, cli;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        panic("socket creation failed\n");
    } else {
        printf("socket successfully created\n");
    }
    
    bzero(&servaddr, sizeof(servaddr));     // reset all fields of servaddr
    
    // assign IP and PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);   // allow all connections
    
    // binding newly created socket to given IP and verify it
    if(bind(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) != 0) {
        panic("socket bind failed...\n");
    } else {
        printf("socket binding successful\n");
    }
    
    // server listens
    if((listen(sockfd, 5)) != 0) {
        panic("listen failed\n");
    } else {
        printf("server listening...\n");
    }
    
    // accept the client
    int len = sizeof(cli);
    connfd = accept(sockfd, (struct sockaddr*)&cli, (socklen_t*)&len);
    
    if(connfd < 0) {
        panic("server accept failed...\n");
    } else {
        printf("accepted the client\n");
    }

    // Now get messages from server, check, and if necessary, return
    // client: check data, determine which package is dropped during the transmission
    // ACK the package before
    // read the messages from server
    char buffer[BUFFER_MAX];
    int seq;    // the expected sequence number for the package
                // if the first time, should we set when we first receive the
                // package? or we can pre-determine the package number?
                // let the seq = 0 at first, meaning that the sequence starts on 0
                // and the client expects the sequence of the first package to be 0
    seq = 0;
    int lastack = 0;
    int size;
    while (1) {
        size = read(connfd, buffer, sizeof(bpkg));  // read from the socket, 1 package
        if(size == 0) {
            break;
        }
        // now get the packet
        pbpkg p = (pbpkg)buffer;
        printf("DEBUG: client: received package, seq = %d, expected %d\n", p->btcpHeader.btcp_seq, seq);
        if(seq != p->btcpHeader.btcp_seq) {  // not receiving the expected package
            // discard, resend the ack
            // is windows size effective when the receiver sends ACK?
            pbpkg np = newPackage(PORT, PORT, seq, lastack, 0, 0, 0, NULL);
            write(connfd, np, sizeof(bpkg));
        } else {
            // great!
            // write the data to file
            printf("DEBUG: Writing message to file, size: %d\n", p->btcpHeader.data_off);
            fwrite(p->payload, p->btcpHeader.data_off, 1, fptr);
            pbpkg np = newPackage(PORT, PORT, seq, p->btcpHeader.btcp_seq, 0, 0, 0, NULL);
            write(connfd, np, sizeof(bpkg));
            seq = (seq + 1) % SEQ_MAX;
        }
        
    }

    shutdown(connfd, SHUT_WR);
    close(connfd);
    close(sockfd);
    fclose(fptr);

    return 0;

}

