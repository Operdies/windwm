SRC = wdwm.c util.c
CC = gcc
CFLAGS = -std=c99 -pedantic -Wall -O0 -g -DDEBUG

wdwm.exe: ${SRC} Makefile config.h
	${CC} ${CFLAGS} ${SRC} -o $@ ${LDFLAGS}

compile_commands.json: wdwm.exe
	compiledb make || echo "compiledb is not installed."

run: wdwm.exe compile_commands.json
	./wdwm.exe
