#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include "socket_error_handler.h"

#define PORT 8080
#define true 1
#define false 0

char read_str[1024];
char write_str[1024];
char name[15];

void *readFromServer(void *vargp); 

static int isConnected;
static int sock;

int main(int argc, char **argv) {
    uint32_t port = PORT;
    if (argc == 2) {
        uint32_t t = atoi(argv[1]);
        if (t != 0) 
            port = t;
    }

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) error("Error: Socket error");
    printf("Socket: %d\n", sock);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET; 
    addr.sin_port = htons(port);

    if(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr)<=0) 
        error("Error: Invalid address/ Address not supported"); 

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0)
        error("Error: Bad Connection");
    
    int welcome = read(sock, read_str, 1024);
    printf("Server says: %s", read_str);
    
    fgets(name, 15, stdin); 
    if (name[strlen(name)-1] == '\n') 
        name[strlen(name)-1] = 0;

    if (strlen(name) == 0)
        sprintf(name, "Uknown");

    send(sock, name, strlen(name), 0); 

    memset(read_str, 0, 1024);
    memset(write_str, 0, 1024);
    

    isConnected = true;

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, readFromServer, NULL);

    while(1) {
        fgets(write_str, 1024, stdin);
        if (write_str[strlen(write_str)-1] == '\n') 
            write_str[strlen(write_str)-1] = 0;
        if (!isConnected) 
            break;

        send(sock, write_str, strlen(write_str), 0); 
        // fprintf(stdout, "Sending: %s\n", write_str);
        memset(write_str, 0, strlen(write_str));
    }
    pthread_join(thread_id, NULL);

    return 0;
}


void *readFromServer(void *vargp) {
    while(isConnected) {
        int valread = read( sock , read_str, 1024);
        if (valread == 0) {
            printf("Connection was shut down by the server\n");
            kill(getpid(), SIGINT);
            break;
        } 
        fprintf(stdout, "Server: %s\n", read_str); 
        memset(read_str, 0, strlen(read_str)); 
    }
    return NULL;
} 
