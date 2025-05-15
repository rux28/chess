#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <SDL.h>

#define BOARD_SIZE 8

#define RESET   "\x1B[0m"
#define WHITE   "\x1B[1;37m"
#define BLACK   "\x1B[1;30m"
#define B_WHITE "\x1B[48;5;250m"
#define B_BLACK "\x1B[48;5;237m"

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

int isWhite(char piece) {
    return piece != ' ' && isupper(piece);
}

void printBoard() {
    for (int row = 0; row < BOARD_SIZE; row++) {
        printf("%d ", 8 - row);
        for (int col = 0; col < BOARD_SIZE; col++) {
            char piece = board[row][col];
            if (piece == ' ') {
                printf("%s    %s", (row + col) % 2 == 0 ? B_WHITE : B_BLACK, RESET);
            } else {
                printf("%s %s %c %s",
                       (row + col) % 2 == 0 ? B_WHITE : B_BLACK,
                       isWhite(piece) ? WHITE : BLACK,
                       piece,
                       RESET);
            }
        }
        printf("\n");
    }
    printf("   a   b   c   d   e   f   g   h\n");
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

int main() {
    char userInput[10];
    int currentTurn = 0;

    while (1) {
        printBoard();
        printf("%s to move (e.g., e2e4), 'save', 'load', or 'q' to quit: ",
               currentTurn % 2 == 0 ? "White" : "Black");
        scanf("%s", userInput);

        if (strcmp(userInput, "q") == 0) {
            break;
        }
        if (strcmp(userInput, "save") == 0) {
            saveGame("chess_save.txt", currentTurn);
        } else if (strcmp(userInput, "load") == 0) {
            loadGame("chess_save.txt", &currentTurn);
        } else if (strlen(userInput) == 4 &&
                   isalpha(userInput[0]) && isdigit(userInput[1]) &&
                   isalpha(userInput[2]) && isdigit(userInput[3])) {
            movePiece(userInput, &currentTurn);
        } else {
            printf("Invalid command. Use 'e2e4', 'save', 'load', or 'q'.\n");
        }
    }

    return 0;
}
