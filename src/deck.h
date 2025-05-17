#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include "card.h"
#include <stack>

using namespace std;

class Deck {
    private:
        stack<Card> cards;

    public: 
        Card Draw();
        Deck();

};