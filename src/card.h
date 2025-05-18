#include<iostream>
#include<string>
#include<cmath>
#include<algorithm>
#include<map>

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
    static int getHashing(Card c1, Card c2, Card c3);
    static void initialiseMap(map<int, int> &mp);
};