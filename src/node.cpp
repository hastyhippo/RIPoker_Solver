#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

#include "node.h"

using namespace std;

#define NUM_ACTIONS 2.0

Node::Node() {

}

void Node::UpdateRegret(vector<double> new_regret) {
    for (int i = 0; i < NUM_ACTIONS; i++) {
        regret_sum[i] += new_regret[i];
        strategy_sum[i] += strategy[i];
    }
}

void Node::UpdateStrategy() {
    double normalising_sum = 0;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        normalising_sum += max(0.0, regret_sum[i]);
    }

    if (normalising_sum == 0) {
        strategy = vector<double>(NUM_ACTIONS, 1.0/NUM_ACTIONS);
    }

    for (int i = 0; i < NUM_ACTIONS; i++) {
        strategy[i] = max(0.0, regret_sum[i]/normalising_sum);
    }
}

vector<double> Node::GetFinalStrategy() {
    vector<double> final_strategy(NUM_ACTIONS);
    double normalising_sum = 0.0;
    for (auto a : strategy_sum) normalising_sum += a;
    for (int i = 0; i < NUM_ACTIONS; i++) {
        final_strategy[i] = strategy_sum[i] / normalising_sum;
    }
    return final_strategy;
}