# makefile for dev and OpenBench

# Compiler: clang (fastest) gcc
CC = clang

# Compilation flags
CF = -std=gnu11 -DNDEBUG -O3 -flto -Wfatal-errors -Wall -Wextra -Wshadow

# Linking flags
LF = -s -lm -lpthread

# EXE is the hook used by OpenBench to specify the output file name
EXE = demolito

# pext is only for Intel CPU with BMI2 support (haswell+). Do not use it for AMD.

default:
	$(CC) -march=native $(CF) -DVERSION=\"dev\" ./*.c -o $(EXE) $(LF)

pext:
	$(CC) -march=native -DPEXT $(CF) -DVERSION=\"dev\" ./*.c -o $(EXE) $(LF)

clean:
	rm $(EXE)
