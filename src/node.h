#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

using namespace std;

#include "game.h"
class Node {
    private:
        string positionHash;
        vector<double> strategy_sum;
        vector<double> regret_sum; 
    public:
        vector<double> strategy;

        Node();
        void UpdateStrategy();
        void UpdateRegret(vector<double> new_regret);
        vector<double> GetFinalStrategy();
        
    
    static char GetAction(int pot, int bet_size, int flag);
    static char GetAction(int bet_size, int last_bet_size);
    static string GetHash(int stage, bool can_raise, double spr, string game_history, Card hole, Card board1, Card board2);
};