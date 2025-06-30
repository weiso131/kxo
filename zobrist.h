#pragma once

#include <linux/list.h>
#include "negamax.h"

#include "game.h"

#define HASH_TABLE_SIZE (100003)

// extern u64 zobrist_table[N_GRIDS][2];

typedef struct {
    u64 key;
    int score;
    int move;
    struct hlist_node ht_list;
} zobrist_entry_t;

void zobrist_init(negamax_context_t *ctx);
zobrist_entry_t *zobrist_get(negamax_context_t *ctx, u64 key);
void zobrist_put(negamax_context_t *ctx, u64 key, int score, int move);
void zobrist_clear(negamax_context_t *ctx);