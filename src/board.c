#include "board.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

static void print_bitboard(uint64_t n);
static void update_castling_rules(Board* board, PieceType type, int current_square, int new_square,
                                  PieceType captured_piece);
static void move_castling_rook(Board* board, PieceType king_type, int current_square, int new_square);
static inline int get_square(int rank, int file);
static int is_en_passant_capture_move(const Board* board, PieceType type, int new_square);

Board init_board() {
    // black at the top and white at the bottom
    Board chess_board = {.b_castle_kingside = 1,
                         .b_castle_queenside = 1,
                         .w_castle_kingside = 1,
                         .w_castle_queenside = 1,
                         .halfmove_clock = 0,
                         .en_passant_square = -1,
                         .promotion_pending = 0,
                         .promotion_square = -1,
                         .promotion_pawn_type = EMPTY};
    chess_board.bitboards[B_PAWN] = 0xff00;
    chess_board.bitboards[B_KNIGHT] = 0x42;
    chess_board.bitboards[B_BISHOP] = 0x24;
    chess_board.bitboards[B_ROOK] = 0x81;
    chess_board.bitboards[B_QUEEN] = 0x8;
    chess_board.bitboards[B_KING] = 0x10;
    chess_board.bitboards[W_PAWN] = 0xff000000000000;
    chess_board.bitboards[W_KNIGHT] = 0x4200000000000000;
    chess_board.bitboards[W_BISHOP] = 0x2400000000000000;
    chess_board.bitboards[W_ROOK] = 0x8100000000000000;
    chess_board.bitboards[W_QUEEN] = 0x800000000000000;
    chess_board.bitboards[W_KING] = 0x1000000000000000;

    return chess_board;
}

int move_piece(Board* board, PieceType type, int current_square, int new_square) {
    // A pending promotion must be resolved before any new move.
    if (board->promotion_pending) {
        return 0;
    }

    BITBOARD legal_moves = get_legal_moves(board, type, current_square);
    uint64_t n_mask = 1ULL << new_square;

    // not a legal move!!
    if (!(n_mask & legal_moves)) {
        // don't allow
        return 0;
    }

    // detect and remove captured piece (if any)
    PieceType captured_piece = piece_at_square(board, new_square);

    int is_en_passant_capture = 0;
    int en_passant_capture_square = -1;

    if (is_en_passant_capture_move(board, type, new_square)) {
        is_en_passant_capture = 1;
        // have to set this so we can capture the pawn without stepping on its square
        en_passant_capture_square = (type == B_PAWN) ? (new_square - 8) : (new_square + 8);
        captured_piece = piece_at_square(board, en_passant_capture_square);
    }

    if (captured_piece != EMPTY) {
        uint64_t capture_mask = is_en_passant_capture ? (1ULL << en_passant_capture_square) : n_mask;
        board->bitboards[captured_piece] &= ~capture_mask;
    }

    uint64_t o_mask = 1ULL << current_square;
    for (int i = 0; i < PIECE_COUNT; i++) {
        if (o_mask & board->bitboards[i]) {
            // remove the 1 from the bitboard using and and "notting" the mask
            board->bitboards[i] &= ~o_mask;
            // add the new location
            board->bitboards[i] |= n_mask;
            break;
        }
    }

    // mark pending pawn promotion after the pawn arrives on last rank
    if (type == B_PAWN || type == W_PAWN) {
        int new_rank = new_square / 8;
        int is_promotion = (type == B_PAWN && new_rank == 7) || (type == W_PAWN && new_rank == 0);
        if (is_promotion) {
            board->promotion_pending = 1;
            board->promotion_square = new_square;
            board->promotion_pawn_type = type;
        }
    }

    if (type == B_KING || type == W_KING) {
        move_castling_rook(board, type, current_square, new_square);
    }

    update_castling_rules(board, type, current_square, new_square, captured_piece);

    // En passant is only available immediately after a double pawn push.
    board->en_passant_square = -1;
    // if there was a double push
    if (type == B_PAWN && new_square - current_square == 16) {
        // set it to the square above it
        board->en_passant_square = current_square + 8;
    } else if (type == W_PAWN && current_square - new_square == 16) {
        board->en_passant_square = current_square - 8;
    }

    // Update halfmove clock: reset on pawn move or any capture, otherwise increment
    int is_pawn = (type == B_PAWN || type == W_PAWN);
    if (is_pawn || captured_piece != EMPTY) {
        board->halfmove_clock = 0;
    } else {
        board->halfmove_clock += 1;
    }

    return 1;
}

int promote_pawn(Board* board, PieceType promotion_type) {
    if (!board->promotion_pending) {
        return 0;
    }

    int square = board->promotion_square;
    PieceType pawn_type = board->promotion_pawn_type;
    int is_black = (pawn_type == B_PAWN);

    int valid_choice = (is_black && (promotion_type == B_QUEEN || promotion_type == B_ROOK || promotion_type == B_BISHOP ||
                                     promotion_type == B_KNIGHT)) ||
                       (!is_black && (promotion_type == W_QUEEN || promotion_type == W_ROOK || promotion_type == W_BISHOP ||
                                      promotion_type == W_KNIGHT));
    if (!valid_choice) {
        return 0;
    }

    uint64_t mask = 1ULL << square;
    if (!(board->bitboards[pawn_type] & mask)) {
        return 0;
    }

    board->bitboards[pawn_type] &= ~mask;
    board->bitboards[promotion_type] |= mask;

    board->promotion_pending = 0;
    board->promotion_square = -1;
    board->promotion_pawn_type = EMPTY;

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
static int in_bounds(int rank, int file);
static int can_castle(Board* board, PieceType king_type, int current_square, int is_kingside);
static int is_square_attacked(const Board* board, int square, int attacker_must_be_black);

// Returns 1 if the move (type from current_square to new_square) would leave
// the mover's king in check on the resulting board, 0 otherwise.
static int move_results_in_check(Board* board, PieceType type, int current_square, int new_square);

// Should return a BITBOARD of all legal moves by that piece type
BITBOARD get_legal_moves(Board* board, PieceType type, int current_square) {
    BITBOARD raw = 0ULL;
    switch (type) {
        case B_PAWN:
        case W_PAWN:
            raw = legal_pawn_moves(board, type, current_square);
            break;
        case B_KNIGHT:
        case W_KNIGHT:
            raw = legal_knight_moves(board, type, current_square);
            break;
        case B_ROOK:
        case W_ROOK:
            raw = legal_rook_moves(board, type, current_square);
            break;
        case B_BISHOP:
        case W_BISHOP:
            raw = legal_bishop_moves(board, type, current_square);
            break;
        case B_QUEEN:
        case W_QUEEN:
            raw = legal_queen_moves(board, type, current_square);
            break;
        case B_KING:
        case W_KING:
            raw = legal_king_moves(board, type, current_square);
            break;
        default:
            return 0ULL;
    }

    // Filter out the legal moves that result in check using bit-scan
    BITBOARD filtered = 0ULL;
    BITBOARD tmp = raw;
    while (tmp) {
        int s = __builtin_ctzll(tmp);
        uint64_t m = 1ULL << s;
        if (!move_results_in_check(board, type, current_square, s)) {
            filtered |= m;
        }
        tmp &= tmp - 1;
    }

    return filtered;
}

int move_results_in_check(Board* board, PieceType type, int current_square, int new_square) {
    Board test = *board;
    uint64_t from_mask = 1ULL << current_square;
    uint64_t to_mask = 1ULL << new_square;

    // remove captured piece if any
    int is_en_passant_capture = 0;
    int en_passant_capture_square = -1;
    if (is_en_passant_capture_move(&test, type, new_square)) {
        is_en_passant_capture = 1;
        en_passant_capture_square = (type == B_PAWN) ? (new_square - 8) : (new_square + 8);
    }

    if (is_en_passant_capture) {
        uint64_t capture_mask = 1ULL << en_passant_capture_square;
        for (int i = 0; i < PIECE_COUNT; i++) {
            if (capture_mask & test.bitboards[i]) {
                test.bitboards[i] &= ~capture_mask;
                break;
            }
        }
    } else {
        for (int i = 0; i < PIECE_COUNT; i++) {
            if (to_mask & test.bitboards[i]) {
                test.bitboards[i] &= ~to_mask;
                break;
            }
        }
    }

    // move moving piece
    for (int i = 0; i < PIECE_COUNT; i++) {
        if (from_mask & test.bitboards[i]) {
            test.bitboards[i] &= ~from_mask;
            test.bitboards[i] |= to_mask;
            break;
        }
    }

    // If the piece is a king, find it at the destination; otherwise find its king
    // Determine the mover's king by the color of the moving piece.
    PieceType king;
    if (type >= B_PAWN && type <= B_KING) {
        king = B_KING;
    } else {
        king = W_KING;
    }

    int king_square = -1;
    if (type == B_KING || type == W_KING) {
        king_square = new_square;
    } else {
        // find king square using bit-scan (faster than looping 0..63)
        if (test.bitboards[king]) {
            king_square = __builtin_ctzll(test.bitboards[king]);
        }
    }

    if (king_square < 0) return 1;  // should not happen, treat as in-check

    int attacker_is_black = (king == W_KING);
    return is_square_attacked(&test, king_square, attacker_is_black);
}

static BITBOARD legal_pawn_moves(Board* board, PieceType type, int current_square) {
    // If a pawn is on the rank it started on then it can move 2 squares forward.
    // Can capture sideways and also en passant (will implement later)

    int is_black = type == B_PAWN;
    int rank = current_square / 8;
    int file = current_square % 8;

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

        if (is_en_passant_capture_move(board, type, get_square(rank + 1, file + 1))) {
            add_move(&legal_moves, rank + 1, file + 1);
        }
        if (is_en_passant_capture_move(board, type, get_square(rank + 1, file - 1))) {
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

        if (is_en_passant_capture_move(board, type, get_square(rank - 1, file + 1))) {
            add_move(&legal_moves, rank - 1, file + 1);
        }
        if (is_en_passant_capture_move(board, type, get_square(rank - 1, file - 1))) {
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

    static const int knight_offsets[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}};

    for (int i = 0; i < 8; i++) {
        int rank_delta = knight_offsets[i][0];
        int file_delta = knight_offsets[i][1];
        add_move(&legal_moves, rank + rank_delta, file + file_delta);
    }

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

// A queen is a bishop and a rook!
static BITBOARD legal_queen_moves(Board* board, PieceType type, int current_square) {
    return legal_bishop_moves(board, type, current_square) | legal_rook_moves(board, type, current_square);
}

static BITBOARD legal_king_moves(Board* board, PieceType king_type, int current_square) {
    int rank = current_square / 8;
    int file = current_square % 8;

    BITBOARD legal_moves = 0ULL;

    static const int king_offsets[8][2] = {{1, 0}, {-1, 0}, {-1, 1}, {-1, -1}, {1, -1}, {1, 1}, {0, -1}, {0, 1}};

    for (int i = 0; i < 8; i++) {
        int rank_delta = king_offsets[i][0];
        int file_delta = king_offsets[i][1];
        add_move(&legal_moves, rank + rank_delta, file + file_delta);
    }

    remove_same_color_capture(board, &legal_moves, king_type);

    // Filter out squares that would still be attacked after the king moves there.
    int attacker_must_be_black = king_type == W_KING;

    // Filter king moves by scanning candidate squares with bit-scan
    BITBOARD candidates = legal_moves;
    BITBOARD filtered_king_moves = legal_moves;
    while (candidates) {
        int square = __builtin_ctzll(candidates);
        uint64_t square_mask = 1ULL << square;

        // we are simulating a move to see if it is attacked.
        // we have to remove the king's old position and remove the captured piece
        Board test_board = *board;
        uint64_t current_mask = 1ULL << current_square;
        // removing the current square
        test_board.bitboards[king_type] &= ~current_mask;

        PieceType captured_piece = piece_at_square(board, square);
        if (captured_piece != EMPTY) {
            // removing the captured piece from the bitboard
            test_board.bitboards[captured_piece] &= ~square_mask;
        }

        // if it is attacked after the king potentially moves there then we don't want to allow it as a legal move3
        if (is_square_attacked(&test_board, square, attacker_must_be_black)) {
            filtered_king_moves &= ~square_mask;
        }

        candidates &= candidates - 1;
    }
    legal_moves = filtered_king_moves;

    if (can_castle(board, king_type, current_square, 1)) {
        add_move(&legal_moves, rank, file + 2);
    }
    if (can_castle(board, king_type, current_square, 0)) {
        add_move(&legal_moves, rank, file - 2);
    }

    return legal_moves;
}

static void add_move(BITBOARD* legal_moves, int rank, int file) {
    if (rank <= 7 && rank >= 0 && file <= 7 && file >= 0) *legal_moves |= (1ULL << get_square(rank, file));
}

static void remove_same_color_capture(Board* board, BITBOARD* legal_moves, PieceType type) {
    int is_black_piece = type >= B_PAWN && type <= B_KING;
    if (is_black_piece) {
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

PieceType piece_at_square(const Board* board, int square) {
    uint64_t mask = 1ULL << square;
    for (int i = 0; i < PIECE_COUNT; i++) {
        if (mask & board->bitboards[i]) {
            return (PieceType)i;
        }
    }
    return EMPTY;
}

static int in_bounds(int rank, int file) { return rank >= 0 && rank < 8 && file >= 0 && file < 8; }

// we can be pretty loose with the checks because if the en_passant_square is checked then these checks should suffice
static int is_en_passant_capture_move(const Board* board, PieceType type, int new_square) {
    if (type != B_PAWN && type != W_PAWN) {
        return 0;
    }

    if (new_square != board->en_passant_square || new_square < 0 || new_square >= 64) {
        return 0;
    }

    return 1;
}

static int is_square_attacked(const Board* board, int square, int attacker_must_be_black) {
    int rank = square / 8;
    int file = square % 8;

    // check for pawn attacks
    if (attacker_must_be_black) {
        if (in_bounds(rank - 1, file - 1) && piece_at_square(board, ((rank - 1) * 8) + (file - 1)) == B_PAWN) return 1;
        if (in_bounds(rank - 1, file + 1) && piece_at_square(board, ((rank - 1) * 8) + (file + 1)) == B_PAWN) return 1;
    } else {
        if (in_bounds(rank + 1, file - 1) && piece_at_square(board, ((rank + 1) * 8) + (file - 1)) == W_PAWN) return 1;
        if (in_bounds(rank + 1, file + 1) && piece_at_square(board, ((rank + 1) * 8) + (file + 1)) == W_PAWN) return 1;
    }

    static const int knight_offsets[8][2] = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2}, {1, -2}, {1, 2}, {2, -1}, {2, 1}};
    // check for knight attacks
    for (int i = 0; i < 8; i++) {
        int attack_rank = rank + knight_offsets[i][0];
        int attack_file = file + knight_offsets[i][1];
        if (!in_bounds(attack_rank, attack_file)) continue;

        PieceType attacker = piece_at_square(board, get_square(attack_rank, attack_file));
        if (attacker == EMPTY) continue;

        if (attacker_must_be_black && attacker == B_KNIGHT) return 1;
        if (!attacker_must_be_black && attacker == W_KNIGHT) return 1;
    }

    static const int king_offsets[8][2] = {{1, 0}, {-1, 0}, {-1, 1}, {-1, -1}, {1, -1}, {1, 1}, {0, -1}, {0, 1}};

    for (int i = 0; i < 8; i++) {
        int attack_rank = rank + king_offsets[i][0];
        int attack_file = file + king_offsets[i][1];
        if (!in_bounds(attack_rank, attack_file)) continue;

        PieceType attacker = piece_at_square(board, get_square(attack_rank, attack_file));
        if (attacker == EMPTY) continue;

        if (attacker_must_be_black && attacker == B_KING) return 1;
        if (!attacker_must_be_black && attacker == W_KING) return 1;
    }

    static const int rook_directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    static const int bishop_directions[4][2] = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};

    for (int direction = 0; direction < 4; direction++) {
        for (int step = 1;; step++) {
            int attack_rank = rank + rook_directions[direction][0] * step;
            int attack_file = file + rook_directions[direction][1] * step;
            if (!in_bounds(attack_rank, attack_file)) break;

            PieceType attacker = piece_at_square(board, get_square(attack_rank, attack_file));
            if (attacker == EMPTY) continue;

            if (attacker_must_be_black && (attacker == B_ROOK || attacker == B_QUEEN)) return 1;
            if (!attacker_must_be_black && (attacker == W_ROOK || attacker == W_QUEEN)) return 1;

            break;
        }
    }

    for (int direction = 0; direction < 4; direction++) {
        for (int step = 1;; step++) {
            int attack_rank = rank + bishop_directions[direction][0] * step;
            int attack_file = file + bishop_directions[direction][1] * step;
            if (!in_bounds(attack_rank, attack_file)) break;

            PieceType attacker = piece_at_square(board, (attack_rank * 8) + attack_file);
            if (attacker == EMPTY) continue;

            if (attacker_must_be_black && (attacker == B_BISHOP || attacker == B_QUEEN)) return 1;
            if (!attacker_must_be_black && (attacker == W_BISHOP || attacker == W_QUEEN)) return 1;

            break;
        }
    }

    return 0;
}

static int can_castle(Board* board, PieceType king_type, int current_square, int is_kingside) {
    int rank = current_square / 8, file = current_square % 8;
    int attacker_must_be_black = king_type == W_KING;

    if (king_type == B_KING) {
        if ((is_kingside && !board->b_castle_kingside) || (!is_kingside && !board->b_castle_queenside)) {
            return 0;
        }
    } else {
        if ((is_kingside && !board->w_castle_kingside) || (!is_kingside && !board->w_castle_queenside)) {
            return 0;
        }
    }

    if (is_square_attacked(board, current_square, attacker_must_be_black)) {
        return 0;
    }

    if (is_kingside) {
        if (is_piece_at(board, rank, file + 1) || is_piece_at(board, rank, file + 2)) {
            return 0;
        }
        if (is_square_attacked(board, current_square + 1, attacker_must_be_black) ||
            is_square_attacked(board, current_square + 2, attacker_must_be_black)) {
            return 0;
        }

        return 1;
    } else {
        if (is_piece_at(board, rank, file - 1) || is_piece_at(board, rank, file - 2) || is_piece_at(board, rank, file - 3)) {
            return 0;
        }
        if (is_square_attacked(board, current_square - 1, attacker_must_be_black) ||
            is_square_attacked(board, current_square - 2, attacker_must_be_black)) {
            return 0;
        }

        return 1;
    }
}

static void move_castling_rook(Board* board, PieceType king_type, int current_square, int new_square) {
    // if we are moving a king and it moves 2 squares then it has to be a castle!
    // fun little trick
    if ((new_square - current_square) != 2 && (current_square - new_square) != 2) {
        return;
    }
    int is_kingside = (new_square > current_square) ? 1 : 0;
    int direction = is_kingside ? -2 : 3;
    int is_king_black = king_type == B_KING;

    if (is_king_black) {
        int square = is_kingside ? 7 : 0;
        // on the kingside the rook is at the 7th square
        // on the queenside the rook is at the 0th square
        // we are removing it from the bitboard
        board->bitboards[B_ROOK] &= ~(1ULL << square);
        // 5 is the square where it ends up at on kingside
        // 3 is the square where it ends up at on kingside
        board->bitboards[B_ROOK] |= 1ULL << (square + direction);
    } else {
        int square = is_kingside ? 63 : 56;
        // on the kingside the rook is at the 63rd square
        // on the queenside the rook is at the 56th square
        // we are removing it from the bitboard
        board->bitboards[W_ROOK] &= ~(1ULL << square);
        // 59 is the square where it ends up at on kingside
        // 61 is the square where it ends up at on kingside
        board->bitboards[W_ROOK] |= 1ULL << (square + direction);
    }
}

int is_piece_at(const Board* board, int rank, int file) {
    uint64_t mask = 1ULL << get_square(rank, file);
    for (int i = 0; i < PIECE_COUNT; i++) {
        if (mask & board->bitboards[i]) {
            return 1;
        }
    }
    return 0;
}

static void update_castling_rules(Board* board, PieceType type, int current_square, int new_square,
                                  PieceType captured_piece) {
    // if the king moves then it can no longer castle
    if (type == B_KING && (board->b_castle_kingside || board->b_castle_queenside)) {
        board->b_castle_kingside = 0;
        board->b_castle_queenside = 0;
    }
    if (type == W_KING && (board->w_castle_kingside || board->w_castle_queenside)) {
        board->w_castle_kingside = 0;
        board->w_castle_queenside = 0;
    }

    // if the rook moves then we can no longer castle
    if (type == B_ROOK && current_square == 0 && board->b_castle_queenside) {
        board->b_castle_queenside = 0;
    }

    if (type == B_ROOK && current_square == 7 && board->b_castle_kingside) {
        board->b_castle_kingside = 0;
    }

    if (type == W_ROOK && current_square == 56 && board->w_castle_queenside) {
        board->w_castle_queenside = 0;
    }

    if (type == W_ROOK && current_square == 63 && board->w_castle_kingside) {
        board->w_castle_kingside = 0;
    }

    // if the captured piece is one of the rooks then we need to disallow castling
    if (captured_piece == B_ROOK) {
        if (new_square == 0) board->b_castle_queenside = 0;
        if (new_square == 7) board->b_castle_kingside = 0;
    }
    if (captured_piece == W_ROOK) {
        if (new_square == 56) board->w_castle_queenside = 0;
        if (new_square == 63) board->w_castle_kingside = 0;
    }
}

// https://stackoverflow.com/questions/26252928/display-msb-to-lsb#26253009
// I use this with layout 0 on https://gekomad.github.io/Cinnamon/BitboardCalculator/
// for debugging
__attribute__((unused)) static void print_bitboard(uint64_t n) {
    for (int i = 63; i >= 0; i--) {
        printf("%llu", (n >> i) & 1ULL);
    }
    printf("\n");
}

int has_legal_moves(Board* board, int black_to_move) {
    int start = black_to_move ? B_PAWN : W_PAWN;
    int end = black_to_move ? B_KING : W_KING;

    for (int pt = start; pt <= end; pt++) {
        BITBOARD bb = board->bitboards[pt];
        if (!bb) continue;
        BITBOARD b = bb;
        while (b) {
            int sq = __builtin_ctzll(b);
            if (get_legal_moves(board, (PieceType)pt, sq) != 0ULL) return 1;
            b &= b - 1;
        }
    }
    return 0;
}

int is_checkmate(Board* board, int black_to_move) {
    if (has_legal_moves(board, black_to_move)) return 0;

    // find king square
    PieceType king_pt = black_to_move ? B_KING : W_KING;
    BITBOARD kbb = board->bitboards[king_pt];
    if (!kbb) return 0;  // no king? treat as not checkmate here
    int king_sq = __builtin_ctzll(kbb);

    int attacker_is_black = !black_to_move;
    if (is_square_attacked(board, king_sq, attacker_is_black)) return 1;
    return 0;
}

int is_stalemate(Board* board, int black_to_move) {
    if (has_legal_moves(board, black_to_move)) return 0;

    // find king square
    PieceType king_pt = black_to_move ? B_KING : W_KING;
    BITBOARD kbb = board->bitboards[king_pt];
    if (!kbb) return 0;  // no king? treat as not stalemate
    int king_sq = __builtin_ctzll(kbb);

    int attacker_is_black = !black_to_move;
    if (!is_square_attacked(board, king_sq, attacker_is_black)) return 1;
    return 0;
}

int is_fifty_move_draw(Board* board) { return board->halfmove_clock >= 100; }

static int get_square(int rank, int file) { return (rank * 8) + file; }