#pragma once
#include "game.h"
typedef struct {
    int score, move;
} move_t;

typedef struct {
    int history_score_sum[N_GRIDS];
    int history_count[N_GRIDS];
    u64 hash_value;
    u64 zobrist_table[N_GRIDS][2];
} negamax_context_t;

void negamax_init(void);
move_t negamax_predict(char *table, char player);