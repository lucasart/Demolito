rm $1
astyle -A3 -s4 -f -xn -xc -xl -xC100 -O ./src/* && rm ./src/*.orig
W="-Wfatal-errors -Wall -Wextra -Wshadow"
O="-O3 -flto -march=native -DNDEBUG"
musl-gcc -static -std=gnu11 $W $O -o $1 ./src/*.c -lpthread -lm
strip $1
$1 search 12 1
