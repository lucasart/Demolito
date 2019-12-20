## Demolito

Demolito is a [UCI](http://www.shredderchess.com/chess-info/features/uci-universal-chess-interface.html)
chess engine written in [C](https://en.wikipedia.org/wiki/C_(programming_language)). As such, it is
a command line program, which is not designed to be used directly, but instead through an UCI capable
UI, such as [CuteChess](http://github.com/cutechess/cutechess.git) or [Banksia](https://banksiagui.com/).

### Versions

Version numbers are automatically generated to be the ISO date of the last commit (ie. YYYY-MM-DD).

### Windows binaries

Windows binaries are automatically generated, courtesy of AppVeyor, and can be found
[here](https://ci.appveyor.com/project/lucasart/demolito/history):
- select the latest corresponding to the `master` branch. Be sure to choose master because other
branches are (mostly) elo-regressive experimental garbage.
- go to Artifacts, where you can download the (compressed) binaries.

The archive contains 3 `.exe` files, and this is how you choose:
- AMD: use `popcnt` if it works, otherwise `no_popcnt` (very old machine).
- Intel: use `pext` if it works, otherwise `popcnt` (old machine), and if that still fails then
`no_popcnt` (very old machine).

### Playing level

Demolito is easily stronger than best humans, yet still significantly below the top engines like
Stockfish, Houdini or Komodo. Here are some independant rating lists:
- [FGRL](http://fastgm.de/)
- [CEGT](http://www.cegt.net/)
- [CCRL](http://www.computerchess.org.uk/ccrl/)

### UCI Options

- **Contempt**: This is used to score draws by chess rules in the search. These rules are: 3-move
repetition, 50 move rule, stalemate, and insufficient material. A positive value will avoid draws
(best against weaker opponents), whereas a negative value will seek draws (best against a stronger opponent).
- **Hash**: Size of the main hash table, in MB. Should be a power of two (if not Demolito will
silently round it down to the nearest power of two).
- **Time Buffer**: In milliseconds. Provides for extra time to compensate the lag between the UI and
the Engine. The default value is just enough for high performance tools like cutechess-cli, but may
not suffice for some slow and bloated GUIs that introduce artificial lag (and even more so if
playing over a network).
- **Threads**: Number of threads to use for SMP search (default 1 = single threaded search). Please
note that SMP search is, by design, non-deterministic. So it is not a bug that SMP search results
are not reproducible.
- **UCI_Chess960**: enable/disable Chess960 castling rules. Demolito accepts either Shredder-FEN
(AHah) or X-FEN (KQkq) notations.

## Compilation

### What do you need ?

- clang
- make
- git

### How to compile ?

In a terminal:
```
git clone https://github.com/lucasart/Demolito.git
cd Demolito/src
make CC=clang pext  # for Intel Haswell+ only
make CC=clang       # for AMD or older Intel
```
You can use gcc instead of clang, but Demolito will be significantly slower (hence weaker).

### How to verify ?

Run the following benchmark:
```
./demolito bench|tail -3
```
The `nodes` are a functional signature of the program. It must match exactly the one indicated in
commit title for the last functional change, otherwise the compile is broken.

The 'NPS' is a speed measure, in nodes per seconds.
