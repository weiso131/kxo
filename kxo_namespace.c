#include <linux/spinlock.h>

#include "kxo_namespace.h"

struct kxo_namespace {
    struct hlist_head head;
    rwlock_t lock;
};

struct kxo_namespace kxo_namespace[NAMESPACE_MAX];
struct lf_list user_list_head;
struct lf_list trash_list_head;


void init_namespace(void)
{
    for (int i = 0; i < NAMESPACE_MAX; i++) {
        INIT_HLIST_HEAD(&kxo_namespace[i].head);
        rwlock_init(&kxo_namespace[i].lock);
    }
    lf_list_init(&user_list_head);
    lf_list_init(&trash_list_head);
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
            WRITE_ONCE(data->user_data_list[i]->unuse, 1);

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
            lf_list_add_head(&user_list_head, &user_data->hlist);
            return i;
        }
    }
    release_user_data(&user_data);

    return -1;
}

UserData *get_user_data(pid_t tid, int user_id)
{
    TidData *tid_data = find_tid_data(tid);
    if (user_id > USER_MAX)
        return NULL;
    UserData *user_data = tid_data->user_data_list[user_id];

    return user_data;
}

void user_list_queue_work(struct workqueue_struct *wq)
{
    struct lf_list *now = NULL, *nxt = NULL, *last = &user_list_head;
    lf_list_for_each_safe(now, nxt, &user_list_head)
    {
        UserData *user_data = container_of(now, UserData, hlist);
        if (!READ_ONCE(user_data->unuse)) {
            if (get_turn_function(user_data))
                queue_work(wq, &user_data->work);
            last = now;
        } else {
            lf_list_remove(last, now, &user_list_head);
            lf_list_add_head(&trash_list_head, now);
        }
    }
}

void release_namespace(void)
{
    struct lf_list *now = NULL, *nxt = NULL, *last = &user_list_head;
    lf_list_for_each_safe(now, nxt, &user_list_head)
    {
        UserData *user_data = container_of(now, UserData, hlist);
        lf_list_remove(last, now, &user_list_head);
        release_user_data(&user_data);
        last = now;
    }
    lf_list_for_each_safe(now, nxt, &trash_list_head)
    {
        UserData *user_data = container_of(now, UserData, hlist);
        lf_list_remove(last, now, &trash_list_head);
        release_user_data(&user_data);
        last = now;
    }
}