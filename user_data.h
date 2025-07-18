#ifndef USER_DATA_H
#define USER_DATA_H

#include <linux/atomic.h>
#include <linux/kfifo.h>
#include <linux/list.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include "negamax.h"

#include "game.h"
#include "kxo_namespace.h"
#include "lock_free_list.h"
#include "type.h"

UserData *init_user_data(ai_func_t ai1_func,
                         ai_func_t ai2_func,
                         TidData *tid_data);


static void release_user_data(UserData **user_data)
{
    kfifo_free(&(*user_data)->user_fifo);
    smp_mb();
    vfree(*user_data);
    *user_data = NULL;
};

static ai_func_t get_turn_function(UserData *user_data)
{
    char turn = READ_ONCE(user_data->turn);
    smp_mb();
    if (turn == 'O')
        return user_data->ai1_func;
    else
        return user_data->ai2_func;
}

static void reset_user_data_table(UserData *user_data)
{
    memset(user_data->table, ' ', N_GRIDS);
    user_data->turn = 'O';
}

#endif