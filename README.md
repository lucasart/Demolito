## Demolito

Demolito is a UCI-compliant chess engine written in C. As such, it is a command line program, which is not designed to be used directly, but instead through an UCI capable GUI, such as [CuteChess](http://github.com/cutechess/cutechess.git). There are no releases at this point. The source code gets continually updated, so you need to compile and run it.

### Engine Strength
Currently about 60 elo behind DiscoCheck 5.2.1, and quickly catching up.

### Compilation
Please use the `make.sh` script provided. It compiles with GCC on Linux. It should also compile the same way under any POSIX compliant system (eg. MacOSX), with either GCC or Clang, but I have not tested.

### UCI Options
- Hash: The hash size in MB.
- UCI_Chess960: enable/disable Chess960 castling rules. In Chess960, FEN can be read both using X-FEN and Shredder-FEN format.
- Contempt: This is used to score draws by chess rules in the search. A positive value will make Demolito avoid draws, and a negative value will make Demolito seek them.
- Threads: Number of threads to use for SMP search (default 1 = single threaded search). Please note that SMP search is, by nature, non-deterministic (ie. it's not a bug, but a feature, that SMP search results are not reproducible).
- Time Buffer: In milliseconds. Allows the engine to provide extra time for lag in GUI <-> Egine communication. The default value is probably far too low for most GUIs, and only suitable for super performant CLI tools like cutechess-cli. Increase it if you experience time losses.
