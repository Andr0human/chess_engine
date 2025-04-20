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

Elsa provides several commands for different use cases:

- `elsa help` - View the command list
- `elsa accuracy` - Run accuracy tests
- `elsa speed` - Benchmark the engine's speed
- `elsa go <fen> <search_time>` - Evaluate a position
- `elsa count <fen> <depth>` - Count nodes at a given depth
- `elsa debug <fen> <depth> <output_file_name>` - Debug move generation
- `elsa static <fen>` - Get static evaluation of a position
- `elsa play` - Play a game using UCI protocol

### Example:

```bash
./output/elsa go "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 5
```

This evaluates the starting position with 5 seconds of search time.

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
