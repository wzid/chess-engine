#include "game.h"

#include <stdio.h>

#define BOARD_SIZE 640
#define TILE_SIZE (BOARD_SIZE / 8)
#define SIDE_PANEL_WIDTH 180

static PieceType piece_at(const Board* board, int square);
static void draw_piece(Texture2D tex, int x, int y);
static void init_textures(Game* game);
static int square_under_mouse(void);
static int is_white_piece(PieceType piece);
static int piece_belongs_to_turn(PieceType piece, int current_turn);
static void handle_mouse_press(Game* game, Board* board, int square, PieceType piece);
static void handle_mouse_release(Game* game, Board* board);
static int try_move_selected_piece(Game* game, Board* board, int target_square);
static void select_piece(Board* board, Game* game, int square, PieceType piece, int start_dragging);
static void draw_move_hints(const Board* board, Game* game);
static void draw_ui(const Game* game);
static void evaluate_game_state(Game* game, Board* board);
static const char* result_text(const Game* game);

Game init_game() {
    Game game = {.light_color = (Color){235, 235, 200, 255},
                 .dark_color = (Color){115, 150, 80, 255},
                 .selected_piece = (SelectedPiece){.active = 0, .square = -1, .type = EMPTY},
                 .dragging = 0,
                 .current_turn = 0,
                 .game_over = 0,
                 .result = 0};
    return game;
}

static void draw_board(Game* game, Board* board) {
    if (game->game_over) {
        game->selected_piece.active = 0;
        game->selected_piece.legal_moves = 0ULL;
        game->dragging = 0;
    }

    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            int x = file * TILE_SIZE;
            int y = rank * TILE_SIZE;
            int square = rank * 8 + file;
            Color square_color = ((rank + file) % 2 == 0) ? game->light_color : game->dark_color;
            PieceType piece = piece_at(board, square);

            if (game->selected_piece.active && game->selected_piece.square == square) {
                DrawRectangle(x, y, TILE_SIZE, TILE_SIZE, (Color){245, 245, 130, 255});
            } else {
                DrawRectangle(x, y, TILE_SIZE, TILE_SIZE, square_color);
            }

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && square_under_mouse() == square) {
                handle_mouse_press(game, board, square, piece);
            }

            if (piece != EMPTY) {
                if (game->selected_piece.active && game->selected_piece.square == square && game->dragging) {
                    continue;
                }

                draw_piece(game->textures[piece], x, y);
            }
        }
    }

    draw_move_hints(board, game);

    if (game->selected_piece.active) {
        if (game->dragging) {
            draw_piece(game->textures[game->selected_piece.type], GetMouseX() - (TILE_SIZE / 2),
                       GetMouseY() - (TILE_SIZE / 2));
        }
    }

    if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        handle_mouse_release(game, board);
    }
}

void game_loop(Game* game, Board* board) {
    InitWindow(BOARD_SIZE + SIDE_PANEL_WIDTH, BOARD_SIZE, "Chess Engine");
    SetTargetFPS(60);

    init_textures(game);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground((Color){24, 26, 32, 255});

        draw_board(game, board);
        draw_ui(game);

        EndDrawing();
    }

    for (int i = 0; i < PIECE_COUNT; i++) {
        UnloadTexture(game->textures[i]);
    }

    CloseWindow();
}

static void draw_piece(Texture2D tex, int x, int y) {
    if (tex.id == 0) return;

    DrawTexturePro(tex, (Rectangle){0, 0, (float)tex.width, (float)tex.height}, (Rectangle){x, y, TILE_SIZE, TILE_SIZE},
                   (Vector2){0, 0}, 0.0f, WHITE);
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

static int square_under_mouse(void) {
    int mouse_x = GetMouseX();
    int mouse_y = GetMouseY();

    if (mouse_x < 0 || mouse_x >= BOARD_SIZE || mouse_y < 0 || mouse_y >= BOARD_SIZE) {
        return -1;
    }

    return (mouse_y / TILE_SIZE) * 8 + (mouse_x / TILE_SIZE);
}

static int is_white_piece(PieceType piece) { return piece >= W_PAWN && piece <= W_KING; }

static int piece_belongs_to_turn(PieceType piece, int current_turn) {
    if (piece == EMPTY) return 0;
    return current_turn == 0 ? is_white_piece(piece) : !is_white_piece(piece);
}

static void select_piece(Board* board, Game* game, int square, PieceType piece, int start_dragging) {
    game->selected_piece =
        (SelectedPiece){.active = 1, .square = square, .type = piece, .legal_moves = get_legal_moves(board, piece, square)};
    game->dragging = start_dragging;
}

static void handle_mouse_press(Game* game, Board* board, int square, PieceType piece) {
    if (game->game_over) {
        return;
    }

    if (!game->selected_piece.active) {
        if (piece_belongs_to_turn(piece, game->current_turn)) {
            select_piece(board, game, square, piece, 1);
        }
        return;
    }
    // piece is already selected

    if (square == game->selected_piece.square) {
        game->dragging = 1;
        return;
    }

    if (piece_belongs_to_turn(piece, game->current_turn)) {
        select_piece(board, game, square, piece, 1);
        return;
    }

    if (piece == EMPTY) {
        game->selected_piece.active = 0;
        game->selected_piece.legal_moves = 0ULL;
        game->dragging = 0;
        return;
    }

    try_move_selected_piece(game, board, square);
}

static void handle_mouse_release(Game* game, Board* board) {
    if (game->game_over) {
        return;
    }

    if (!game->dragging || !game->selected_piece.active) {
        return;
    }

    int release_square = square_under_mouse();
    if (release_square >= 0) {
        if (try_move_selected_piece(game, board, release_square)) {
            return;
        }
    }

    game->dragging = 0;
}

static int try_move_selected_piece(Game* game, Board* board, int target_square) {
    if (!game->selected_piece.active) {
        return 0;
    }

    if (game->game_over) {
        return 0;
    }

    // if you didnt move it at all.
    if (target_square == game->selected_piece.square) {
        return 0;
    }

    if (!move_piece(board, game->selected_piece.type, game->selected_piece.square, target_square)) {
        return 0;
    }

    // unselect
    game->selected_piece.active = 0;
    game->selected_piece.legal_moves = 0ULL;
    game->dragging = 0;
    
    game->current_turn = 1 - game->current_turn;

    evaluate_game_state(game, board);
    return 1;
}

static void draw_move_hints(const Board* board, Game* game) {
    if (!game->selected_piece.active) {
        return;
    }

    for (int square = 0; square < 64; square++) {
        uint64_t mask = 1ULL << square;
        // game->selected_piece.legal_moves is set in the select_piece function
        if (!(mask & game->selected_piece.legal_moves)) {
            continue;
        }
        int x = (square % 8) * TILE_SIZE;
        int y = (square / 8) * TILE_SIZE;

        if (is_piece_at_sq(board, square)) {
            DrawRing((Vector2){x + TILE_SIZE / 2, y + TILE_SIZE / 2}, 34, 40, 0, 360, 32, (Color){40, 40, 40, 100});

        } else {
            DrawCircle(x + TILE_SIZE / 2, y + TILE_SIZE / 2, 15, (Color){40, 40, 40, 100});
        }
    }
}

static void draw_ui(const Game* game) {
    int ui_x = BOARD_SIZE + 20;
    int ui_y = 20;

    DrawText("Current Turn:", ui_x, ui_y, 20, RAYWHITE);
    const char* turn_text = (game->current_turn == 0) ? "WHITE" : "BLACK";
    Color turn_color = (game->current_turn == 0) ? (Color){240, 240, 240, 255} : (Color){60, 60, 60, 255};
    DrawText(turn_text, ui_x, ui_y + 30, 28, turn_color);

    if (game->game_over) {
        DrawText("Game Over", ui_x, ui_y + 70, 20, (Color){255, 220, 120, 255});
        DrawText(result_text(game), ui_x, ui_y + 100, 22, (Color){255, 255, 255, 255});
    }
}

static void evaluate_game_state(Game* game, Board* board) {
    if (is_checkmate(board, game->current_turn)) {
        game->game_over = 1;
        game->result = (game->current_turn == 0) ? 2 : 1;
        return;
    }

    if (is_stalemate(board, game->current_turn) || is_fifty_move_draw(board)) {
        game->game_over = 1;
        game->result = 3;
    }
}

static const char* result_text(const Game* game) {
    switch (game->result) {
        case 1:
            return "White wins";
        case 2:
            return "Black wins";
        case 3:
            return "Draw";
        default:
            return "";
    }
}