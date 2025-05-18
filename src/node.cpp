#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

#include "node.h"

using namespace std;

#define NUM_ACTIONS 2

Node::Node() {

}

void Node::UpdateRegret(vector<double> new_regret) {
    int n = regret_sum.size();
    for (int i = 0; i < n; i++) {
        regret_sum[i] += new_regret[i];
        strategy_sum[i] += strategy[i];
    }
}

void Node::UpdateStrategy() {
    double normalising_sum = 0;
    int n = regret_sum.size();
    for (int i = 0; i < n; i++) {
        normalising_sum += max(0.0, regret_sum[i]);
    }

    if (normalising_sum == 0) {
        strategy = vector<double>(n, 1.0/n);
    }

    for (int i = 0; i < n; i++) {
        strategy[i] = max(0.0, regret_sum[i]/normalising_sum);
    }
}

vector<double> Node::GetFinalStrategy() {
    int n = strategy.size();
    vector<double> final_strategy(n);
    double normalising_sum = 0.0;
    for (auto a : strategy_sum) normalising_sum += a;
    for (int i = 0; i < n; i++) {
        final_strategy[i] = strategy_sum[i] / normalising_sum;
    }
    return final_strategy;
}