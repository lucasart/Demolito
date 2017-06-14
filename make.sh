rm $1
astyle -A3 -s4 -f -xn -xc -xl -xC100 -O ./src/* && rm ./src/*.orig
VERSION="-DVERSION=\"$(git show -s --format=%ci | cut -d\  -f1)\""
W="-Wfatal-errors -Wall -Wextra -Wshadow"
O="-O3 -flto -march=native -DNDEBUG"
C="gcc"  # Linux: gcc, Windows: x86_64-w64-mingw32-gcc -static, Android: arm-linux-gnueabi-gcc -static
$C $VERSION -std=gnu11 $W $O -o $1 ./src/*.c -lpthread -lm -s
$1 search 12 1
