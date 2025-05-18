#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include "deck.h"
#include <stack>
#include <vector>
#include <random>

using namespace std;

Deck::Deck() {
    stack<Card> cards;

    vector<int> v(52);
    for (int i = 0; i < 52; i++) {
        v[i] = i;
    }

    random_device rd;
    mt19937 g(rd());
    shuffle(v.begin(), v.end(), g);

    for (int i = 0; i < 52; i++) {
        Card next_card(v[i]);
        cards.push(next_card); 
    }

    this->cards = cards;
}

Card Deck::Draw() {
    if (cards.empty()) {
        cout << "deck is empty\n";
        return Card(-1);
    }
    Card c = cards.top();
    cards.pop();
    return c;
}

