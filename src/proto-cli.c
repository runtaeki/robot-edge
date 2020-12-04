#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"

int error_send(int client_sockfd, uint16_t seqNum){
    H_HEADER *pckhead = malloc(sizeof(H_HEADER));
    pckhead->version = 0x04;
    pckhead->USERID = 0x08;
    pckhead->sequence = htons(seqNum);
    pckhead->length = htons(sizeof(H_HEADER));
    pckhead->command = htons(ERROR);
    if(write(client_sockfd, (char *)pckhead, sizeof(pckhead))<0){
        free(pckhead);
        return -1;
    }
    free(pckhead);
    return 0;
}

void getIP(char* hostname, char* save){
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    if ((status = getaddrinfo(hostname, NULL, &hints, &res)) != 0){
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
	exit(1);
    }

    for(p = res ; p != NULL; p = p->ai_next){
        void* addr;
	    char* ipver;
        if (p->ai_family == AF_INET){
            struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
            addr = &(ipv4->sin_addr);
            ipver = "IPv4";
        }
        else {
            struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            ipver = "IPv6";
        }
        inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
        strcpy(save, ipstr);
    }
    freeaddrinfo(res);
}

int main(int argc, char *argv[]) {

    int client_len;
    int client_sockfd;

    int port_num = -1;
    char host_name[256];
    char save_addr[256];

    struct sockaddr_in clientaddr;

    FILE *fp;

    if (argc != 4) {
	if (argc < 4)
        fprintf(stderr, "need more args!\n --> ex) $ ./client 127.0.0.1 12345 test.txt \n");
	if (argc > 4)
	    fprintf(stderr, "too many args!\n --> ex) $ ./client 127.0.0.1 12345 test.txt \n");
        exit(1);
    }
    
    memset(host_name, 0x00, 256);
    memset(save_addr, 0x00, 256);

    for (int i = 0; i < argc; i++) {
        if (i<argc-1 && i==1){
	        strcpy(host_name,argv[i]); // ip address size error check please
	}
	if (i==2){
        	port_num = atoi(argv[i]);
    	}
    }
    
    if(port_num < 0 || port_num >65535){
    	fprintf(stderr, "wrong with options!: port_num\n");
	    exit(1);
    }

    getIP(host_name, save_addr);

    client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    clientaddr.sin_family = AF_INET;
    clientaddr.sin_addr.s_addr = inet_addr(save_addr);
    clientaddr.sin_port = htons(port_num);
    
    client_len = sizeof(clientaddr);
    sleep(1);
    if (connect(client_sockfd, (struct sockaddr*) & clientaddr, client_len) < 0){
        perror("connecting error --> ");
        exit(1);
    }

    // ------- after connection set --------
    //random number gen
    srand(time(NULL));
    int seqNum = (rand()%4000)+1;

    fp = fopen(argv[3], "r"); //change to argv[4] later
    if (fp == NULL){
        printf("no such file: %s \n", argv[3]);
        close(client_sockfd);
        exit(1);
    }

    H_PACK *send_pack = malloc(sizeof(H_PACK));

    send_pack->version = 0x04;
    send_pack->USERID = 0x08;
    send_pack->sequence = htons(seqNum); //shold be random
    send_pack->length = htons(sizeof(H_PACK)-BUF_SIZE);
    send_pack->command = htons(C_HELLO);

    printf("|v: %x|ID: %x|seq: %x|\n| len:  %x | com:  %x |\n", send_pack->version, send_pack->USERID, send_pack->sequence, send_pack->length, send_pack->command);
    
    if (write(client_sockfd, (char *)send_pack, ntohs(send_pack->length))<0){
        perror("Fail to write to server");
        free(send_pack);
        close(client_sockfd);
        return 0;
    }

    H_HEADER *recv_header = malloc(sizeof(H_HEADER));
    if (read(client_sockfd, (char *)recv_header, sizeof(H_HEADER)) < 0){
        error_send(client_len, seqNum+1);
        perror("fail to read from server");
        free(send_pack);
        free(recv_header);
        close(client_sockfd);
        return 0;
    }

    if (recv_header->sequence!=ntohs(seqNum++) 
        && recv_header->command!=ntohs(S_HELLO)){
        printf("recv sequence number or command error!\n");
        error_send(client_len, seqNum+1);
        free(send_pack);
        free(recv_header);
        fclose(fp);
        close(client_sockfd);
        exit(1);
    }

    send_pack->command = htons(DT_DLVR);
    free(recv_header);
    fflush(stdout);

    while(1){
        memset(send_pack->msg, 0x00, BUF_SIZE);
        if(fgets(send_pack->msg, BUF_SIZE, fp)==NULL){
            break;
        }
        send_pack->sequence = htons(seqNum++);
        send_pack->length = htons(strlen(send_pack->msg)+8);
        if (write(client_sockfd, (char *)send_pack, ntohs(send_pack->length))<0){
            perror("Fail to write to server");
            free(send_pack);
            fclose(fp);
            exit(1);
        }
    }
    fclose(fp);

    send_pack->command = htons(DT_STRE);
    send_pack->sequence = htons(ntohs(send_pack->sequence)+1);
    send_pack->length = htons(sizeof(H_PACK)-BUF_SIZE+strlen(argv[3]));
    memset(send_pack->msg, 0x00, BUF_SIZE);
    strcpy(send_pack->msg, argv[3]);
    
    if (write(client_sockfd, (char *)send_pack, ntohs(send_pack->length))<0){
        perror("Fail to write to server");
        free(send_pack);
    }
    //----------------------------------------------
    printf("---------File sent, done--------\n");
    free(send_pack);
    close(client_sockfd);
    return 0;
}
