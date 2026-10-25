# Chess (C++17 + SFML)

A complete chess game for the desktop, written in C++17 with an SFML 2.6 user interface.
It implements every rule of chess, has a five-level computer opponent with its own search
engine, chess clocks, PGN/FEN support and a modern dark interface.

All game code is C++ (plus `CMakeLists.txt`). SFML is downloaded and built automatically
by CMake, so you only need a compiler and CMake.

---

## Features

**Rules (all verified with perft tests)**
- Legal move generation for every piece, check, checkmate, stalemate
- Castling on both sides with all conditions (king/rook not moved, empty path, king not in check and not crossing an attacked square)
- En passant, promotion to any piece (picker on the board, or auto-queen)
- Draws: stalemate, insufficient material, **threefold repetition and 50-move rule (claimable)**, **fivefold repetition and 75-move rule (automatic)**, agreement
- Resignation, loss on time, and "timeout vs insufficient material" = draw

**Playing**
- Start menu: play a friend (same computer) or the computer as White / Black / Random
- 5 computer levels (Beginner → Strong) and time controls: no clock, 1+0, 3+2, 5+0, 10+0, 15+10, or custom (e.g. `7+3`)
- Chess clocks with increment, shown beside the board; low-time warning in red
- Click-to-move **and** drag-and-drop, legal-move dots and capture rings, last-move highlight, red glow on a king in check, drag shadow, smooth move animation
- Captured pieces and material advantage (+N) for each player
- Move list in Standard Algebraic Notation (`Nf3`, `exd5`, `O-O`, `e8=Q+`, `Qh4#`); click any move to review that position, step with the arrow keys / mouse wheel; the game resumes at the latest move
- Undo, flip board, hint (engine suggests a move with an arrow), offer draw (the computer accepts only when it is clearly worse), claim draw, resign
- Opening book (40 main lines) and opening names ("Sicilian Defence: Najdorf")
- Game-over dialog with result and reason, **Rematch** button

**Files**
- Save the game as PGN (`saves/` folder next to the program), load saved games
- Copy PGN / copy FEN to the clipboard, paste a FEN or PGN to continue from any position

**Interface**
- 5 board themes (Green, Brown, Blue, Purple, Graphite), settings panel: legal moves, coordinates, animation, sound, auto-queen, auto-flip in 2-player mode — saved in `chess_settings.cfg`
- Sound effects (move, capture, castle, check, game end) generated in code, no audio files; mute toggle
- Resizable window: everything scales and keeps its proportions

## Controls

| Action | Mouse / key |
|---|---|
| Move a piece | click piece, then click target — or drag and drop |
| Cancel selection | right click or `Esc` |
| New game (same settings) | `N` or **New game** |
| Undo | `U` or `Ctrl+Z` (against the computer it also takes back the computer's reply) |
| Flip board | `F` |
| Hint | `H` |
| Review moves | `←` `→` (`Home`/`End` = start/latest), mouse wheel over the board, or click a move |
| Mute / unmute | `M` |
| Save PGN / copy PGN / paste FEN or PGN | `Ctrl+S` / `Ctrl+C` / `Ctrl+V` |
| Promotion | click the piece in the picker (or `Esc` to cancel) |

---

## Building and running

The first configure downloads SFML 2.6.2 (~30 MB, needs internet) and builds it as static
libraries; this takes a few minutes once. Afterwards the build only recompiles the game.

### Windows: VS Code + CMake Tools + MinGW (g++) — recommended

**What you need (once):**
1. **MinGW-w64 g++ 9 or newer, 64-bit, "posix" threads, MSVCRT runtime** (SFML's bundled Windows
   libraries are built for this runtime). The easiest options:
   - [MSYS2](https://www.msys2.org): install it, open the **MSYS2 MINGW64** terminal and run
     `pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja`,
     then add `C:\msys64\mingw64\bin` to your Windows `PATH`.
   - or [WinLibs](https://winlibs.com): download a **"POSIX threads" + "MSVCRT"** x86_64 release (GCC 13 is
     what SFML 2.6 itself is built with), unzip it to e.g. `C:\mingw64` and add `C:\mingw64\bin` to `PATH`.
   - If you already have a MinGW installed, it probably works as long as `g++ --version` shows 9 or newer.
2. **CMake 3.16+** (included with MSYS2 above, or from [cmake.org](https://cmake.org/download/) — tick *"Add CMake to the system PATH"*).
3. **VS Code** with the extensions **CMake Tools** and **C/C++** (Microsoft). VS Code suggests them when you open the folder.

Check in a *new* terminal: `g++ --version` and `cmake --version` must both work.

**Build and run:**
1. Unzip the project, e.g. to `C:\Projects\ChessGame` (avoid very long paths).
2. In VS Code: **File → Open Folder…** → select the `ChessGame` folder.
3. When asked *"Select a kit"* (or press `Ctrl+Shift+P` → **CMake: Select a Kit**) choose your MinGW compiler,
   e.g. **"GCC 13.2.0 x86_64-w64-mingw32"**. If it is not listed, run **CMake: Scan for Kits** first.
4. CMake Tools configures automatically (watch the *Output → CMake* panel; the first time it downloads and builds SFML).
   The build type is **Release** by default (status bar → *Release*), which makes the AI much faster.
5. Press **F7** (or click **Build** in the status bar) to build everything.
6. Choose the target to run: status bar → *launch target* → **ChessGame**, then press **Shift+F5** (or the ▶ button).

The program ends up in **`ChessGame\build\ChessGame.exe`**, together with the `assets` folder and `openal32.dll`
(both copied there automatically). SFML, libgcc and libstdc++ are linked statically, so you can double-click
`ChessGame.exe` — just keep `assets\` and `openal32.dll` next to it if you copy it elsewhere.

**Command line alternative** (same compiler, from the project folder):
```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\ChessGame.exe
```
(Without Ninja use `-G "MinGW Makefiles"`, which needs `mingw32-make` — included in WinLibs.)

**Run the tests:** `build\perft.exe` and `build\unit_tests.exe`, or `ctest --test-dir build`.

### Windows: Visual Studio 2022 (MSVC) — alternative

Install *Visual Studio 2022* (or the *Build Tools*) with the workload **"Desktop development with C++"** (includes CMake).
- **Visual Studio IDE:** *File → Open → Folder…* → `ChessGame`. VS configures CMake automatically.
  Select configuration **x64-Release**, choose **ChessGame.exe** as startup item, press **F5**.
- **VS Code:** same steps as above but pick the kit **"Visual Studio Community 2022 Release - amd64"**.
- **Command line** (*Developer PowerShell for VS 2022*):
  ```bat
  cmake -S . -B build
  cmake --build build --config Release
  build\Release\ChessGame.exe
  ```
With MSVC the executable is in `build\Release\` (multi-config); assets and `openal32.dll` are copied there too.

### Linux (Debian/Ubuntu)
```bash
sudo apt install build-essential cmake libx11-dev libxrandr-dev libxcursor-dev libxi-dev libudev-dev \
     libgl1-mesa-dev libfreetype-dev libflac-dev libvorbis-dev libopenal-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ChessGame
```
(Fedora: `sudo dnf install gcc-c++ cmake libX11-devel libXrandr-devel libXcursor-devel libXi-devel systemd-devel mesa-libGL-devel freetype-devel flac-devel libvorbis-devel openal-soft-devel`.)

### macOS
Install the Xcode command line tools (`xcode-select --install`) and CMake (`brew install cmake`), then:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ChessGame
```

### Troubleshooting
- **`cmake` (or `g++`) is not recognized:** the folder is not on `PATH`. Add it (e.g. `C:\msys64\mingw64\bin`, `C:\mingw64\bin` or
  `C:\Program Files\CMake\bin`) via *Start → "Edit the system environment variables" → Environment Variables → Path*,
  then **restart VS Code** (and terminals). In VS Code you can also set the path to CMake explicitly:
  *Settings → "cmake.cmakePath"* → e.g. `C:\msys64\mingw64\bin\cmake.exe`.
- **No kit found:** `Ctrl+Shift+P` → **CMake: Scan for Kits**; make sure `g++ --version` works in a new terminal.
- **`std::thread` / `mutex` errors with MinGW:** your MinGW uses the old *win32* thread model. Install a *posix* build (MSYS2 or WinLibs, see above).
- **Linker errors mentioning freetype / FLAC / vorbis / `__imp_` symbols:** your MinGW uses the UCRT runtime
  (e.g. MSYS2 *UCRT64*), but SFML's bundled Windows libraries are built for MSVCRT. Use the MSYS2 *MINGW64*
  packages or a WinLibs *MSVCRT* build, delete `build` and configure again.
- **Configure fails while downloading SFML:** check the internet connection/proxy and delete the `build` folder, then configure again.
- **Changed compiler/kit and the build is confused:** `Ctrl+Shift+P` → **CMake: Delete Cache and Reconfigure** (or delete `build`).
- **No sound:** sound is optional; the game runs normally without an audio device. Check the *Sound* setting (`M`).

---

## Project structure

```
ChessGame/
├── CMakeLists.txt          build script (fetches SFML, builds game + tests, copies assets)
├── src/
│   ├── main.cpp            entry point: finds the assets folder, starts App
│   ├── engine/             chess logic only — no graphics, used by the game AND the tests
│   │   ├── Types.h         Color, PieceType, Piece, squares, castling flags
│   │   ├── Move.h/.cpp     a move (from, to, promotion, special flag)
│   │   ├── Board.h/.cpp    position, FEN, attack detection, make/unmake, Zobrist key
│   │   ├── MoveGenerator   legal move generation + perft
│   │   ├── Notation        SAN and UCI move text (write and parse)
│   │   ├── Game            history, undo, results, draw rules, resign/claim/timeout
│   │   ├── Pgn             PGN export / import
│   │   ├── ChessClock      two-sided clock with increment
│   │   ├── OpeningBook     built-in opening lines and names
│   │   ├── Search          the AI (alpha-beta, iterative deepening, TT, ...)
│   │   ├── AsyncSearch     runs Search on a worker thread
│   │   └── AiLevel.h       the five difficulty levels
│   └── gui/                everything that uses SFML
│       ├── App             window, scaling/letter-boxing, switching screens
│       ├── AppContext.h    shared fonts/images/sounds/settings, GameConfig
│       ├── MenuScreen      start menu
│       ├── GameScreen      playing screen: input, clocks, AI, panels, dialogs
│       ├── Renderer        draws board, pieces, highlights, arrows, animation
│       ├── Dialogs         generic modal dialog, settings dialog, file helpers
│       ├── Ui              colours, themes, fonts, rounded rects, buttons, icons
│       ├── Settings        load/save chess_settings.cfg
│       └── SoundManager    procedurally generated sound effects
├── tests/
│   ├── perft.cpp           move generator test (node counts)
│   └── unit_tests.cpp      FEN, SAN, draw rules, PGN, clock, book, AI tests
├── assets/
│   ├── pieces/             piece images (PNG) + LICENSE.txt
│   └── fonts/              Inter font + LICENSE-Inter.txt (SIL OFL)
├── tools/                  optional helper (not part of the build): SVG → PNG for the pieces
└── .vscode/                CMake Tools settings (Release by default)
```

---

## Design (for the report / viva)

### 1. Separation of engine and GUI
The project is split into two layers. The **engine** (`src/engine`) knows the rules of chess and nothing
about windows or graphics; it is compiled into a static library `chess_engine` that is used by the game
and by the two test programs. The **GUI** (`src/gui`) only displays the engine's state and turns mouse and
keyboard input into calls such as `game.makeMove(move)`. This makes the rules testable without a screen and
keeps each class small and focused (single responsibility).

### 2. Board representation and moves
- `Board` is a *mailbox*: an array of 64 `Piece` values, square = `rank * 8 + file` (a1 = 0, h8 = 63),
  plus side to move, castling rights (4 bit flags), en-passant square, half-move clock and move number —
  exactly the six fields of a **FEN** string, which `Board` can read and write.
- `makeMove` changes the board and pushes an `UndoInfo` record (captured piece, old castling rights,
  old en-passant square, old clocks) on a stack; `unmakeMove` pops it and restores the exact previous
  position. This is used by move generation, the AI and the undo button.
- Castling rights are updated with a mask per square: moving from/to a1, h1, e1, a8, h8 or e8 removes the
  corresponding rights (this also handles a rook being captured on its home square).

### 3. Legal move generation (`MoveGenerator`)
1. Generate **pseudo-legal** moves: each piece's movement pattern (pawn pushes/double pushes/captures/en
   passant/promotions, knight jumps, sliding rays for bishop/rook/queen, king steps, castling).
2. For each one: make it, test whether our own king is attacked (`Board::isSquareAttacked`), unmake it.
   Moves that leave the king in check are discarded. This one rule automatically handles pins, checks,
   discovered checks and the tricky en-passant pin cases.
3. Castling is checked fully in step 1: right still available, rook present, squares between empty, king not
   in check, and the squares the king crosses/lands on not attacked.

**Verification — perft:** `perft(depth)` counts all move sequences to a depth. The counts for standard
positions are published; matching them proves the generator correct.

| Position | Depth | Nodes |
|---|---|---|
| Start position | 1 / 2 / 3 / 4 | 20 / 400 / 8 902 / 197 281 |
| "Kiwipete" | 1 / 2 / 3 / 4 | 48 / 2 039 / 97 862 / 4 085 603 |
| Position 3 | 4 / 5 | 43 238 / 674 624 |
| Position 4, Position 5 | 3 | 9 467 / 62 379 |

### 4. Game rules and results (`Game`)
`Game` keeps the move list, SAN strings and a **Zobrist key** for every position. A Zobrist key is a 64-bit
number: XOR of fixed random numbers for each (piece, square), the side to move, castling rights and
capturable en-passant file. Equal positions have equal keys, so repetition is detected by comparing keys
(only since the last capture or pawn move). After every move `Game` checks: no legal moves → checkmate or
stalemate; insufficient material; 75-move rule and fivefold repetition (automatic). Threefold repetition and
the 50-move rule are offered as **claims**, as in the FIDE rules. Resignation, agreement and time forfeit set
the result from outside.

### 5. Notation and files
`Notation` writes **SAN** including disambiguation (`Nbd2`, `R1e2`), captures, promotion and `+`/`#`, and
parses SAN tolerantly (accepts `0-0`, `e8Q`, `Ng1f3`). `Pgn` writes standard PGN (tags + numbered movetext)
and reads PGN by skipping comments, variations and NAGs and replaying each SAN move through the legal-move
generator — so an illegal file can never corrupt a game.

### 6. The computer opponent (`Search`)
- **Negamax with alpha-beta pruning:** the score of a position is the best of the negated scores of its
  children; branches that cannot change the result are cut off.
- **Iterative deepening:** search depth 1, 2, 3, … until the time limit; the best move of the previous depth
  is searched first, and there is always a move ready when time runs out.
- **Transposition table:** results are stored by Zobrist key (score, depth, bound type, best move) so positions
  reached by different move orders are not searched twice.
- **Move ordering:** hash move first, then captures by **MVV-LVA** (most valuable victim, least valuable
  attacker), promotions, **killer moves** (quiet moves that caused cut-offs at the same depth), history heuristic.
- **Quiescence search:** at depth 0 only captures are searched further, so the engine never stops in the middle
  of an exchange (horizon effect). **Check extension**: positions in check are searched one ply deeper.
- **Evaluation:** material + piece-square tables (tapered between middlegame and endgame by the amount of
  material left), bishop pair, passed/isolated/doubled pawns, rooks on open files, and a "mop-up" term that
  drives a lone king to the edge so the engine can actually checkmate.
- **Draws in search:** repetitions (including earlier game positions) and the 50-move rule score 0.
- **Opening book:** 40 hard-coded main lines; the engine plays a random book move while in book.
- **Levels:** 1 Beginner (depth 1, random choice among moves up to 2.2 pawns worse), 2 Casual (depth 2, ±0.9),
  3 Club (depth 3, ±0.25), 4 Advanced (up to depth 6 / 1.5 s), 5 Strong (up to 4 s per move). With a clock the
  time per move is about 1/25 of the remaining time plus most of the increment.
- **Worker thread:** `AsyncSearch` runs the search on a `std::thread`; the GUI checks an atomic "finished" flag
  every frame, so the window never freezes. A `stop` flag lets the search be cancelled instantly (undo, new game).

### 7. GUI architecture
- `App` owns the window and two screens (`MenuScreen`, `GameScreen`). Everything is drawn on a fixed virtual
  canvas of 1220×780; an `sf::View` with a letter-box viewport scales it to any window size, and mouse
  positions are converted back with `mapPixelToCoords`.
- `GameScreen` is the controller: it turns clicks/drags into moves, runs the clock (`ChessClock` is pure logic;
  the screen feeds it elapsed time every frame), starts/collects AI searches, and manages review mode and dialogs.
- `Renderer` is the view for the board: it receives a `BoardState` (what to highlight, which piece is dragged,
  animation progress) and draws it; it never changes the game.
- `Modal` is one reusable dialog class (title, text, optional text field, buttons) used for game over, settings,
  draw offers, resign confirmation, loading games and entering a FEN.
- Sounds are synthesised at start-up (sine waves + filtered noise with decay envelopes) into `sf::SoundBuffer`s.

### 8. Testing
- `perft` — move-generation counts above (all pass).
- `unit_tests` — 81 checks: FEN round-trip and rejection of invalid FENs, SAN writing/parsing and
  disambiguation, every draw rule, resignation/timeout, undo restoring the exact position, PGN round-trip
  (including custom start positions, comments and variations), clock and increment, validity of all opening-book
  lines, AI tactics (mate in 1, mate in 2, winning a free queen, avoiding stalemate), every level, thread
  start/cancel, and an 80-ply self-play game.

---

## Known limitations
- The engine is a classical alpha-beta engine without null-move pruning or late-move reductions, so it is
  strong for casual players but far below engines like Stockfish.
- Insufficient material for a time-out draw is simplified: the side with time left must have more than a lone
  king or a king and a single minor piece to win.
- Clocks start after each side has made its first move (as on most chess sites); takebacks are allowed even in
  timed games (the clock times are restored).
- There is no native file-open dialog (SFML has none): games are saved to and listed from the `saves` folder;
  any other PGN can be pasted from the clipboard.
- Online play, premoves, arrows drawn by the user and Chess960 are not included.

## Credits and licences
- Piece images: "cburnett" set by Colin M.L. Burnett (GPLv2+ / CC BY-SA 3.0), see `assets/pieces/LICENSE.txt`.
- Font: Inter by The Inter Project Authors, SIL Open Font License 1.1, see `assets/fonts/LICENSE-Inter.txt`.
- SFML (zlib licence) is downloaded at build time from github.com/SFML/SFML.
- Piece-square tables based on Tomasz Michniewski's "Simplified Evaluation Function".
