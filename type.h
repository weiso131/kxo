#ifndef TYPE_H
#define TYPE_H
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "lock_free_list.h"

typedef int (*ai_func_t)(const char *table, char turn);

typedef struct user_data UserData;

typedef struct tid_data {
    pid_t tid;
    UserData **user_data_list;
    struct hlist_node hlist;
    struct wait_queue_head tid_wait;
    unsigned char user_cnt;
} TidData;

typedef struct user_data {
    char table[16];
    char turn;           //'O' or 'X'
    char unuse;          // tasklet function will release user_data if unuse
    ai_func_t ai1_func;  //'O', if NULL mean user space control
    ai_func_t ai2_func;  //'X', if NULL mean user space control

    struct work_struct work;

    DECLARE_KFIFO_PTR(user_fifo, unsigned char);

    TidData *tid_data;

    struct lf_list hlist;
} UserData;

#endif