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
vector<int> primeValue = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 43};
int hasFlush = 47;
map<int, int> Card::handStrengthMap;

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

int Card::getStrength(Card c1, Card c2, Card c3) {
    int hash = 1;

    if (c1.getSuit() == c2.getSuit() && c2.getSuit() == c3.getSuit()) {
        hash *= hasFlush;
    }

    hash *= primeValue[c1.getValue()];
    hash *= primeValue[c2.getValue()];
    hash *= primeValue[c3.getValue()];
    if (Card::handStrengthMap.count(hash) == 0) {
        return -1;
    } 
    return Card::handStrengthMap[hash];
}

void Card::initialiseMap(){
    int strength = 0;
    //Straight Flush
    for (int i = NUM_CARDS - 2; i >=0; i--) {
        Card::handStrengthMap[hasFlush * primeValue[i] * primeValue[i+1] * primeValue[i+2]] = strength++;
    }
    // Trips
    for (int i = NUM_CARDS; i >= 0; i--) {
        Card::handStrengthMap[primeValue[i] * primeValue[i] * primeValue[i]] = strength++;
    }

    // Straight
    for (int i = NUM_CARDS - 2; i >= 0; i--) {
        Card::handStrengthMap[primeValue[i] * primeValue[i+1] * primeValue[i+2]] = strength++;
    }

    //Flush
    for (int i = NUM_CARDS; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            for (int k = j - 1; k >= 0; k--) {
                if (Card::handStrengthMap.count(primeValue[i] * primeValue[j] * primeValue[k] * hasFlush) != 0) continue;
                Card::handStrengthMap[hasFlush * primeValue[i] * primeValue[j] * primeValue[k]] = strength++;
            }
        }
    }

    //PAIR
    for (int i = NUM_CARDS; i >= 0; i--) {
        for (int j = NUM_CARDS; j >= 0; j--) {
            if (i == j) continue;
            Card::handStrengthMap[primeValue[i] * primeValue[i] * primeValue[j]] = strength++;
        }
    }

    //HIGH CARD
    for (int i = NUM_CARDS; i >= 0; i--) {
        for (int j = i - 1; j >= 0; j--) {
            for (int k = j - 1; k >= 0; k--) {
                if (Card::handStrengthMap.count(primeValue[i] * primeValue[j] * primeValue[k]) != 0) continue;
                Card::handStrengthMap[primeValue[i] * primeValue[j] * primeValue[k]] = strength++;
            }
        }
    }
}