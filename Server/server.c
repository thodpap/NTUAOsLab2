#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include "socket_error_handler.h"

#define PORT 8080
#define MAX_CONNECTIONS (int)3

char buffer[1024];
char *hello = "Hello from FancyServer. Tells us your name: ";
char name[15];

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
    int new_socket = accept(server_socket, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
    if (new_socket < 0) {
        error("Error: Accept");
    } else {
        send(new_socket, hello, strlen(hello), 0);
        // get response
        int valread = read( new_socket, name, 15);
    }
    printf("Accepted connection %d\n", new_socket);
    while(1) {
        int valread = read( new_socket , buffer, 1024);
        if (valread == 0) {
            printf("Connection closed\n");
            break;
        }
        printf("%s: %s\n", name, buffer);
        memset(buffer, 0, strlen(buffer));
    }
    
    // printf("%s\n",buffer );
    // send(new_socket , hello , strlen(hello) , 0 );
    // printf("Hello message sent\n");
    
    return 0;
}
 