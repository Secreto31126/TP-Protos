CC = gcc
CFLAGS = -Wall -fdiagnostics-color=always -lm

ifneq ($(DEBUG), 0)
CFLAGS += -g -DDEVELOPMENT
else
CFLAGS += -O2
endif

SRC = $(wildcard lib/*.c) main.c
HDR = ./include

OBJ = $(SRC:.c=.o)

EXEC = ../../dist/server

all: log $(EXEC)

log:
	@echo
	@echo "\033[0;36mMAKEFILE SERVER\033[0m"
	@echo "\tSOURCES=$(SRC)"
	@echo

$(EXEC):
	$(CC) $(CFLAGS) -I$(HDR) -o $@ $(SRC)

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
