#include <fcntl.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>

#include "game.h"
#include "history.h"

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
            history_release();
            break;
        }
    }
}

__uint32_t board = 0;

static char *display_board(const char table)
{
    history_update(table);

    board = (board | ((1 << (table & 15)) << 16));
    if ((table >> 4) & 1)
        board |= (1 << (table & 0xF));

    if (read_attr) {
        int k = 0;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < (BOARD_SIZE << 1) - 1 && k < N_GRIDS; j++) {
                putchar(
                    (j & 1)
                        ? '|'
                        : (((board >> 16) & (1 << ((j >> 1) + (i << 2))))
                               ? (((board & (1 << ((j >> 1) + (i << 2)))) != 0)
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
        board = 0;
        history_new_table();
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (!status_check())
        exit(1);

    raw_mode_enable();
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);

    char display_buf;

    fd_set readset;
    int device_fd = open(XO_DEVICE_FILE, O_RDONLY);
    int max_fd = device_fd > STDIN_FILENO ? device_fd : STDIN_FILENO;
    read_attr = true;
    end_attr = false;

    history_init();

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
            printf("\033[H\033[J"); /* ASCII escape code to clear the screen */
            read(device_fd, &display_buf, 1);
            display_board(display_buf);
        }
    }

    raw_mode_disable();
    fcntl(STDIN_FILENO, F_SETFL, flags);

    close(device_fd);

    return 0;
}
