#pragma once

typedef struct {
    int score, move;
} move_t;

typedef struct {
    int history_score_sum[N_GRIDS];
    int history_count[N_GRIDS];
    u64 hash_value;
} negamax_context_t;

void negamax_init(void);
move_t negamax_predict(char *table, char player);
