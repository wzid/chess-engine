#ifndef BOARD_H
#define BOARD_H

#include <stdint.h>

typedef struct {
    uint64_t w_pawn;
    uint64_t w_rook;
    uint64_t w_knight;
    uint64_t w_bishop;
    uint64_t w_queen;
    uint64_t w_king;
    uint64_t b_pawn;
    uint64_t b_rook;
    uint64_t b_knight;
    uint64_t b_bishop;
    uint64_t b_queen;
    uint64_t b_king;
} Board;

Board init_board();

void display_board(Board *board, int is_dark_mode);


#endif