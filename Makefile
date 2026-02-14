ALL:
	gcc -ggdb -O0 -pedantic -Wall -Wextra main.c -o prog

run: ALL
	./prog
