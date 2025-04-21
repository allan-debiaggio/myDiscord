CC = gcc
CFLAGS = -Wall -Wextra -I/opt/homebrew/include
LDFLAGS = -L/opt/homebrew/lib -ljson-c

all: server client

server: server.c common.h
	$(CC) $(CFLAGS) -o server server.c $(LDFLAGS)

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

clean:
	rm -f server client

.PHONY: all clean 