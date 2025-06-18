#include "history.h"

struct history *current = NULL;

void history_init(void)
{
    struct history *new = malloc(sizeof(struct history));

    INIT_HISTORY(new, 0, new);
    current = new;

    return;
}

void history_update(int move)
{
    current->value += ((move & 15) << (current->count << 2));
    current->count++;

    return;
}

void history_new_table(void)
{
    struct history *new = malloc(sizeof(struct history));

    if (current->index >= HISTORY_MAX) {
        struct history *temp = current->head;
        current->head = current->head->next;
        free(temp);
    }

    INIT_HISTORY(new, ++current->index, current->head);
    current->next = new;
    current = current->next;

    return;
}

void print_history(__int64_t history, int count)
{
    for (int i = 0; i < count; i++) {
        printf("%lx%ld->", ((history >> 2) & 3) + 10, (history & 3) + 1);
        history = (history >> 4);
    }
}

void history_release(void)
{
    current = current->head;
    while (current) {
        print_history(current->value, current->count);
        printf("\n");
        struct history *temp = current;
        current = current->next;
        free(temp);
    }
}