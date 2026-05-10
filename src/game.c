#include "game.h"

#include <stdio.h>

#define BOARD_SIZE 640
#define TILE_SIZE (BOARD_SIZE / 8)

static PieceType piece_at(const Board* board, int square);
static void draw_piece(Texture2D tex, int x, int y);
static void init_textures(Game* game);
static int is_mouse_within(int x, int y);

Game init_game() {
    Game game = {.light_color = (Color){240, 217, 181, 255},
                 .dark_color = (Color){181, 136, 99, 255},
                 .held_piece = (HeldPiece){.active = 0, .square = -1}};
    return game;
}

static void draw_board(Game* game, Board* board) {
    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int x = file * TILE_SIZE;
            int y = rank * TILE_SIZE;
            Color square_color = ((rank + file) % 2 == 0) ? game->light_color : game->dark_color;
            DrawRectangle(x, y, TILE_SIZE, TILE_SIZE, square_color);
        }
    }

    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int x = file * TILE_SIZE;
            int y = rank * TILE_SIZE;
            int square = rank * 8 + file;

            PieceType piece = piece_at(board, square);
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && is_mouse_within(x, y)) {
                if (!game->held_piece.active && piece != EMPTY) {
                    game->held_piece = (HeldPiece){.active = 1, .square = square, .type = piece};
                } else {
                    move_piece(board, game->held_piece.square, square);
                    game->held_piece = (HeldPiece){.active = 0};
                }
            }

            if (piece != EMPTY) {
                if (game->held_piece.active && game->held_piece.square == square) continue;

                draw_piece(game->textures[piece], x, y);
            }
        }
    }

    if (game->held_piece.active) {
        draw_piece(game->textures[game->held_piece.type], GetMouseX() - (TILE_SIZE / 2), GetMouseY() - (TILE_SIZE / 2));
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

    if (mask & board->bitboards[W_PAWN]) return W_PAWN;
    if (mask & board->bitboards[W_ROOK]) return W_ROOK;
    if (mask & board->bitboards[W_KNIGHT]) return W_KNIGHT;
    if (mask & board->bitboards[W_BISHOP]) return W_BISHOP;
    if (mask & board->bitboards[W_QUEEN]) return W_QUEEN;
    if (mask & board->bitboards[W_KING]) return W_KING;
    if (mask & board->bitboards[B_PAWN]) return B_PAWN;
    if (mask & board->bitboards[B_ROOK]) return B_ROOK;
    if (mask & board->bitboards[B_KNIGHT]) return B_KNIGHT;
    if (mask & board->bitboards[B_BISHOP]) return B_BISHOP;
    if (mask & board->bitboards[B_QUEEN]) return B_QUEEN;
    if (mask & board->bitboards[B_KING]) return B_KING;

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

static int is_mouse_within(int x, int y) {
    int mouse_x = GetMouseX();
    int mouse_y = GetMouseY();
    return mouse_x >= x && mouse_x <= x + TILE_SIZE && mouse_y >= y && mouse_y <= y + TILE_SIZE;
}