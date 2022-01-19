CC = gcc

CFLAGS = -Wall -g

LIBS = -lpthread

BINS = server client test

all: $(BINS)

server: server.c socket_error_handler.h 
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

client: client.c socket_error_handler.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

test: test.c
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o *~ $(BINS)