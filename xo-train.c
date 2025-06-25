#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "history.h"
#include "kxo_ioctl.h"
#include "rl/train.h"

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

Userspace *userspace_data = NULL;

int main(int argc, char *argv[])
{
    if (!status_check())
        exit(1);

    int device_fd = open(XO_DEVICE_FILE, O_RDWR);

    userspace_data = init_userspace(device_fd, USER_CTL, USER_CTL);

    printf("init training\n");
    init_training();
    printf("start train\n");
    train(userspace_data, NUM_EPISODE, 1);
    release_training();
    close(device_fd);

    return 0;
}
