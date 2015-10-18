W="-Wfatal-errors -Wall -Wextra -Wshadow"
O="-O3 -flto -march=native -DNDEBUG"
g++ -std=c++11 $W $O -o $1 ./src/*.cc -lpthread
strip ./demolito
