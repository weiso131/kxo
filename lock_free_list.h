#ifndef LOCK_FREE_LIST_H
#define LOCK_FREE_LIST

#include <asm/cmpxchg.h>
#include <linux/compiler.h>

struct lf_list {
    struct lf_list *next;
};

static inline void lf_list_init(struct lf_list *head)
{
    head->next = NULL;
}

static inline void lf_list_add_head(struct lf_list *head, struct lf_list *node)
{
    while (1) {
        struct lf_list *next = head->next;
        WRITE_ONCE(node->next, next);

        if (cmpxchg(&head->next, next, node) == next)
            break;
    }
}

static inline struct lf_list *lf_list_remove(struct lf_list *last,
                                             struct lf_list *node,
                                             struct lf_list *head)
{
    if (last != head)
        last->next = node->next;
    else if (cmpxchg(&head->next, node, node->next) != node)
        return NULL;
    return node;
}

#define lf_list_for_each_safe(now, nxt, head)                \
    for (now = READ_ONCE((head)->next); now && ({            \
                                            nxt = now->next; \
                                            1;               \
                                        });                  \
         now = nxt)

#endif