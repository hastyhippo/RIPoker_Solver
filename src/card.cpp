#include<iostream>
#include<string>
#include<cmath>
#include<algorithm>
#include<inttypes.h>
#include <map>
#include "card.h"
#include <vector>
using namespace std;

#define NUM_CARDS 12
vector<string> suitNames = {"Spades", "Clubs", "Diamonds", "Hearts"};
vector<string> valueNames = {"2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A"};
vector<int> cardHashingValues = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 43};
int hasFlush = 47;

Card::Card(int value){
    if(value >= 52 || value < -1) {
        cout << "Card value is out of bounds\n";
        exit(0);
    }
    this->card_value = value;
}

int Card::getValue() {
    return card_value / 4;
}

int Card::getSuit() {
    return card_value % 4;
}

string Card::getName() {
    if (card_value == -1) return "INVALID CARD";
    return valueNames[card_value / 4] + " of " + suitNames[card_value % 4];
}

int Card::getHashing(Card c1, Card c2, Card c3) {
    int hash = 1;

    if (c1.getSuit() == c2.getSuit() && c2.getSuit() == c3.getSuit()) {
        hash *= hasFlush;
    }

    hash *= cardHashingValues[c1.getValue()];
    hash *= cardHashingValues[c2.getValue()];
    hash *= cardHashingValues[c3.getValue()];
    return hash;
}

void Card::initialiseMap(map<int, int> &mp){
    int strength = 0;
    //Straight Flush
    for (int i = NUM_CARDS - 2; i >=0; i--) {
        mp[hasFlush * cardHashingValues[i] * cardHashingValues[i+1] * cardHashingValues[i+2]] = strength++;
    }
    cout << "SF: " << strength << '\n';
    // Trips
    for (int i = NUM_CARDS; i >= 0; i--) {
        mp[cardHashingValues[i] * cardHashingValues[i] * cardHashingValues[i]] = strength++;
    }
    cout << "Trips: " << strength << '\n';

    // Straight
    for (int i = NUM_CARDS - 2; i >= 0; i--) {
        mp[cardHashingValues[i] * cardHashingValues[i+1] * cardHashingValues[i+2]] = strength++;
    }
    cout << "Straight: " << strength << '\n';

    //Flush
    for (int i = NUM_CARDS; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            for (int k = j - 1; k >= 0; k--) {
                if (mp.count(cardHashingValues[i] * cardHashingValues[j] * cardHashingValues[k] * hasFlush) != 0) continue;
                mp[hasFlush * cardHashingValues[i] * cardHashingValues[j] * cardHashingValues[k]] = strength++;
            }
        }
    }
    cout << "Flush: " << strength << '\n';

    //PAIR
    for (int i = NUM_CARDS; i >= 0; i--) {
        for (int j = NUM_CARDS; j >= 0; j--) {
            if (i == j) continue;
            mp[cardHashingValues[i] * cardHashingValues[i] * cardHashingValues[j]] = strength++;
        }
    }
    cout << "Pair: " << strength << '\n';

    //HIGH CARD
    for (int i = NUM_CARDS; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            for (int k = j - 1; k >= 0; k--) {
                if (mp.count(cardHashingValues[i] * cardHashingValues[j] * cardHashingValues[k]) != 0) continue;
                mp[cardHashingValues[i] * cardHashingValues[j] * cardHashingValues[k]] = strength++;
            }
        }
    }
    cout << "High Card: " << strength << '\n';

}