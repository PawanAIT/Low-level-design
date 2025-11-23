# LLD Solution for S1.3 Chess Game (Complex State Management)

## 1. Problem Analysis
### Requirements
- **Functional**:
    - Initialize an 8x8 chess board with standard piece placement.
    - Support two players (White and Black).
    - Validate moves based on piece type and game rules (no jumping for most, diagonal for bishop, etc.).
    - Handle special moves: Castling, En Passant, Pawn Promotion.
    - Detect Game State: Check, Checkmate, Stalemate, Draw.
    - Maintain turn-based gameplay.
    - Support Undo/Redo (implied by complex state management).
- **Non-Functional**:
    - Extensible (easy to add new piece types or board variations).
    - Decoupled UI and Logic.

## 2. High-Level Design
The system follows an **Object-Oriented Architecture** with **MVC** principles (Model-View-Controller) where the Game Logic is the Model.

### Key Components & Relationships
1.  **Game**: The central controller / facade. Manages the flow, turns, and state.
2.  **Board**: Represents the 8x8 grid. specific logic for board state.
3.  **Spot (Cell)**: A specific coordinate on the board, may contain a piece.
4.  **Piece**: Abstract base class. Concrete classes: King, Queen, Rook, Bishop, Knight, Pawn.
5.  **Player**: Represents a user (Human or Engine).
6.  **Move**: Encapsulates a movement action (Command Pattern).
7.  **MoveValidator**: Helper to separate complex validation logic (Strategy/Helper).

## 3. Detailed Component Design (Class Structure)

```cpp
// Forward declarations
class Board;
class Move;
class Piece;

// Enums for standard types
enum class PieceType { KING, QUEEN, ROOK, BISHOP, KNIGHT, PAWN };
enum class Color { WHITE, BLACK };
enum class GameStatus { ACTIVE, BLACK_WIN, WHITE_WIN, FORFEIT, STALEMATE, RESIGNATION };

// ---------------------------------------------------------
// 1. Core Data Structures
// ---------------------------------------------------------

// Represents a coordinate on the board
struct Position {
    int row;
    int col;
    
    // Overloaded operators for equality check
    bool operator==(const Position& other) const;
};

// ---------------------------------------------------------
// 2. The Move Class (Command Pattern)
// ---------------------------------------------------------

/**
 * Encapsulates all information required to execute and undo a move.
 * Essential for history tracking and validating "en passant" or revert on invalid check.
 */
class Move {
private:
    Position start;
    Position end;
    Piece* pieceMoved;
    Piece* pieceKilled; // Null if no capture
    bool isEnPassantMove;
    Piece* promotionPiece; // Null if not promotion

public:
    Move(Position start, Position end, Piece* piece);
    
    // Getters
    Position getStart() const;
    Position getEnd() const;
    
    // For Undo functionality
    Piece* getKilledPiece(); 
};

// ---------------------------------------------------------
// 3. Piece Hierarchy (Factory + Strategy for Movement)
// ---------------------------------------------------------

/**
 * Abstract Base Class for all pieces.
 * Encapsulates basic attributes and movement validation contract.
 */
class Piece {
protected:
    PieceType type;
    Color color;
    bool hasMoved; // Critical for Castling and Pawn first move

public:
    Piece(PieceType type, Color color);
    virtual ~Piece();

    // Core validation logic: Checks if the move pattern is valid for this piece
    // strictly based on geometry (e.g., Bishop moves diagonally).
    // Does NOT check for "King in Check" (that's Game/Board level).
    virtual bool canMove(Board* board, Position start, Position end) = 0;

    // Getters and Setters
    Color getColor();
    PieceType getType();
    void setMoved(bool moved);
};

// Concrete Implementations
class King : public Piece {
    // Implements canMove for 1 step in any direction + Castling logic hooks
};

class Knight : public Piece {
    // Implements L-shape movement (can jump over pieces)
};

// ... (Queen, Rook, Bishop, Pawn implementations)

/**
 * Factory Pattern to create pieces easily.
 * Usage: PieceFactory::createPiece(PieceType::ROOK, Color::WHITE);
 */
class PieceFactory {
public:
    static Piece* createPiece(PieceType type, Color color);
};

// ---------------------------------------------------------
// 4. Board (Container)
// ---------------------------------------------------------

/**
 * Manages the grid of spots/pieces.
 * Responsible for low-level board state but not high-level game flow.
 */
class Board {
private:
    Piece* grid[8][8]; // 8x8 grid of pointers to Pieces (nullptr if empty)
    std::vector<Piece*> whitePieces; // Cache for quick access
    std::vector<Piece*> blackPieces;

public:
    Board();
    void resetBoard(); // Re-initialize with standard setup

    // Basic Access
    Piece* getPieceAt(Position pos);
    void setPieceAt(Position pos, Piece* piece);

    // Movement execution (Low level: updates grid, handles capture)
    // Does not validate rules, just executes.
    void movePiece(Move* move);
    void undoMove(Move* move);

    // Board State Analysis (Helper for Game class)
    bool isKingInCheck(Color kingColor);
    
    // Check if a path is clear between two points (useful for Rook/Bishop/Queen)
    bool isPathClear(Position start, Position end);
};

// ---------------------------------------------------------
// 5. Player
// ---------------------------------------------------------

class Player {
protected:
    Color color;
    bool isHuman;

public:
    Player(Color color, bool isHuman);
    Color getColor();
    // Potential for Engine extension:
    // virtual Move* decideMove(Board* board) = 0; 
};

// ---------------------------------------------------------
// 6. Game (Singleton / Facade / Observer Subject)
// ---------------------------------------------------------

/**
 * The Controller. Manages the flow of the game.
 * Implements the Rule Validation pipeline.
 */
class Game {
private:
    Board* board;
    Player* p1;
    Player* p2;
    Player* currentTurn;
    GameStatus status;
    std::vector<Move*> moveHistory; // For Undo/Redo

    // Private helpers
    void changeTurn();

public:
    Game();
    void initialize(Player* p1, Player* p2);

    // The main entry point for a player action
    bool processMove(Player* player, Position start, Position end);

    // Complex Validation Pipeline
    // 1. Is it player's turn?
    // 2. Is source valid piece of player?
    // 3. Is destination valid (bounds, not own piece)?
    // 4. Does Piece::canMove allow this geometry?
    // 5. Does the move result in King being in check? (Simulation)
    bool validateMove(Move* move);

    // Status Checks
    bool isCheckMate(Color color);
    bool isStalemate(Color color);
    GameStatus getStatus();
    
    // Command Pattern
    void undoLastMove();
};

```

## 4. Design Patterns Applied

1.  **Factory Pattern**: `PieceFactory` encapsulates the creation logic of complex pieces.
2.  **Command Pattern**: The `Move` class acts as a command. It stores all data needed to execute and *un-execute* a move, enabling "Undo" and hypothetical move testing (for checkmate detection).
3.  **Singleton Pattern** (Optional): The `Game` class could be a Singleton if only one game runs per application instance.
4.  **Observer Pattern**: Although not explicitly coded above, the `Game` class usually acts as a Subject, notifying a UI (Observer) whenever the board state changes (`notifyMoveMade()`, `notifyCheckMate()`).
5.  **Strategy Pattern**: `Piece::canMove` implements different movement strategies for different pieces (polymorphism).

## 5. Key Algorithms & Validation Logic

### Move Validation Flow (Inside `Game::processMove`)
1.  **Basic Validation**: Check bounds, check if `start` has a piece belonging to `currentTurn`.
2.  **Piece Logic**: Call `piece->canMove(board, start, end)`.
    *   *Knight*: Checks L-shape offset.
    *   *Rook*: Checks straight line + `board->isPathClear()`.
    *   *Pawn*: Checks forward step, diagonal capture, en-passant condition.
3.  **Simulated Safety Check**:
    *   Execute the move on the board temporarily.
    *   Call `board->isKingInCheck(currentTurn->color)`.
    *   If `true`, revert the move and return `false` (Illegal move: Self-Check).
    *   If `false`, keep the move.

### Checkmate Detection
To detect if White is checkmated:
1.  Check if White's King is currently in Check.
2.  Iterate through **ALL** White pieces.
3.  For each piece, generate **ALL** pseudo-legal moves.
4.  Simulate each move.
5.  If **ANY** move results in the King NOT being in check, it is NOT checkmate.
6.  If no such move exists, Game Over.

## 6. Edge Cases & Complex States
-   **Castling**: Requires checking:
    1.  King & Rook haven't moved (`hasMoved` flag).
    2.  Path between them is empty.
    3.  King is not in check, does not pass through check, and does not end in check.
-   **En Passant**: Requires checking the immediate previous move of the opponent (stored in `moveHistory`).
-   **Promotion**: When a pawn reaches the last rank, the move execution must trigger a promotion selection (changing the piece type).
