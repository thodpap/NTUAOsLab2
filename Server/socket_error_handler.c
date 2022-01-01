#include "socket_error_handler.h"

void error(char *message) {
    printf("%s\n", message);
    print_errno();
    exit(EXIT_FAILURE);
}
void print_errno() {
    switch(errno) {
        case EDOM:
            printf("Errno: Domain error\n");
            break;
        case ERANGE:
            printf("Errno: Range error\n");
            break;
        case EACCES:
            printf("Errno: Permission denied\n");
            break;
        case EAGAIN:
            printf("Errno: Resource temporarily unavailable\n");
            break;
        case EBADF:
            printf("Errno: Bad Socket Description\n");
            break;
        case EBUSY:
            printf("Errno: Resource busy\n");
            break;
        case EEXIST:
            printf("Errno: File exists.\n");
            break;
        case EINVAL:
            printf("Errno: Invalid parameter\n");
            break;
        case EPIPE:
            printf("Errno: Resource busy\n");
            break;
        case EIO:
            printf("Errno: Socket closed\n");
            break; 
        case ENOENT:
            printf("Errno: No such socket.\n");
            break; 
        case EFAULT:
            printf("Errno: Bad Address\n");
            break;
        
        default:
            printf("Errno: %d\n", errno);

    }
}