#ifndef GAME_H
#define GAME_H

#include <raylib.h>
#include "board.h"


typedef enum PieceType {
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    EMPTY,
    PIECE_COUNT
} PieceType;

typedef struct {
    Color light_color;
    Color dark_color;
    Texture2D textures[PIECE_COUNT];
} Game;

Game init_game();

void game_loop(Game *game, Board *board);

#endif