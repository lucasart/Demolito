## Demolito

Demolito is a [UCI](http://www.shredderchess.com/chess-info/features/uci-universal-chess-interface.html) chess
engine written in C. As such, it is a command line program, which is not designed to be used directly, but
instead through an UCI capable GUI, such as [CuteChess](http://github.com/cutechess/cutechess.git) or
[Lucas Chess](https://github.com/lukasmonk/lucaschess). There are no releases at this point. The source code
gets continually updated, so you need to compile it yourself.

### Engine Strength
About 3000 elo on a human scale, or 2900 on the
[CCRL 40/40](http://www.computerchess.org.uk/ccrl/4040/rating_list_pure.html) scale.

### Compilation
Please use the `make.sh` script provided. It compiles with GCC on Linux. It should also compile the same way using a
C11 capable compiler, and POSIX operating system (eg. Linux, MacOSX, iOS, Android), but I have not tested.

### UCI Options
- **Contempt**: This is used to score draws by chess rules (such as repetition) in the search. A positive value will
avoid draws, and a negative value will seek them.
- **Hash**: Size of the main hash table, in MB.
- **Time Buffer**: In milliseconds. Provides for extra time to compensate the lag between the UI and the Engine. The
default value is probably too low for most GUIs, and only suitable for high performance tools like cutechess-cli.
- **Threads**: Number of threads to use for SMP search (default 1 = single threaded search). Please note that SMP search
is, by nature, non-deterministic (ie. it's not a bug that SMP search results are not reproducible).
- **UCI_Chess960**: enable/disable Chess960 castling rules. Demolito accepts either Shredder-FEN (AHah) or X-FEN (KQkq)
notations for castling.
