#include <linux/slab.h>

#include "zobrist.h"

#define HASH(key) ((key) % HASH_TABLE_SIZE)

static struct hlist_head *hash_table;

/* See https://github.com/wangyi-fudan/wyhash
 */
static inline u64 wyhash64_stateless(u64 *seed)
{
    *seed += 0x60bee2bee120fc15;
    u128 tmp;
    tmp = (u128) *seed * 0xa3b195354a39b70d;
    u64 m1 = (tmp >> 64) ^ tmp;
    tmp = (u128) m1 * 0x1b03738712fad5c9;
    u64 m2 = (tmp >> 64) ^ tmp;
    return m2;
}

static u64 wyhash64(void)
{
    u64 seed = (u64) ktime_to_ns(ktime_get());
    return wyhash64_stateless(&seed);
}

void zobrist_init(negamax_context_t *ctx)
{
    int i;
    for (i = 0; i < N_GRIDS; i++) {
        ctx->zobrist_table[i][0] = wyhash64();
        ctx->zobrist_table[i][1] = wyhash64();
    }
    hash_table =
        kmalloc(sizeof(struct hlist_head) * HASH_TABLE_SIZE, GFP_KERNEL);
    if (!hash_table) {
        pr_info("kxo: Failed to allocate space for hash_table\n");
        return;
    }
    for (i = 0; i < HASH_TABLE_SIZE; i++)
        INIT_HLIST_HEAD(&hash_table[i]);
}

zobrist_entry_t *zobrist_get(u64 key)
{
    unsigned long long hash_key = HASH(key);

    if (hlist_empty(&hash_table[hash_key]))
        return NULL;

    zobrist_entry_t *entry = NULL;

    hlist_for_each_entry(entry, &hash_table[hash_key], ht_list) {
        if (entry->key == key)
            return entry;
    }
    return NULL;
}

void zobrist_put(u64 key, int score, int move)
{
    unsigned long long hash_key = HASH(key);
    zobrist_entry_t *new_entry = kmalloc(sizeof(zobrist_entry_t), GFP_KERNEL);
    new_entry->key = key;
    new_entry->move = move;
    new_entry->score = score;
    hlist_add_head(&new_entry->ht_list, &hash_table[hash_key]);
}

void zobrist_clear(void)
{
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        while (!hlist_empty(&hash_table[i])) {
            zobrist_entry_t *entry =
                hlist_entry(hash_table[i].first, zobrist_entry_t, ht_list);
            hlist_del(&entry->ht_list);
            kfree(entry);
        }
        INIT_HLIST_HEAD(&hash_table[i]);
    }
}