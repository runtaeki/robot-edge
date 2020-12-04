/* this is request handling server for dhive robots
 * app protocol spec is specified in the README.md
 * 
 */
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>

#include "common.h"

void childHandler(int signal)
{
    int status;
    pid_t spid;
    while((spid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        printf("-------------------------------\n");
        printf("PID Exit-V Exit-S : %d  %d  %d\n", spid,WEXITSTATUS(status), WIFEXITED(status));
    }
}



int main(int argc, char *argv[]) {
    int server_sockfd, client_sockfd; //socket communication btwn server, client 
	int state, client_len; //state will save the return
	
	pid_t pid; //to user fork()
	
	int port_num = -1; // port_num, -1 means error with port_num

	struct sockaddr_in client_addr, server_addr; //socket address for comm
	
	//struct addrinfo hints;
	//struct addrinfo *servinfo;

	char buf[BUF_SIZE];
	char *memory = malloc(sizeof(char) * 4 * 1048576); //4MB malloc memory
	memset(memory, 0x00, sizeof(char) * 4 * 1048576);
	// -------------------------------------------S
	// execution validation,  $ ./server port_num
	if (argc != 2){
		if(argc<2)
			fprintf(stderr, "need more args!\n --> ex) $ ./server 12345\n");
		if(argc>2)
			fprintf(stderr, "too many args!\n --> ex) $ ./server 12345\n");
		exit(1);
	}

	for (int i = 0; i < argc; i++) {
		if (i==1) {
			port_num = atoi(argv[i]);
			break;
		}
	}
	
	if (port_num == -1){
		fprintf(stderr, "please use -p option correctly!!\n --> ex) $ ./server 12345\n");
		exit(1);
	}

	state = 0;

	client_len = sizeof(client_addr);

	// -------------------------------------------
	// socket setting
	if ((server_sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
		perror("socket error --> ");
		exit(1);
	}

	bzero(&server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY); //localhost IP with INADDR_ANY
	server_addr.sin_port = htons(port_num);

	state = bind(server_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

	if (state == -1){
		perror("binding error --> ");
		exit(1);
	}

	state = listen(server_sockfd, 5);

	if (state == -1){
		perror("listen error ---> ");
		exit(1);
	}
	// -------------------------------------------
	// server start

	signal(SIGCHLD, (void *)childHandler);  //fork() setting, 

	while (1){
		client_sockfd = accept(server_sockfd, (struct sockaddr *)&client_addr, &client_len);
		

		pid = fork();
		if(pid == 0){
		    if (client_sockfd == -1){
				perror("accept error --> ");
				exit(1);
		    }
			close(server_sockfd);
			
			H_PACK *recv_header = malloc(sizeof(H_HEADER));
			memset(recv_header, 0x00, sizeof(H_HEADER));

			if (read(client_sockfd, (char *)recv_header, sizeof(H_HEADER)) < 0){
				free(recv_header);
				close(client_sockfd);
				break;
			}

			if (ntohs(recv_header->command)!=C_HELLO){
				printf("recv command error\n");
				free(recv_header);
				close(client_sockfd);
				exit(1);
			} //error send

			H_PACK *send_header = malloc(sizeof(H_HEADER));
			send_header->version = 0x04;
			send_header->USERID = 0x08; //change this to recv.~~
			send_header->sequence = htons(ntohs(recv_header->sequence)+1);
			send_header->command = htons(S_HELLO);
			send_header->length = htons(sizeof(H_HEADER));
			if (write(client_sockfd, (char *)send_header, ntohs(send_header->length))<0){
				perror("Fail to write to client\n");
				free(send_header);
				free(recv_header);
				close(client_sockfd);
				return 0;
			}
			free(send_header);
			int count = 0;

			while (1){
				memset(recv_header, 0x00, sizeof(recv_header));
				memset(buf, 0x00, BUF_SIZE);
				if (read(client_sockfd, (char *)recv_header, sizeof(recv_header)) < 0){
					free(recv_header);
					close(client_sockfd);
					break;
				}
				if (ntohs(recv_header->length)>8){
					read(client_sockfd, buf, ntohs(recv_header->length)-8);
				}

				if(ntohs(recv_header->command) == DT_STRE){
					printf("saved file name :%s\n", buf);
					//strcat(buf, "server");
					FILE *fp = fopen(buf, "w");
					fprintf(fp, "%s", memory);
					fclose(fp);
					free(recv_header);
					free(memory);
					break;
				}
				//printf("%s", buf);
				strcat(memory, buf);
		    }
			close(client_sockfd);
			exit(0);
		}
		else if(pid==-1){
		    perror("fork error --> ");
			return -1;
		}
		else{
			close(client_sockfd);
		}
	}
	fflush(stdout);
	close(client_sockfd);
    return 0;
}
