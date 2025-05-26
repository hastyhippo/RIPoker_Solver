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

using namespace std;

class CFRSolver {
    private:
        map<string, Node> positionMap;

    public: 
        CFRSolver(int num_players);
        int getAction(string s);
        Node getNode(string history);

};