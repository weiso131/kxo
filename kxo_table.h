#ifndef KXO_TABLE_H
#define KXO_TABLE_H

#include <linux/types.h>

#include "game.h"

struct table {
    uint64_t player1;
    uint64_t player2;
};

#define TABLE_MASK 0xFFFF

#define GET_TABLE(table) \
    ((table.player1 & TABLE_MASK) << 16 | (table.player2 & TABLE_MASK))

#define GET_AVAILABLE(table) \
    (~((table.player1 & TABLE_MASK) | (table.player2 & TABLE_MASK)))

uint64_t MOVE_MASK[16] = {
    0x1000100010001,    0x4000401010002,    0x100001001010004,
    0x400004001000008,  0x10010100040010,   0x141040404040020,
    0x1404101004040040, 0x4000404004000080, 0x100010100100100,
    0x1410040410100200, 0x4041101010100400, 0x4404010000800,
    0x1000010000401000, 0x4000040040402000, 0x10100040404000,
    0x40400040008000,
};

#define CHECK_WIN(player) \
    ((player) - 0x555555555555ULL) & (~player) & 0xAAAAAAAAAAAAULL

static char is_win(const struct table *table)
{
    if (CHECK_WIN(table->player1 >> 16))
        return 'O';
    else if (CHECK_WIN(table->player2 >> 16))
        return 'X';
    return ' ';
}


#define PLAYER_MOVE(player, move) player += MOVE_MASK[move]


#endif
