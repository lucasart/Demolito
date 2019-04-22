# Compiler
# Linux: clang or gcc
# Linux->Windows cross: x86_64-w64-mingw32-gcc
# Linux->Android cross: arm-linux-gnueabihf-gcc -fPIE
CC = clang

# Compilation flags
CF = -std=gnu11 -DNDEBUG -O3 -flto -Wfatal-errors -Wall -Wextra -Wshadow

# Linking flags
LF = -s -lm -lpthread

# Output file name (add .exe for Windows)
OUT = demolito

# Automatic versioning: ISO format date YYYY-MM-DD from the last commit
VERSION = $(shell git show -s --format=%ci | cut -d\  -f1)

all:
	$(CC) -march=native -DPEXT $(CF) -DVERSION=\"$(VERSION)\" ./src/*.c -o $(OUT) $(LF)
	
no_popcnt:
	$(CC) -march=core2 $(CF) -DVERSION=\"$(VERSION)\" ./src/*.c -o $(OUT)_no_popcnt -static $(LF)

popcnt:
	$(CC) -mpopcnt $(CF) -DVERSION=\"$(VERSION)\" ./src/*.c -o $(OUT)_popcnt -static $(LF)

pext:
	$(CC) -mpopcnt -mbmi2 -DPEXT $(CF) -DVERSION=\"$(VERSION)\" ./src/*.c -o $(OUT)_pext -static $(LF)

clean:
	rm $(OUT)
