#pragma once

#define BOARD_SIZE 4
#define GOAL 3
#define ALLOW_EXCEED 1
#define N_GRIDS (BOARD_SIZE * BOARD_SIZE)
#define GET_INDEX(i, j) ((i) * (BOARD_SIZE) + (j))

#define for_each_empty_grid(i, board) \
    for (int i = 0; i < N_GRIDS; i++) \
        if (!((board >> (16 + i)) & 1))

static double calculate_win_value(char win, char player)
{
    if (win == player)
        return 1.0;
    if (win == (player ^ 'O' ^ 'X'))
        return 0.0;
    return 0.5;
}
