g++ -std=c++11 -O3 -flto -DNDEBUG -Wall -Wextra -Wshadow -Wpedantic -o ./demolito ./src/*.cc
strip ./demolito
