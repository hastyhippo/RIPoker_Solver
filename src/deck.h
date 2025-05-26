#pragma once
#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include "card.h"
#include <stack>
#include <vector>
using namespace std;

class Deck {
    private:
        int it;
        vector<Card> cards;

    public: 
        Card Draw();
        Deck();
        void Shuffle();
        void PrintDeck();
};