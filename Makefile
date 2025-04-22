CC = gcc
CFLAGS = -Wall -Wextra -I/opt/homebrew/include -I/opt/homebrew/opt/libpq/include
LDFLAGS = -L/opt/homebrew/lib -ljson-c -L/opt/homebrew/opt/libpq/lib -lpq

all: server client test_db

server: server.c db_connection.c common.h db_connection.h
	$(CC) $(CFLAGS) -o server server.c db_connection.c $(LDFLAGS)

client: client.c common.h
	$(CC) $(CFLAGS) -o client client.c $(LDFLAGS)

test_db: test_db.c db_connection.c db_connection.h
	$(CC) $(CFLAGS) -o test_db test_db.c db_connection.c $(LDFLAGS)

clean:
	rm -f server client test_db

.PHONY: all clean 