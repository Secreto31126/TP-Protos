CC = gcc
CFLAGS = -Wall -fdiagnostics-color=always

ifneq ($(DEBUG), 0)
CFLAGS += -g -DDEVELOPMENT
else
CFLAGS += -O2
endif

SRC = main.c

OBJ = $(SRC:.c=.o)

EXEC = ../../dist/bytestuff

all: log $(EXEC)

log:
	@echo
	@echo "\033[0;36mMAKEFILE BYTE-STUFF\033[0m"
	@echo "\tSOURCES=$(SRC)"
	@echo

$(EXEC):
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm

clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
