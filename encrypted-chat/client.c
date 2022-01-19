#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h> 
#include <sys/stat.h>
#include <sys/ioctl.h>  
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include "socket_error_handler.h"

#include <crypto/cryptodev.h>

#define PORT 8080
#define true 1
#define false 0

#define DATA_SIZE       1024
#define BLOCK_SIZE      15
#define KEY_SIZE	    15  /* AES128 */

void *readFromServer(void *vargp); 
static void signal_handler(int signal);

static void encrypt_init();
static void encrypt(char * buff);
static void decrypt(char * buff);

char read_str[DATA_SIZE];
char write_str[DATA_SIZE];
char name[15];

unsigned char key[] = "okhfgdnbgfvtrgf";        //encryption key
unsigned char iv[] =  "qghgftrgfbvgfhy";        //initialization vector

struct session_op sess;
struct crypt_op cryp;
struct {
    unsigned char 
            in[DATA_SIZE],
            encrypted[DATA_SIZE],
            decrypted[DATA_SIZE],
            iv[BLOCK_SIZE],
            key[KEY_SIZE];
} data;
    
static int isConnected;
static int sock;
int fd = -1;

int main(int argc, char **argv) { 
    uint32_t port = PORT;
    if (argc == 2) {
        uint32_t t = atoi(argv[1]);
        if (t != 0) 
            port = t;
    }

    /* Set up sigint action */ 
    struct sigaction act;
    memset(&act, 0, sizeof(act));
    act.sa_handler = signal_handler;
    sigaction(SIGINT, &act, NULL);

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


    isConnected = true;
    
    int welcome = read(sock, read_str, 1024);
    if (welcome < 0) {
        error("Error: On first message read from server");
    }
    
    printf("Server says: %s", read_str);
    
    if(strlen(read_str) == 0) {
        error("Connection was closed by the server");
    }
    fgets(name, 15, stdin); 
    if (name[strlen(name)-1] == '\n') 
        name[strlen(name)-1] = 0;

    if (strlen(name) == 0)
        sprintf(name, "Uknown");

    send(sock, name, strlen(name), 0); 

    memset(read_str, 0, 1024);
    memset(write_str, 0, 1024);


    fd = open("/dev/crypto", O_RDWR);

    if (fd < 0) {
        error("Error: Cannot open /dev/crypto");
    }
    printf("Opening /dev/crypto\n");
    
	encrypt_init();

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, readFromServer, NULL);

    while(1) {
        fgets(write_str, 1024, stdin);
        if (write_str[strlen(write_str)-1] == '\n') 
            write_str[strlen(write_str)-1] = 0;
        if (!isConnected) 
            break;

        char sending_str[1024];
        sprintf(sending_str, "%s: %s",name, write_str);

        encrypt(write_str);
        
        send(sock, data.encrypted, strlen((char *)data.encrypted), 0); 
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
        
        // TODO: decrypt the message
        decrypt(read_str);

        printf("%s\n", data.decrypted); 
        memset(read_str, 0, strlen(read_str)); 
    }
    return NULL;
} 

static void signal_handler(int signal) {
    // close(server_socket);
    
    // TODO: Close session
    if (ioctl(fd, CIOCFSESSION, &sess.ses)) {
		error("ioctl(CIOCFSESSION)");
	}

}

static void encrypt_init() { 
	memset(&sess, 0, sizeof(sess));
	memset(&cryp, 0, sizeof(cryp));

    memcpy(&data.iv, iv, BLOCK_SIZE);
    memcpy(&data.key, key, KEY_SIZE);

    sess.cipher = CRYPTO_AES_CBC;
	sess.keylen = KEY_SIZE;
	sess.key = data.key;

	if (ioctl(fd, CIOCGSESSION, &sess)) {
		error("ioctl(CIOCGSESSION)");
	}
}

static void encrypt(char * buff) {
    printf("Message to be encrypted: %s\n", buff);

    memcpy(data.in, buff, strlen(buff));
    cryp.ses = sess.ses;
	cryp.len = sizeof(data.in);
	cryp.src = data.in;
	cryp.dst = data.encrypted;
	cryp.iv = data.iv;
	cryp.op = COP_ENCRYPT;

	if (ioctl(fd, CIOCCRYPT, &cryp)) {
		error("ioctl(CIOCCRYPT)");
	}
    printf("Encrypt message: %s\n", data.encrypted);
}
static void decrypt(char * buff) {
    printf("Message to be encrypted: %s\n", buff); 
    
    cryp.src = data.encrypted;
	cryp.dst = data.decrypted;
	cryp.op = COP_DECRYPT;
	if (ioctl(fd, CIOCCRYPT, &cryp)) {
		error("ioctl(CIOCCRYPT)");
	}
 
    printf("Original message: %s\n", data.decrypted);
}