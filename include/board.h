#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

typedef uint64_t BITBOARD;

typedef enum PieceType {
    B_PAWN, B_KNIGHT, B_BISHOP, B_ROOK, B_QUEEN, B_KING,
    W_PAWN, W_KNIGHT, W_BISHOP, W_ROOK, W_QUEEN, W_KING,
    PIECE_COUNT, EMPTY
} PieceType;

typedef struct {
    BITBOARD bitboards[PIECE_COUNT];
} Board;

Board init_board();

void move_piece(Board *board, PieceType type, int current_square, int new_square);

#endif