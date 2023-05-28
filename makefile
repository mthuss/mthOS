main: main.c
	gcc main.c -o main -lm

run: main.c
	gcc main.c -o main -lm && ./main
