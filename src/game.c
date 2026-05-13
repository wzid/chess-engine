#include "game.h"

#include "engine.h"

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
static void draw_ui(Game* game, Board* board);
static void reset_game(Game* game, Board* board);
static void evaluate_game_state(Game* game, Board* board);
static const char* result_text(const Game* game);
static int is_promotion_move(PieceType piece, int target_square);
static void draw_promotion_chooser(Game* game, Board* board);

// Set to 1 for AI as black, 0 for AI as white.
#define AI_PLAYS_BLACK 0
#define AI_TURN (AI_PLAYS_BLACK ? 1 : 0)

Game init_game() {
    Game game = {.light_color = (Color){235, 235, 200, 255},
                 .dark_color = (Color){115, 150, 80, 255},
                 .selected_piece = (SelectedPiece){.active = 0, .square = -1, .type = EMPTY},
                 .dragging = 0,
                 .current_turn = 0,
                 .game_over = 0,
                 .result = 0,
                 .promotion_active = 0,
                 .promotion_from_square = -1,
                 .promotion_to_square = -1,
                 .promotion_pawn_type = EMPTY,
                 .ai_move_pending = 0,
                 .ai_move_delay_frames = 0};

    if (AI_TURN == game.current_turn) {
        game.ai_move_pending = 1;
        game.ai_move_delay_frames = 1;
    }

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

    draw_promotion_chooser(game, board);

    if (game->game_over) {
        DrawRectangle(0, 0, BOARD_SIZE, BOARD_SIZE, (Color){80, 80, 80, 140});
    }
}

void game_loop(Game* game, Board* board) {
    Engine engine = init_engine();

    // Request 4x MSAA before creating the window for smoother rendering
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(BOARD_SIZE + SIDE_PANEL_WIDTH, BOARD_SIZE, "Chess Engine");
    SetTargetFPS(60);

    init_textures(game);

    while (!WindowShouldClose()) {
        if (game->ai_move_delay_frames > 0) {
            game->ai_move_delay_frames--;
        }

        BeginDrawing();
        ClearBackground((Color){24, 26, 32, 255});

        draw_board(game, board);
        draw_ui(game, board);

        EndDrawing();

        if (game->ai_move_pending && game->ai_move_delay_frames == 0 && !game->game_over && !game->promotion_active &&
            game->current_turn == AI_TURN) {
            game->ai_move_pending = 0;
            if (engine_move(&engine, board, AI_TURN)) {
                game->current_turn = 1 - AI_TURN;
            }
            evaluate_game_state(game, board);
        }
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

    // Use bilinear filtering for textures to reduce pixelation when scaled
    for (int i = 0; i < PIECE_COUNT; i++) {
        SetTextureFilter(game->textures[i], TEXTURE_FILTER_BILINEAR);
    }
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
    if (game->game_over || game->promotion_active || game->ai_move_pending || game->current_turn == AI_TURN) {
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

    uint64_t square_mask = 1ULL << square;
    // if its an empty piece and not one of the move hints
    if (piece == EMPTY && !(square_mask & game->selected_piece.legal_moves)) {
        game->selected_piece.active = 0;
        game->selected_piece.legal_moves = 0ULL;
        game->dragging = 0;
        return;
    }

    try_move_selected_piece(game, board, square);
}

static void handle_mouse_release(Game* game, Board* board) {
    if (game->game_over || game->promotion_active) {
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

    if (game->game_over || game->promotion_active) {
        return 0;
    }

    // if you didnt move it at all.
    if (target_square == game->selected_piece.square) {
        return 0;
    }

    if (is_promotion_move(game->selected_piece.type, target_square)) {
        if (!move_piece(board, game->selected_piece.type, game->selected_piece.square, target_square)) {
            return 0;
        }

        game->promotion_active = 1;
        game->promotion_from_square = game->selected_piece.square;
        game->promotion_to_square = target_square;
        game->promotion_pawn_type = game->selected_piece.type;

        game->selected_piece.active = 0;
        game->selected_piece.legal_moves = 0ULL;
        game->dragging = 0;
        return 1;
    }

    if (!move_piece(board, game->selected_piece.type, game->selected_piece.square, target_square)) {
        return 0;
    }

    // unselect
    game->selected_piece.active = 0;
    game->selected_piece.legal_moves = 0ULL;
    game->dragging = 0;

    game->current_turn = 1 - game->current_turn;
    if (game->current_turn == AI_TURN) {
        game->ai_move_pending = 1;
        game->ai_move_delay_frames = 1;
    }

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

static void reset_game(Game* game, Board* board) {
    *board = init_board();
    game->selected_piece = (SelectedPiece){.active = 0, .square = -1, .type = EMPTY, .legal_moves = 0ULL};
    game->dragging = 0;
    game->current_turn = 0;
    game->game_over = 0;
    game->result = 0;
    game->promotion_active = 0;
    game->promotion_from_square = -1;
    game->promotion_to_square = -1;
    game->promotion_pawn_type = EMPTY;
    game->ai_move_pending = 0;
    game->ai_move_delay_frames = 0;

    if (game->current_turn == AI_TURN) {
        game->ai_move_pending = 1;
        game->ai_move_delay_frames = 1;
    }
}

static void draw_ui(Game* game, Board* board) {
    int ui_x = BOARD_SIZE + 18;
    int ui_y = 20;

    DrawText("Current Turn:", ui_x, ui_y, 20, RAYWHITE);
    const char* turn_text = (game->current_turn == 0) ? "WHITE" : "BLACK";
    Color turn_color = (game->current_turn == 0) ? (Color){240, 240, 240, 255} : (Color){60, 60, 60, 255};
    DrawText(turn_text, ui_x, ui_y + 30, 28, turn_color);

    // Restart button (smaller, positioned at bottom of side panel)
    float btn_w = 100.f;
    float btn_h = 28.f;
    float btn_x = (ui_x - 18) + (SIDE_PANEL_WIDTH / 2) - (btn_w / 2);
    float btn_y = BOARD_SIZE - 60.f;
    Rectangle btn = (Rectangle){btn_x, btn_y, btn_w, btn_h};

    DrawRectangleRec(btn, (Color){200, 200, 200, 255});
    DrawText("Restart", (int)(btn_x + (btn_w - MeasureText("Restart", 20)) / 2), (int)(btn_y + 4), 20,
             (Color){20, 20, 20, 255});

    // handle click on restart
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int mx = GetMouseX();
        int my = GetMouseY();
        if (CheckCollisionPointRec((Vector2){(float)mx, (float)my}, btn)) {
            reset_game(game, board);
        }
    }

    if (game->game_over) {
        DrawText("Game Over", ui_x, ui_y + 70, 20, (Color){255, 220, 120, 255});

        // draw bold result
        const char* res = result_text(game);
        DrawText(res, ui_x, ui_y + 100, 22, (Color){255, 255, 255, 255});
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

static int is_promotion_move(PieceType piece, int target_square) {
    if (piece != W_PAWN && piece != B_PAWN) {
        return 0;
    }
    int target_rank = target_square / 8;
    return (piece == W_PAWN && target_rank == 0) || (piece == B_PAWN && target_rank == 7);
}

static void draw_promotion_chooser(Game* game, Board* board) {
    if (!game->promotion_active) {
        return;
    }

    DrawRectangle(0, 0, BOARD_SIZE, BOARD_SIZE, (Color){20, 20, 20, 140});

    PieceType options[4];
    if (game->promotion_pawn_type == W_PAWN) {
        options[0] = W_QUEEN;
        options[1] = W_ROOK;
        options[2] = W_BISHOP;
        options[3] = W_KNIGHT;
    } else {
        options[0] = B_QUEEN;
        options[1] = B_ROOK;
        options[2] = B_BISHOP;
        options[3] = B_KNIGHT;
    }

    int box_size = 88;
    int gap = 14;
    int total_w = (box_size * 4) + (gap * 3);
    int start_x = (BOARD_SIZE - total_w) / 2;
    int y = (BOARD_SIZE - box_size) / 2;

    DrawText("Choose promotion", start_x, y - 34, 28, RAYWHITE);

    int mx = GetMouseX();
    int my = GetMouseY();
    int click = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    for (int i = 0; i < 4; i++) {
        int x = start_x + i * (box_size + gap);
        Rectangle r = (Rectangle){(float)x, (float)y, (float)box_size, (float)box_size};
        int hover = CheckCollisionPointRec((Vector2){(float)mx, (float)my}, r);

        DrawRectangleRec(r, hover ? (Color){230, 230, 230, 255} : (Color){200, 200, 200, 255});
        DrawRectangleLinesEx(r, 2.0f, (Color){35, 35, 35, 255});
        Texture2D tex = game->textures[options[i]];
        draw_piece(tex, x + 4, y + 3);

        if (click && hover) {
            int ok = promote_pawn(board, options[i]);
            game->promotion_active = 0;
            game->promotion_from_square = -1;
            game->promotion_to_square = -1;
            game->promotion_pawn_type = EMPTY;

            if (ok) {
                game->current_turn = 1 - game->current_turn;
                if (game->current_turn == AI_TURN) {
                    game->ai_move_pending = 1;
                    game->ai_move_delay_frames = 1;
                }
                evaluate_game_state(game, board);
            }
            return;
        }
    }
}