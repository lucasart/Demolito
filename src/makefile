# makefile for dev and OpenBench

# TODO:
# * auto-detect compiler: clang if available (8% faster), otherwise gcc. For now assume gcc only
#   for openbench.
# * auto-detect PEXT. Must work on Linux and Windows machines for openbench.

# Compilation flags
CF = -std=gnu11 -DNDEBUG -O3 -flto -Wfatal-errors -Wall -Wextra -Wshadow

# Linking flags
LF = -s -lm -lpthread

# EXE is the hook used by OpenBench to specify the output file name
EXE = demolito

# POPCNT: auto-detection (for both targets), thanks to march=native and compiler intrinsic.
# PEXT: off for openbench target (we can't assume that all OpenBench workers have it).

openbench:
	gcc -march=native $(CF) -DVERSION=\"dev\" ./*.c -o $(EXE) $(LF)

dev:
	clang -march=native $(CF) -DPEXT -DVERSION=\"dev\" ./*.c -o $(EXE) $(LF)

clean:
	rm $(EXE)
