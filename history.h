#define HISTORY_MAX 128
#include <stdio.h>
#include <stdlib.h>
struct history {
    __uint64_t value;
    int count;
    int index;
    struct history *next;
    struct history *head;
};

static inline void INIT_HISTORY(struct history *init,
                                int index,
                                struct history *head)
{
    init->index = index;
    init->count = 0;
    init->head = head;
    init->next = NULL;
    init->value = 0;
}

void history_init(void);

void history_update(int move);

void history_new_table(void);

void history_release(void);