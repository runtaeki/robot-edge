::Makefile for prototype

CLIENT_HEADER = src/common.h
CLIENT_SOURCE = src/proto-cli.c

R_API_HANDLE_HDR = src/common.h src.urlencode.h
R_API_HANDLE_SRC = src/proto-apihandler.c src/urlencode.c

SERVER_HEADER = src/common.h 
SERVER_SOURCE = src/proto-server.c 

##############################

CC=gcc
CFLAGS=-I. -g
LDFLAGS= -lpthread -levent -levent_core
LIBS_PATH= -L/usr/local/lib

build: bin/proto-client bin/proto-apihandle bin/proto-server

bin/proto-client: $(CLIENT_SOURCE) $(CLIENT_HEADER) bin
	$(CC) $(CFLAGS) -o $@ $(CLIENT_SOURCE)

bin/proto-apihandle: $(R_API_HANDLE_SRC) $(R_API_HANDLE_HDR) bin
	$(CC) $(CFLAGS) -o $@ $(R_API_HANDLE_SRC) $(LIBS_PATH) $(LDFLAGS)

bin/proto-server: $(SERVER_SOURCE) $(SERVER_HEADER) bin
	$(CC) $(CFLAGS) -o $@ $(SERVER_SOURCE)
    
.PHONY: clean test build

bin:
	mkdir -p bin

clean:
	rm -rf bin
