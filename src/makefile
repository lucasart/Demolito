# makefile for dev and OpenBench

# Compiler: clang (fastest) gcc
CC = clang

# Compilation flags
CF = -std=gnu11 -DNDEBUG -Ofast -flto -Wfatal-errors -Wall -Wextra -Wshadow -Wstrict-prototypes -Wsign-conversion -Wmissing-prototypes

ifeq ($(CC), clang)
	CF += -Wcast-align -Wmissing-variable-declarations -Wshorten-64-to-32 -Wimplicit-int-conversion -Wimplicit-int-float-conversion
endif

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
