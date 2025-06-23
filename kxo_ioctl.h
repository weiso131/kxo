#ifndef KXO_IOCTL_H
#define KXO_IOCTL_H

enum IOCTL_TYPE { GET_USER_ID };

enum PLAYER_PERMISSION { USER_CTL = 0, MCTS = 1, NEGAMAX = 2 };

#define get_user_id(user_id, player1, player2)   \
    ({                                           \
        user_id = player1 << 4 | player2;        \
        ioctl(device_fd, GET_USER_ID, &user_id); \
    })

#endif