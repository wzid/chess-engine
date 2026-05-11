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
    int is_in_check;
    int halfmove_clock;
    int b_castle_queenside;
    int b_castle_kingside;
    int w_castle_queenside;
    int w_castle_kingside;
} Board;

Board init_board();

int move_piece(Board *board, PieceType type, int current_square, int new_square);

BITBOARD get_legal_moves(Board* board, PieceType type, int current_square);

// Returns 1 if the move would leave the mover's king in check on the resulting board
int move_results_in_check(Board* board, PieceType type, int current_square, int new_square);

int is_checkmate(Board* board, int black_to_move);
int is_stalemate(Board* board, int black_to_move);
int is_fifty_move_draw(Board* board);

int is_piece_at(const Board* board, int rank, int file);
#define is_piece_at_sq(b, sq) is_piece_at(b, (sq) / 8, (sq) % 8)



#endif