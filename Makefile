CC = gcc
CFLAGS = -Wall -Werror -O2

ifneq ($(DEBUG), 0)
CFLAGS += -g
endif

SRC = $(wildcard src/*.c)
OBJ = $(SRC:.c=.o)

EXEC = dist/exec

all: $(EXEC)

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
