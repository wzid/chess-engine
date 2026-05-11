#include "board.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_bitboard(uint64_t n);
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

int move_piece(Board* board, PieceType type, int current_square, int new_square) {
    // check if legal
    // printf("Moving Piece\n");

    BITBOARD legal_moves = get_legal_moves(board, type, current_square);
    uint64_t n_mask = 1ULL << new_square;

    // not a legal move!!
    if (!(n_mask & legal_moves)) {
        // do something
        return 0;
    }

    // remove old piece for capture
    for (int i = 0; i < PIECE_COUNT; i++) {
        if (n_mask & board->bitboards[i]) {
            board->bitboards[i] &= ~n_mask;
            break;
        }
    }

    uint64_t o_mask = 1ULL << current_square;
    for (int i = 0; i < PIECE_COUNT; i++) {
        if (o_mask & board->bitboards[i]) {
            // remove the 1 from the bitboard using xor and the mask
            board->bitboards[i] &= ~o_mask;
            // add the new location
            board->bitboards[i] |= n_mask;
            break;
        }
    }

    return 1;
}

static BITBOARD legal_pawn_moves(Board* board, PieceType type, int current_square);
static BITBOARD legal_knight_moves(Board* board, PieceType type, int current_square);
static BITBOARD legal_rook_moves(Board* board, PieceType type, int current_square);
static BITBOARD legal_bishop_moves(Board* board, PieceType type, int current_square);
static BITBOARD legal_queen_moves(Board* board, PieceType type, int current_square);
static BITBOARD legal_king_moves(Board* board, PieceType type, int current_square);
static void add_move(BITBOARD* legal_moves, int rank, int file);
static void remove_same_color_capture(Board* board, BITBOARD* legal_moves, PieceType type);

// Should return a BITBOARD of all legal moves by that piece type
BITBOARD get_legal_moves(Board* board, PieceType type, int current_square) {
    switch (type) {
        case B_PAWN:
        case W_PAWN:
            return legal_pawn_moves(board, type, current_square);
        case B_KNIGHT:
        case W_KNIGHT:
            return legal_knight_moves(board, type, current_square);
        case B_ROOK:
        case W_ROOK:
            return legal_rook_moves(board, type, current_square);
        case B_BISHOP:
        case W_BISHOP:
            return legal_bishop_moves(board, type, current_square);
        case B_QUEEN:
        case W_QUEEN:
            return legal_queen_moves(board, type, current_square);
        case B_KING:
        case W_KING:
            return legal_king_moves(board, type, current_square);
        default:
            return 0ULL;
    }
}

static BITBOARD legal_pawn_moves(Board* board, PieceType type, int current_square) {
    // If a pawn is on the rank it started on then it can move 2 squares forward.
    // Can capture sideways and also en passant (will implement later)

    int is_black = type == B_PAWN;
    int rank = current_square / 8;
    int file = current_square % 8;

    // printf("Current rank: %i, file: %i\n", rank, file);

    BITBOARD legal_moves = 0ULL;
    if (is_black) {
        if (!is_piece_at(board, rank + 1, file)) {
            add_move(&legal_moves, rank + 1, file);
            if (rank == 1 && !is_piece_at(board, rank + 2, file)) {
                add_move(&legal_moves, rank + 2, file);
            }
        }
        // diagonal
        if (is_piece_at(board, rank + 1, file + 1)) {
            add_move(&legal_moves, rank + 1, file + 1);
        }
        if (is_piece_at(board, rank + 1, file - 1)) {
            add_move(&legal_moves, rank + 1, file - 1);
        }

    } else {
        if (!is_piece_at(board, rank - 1, file)) {
            add_move(&legal_moves, rank - 1, file);
            if (rank == 6 && !is_piece_at(board, rank - 2, file)) {
                add_move(&legal_moves, rank - 2, file);
            }
        }

        // diagonal
        if (is_piece_at(board, rank - 1, file + 1)) {
            add_move(&legal_moves, rank - 1, file + 1);
        }
        if (is_piece_at(board, rank - 1, file - 1)) {
            add_move(&legal_moves, rank - 1, file - 1);
        }
    }

    remove_same_color_capture(board, &legal_moves, type);
    return legal_moves;
}

static BITBOARD legal_knight_moves(Board* board, PieceType type, int current_square) {
    int rank = current_square / 8;
    int file = current_square % 8;

    BITBOARD legal_moves = 0ULL;

    add_move(&legal_moves, rank - 2, file - 1);
    add_move(&legal_moves, rank - 2, file + 1);

    add_move(&legal_moves, rank - 1, file - 2);
    add_move(&legal_moves, rank - 1, file + 2);
    add_move(&legal_moves, rank + 1, file + 2);
    add_move(&legal_moves, rank + 1, file - 2);

    add_move(&legal_moves, rank + 2, file - 1);
    add_move(&legal_moves, rank + 2, file + 1);

    remove_same_color_capture(board, &legal_moves, type);

    return legal_moves;
}

static BITBOARD legal_rook_moves(Board* board, PieceType type, int current_square) {
    int rank = current_square / 8;
    int file = current_square % 8;

    BITBOARD legal_moves = 0ULL;

    // down
    for (int r = rank + 1; r < 8; r++) {
        add_move(&legal_moves, r, file);
        if (is_piece_at(board, r, file)) break;
    }

    // up
    for (int r = rank - 1; r >= 0; r--) {
        add_move(&legal_moves, r, file);
        if (is_piece_at(board, r, file)) break;
    }

    // right
    for (int f = file + 1; f < 8; f++) {
        add_move(&legal_moves, rank, f);
        if (is_piece_at(board, rank, f)) break;
    }

    // left
    for (int f = file - 1; f >= 0; f--) {
        add_move(&legal_moves, rank, f);
        if (is_piece_at(board, rank, f)) break;
    }

    remove_same_color_capture(board, &legal_moves, type);
    return legal_moves;
}

static BITBOARD legal_bishop_moves(Board* board, PieceType type, int current_square) {
    int rank = current_square / 8;
    int file = current_square % 8;

    BITBOARD legal_moves = 0ULL;

    // bottom right
    for (int r = rank + 1, f = file + 1; r < 8 && f < 8; r++, f++) {
        add_move(&legal_moves, r, f);
        if (is_piece_at(board, r, f)) break;
    }

    // bottom left
    for (int r = rank + 1, f = file - 1; r < 8 && f >= 0; r++, f--) {
        add_move(&legal_moves, r, f);
        if (is_piece_at(board, r, f)) break;
    }

    // top right
    for (int r = rank - 1, f = file + 1; r >= 0 && f < 8; r--, f++) {
        add_move(&legal_moves, r, f);
        if (is_piece_at(board, r, f)) break;
    }

    // top left
    for (int r = rank - 1, f = file - 1; r >= 0 && f >= 0; r--, f--) {
        add_move(&legal_moves, r, f);
        if (is_piece_at(board, r, f)) break;
    }

    remove_same_color_capture(board, &legal_moves, type);

    return legal_moves;
}

// coded like this just incase I need to debug the legal moves.
static BITBOARD legal_queen_moves(Board* board, PieceType type, int current_square) {
    uint64_t legal_moves = legal_bishop_moves(board, type, current_square) | legal_rook_moves(board, type, current_square);
    // print_bitboard(legal_moves);
    return legal_moves;
}

static void add_king_moves(BITBOARD* moves, int rank, int file);

static BITBOARD legal_king_moves(Board* board, PieceType type, int current_square) {
    int rank = current_square / 8;
    int file = current_square % 8;

    BITBOARD legal_moves = 0ULL;

    add_king_moves(&legal_moves, rank, file);

    remove_same_color_capture(board, &legal_moves, type);

    // need to make sure that the opponent is not attacking any of the legal moves
    int start = type == B_KING ? W_PAWN : B_PAWN;
    int end = type == B_KING ? W_KING : B_KING;

    for (int i = 0; i < 64; i++) {
        uint64_t square_mask = 1ULL << i;
        for (int opp_type = start; opp_type <= end; opp_type++) {

            BITBOARD opponent_legal_moves = 0LL;
            // if there exists an opponents piece then we need to check the legal moves
            if (square_mask & board->bitboards[opp_type]) {
                // if the opponent type is the king just check the adjacent squares for that other king
                if (opp_type == end) {
                    int opp_rank = i / 8, opp_file = i % 8;
                    add_king_moves(&opponent_legal_moves, opp_rank, opp_file);
                } else {
                    // otherwise get the legal moves of the piece
                    opponent_legal_moves = get_legal_moves(board, opp_type, i);
                }
                // if and moves in opponent_legal_moves exist in legal_moves then we remove it from legal_moves
                if (opponent_legal_moves & legal_moves) {
                    legal_moves &= ~opponent_legal_moves;
                }
            }
        }
    }

    return legal_moves;
}

static void add_king_moves(BITBOARD* moves, int rank, int file) {
    add_move(moves, rank + 1, file);
    add_move(moves, rank - 1, file);
    add_move(moves, rank - 1, file + 1);
    add_move(moves, rank - 1, file - 1);
    add_move(moves, rank + 1, file - 1);
    add_move(moves, rank + 1, file + 1);
    add_move(moves, rank, file + 1);
    add_move(moves, rank, file - 1);
}

static void add_move(BITBOARD* legal_moves, int rank, int file) {
    if (rank <= 7 && rank >= 0 && file <= 7 && file >= 0) *legal_moves |= (1ULL << ((rank * 8) + file));
}

static void remove_same_color_capture(Board* board, BITBOARD* legal_moves, PieceType type) {
    int is_black =
        type == B_PAWN || type == B_BISHOP || type == B_KNIGHT || type == B_ROOK || type == B_QUEEN || type == B_KING;

    if (is_black) {
        for (int i = B_PAWN; i <= B_KING; i++) {
            if (*legal_moves & board->bitboards[i]) {
                *legal_moves &= ~board->bitboards[i];
            }
        }
    } else {
        for (int i = W_PAWN; i <= W_KING; i++) {
            if (*legal_moves & board->bitboards[i]) {
                *legal_moves &= ~board->bitboards[i];
            }
        }
    }
}

int is_piece_at(const Board* board, int rank, int file) {
    uint64_t mask = 1ULL << ((rank * 8) + file);
    for (int i = 0; i < PIECE_COUNT - 1; i++) {
        if (mask & board->bitboards[i]) {
            return 1;
        }
    }
    return 0;
}

// https://stackoverflow.com/questions/26252928/display-msb-to-lsb#26253009
// I use this with layout 0 on https://gekomad.github.io/Cinnamon/BitboardCalculator/
// for debugging
static void print_bitboard(uint64_t n) {
    for (int i = 63; i >= 0; i--) {
        printf("%llu", (n >> i) & 1ULL);
    }
    printf("\n");
}