#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

using namespace std;

class Game;
#include "game.h"
class Node {
    private:
        vector<double> strategy_sum;
        vector<double> regret_sum; 
        vector<bool> possible_actions;
    public:
        vector<double> strategy;
        Node(vector<bool> actions);
        Node();

        void UpdateStrategy();
        void UpdateRegret(vector<double> new_regret);
        vector<double> GetFinalStrategy();

    static char GetBetAction(int pot, int bet_size);
    static char GetRaiseAction(int bet_size, int last_bet_size);
    static string GetHash(Game &g);
};