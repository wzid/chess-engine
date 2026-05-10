#ifndef GAME_H
#define GAME_H

#include <raylib.h>
#include "board.h"

typedef struct {
    int active;
    int square;
    PieceType type;
} HeldPiece;

typedef struct {
    Color light_color;
    Color dark_color;
    Texture2D textures[PIECE_COUNT];
    HeldPiece held_piece;
} Game;

Game init_game();

void game_loop(Game *game, Board *board);

#endif