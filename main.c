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

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *textures[128];
TTF_Font *font = NULL;
TTF_Font *smallFont = NULL;

typedef enum { MAIN_MENU, CHESS_BOARD } GameState;

GameState currentState = MAIN_MENU;

SDL_Rect playButton = {WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 - 50, 200, 100};
SDL_Rect botButton = {WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 + 70, 200, 100};
SDL_Rect backButton = {WINDOW_WIDTH - 90, 10, 80, 30};

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

int selectedRow = -1, selectedCol = -1;
bool pieceSelected = false;
int currentTurn = 0; // 0 = White to move, 1 = Black to move
bool validMoves[BOARD_SIZE][BOARD_SIZE] = {false};

// Captured‐piece stacks (for display)
char whiteCaptured[32] = {0}; // pieces captured from White (i.e. Black took them)
char blackCaptured[32] = {0}; // pieces captured from Black (i.e. White took them)
int whiteCapCount = 0;
int blackCapCount = 0;

bool playWithBot = false;
int botPlaysColor = 1; // 0 = white, 1 = black
char imageBasePath[256];
char fontPath[256];

typedef struct {
    int from_r, from_c;
    int to_r, to_c;
} Move;

#define MAX_MOVES 256
static Move moveList[MAX_MOVES];
static int moveCount;

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
}


// ---------------------
// Utility: Draw Text
// ---------------------
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
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderClear(renderer);
    drawButton(playButton, "Play Human");
    drawButton(botButton, "Play with Bot");
    SDL_RenderPresent(renderer);
}

void drawCapturedPieces() {
    // Label: “Captured by White” (i.e. black's pieces taken by white)
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

    // Label: “Captured by Black” (i.e. white's pieces taken by black)
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

void renderBoard();

// Renders the board plus the “Back” button, turn indicator, captured‐pieces.


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

void loadPieceTextures() {
    // Initialize all to NULL
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

void renderBoard() {
    // Draw all 64 tiles
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

            // highlight valid‐move squares
            if (validMoves[row][col]) {
                SDL_SetRenderDrawColor(renderer, 102, 240, 102, 100);
                SDL_RenderFillRect(renderer, &tile);
            }

            // highlight selected square
            if (pieceSelected && row == selectedRow && col == selectedCol) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_RenderDrawRect(renderer, &tile);
            }

            // draw piece if any
            char piece = board[row][col];
            if (piece != ' ' && textures[(int) piece]) {
                SDL_RenderCopy(renderer, textures[(int) piece], NULL, &tile);
            }
        }
    }
}

//---------------------
// Move‐generation / rules
//---------------------
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

int isValidMove(int r1, int c1, int r2, int c2, int turn) {
    // Bounds check
    if (r1 < 0 || r1 >= 8 || c1 < 0 || c1 >= 8 || r2 < 0 || r2 >= 8 || c2 < 0 || c2 >= 8) return 0;
    // Same square
    if (r1 == r2 && c1 == c2) return 0;
    char piece = board[r1][c1];
    char targ = board[r2][c2];
    if (piece == ' ') return 0;
    // Must move your own color
    if ((turn % 2) == 0 && !isWhitePiece(piece)) return 0;
    if ((turn % 2) == 1 && isWhitePiece(piece)) return 0;
    // Cannot capture your own side
    if (targ != ' ' && isWhitePiece(targ) == isWhitePiece(piece)) return 0;

    int dr = r2 - r1, dc = c2 - c1;
    switch (tolower(piece)) {
        case 'p': {
            if (isWhitePiece(piece)) {
                // move one forward
                if (dr == -1 && dc == 0 && targ == ' ') return 1;
                // first‐move two forward
                if (r1 == 6 && dr == -2 && dc == 0 && board[r1 - 1][c1] == ' ' && targ == ' ') return 1;
                // capture diagonally
                if (dr == -1 && abs(dc) == 1 && targ != ' ' && !isWhitePiece(targ)) return 1;
            } else {
                // black pawn goes “down”
                if (dr == 1 && dc == 0 && targ == ' ') return 1;
                if (r1 == 1 && dr == 2 && dc == 0 && board[r1 + 1][c1] == ' ' && targ == ' ') return 1;
                if (dr == 1 && abs(dc) == 1 && targ != ' ' && isWhitePiece(targ)) return 1;
            }
            break;
        }
        case 'r':
            if (dr == 0 || dc == 0) return isPathClear(r1, c1, r2, c2);
            break;
        case 'n':
            if ((abs(dr) == 2 && abs(dc) == 1) || (abs(dr) == 1 && abs(dc) == 2)) return 1;
            break;
        case 'b':
            if (abs(dr) == abs(dc)) return isPathClear(r1, c1, r2, c2);
            break;
        case 'q':
            if ((abs(dr) == abs(dc)) || dr == 0 || dc == 0) return isPathClear(r1, c1, r2, c2);
            break;
        case 'k':
            if (abs(dr) <= 1 && abs(dc) <= 1) return 1;
            break;
    }
    return 0;
}

void computeValidMoves(int r, int c) {
    memset(validMoves, 0, sizeof(validMoves));
    for (int rr = 0; rr < 8; rr++) {
        for (int cc = 0; cc < 8; cc++) {
            if (isValidMove(r, c, rr, cc, currentTurn)) {
                // We also must check: does this leave our king in check?
                // For simplicity right now, we skip “in‐check” filtering here.
                validMoves[rr][cc] = true;
            }
        }
    }
}

void findKingPosition(int color, int *kr, int *kc) {
    char searchChar = (color == 0) ? 'K' : 'k';
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (board[r][c] == searchChar) {
                *kr = r;
                *kc = c;
                return;
            }
    *kr = *kc = -1;
}

int canCaptureSquare(int r1, int c1, int r2, int c2) {
    char p = board[r1][c1];
    if (p == ' ') return 0;
    if (board[r2][c2] != ' ' && isWhitePiece(board[r2][c2]) == isWhitePiece(p))
        return 0;
    int pseudoTurn = isWhitePiece(p) ? 0 : 1;
    return isValidMove(r1, c1, r2, c2, pseudoTurn);
}

int isKingInCheck(int color) {
    int kr, kc;
    findKingPosition(color, &kr, &kc);
    if (kr < 0) return 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char p = board[r][c];
            if (p == ' ') continue;
            if (isWhitePiece(p) == (color == 0)) continue;
            if (canCaptureSquare(r, c, kr, kc)) return 1;
        }
    }
    return 0;
}

int hasAnyLegalMove(int color) {
    for (int r1 = 0; r1 < 8; r1++) {
        for (int c1 = 0; c1 < 8; c1++) {
            char p = board[r1][c1];
            if (p == ' ' || isWhitePiece(p) != (color == 0)) continue;
            for (int r2 = 0; r2 < 8; r2++) {
                for (int c2 = 0; c2 < 8; c2++) {
                    if (r1 == r2 && c1 == c2) continue;
                    if (!isValidMove(r1, c1, r2, c2, color)) continue;
                    // Make the move “tentatively”
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

void logMove(const char *mv) {
    FILE *f = fopen("moves.txt", "a");
    if (f) {
        fprintf(f, "%s\n", mv);
        fclose(f);
    }
}

const char *getGameStatusText() {
    int color = currentTurn % 2;
    int inCheck = isKingInCheck(color);
    int canMove = hasAnyLegalMove(color);

    if (!canMove) {
        if (inCheck)
            return (color == 0) ? "Checkmate! Black wins" : "Checkmate! White wins";
        else
            return "Stalemate";
    }

    if (inCheck)
        return "Check";

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
    drawTurnIndicator(currentTurn);
    drawCapturedPieces();
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

    // Make the move
    board[r2][c2] = mover;
    board[r1][c1] = ' ';

    int moverColor = isWhitePiece(mover) ? 0 : 1;
    if (isKingInCheck(moverColor)) {
        // Undo
        board[r1][c1] = mover;
        board[r2][c2] = captured;
        printf("You cannot leave your king in check! Move undone.\n");
        return;
    }

    // If capture happened, store it
    if (captured != ' ') {
        if (isWhitePiece(captured)) {
            whiteCaptured[whiteCapCount++] = captured;
        } else {
            blackCaptured[blackCapCount++] = captured;
        }
    }
    logMove(mv);
    currentTurn++;

    int nextColor = currentTurn % 2;
    int inCheck = isKingInCheck(nextColor);
    int canMove = hasAnyLegalMove(nextColor);
    if (!canMove) {
        if (inCheck)
            printf("Checkmate! %s wins!\n", moverColor == 0 ? "White" : "Black");
        else
            printf("Stalemate! It's a draw.\n");
        SDL_Delay(500);
        exit(0);
    }
    if (inCheck) {
        printf("Check!\n");
    }
}

void saveGame(const char *filename, int turn) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Error: Could not open %s for saving.\n", filename);
        return;
    }
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
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
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            int ch = fgetc(f);
            if (ch == EOF) {
                printf("Error: Incomplete board data.\n");
                fclose(f);
                return;
            }
            board[r][c] = (char) ch;
        }
        fgetc(f); // newline
    }
    if (fscanf(f, "%d", turn) != 1) {
        printf("Error: Could not read turn number.\n");
    }
    fclose(f);
    printf("Game loaded from %s.\n", filename);
}


// Generate pseudolegal moves (does not yet filter out “leaves king in check”)
void generateMoves(int color) {
    moveCount = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char p = board[r][c];
            if (p == ' ') continue;
            if ((isWhitePiece(p) ? 0 : 1) != color) continue;

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

// Static evaluation: material + PST for pawns
int evaluateStatic() {
    static const int VALS[128] = {
        ['P'] = 100, ['N'] = 320, ['B'] = 330, ['R'] = 500, ['Q'] = 900, ['K'] = 20000,
        ['p'] = -100, ['n'] = -320, ['b'] = -330, ['r'] = -500, ['q'] = -900, ['k'] = -20000
    };
    int sc = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char p = board[r][c];
            if (!p) continue;
            sc += VALS[(int) p];
            if (tolower(p) == 'p') {
                if (isWhitePiece(p))
                    sc += PST_PAWN[r][c];
                else
                    sc -= PST_PAWN[7 - r][c];
            }
        }
    }
    return sc;
}

// Dynamic evaluation: mobility + center-control
int evaluateDynamic(int color) {
    int mob = 0, center = 0;
    generateMoves(color);
    mob = moveCount;
    const int centers[4][2] = {{3, 3}, {3, 4}, {4, 3}, {4, 4}};
    for (int i = 0; i < 4; i++) {
        char p = board[centers[i][0]][centers[i][1]];
        if (p != ' ' && ((isWhitePiece(p) ? 0 : 1) == color)) center++;
    }
    return mob * 10 + center * 25;
}

// Alpha–beta
int alphabeta(int depth, int alpha, int beta, int color) {
    int opp = 1 - color;
    if (depth == 0) {
        int stat = evaluateStatic();
        int dyn = evaluateDynamic(color) - evaluateDynamic(opp);
        return stat + dyn;
    }
    generateMoves(color);
    if (moveCount == 0) {
        // Checkmate or stalemate
        if (isKingInCheck(color)) return -100000 + (8 - depth);
        else return 0;
    }
    for (int i = 0; i < moveCount; i++) {
        Move m = moveList[i];
        char movingPiece = board[m.from_r][m.from_c];
        char captured = board[m.to_r][m.to_c];

        // Make the move
        board[m.to_r][m.to_c] = movingPiece;
        board[m.from_r][m.from_c] = ' ';

        // LEGALITY CHECK: skip if own king is left in check
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

        // Make the move
        board[m.to_r][m.to_c] = movingPiece;
        board[m.from_r][m.from_c] = ' ';

        // Evaluate the position recursively
        int sc = -alphabeta(depth - 1, -1000000, 1000000, 1 - color);

        // Undo the move
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

    if (moveCount == 0) {
        int col = currentTurn % 2;
        if (isKingInCheck(col))
            printf("Checkmate! %s wins!\n", (col == 0) ? "Black" : "White");
        else
            printf("Stalemate! It's a draw.\n");
        SDL_Delay(500);
        exit(0);
    }

    // Construct “e2e4” style string
    char mv[6] = {
        (char) ('a' + m.from_c),
        (char) ('8' - m.from_r),
        (char) ('a' + m.to_c),
        (char) ('8' - m.to_r),
        '\0'
    };

    // If capture
    char captured = board[m.to_r][m.to_c];
    if (captured != ' ') {
        if (isWhitePiece(captured))
            whiteCaptured[whiteCapCount++] = captured;
        else
            blackCaptured[blackCapCount++] = captured;
    }
    // Make it
    board[m.to_r][m.to_c] = board[m.from_r][m.from_c];
    board[m.from_r][m.from_c] = ' ';
    logMove(mv);
    currentTurn++;

    // After engine move, force a redraw
    SDL_SetRenderDrawColor(renderer, 255, 192, 203, 255);
    SDL_RenderClear(renderer);
    renderBoardWithBack();
    SDL_RenderPresent(renderer);
    SDL_Delay(200); // so human can see engine's move

    int nxtColor = currentTurn % 2;
    int inChk = isKingInCheck(nxtColor);
    int canMv = hasAnyLegalMove(nxtColor);
    if (!canMv) {
        if (inChk)
            printf("Checkmate! %s wins!\n", (nxtColor == 0) ? "Black" : "White");
        else
            printf("Stalemate! It's a draw.\n");
        SDL_Delay(200);
        exit(0);
    }
    if (inChk) printf("Check!\n");

    // Clear any old selection
    pieceSelected = false;
    memset(validMoves, 0, sizeof(validMoves));
}

// --------------
// Main
// --------------
int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        fprintf(stderr, "SDL init error: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() == -1) {
        fprintf(stderr, "TTF init error: %s\n", TTF_GetError());
        return 1;
    }

    // Read paths.txt
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

    font = TTF_OpenFont(fontPath, 24);
    if (!font) {
        fprintf(stderr, "Failed to load font: %s\n", TTF_GetError());
        return 1;
    }
    smallFont = TTF_OpenFont(fontPath, 16);
    if (!smallFont) {
        fprintf(stderr, "Failed to load small font: %s\n", TTF_GetError());
        return 1;
    }

    window = SDL_CreateWindow("Chess", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
        return 1;
    }
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
        return 1;
    }

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
                int mx = e.button.x;
                int my = e.button.y;

                if (currentState == MAIN_MENU) {
                    if (SDL_PointInRect(&(SDL_Point){mx, my}, &playButton)) {
                        playWithBot = false;
                        currentState = CHESS_BOARD;
                        firstBoardDrawn = false;
                    } else if (SDL_PointInRect(&(SDL_Point){mx, my}, &botButton)) {
                        playWithBot = true;
                        currentState = CHESS_BOARD;
                        firstBoardDrawn = false;
                    }
                } else if (currentState == CHESS_BOARD) {
                    // “Back” button
                    if (SDL_PointInRect(&(SDL_Point){mx, my}, &backButton)) {
                        resetGameState();
                        currentState = MAIN_MENU;
                        continue;
                    }
                    int col = mx / TILE_SIZE;
                    int row = my / TILE_SIZE;
                    if (row < 0 || row >= 8 || col < 0 || col >= 8) {
                        // clicked on side panel; ignore
                    } else {
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
                                SDL_RenderPresent(renderer);
                                SDL_Delay(200);

                                if (playWithBot && currentTurn % 2 == botPlaysColor) {
                                    // Give bot a short delay to think a bit
                                    SDL_Delay(300);
                                    engineMove(1);
                                }
                            }
                            // Clear selection either way
                            pieceSelected = false;
                            memset(validMoves, 0, sizeof(validMoves));
                        }
                    }
                }
            }
        }

        // Render loop
        SDL_SetRenderDrawColor(renderer, 255, 192, 203, 255);
        SDL_RenderClear(renderer);

        if (currentState == MAIN_MENU) {
            renderMainMenu();
        } else {
            if (!firstBoardDrawn) {
                renderBoardWithBack();
                SDL_RenderPresent(renderer);
                firstBoardDrawn = true;
            } else {
                renderBoardWithBack();
                SDL_RenderPresent(renderer);
            }
        }
        SDL_Delay(20); // small throttle so it’s not 100% CPU
    }

    // Cleanup
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
    return 0;
}
