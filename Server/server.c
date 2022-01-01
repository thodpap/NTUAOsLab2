#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include "socket_error_handler.h"

#define PORT 8080
#define MAX_CONNECTIONS (int)3

char buffer[1024];
char *hello = "Hello from FancyServer. Tells us your name: ";
char name[15];

int sock;
char read_str[1024];
char write_str[1024];

void *readFromClient(void *vargp);

int main(int argc, char **argv) {
    uint16_t port = PORT;
    if (argc == 2) {
        uint16_t t = atoi(argv[1]);
        if (t != 0)
            port = t;
    }
    /*
        Domain -> AF_INET: IPv4 Internet protocols
        Type   -> SOCK_STREAM: TCP
        Protocol -> 0 : means ip, check /etc/protocols for more 
    */
    int server_socket = socket(AF_INET, SOCK_STREAM, 0); 
    printf("Socket: %d\n", server_socket);
    if (server_socket == 0) {
        perror("Error: socket");
        print_errno();
        exit(EXIT_FAILURE);
    }
 
    /* 
        setsockopt is used to make sure that the socket can be used
        and a connection can be made
    */
    int opt = 1;
    int set_socket_option = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt));
    if (set_socket_option == -1) {
        error("Error: setsockopt");
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    int bind_ = bind(server_socket, (struct sockaddr *)&addr, sizeof(addr));
    if (bind_ < 0) {
        error("Error: Bind failed");
    }
    /*
        listen(socket_fd, backlog): backlog defines the maximum length 
        to which the queue of pending connectiosn can grow
    */
    int listen_ = listen(server_socket, MAX_CONNECTIONS);
    if (listen_ < 0) {
        error("Error: listen");
    }

    /*
        Waits for client
    */
    int addrlen = sizeof(addr);
    sock = accept(server_socket, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
    if (sock < 0) {
        error("Error: Accept");
    } else {
        send(sock, hello, strlen(hello), 0);
        // get response
        int valread = read( sock, name, 15); 
        printf("%s Joined the chat\n", name);

        char buf[1024];
        memset(buf, 0, 1024);
        sprintf(buf, "Connection with server Established");
        send(sock, buf, strlen(buf), 0);
    }
    printf("Accepted connection %d\n", sock);
    

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, readFromClient, NULL);

    while(1) {
        memset(write_str, 0, strlen(write_str)); 
        fgets(write_str, 1024, stdin);
        if (write_str[strlen(write_str)-1] == '\n') 
            write_str[strlen(write_str)-1] = 0;
            
        send(sock, write_str, strlen(write_str), 0); 
        // printf(stdout, "Sending: %s\n", write_str);
        memset(write_str, 0, strlen(write_str)); 
    } 

    pthread_join(thread_id, NULL);
    
    return 0;
}
 
void *readFromClient(void *vargp) {
    while(1) {
        memset(read_str, 0, strlen(read_str));
        int valread = read( sock , read_str, 1024);
        if (valread == 0) {
            printf("Connection was shut down by the client\n"); 
            kill(getpid(), SIGINT);
            break;
        } 
        printf("\r%s: %s\n",name, read_str); 
        memset(read_str, 0, strlen(read_str));
        
    }
    return NULL;
}