all: snake logger

snake : main.c
	mkdir -p bin
	gcc -o bin/snake main.c -lncurses

logger : logger.c 
	mkdir -p bin
	gcc -o bin/logger logger.c

clean:
	rm -rf bin
