## Demolito

Demolito is a [UCI](http://www.shredderchess.com/chess-info/features/uci-universal-chess-interface.html)
chess engine written in [C](https://en.wikipedia.org/wiki/C_(programming_language)). As such, it is
a command line program, which is not designed to be used directly, but instead through an UCI capable
UI, such as [CuteChess](http://github.com/cutechess/cutechess.git) or [Banksia](https://banksiagui.com/),
or [c-chess-cli](https://github.com/lucasart/c-chess-cli.git).

### Versions

Version numbers are automatically generated to be the ISO date of the last commit (ie. YYYY-MM-DD).

### Windows binaries

#### Occasional releases

Here: [releases](https://github.com/lucasart/Demolito/releases).

#### Automatic compiles

Windows binaries are automatically generated when patches are pushed to github, and can be found
[here](https://ci.appveyor.com/project/lucasart/demolito/history):
- select the latest corresponding to the `master` branch. Be sure to choose master because other
branches are (mostly) elo-regressive experimental garbage.
- go to Artifacts, where you can download the (compressed) binaries.

The archive contains 3 `.exe` files, and this is how you choose:
- AMD: use `popcnt` if it works, otherwise `no_popcnt` (very old machine).
- Intel: use `pext` if it works, otherwise `popcnt` (old machine), and if that still fails then
`no_popcnt` (very old machine).

### Playing level

By default playing strength is at maximum. This is suitable for engine vs. engine matches, but far
stronger than the best human players. Here are some rating lists which have tested Demolito:
- [FGRL](http://fastgm.de/)
- [CEGT](http://www.cegt.net/)
- [CCRL](http://www.computerchess.org.uk/ccrl/)

If you want to play against Demolito, you are advised to use the `Level` UCI option.

### UCI Options

- **Contempt**: This is used to score draws by chess rules in the search. These rules are: 3-move
repetition, 50 move rule, stalemate, and insufficient material. A positive value will avoid draws
(best against weaker opponents), whereas a negative value will seek draws (best against a stronger
opponent).
- **Hash**: Size of the main hash table, in MB. Should be a power of two (if not Demolito will
silently round it down to the nearest power of two).
- **Level**: The default value is `0`, which means the level feature is off, and Demolito plays at
full strength. Level `1` is the weakest, and `12` is the strongest (but still weaker than switching
off strength limitation with `Level=0`). Note that Demolito becomes non-deterministic (on purpose),
so that it will play differently every game, even if it reaches the same position.
- **Fake Time**: Use this in combination with `Level` feature, if you want Demolito to (pretend to)
think, instead of moving instantly. Does not affect playing strength of any level, but makes game
play more human friendly (ie. you can think on your opponent's turn, as you would against a human).
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

- clang or gcc
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
You can use gcc instead of clang, but Demolito will be a bit slower (hence weaker).

### How to verify ?

Run the following benchmark:
```
./demolito bench|tail -4
```
The `seal` is a functional signature of the program. It must match exactly the one indicated in the
last commit message. Otherwise, Demolito was miscompiled.

The rest is obvious: nodes, time, nodes per seconds (speed benchmark).
