#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#define BOARD_SIZE      8
#define TILE_SIZE       70
#define BOARD_WIDTH     (TILE_SIZE * BOARD_SIZE)
#define WINDOW_WIDTH    (BOARD_WIDTH + 300)
#define WINDOW_HEIGHT   (BOARD_WIDTH)

#define MAX_PGN_LEN     8192
#define MAX_MOVES       256

// -------------------------
// Enumerations and Typedefs
// -------------------------

typedef enum {
    MAIN_MENU,
    CHESS_BOARD
} GameState;

typedef struct {
    int from_r, from_c;
    int to_r, to_c;
} Move;

// -------------------------
// Global Constants
// -------------------------

static const int PST_PAWN[8][8] = {
    {0, 0, 0, 0, 0, 0, 0, 0},
    {50, 50, 50, 50, 50, 50, 50, 50},
    {10, 10, 20, 30, 30, 20, 10, 10},
    {5, 5, 10, 25, 25, 10, 5, 5},
    {0, 0, 0, 20, 20, 0, 0, 0},
    {5, -5, -10, 0, 0, -10, -5, 5},
    {5, 10, 10, -20, -20, 10, 10, 5},
    {0, 0, 0, 0, 0, 0, 0, 0}
};

static const int VALS[128] = {
    ['P'] = 100, ['N'] = 320, ['B'] = 330, ['R'] = 500, ['Q'] = 900, ['K'] = 20000,
    ['p'] = -100, ['n'] = -320, ['b'] = -330, ['r'] = -500, ['q'] = -900, ['k'] = -20000
};

// -------------------------
// Global Variables: SDL
// -------------------------

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *textures[128];
TTF_Font *font = NULL;
TTF_Font *smallFont = NULL;

// -------------------------
// Global Variables: UI Layout
// -------------------------

SDL_Rect playButton = {WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 - 200, 200, 100};
SDL_Rect botButton = {WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 - 50, 200, 100};
SDL_Rect pgnButton = {WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 + 100, 200, 100};
SDL_Rect backButton = {WINDOW_WIDTH - 90, 10, 80, 30};
SDL_Rect savePGNButton = {WINDOW_WIDTH - 180, 10, 80, 30};

// -------------------------
// Global Variables: Game State
// -------------------------

GameState currentState = MAIN_MENU;

char board[BOARD_SIZE][BOARD_SIZE] = {
    {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'},
    {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
    {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}
};

int selectedRow = -1;
int selectedCol = -1;
bool pieceSelected = false;
int currentTurn = 0; // 0 = White to move, 1 = Black to move
bool validMoves[BOARD_SIZE][BOARD_SIZE] = {false};

char whiteCaptured[32] = {0}; // pieces captured from White
char blackCaptured[32] = {0}; // pieces captured from Black
int whiteCapCount = 0;
int blackCapCount = 0;

bool playWithBot = false;
int botPlaysColor = 1; // 0 = White, 1 = Black

char imageBasePath[256];
char fontPath[256];

int enPassantRow = -1;
int enPassantCol = -1;

bool whiteKingMoved = false;
bool whiteKingsideRookMoved = false;
bool whiteQueensideRookMoved = false;
bool blackKingMoved = false;
bool blackKingsideRookMoved = false;
bool blackQueensideRookMoved = false;

bool awaitingPromotion = false;
int promoRow = -1;
int promoCol = -1;
char promoColor = ' '; // 'w' or 'b'
bool promotionJustCompleted = false;

char pgnMoves[MAX_PGN_LEN] = {0};
int fullMoveNumber = 1;

static Move moveList[MAX_MOVES];
static int moveCount = 0;

// -------------------------
// Function Prototypes
// -------------------------

// Initialization / Cleanup
void initSDL();

void loadPaths();

void loadFonts();

void loadPieceTextures();

void cleanupSDL();

// Game State Management
void resetGameState();

void saveGame(const char *filename, int turn);

void loadGame(const char *filename, int *turn);

// PGN and SAN Handling
void logMoveToPGN(const char *mv, char pieceMoved, bool isCapture, bool isCheck, bool isCheckmate);

void savePGN(const char *filename);

void applySANMove(const char *san);

void playPGNFile(const char *filename);

// Valid Moves / Move Generation
int isWhitePiece(char piece);

int isPathClear(int r1, int c1, int r2, int c2);

void findKingPosition(int color, int *kr, int *kc);

int isValidMove(int r1, int c1, int r2, int c2, int turn);

int canCaptureSquare(int r1, int c1, int r2, int c2);

int isKingInCheck(int color);

void computeValidMoves(int r, int c);

int hasAnyLegalMove(int color);

void generateMoves(int color);

void movePieceStoringLog(const char *mv);

// Evaluation and Engine
int evaluateStatic();

int evaluateDynamic(int color);

int alphabeta(int depth, int alpha, int beta, int color);

Move findBestMove(int depth, int color, int *outScore);

void engineMove(int depth);

// Rendering / UI
void drawTextWithFont(const char *text, SDL_Rect rect, TTF_Font *fontToUse);

void drawText(const char *text, SDL_Rect rect);

void drawButton(SDL_Rect rect, const char *text);

void renderMainMenu();

void getPiecePath(char piece, char *outPath);

void renderBoard();

void drawCapturedPieces();

const char *getGameStatusText();

void drawTurnIndicator(int turn);

void renderBoardWithBack();

// Event Handling
void handleMouseClick(int mx, int my);

// -------------------------
// Initialization / Cleanup
// -------------------------

void initSDL() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        exit(1);
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF init error: %s\n", TTF_GetError());
        exit(1);
    }

    window = SDL_CreateWindow(
        "Chess",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        exit(1);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        exit(1);
    }
}

void loadPaths() {
    FILE *file = fopen("paths.txt", "r");
    if (!file) {
        fprintf(stderr, "Error: cannot open paths.txt\n");
        exit(1);
    }

    if (!fgets(imageBasePath, sizeof(imageBasePath), file)) {
        fprintf(stderr, "Error reading imageBasePath\n");
        fclose(file);
        exit(1);
    }
    imageBasePath[strcspn(imageBasePath, "\n")] = '\0';

    if (!fgets(fontPath, sizeof(fontPath), file)) {
        fprintf(stderr, "Error reading fontPath\n");
        fclose(file);
        exit(1);
    }
    fontPath[strcspn(fontPath, "\n")] = '\0';
    fclose(file);
}

void loadFonts() {
    font = TTF_OpenFont(fontPath, 24);
    if (!font) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
        exit(1);
    }

    smallFont = TTF_OpenFont(fontPath, 16);
    if (!smallFont) {
        fprintf(stderr, "Failed to load small font: %s\n", TTF_GetError());
        exit(1);
    }
}

void loadPieceTextures() {
    for (int i = 0; i < 128; i++) {
        textures[i] = NULL;
    }

    char types[] = {'P', 'R', 'N', 'B', 'Q', 'K', 'p', 'r', 'n', 'b', 'q', 'k'};
    int typeCount = sizeof(types) / sizeof(types[0]);

    for (int i = 0; i < typeCount; i++) {
        char path[256];
        getPiecePath(types[i], path);

        SDL_Surface *surf = IMG_Load(path);
        if (!surf) {
            fprintf(stderr, "Failed to load %s: %s\n", path, IMG_GetError());
            exit(1);
        }
        textures[(int) types[i]] = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_FreeSurface(surf);
    }
}

void cleanupSDL() {
    for (int i = 0; i < 128; i++) {
        if (textures[i]) SDL_DestroyTexture(textures[i]);
    }
    if (smallFont) TTF_CloseFont(smallFont);
    if (font) TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}

// -------------------------
// Game State Management
// -------------------------

void resetGameState() {
    char defaultBoard[BOARD_SIZE][BOARD_SIZE] = {
        {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'},
        {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '},
        {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
        {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}
    };

    memcpy(board, defaultBoard, sizeof(board));
    selectedRow = -1;
    selectedCol = -1;
    pieceSelected = false;
    currentTurn = 0;
    memset(validMoves, 0, sizeof(validMoves));
    memset(whiteCaptured, 0, sizeof(whiteCaptured));
    memset(blackCaptured, 0, sizeof(blackCaptured));
    whiteCapCount = 0;
    blackCapCount = 0;
    playWithBot = false;
    botPlaysColor = 1;
    moveCount = 0;
    enPassantRow = -1;
    enPassantCol = -1;
    whiteKingMoved = false;
    whiteKingsideRookMoved = false;
    whiteQueensideRookMoved = false;
    blackKingMoved = false;
    blackKingsideRookMoved = false;
    blackQueensideRookMoved = false;
    awaitingPromotion = false;
    promoRow = -1;
    promoCol = -1;
    promoColor = ' ';
    promotionJustCompleted = false;
    memset(pgnMoves, 0, sizeof(pgnMoves));
    fullMoveNumber = 1;
}

void saveGame(const char *filename, int turn) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Error: Could not open %s for saving.\n", filename);
        return;
    }
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            fputc(board[r][c], f);
        }
        fputc('\n', f);
    }
    fprintf(f, "%d\n", turn);
    fclose(f);
    printf("Game saved to %s.\n", filename);
}

void loadGame(const char *filename, int *turn) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Error: Could not open %s for loading.\n", filename);
        return;
    }
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            int ch = fgetc(f);
            if (ch == EOF) {
                printf("Error: Incomplete board data.\n");
                fclose(f);
                return;
            }
            board[r][c] = (char) ch;
        }
        fgetc(f); // consume newline
    }
    if (fscanf(f, "%d", turn) != 1) {
        printf("Error: Could not read turn number.\n");
    }
    fclose(f);
    printf("Game loaded from %s.\n", filename);
}

// -------------------------
// PGN and SAN Handling
// -------------------------

void logMoveToPGN(const char *mv, char pieceMoved, bool isCapture, bool isCheck, bool isCheckmate) {
    char buffer[32] = {0};
    char pieceChar = (tolower(pieceMoved) == 'p') ? '\0' : toupper(pieceMoved);
    char moveStr[16];

    if (pieceChar) {
        // e.g., Nf3 → pieceChar = 'N', mv "+2" is target square
        sprintf(moveStr, "%c%s", pieceChar, mv + 2);
    } else {
        // pawn move: "e4"
        sprintf(moveStr, "%s", mv + 2);
    }

    if (isCapture) {
        if (pieceChar == '\0') {
            // pawn capture: exd5
            sprintf(moveStr, "%cxe%c", mv[0], mv[2]);
        } else {
            // piece capture: Nxe5
            sprintf(moveStr, "%cxe%c", pieceChar, mv[2]);
        }
    }

    if (isCheckmate) {
        strcat(moveStr, "#");
    } else if (isCheck) {
        strcat(moveStr, "+");
    }

    if (currentTurn % 2 == 0) {
        // White's move → add "n. Move"
        sprintf(buffer, "%d. %s ", fullMoveNumber++, moveStr);
    } else {
        // Black's move → just add the move
        sprintf(buffer, "%s ", moveStr);
    }

    strcat(pgnMoves, buffer);
}

void savePGN(const char *filename) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Could not save PGN file\n");
        return;
    }

    fprintf(f, "[Event \"Casual Game\"]\n");
    fprintf(f, "[Site \"Local\"]\n");
    fprintf(f, "[Date \"2025.06.04\"]\n");
    fprintf(f, "[Round \"1\"]\n");
    fprintf(f, "[White \"Player1\"]\n");
    fprintf(f, "[Black \"Player2\"]\n");
    fprintf(f, "[Result \"*\"]\n\n");

    fprintf(f, "%s *\n", pgnMoves);
    fclose(f);
    printf("PGN saved to %s\n", filename);
}

void applySANMove(const char *san) {
    // Castling handling
    if (strcmp(san, "O-O") == 0 || strcmp(san, "O-O-O") == 0) {
        bool isWhite = (currentTurn % 2 == 0);
        if (strcmp(san, "O-O") == 0) {
            movePieceStoringLog(isWhite ? "e1g1" : "e8g8");
        } else {
            movePieceStoringLog(isWhite ? "e1c1" : "e8c8");
        }
        return;
    }

    char piece = 'P';
    int srcCol = -1, srcRow = -1;
    int dstCol = -1, dstRow = -1;
    bool isCapture = (strchr(san, 'x') != NULL);
    int len = strlen(san);
    bool isWhite = (currentTurn % 2 == 0);

    // Determine piece type
    if (san[0] >= 'A' && san[0] <= 'Z' && san[0] != 'O') {
        piece = san[0];
        san++; // skip piece letter
    }

    // Destination square: last two chars
    char file = san[len - 2];
    char rank = san[len - 1];
    dstCol = file - 'a';
    dstRow = '8' - rank;

    // Disambiguation (e.g., Nbd2 or R1e1)
    for (int i = 0; i < len - 2; i++) {
        if (san[i] >= 'a' && san[i] <= 'h') srcCol = san[i] - 'a';
        if (san[i] >= '1' && san[i] <= '8') srcRow = '8' - san[i];
    }

    // Search for matching piece
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char p = board[r][c];
            if (p == ' ') continue;
            if ((isWhite && !isWhitePiece(p)) || (!isWhite && isWhitePiece(p))) continue;
            if (toupper(p) != piece) continue;
            if (srcCol != -1 && c != srcCol) continue;
            if (srcRow != -1 && r != srcRow) continue;

            if (isValidMove(r, c, dstRow, dstCol, currentTurn)) {
                char coordMove[5];
                coordMove[0] = 'a' + c;
                coordMove[1] = '8' - r;
                coordMove[2] = 'a' + dstCol;
                coordMove[3] = '8' - dstRow;
                coordMove[4] = '\0';
                movePieceStoringLog(coordMove);
                return;
            }
        }
    }

    printf("SAN parser failed for move: %s\n", san);
}

void playPGNFile(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Could not open PGN file: %s\n", filename);
        return;
    }

    resetGameState();

    char token[64];
    while (fscanf(f, "%s", token) == 1) {
        if (token[0] == '[' || isdigit(token[0])) continue; // skip headers / move numbers
        if (strcmp(token, "*") == 0 ||
            strcmp(token, "1-0") == 0 ||
            strcmp(token, "0-1") == 0 ||
            strcmp(token, "1/2-1/2") == 0) {
            break;
        }
        applySANMove(token);
    }

    fclose(f);
    currentState = CHESS_BOARD;
}

// -------------------------
// Valid Moves / Move Generation
// -------------------------

int isWhitePiece(char piece) {
    return (piece != ' ' && isupper(piece));
}

int isPathClear(int r1, int c1, int r2, int c2) {
    int dr = (r2 > r1) ? 1 : (r2 < r1) ? -1 : 0;
    int dc = (c2 > c1) ? 1 : (c2 < c1) ? -1 : 0;
    int r = r1 + dr, c = c1 + dc;
    while (r != r2 || c != c2) {
        if (board[r][c] != ' ') return 0;
        r += dr;
        c += dc;
    }
    return 1;
}

void findKingPosition(int color, int *kr, int *kc) {
    char searchChar = (color == 0) ? 'K' : 'k';
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (board[r][c] == searchChar) {
                *kr = r;
                *kc = c;
                return;
            }
        }
    }
    *kr = *kc = -1;
}

int isValidMove(int r1, int c1, int r2, int c2, int turn) {
    if (r1 < 0 || r1 >= BOARD_SIZE || c1 < 0 || c1 >= BOARD_SIZE) return 0;
    if (r2 < 0 || r2 >= BOARD_SIZE || c2 < 0 || c2 >= BOARD_SIZE) return 0;
    if (r1 == r2 && c1 == c2) return 0;

    char piece = board[r1][c1];
    char targ = board[r2][c2];
    if (piece == ' ') return 0;

    // Must move your own color
    if ((turn % 2 == 0 && !isWhitePiece(piece)) ||
        (turn % 2 == 1 && isWhitePiece(piece))) {
        return 0;
    }

    // Cannot capture your own piece
    if (targ != ' ' && (isWhitePiece(targ) == isWhitePiece(piece))) return 0;

    int dr = r2 - r1;
    int dc = c2 - c1;

    switch (tolower(piece)) {
        case 'p': {
            // Pawn logic
            if (isWhitePiece(piece)) {
                // Single push
                if (dr == -1 && dc == 0 && targ == ' ') return 1;
                // Double push from rank 6
                if (r1 == 6 && dr == -2 && dc == 0 &&
                    board[r1 - 1][c1] == ' ' && targ == ' ') {
                    return 1;
                }
                // Capture
                if (dr == -1 && abs(dc) == 1 && targ != ' ' &&
                    !isWhitePiece(targ)) {
                    return 1;
                }
                // En passant for White
                if (r1 == 3 && dr == -1 && abs(dc) == 1 &&
                    board[r2][c2] == ' ' &&
                    r2 == enPassantRow && c2 == enPassantCol) {
                    return 1;
                }
            } else {
                // Black pawn logic
                if (dr == 1 && dc == 0 && targ == ' ') return 1;
                if (r1 == 1 && dr == 2 && dc == 0 &&
                    board[r1 + 1][c1] == ' ' && targ == ' ') {
                    return 1;
                }
                if (dr == 1 && abs(dc) == 1 &&
                    targ != ' ' && isWhitePiece(targ)) {
                    return 1;
                }
                // En passant for Black
                if (r1 == 4 && dr == 1 && abs(dc) == 1 &&
                    board[r2][c2] == ' ' &&
                    r2 == enPassantRow && c2 == enPassantCol) {
                    return 1;
                }
            }
            break;
        }
        case 'r':
            if (dr == 0 || dc == 0) return isPathClear(r1, c1, r2, c2);
            break;
        case 'n':
            if ((abs(dr) == 2 && abs(dc) == 1) ||
                (abs(dr) == 1 && abs(dc) == 2)) {
                return 1;
            }
            break;
        case 'b':
            if (abs(dr) == abs(dc)) return isPathClear(r1, c1, r2, c2);
            break;
        case 'q':
            if ((abs(dr) == abs(dc)) || dr == 0 || dc == 0) {
                return isPathClear(r1, c1, r2, c2);
            }
            break;
        case 'k':
            // Normal one-square king move
            if (abs(dr) <= 1 && abs(dc) <= 1) return 1;

        // Castling White
            if (isWhitePiece(piece) && r1 == 7 && c1 == 4 && !whiteKingMoved) {
                // Kingside
                if (r2 == 7 && c2 == 6 && !whiteKingsideRookMoved &&
                    board[7][5] == ' ' && board[7][6] == ' ') {
                    return 1;
                }
                // Queenside
                if (r2 == 7 && c2 == 2 && !whiteQueensideRookMoved &&
                    board[7][1] == ' ' && board[7][2] == ' ' && board[7][3] == ' ') {
                    return 1;
                }
            }
        // Castling Black
            if (!isWhitePiece(piece) && r1 == 0 && c1 == 4 && !blackKingMoved) {
                if (r2 == 0 && c2 == 6 && !blackKingsideRookMoved &&
                    board[0][5] == ' ' && board[0][6] == ' ') {
                    return 1;
                }
                if (r2 == 0 && c2 == 2 && !blackQueensideRookMoved &&
                    board[0][1] == ' ' && board[0][2] == ' ' && board[0][3] == ' ') {
                    return 1;
                }
            }
            break;
    }

    return 0;
}

int canCaptureSquare(int r1, int c1, int r2, int c2) {
    char p = board[r1][c1];
    if (p == ' ') return 0;
    if (board[r2][c2] != ' ' &&
        isWhitePiece(board[r2][c2]) == isWhitePiece(p)) {
        return 0;
    }
    int pseudoTurn = isWhitePiece(p) ? 0 : 1;
    return isValidMove(r1, c1, r2, c2, pseudoTurn);
}

int isKingInCheck(int color) {
    int kr, kc;
    findKingPosition(color, &kr, &kc);
    if (kr < 0) return 0;

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char p = board[r][c];
            if (p == ' ') continue;
            if (isWhitePiece(p) == (color == 0)) continue;
            if (canCaptureSquare(r, c, kr, kc)) return 1;
        }
    }
    return 0;
}

void computeValidMoves(int r, int c) {
    memset(validMoves, 0, sizeof(validMoves));
    for (int rr = 0; rr < BOARD_SIZE; rr++) {
        for (int cc = 0; cc < BOARD_SIZE; cc++) {
            if (isValidMove(r, c, rr, cc, currentTurn)) {
                // We skip “leaves king in check” filtering here for simplicity
                validMoves[rr][cc] = true;
            }
        }
    }
}

int hasAnyLegalMove(int color) {
    for (int r1 = 0; r1 < BOARD_SIZE; r1++) {
        for (int c1 = 0; c1 < BOARD_SIZE; c1++) {
            char p = board[r1][c1];
            if (p == ' ' || isWhitePiece(p) != (color == 0)) continue;
            for (int r2 = 0; r2 < BOARD_SIZE; r2++) {
                for (int c2 = 0; c2 < BOARD_SIZE; c2++) {
                    if (r1 == r2 && c1 == c2) continue;
                    if (!isValidMove(r1, c1, r2, c2, color)) continue;

                    // Tentatively make move
                    char saveDest = board[r2][c2];
                    board[r2][c2] = board[r1][c1];
                    board[r1][c1] = ' ';
                    int stillCheck = isKingInCheck(color);
                    // Undo
                    board[r1][c1] = board[r2][c2];
                    board[r2][c2] = saveDest;

                    if (!stillCheck) return 1;
                }
            }
        }
    }
    return 0;
}

void generateMoves(int color) {
    moveCount = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char p = board[r][c];
            if (p == ' ' || ((isWhitePiece(p) ? 0 : 1) != color)) continue;

            for (int r2 = 0; r2 < BOARD_SIZE; r2++) {
                for (int c2 = 0; c2 < BOARD_SIZE; c2++) {
                    if (!isValidMove(r, c, r2, c2, color)) continue;

                    // Tentatively apply
                    char saved = board[r2][c2];
                    board[r2][c2] = p;
                    board[r][c] = ' ';

                    if (!isKingInCheck(color)) {
                        moveList[moveCount++] = (Move){r, c, r2, c2};
                    }

                    // Undo
                    board[r][c] = p;
                    board[r2][c2] = saved;
                }
            }
        }
    }
}

void movePieceStoringLog(const char *mv) {
    int c1 = tolower(mv[0]) - 'a';
    int r1 = '8' - mv[1];
    int c2 = tolower(mv[2]) - 'a';
    int r2 = '8' - mv[3];

    if (!isValidMove(r1, c1, r2, c2, currentTurn)) {
        printf("Invalid move: %s\n", mv);
        return;
    }

    char captured = board[r2][c2];
    char mover = board[r1][c1];

    // En passant capture logic
    if (tolower(mover) == 'p' && board[r2][c2] == ' ' && c1 != c2) {
        if (isWhitePiece(board[r1][c1])) {
            captured = board[r2 + 1][c2];
            board[r2 + 1][c2] = ' ';
        } else {
            captured = board[r2 - 1][c2];
            board[r2 - 1][c2] = ' ';
        }
    } else {
        captured = board[r2][c2];
    }

    // Make the move
    board[r2][c2] = mover;
    board[r1][c1] = ' ';

    // Reset en passant target
    enPassantRow = -1;
    enPassantCol = -1;

    // If pawn moved two squares, set en passant square
    if (tolower(mover) == 'p' && abs(r2 - r1) == 2) {
        enPassantRow = (r1 + r2) / 2;
        enPassantCol = c1;
    }

    // Pawn promotion
    if ((mover == 'P' && r2 == 0) || (mover == 'p' && r2 == 7)) {
        awaitingPromotion = true;
        promoRow = r2;
        promoCol = c2;
        promoColor = isWhitePiece(mover) ? 'w' : 'b';
        return; // pause for promotion
    }

    // Castling logic
    if (tolower(mover) == 'k' && abs(c2 - c1) == 2) {
        if (mover == 'K') {
            if (c2 == 6) {
                // White kingside
                board[7][5] = board[7][7];
                board[7][7] = ' ';
            } else if (c2 == 2) {
                // White queenside
                board[7][3] = board[7][0];
                board[7][0] = ' ';
            }
        } else {
            if (c2 == 6) {
                // Black kingside
                board[0][5] = board[0][7];
                board[0][7] = ' ';
            } else if (c2 == 2) {
                // Black queenside
                board[0][3] = board[0][0];
                board[0][0] = ' ';
            }
        }
    }

    int moverColor = isWhitePiece(mover) ? 0 : 1;
    if (isKingInCheck(moverColor)) {
        // Undo move if leaves king in check
        board[r1][c1] = mover;
        board[r2][c2] = captured;
        printf("You cannot leave your king in check! Move undone.\n");
        return;
    }

    // Store captured piece
    if (captured != ' ') {
        if (isWhitePiece(captured)) {
            whiteCaptured[whiteCapCount++] = captured;
        } else {
            blackCaptured[blackCapCount++] = captured;
        }
    }

    bool inCheck = isKingInCheck(currentTurn % 2);
    bool noLegalNext = !hasAnyLegalMove(currentTurn % 2);
    logMoveToPGN(mv, mover, captured != ' ', inCheck, noLegalNext);
    currentTurn++;

    // Update castling rights
    if (mover == 'K') whiteKingMoved = true;
    if (mover == 'R') {
        if (r1 == 7 && c1 == 0) whiteQueensideRookMoved = true;
        if (r1 == 7 && c1 == 7) whiteKingsideRookMoved = true;
    }
    if (mover == 'k') blackKingMoved = true;
    if (mover == 'r') {
        if (r1 == 0 && c1 == 0) blackQueensideRookMoved = true;
        if (r1 == 0 && c1 == 7) blackKingsideRookMoved = true;
    }

    int nextColor = currentTurn % 2;
    bool inChk = isKingInCheck(nextColor);
    bool canMv = hasAnyLegalMove(nextColor);
    if (!canMv) {
        if (inChk) {
            printf("Checkmate! %s wins!\n", moverColor == 0 ? "White" : "Black");
        } else {
            printf("Stalemate! It's a draw.\n");
        }
        SDL_Delay(500);
        exit(0);
    }

    if (inChk) {
        printf("Check!\n");
    }
}

// -------------------------
// Evaluation and Engine
// -------------------------

int evaluateStatic() {
    int sc = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char p = board[r][c];
            if (!p || p == ' ') continue;
            sc += VALS[(int) p];
            if (tolower(p) == 'p') {
                if (isWhitePiece(p)) {
                    sc += PST_PAWN[r][c];
                } else {
                    sc -= PST_PAWN[7 - r][c];
                }
            }
        }
    }
    return sc;
}

int evaluateDynamic(int color) {
    int mob = 0;
    int center = 0;
    generateMoves(color);
    mob = moveCount;

    const int centers[4][2] = {{3, 3}, {3, 4}, {4, 3}, {4, 4}};
    for (int i = 0; i < 4; i++) {
        char p = board[centers[i][0]][centers[i][1]];
        if (p != ' ' && ((isWhitePiece(p) ? 0 : 1) == color)) {
            center++;
        }
    }
    return mob * 10 + center * 25;
}

int alphabeta(int depth, int alpha, int beta, int color) {
    int opp = 1 - color;

    if (depth == 0) {
        int stat = evaluateStatic();
        int dyn = evaluateDynamic(color) - evaluateDynamic(opp);
        return stat + dyn;
    }

    generateMoves(color);
    if (moveCount == 0) {
        if (isKingInCheck(color)) return -100000 + (8 - depth);
        return 0; // stalemate
    }

    for (int i = 0; i < moveCount; i++) {
        Move m = moveList[i];
        char movingPiece = board[m.from_r][m.from_c];
        char captured = board[m.to_r][m.to_c];

        // Make move
        board[m.to_r][m.to_c] = movingPiece;
        board[m.from_r][m.from_c] = ' ';

        // Skip if leaves king in check
        if (isKingInCheck(color)) {
            board[m.from_r][m.from_c] = movingPiece;
            board[m.to_r][m.to_c] = captured;
            continue;
        }

        int val = -alphabeta(depth - 1, -beta, -alpha, opp);

        // Undo
        board[m.from_r][m.from_c] = movingPiece;
        board[m.to_r][m.to_c] = captured;

        if (val >= beta) return beta;
        if (val > alpha) alpha = val;
    }

    return alpha;
}

Move findBestMove(int depth, int color, int *outScore) {
    Move best = {0, 0, 0, 0};
    int alpha = -1000000;

    generateMoves(color);

    for (int i = 0; i < moveCount; i++) {
        Move m = moveList[i];
        char movingPiece = board[m.from_r][m.from_c];
        char saved = board[m.to_r][m.to_c];

        // Make move
        board[m.to_r][m.to_c] = movingPiece;
        board[m.from_r][m.from_c] = ' ';

        int sc = -alphabeta(depth - 1, -1000000, 1000000, 1 - color);

        // Undo
        board[m.from_r][m.from_c] = movingPiece;
        board[m.to_r][m.to_c] = saved;

        if (sc > alpha) {
            alpha = sc;
            best = m;
        }
    }

    *outScore = alpha;
    return best;
}

void engineMove(int depth) {
    int score;
    Move m = findBestMove(depth, currentTurn % 2, &score);

    generateMoves(currentTurn % 2);
    if (moveCount == 0) {
        int col = currentTurn % 2;
        if (isKingInCheck(col)) {
            printf("Checkmate! %s wins!\n", (col == 0) ? "Black" : "White");
        } else {
            printf("Stalemate! It's a draw.\n");
        }
        SDL_Delay(500);
        exit(0);
    }

    char mv[6] = {
        (char) ('a' + m.from_c),
        (char) ('8' - m.from_r),
        (char) ('a' + m.to_c),
        (char) ('8' - m.to_r),
        '\0'
    };

    char captured = board[m.to_r][m.to_c];
    if (captured != ' ') {
        if (isWhitePiece(captured)) {
            whiteCaptured[whiteCapCount++] = captured;
        } else {
            blackCaptured[blackCapCount++] = captured;
        }
    }

    board[m.to_r][m.to_c] = board[m.from_r][m.from_c];
    board[m.from_r][m.from_c] = ' ';

    bool inCheck = isKingInCheck(currentTurn % 2);
    bool noLegalNext = !hasAnyLegalMove(currentTurn % 2);
    logMoveToPGN(mv, board[m.to_r][m.to_c], captured != ' ', inCheck, noLegalNext);

    currentTurn++;

    // Force redraw of board and back button
    SDL_SetRenderDrawColor(renderer, 255, 192, 203, 255);
    SDL_RenderClear(renderer);
    renderBoardWithBack();
    SDL_RenderPresent(renderer);
    SDL_Delay(200);

    int nxtColor = currentTurn % 2;
    bool inChk = isKingInCheck(nxtColor);
    bool canMv = hasAnyLegalMove(nxtColor);

    if (!canMv) {
        if (inChk) {
            printf("Checkmate! %s wins!\n", (nxtColor == 0) ? "Black" : "White");
        } else {
            printf("Stalemate! It's a draw.\n");
        }
        SDL_Delay(200);
        exit(0);
    }

    if (inChk) {
        printf("Check!\n");
    }

    // Clear any old selection
    pieceSelected = false;
    memset(validMoves, 0, sizeof(validMoves));
}

// -------------------------
// Rendering / UI
// -------------------------

void drawTextWithFont(const char *text, SDL_Rect rect, TTF_Font *fontToUse) {
    SDL_Color color = {0, 0, 0, 255};
    SDL_Surface *surface = TTF_RenderText_Blended(fontToUse, text, color);
    if (!surface) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!tex) return;

    int texW = 0, texH = 0;
    SDL_QueryTexture(tex, NULL, NULL, &texW, &texH);

    SDL_Rect dst = {
        rect.x + (rect.w - texW) / 2,
        rect.y + (rect.h - texH) / 2,
        texW, texH
    };
    SDL_RenderCopy(renderer, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

void drawText(const char *text, SDL_Rect rect) {
    drawTextWithFont(text, rect, font);
}

void drawButton(SDL_Rect rect, const char *text) {
    // Pinkish fill
    SDL_SetRenderDrawColor(renderer, 230, 168, 175, 255);
    SDL_RenderFillRect(renderer, &rect);

    // Border
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &rect);

    // Text
    drawText(text, rect);
}

void renderMainMenu() {
    SDL_SetRenderDrawColor(renderer, 255, 192, 203, 255); // Light pink
    SDL_RenderClear(renderer);

    drawButton(playButton, "Play Human");
    drawButton(botButton, "Play with Bot");
    drawButton(pgnButton, "Play again");

    SDL_RenderPresent(renderer);
}

void getPiecePath(char piece, char *outPath) {
    if (piece == ' ') {
        outPath[0] = '\0';
        return;
    }
    const char *color = isupper(piece) ? "white" : "black";
    char type = tolower(piece);
    const char *name;

    switch (type) {
        case 'p': name = "pawn";
            break;
        case 'r': name = "rook";
            break;
        case 'n': name = "knight";
            break;
        case 'b': name = "bishop";
            break;
        case 'q': name = "queen";
            break;
        case 'k': name = "king";
            break;
        default: name = "unknown";
            break;
    }
    sprintf(outPath, "%s/%s_%s.png", imageBasePath, color, name);
}

void renderBoard() {
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            SDL_Rect tile = {col * TILE_SIZE, row * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            bool light = ((row + col) % 2 == 0);

            if (light) {
                SDL_SetRenderDrawColor(renderer, 240, 217, 181, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 181, 136, 99, 255);
            }
            SDL_RenderFillRect(renderer, &tile);

            // Highlight valid‐move squares
            if (validMoves[row][col]) {
                SDL_SetRenderDrawColor(renderer, 102, 240, 102, 100);
                SDL_RenderFillRect(renderer, &tile);
            }

            // Highlight selected square
            if (pieceSelected && row == selectedRow && col == selectedCol) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_RenderDrawRect(renderer, &tile);
            }

            // Draw piece if any
            char piece = board[row][col];
            if (piece != ' ' && textures[(int) piece]) {
                SDL_RenderCopy(renderer, textures[(int) piece], NULL, &tile);
            }
        }
    }
}

void drawCapturedPieces() {
    // Captured by White (black pieces taken by white)
    SDL_Rect label1 = {BOARD_WIDTH + 20, WINDOW_HEIGHT - 120, 260, 20};
    drawTextWithFont("Captured by White:", label1, smallFont);

    const int CAPSZ = 25;
    SDL_Rect blackRect = {BOARD_WIDTH + 20, label1.y + 25, CAPSZ, CAPSZ};
    for (int i = 0; i < blackCapCount; i++) {
        char piece = blackCaptured[i];
        if (textures[(int) piece]) {
            SDL_RenderCopy(renderer, textures[(int) piece], NULL, &blackRect);
            blackRect.x += CAPSZ + 2;
        }
    }

    // Captured by Black (white pieces taken by black)
    SDL_Rect label2 = {BOARD_WIDTH + 20, blackRect.y + 35, 260, 20};
    drawTextWithFont("Captured by Black:", label2, smallFont);

    SDL_Rect whiteRect = {BOARD_WIDTH + 20, label2.y + 25, CAPSZ, CAPSZ};
    for (int i = 0; i < whiteCapCount; i++) {
        char piece = whiteCaptured[i];
        if (textures[(int) piece]) {
            SDL_RenderCopy(renderer, textures[(int) piece], NULL, &whiteRect);
            whiteRect.x += CAPSZ + 2;
        }
    }
}

const char *getGameStatusText() {
    int color = currentTurn % 2;
    bool inCheck = isKingInCheck(color);
    bool canMove = hasAnyLegalMove(color);

    if (!canMove) {
        if (inCheck) {
            return (color == 0) ? "Checkmate! Black wins" : "Checkmate! White wins";
        } else {
            return "Stalemate";
        }
    }
    if (inCheck) {
        return "Check";
    }
    return "";
}

void drawTurnIndicator(int turn) {
    const char *turnText = (turn % 2 == 0) ? "White's Turn" : "Black's Turn";
    SDL_Rect textRect = {BOARD_WIDTH + 20, 100, 260, 40};
    drawText(turnText, textRect);

    const char *status = getGameStatusText();
    if (strlen(status) > 0) {
        SDL_Rect statusRect = {BOARD_WIDTH + 20, 140, 260, 30};
        drawTextWithFont(status, statusRect, smallFont);
    }
}

void renderBoardWithBack() {
    renderBoard();
    drawButton(backButton, "Back");
    drawButton(savePGNButton, "Save");
    drawTurnIndicator(currentTurn);
    drawCapturedPieces();
}

// -------------------------
// Event Handling
// -------------------------

void handleMouseClick(int mx, int my) {
    if (awaitingPromotion) {
        const char *options = "qrbn";
        for (int i = 0; i < 4; i++) {
            SDL_Rect optRect = {BOARD_WIDTH + 40 + i * 60, 200, 50, 50};
            if (mx >= optRect.x && mx <= optRect.x + optRect.w &&
                my >= optRect.y && my <= optRect.y + optRect.h) {
                char chosen = (promoColor == 'w') ? toupper(options[i]) : tolower(options[i]);
                board[promoRow][promoCol] = chosen;
                awaitingPromotion = false;
                promotionJustCompleted = true;
                currentTurn++;
                break;
            }
        }
        return;
    }

    if (currentState == MAIN_MENU) {
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &playButton)) {
            playWithBot = false;
            currentState = CHESS_BOARD;
        } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &botButton)) {
            playWithBot = true;
            currentState = CHESS_BOARD;
        } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &pgnButton)) {
            playPGNFile("game.pgn");
        }
    } else if (currentState == CHESS_BOARD) {
        // “Back” button
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &backButton)) {
            resetGameState();
            currentState = MAIN_MENU;
            return;
        }
        // “Save PGN” button
        if (SDL_PointInRect(&(SDL_Point){mx, my}, &savePGNButton)) {
            savePGN("game.pgn");
            return;
        }

        int col = mx / TILE_SIZE;
        int row = my / TILE_SIZE;
        if (row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE) {
            // clicked on side panel; ignore
            return;
        }

        if (!pieceSelected) {
            // Select a piece
            char p = board[row][col];
            if (p != ' ' &&
                ((currentTurn % 2 == 0 && isWhitePiece(p)) ||
                 (currentTurn % 2 == 1 && !isWhitePiece(p)))) {
                selectedRow = row;
                selectedCol = col;
                pieceSelected = true;
                computeValidMoves(row, col);
            }
        } else {
            // Already have a piece selected
            if (validMoves[row][col]) {
                // Do the human move
                char mv[6] = {
                    (char) ('a' + selectedCol),
                    (char) ('8' - selectedRow),
                    (char) ('a' + col),
                    (char) ('8' - row),
                    '\0'
                };
                movePieceStoringLog(mv);

                // Redraw after human move
                SDL_SetRenderDrawColor(renderer, 255, 192, 203, 255);
                SDL_RenderClear(renderer);
                renderBoardWithBack();
                SDL_Delay(200);

                if (playWithBot && currentTurn % 2 == botPlaysColor) {
                    SDL_Delay(300);
                    engineMove(1);
                }
            }
            // Clear selection
            pieceSelected = false;
            memset(validMoves, 0, sizeof(validMoves));
        }
    }
}

// -------------------------
// Main
// -------------------------

int main(int argc, char *argv[]) {
    initSDL();
    loadPaths();
    loadFonts();
    loadPieceTextures();

    // As soon as we enter CHESS_BOARD, force an initial draw
    bool firstBoardDrawn = false;

    SDL_Event e;
    bool running = true;
    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) {
                running = false;
                break;
            }
            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                handleMouseClick(e.button.x, e.button.y);
            }
        }

        // Render loop
        SDL_SetRenderDrawColor(renderer, 255, 192, 203, 255);
        SDL_RenderClear(renderer);

        if (currentState == MAIN_MENU) {
            renderMainMenu();
        } else if (currentState == CHESS_BOARD) {
            renderBoardWithBack();

            // Always render promotion options if needed
            if (awaitingPromotion) {
                const char *options = "qrbn";
                for (int i = 0; i < 4; i++) {
                    char piece = (promoColor == 'w') ? toupper(options[i]) : tolower(options[i]);
                    SDL_Rect optRect = {BOARD_WIDTH + 40 + i * 60, 200, 50, 50};
                    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
                    SDL_RenderFillRect(renderer, &optRect);
                    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                    SDL_RenderDrawRect(renderer, &optRect);
                    if (textures[(int) piece]) {
                        SDL_RenderCopy(renderer, textures[(int) piece], NULL, &optRect);
                    }
                }
            }

            SDL_RenderPresent(renderer);

            if (!firstBoardDrawn) {
                renderBoardWithBack();
                SDL_RenderPresent(renderer);
                firstBoardDrawn = true;
            }
        }

        SDL_Delay(20);
    }

    cleanupSDL();
    return 0;
}
