#include <linux/spinlock.h>

#include "kxo_namespace.h"

struct kxo_namespace {
    struct hlist_head head;
    rwlock_t lock;
};

struct kxo_namespace kxo_namespace[NAMESPACE_MAX];
struct hlist_head user_list_head;
struct hlist_head trash_list_head;

static DEFINE_SPINLOCK(user_list_lock);
unsigned long user_list_flags;

void init_namespace(void)
{
    for (int i = 0; i < NAMESPACE_MAX; i++) {
        INIT_HLIST_HEAD(&kxo_namespace[i].head);
        rwlock_init(&kxo_namespace[i].lock);
    }
    INIT_HLIST_HEAD(&user_list_head);
    INIT_HLIST_HEAD(&trash_list_head);
}

int add_tid_data(pid_t tid)
{
    TidData *data = vmalloc(sizeof(TidData));
    if (!data)
        goto data_fail;
    data->user_data_list = (UserData **) vmalloc(sizeof(UserData *) * USER_MAX);
    for (int i = 0; i < USER_MAX; i++)
        data->user_data_list[i] = NULL;

    if (!data->user_data_list)
        goto user_list_fail;

    data->tid = tid;
    unsigned int nid = hash_function(tid);
    write_lock(&kxo_namespace[nid].lock);
    hlist_add_head(&data->hlist, &kxo_namespace[nid].head);
    write_unlock(&kxo_namespace[nid].lock);

    return 0;

user_list_fail:
    vfree(data);
data_fail:
    return -1;
}

int delete_tid_data(pid_t tid)
{
    TidData *data = find_tid_data(tid);
    if (!data)
        return -1;

    unsigned int nid = hash_function(tid);
    write_lock(&kxo_namespace[nid].lock);
    hlist_del(&data->hlist);
    write_unlock(&kxo_namespace[nid].lock);

    for (int i = 0; i < USER_MAX; i++)
        if (data->user_data_list[i] != NULL)
            atomic_set(&data->user_data_list[i]->unuse, 1);

    vfree(data);

    return 0;
}

TidData *find_tid_data(pid_t tid)
{
    unsigned int nid = hash_function(tid);
    TidData *pos = NULL;
    read_lock(&kxo_namespace[nid].lock);
    hlist_for_each_entry(pos, &kxo_namespace[nid].head, hlist) {
        if (pos->tid == tid)
            break;
    }
    read_unlock(&kxo_namespace[nid].lock);

    return pos;
}

int add_user(pid_t tid, ai_func_t ai1_func, ai_func_t ai2_func)
{
    TidData *tid_data = find_tid_data(tid);
    UserData *user_data = init_user_data(ai1_func, ai2_func);
    pr_info("kxo: real user_data: %p\n", user_data);
    if (!user_data)
        return -1;
    for (int i = 0; i < USER_MAX; i++) {
        if (cmpxchg((tid_data->user_data_list + i), NULL, user_data) == NULL) {
            spin_lock_irqsave(&user_list_lock, user_list_flags);
            hlist_add_head(&user_data->hlist, &user_list_head);
            spin_unlock_irqrestore(&user_list_lock, user_list_flags);
            return i;
        }
    }
    release_user_data(&user_data);

    return -1;
}

UserData *get_user_data(pid_t tid, int user_id)
{
    TidData *tid_data = find_tid_data(tid);
    UserData *user_data = tid_data->user_data_list[user_id];

    return user_data;
}

void user_list_queue_work(struct workqueue_struct *wq)
{
    spin_lock_irqsave(&user_list_lock, user_list_flags);
    UserData *user_data =
        hlist_entry_safe(user_list_head.first, UserData, hlist);
    spin_unlock_irqrestore(&user_list_lock, user_list_flags);

    while (1) {
        pr_info("kxo: user_data_ptr: %p\n", user_data);
        if (!user_data)
            break;
        struct hlist_node *n = user_data->hlist.next;

        if (!atomic_read(&user_data->unuse))
            queue_work(wq, &user_data->work);
        else {
            hlist_del(&user_data->hlist);
            hlist_add_head(&user_data->hlist, &trash_list_head);
        }
        user_data = hlist_entry_safe(n, UserData, hlist);
    }
}

void release_namespace(void)
{
    UserData *pos = NULL;
    struct hlist_node *n;
    hlist_for_each_entry_safe(pos, n, &user_list_head, hlist)
    {
        hlist_del(&pos->hlist);
        release_user_data(&pos);
    }
    hlist_for_each_entry_safe(pos, n, &trash_list_head, hlist)
    {
        hlist_del(&pos->hlist);
        release_user_data(&pos);
    }
}