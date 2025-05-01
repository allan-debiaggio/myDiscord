CC = cc
CFLAGS = -Wall -g
LDFLAGS = -lpq -pthread
PG_INC = -I/opt/homebrew/opt/libpq/include
PG_LIB = -L/opt/homebrew/opt/libpq/lib
DB_FLAG = -DUSE_DATABASE

SERVER_SRC = src/server_mock_db_fixed.c
CLIENT_SRC = mydiscord_client.c

all: server client

server: $(SERVER_SRC)
	$(CC) $(CFLAGS) $(PG_INC) $(PG_LIB) $(DB_FLAG) -o mydiscord-server-fixed $(SERVER_SRC) $(LDFLAGS)

client: $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o mydiscord_client $(CLIENT_SRC) -pthread

clean:
	rm -f mydiscord-server-fixed mydiscord_client
	rm -rf mydiscord-server-fixed.dSYM
	rm -rf logs/test_logs/*

cleanall: clean
	rm -rf logs/*

test: server client
	cd tests && ./test_all_functionality.sh

broadcast-test: server client
	cd tests && ./test_broadcasting.sh

auto-broadcast-test: server client
	cd tests && ./test_broadcast_auto.sh

restart: server
	./restart_server.sh

.PHONY: all server client clean cleanall test broadcast-test auto-broadcast-test restart 