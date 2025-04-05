#include <linux/types.h>
#include <linux/vmalloc.h>

#define HISTORY_LEN 128
#define GET_MOVE(current, idx, i) ((current->history[idx] >> i) & 0x0f)

#define malloc malloc
#define free free

static char table_form[][3] = {"A1", "A2", "A3", "A4", "B1", "B2", "B3", "B4",
                               "C1", "C2", "C3", "C4", "D1", "D2", "D3", "D4"};

struct kxo_history {
    uint64_t history[HISTORY_LEN];
    int count;
    int index;
    struct kxo_history *next;
};

extern struct kxo_history *history, *top;

struct kxo_history *history_add(void);

void history_init(void);

void history_update(uint64_t move);

void history_new_table(void);

void history_release(void);