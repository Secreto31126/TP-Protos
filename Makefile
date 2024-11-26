all: before server bytestuff manager

before:
	@echo
	@echo "\033[0;36mSTARTING BUILDING...\033[0m"
	@echo
	mkdir -p dist

server:
	make all -C src/server
	rm -f dist/mail/*/lock

bytestuff:
	make all -C src/bytestuff

manager:
	make all -C src/manager

clean:
	make clean -C src/server
	make clean -C src/bytestuff
	make clean -C src/manager

.PHONY: all server bytestuff manager clean
