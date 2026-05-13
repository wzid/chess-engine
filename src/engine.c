#include "engine.h"

#include <limits.h>
#include <stdlib.h>

#include "board.h"

typedef struct {
    PieceType piece;
    int from;
    int to;
    int score;
} BestMove;

typedef struct {
    PieceType piece;
    int from;
    int to;
    int sort_key;
} ScoredMove;

typedef struct {
    int from;
    int to;
} KillerSlot;

#define MAX_MOVES 256
#define MAX_PLY 64

static const int SEARCH_INF = 1000000000;

/*
 * Mate score must dwarf normal eval (a few thousand centipawns) but stay below SEARCH_INF.
 * The +ply term: side being mated gets a slightly less awful score deeper in the tree, so
 * after negamax the winner prefers the shortest mate line.
 */
#define MATE_SCORE 50000

/*
 * Move ordering tries strong moves first so alpha-beta cuts off branches earlier (fewer nodes).
 *
 * Ply: distance from the root of the current search. Root nega_max uses ply 0; each recursive
 * call passes ply + 1 (one half-move deeper). Used so killer slots match the same tree height.
 *
 * Killer moves (g_killers + record_killer): when a quiet move causes a beta cutoff, it often
 * refutes sibling positions too. We remember up to two such (from,to) pairs per ply; later
 * nodes at that ply score matching moves higher so they are tried sooner. Captures are excluded
 * (MVV-LVA already handles them). g_killers is global to the search; clear_killers runs at each
 * new engine_move so data does not leak across unrelated root searches.
 */
static KillerSlot g_killers[MAX_PLY][2];

static int piece_score(PieceType type);
static int finalize_promotion_if_needed(Board* board, PieceType piece);
static int eval_leaf(Board* board, int side_to_move_black, int ply);
static void clear_killers(void);
static void record_killer(int ply, int from, int to, int is_capture);
static PieceType victim_for_move(const Board* board, PieceType mover, int to);
static int is_capture_move(const Board* board, PieceType mover, int to);
static int move_sort_key(const Board* board, PieceType piece, int from, int to, int ply);
static int cmp_scored_move_desc(const void* a, const void* b);
static int collect_scored_moves(Board* board, int is_black, int ply, ScoredMove* out);

// clang-format off
static int pawn_table[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    50, 50, 50, 50, 50, 50, 50, 50,
    10, 10, 20, 30, 30, 20, 10, 10,
    5,  5, 10, 25, 25, 10,  5,  5,
    0,  0,  0, 20, 20,  0,  0,  0,
    5, -5,-10,  0,  0,-10, -5,  5,
    5, 10, 10,-20,-20, 10, 10,  5,
    0,  0,  0,  0,  0,  0,  0,  0
};

static int knight_table[64] = {
    -50,-40,-30,-30,-30,-30,-40,-50,
    -40,-20,  0,  0,  0,  0,-20,-40,
    -30,  0, 10, 15, 15, 10,  0,-30,
    -30,  5, 15, 20, 20, 15,  5,-30,
    -30,  0, 15, 20, 20, 15,  0,-30,
    -30,  5, 10, 15, 15, 10,  5,-30,
    -40,-20,  0,  5,  5,  0,-20,-40,
    -50,-40,-30,-30,-30,-30,-40,-50,
};

static int bishop_table[64] = {
    -20,-10,-10,-10,-10,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5, 10, 10,  5,  0,-10,
    -10,  5,  5, 10, 10,  5,  5,-10,
    -10,  0, 10, 10, 10, 10,  0,-10,
    -10, 10, 10, 10, 10, 10, 10,-10,
    -10,  5,  0,  0,  0,  0,  5,-10,
    -20,-10,-10,-10,-10,-10,-10,-20,
};

static int rook_table[64] = {
    0,  0,  0,  0,  0,  0,  0,  0,
    5, 10, 10, 10, 10, 10, 10,  5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    0,  0,  0,  5,  5,  0,  0,  0
};

static int queen_table[64] = {
    -20,-10,-10, -5, -5,-10,-10,-20,
    -10,  0,  0,  0,  0,  0,  0,-10,
    -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
    0,  0,  5,  5,  5,  5,  0, -5,
    -10,  5,  5,  5,  5,  5,  0,-10,
    -10,  0,  5,  0,  0,  0,  0,-10,
    -20,-10,-10, -5, -5,-10,-10,-20
};

static int king_table_mg[64] = {
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -30,-40,-40,-50,-50,-40,-40,-30,
    -20,-30,-30,-40,-40,-30,-30,-20,
    -10,-20,-20,-20,-20,-20,-20,-10,
    20, 20,  0,  0,  0,  0, 20, 20,
    20, 30, 10,  0,  0, 10, 30, 20
};
// clang-format on

static int* white_sq_weights[6] = { pawn_table, knight_table, bishop_table, rook_table, queen_table, king_table_mg };

Engine init_engine() { return (Engine){.depth = 6}; }

/* Reset killer table before a new root search (new engine_move). */
static void clear_killers(void) {
    for (int p = 0; p < MAX_PLY; p++) {
        g_killers[p][0].from = g_killers[p][0].to = -1;
        g_killers[p][1].from = g_killers[p][1].to = -1;
    }
}

/*
 * Remember a quiet move that refuted this node (beta cutoff). Older slot [1] is bumped when
 * the new killer differs from the current primary [0].
 */
static void record_killer(int ply, int from, int to, int is_capture) {
    if (is_capture || ply < 0 || ply >= MAX_PLY) {
        return;
    }
    if (g_killers[ply][0].from == from && g_killers[ply][0].to == to) {
        return;
    }
    g_killers[ply][1] = g_killers[ply][0];
    g_killers[ply][0].from = from;
    g_killers[ply][0].to = to;
}

static PieceType victim_for_move(const Board* board, PieceType mover, int to) {
    /* Destination square occupant, or the EP-captured pawn (not on `to`). */
    if ((mover == B_PAWN || mover == W_PAWN) && to >= 0 && to == board->en_passant_square) {
        int cap_sq = (mover == B_PAWN) ? (to - 8) : (to + 8);
        return piece_at_square(board, cap_sq);
    }
    return piece_at_square(board, to);
}

static int different_sides(PieceType a, PieceType b) {
    int a_black = (a <= B_KING);
    int b_black = (b <= B_KING);
    return a_black != b_black;
}

static int is_capture_move(const Board* board, PieceType mover, int to) {
    PieceType vic = victim_for_move(board, mover, to);
    return vic != EMPTY && different_sides(mover, vic);
}

/* Larger key is tried first after qsort descending. Tiers: captures, killers, promos, castle. */
static int move_sort_key(const Board* board, PieceType piece, int from, int to, int ply) {
    /* MVV-LVA: good captures first (valuable victim, cheap attacker). */
    if (is_capture_move(board, piece, to)) {
        PieceType vic = victim_for_move(board, piece, to);
        return 1000000 + 100 * piece_score(vic) - piece_score(piece);
    }
    /* Quiet moves that recently cut off at this ply — see g_killers comment above. */
    if (ply >= 0 && ply < MAX_PLY) {
        if (g_killers[ply][0].from == from && g_killers[ply][0].to == to) {
            return 90000;
        }
        if (g_killers[ply][1].from == from && g_killers[ply][1].to == to) {
            return 80000;
        }
    }
    /* Pawn step onto promotion rank (engine promotes to queen after the search step). */
    int rank = to / 8;
    if (piece == B_PAWN && rank == 7) {
        return 70000;
    }
    if (piece == W_PAWN && rank == 0) {
        return 70000;
    }
    /* King moves two files: castling. */
    if (piece == B_KING || piece == W_KING) {
        int df = (to % 8) - (from % 8);
        if (df == 2 || df == -2) {
            return 60000;
        }
    }
    return 0;
}

static int cmp_scored_move_desc(const void* a, const void* b) {
    /* qsort expects negative when `a` sorts before `b`; we want larger sort_key first. */
    const ScoredMove* ma = (const ScoredMove*)a;
    const ScoredMove* mb = (const ScoredMove*)b;
    if (ma->sort_key > mb->sort_key) {
        return -1;
    }
    if (ma->sort_key < mb->sort_key) {
        return 1;
    }
    return 0;
}

static int collect_scored_moves(Board* board, int is_black, int ply, ScoredMove* out) {
    /* Enumerate every legal move for the side to move; sort_key is filled for ordering. */
    int n = 0;
    int start = is_black ? B_PAWN : W_PAWN;
    int end = is_black ? B_KING : W_KING;
    for (int i = start; i <= end; i++) {
        BITBOARD bb = board->bitboards[i];
        while (bb) {
            int square = __builtin_ctzll(bb);
            BITBOARD legal_moves = get_legal_moves(board, i, square);
            while (legal_moves) {
                int new_square = __builtin_ctzll(legal_moves);
                if (n >= MAX_MOVES) {
                    return n;
                }
                out[n].piece = (PieceType)i;
                out[n].from = square;
                out[n].to = new_square;
                out[n].sort_key = move_sort_key(board, (PieceType)i, square, new_square, ply);
                n++;
                legal_moves &= legal_moves - 1;
            }
            bb &= bb - 1;
        }
    }
    return n;
}

/* Static eval from side to move's view, or terminal: checkmate / stalemate (no legal moves). */
static int eval_leaf(Board* board, int side_to_move_black, int ply) {
    if (!has_legal_moves(board, side_to_move_black)) {
        if (is_checkmate(board, side_to_move_black)) {
            return -MATE_SCORE + ply;
        }
        return 0;
    }
    return eval_board(board, side_to_move_black);
}

/*
 * Negamax + alpha-beta. depth = remaining plies; ply = index from root (for killers).
 * Moves are collected, scored, sorted, then tried in order.
 */
static BestMove nega_max(Board* board, int is_black, int depth, int alpha, int beta, int ply) {
    ScoredMove moves[MAX_MOVES];
    int move_count = collect_scored_moves(board, is_black, ply, moves);
    BestMove best = {EMPTY, -1, -1, INT_MIN};

    if (move_count == 0) {
        best.score = eval_leaf(board, is_black, ply);
        return best;
    }

    qsort(moves, (size_t)move_count, sizeof(ScoredMove), cmp_scored_move_desc);

    for (int mi = 0; mi < move_count; mi++) {
        PieceType piece = moves[mi].piece;
        int square = moves[mi].from;
        int new_square = moves[mi].to;
        int cap = is_capture_move(board, piece, new_square);

        Board new_board = *board;
        if (!move_piece(&new_board, piece, square, new_square)) {
            continue;
        }
        finalize_promotion_if_needed(&new_board, piece);
        int score = (depth == 1) ? -eval_leaf(&new_board, !is_black, ply + 1)
                                 : -nega_max(&new_board, !is_black, depth - 1, -beta, -alpha, ply + 1).score;

        if (score > best.score) {
            best.piece = piece;
            best.from = square;
            best.to = new_square;
            best.score = score;
        }

        if (score > alpha) {
            alpha = score;
        }

        if (alpha >= beta) {
            /* Beta cutoff: this move refuted the line; remember if quiet (killer heuristic). */
            record_killer(ply, square, new_square, cap);
            return best;
        }
    }

    return best;
}

int engine_move(Engine* engine, Board* board, int is_black) {
    clear_killers(); /* ply 0 and killer table for this search only */
    BestMove best = nega_max(board, is_black, engine->depth, -SEARCH_INF, SEARCH_INF, 0);

    if (best.piece == EMPTY) {
        return 0;
    }

    if (!move_piece(board, best.piece, best.from, best.to)) {
        return 0;
    }

    finalize_promotion_if_needed(board, best.piece);
    return 1;
}

int eval_board(Board* board, int is_black) {
    int multiplier = is_black ? -1 : 1;

    int black_piece_value = 0;
    int black_sq_values = 0;
    for (int i = B_PAWN; i <= B_KING; i++) {
        BITBOARD b = board->bitboards[i];
        while (b) {
            int s = __builtin_ctzll(b);
            black_piece_value += piece_score(i);
            black_sq_values += white_sq_weights[i][63 - s];
            b &= b - 1;
        }
    }

    int white_piece_value = 0;
    int white_sq_values = 0;
    for (int i = W_PAWN; i <= W_KING; i++) {
        BITBOARD b = board->bitboards[i];
        while (b) {
            int s = __builtin_ctzll(b);
            white_piece_value += piece_score(i);
            white_sq_values += white_sq_weights[i - W_PAWN][s];
            b &= b - 1;
        }
    }

    return ((white_piece_value - black_piece_value) + (white_sq_values - black_sq_values)) * multiplier;
}

static int finalize_promotion_if_needed(Board* board, PieceType piece) {
    if (!board->promotion_pending) {
        return 1;
    }

    PieceType promotion_type = (piece <= B_KING) ? B_QUEEN : W_QUEEN;
    return promote_pawn(board, promotion_type);
}

// https://www.chessprogramming.org/Simplified_Evaluation_Function#Piece_Values
static int piece_score(PieceType type) {
    switch (type) {
        case B_PAWN:
        case W_PAWN:
            return 100;
        case B_KNIGHT:
        case W_KNIGHT:
            return 320;
        case B_BISHOP:
        case W_BISHOP:
            return 330;
        case B_ROOK:
        case W_ROOK:
            return 500;
        case B_QUEEN:
        case W_QUEEN:
            return 900;
        case B_KING:
        case W_KING:
            return 2000;
        default:
            return 0;
    }
}
