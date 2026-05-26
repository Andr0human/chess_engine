# Elsa Chess Engine

Elsa is a powerful chess engine written in C++ used in Chessmate. This engine features advanced search techniques, efficient move generation, and sophisticated evaluation to provide strong chess play.

## Features

- Advanced alpha-beta search with various enhancements:
  - Principal Variation Search
  - Iterative Deepening
  - Quiescence Search
  - Late Move Reduction (LMR)
  - Null Move Pruning
  - Search Extensions
- Efficient move generation and board representation
- Bitboard-based implementation for performance
- Transposition Table for caching positions
- Sophisticated positional evaluation
- Endgame recognition and specialized evaluation

## Building from Source

### Requirements

- C++20 compatible compiler (GCC recommended)
- GNU Make

### Build Instructions

Simply run the make command from the project root directory:

```bash
make elsa
```

This will create the executable in the `output` directory.

To clean up build files:

```bash
make clean      # Clean everything
make clean_ob   # Clean object files only
```

## Usage

### UCI mode (default)

Running the binary with no arguments — or with the explicit `uci` argument — starts a UCI session on stdin/stdout, so any UCI-compatible chess GUI (Cute Chess, Arena, Banksia, etc.) can drive Elsa as an engine:

```bash
./output/elsa          # drops into UCI loop
./output/elsa uci      # same, explicit
```

Elsa supports the standard UCI commands: `uci`, `isready`, `ucinewgame`, `position [startpos | fen <fen>] [moves ...]`, `go [movetime | wtime/btime/winc/binc | depth | infinite]`, `stop`, and `quit`.

### CLI subcommands

For benchmarking, debugging, and scripted use, Elsa also exposes a CLI. Arguments are order-independent and use named flags (`fen`, `depth`, `time`, ...):

- `elsa help` — view the command list
- `elsa accuracy` — run perft accuracy tests against the baked-in suite
- `elsa speed` — benchmark perft node throughput
- `elsa go [fen <fen>] [time <seconds>] [depth <d>] [debug]` — search a position with iterative-deepening output
- `elsa bestmove [fen <fen>] [difficulty <beginner|easy|medium|hard|expert>]` — print best move + resulting FEN (used by Chessmate)
- `elsa count [fen <fen>] [depth <d>]` — perft node count with timing
- `elsa movegen [fen <fen>] [depth <d>] [output <file>]` — dump per-root-move perft breakdown
- `elsa static [fen <fen>]` — print ordered moves and static evaluation
- `elsa tune [data <path.epd>] [iters <n>]` — Texel-tune the evaluation blend weights against a labeled EPD set
- `elsa tune --all [dir <folder>] [iters <n>]` — run the tuner across every `.epd` file in a folder
- `elsa readyOk` — quick smoke test (perft 3 from startpos = 8902)

### Example

```bash
./output/elsa go fen "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" time 5
```

Searches the starting position for 5 seconds.

## Code Structure

The engine is organized into several components:

- Move generation (`movegen.cpp`, `movegen.h`)
- Board representation (`bitboard.cpp`, `bitboard.h`)
- Search algorithms (`search.cpp`, `search.h`, `single_thread.cpp`, `single_thread.h`)
- Evaluation (`evaluation.cpp`, `evaluation.h`)
- Transposition tables (`tt.cpp`, `tt.h`)
- Lookup tables and attack patterns (`lookup_table.cpp`, `lookup_table.h`, `attacks.cpp`, `attacks.h`)
- Performance measurement (`perf.h`)
- Utility functions (`base_utils.cpp`, `base_utils.h`)

## Naming Conventions

The codebase follows these naming conventions:

- **Variables**: camelCase

  ```cpp
  int moveCount;
  Bitboard myPawns;
  ```

- **Functions**: camelCase

  ```cpp
  void makeMove();
  Score evaluatePosition();
  ```

- **Constants**: UPPER_SNAKE_CASE

  ```cpp
  const int MAX_DEPTH = 36;
  const double DEFAULT_SEARCH_TIME = 1.0;
  ```

- **Macros**: UPPER_SNAKE_CASE

  ```cpp
  #define FAST_IO() std::ios::sync_with_stdio(0); std::cin.tie(0)
  ```

- **Classes/Structs**: PascalCase

  ```cpp
  class ChessBoard;
  struct MoveList;
  ```

- **Enums**: PascalCase for enum name, UPPER_SNAKE_CASE for values

  ```cpp
  enum PieceType { NONE, PAWN, BISHOP, KNIGHT, ROOK, QUEEN, KING, ALL };
  ```

- **Namespaces**: lowercase

  ```cpp
  namespace plt;
  namespace perf;
  ```

## Acknowledgments

Elsa Chess Engine is part of the [Chessmate](https://github.com/Andr0human/Chessmate/) project, which provides a complete web-based chess interface.
