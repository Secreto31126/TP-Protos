CC = gcc
CFLAGS = -Wall -Werror -O2

ifneq ($(DEBUG), 0)
CFLAGS += -g
endif

BASE_PATH = src/server

SRC = $(wildcard $(BASE_PATH)/lib/*.c) $(BASE_PATH)/main.c
HDR = $(BASE_PATH)/include/

OBJ = $(SRC:.c=.o)

EXEC = dist/server

all: log $(EXEC)

log:
	@echo
	@echo "\033[0;36mMAKEFILE SERVER\033[0m"
	@echo "\tSOURCES=$(SRC) $(SOURCES_ASM)"
	@echo

$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -I$(HDR) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
