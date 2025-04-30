CC = gcc
CFLAGS = -Wall -Wextra

# Full application with all dependencies
all: server client

server: src/server.c src/security.c
	$(CC) $(CFLAGS) -o mydiscord-server src/server.c src/security.c `pkg-config --libs gtk+-3.0` -lpq -lbcrypt -lcrypto

client: src/ui.c
	$(CC) $(CFLAGS) -o mydiscord-client src/ui.c `pkg-config --libs gtk+-3.0`

# Simple test client without GTK dependency
test_client: src/test_client.c
	$(CC) $(CFLAGS) -o mydiscord-test-client src/test_client.c -pthread

# Target for compiling with mock database functionality (no PostgreSQL dependency)
mock: src/server_mock.c
	$(CC) $(CFLAGS) -o mydiscord-server-mock src/server_mock.c -pthread

# Setup for when PostgreSQL is properly configured
setup_db:
	createdb mydiscord_test
	psql mydiscord_test -f myDiscord.sql
	psql mydiscord_test -f test_data.sql

clean:
	rm -f mydiscord-server mydiscord-client mydiscord-server-mock mydiscord-test-client

# Simple test without GTK or PostgreSQL dependencies
test_simple: mock test_client
	@echo "Starting mock server..."
	@./mydiscord-server-mock &
	@sleep 1
	@echo "Starting test client..."
	@./mydiscord-test-client

.PHONY: all clean setup_db test_simple mock test_client 