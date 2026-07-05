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

namespace {
// Seeded once per thread; the old per-shuffle random_device+mt19937
// construction was a large fixed cost on every hand in the CFR loop.
mt19937 &Rng() {
    static thread_local mt19937 rng{random_device{}()};
    return rng;
}
}

Deck::Deck() {
    cards.resize(NUM_CARDS);
    for (int i = 0; i < NUM_CARDS; i++) {
        cards[i] = Card(i);
    }
    // No shuffle here: every consumer calls InitialiseGame -> Shuffle() first.
    it = 0;
}

Card Deck::Draw() {
    if (it >= (int)cards.size()) {
        cout << "deck is empty\n";
        return Card(-1);
    }
    return cards[it++];
}

void Deck::Shuffle() {
    shuffle(cards.begin(), cards.end(), Rng());
    it = 0; // a shuffle restarts the deck
}

void Deck::PrintDeck() {
    for (auto a : cards) {
        cout << a.getName() << " ";
    }
    cout << "\n";
}