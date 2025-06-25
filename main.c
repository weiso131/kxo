/* kxo: A Tic-Tac-Toe Game Engine implemented as Linux kernel module */

#include <linux/cdev.h>
#include <linux/circ_buf.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <linux/workqueue.h>

#include "game.h"
#include "kxo_ioctl.h"
#include "kxo_namespace.h"
#include "mcts.h"
#include "negamax.h"
#include "user_data.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("In-kernel Tic-Tac-Toe game engine");

/* Macro DECLARE_TASKLET_OLD exists for compatibility.
 * See https://lwn.net/Articles/830964/
 */
#ifndef DECLARE_TASKLET_OLD
#define DECLARE_TASKLET_OLD(arg1, arg2) DECLARE_TASKLET(arg1, arg2, 0L)
#endif

#define DEV_NAME "kxo"

#define NR_KMLDRV 1

static int delay = 100; /* time (in ms) to generate an event */

static int negamax_move(const char *table, char player)
{
    char table_copy[16];
    memcpy(table_copy, table, N_GRIDS);
    return negamax_predict(table_copy, player).move;
}


/* Data produced by the simulated device */

/* Timer to simulate a periodic IRQ */
static struct timer_list timer;

/* Character device stuff */
static int major;
static struct class *kxo_class;
static struct cdev kxo_cdev;

static char draw_buffer[DRAWBUFFER_SIZE];

/* We use an additional "faster" circular buffer to quickly store data from
 * interrupt context, before adding them to the kfifo.
 */
static struct circ_buf fast_buf;

/* Clear all data from the circular buffer fast_buf */
static void fast_buf_clear(void)
{
    fast_buf.head = fast_buf.tail = 0;
}

static int finish;

/* Workqueue for asynchronous bottom-half processing */
static struct workqueue_struct *kxo_workqueue;

/* Tasklet handler.
 *
 * NOTE: different tasklets can run concurrently on different processors, but
 * two of the same type of tasklet cannot run simultaneously. Moreover, a
 * tasklet always runs on the same CPU that schedules it.
 */
static void game_tasklet_func(unsigned long __data)
{
    ktime_t tv_start, tv_end;
    s64 nsecs;

    WARN_ON_ONCE(!in_interrupt());
    WARN_ON_ONCE(!in_softirq());

    tv_start = ktime_get();

    user_list_queue_work(kxo_workqueue);

    tv_end = ktime_get();

    nsecs = (s64) ktime_to_ns(ktime_sub(tv_end, tv_start));

    pr_info("kxo: [CPU#%d] %s in_softirq: %llu usec\n", smp_processor_id(),
            __func__, (unsigned long long) nsecs >> 10);
}

/* Tasklet for asynchronous bottom-half processing in softirq context */
static DECLARE_TASKLET_OLD(game_tasklet, game_tasklet_func);

static void ai_game(void)
{
    WARN_ON_ONCE(!irqs_disabled());

    pr_info("kxo: [CPU#%d] doing AI game\n", smp_processor_id());
    pr_info("kxo: [CPU#%d] scheduling tasklet\n", smp_processor_id());
    tasklet_schedule(&game_tasklet);
}

static void timer_handler(struct timer_list *__timer)
{
    ktime_t tv_start, tv_end;
    s64 nsecs;

    pr_info("kxo: [CPU#%d] enter %s\n", smp_processor_id(), __func__);
    /* We are using a kernel timer to simulate a hard-irq, so we must expect
     * to be in softirq context here.
     */
    WARN_ON_ONCE(!in_softirq());

    /* Disable interrupts for this CPU to simulate real interrupt context */
    local_irq_disable();

    tv_start = ktime_get();

    ai_game();
    mod_timer(&timer, jiffies + msecs_to_jiffies(delay));
    tv_end = ktime_get();

    nsecs = (s64) ktime_to_ns(ktime_sub(tv_end, tv_start));

    pr_info("kxo: [CPU#%d] %s in_irq: %llu usec\n", smp_processor_id(),
            __func__, (unsigned long long) nsecs >> 10);

    local_irq_enable();
}

ai_func_t alg_list[] = {NULL, &mcts, &negamax_move};

static long kxo_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    switch (cmd) {
    case GET_USER_ID:
        char data;
        if (copy_from_user(&data, (char __user *) arg, sizeof(data))) {
            ret = EFAULT;
            goto error;
        }
        unsigned char p1 = data >> 4;
        unsigned char p2 = data & 0x0f;

        int user_id = add_user(current->pid, alg_list[p1], alg_list[p2]);
        if (user_id == -1) {
            ret = -ENOMEM;
            goto error;
        }

        data = (unsigned char) user_id;

        if (copy_to_user((unsigned char __user *) arg, &data, sizeof(data))) {
            ret = -EFAULT;
            goto error;
        }

        break;
    default:
        break;
    }

error:
    return ret;
}

static ssize_t kxo_read(struct file *file,
                        char __user *buf,
                        size_t count,
                        loff_t *ppos)
{
    TidData *tid_data = find_tid_data(current->pid);
    pr_info("kxo: tid %d's tid_data_ptr: %p\n", current->pid, tid_data);

    unsigned char user_id;
    if (copy_from_user(&user_id, (char __user *) buf, sizeof(user_id)))
        return -EFAULT;
    UserData *user_data = get_user_data(current->pid, user_id);

    if (!user_data)
        return -EFAULT;
    if (get_turn_function(user_data) == NULL)
        return -EPERM;

    unsigned int read;
    int ret;

    pr_debug("kxo: %s(%p, %zd, %lld)\n", __func__, buf, count, *ppos);

    if (unlikely(!access_ok(buf, count)))
        return -EFAULT;

    do {
        ret = kfifo_to_user(&user_data->user_fifo, buf, count, &read);
        if (unlikely(ret < 0))
            break;
        if (read)
            break;
        if (file->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            break;
        }
        ret = wait_event_interruptible(user_data->rx_wait,
                                       kfifo_len(&user_data->user_fifo));
    } while (ret == 0);

    return ret ? ret : read;
}

static ssize_t kxo_write(struct file *file,
                         const char __user *buff,
                         size_t len,
                         loff_t *off)
{
    TidData *tid_data = find_tid_data(current->pid);
    pr_info("kxo: tid %d's tid_data_ptr: %p\n", current->pid, tid_data);

    short int data;
    if (copy_from_user(&data, buff, 2))
        return -EFAULT;
    unsigned char user_id = data & 0xff;
    unsigned char move = (data >> 8) & 0xf;

    UserData *user_data = get_user_data(current->pid, user_id);

    if (!user_data)
        return -EFAULT;
    if (get_turn_function(user_data) != NULL)
        return -EPERM;
    if (move >= 16 || user_data->table[move] != ' ')
        return -EPERM;
    WRITE_ONCE(user_data->table[move], user_data->turn);

    WRITE_ONCE(move, (user_data->turn == 'X') << 4 | move);

    char win;
    WRITE_ONCE(win, check_win(user_data->table));

    if (win != ' ') {
        move |= 1 << 5;
        move |= (win == 'D') << 6;
        reset_user_data_table(user_data);
    } else
        WRITE_ONCE(user_data->turn, user_data->turn ^ 'O' ^ 'X');

    if (copy_to_user(buff, &move, sizeof(move)))
        return -EFAULT;


    return 0;
}

static atomic_t open_cnt;

static int kxo_open(struct inode *inode, struct file *filp)
{
    int result = add_tid_data(current->pid);
    pr_info("kxo: tid %d open kxo, result: %d\n", current->pid, result);

    pr_debug("kxo: %s\n", __func__);
    if (atomic_inc_return(&open_cnt) == 1)
        mod_timer(&timer, jiffies + msecs_to_jiffies(delay));
    pr_info("openm current cnt: %d\n", atomic_read(&open_cnt));

    return 0;
}

static int kxo_release(struct inode *inode, struct file *filp)
{
    int result = delete_tid_data(current->pid);
    pr_info("kxo: tid %d close kxo, result: %d\n", current->pid, result);
    pr_debug("kxo: %s\n", __func__);
    if (atomic_dec_and_test(&open_cnt)) {
        del_timer_sync(&timer);
        flush_workqueue(kxo_workqueue);
        fast_buf_clear();
    }
    pr_info("release, current cnt: %d\n", atomic_read(&open_cnt));

    return 0;
}

static const struct file_operations kxo_fops = {
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    .owner = THIS_MODULE,
#endif
    .read = kxo_read,
    .write = kxo_write,
    .llseek = no_llseek,
    .open = kxo_open,
    .release = kxo_release,
    .unlocked_ioctl = kxo_ioctl};

static char *kxo_devnode(const struct device *dev, umode_t *mode)
{
    if (mode)
        *mode = 0666;
    return NULL;
}

static int __init kxo_init(void)
{
    dev_t dev_id;
    int ret;

    init_namespace();

    negamax_init();
    mcts_init();

    /* Register major/minor numbers */
    ret = alloc_chrdev_region(&dev_id, 0, NR_KMLDRV, DEV_NAME);
    if (ret)
        goto out;
    major = MAJOR(dev_id);

    /* Add the character device to the system */
    cdev_init(&kxo_cdev, &kxo_fops);
    ret = cdev_add(&kxo_cdev, dev_id, NR_KMLDRV);
    if (ret) {
        kobject_put(&kxo_cdev.kobj);
        goto error_region;
    }

    /* Create a class structure */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
    kxo_class = class_create(THIS_MODULE, DEV_NAME);
#else
    kxo_class = class_create(DEV_NAME);
#endif
    kxo_class->devnode = kxo_devnode;
    if (IS_ERR(kxo_class)) {
        printk(KERN_ERR "error creating kxo class\n");
        ret = PTR_ERR(kxo_class);
        goto error_cdev;
    }

    /* Register the device with sysfs */
    device_create(kxo_class, NULL, MKDEV(major, 0), NULL, DEV_NAME);

    /* Allocate fast circular buffer */
    fast_buf.buf = vmalloc(PAGE_SIZE);
    if (!fast_buf.buf) {
        ret = -ENOMEM;
        goto error_vmalloc;
    }

    /* Create the workqueue */
    kxo_workqueue = alloc_workqueue("kxod", WQ_UNBOUND, WQ_MAX_ACTIVE);
    if (!kxo_workqueue) {
        ret = -ENOMEM;
        goto error_workqueue;
    }

    /* Setup the timer */
    timer_setup(&timer, timer_handler, 0);
    atomic_set(&open_cnt, 0);

    pr_info("kxo: registered new kxo device: %d,%d\n", major, 0);
out:
    return ret;
error_workqueue:
    vfree(fast_buf.buf);
error_vmalloc:
    device_destroy(kxo_class, dev_id);
    class_destroy(kxo_class);
error_cdev:
    cdev_del(&kxo_cdev);
error_region:
    unregister_chrdev_region(dev_id, NR_KMLDRV);
    goto out;
}

static void __exit kxo_exit(void)
{
    dev_t dev_id = MKDEV(major, 0);

    del_timer_sync(&timer);
    tasklet_kill(&game_tasklet);
    flush_workqueue(kxo_workqueue);
    destroy_workqueue(kxo_workqueue);
    vfree(fast_buf.buf);
    device_destroy(kxo_class, dev_id);
    class_destroy(kxo_class);
    cdev_del(&kxo_cdev);
    unregister_chrdev_region(dev_id, NR_KMLDRV);

    release_namespace();
    pr_info("kxo: unloaded\n");
}

module_init(kxo_init);
module_exit(kxo_exit);
