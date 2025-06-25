#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <float.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "game_user.h"
#include "reinforcement_learning.h"
#include "train.h"
#include "util.h"

#define RAND_UNIFORM ((float) rand() / (float) RAND_MAX)

#if EPSILON_GREEDY
static double epsilon = EPSILON_START;
static double decay_factor;
#endif

static unsigned int N_STATES = 1;
static rl_agent_t agent[2];

_Static_assert(INITIAL_MUTIPLIER >= 0,
               "Initial mutiplier must not be less than 0");
_Static_assert(LEARNING_RATE > 0, "Learning rate must be greater than 0");
_Static_assert(NUM_EPISODE > 0,
               "The number of episodes must be greater than 0.");
_Static_assert(EPSILON_GREEDY == 0 || EPSILON_GREEDY == 1,
               "EPSILON_GREEDY must be a boolean that is 0 or 1");
_Static_assert(MONTE_CARLO == 0 || MONTE_CARLO == 1,
               "MONTE_CARLO must be a boolean that is 0 or 1");
_Static_assert(GAMMA >= 0 && GAMMA < 1,
               "Gamma must be within the range [0, 1)");
_Static_assert(REWARD_TRADEOFF >= 0 && REWARD_TRADEOFF <= 1,
               "REWARD_TRADEOFF must be within the range [0, 1]");
_Static_assert(EPSILON_END >= 0, "EPSILON_END must not be less than 0");
_Static_assert(EPSILON_START >= EPSILON_END,
               "EPSILON_START must not be less than EPSILON_END");



static void init_agent(rl_agent_t *agent, unsigned int state_num, char player)
{
    init_rl_agent(agent, state_num, player);
    for (unsigned int i = 0; i < state_num; i++)
        agent->state_value[i] =
            get_score(hash_to_table(i), player) * INITIAL_MUTIPLIER;
}

void init_training()
{
    srand(time(NULL));
    CALC_STATE_NUM(N_STATES);
    init_agent(&agent[0], N_STATES, 'O');
    init_agent(&agent[1], N_STATES, 'X');
#if EPSILON_GREEDY
    decay_factor =
        pow((EPSILON_END / EPSILON_START), (1.0 / (NUM_EPISODE * N_GRIDS)));
#endif
}

void release_training()
{
    free(agent[0].state_value);
    free(agent[1].state_value);
}

#if EPSILON_GREEDY

static int get_action_epsilon_greedy(const Userspace *us_data,
                                     rl_agent_t *agent)
{
    if (RAND_UNIFORM < epsilon) {  // explore
        int act = random_get_move(us_data);
        return act;
    }
    int act = get_action_exploit(us_data, agent);
    epsilon *= decay_factor;
    return act;
}
#endif

static float update_state_value(int after_state_hash,
                                float reward,
                                float next,
                                rl_agent_t *agent)
{
    float curr = reward - GAMMA * next;  // curr is TD target in TD learning
                                         // and return/gain in MC learning.
    agent->state_value[after_state_hash] =
        (1 - LEARNING_RATE) * agent->state_value[after_state_hash] +
        LEARNING_RATE * curr;
#if MONTE_CARLO
    return curr;
#else
    return agent->state_value[after_state_hash];
#endif
}

void print_progress_bar(int current, int total)
{
    int width = 50;
    int progress = (current * width) / total;

    printf("\r[");
    for (int i = 0; i < width; i++) {
        if (i < progress)
            printf("=");
        else
            printf(" ");
    }
    printf("] %3d%%", (current * 100) / total);
    fflush(stdout);
}

void train(Userspace *us_data, int num_episode, char store_data)
{
    for (int episode = 0; episode < num_episode; episode++) {
        print_progress_bar(episode, num_episode);
        int episode_moves[N_GRIDS];  // from 0 moves to N_GRIDS moves.
        float reward[N_GRIDS];
        int episode_len = 0;
        char player;
        while (1) {
            player = us_data->turn;
#if EPSILON_GREEDY
            int move =
                get_action_epsilon_greedy(us_data, &agent[(player ^ 'X') == 0]);
#else
            int move = get_action_exploit(us_data, &agent[(player ^ 'X') == 0]);
#endif
            char buf;
            char win;

            user_control(us_data, (unsigned char) move, &buf);
            update_board(&us_data->board, buf);
            win = "  OX  DD"[buf >> 4];

            episode_moves[episode_len] = board_to_hash(us_data->board);
            reward[episode_len] = calculate_win_value(win, player);
            ++episode_len;
            if (win != ' ')
                break;
        }

        float next = 0;
        for (int i_move = episode_len - 1; i_move >= 0; --i_move) {
            next = update_state_value(episode_moves[i_move], reward[i_move],
                                      next, &agent[(player ^ 'X') == 0]);
        }
        us_data->board = 0;
    }

    if (store_data)
        store_state_value(agent, N_STATES);
}
