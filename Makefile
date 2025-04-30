CC = gcc
CFLAGS = -Wall -Wextra -g -pthread
LDFLAGS = -pthread

# Uncomment to enable PostgreSQL
USE_POSTGRES = 1

# PostgreSQL paths - updated with various potential paths for Mac/Linux
ifdef USE_POSTGRES
  # Use specific homebrew paths for libpq on Mac
  PG_CFLAGS = -I/opt/homebrew/opt/libpq/include -DUSE_POSTGRES
  PG_LDFLAGS = -L/opt/homebrew/opt/libpq/lib -lpq
else
  PG_CFLAGS = 
  PG_LDFLAGS = 
endif

GTK_CFLAGS = $(shell pkg-config --cflags gtk+-3.0)
GTK_LDFLAGS = $(shell pkg-config --libs gtk+-3.0)
JSON_CFLAGS = $(shell pkg-config --cflags json-c)
JSON_LDFLAGS = $(shell pkg-config --libs json-c)

SERVER_SRC = src/server/main.c src/server/server.c src/common/utils.c src/common/auth.c src/common/json_protocol.c
CLIENT_SRC = src/client/main.c src/client/client.c src/common/utils.c src/common/auth.c src/common/json_protocol.c
GTK_CLIENT_SRC = src/client/gtk_main.c src/client/gtk_client.c src/client/client.c src/common/utils.c src/common/auth.c src/common/json_protocol.c
DB_SRC = src/db/database.c

SERVER_OBJ = $(SERVER_SRC:.c=.o)
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
GTK_CLIENT_OBJ = $(GTK_CLIENT_SRC:.c=.o)
DB_OBJ = $(DB_SRC:.c=.o)

all: server client gtk_client

server: $(SERVER_OBJ) $(DB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(PG_LDFLAGS) $(JSON_LDFLAGS)

client: $(CLIENT_OBJ) $(DB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(PG_LDFLAGS) $(JSON_LDFLAGS)

gtk_client: $(GTK_CLIENT_OBJ) $(DB_OBJ)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) $(PG_LDFLAGS) $(GTK_LDFLAGS) $(JSON_LDFLAGS)

src/server/%.o: src/server/%.c
	$(CC) $(CFLAGS) $(PG_CFLAGS) $(JSON_CFLAGS) -I./include -c $< -o $@

src/client/%.o: src/client/%.c
	$(CC) $(CFLAGS) $(PG_CFLAGS) $(GTK_CFLAGS) $(JSON_CFLAGS) -I./include -c $< -o $@

src/common/%.o: src/common/%.c
	$(CC) $(CFLAGS) $(PG_CFLAGS) $(JSON_CFLAGS) -I./include -c $< -o $@

src/db/%.o: src/db/%.c
	$(CC) $(CFLAGS) $(PG_CFLAGS) -I./include -c $< -o $@

clean:
	rm -f $(SERVER_OBJ) $(CLIENT_OBJ) $(GTK_CLIENT_OBJ) $(DB_OBJ) server client gtk_client

.PHONY: all clean 