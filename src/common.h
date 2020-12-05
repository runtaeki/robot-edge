#ifndef __COMMON_H_
#define __COMMON_H_

#define BUF_SIZE 2048
#define MEMORY_SIZE 4 * 1048576

// commands list
#define C_HELLO 0x0001  // client hello (for test)
#define S_HELLO 0x0002 // server hello (for test)
#define DT_DLVR 0x0003 // write the log
#define DT_STRE 0x0004 // store the data at redis server and write log
#define DT_END 0x0005 // disconnect the client and stop log writing
#define ERROR 0x0006

/*
C_HELLO -> client hello 0x0001
S_HELLO -> server hello 0x0002
DT_DLVR -> data delivery 0x0003
DT_STRE -> data store 0x0004
ERROR -> error 0x0005
*/

/*H_PACK form
    version: 0x04 -> static
    USERID: 0x05 -> static
    sequence: random 2 byte 
    length: total packet length include the header
    command: 0x000# -> command number
    char[BUF_SIZE]
*/


typedef struct hong_header{
    uint8_t version;
    uint8_t USERID;
    uint16_t sequence;
    uint16_t length;
    uint16_t command;
} H_HEADER;


typedef struct hong_pack{
    uint8_t version;
    uint8_t USERID;
    uint16_t sequence;
    uint16_t length;
    uint16_t command;
    char msg[BUF_SIZE];
} H_PACK;

int error_send(int client_sockfd, uint16_t seqNum);

#endif
