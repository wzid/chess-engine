#ifndef ENGINE_H
#define ENGINE_H

#include "board.h"

typedef struct {
    int depth;
} Engine;


Engine init_engine();
int eval_board(Board *board, int is_black);
int engine_move(Engine *engine, Board *board, int is_black);

#endif