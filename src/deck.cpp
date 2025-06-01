#include <iostream>
#include <string>
#include <cmath>
#include <algorithm>
#include <stack>
#include <vector>
#include <random>

#include "defines.h"
#include "deck.h"

using namespace std;

Deck::Deck() {
    vector<Card> v(NUM_CARDS);
    for (int i = 0; i < NUM_CARDS; i++) {
        v[i] = Card(i);
    }

    random_device rd;
    mt19937 g(rd());
    shuffle(v.begin(), v.end(), g);
    it = 0;

    this->cards = v;
}

Card Deck::Draw() {
    if (it >= (int)cards.size()) {
        cout << "deck is empty\n";
        return Card(-1);
    }
    return cards[it++];
}

void Deck::Shuffle() {
    random_device rd;
    mt19937 g(rd());
    shuffle(cards.begin(), cards.end(), g);

}

void Deck::PrintDeck() {
    for (auto a : cards) {
        cout << a.getName() << " ";
    }
    cout << "\n";
}