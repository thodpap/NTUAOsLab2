#ifndef __SOCKET_ERROR_HANDLER_H__
#define __SOCKET_ERROR_HANDLER_H__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

void print_errno();
void error(char *message);

#endif