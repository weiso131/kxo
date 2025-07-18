#ifndef KXO_NAMESPACE_H
#define KXO_NAMESPACE_H
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include "type.h"
#include "user_data.h"

#define NAMESPACE_MAX 256
#define USER_MAX 32

static unsigned int hash_function(pid_t input)
{
    return input % NAMESPACE_MAX;
}

void init_namespace(void);

/**
 * add_tid_data - Create a TidData object and insert it into the kxo_namespace
 * hash table.
 *
 * @tid: The thread ID of the currently operating thread.
 *
 * Return:
 *  0  - Object creation succeeded.
 * -1  - Object creation failed.
 */
int add_tid_data(pid_t tid);

/**
 * delete_tid_data - Delete a TidData from the kxo_namespace hash table.
 *
 * @tid: The thread ID of the currently operating thread.
 *
 * Return:
 *  0  - Object creation succeeded.
 * -1  - Object creation failed.
 */
int delete_tid_data(pid_t tid);

// find TidData by tid
TidData *find_tid_data(pid_t tid);

// use when a thread need a new user
int add_user(pid_t tid, ai_func_t ai1_func, ai_func_t ai2_func);

// remove user by id
void remove_user(pid_t tid, int user_id);

void user_list_queue_work(struct workqueue_struct *wq);

UserData *get_user_data(pid_t tid, int user_id);

// only use when rmmod
void release_namespace(void);
#endif