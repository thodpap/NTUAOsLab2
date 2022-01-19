#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h> //FD_SET, FD_ISSET, FD_ZERO macros  
#include <signal.h>

#include "socket_error_handler.h"

#define PORT 8080
#define MAX_CONNECTIONS (int)2

#define true 1
#define false 0

// char buffer[1009];
char *hello = "Hello from FancyServer. Tell us your name: ";

static char names[MAX_CONNECTIONS][15];
static int accepted_sockets[MAX_CONNECTIONS];
static char input[1024];  

static int server_socket;

struct sockaddr_in addr;
static int addrlen; 

pthread_t thread_id[MAX_CONNECTIONS];
 
void *readFromClient(void *vargp); 

static int findOpenSocketSpot();
static void signal_handler(int signal);

int main(int argc, char **argv) {
    uint16_t port = PORT;
    if (argc == 2) {
        uint16_t t = atoi(argv[1]);
        if (t != 0)
            port = t;
    }

    /* Set up sigint action */ 
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    sigaction(SIGINT, &act, NULL);

    /*
        Domain -> AF_INET: IPv4 Internet protocols
        Type   -> SOCK_STREAM: TCP
        Protocol -> 0 : means ip, check /etc/protocols for more 
    */
    server_socket = socket(AF_INET, SOCK_STREAM, 0);  
    if (server_socket == 0) {
        error("Error: socket");
    }
 
    /* 
        setsockopt is used to make sure that the socket can be used
        and a connection can be made
    */
    int opt = 1;
    int set_socket_option = setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    if (set_socket_option == -1) {
        error("Error: setsockopt");
    }

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
    addrlen = sizeof(addr);     
    
    while (1) { 
        int sock = accept(server_socket, (struct sockaddr *)&addr, (socklen_t *)&addrlen);
        if (sock < 0) {
            error("Error: Accepting new socket");            
        } else { 
            int pos = findOpenSocketSpot();

            if (pos == -1) { 
                close(sock);
                continue;
            }
            accepted_sockets[pos] = sock;   
            printf("Creating thread %d\n", pos);
            pthread_create(&thread_id[pos], NULL, readFromClient, (void *)&pos);   
        }
    }
    
    
    return 0;
}
  
void *readFromClient(void *vargp) {
    int *socket = (int *)vargp;  

    int socket_number = *socket;
     
    printf("Connection established with %d, %d\n", socket_number, accepted_sockets[socket_number]);
    send(accepted_sockets[socket_number], hello, strlen(hello), 0);

    // get response
    char name[15]; 
    memset(name, 0, 15); 
    
    int valread = read(accepted_sockets[socket_number], name, 15);
    if (valread < 0) {
        error("Error on read on readFromClient");
    }

    memset(names[socket_number], 0, 15);
    memcpy(names[socket_number], name, strlen(name));

    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        if (accepted_sockets[i] > 0)  {
            char buff[1000];
            sprintf(buff, "%s joined the chat", names[socket_number]);    
            send(accepted_sockets[i], buff, strlen(buff), 0);
        }
    }

    while(1) {    
        if (accepted_sockets[socket_number] == 0) 
            continue;  
            
        int valread = read(accepted_sockets[socket_number] , input, 1009);
        
        if (valread <= 0) {
            printf("Connection was shut down by the client %s\n", names[socket_number]);    
            // close connection
            
            accepted_sockets[socket_number] = 0;

            for (int i = 0; i < MAX_CONNECTIONS; ++i) {
                if (accepted_sockets[i] > 0)  {
                    char buff[1000];
                    sprintf(buff, "%s left the chat\n", names[socket_number]);    
                    send(accepted_sockets[i], buff, strlen(buff), 0);
                }
            }
            memset(names[socket_number], 0, strlen(names[socket_number]));
            return NULL;
        } else {
            printf("%s: %s\n",names[socket_number], input);  
            
            char transmit[1009];  
            sprintf(transmit, "%s: %s", names[socket_number], input); 
            for (int s = 0; s < MAX_CONNECTIONS; ++s) {
                if (accepted_sockets[s] > 0) 
                    send(accepted_sockets[s], transmit, strlen(transmit), 0);            
            }
            
            memset(input, 0, strlen(input));  
        } 
    }
    return NULL;
}

static int findOpenSocketSpot() {
    for (int i = 0; i < MAX_CONNECTIONS; ++i) {
        if (accepted_sockets[i] == 0) 
            return i;
    }
    
    return -1;
}

static void signal_handler(int signal) {
    printf("Terminating the server by signal %d\n", signal);
    for (int s = 0; s < MAX_CONNECTIONS; ++s) {
        if (accepted_sockets[s] > 0) 
            close(accepted_sockets[s]);
    }
    exit(EXIT_FAILURE);
}