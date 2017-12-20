CC = gcc
CFLAGS = -g3 -std=c99 -Wall -pedantic

all: Bash

Bash: stack.o process.c mainBash.c parse.o tokenize.o
	${CC} ${CFLAGS} -o $@ $^

