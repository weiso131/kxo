#ifndef HISTORY_H
#define HISTORY_H
#define HISTORY_MAX 128
#include <stdio.h>
#include <stdlib.h>


struct history_node {
    __uint64_t value;
    int count;
    int index;
    struct history_node *next;
};

typedef struct history {
    struct history_node *tail, *head;
} History;

static inline void INIT_HISTORY(struct history_node *init, int index)
{
    init->index = index;
    init->count = 0;
    init->next = NULL;
    init->value = 0;
}

void history_init(History *history);

void history_update(History *history, int move);

void history_new_table(History *history);

void history_release(History *history);
#endif