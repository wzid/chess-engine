#ifndef GAME_H
#define GAME_H

#include <raylib.h>
#include "board.h"

typedef struct {
    int active;
    int square;
    PieceType type;
    BITBOARD legal_moves;
} SelectedPiece;

typedef struct {
    Color light_color;
    Color dark_color;
    Texture2D textures[PIECE_COUNT];
    SelectedPiece selected_piece;
    int dragging;
    int current_turn;  // 0 = white, 1 = black
    int game_over;     // 0 = no, 1 = yes
    int result;        // 0 = none, 1 = white wins, 2 = black wins, 3 = draw
} Game;

Game init_game();

void game_loop(Game *game, Board *board);

#endif