#include "user_data.h"

static void produce_board(UserData *user_data, int move, char is_win)
{
    char buffer;
    WRITE_ONCE(buffer, (char) move +
                           (((user_data->turn ^ 'O' ^ 'X') == 'X') << 4) +
                           (is_win << 5));
    smp_mb();
    unsigned int len = kfifo_in(
        &user_data->user_fifo, (const unsigned char *) &buffer, sizeof(buffer));
    if (unlikely(len < sizeof(buffer)))
        pr_warn_ratelimited("%s: %zu bytes dropped\n", __func__,
                            sizeof(buffer) - len);

    pr_info("kxo: %s: in %u/%u bytes\n", __func__, len,
            kfifo_len(&user_data->user_fifo));
}
static void ai_work_func(struct work_struct *w)
{
    WARN_ON_ONCE(in_softirq());
    WARN_ON_ONCE(in_interrupt());

    int cpu;
    cpu = get_cpu();
    pr_info("kxo: [CPU#%d] start doing %s\n", cpu, __func__);

    ktime_t tv_start, tv_end;
    s64 nsecs;
    tv_start = ktime_get();

    UserData *user_data = container_of(w, UserData, work);
    smp_mb();
    ai_func_t ai_func = get_turn_function(user_data);
    smp_mb();

    if (ai_func == NULL)
        goto null_func;

    smp_mb();

    int move;
    WRITE_ONCE(move, ai_func(user_data->table, user_data->turn));
    smp_mb();

    if (move != -1)
        WRITE_ONCE(user_data->table[move], user_data->turn);

    WRITE_ONCE(user_data->turn, user_data->turn ^ 'O' ^ 'X');
    smp_wmb();

    char win;
    WRITE_ONCE(win, check_win(user_data->table));
    smp_mb();

    produce_board(user_data, move, win != ' ');

    smp_mb();

    wake_up_interruptible(&user_data->rx_wait);

    if (win != ' ')
        reset_user_data_table(user_data);

null_func:
    put_cpu();
    tv_end = ktime_get();
    nsecs = (s64) ktime_to_ns(ktime_sub(tv_end, tv_start));
    pr_info("kxo: [CPU#%d] %s completed in %llu usec\n", cpu, __func__,
            (unsigned long long) nsecs >> 10);
}

UserData *init_user_data(ai_func_t ai1_func, ai_func_t ai2_func)
{
    UserData *user_data = vmalloc(sizeof(UserData));

    if (!user_data)
        goto user_data_alloc_fail;

    reset_user_data_table(user_data);
    atomic_set(&user_data->unuse, 0);
    user_data->ai1_func = ai1_func;
    user_data->ai2_func = ai2_func;

    INIT_WORK(&user_data->work, ai_work_func);

    init_waitqueue_head(&user_data->rx_wait);

    if (kfifo_alloc(&user_data->user_fifo, PAGE_SIZE, GFP_KERNEL) < 0)
        goto kfifo_alloc_fail;

    return user_data;

kfifo_alloc_fail:
    vfree(user_data);
user_data_alloc_fail:
    return NULL;
}
