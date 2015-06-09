W="-Wfatal-errors -Wall -Wextra -Wshadow -Wpedantic"
O="-O3 -flto -DNDEBUG"
g++ -std=c++11 $O $W -o ./demolito ./src/*.cc
strip ./demolito
