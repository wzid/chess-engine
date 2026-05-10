#include "board.h"

#include <stdio.h>
#include <stdlib.h>

static void print_bin(uint64_t n);

Board init_board() {
    // black at the top and white at the bottom
    Board chess_board;
    chess_board.bitboards[B_PAWN] = 0xff00;
    chess_board.bitboards[B_ROOK] = 0x81;
    chess_board.bitboards[B_KNIGHT] = 0x42;
    chess_board.bitboards[B_BISHOP] = 0x24;
    chess_board.bitboards[B_QUEEN] = 0x8;
    chess_board.bitboards[B_KING] = 0x10;
    chess_board.bitboards[W_PAWN] = 0xff000000000000;
    chess_board.bitboards[W_ROOK] = 0x8100000000000000;
    chess_board.bitboards[W_KNIGHT] = 0x4200000000000000;
    chess_board.bitboards[W_BISHOP] = 0x2400000000000000;
    chess_board.bitboards[W_QUEEN] = 0x800000000000000;
    chess_board.bitboards[W_KING] = 0x1000000000000000;

    return chess_board;
}

void move_piece(Board *board, int current_square, int new_square) {
    // check if legal
    uint64_t o_mask = 1ULL << current_square;
    uint64_t n_mask = 1ULL << new_square;
    for (int i = 0; i < PIECE_COUNT; i++) {
        if (o_mask & board->bitboards[i]) {
            // remove the 1 from the bitboard using xor and the mask
            board->bitboards[i] ^= o_mask;
            // add the new location
            board->bitboards[i] |= n_mask;
            break;
        }
    }
}

// static void print_bin(uint64_t n) {
//     while (n) {
//         if (n & 1)
//             printf("1");
//         else
//             printf("0");

//         n >>= 1;
//     }
//     printf("\n");
// }