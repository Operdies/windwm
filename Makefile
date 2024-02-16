CC = gcc
CFLAGS = -std=c99 -pedantic -Wall -Os

main.exe: main.c Makefile
	${CC} ${CFLAGS} $< -o $@ ${LDFLAGS}

compile_commands.json: Makefile
	compiledb make || echo "compiledb is not installed."

run: main.exe compile_commands.json
	./main.exe
