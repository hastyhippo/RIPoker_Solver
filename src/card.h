#include<iostream>
#include<string>
#include<cmath>
#include<algorithm>

using namespace std;

class Card {
    private:
        int card_value;

    public: 
        int getSuit();
        int getValue();
        string getName();
        Card(int value);
        Card() : card_value(-1) {};

};