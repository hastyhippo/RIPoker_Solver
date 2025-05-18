#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

using namespace std;

class Node {
    private:
        vector<double> strategy;
        vector<double> strategy_sum;
        vector<double> regret_sum; 
    public:
        Node();
        void UpdateStrategy();
        void UpdateRegret(vector<double> new_regret);
        vector<double> GetFinalStrategy();
};