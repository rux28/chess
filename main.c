#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>



void engineMove(int depth);

#define BOARD_SIZE 8
#define TILE_SIZE 70
#define BOARD_WIDTH (TILE_SIZE * BOARD_SIZE)
#define WINDOW_WIDTH (BOARD_WIDTH + 300)
#define WINDOW_HEIGHT BOARD_WIDTH

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *textures[128];
TTF_Font *font = NULL;
TTF_Font *smallFont = NULL;

typedef enum { MAIN_MENU, CHESS_BOARD } GameState;

GameState currentState = MAIN_MENU;

SDL_Rect playButton = {WINDOW_WIDTH / 2 - 100, WINDOW_HEIGHT / 2 - 50, 200, 100};
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
int currentTurn = 0;
bool validMoves[BOARD_SIZE][BOARD_SIZE] = {false};
char whiteCaptured[32] = {0}; // pieces captured from white (by black)
char blackCaptured[32] = {0}; // pieces captured from black (by white)
int whiteCapCount = 0;
int blackCapCount = 0;
char imageBasePath[256];
char fontPath[256];

void drawTextWithFont(const char *text, SDL_Rect rect, TTF_Font *fontToUse) {
    SDL_Color color = {0, 0, 0, 255};
    SDL_Surface *surface = TTF_RenderText_Blended(fontToUse, text, color);
    if (!surface) return;
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!texture) return;

    int texW = 0, texH = 0;
    SDL_QueryTexture(texture, NULL, NULL, &texW, &texH);
    SDL_Rect dstRect = {
        rect.x + (rect.w - texW) / 2,
        rect.y + (rect.h - texH) / 2,
        texW, texH
    };
    SDL_RenderCopy(renderer, texture, NULL, &dstRect);
    SDL_DestroyTexture(texture);
}

void drawText(const char *text, SDL_Rect rect) {
    drawTextWithFont(text, rect, font);
}

void drawButton(SDL_Rect rect, const char *text) {
    SDL_SetRenderDrawColor(renderer, 230, 168, 175, 255);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderDrawRect(renderer, &rect);
    drawText(text, rect);
}

void renderMainMenu() {
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderClear(renderer);
    drawButton(playButton, "Play");
    SDL_RenderPresent(renderer);
}

void renderBoard();

void drawTurnIndicator(int currentTurn) {
    const char *turnText = (currentTurn % 2 == 0) ? "White's Turn" : "Black's Turn";
    SDL_Rect textRect = {BOARD_WIDTH + 20, 100, 260, 40};
    drawText(turnText, textRect);
}

void drawCapturedPieces() {
    SDL_Rect labelRect1 = {BOARD_WIDTH + 20, WINDOW_HEIGHT - 120, 260, 20};
    drawTextWithFont("Captured by White:", labelRect1, smallFont);

    const int CAPTURED_PIECE_SIZE = 25;
    SDL_Rect blackRect = {BOARD_WIDTH + 20, labelRect1.y + 25, CAPTURED_PIECE_SIZE, CAPTURED_PIECE_SIZE};
    for (int i = 0; i < blackCapCount; i++) {
        char piece = blackCaptured[i];
        if (textures[(int) piece]) {
            SDL_RenderCopy(renderer, textures[(int) piece], NULL, &blackRect);
            blackRect.x += CAPTURED_PIECE_SIZE + 2;
        }
    }

    SDL_Rect labelRect2 = {BOARD_WIDTH + 20, blackRect.y + 35, 260, 20};
    drawTextWithFont("Captured by Black:", labelRect2, smallFont);

    SDL_Rect whiteRect = {BOARD_WIDTH + 20, labelRect2.y + 25, CAPTURED_PIECE_SIZE, CAPTURED_PIECE_SIZE};
    for (int i = 0; i < whiteCapCount; i++) {
        char piece = whiteCaptured[i];
        if (textures[(int) piece]) {
            SDL_RenderCopy(renderer, textures[(int) piece], NULL, &whiteRect);
            whiteRect.x += CAPTURED_PIECE_SIZE + 2;
        }
    }
}

void renderBoardWithBack() {
    renderBoard();
    drawButton(backButton, "Back");
    drawTurnIndicator(currentTurn);
    drawCapturedPieces();
}

void getPiecePath(char piece, char *path) {
    if (piece == ' ') {
        path[0] = '\0';
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
    }
    sprintf(path, "%s/%s_%s.png", imageBasePath, color, name);
}

void loadPieceTextures() {
    char types[] = {'P', 'R', 'N', 'B', 'Q', 'K', 'p', 'r', 'n', 'b', 'q', 'k'};
    for (int i = 0; i < sizeof(types); i++) {
        char path[128];
        getPiecePath(types[i], path);
        SDL_Surface *surface = IMG_Load(path);
        if (!surface) {
            printf("Failed to load %s: %s\n", path, IMG_GetError());
            exit(1);
        }
        textures[(int) types[i]] = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
    }
}

void renderBoard() {
    for (int row = 0; row < BOARD_SIZE; row++) {
        for (int col = 0; col < BOARD_SIZE; col++) {
            SDL_Rect tile = {col * TILE_SIZE, row * TILE_SIZE, TILE_SIZE, TILE_SIZE};
            SDL_SetRenderDrawColor(renderer, (row + col) % 2 == 0 ? 240 : 181,
                                   (row + col) % 2 == 0 ? 217 : 136,
                                   (row + col) % 2 == 0 ? 181 : 99, 255);
            SDL_RenderFillRect(renderer, &tile);

            if (validMoves[row][col]) {
                SDL_SetRenderDrawColor(renderer, 102, 240, 102, 100);
                SDL_RenderFillRect(renderer, &tile);
            }

            if (pieceSelected && row == selectedRow && col == selectedCol) {
                SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
                SDL_RenderDrawRect(renderer, &tile);
            }

            char piece = board[row][col];
            if (piece != ' ' && textures[(int) piece]) {
                SDL_RenderCopy(renderer, textures[(int) piece], NULL, &tile);
            }
        }
    }
}

int isWhite(char piece) {
    return piece != ' ' && isupper(piece);
}

int isPathClear(int row1, int col1, int row2, int col2) {
    int rowStep = row2 > row1 ? 1 : row2 < row1 ? -1 : 0;
    int colStep = col2 > col1 ? 1 : col2 < col1 ? -1 : 0;

    int r = row1 + rowStep;
    int c = col1 + colStep;
    while (r != row2 || c != col2) {
        if (board[r][c] != ' ') {
            return 0;
        }
        r += rowStep;
        c += colStep;
    }
    return 1;
}

int isValidMove(int row1, int col1, int row2, int col2, int currentTurn) {
    if (row1 < 0 || row1 >= 8 || col1 < 0 || col1 >= 8 ||
        row2 < 0 || row2 >= 8 || col2 < 0 || col2 >= 8) {
        return 0;
    }
    if (row1 == row2 && col1 == col2) {
        return 0;
    }

    char piece = board[row1][col1];
    char target = board[row2][col2];

    if (piece == ' ') {
        return 0;
    }
    if ((currentTurn % 2 == 0 && !isWhite(piece)) ||
        (currentTurn % 2 == 1 && isWhite(piece))) {
        return 0;
    }
    if (target != ' ' && isWhite(target) == isWhite(piece)) {
        return 0;
    }

    int rowDiff = row2 - row1;
    int colDiff = col2 - col1;

    switch (tolower(piece)) {
        case 'p': {
            if (isWhite(piece)) {
                if (rowDiff == -1 && colDiff == 0 && target == ' ') {
                    return 1;
                }
                if (row1 == 6 && rowDiff == -2 && colDiff == 0 &&
                    board[row1 - 1][col1] == ' ' && target == ' ') {
                    return 1;
                }
                if (rowDiff == -1 && abs(colDiff) == 1 && target != ' ' && !isWhite(target)) {
                    return 1;
                }
            } else {
                if (rowDiff == 1 && colDiff == 0 && target == ' ') {
                    return 1;
                }
                if (row1 == 1 && rowDiff == 2 && colDiff == 0 &&
                    board[row1 + 1][col1] == ' ' && target == ' ') {
                    return 1;
                }
                if (rowDiff == 1 && abs(colDiff) == 1 && target != ' ' && isWhite(target)) {
                    return 1;
                }
            }
            break;
        }
        case 'r':
            if (rowDiff == 0 || colDiff == 0) {
                return isPathClear(row1, col1, row2, col2);
            }
            break;
        case 'n':
            if ((abs(rowDiff) == 2 && abs(colDiff) == 1) ||
                (abs(rowDiff) == 1 && abs(colDiff) == 2)) {
                return 1;
            }
            break;
        case 'b':
            if (abs(rowDiff) == abs(colDiff)) {
                return isPathClear(row1, col1, row2, col2);
            }
            break;
        case 'q':
            if ((abs(rowDiff) == abs(colDiff)) || rowDiff == 0 || colDiff == 0) {
                return isPathClear(row1, col1, row2, col2);
            }
            break;
        case 'k':
            if (abs(rowDiff) <= 1 && abs(colDiff) <= 1) {
                return 1;
            }
            break;
        default:
            return 0;
    }
    return 0;
}

void computeValidMoves(int row, int col) {
    memset(validMoves, 0, sizeof(validMoves));
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (isValidMove(row, col, r, c, currentTurn)) {
                validMoves[r][c] = true;
            }
        }
    }
}

void findKingPosition(int color, int *kr, int *kc) {
    char kingChar = color == 0 ? 'K' : 'k';
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (board[r][c] == kingChar) {
                *kr = r;
                *kc = c;
                return;
            }
        }
    }
    *kr = -1;
    *kc = -1;
}

int canCaptureSquare(int r1, int c1, int r2, int c2) {
    char piece = board[r1][c1];
    if (piece == ' ') {
        return 0;
    }

    if (board[r2][c2] != ' ' && isWhite(board[r2][c2]) == isWhite(piece)) {
        return 0;
    }

    int pseudoTurn = isWhite(piece) ? 0 : 1;

    if (isValidMove(r1, c1, r2, c2, pseudoTurn)) {
        return 1;
    }
    return 0;
}

int isKingInCheck(int color) {
    int kr, kc;
    findKingPosition(color, &kr, &kc);
    if (kr == -1 || kc == -1) {
        return 0;
    }
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char p = board[r][c];
            if (p == ' ') continue;
            if (isWhite(p) == (color == 0)) {
                continue;
            }
            if (canCaptureSquare(r, c, kr, kc)) {
                return 1;
            }
        }
    }
    return 0;
}

int hasAnyLegalMove(int color) {
    for (int r1 = 0; r1 < 8; r1++) {
        for (int c1 = 0; c1 < 8; c1++) {
            char piece = board[r1][c1];
            if (piece == ' ' || isWhite(piece) != (color == 0)) {
                continue;
            }
            for (int r2 = 0; r2 < 8; r2++) {
                for (int c2 = 0; c2 < 8; c2++) {
                    if (r1 == r2 && c1 == c2) {
                        continue;
                    }
                    int pseudoTurn = color == 0 ? 0 : 1;
                    if (!isValidMove(r1, c1, r2, c2, pseudoTurn)) {
                        continue;
                    }
                    char savedTarget = board[r2][c2];
                    board[r2][c2] = board[r1][c1];
                    board[r1][c1] = ' ';
                    int stillInCheck = isKingInCheck(color);

                    board[r1][c1] = board[r2][c2];
                    board[r2][c2] = savedTarget;

                    if (!stillInCheck) {
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

void logMove(const char *move) {
    FILE *file = fopen("moves.txt", "w");
    if (file) {
        fprintf(file, "%s\n", move);
        fclose(file);
    }
}

void movePiece(char *move, int *currentTurn) {
    int col1 = tolower(move[0]) - 'a';
    int row1 = '8' - move[1];
    int col2 = tolower(move[2]) - 'a';
    int row2 = '8' - move[3];

    if (!isValidMove(row1, col1, row2, col2, *currentTurn)) {
        printf("Invalid move!\n");
        return;
    }

    char captured = board[row2][col2];
    char mover = board[row1][col1];

    board[row2][col2] = mover;
    board[row1][col1] = ' ';

    int moverColor = isWhite(mover) ? 0 : 1;
    if (isKingInCheck(moverColor)) {
        board[row1][col1] = mover;
        board[row2][col2] = captured;
        printf("You cannot leave your own king in check! Invalid move.\n");
        return;
    }

    move[4] = '\0';
    logMove(move);
    (*currentTurn)++;

    int nextColor = *currentTurn % 2;
    int inCheck = isKingInCheck(nextColor);
    int canMove = hasAnyLegalMove(nextColor);

    if (!canMove) {
        if (inCheck) {
            printf("Checkmate! %s wins!\n", moverColor == 0 ? "White" : "Black");
        } else {
            printf("Stalemate! It's a draw.\n");
        }
        exit(0);
    }
    if (inCheck) {
        printf("Check!\n");
    }
}

void saveGame(const char *filename, int currentTurn) {
    FILE *f = fopen(filename, "w");
    if (!f) {
        printf("Error: Could not open %s for saving.\n", filename);
        return;
    }

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            fputc(board[row][col], f);
        }
        fputc('\n', f);
    }

    fprintf(f, "%d\n", currentTurn);

    fclose(f);
    printf("Game saved to %s.\n", filename);
}

void loadGame(const char *filename, int *currentTurn) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        printf("Error: Could not open %s for loading.\n", filename);
        return;
    }

    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            int ch = fgetc(f);
            if (ch == EOF) {
                printf("Error: Incomplete board data in %s.\n", filename);
                fclose(f);
                return;
            }
            board[row][col] = (char) ch;
        }
        fgetc(f);
    }

    if (fscanf(f, "%d", currentTurn) != 1) {
        printf("Error: Could not read turn number in %s.\n", filename);
    }

    fclose(f);
    printf("Game loaded from %s.\n", filename);
}

int main(int argc, char *argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0 || !(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("SDL init error: %s\n", SDL_GetError());
        return 1;
    }

    if (TTF_Init() == -1) {
        printf("TTF init error: %s\n", TTF_GetError());
        return 1;
    }

    FILE *file = fopen("paths.txt", "r");
    if (!file) {
        printf("Error opening paths.txt\n");
        exit(1);
    }

    if (!fgets(imageBasePath, sizeof(imageBasePath), file)) {
        printf("Error reading image path from file\n");
        fclose(file);
        exit(1);
    }
    imageBasePath[strcspn(imageBasePath, "\n")] = 0; // remove newline

    if (!fgets(fontPath, sizeof(fontPath), file)) {
        printf("Error reading font path from file\n");
        fclose(file);
        exit(1);
    }
    fontPath[strcspn(fontPath, "\n")] = 0;

    fclose(file);

    font = TTF_OpenFont(fontPath, 24);
    if (!font) {
        printf("Failed to load font: %s\n", TTF_GetError());
        return 1;
    }

    smallFont = TTF_OpenFont(fontPath, 16);  // or a separate small font file
    if (!smallFont) {
        printf("Failed to load small font: %s\n", TTF_GetError());
        return 1;
    }


    window = SDL_CreateWindow("Chess", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    loadPieceTextures();

    SDL_Event e;
    bool running = true;

    while (running) {
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;

            if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
                int mx = e.button.x;
                int my = e.button.y;

                if (currentState == MAIN_MENU) {
                    if (SDL_PointInRect(&(SDL_Point){mx, my}, &playButton)) {
                        currentState = CHESS_BOARD;
                    }
                } else if (currentState == CHESS_BOARD) {
                    if (SDL_PointInRect(&(SDL_Point){mx, my}, &backButton)) {
                        currentState = MAIN_MENU;
                        continue;
                    }

                    int col = mx / TILE_SIZE;
                    int row = my / TILE_SIZE;

                    if (!pieceSelected && board[row][col] != ' ' &&
                        ((currentTurn % 2 == 0 && isWhite(board[row][col])) ||
                         (currentTurn % 2 == 1 && !isWhite(board[row][col])))) {
                        selectedRow = row;
                        selectedCol = col;
                        pieceSelected = true;
                        computeValidMoves(row, col);
                         } else if (pieceSelected) {
                             if (validMoves[row][col]) {
                                 char captured = board[row][col];
                                 if (captured != ' ') {
                                     if (isWhite(captured))
                                         whiteCaptured[whiteCapCount++] = captured;
                                     else
                                         blackCaptured[blackCapCount++] = captured;
                                 }

                                 board[row][col] = board[selectedRow][selectedCol];
                                 board[selectedRow][selectedCol] = ' ';
                                 currentTurn++;
                                 if (currentState == CHESS_BOARD && currentTurn % 2 == 1)
                                     //0 e white s turn, 1 black s
                                     {
                                     engineMove(4);
                                 }

                             }
                             pieceSelected = false;
                             memset(validMoves, 0, sizeof(validMoves));
                         }
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 255, 192, 203, 255); // pink background

        SDL_RenderClear(renderer);
        if (currentState == MAIN_MENU) {
            renderMainMenu();
        } else {
            renderBoardWithBack();
            SDL_RenderPresent(renderer);
        }
    }

    for (int i = 0; i < 128; i++) if (textures[i]) SDL_DestroyTexture(textures[i]);
    if (smallFont) TTF_CloseFont(smallFont);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    IMG_Quit();
    SDL_Quit();
}
    //  Structuri și declarări helper

typedef struct {
    int from_r, from_c;
    int to_r,   to_c;
} Move;

#define MAX_MOVES 256

// vector de mutări
static Move moveList[MAX_MOVES];
static int   moveCount;

// Generare mutări legale-n am pus sah
void generateMoves(int color){
    moveCount = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            char p = board[r][c];
            if (p==' ' || (isWhite(p)?0:1) != color) continue;
            for (int r2=0; r2<BOARD_SIZE; r2++){
                for(int c2=0;c2<BOARD_SIZE;c2++){
                    if (isValidMove(r,c,r2,c2, color) ) {
                        moveList[moveCount++] = (Move){r,c,r2,c2};
                    }
                }
            }
        }
    }
}

// Piece‐Square Tables (exemplu pentru pioni)
static const int PST_PAWN[8][8] = {
    {  0,   0,   0,   0,   0,   0,   0,   0},
    { 50,  50,  50,  50,  50,  50,  50,  50},
    { 10,  10,  20,  30,  30,  20,  10,  10},
    {  5,   5,  10,  25,  25,  10,   5,   5},
    {  0,   0,   0,  20,  20,   0,   0,   0},
    {  5,  -5, -10,   0,   0, -10,  -5,   5},
    {  5,  10,  10, -20, -20,  10,  10,   5},
    {  0,   0,   0,   0,   0,   0,   0,   0}
};

//Evaluare statică
int evaluateStatic() {
    static const int VALS[128] = { ['P']=100, ['N']=320, ['B']=330, ['R']=500, ['Q']=900, ['K']=20000,
                                   ['p']=-100,['n']=-320,['b']=-330,['r']=-500,['q']=-900,['k']=-20000 };
    int score = 0;
    for(int r=0;r<8;r++)for(int c=0;c<8;c++){
        char p = board[r][c];
        if (!p) continue;
        score += VALS[(int)p];
        // ex:PST pentru pioni
        if (tolower(p)=='p') {
            score += isupper(p)
                   ? PST_PAWN[r][c]
                   : -PST_PAWN[7-r][c];
        }
    }
    return score;
}

// Evaluare dinamică (mobilitate + control centru)
int evaluateDynamic(int color) {
    int mob = 0, center = 0;
    // mobilitate: nr mutări legale
    generateMoves(color);
    mob = moveCount;
    // control centru: bonus pentru ocuparea/dotarea celor 4 patrate (d4,e4,d5,e5)
    const int centers[4][2] = {{3,3},{3,4},{4,3},{4,4}};
    for(int i=0;i<4;i++){
        char p = board[ centers[i][0] ][ centers[i][1] ];
        if (p!=' ' && (isWhite(p)?0:1)==color) center++;
    }
    return mob * 10 + center * 25;
}

//  Minimax + Alpha–Beta Pruning
int alphabeta(int depth, int alpha, int beta, int color) {
    int opponent = 1 - color;

    if (depth == 0) {
        int stat = evaluateStatic();
        int dyn  = evaluateDynamic(color) - evaluateDynamic(opponent);

        return stat + dyn;
    }
    generateMoves(color);
    if (moveCount == 0) {
        // mat sau remiză
        if (isKingInCheck(color)) return -100000 + (8-depth);
        else                       return    0;
    }
    for(int i=0;i<moveCount;i++){
        Move m = moveList[i];
        char captured = board[m.to_r][m.to_c];
        // aplica mutarea
        board[m.to_r][m.to_c] = board[m.from_r][m.from_c];
        board[m.from_r][m.from_c] = ' ';
        int val = -alphabeta(depth-1, -beta, -alpha, opponent);
        // undo
        board[m.from_r][m.from_c] = board[m.to_r][m.to_c];
        board[m.to_r][m.to_c]     = captured;
        if (val >= beta) return beta;       // prune
        if (val > alpha) alpha = val;
    }
    return alpha;
}

// găsim cea mai bună mutare
Move findBestMove(int depth, int color, int *outScore) {
    Move best = {0,0,0,0};
    int alpha = -1000000;
    generateMoves(color);
    for(int i=0;i<moveCount;i++){
        Move m = moveList[i];
        char cap = board[m.to_r][m.to_c];
        board[m.to_r][m.to_c] = board[m.from_r][m.from_c];
        board[m.from_r][m.from_c] = ' ';
        int score = -alphabeta(depth-1, -1000000, 1000000, 1-color);
        // undo
        board[m.from_r][m.from_c] = board[m.to_r][m.to_c];
        board[m.to_r][m.to_c]     = cap;
        if (score > alpha) {
            alpha = score;
            best  = m;
        }
    }
    *outScore = alpha;
    return best;
}

// Funcție de apel din main loop
void engineMove(int depth) {
    int score;
    Move m = findBestMove(depth, /* culoare engine = */ currentTurn%2, &score);
    // aplică mutarea în board-ul curent
    char logm[6] = {
        'a'+m.from_c, '8'-m.from_r,
        'a'+m.to_c,   '8'-m.to_r,
        '\0'
    };
    printf("Engine plays %s  (eval=%d centipawns)\n", logm, score);
    movePiece(logm, &currentTurn);  // folosește funcția ta deja existentă
}




