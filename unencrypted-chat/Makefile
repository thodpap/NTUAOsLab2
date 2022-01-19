CC = gcc

CFLAGS = -Wall 

LIBS = -lpthread

BINS = server client

all: $(BINS)

server: server.c socket_error_handler.h 
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

client: client.c socket_error_handler.h
	$(CC) $(CFLAGS) -o $@ $< $(LIBS)

clean:
	rm -f *.o *~ $(BINS)