#ifndef TRAIN_H
#define TRAIN_H
#include "../userspace.h"
// for training
#define INITIAL_MUTIPLIER 0.0001
#define LEARNING_RATE 0.02
#define NUM_EPISODE 10000
#define EPSILON_GREEDY 1
#define MONTE_CARLO 1

// for Markov decision model
#define GAMMA 0.99
#define REWARD_TRADEOFF 1

// for epsilon greedy
#define EPSILON_START 0.5
#define EPSILON_END 0.001

// Uncomment it if you want to see the log output.
// #define VERBOSE

void init_training();
void train(Userspace *us_data, int num_episode, char store_data);
void release_training();
#endif