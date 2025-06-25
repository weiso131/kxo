#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "game.h"
#include "history.h"
#include "kxo_ioctl.h"
#include "userspace.h"

#define XO_STATUS_FILE "/sys/module/kxo/initstate"
#define XO_DEVICE_FILE "/dev/kxo"

static bool status_check(void)
{
    FILE *fp = fopen(XO_STATUS_FILE, "r");
    if (!fp) {
        printf("kxo status : not loaded\n");
        return false;
    }

    char read_buf[20];
    fgets(read_buf, 20, fp);
    read_buf[strcspn(read_buf, "\n")] = 0;
    if (strcmp("live", read_buf)) {
        printf("kxo status : %s\n", read_buf);
        fclose(fp);
        return false;
    }
    fclose(fp);
    return true;
}

static struct termios orig_termios;

static void raw_mode_disable(void)
{
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

static void raw_mode_enable(void)
{
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(raw_mode_disable);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~IXON;
    raw.c_lflag &= ~(ECHO | ICANON);
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static bool read_attr, end_attr;

Userspace *userspace_data = NULL;

static void listen_keyboard_handler(void)
{
    char input;
    if (read(STDIN_FILENO, &input, 1) == 1) {
        switch (input) {
        case 16: /* Ctrl-P */

            read_attr ^= 1;
            if (!read_attr)
                printf("\n\nStopping to display the chess board...\n");
            break;
        case 17: /* Ctrl-Q */
            read_attr = false;
            end_attr = true;
            printf("\n\nStopping the kernel space tic-tac-toe game...\n");
            history_release(&userspace_data->history);
            break;
        }
    }
}


static char *display_board(const char table)
{
    history_update(&userspace_data->history, table);

    update_board(&userspace_data->board, table);

    if (read_attr) {
        int k = 0;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < (BOARD_SIZE << 1) - 1 && k < N_GRIDS; j++) {
                putchar((j & 1) ? '|'
                                : (((userspace_data->board >> 16) &
                                    (1 << ((j >> 1) + (i << 2))))
                                       ? (((userspace_data->board &
                                            (1 << ((j >> 1) + (i << 2)))) != 0)
                                              ? 'X'
                                              : 'O')
                                       : ' '));
            }
            putchar('\n');
            for (int j = 0; j < (BOARD_SIZE << 1) - 1; j++)
                putchar('-');
            putchar('\n');
        }
    }

    if (table >> 5) {
        userspace_data->board = 0;
        history_new_table(&userspace_data->history);
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
    wrong_input:
        printf("Please input two value for player1 and player2\n");
        printf("Player type:\n");
        printf("USER_CTL: 0\nMCTS: 1\nNEGAMAX: 2\n");
        return 0;
    }
    int player1 = argv[1][0] - '0', player2 = argv[2][0] - '0';
    if (player1 > 2 || player2 > 2)
        goto wrong_input;
    if (!status_check())
        exit(1);

    raw_mode_enable();
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    fd_set readset;
    int device_fd = open(XO_DEVICE_FILE, O_RDWR);
    int max_fd = device_fd > STDIN_FILENO ? device_fd : STDIN_FILENO;
    read_attr = true;
    end_attr = false;

    userspace_data = init_userspace(device_fd, player1, player2);

    while (!end_attr) {
        FD_ZERO(&readset);
        FD_SET(STDIN_FILENO, &readset);
        FD_SET(device_fd, &readset);

        int result = select(max_fd + 1, &readset, NULL, NULL, NULL);
        if (result < 0) {
            printf("Error with select system call\n");
            exit(1);
        }

        if (FD_ISSET(STDIN_FILENO, &readset)) {
            FD_CLR(STDIN_FILENO, &readset);
            listen_keyboard_handler();
        } else if (FD_ISSET(device_fd, &readset)) {
            FD_CLR(device_fd, &readset);
            char display_buf;
            if (get_permission(userspace_data) == USER_CTL) {
                unsigned char move = random_get_move(userspace_data);
                user_control(userspace_data, move, &display_buf);
            } else
                mod_control(userspace_data, &display_buf);

            printf("\033[H\033[J"); /* ASCII escape code to clear the screen */
            display_board(display_buf);
        }
    }

    raw_mode_disable();
    fcntl(STDIN_FILENO, F_SETFL, flags);

    close(device_fd);

    return 0;
}
