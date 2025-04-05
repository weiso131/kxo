#include "history.h"

struct kxo_history *history = NULL, *top = NULL;

struct kxo_history *history_add(void)
{
    struct kxo_history *new = vmalloc(sizeof(struct kxo_history));

    new->count = 0;
    new->index = 0;
    new->next = NULL;
    return new;
}

void history_init(void)
{
    history = history_add();
    top = history;
}

void history_update(uint64_t move)
{
    top->history[top->index] += move << (top->count << 2);
    top->count++;
}

void history_new_table(void)
{
    top->history[top->index] += GET_MOVE(top, top->index, 0)
                                << (top->count << 2);
    top->index++;
    if (top->index < HISTORY_LEN - 1) {
        top->count = 0;
    } else {
        struct kxo_history *new = vmalloc(sizeof(struct kxo_history));
        top->next = new;
        top = new;
    }
}

void history_release(void)
{
    struct kxo_history *tmp = history;

    while (tmp) {
        struct kxo_history *trash = tmp;
        tmp = tmp->next;
        vfree(trash);
    }
    history = top = NULL;
}