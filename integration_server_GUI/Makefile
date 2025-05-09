CC = gcc
CFLAGS = -Wall -g -Iinclude
LDFLAGS = -lncurses -lpanel -lpthread
OBJDIR = build

# Source files
CLIENT_SRCS = src/client/main.c src/client/ui.c src/client/auth.c src/client/channels.c src/client/messaging.c src/client/users.c src/shared/network.c
SERVER_SRCS = src/server/server.c src/server/server_handlers.c src/server/server_main.c

# Object files
CLIENT_OBJS = $(addprefix $(OBJDIR)/, main.o ui.o auth.o channels.o messaging.o users.o network.o)
SERVER_OBJS = $(addprefix $(OBJDIR)/, server.o server_handlers.o server_main.o)

all: $(OBJDIR)/my_dispute $(OBJDIR)/my_dispute_server

# Client executable
$(OBJDIR)/my_dispute: $(CLIENT_OBJS)
	$(CC) -o $@ $(CLIENT_OBJS) $(LDFLAGS)

# Server executable
$(OBJDIR)/my_dispute_server: $(SERVER_OBJS)
	$(CC) -o $@ $(SERVER_OBJS) $(LDFLAGS)

# Pattern rule for building object files
$(OBJDIR)/%.o: src/client/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: src/server/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: src/shared/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJDIR):
	mkdir -p $(OBJDIR)

clean:
	rm -rf $(OBJDIR)/*.o $(OBJDIR)/my_dispute $(OBJDIR)/my_dispute_server

.PHONY: all clean 