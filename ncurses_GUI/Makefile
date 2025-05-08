CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lncurses

SRC = main.c ui.c auth.c channels.c users.c messaging.c
OBJ = $(SRC:.c=.o)
EXEC = my_dispute

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean 