#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>

#include "deck.h"

using namespace std;

class Game {
    private:
        int num_players;
        vector<Card> hands;
        vector<Card> board;

    public:
        Game(int num_players);
        void StartGame();
        void PrintGame();

};