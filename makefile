CC = clang
CFLAGS = -std=gnu11 -DNDEBUG -O3 -flto -march=native -DPEXT
WFLAGS = -Wfatal-errors -Wall -Wextra -Wshadow
LDFLAGS = -lm -lpthread

EXE = demolito
VERSION = $(shell git show -s --format=%ci | cut -d\  -f1)

all:
	$(CC) $(CFLAGS) $(WFLAGS) -DVERSION=\"$(VERSION)\" ./src/*.c -o $(EXE) $(LDFLAGS)

clean:
	rm $(EXE)
