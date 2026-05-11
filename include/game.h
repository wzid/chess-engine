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
} Game;

Game init_game();

void game_loop(Game *game, Board *board);

#endif