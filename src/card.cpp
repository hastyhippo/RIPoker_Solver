#include<iostream>
#include<string>
#include<cmath>
#include<algorithm>
#include "card.h"
#include <vector>
using namespace std;

vector<string> suitNames = {"Spades", "Clubs", "Diamonds", "Hearts"};
vector<string> valueNames = {"2", "3", "4", "5", "6", "7", "8", "9", "T", "J", "Q", "K", "A"};

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