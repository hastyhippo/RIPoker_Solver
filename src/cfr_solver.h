#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include <map>
#include <cstdint>

#include "game.h"
#include "node.h"

class CFRSolver {
    private:
        map<uint64_t, Node> positionMap;

    public: 
        CFRSolver(int num_players);
        double CFR(Game G);
};