/* this is request handling server for dhive robots
 * app protocol spec is specified in the README.md
 * keep updating 
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

int glb_redis_port = 6388;


int pair_resp(char* memory, char *method, char* key, char* val){
	char str_len[20];
	//printf("pair transforming start\nmethod: %s\n", method);
	char* dec_key = key;
	char* dec_val = val;
	memset(str_len, 0x00, 20);
	strcat(memory, "*"); //*
	if (strcmp(method, "GET")==0){strcat(memory, "2");} // GET - total 2 followings
	else {strcat(memory, "3");} // SET, APPEND - total 3 followings
	strcat(memory, "\r\n"); // *[#]\r\n
	strcat(memory, "$"); // *[#]\r\n$
	sprintf(str_len, "%lu", strlen(method));
	strcat(memory, str_len);
	strcat(memory, "\r\n");  // *[#]\r\n$[len-method]\r\n
	strcat(memory, method);
	strcat(memory, "\r\n"); // *[#]\r\n$[len-method]\r\n[method]\r\n
	strcat(memory, "$");
	sprintf(str_len, "%lu", strlen(dec_key));
	strcat(memory, str_len);
	strcat(memory, "\r\n");  // *[#]\r\n$[len(mth)]\r\n[method]\r\n$[len(key)]\r\n
	strcat(memory, dec_key);
	strcat(memory, "\r\n"); // *[#]\r\n$[len(mth)]\r\n[method]\r\n$[len(key)]\r\n[key]\r\n
	if (strcmp(method, "GET")==0){
		free(dec_key);
		free(dec_val);
		// end of resp transforminga
		return 0;
	}
	strcat(memory, "$");
	sprintf(str_len, "%lu", strlen(dec_val));
	strcat(memory, str_len);
	strcat(memory, "\r\n");  // *[#]\r\n$[len(mth)]\r\n[method]\r\n$[len(key)]\r\n[key]\r\n$[len(val)]\r\n
	strcat(memory, dec_val);
	strcat(memory, "\r\n"); // *[#]\r\n$[len(mth)]\r\n[method]\r\n$[len(key)]\r\n[key]\r\n$[len(val)]\r\n[val]\r\n
	free(dec_key);
	free(dec_val);
	return 0;
}


int getRoID(char *buf, char * mytitle){
	int level = 0;
	int idfound = 0;
	int size = strlen(buf);
	int position = 0;

	if (buf[position] !=  '{')
		return -1;

	while( position < size){
		switch(buf[position]){
			case '"':{
				char* begin = buf + position + 1; // string
				char* end = strchr(begin, '"');
				if (end == NULL){
					printf("end with NULL, Invalid string\n");
					break;
				}

				int stringlength = end - begin; 
				if (idfound == 1){
					strncpy(mytitle, begin, stringlength);
					printf("RoId: [%s]", mytitle);
					return stringlength;
				}

				char samp[5] = "RoID";
				if (strncmp(samp, begin, 4)!=0){
					printf("not RoID\n");
				} else {
					idfound = 1;
				}
				position = position + stringlength + 1;
				break;
				 }
			default:{
				position++;
				printf("default case\n");
				break;
				}
		}
	}
	printf("No RoId here\n");
	return -1;

}

void redishandle(char * buf){
	char myRoID[20];
	memset(myRoID, 0x00, 20);
	getRoID(buf, myRoID);

	char memory[2048];
	memset(memory, 0x00, 2048);
	pair_resp(memory, "SET", myRoID, buf);

	int redis_len;
	int redis_sockfd;
	char redis_addr[15] = "127.0.0.1";
	struct sockaddr_in redisaddr;

	//printf("redis_handler start\n");
	redis_sockfd = socket(AF_INET, SOCK_STREAM, 0);
    redisaddr.sin_family = AF_INET;
    redisaddr.sin_addr.s_addr = inet_addr(redis_addr);
    redisaddr.sin_port = htons(glb_redis_port);
    redis_len = sizeof(redisaddr);

    if (connect(redis_sockfd, (struct sockaddr*) & redisaddr, redis_len) < 0){
        perror("connecting error --> ");
        exit(1);
    }

	if (write(redis_sockfd, memory, strlen(memory))<0){
        perror("Fail to write to redis server");
        close(redis_sockfd);
        return ;
    }

	char red_buf[2048];
	memset(red_buf, 0x00, 2048);
	if (read(redis_sockfd, red_buf, 2048) < 0){
        perror("fail to read from redis server");
        close(redis_sockfd);
        return ;
    }
	//printf("SET, redis read done\n");
	close(redis_sockfd);
	printf("%s", red_buf);
}


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
	int port_num = -1; // port_num, -1 means error with port_num
	pid_t pid; //to user fork()
	struct sockaddr_in client_addr, server_addr; //socket address for comm
	
	char buf[BUF_SIZE];
	char *memory = malloc(sizeof(char) * MEMORY_SIZE); //4MB malloc memory
	memset(memory, 0x00, sizeof(char) * MEMORY_SIZE);
	// -------------------------------------------S
	// execution validation,  $ ./server port_num
	if (argc != 2){
		if(argc<2)
			fprintf(stderr, "need more args!\n --> ex) $ ./proto-server 12345\n");
		if(argc>2)
			fprintf(stderr, "too many args!\n --> ex) $ ./proto-server 12345\n");
		exit(1);
	}

	port_num = atoi(argv[1]);
	
	if (port_num == -1){
		fprintf(stderr, "please use -p option correctly!!\n --> ex) $ ./proto-server 12345\n");
		exit(1);
	}

	state = 0;
	client_len = sizeof(client_addr);

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
			
			H_HEADER *recv_header = malloc(sizeof(H_HEADER));
			char buf[BUF_SIZE];
			int count = 0;

			char lognum [4];
			sprintf(lognum, "%d", pid);
			
			char filename[20];
			memset(filename, 0x00, 20);
			strcat(filename, "./log/");
			strcat(filename, lognum);
			strcat(filename, ".log");
			FILE *fp = fopen(filename, "w");

			while (1){
				memset(recv_header, 0x00, sizeof(recv_header));
				memset(buf, 0x00, BUF_SIZE);
				if (read(client_sockfd, (char *)recv_header, sizeof(recv_header)) < 0){
					free(recv_header);
					close(client_sockfd);
					break;
				}
				if (recv_header->length>8){
					read(client_sockfd, buf, recv_header->length-8);
				} else {
					if(recv_header->command == DT_END){
						fclose(fp);
						free(recv_header);
						break;
					}
				}

				if(recv_header->command == DT_STRE || recv_header->command == DT_DLVR){
					//printf("saved file name :%s\n", buf);
					//strcat(buf, "server");
					fprintf(fp, "%s\n", buf);
					if(recv_header->command == DT_STRE){
						redishandle(buf);
					}
				}else if(recv_header->command == DT_END){
					fclose(fp);
					free(recv_header);
					break;
				}
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
