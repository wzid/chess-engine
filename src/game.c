#include "game.h"

#define BOARD_SIZE 640
#define TILE_SIZE (BOARD_SIZE / 8)

static PieceType piece_at(const Board* board, int square);
static void draw_piece(Texture2D tex, int x, int y);
static void init_textures(Game* game);

Game init_game() {
    Game game = {
        .light_color = (Color){240, 217, 181, 255},
        .dark_color = (Color){181, 136, 99, 255},
    };
    return game;
}

static void draw_board(const Game* game, const Board* board) {
    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int x = file * TILE_SIZE;
            int y = rank * TILE_SIZE;
            int square = rank * 8 + file;
            Color square_color = ((rank + file) % 2 == 0) ? game->light_color : game->dark_color;

            DrawRectangle(x, y, TILE_SIZE, TILE_SIZE, square_color);

            PieceType piece = piece_at(board, square);
            if (piece != EMPTY) {
                draw_piece(game->textures[piece], x, y);
            }
        }
    }
}

void game_loop(Game* game, Board* board) {
    InitWindow(BOARD_SIZE, BOARD_SIZE, "Chess Engine w/raylib");
    SetTargetFPS(60);

    init_textures(game);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){24, 26, 32, 255});

        draw_board(game, board);

        EndDrawing();
    }

    for (int i = 0; i < PIECE_COUNT; i++) {
        UnloadTexture(game->textures[i]);
    }

    CloseWindow();
}

static void draw_piece(Texture2D tex, int x, int y) {
    if (tex.id == 0) return;  // defensive: texture not loaded

    DrawTexturePro(tex, (Rectangle){0, 0, (float)tex.width, (float)tex.height},  // source rectangle (full image)
                   (Rectangle){x, y, TILE_SIZE, TILE_SIZE}, (Vector2){0, 0},     // origin offset (not needed here)
                   0.0f,                                                         // rotation
                   WHITE                                                         // tint (WHITE = no tint)
    );
}

static PieceType piece_at(const Board* board, int square) {
    if (square < 0 || square >= 64) return EMPTY;
    uint64_t mask = 1ULL << square;

    if (mask & board->w_pawn) return W_PAWN;
    if (mask & board->w_rook) return W_ROOK;
    if (mask & board->w_knight) return W_KNIGHT;
    if (mask & board->w_bishop) return W_BISHOP;
    if (mask & board->w_queen) return W_QUEEN;
    if (mask & board->w_king) return W_KING;
    if (mask & board->b_pawn) return B_PAWN;
    if (mask & board->b_rook) return B_ROOK;
    if (mask & board->b_knight) return B_KNIGHT;
    if (mask & board->b_bishop) return B_BISHOP;
    if (mask & board->b_queen) return B_QUEEN;
    if (mask & board->b_king) return B_KING;

    return EMPTY;
}

static void init_textures(Game* game) {
    game->textures[W_PAWN] = LoadTexture("assets/wp.png");
    game->textures[W_ROOK] = LoadTexture("assets/wr.png");
    game->textures[W_KNIGHT] = LoadTexture("assets/wn.png");
    game->textures[W_BISHOP] = LoadTexture("assets/wb.png");
    game->textures[W_QUEEN] = LoadTexture("assets/wq.png");
    game->textures[W_KING] = LoadTexture("assets/wk.png");
    game->textures[B_PAWN] = LoadTexture("assets/bp.png");
    game->textures[B_ROOK] = LoadTexture("assets/br.png");
    game->textures[B_KNIGHT] = LoadTexture("assets/bn.png");
    game->textures[B_BISHOP] = LoadTexture("assets/bb.png");
    game->textures[B_QUEEN] = LoadTexture("assets/bq.png");
    game->textures[B_KING] = LoadTexture("assets/bk.png");
}