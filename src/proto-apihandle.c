#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <netdb.h>

#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/listener.h>
#include <event2/util.h>
#include <event2/visibility.h>
#include <event2/event-config.h>
#include <stdarg.h>

#include "common.h"
#include "urlencode.h"

#define MAX_OUTPUT (512*1024)
#define API_BUF_SIZE 4096*2 //4096
#define MEM_SIZE 4096*4  //sizeof(char)*4*1048576
#define THR_SIZE 2048

static struct event_base *base;
static struct sockaddr_in listen_on_addr;
static struct sockaddr_in connect_to_addr;
static int connect_to_addrlen;

int aas = 0;
int port_num = -1;
int port_num_red = -1;
char save_addr[256];

static void drained_writecb(struct bufferevent *bev, void *ctx);
static void eventcb(struct bufferevent *bev, short what, void *ctx);

//=================================================================
int recvd_resp(char* memory, char* return_mem){
	memset(return_mem, 0x00, sizeof(return_mem));
	char item[20];
	int list_num = 0;
	int item_len = 0;

	int total_len = 0;
	printf("pair transforming start\nmethod:\n");

	char * ptr1 = memory;
	char* next_ptr;

	//printf("%s\n", memory);

	if (strncmp(memory, "*", 1)==0){
		//printf("* is here\n");
		ptr1 = ptr1 + 1;
		ptr1 = strtok_r(ptr1, "\r\n", &next_ptr);
		list_num = atoi(ptr1);
		printf("%d\n", list_num);

		for (int i = 0; i<list_num; i++){
			ptr1 = strtok_r(NULL, "\r\n", &next_ptr);
			//printf("%s", ptr1);
			memset(item, 0x00, sizeof(item));
			if(strncmp(ptr1, "$", 1)==0){
				ptr1 = ptr1 + 1;
				item_len = atoi(ptr1);
				ptr1 = strtok_r(NULL, "\r\n", &next_ptr);
				strncpy(item, ptr1, item_len);

				strcat(return_mem, item);
				strcat(return_mem, "\r\n");
			}
			else{
				printf("why this case?\n");
			}
		}
		return 1;
	} else if (strncmp(memory, ":0", 2)==0){
		return 400;
	} else if (strncmp(memory, "-", 1)==0) {
		return -1;
	} else if (strncmp(memory, "$", 1)==0) {
		ptr1 = ptr1+1;
		ptr1 = strtok_r(ptr1, "\r\n", &next_ptr);
		item_len = atoi(ptr1);
		strncpy(return_mem, next_ptr+1, item_len);
		return 0;
	} else {
		//printf("else case!!\n");
		//strcpy(return_mem, memory);
		return 0;
	}

}


int pair_resp(char* memory, char *method, char* key, char* val){
	char str_len[20];
	printf("pair transforming start\nmethod: %s\n", method);
	char* dec_key = url_decode(key);
	char* dec_val = url_decode(val);
	memset(str_len, 0x00, 20);
	strcat(memory, "*"); //*
	if (strcmp(method, "GET")==0){strcat(memory, "2");} // GET - total 2 followings
    else if (strcmp(method, "DEL")==0){strcat(memory, "2");}
	else {strcat(memory, "3");} // SET, APPEND - total 3 followings
	strcat(memory, "\r\n$"); // *[#]\r\n$
	sprintf(str_len, "%lu", strlen(method));
	strcat(memory, str_len);
	strcat(memory, "\r\n");  // *[#]\r\n$[len-method]\r\n
	strcat(memory, method);
	strcat(memory, "\r\n$"); // *[#]\r\n$[len-method]\r\n[method]\r\n$
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
	} else if(strcmp(method, "DEL")==0){
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

void getIP(char* hostname, char* save){
    struct addrinfo hints, *res, *p;
    int status;
    char ipstr[INET6_ADDRSTRLEN];
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
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


static void readcb(struct bufferevent *bev, void *ctx){
	//printf("readcb start\n");
	struct bufferevent *partner = ctx;
	struct evbuffer *src, *dst;
	size_t len;
	src = bufferevent_get_input(bev);
	//printf("%d\n", evbuffer_get_max_read(src));
	len = evbuffer_get_length(src);
	if (!partner) {
		evbuffer_drain(src, len);
		return;
	}

    char buf[1024*8];
    memset(buf, 0x00, 1024*8);
    int n;
    while ((n = evbuffer_remove(src, buf, sizeof(buf))) > 0) {
		//fwrite(buf, 1, n, stdout);
    	//fflush(stdout);
	printf("n is -> [%d]\n", n);
        printf("recv: %s\n", buf);
	fflush (stdout);
        //bufferevent_write(bev, buf, n);
		//strcat(buf, tempbuf);
    }
    if (strlen(buf)==0){
	    printf("n-%d\n", n);
	    return;
	    //bufferevent_free();
    }
    //////////////////////////////////////////////////////////
    int header_check = -1;
	int incomplete_key = -1;
	int incomplete_val = -1;
	int total_len = 0;

	//char buf[BUF_SIZE];
	char key_buf[API_BUF_SIZE];
	char val_buf[API_BUF_SIZE];
	//memset(buf, 0x00, BUF_SIZE);
	memset(key_buf, 0x00, API_BUF_SIZE);
	//buf[BUF_SIZE-1] = '\0';

	char *memory = malloc(MEM_SIZE); //4MB malloc memory
	memset(memory, 0x00, MEM_SIZE);
	char *memory_red = malloc(MEM_SIZE); //4MB malloc memory
	memset(memory_red, 0x00, MEM_SIZE);
    int read_var = strlen(buf);
    printf("read var length : [%d]\n", read_var);
	//^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
	char *next_ptr; // next pointer for strtok_r
	//printf("=======================\n%s\n======================\n", buf);
	header_check = 1;
	if (strstr(buf, "\r\n\r\n")!=NULL){header_check = -1;incomplete_key = -1; incomplete_val = -1;}
	char *m_ptr = NULL;
	char *ptr2;
	total_len = read_var;
	char *ptr1 = strstr(buf, "GET"); // check "GET" method
	if(ptr1 != NULL){
		printf("==> GET\n");
		ptr1 = strtok_r(ptr1, " ", &next_ptr); // ptr1 = GET
		ptr1 = strtok_r(NULL, " ", &next_ptr); // ptr1 = /location
		strcpy(key_buf, ptr1+1); // the location(=key)
		ptr1 = strtok_r(NULL, "\r\n", &next_ptr); // HTTP version
        if (strncmp("robot", key_buf, 5)==0){
			if(strncmp("robot/", key_buf, 6)==0){
            	ptr2 = key_buf+6;
				printf("RoID in path:[%s]\n", ptr2);
				int temp_str_len = strlen(ptr2);
				pair_resp(memory_red, "GET", ptr2, ptr2);
            } else if (strlen(key_buf) == 5){
            	strcpy(memory_red, "*2\r\n$4\r\nKEYS\r\n$1\r\n*\r\n");
			} else {
				printf("no robot/RoID in path, GET \n");
			}
        } else{
            printf("no robot in path, GET\n");
        }
	} else {
		char *body_start;
		ptr1 = strstr(buf, "POST");
		if (ptr1 != NULL){
			printf("==> POST\n");
			body_start = strstr(buf, "\r\n\r\n");
			ptr1 = strtok_r(ptr1, " ", &next_ptr); // ptr1 = POST
			ptr1 = strtok_r(NULL, " ", &next_ptr); // ptr1 = /
            strcpy(key_buf, ptr1+1); // the location(=key)
            if(strncmp("robot/", key_buf, 6)==0){
                ptr2 = key_buf+6;
				printf("RoID in path:[%s]\n", ptr2);
            } else {
                printf("no /robot/ROID\n");
                bufferevent_free(bev);
                return ;
            }
			ptr1 = strtok_r(NULL, "\r\n", &next_ptr); // HTTP version
			ptr1 = strstr(next_ptr, "Content-Length"); // length header check
			ptr1 = strtok_r(ptr1, " ", &next_ptr);
			ptr1 = strtok_r(NULL, "\r\n", &next_ptr); // find length
			total_len = atoi(ptr1);
			//printf("===========body-len: [%d]==========\n", total_len);
			read_var = (int)strlen(body_start+4);
            pair_resp(memory_red, "SET", ptr2, body_start);
		} // if tail, POST handle
		else {
            ptr1 = strstr(buf, "DELETE");
            if( ptr1 != NULL){
                ptr1 = strtok_r(ptr1, " ", &next_ptr); // ptr1 = DELETE
                ptr1 = strtok_r(NULL, " ", &next_ptr); // ptr1 = /location
                strcpy(key_buf, ptr1+1); // the location(=key)
                if(strncmp("robot/", key_buf, 6)==0){
                    ptr2 = key_buf+6;
                    pair_resp(memory_red, "DEL", ptr2, ptr2);
                } else {
                    printf("no /robot/ROID\n");
                    bufferevent_free(bev);
                    return;
                }
                //ptr1 = strtok_r(NULL, "\r\n", &next_ptr); // HTTP version
            }
            else {
                printf("Invalid method\n");
		return ;
            }
		}
	} 
	printf(">>>> memory_red---------\n%s--------<<<<\n", memory_red);
    dst = bufferevent_get_output(partner);
    //evbuffer_set_max_read(dst, 8192*2);
    n = evbuffer_add(dst, memory_red, strlen(memory_red));
	free(memory);
	free(memory_red);
	//evbuffer_add_buffer(dst, src);
}

static void
read_2cb(struct bufferevent *bev, void *ctx)
{
	printf("read_2cb start\n");
	struct bufferevent *partner = ctx;
	struct evbuffer *src, *dst;
	size_t len;
	src = bufferevent_get_input(bev);
	//evbuffer_set_max_read(src, 8192*2);
	len = evbuffer_get_length(src);
	if (!partner) {
		evbuffer_drain(src, len);
		return;
	}
    char buf[1024*8];
    char red_buf[1024*9];
    int n;

    memset(buf, 0x00, 1024*8);
    memset(red_buf, 0x00, 1024*9);

    while ((n = evbuffer_remove(src, buf, sizeof(buf))) > 0) {
		fwrite(buf, 1, n, stdout);
    	fflush(stdout);
    }
    
	dst = bufferevent_get_output(partner);
	//evbuffer_set_max_read(dst, 8192*2);
	//evbuffer_add_buffer(dst, src);
	if (strstr(buf, "$-1") != NULL){
        char resp_str[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length:5\r\n\r\nERROR";
        evbuffer_add(dst, resp_str, strlen(resp_str));
    } else if (strstr(buf, "+OK") != NULL){
        char resp_str[] = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: 2\r\n\r\nOK";
		evbuffer_add(dst, resp_str, strlen(resp_str));
	} else{
        memset(red_buf, 0x00, sizeof(red_buf));

		char *ret_memory = malloc(MEM_SIZE); //4MB malloc memory
		memset(ret_memory, 0x00, MEM_SIZE);

		int ret_val = recvd_resp(buf, ret_memory);
		int buf_len = strlen(ret_memory);

		if(ret_val == -1){
			printf("ret:-1");
			char resp_str[] = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\nContent-Length:5\r\n\r\nERROR";
        	evbuffer_add(dst, resp_str, strlen(resp_str));
		} else if( ret_val == 400 ){
			printf("ret:400");
			char resp_str[] = "HTTP/1.1 400 Not Found\r\nContent-Type: text/html\r\nContent-Length:5\r\n\r\nNot Found";
        	evbuffer_add(dst, resp_str, strlen(resp_str));
		} else if(ret_val == 1){
			printf("ret:1");
        	strcpy(red_buf, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ");
			char ch_buf_len[10];
			sprintf(ch_buf_len, "%d", buf_len);
			strcat(red_buf, ch_buf_len);
			strcat(red_buf, "\r\n\r\n");
			strcat(red_buf, ret_memory);
			evbuffer_add(dst, red_buf, strlen(red_buf));
		} else {
			strcpy(red_buf, "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nContent-Length: ");
			char ch_buf_len[10];
			sprintf(ch_buf_len, "%d", buf_len);
			strcat(red_buf, ch_buf_len);
			strcat(red_buf, "\r\n\r\n");
			strcat(red_buf, ret_memory);
			evbuffer_add(dst, red_buf, strlen(red_buf));
		}
		free(ret_memory);
	}
}


static void
drained_writecb(struct bufferevent *bev, void *ctx)
{
	struct bufferevent *partner = ctx;

	/* We were choking the other side until we drained our outbuf a bit.
	 * Now it seems drained. */
	bufferevent_setcb(bev, readcb, NULL, eventcb, partner);
	bufferevent_setwatermark(bev, EV_WRITE, 0, 0);
	if (partner)
		bufferevent_enable(partner, EV_READ);
}

static void
close_on_finished_writecb(struct bufferevent *bev, void *ctx)
{
	struct evbuffer *b = bufferevent_get_output(bev);

	if (evbuffer_get_length(b) == 0) {
		bufferevent_free(bev);
	}
}

static void
eventcb(struct bufferevent *bev, short what, void *ctx)
{
	struct bufferevent *partner = ctx;
	printf("eventcb start\n");
	if (what & (BEV_EVENT_EOF|BEV_EVENT_ERROR)) {
		if (what & BEV_EVENT_ERROR) {
            fprintf(stderr, "%s in %s %s\n", "heeloo?", "good", "gege");
			if (errno)
				perror("connection error");
		}
		printf("eof\n");

		if (partner) {
			printf("partner eof\n");
			/* Flush all pending data */
			//readcb(bev, ctx);
			/*

			if (evbuffer_get_length(
				    bufferevent_get_output(partner))) {
				bufferevent_setcb(partner,
				    NULL, close_on_finished_writecb,
				    eventcb, NULL);
				bufferevent_disable(partner, EV_READ);
			} else {
				bufferevent_free(partner);
			}*/
			bufferevent_free(partner);
		}
		bufferevent_free(bev);
	}
}

static void
accept_cb(struct evconnlistener *listener, evutil_socket_t fd,
    struct sockaddr *a, int slen, void *p)
{
	//printf("accept start\n");
	struct bufferevent *b_out, *b_in;
	/* Create two linked bufferevent objects: one to connect, one for the
	 * new connection */
	b_in = bufferevent_socket_new(base, fd,
	    BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

    b_out = bufferevent_socket_new(base, -1,
        BEV_OPT_CLOSE_ON_FREE|BEV_OPT_DEFER_CALLBACKS);

	assert(b_in && b_out);

	if (bufferevent_socket_connect(b_out,
		(struct sockaddr*)&connect_to_addr, connect_to_addrlen)<0) {
		perror("bufferevent_socket_connect");
		bufferevent_free(b_out);
		bufferevent_free(b_in);
		return;
	}

	bufferevent_setcb(b_in, readcb, NULL, eventcb, b_out);
	bufferevent_setcb(b_out, read_2cb, NULL, eventcb, b_in);

	bufferevent_enable(b_in, EV_READ|EV_WRITE);
	bufferevent_enable(b_out, EV_READ|EV_WRITE);
}

int
main(int argc, char **argv)
{
	int i;
	int socklen;
    char host_name_red[256]; 
	int use_ssl = 0;
	struct evconnlistener *listener;

	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR) {
		perror("signal()");
		return 1;
	}

	if (argc != 4){
		if(argc<4)
			fprintf(stderr, "need more args!\n --> ex) $ ./proto-apihandle 1234 127.0.0.1 6398\n");
		if(argc>4)
			fprintf(stderr, "too many args!\n --> ex) $ ./proto-apihandle 1234 127.0.0.1 6398\n");
		exit(1);
	}
	//put argvs to variables
    port_num = atoi(argv[1]);
    strcpy(host_name_red, argv[2]);
    port_num_red = atoi(argv[3]);
	
    if(port_num < 0 || port_num >65535){
    	fprintf(stderr, "wrong with options!: server-port_num\n");
	    exit(1);
    }
    if(port_num_red < 0 || port_num_red >65535){
    	fprintf(stderr, "wrong with options!: redis-port_num\n");
	    exit(1);
    }
    getIP(host_name_red, save_addr);

	memset(&listen_on_addr, 0, sizeof(listen_on_addr));
    listen_on_addr.sin_family = AF_INET;
    listen_on_addr.sin_port = htons(port_num);
	socklen = sizeof(listen_on_addr);

	memset(&connect_to_addr, 0, sizeof(connect_to_addr));
    connect_to_addr.sin_family = AF_INET;
    connect_to_addr.sin_addr.s_addr = inet_addr(save_addr);
    connect_to_addr.sin_port = htons(port_num_red);
	connect_to_addrlen = sizeof(connect_to_addr);
	
	base = event_base_new();
	if (!base) {perror("event_base_new()");	return 1;}

	listener = evconnlistener_new_bind(base, accept_cb, NULL,
	    LEV_OPT_CLOSE_ON_FREE|LEV_OPT_CLOSE_ON_EXEC|LEV_OPT_REUSEABLE,
	    -1, (struct sockaddr*)&listen_on_addr, socklen);

	if (! listener) {fprintf(stderr, "Couldn't open listener.\n"); event_base_free(base);return 1;}
	event_base_dispatch(base);
	evconnlistener_free(listener);
	event_base_free(base);

	return 0;
}
