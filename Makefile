TARGET = kxo
kxo-objs = main.o kxo_namespace.o user_data.o game.o xoroshiro.o mcts.o negamax.o zobrist.o
obj-m := $(TARGET).o

ccflags-y := -std=gnu99 -Wno-declaration-after-statement
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

GIT_HOOKS := .git/hooks/applied
all: kmod xo-user xo-train

kmod: $(GIT_HOOKS) main.c
	$(MAKE) -C $(KDIR) M=$(PWD) modules

xo-user: xo-user.c history.c rl/reinforcement_learning.c
	$(CC) $(ccflags-y) -o $@ $^

xo-train: xo-train.c history.c rl/train.c rl/reinforcement_learning.c
	$(CC) $(ccflags-y) -o $@ $^
$(GIT_HOOKS):
	@scripts/install-git-hooks
	@echo


clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(RM) xo-user
