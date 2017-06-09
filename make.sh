rm $1
astyle -A3 -s4 -f -xn -xc -xl -xC100 -O ./src/* && rm ./src/*.orig
VERSION="-DVERSION=\"$(git show -s --format=%ci | cut -d\  -f1)\""
W="-Wfatal-errors -Wall -Wextra -Wshadow"
O="-O3 -flto -march=native -DNDEBUG"
gcc -static $VERSION -std=gnu11 $W $O -o $1 ./src/*.c -lpthread -lm
strip $1
$1 search 12 1

# For Android
# arm-linux-gnueabi-gcc -static -std=gnu11 -Wfatal-errors -Wall -Wextra -Wshadow -O3 -flto -march=armv8-a -DNDEBUG ./src/*.c -lpthread -lm -s
