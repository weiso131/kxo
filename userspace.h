#ifndef USERSPACE_H
#define USERSPACE_H
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "history.h"
#include "kxo_ioctl.h"
typedef struct userspace {
    int device_fd;
    uint32_t board;
    unsigned char user_id;
    PlayerPermission player1, player2;
    char turn;
    History history;
} Userspace;


static Userspace *init_userspace(int device_fd,
                                 PlayerPermission player1,
                                 PlayerPermission player2)
{
    Userspace *new_data = (Userspace *) malloc(sizeof(Userspace));
    if (!new_data)
        goto malloc_fail;
    if (get_user_id(device_fd, new_data->user_id, (player1 & 15),
                    (player2 & 15)) < 0)
        goto get_user_id_fail;
    new_data->device_fd = device_fd;
    new_data->board = 0;
    new_data->player1 = player1;
    new_data->player2 = player2;
    new_data->turn = 'O';
    history_init(&new_data->history);

    return new_data;
get_user_id_fail:
    free(new_data);
malloc_fail:
    return NULL;
}

static inline void update_board(uint32_t *board, const char move)
{
    *board = (*board | ((1 << (move & 15)) << 16));
    if ((move >> 4) & 1)
        *board |= (1 << (move & 0xF));
}

static inline PlayerPermission get_permission(Userspace *us_data)
{
    if (us_data->turn == 'O')
        return us_data->player1;
    else
        return us_data->player2;
}

static inline int user_control(Userspace *us_data,
                               const unsigned char move,
                               char *display_buf)
{
    uint16_t data = us_data->user_id | (((uint16_t) move) << 8);
    int fd_result = write(us_data->device_fd, &data, 2);
    if (fd_result < 0)
        return fd_result;
    *display_buf = data & 0xff;
    us_data->turn = us_data->turn ^ 'O' ^ 'X';
    if ((*display_buf) >> 5)
        us_data->turn = 'O';
    return 0;
}

static inline int mod_control(Userspace *us_data, char *display_buf)
{
    *display_buf = us_data->user_id;
    int fd_result = read(us_data->device_fd, display_buf, 1);
    if (fd_result < 0)
        return fd_result;
    us_data->turn = us_data->turn ^ 'O' ^ 'X';
    if ((*display_buf) >> 5)
        us_data->turn = 'O';
    return 0;
}

static unsigned char random_get_move(const Userspace *data)
{
    __uint16_t available = data->board >> 16;
    int available_cnt = 0;
    int available_move[16];
    for (int i = 0; i < 16; i++) {
        if (!((available >> i) & 1)) {
            available_move[available_cnt] = i;
            available_cnt++;
        }
    }

    unsigned char move = available_move[rand() % available_cnt];

    return move;
}

#endif