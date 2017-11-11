rm -f $1
VERSION="-DVERSION=\"$(git show -s --format=%ci | cut -d\  -f1)\""
W="-Wfatal-errors -Wall -Wextra -Wshadow"
O="-O3 -flto -march=native -DPEXT -DNDEBUG"
C="clang"  # Linux: clang/gcc, Windows: x86_64-w64-mingw32-gcc -static, Android: arm-linux-gnueabi-gcc -static
$C $VERSION -std=gnu11 $W $O -o $1 ./src/*.c -lpthread -lm -s
$1 search 12 1
