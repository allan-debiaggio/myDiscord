CC = gcc
CFLAGS = -Wall -Wextra -I/usr/local/include -I/opt/homebrew/include -I/opt/homebrew/opt/json-c/include -I/opt/homebrew/Cellar/postgresql@14/14.17_1/include -I/opt/homebrew/opt/openssl@3/include
LDFLAGS = -L/usr/local/lib -L/opt/homebrew/opt/json-c/lib -L/opt/homebrew/lib/postgresql@14 -L/opt/homebrew/opt/openssl@3/lib -ljson-c -lpq -lcrypto

SRCS = server.c db_connection.c db_operations.c auth.c
OBJS = $(SRCS:.c=.o)
TEST_SRCS = test_db.c db_connection.c db_operations.c
TEST_OBJS = $(TEST_SRCS:.c=.o)
TEST_DB_OPS_SRCS = test_db_operations.c db_connection.c db_operations.c
TEST_DB_OPS_OBJS = $(TEST_DB_OPS_SRCS:.c=.o)
TEST_AUTH_SRCS = test_auth.c db_connection.c db_operations.c auth.c
TEST_AUTH_OBJS = $(TEST_AUTH_SRCS:.c=.o)
CLIENT_SRCS = client.c client_auth.c
CLIENT_OBJS = $(CLIENT_SRCS:.c=.o)

all: server test_db test_db_operations test_auth client

server: $(OBJS)
	$(CC) $(OBJS) -o server $(LDFLAGS)

test_db: $(TEST_OBJS)
	$(CC) $(TEST_OBJS) -o test_db $(LDFLAGS)

test_db_operations: $(TEST_DB_OPS_OBJS)
	$(CC) $(TEST_DB_OPS_OBJS) -o test_db_operations $(LDFLAGS)

test_auth: $(TEST_AUTH_OBJS)
	$(CC) $(TEST_AUTH_OBJS) -o test_auth $(LDFLAGS)

client: $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o client $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TEST_OBJS) $(TEST_DB_OPS_OBJS) $(TEST_AUTH_OBJS) $(CLIENT_OBJS) server test_db test_db_operations test_auth client

.PHONY: all clean 