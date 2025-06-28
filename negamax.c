#include <linux/slab.h>
#include <linux/sort.h>
#include <linux/string.h>

#include "game.h"
#include "negamax.h"
#include "util.h"
#include "zobrist.h"

#define MAX_SEARCH_DEPTH 6

static negamax_context_t *current_ctx = NULL;
static DEFINE_SPINLOCK(negamax_lock);

static int cmp_moves(const void *a, const void *b)
{
    const int *_a = (int *) a, *_b = (int *) b;
    int score_a = 0, score_b = 0;

    if (current_ctx->history_count[*_a])
        score_a = current_ctx->history_score_sum[*_a] /
                  current_ctx->history_count[*_a];
    if (current_ctx->history_count[*_b])
        score_b = current_ctx->history_score_sum[*_b] /
                  current_ctx->history_count[*_b];
    return score_b - score_a;
}

static move_t negamax(negamax_context_t *ctx,
                      char *table,
                      int depth,
                      char player,
                      int alpha,
                      int beta)
{
    if (check_win(table) != ' ' || depth == 0) {
        move_t result = {get_score(table, player), -1};
        return result;
    }
    const zobrist_entry_t *entry = zobrist_get(ctx->hash_value);
    if (entry)
        return (move_t){.score = entry->score, .move = entry->move};

    int score;
    move_t best_move = {-10000, -1};
    int *moves = available_moves(table);
    int n_moves = 0;
    while (n_moves < N_GRIDS && moves[n_moves] != -1)
        ++n_moves;

    spin_lock(&negamax_lock);
    current_ctx = ctx;
    sort(moves, n_moves, sizeof(int), cmp_moves, NULL);
    current_ctx = NULL;
    spin_unlock(&negamax_lock);

    for (int i = 0; i < n_moves; i++) {
        table[moves[i]] = player;
        ctx->hash_value ^= zobrist_table[moves[i]][player == 'X'];
        if (!i)
            score = -negamax(ctx, table, depth - 1, player == 'X' ? 'O' : 'X',
                             -beta, -alpha)
                         .score;
        else {
            score = -negamax(ctx, table, depth - 1, player == 'X' ? 'O' : 'X',
                             -alpha - 1, -alpha)
                         .score;
            if (alpha < score && score < beta)
                score = -negamax(ctx, table, depth - 1,
                                 player == 'X' ? 'O' : 'X', -beta, -score)
                             .score;
        }
        ctx->history_count[moves[i]]++;
        ctx->history_score_sum[moves[i]] += score;
        if (score > best_move.score) {
            best_move.score = score;
            best_move.move = moves[i];
        }
        table[moves[i]] = ' ';
        ctx->hash_value ^= zobrist_table[moves[i]][player == 'X'];
        if (score > alpha)
            alpha = score;
        if (alpha >= beta)
            break;
    }

    kfree((char *) moves);
    zobrist_put(ctx->hash_value, best_move.score, best_move.move);
    return best_move;
}

void negamax_init(void)
{
    zobrist_init();
}

move_t negamax_predict(char *table, char player)
{
    negamax_context_t ctx;
    memset(&ctx, 0, sizeof(ctx));
    move_t result;
    for (int depth = 2; depth <= MAX_SEARCH_DEPTH; depth += 2) {
        result = negamax(&ctx, table, depth, player, -100000, 100000);
        zobrist_clear();
    }
    return result;
}
