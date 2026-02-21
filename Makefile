snake : main.c
	gcc -g -o bin/snake main.c -lcurses

logger : logger.c 
	gcc -o bin/logger logger.c
