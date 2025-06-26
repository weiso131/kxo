#pragma once

#include <stdint.h>

#include "../userspace.h"
#define MODEL_NAME "state_value.bin"

#define CALC_STATE_NUM(x)                 \
    {                                     \
        x = 1;                            \
        for (int i = 0; i < N_GRIDS; i++) \
            x *= 3;                       \
    }

typedef struct td_agent {
    char player;
    float *state_value;
} rl_agent_t;

int board_to_hash(uint32_t board);
char *hash_to_table(int hash);
void load_model(rl_agent_t *agent,
                unsigned int state_num,
                const char *model_path);
int get_action_exploit(const Userspace *us_data, const rl_agent_t *agent);
void store_state_value(const rl_agent_t *agent, unsigned int N_STATES);
void init_rl_agent(rl_agent_t *agent, unsigned int state_num, char player);
