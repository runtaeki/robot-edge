# Robot server and API handler
This is prototypes of robot server(also simple client you can change this as you wish) and api handler. <br> The Robot server is based on socket programming and the app protocol is specified in the below(with unique structure). The API handler uses http, and validating is very simple. It takes the requests from http clients(any clients) and then send parse the json body.<br> This parsed data is sent to the redis server with RESP and takes responses. then return the http response to clients.<br>
## Dependency
`C`, `C++`, `linux`, `libevent`

## Build

check the Makefile
## Server Run procedure
redis_server -> APIhandle_server, robot_server 

## Robot Server
The Robot Server takes the data from robot requests. The request specs are shown below. 
### Request Protocol description
1. version: 0x04 (static value)
2. userID: 0x08 (or RobotID)
3. sequence: order sequence(check the received request order)
4. length: total packet length including the headers
5. command: commands for server operation(check common.h)
### File
- proto-server.c
### Run
```bash
~$ ./bin/proto-server ${PORT}
```

### Protocol structure
```c
//common.h
typedef struct hong_pack{
    uint8_t version;
    uint8_t USERID;
    uint16_t sequence;
    uint16_t length;
    uint16_t command;
    char msg[BUF_SIZE]; //BUF_SIZE = 2048
} H_PACK;

/*
|<-1byte->|
+---------+---------+---------+---------+
| version | user ID |      sequence     |
+---------+---------+---------+---------+
|       length      |      command      |
+---------+---------+---------+---------+

+-------------- 2048 byte --------------+
|                 body                  |
+---------------------------------------+
*/
```

## Robot Client
This client is an example client for showing communication between server and client. you can change this part as you wish.
### File
- proto-client.c
### Run
```bash
~$ ./bin/proto-client ${SERVER_IP} ${SERVER_PORT}
```

## API handle
This is for http requests which want to GET(or SET, POST) robot data. You can check the basic robot API docs at this link.
### Install libevent
```bash
~$ sudo apt-get install libevent-dev
```

---
## Redis server
This is a prototype so we can use Redis server with RESP as a simple DB. You can check how to use redis server below.
### Install and Run Redis Server
```bash
~$ wget http://download.redis.io/release/redis-6.0.8.tar.gz
~$ tar xzf redis-6.0.8.tar.gz
~$ cd redis-6.0.8
~$ make

~$ ./redis-server --port ${REDIS_PORT}
```