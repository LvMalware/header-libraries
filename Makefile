ALL:
	gcc -ggdb -O0 -pedantic -Wall main.c -o prog

run: ALL
	./prog
