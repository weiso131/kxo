#include <linux/spinlock.h>

#include "kxo_namespace.h"

struct kxo_namespace {
    struct hlist_head head;
    rwlock_t lock;
};

struct kxo_namespace kxo_namespace[NAMESPACE_MAX];

void init_namespace(void)
{
    for (int i = 0; i < NAMESPACE_MAX; i++) {
        INIT_HLIST_HEAD(&kxo_namespace[i].head);
        rwlock_init(&kxo_namespace[i].lock);
    }
}

int add_tid_data(pid_t tid)
{
    TidData *data = vmalloc(sizeof(TidData));
    if (!data)
        goto data_fail;
    data->user_data_list = (UserData **) vmalloc(sizeof(UserData *) * USER_MAX);
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
