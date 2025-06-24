#include "history.h"

struct history *current = NULL, *head = NULL;

void history_init(void)
{
    struct history *new = malloc(sizeof(struct history));
    head = new;

    INIT_HISTORY(new, 0);
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
        head = head->next;
    }

    INIT_HISTORY(new, ++(current->index));
    current->next = new;
    current = current->next;

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

void history_release(void)
{
    current = head;
    while (current) {
        print_history(current->value, current->count);
        printf("\n");
        struct history *temp = current;
        current = current->next;
        free(temp);
    }
}
