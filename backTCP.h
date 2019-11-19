//
//  backTCP.h
//  backTCP
//
//  Created by bbn on 2019/11/18.
//  Copyright © 2019 bbn. All rights reserved.
//

#ifndef backTCP_h
#define backTCP_h

// backTCP structure
#include <stdint.h>
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
#endif /* backTCP_h */
