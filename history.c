#include "history.h"

void history_init(History *history)
{
    struct history_node *new = malloc(sizeof(struct history_node));
    INIT_HISTORY(new, 0);
    history->head = new;
    history->tail = new;
    return;
}

void history_update(History *history, int move)
{
    history->tail->value += ((move & 15) << (history->tail->count << 2));
    history->tail->count++;

    return;
}

void history_new_table(History *history)
{
    struct history_node *new = malloc(sizeof(struct history_node));

    if (history->tail->index >= HISTORY_MAX) {
        history->head = history->head->next;
    }

    INIT_HISTORY(new, ++(history->tail->index));
    history->tail->next = new;
    history->tail = history->tail->next;

    return;
}

void print_history(__int64_t history, int count)
{
    for (int i = 0; i < count; i++) {
        printf("%lx%ld", ((history >> 2) & 3) + 10, (history & 3) + 1);
        history = (history >> 4);
        if (i == count - 1)
            break;
        printf("->");
    }
}

void history_release(History *history)
{
    struct history_node *current = history->head;
    while (current) {
        print_history(current->value, current->count);
        printf("\n");
        struct history_node *temp = current;
        current = current->next;
        free(temp);
    }
}
