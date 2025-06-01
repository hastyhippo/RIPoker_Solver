#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include <map>
#include <cstdint>

#include "node.h"

class Game;
class Node;

using namespace std;

class CFRSolver {
    private:
        map<string, Node> positionMap;

    public: 
        CFRSolver();
        Node& GetNode(string hash);
        void TrainCFR();
        double CFR(Game &g, double p1, double p2);

};