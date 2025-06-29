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
    struct hlist_head *hash_table;
} negamax_context_t;

void negamax_init(negamax_context_t *ctx);
move_t negamax_predict(negamax_context_t *ctx, char *table, char player);