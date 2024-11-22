all: before server bytestuff

before:
	@echo
	@echo "\033[0;36mSTARTING BUILDING...\033[0m"
	@echo
	mkdir -p dist

server:
	make all -C src/server

bytestuff:
	make all -C src/bytestuff

clean:
	make clean -C src/server

.PHONY: all server clean
