#include "board.h"

#include <stdio.h>
#include <stdlib.h>

Board init_board() {
    // black at the top and white at the bottom
    Board chess_board = {
        .b_pawn = 0xff00,
        .b_rook = 0x81,
        .b_knight = 0x42,
        .b_bishop = 0x24,
        .b_queen = 0x8,
        .b_king = 0x10,
        .w_pawn = 0xff000000000000,
        .w_rook = 0x8100000000000000,
        .w_knight = 0x4200000000000000,
        .w_bishop = 0x2400000000000000,
        .w_queen = 0x800000000000000,
        .w_king = 0x1000000000000000,
    };
    return chess_board;
}

void print_bin(uint64_t n) {
    while (n) {
        if (n & 1)
            printf("1");
        else
            printf("0");

        n >>= 1;
    }
    printf("\n");
}

void display_board(Board* board, int is_dark_mode) {
    for (int i = 0; i < 64; i++) {
        int row = (int)(i / 8);
        int col = i % 8;
        uint64_t mask = 1ULL << i;

        if (i != 0 && i % 8 == 0) {
            printf("\n");
        }
        if (col == 0) {
            printf("%i ", 8 - row);
        }
        // on my terminal (dark)
        if (mask & board->w_pawn) {
            printf(is_dark_mode ? "♟" : "♙");
        } else if (mask & board->w_rook) {
            printf(is_dark_mode ? "♜" : "♖");
        } else if (mask & board->w_knight) {
            printf(is_dark_mode ? "♞" : "♘");
        } else if (mask & board->w_bishop) {
            printf(is_dark_mode ? "♝" : "♗");
        } else if (mask & board->w_queen) {
            printf(is_dark_mode ? "♛" : "♕");
        } else if (mask & board->w_king) {
            printf(is_dark_mode ? "♚" : "♔");
        } else if (mask & board->b_pawn) {
            printf(is_dark_mode ? "♙" : "♟");
        } else if (mask & board->b_rook) {
            printf(is_dark_mode ? "♖" : "♜");
        } else if (mask & board->b_knight) {
            printf(is_dark_mode ? "♘" : "♞");
        } else if (mask & board->b_bishop) {
            printf(is_dark_mode ? "♗" : "♝");
        } else if (mask & board->b_queen) {
            printf(is_dark_mode ? "♕" : "♛");
        } else if (mask & board->b_king) {
            printf(is_dark_mode ? "♔" : "♚");
        } else {
            printf(".");
        }
    }
    printf("\n");
    printf("  abcdefgh\n");
}