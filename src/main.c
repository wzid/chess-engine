#include "board.h"
#include <raylib.h>
#include <stdint.h>
#include "game.h"

int main() {
    Board board = init_board();
    Game game = init_game();
    game_loop(&game, &board);

    return 0;
}