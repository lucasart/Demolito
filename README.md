## Demolito

Demolito is a [UCI](http://www.shredderchess.com/chess-info/features/uci-universal-chess-interface.html) chess
engine written in C. As such, it is a command line program, which is not designed to be used directly, but
instead through an UCI capable GUI, such as [CuteChess](http://github.com/cutechess/cutechess.git) or
[Lucas Chess](https://github.com/lukasmonk/lucaschess).

### Versions

The version number is simply the ISO date of the last commit (ie. YYYY-MM-DD). From time to time, I publish some binaries.
[Here](http://open-chess.org/viewtopic.php?f=7&t=3069&start=10#p23534) are the latest ones. If you really want the newest
possible one, you'll need to compile yourself (see below).

### Engine Strength
Stronger than you, even if you are a GM. But nowhere near the top engines like Stockfish, Houdini, and Komodo.
- [FGRL](http://fastgm.de/)
- [CEGT](http://www.cegt.net/)
- [CCRL](http://www.computerchess.org.uk/ccrl/)

### Compilation
Using GCC on Linux, type: `./make.sh ./demolito`. This will compile the program and run a verification test.
You should also be able to compile it on any platform (POSIX or Windows), using a C11 compiler (eg. GCC or Clang).
But you'll have to figure out the exact commands for yourself, depending on your compiler, your libc, and your system.
No spoon feeding here.

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
