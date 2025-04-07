#include <linux/sched.h> /* For current */
#include <linux/tty.h>   /* For the tty declarations */
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "kxo_ioctl.h"

#define HISTORY_LEN 128


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

