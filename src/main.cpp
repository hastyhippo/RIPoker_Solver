#include <iostream>
#include <cstdint>
#include <map>
#include "game.h"
using namespace std;

map<int,int> handStrengthMap;
void Initialise() {
    Card::initialiseMap(handStrengthMap);
}
int main() {
    Initialise();
    // Game g(200,3);
    // g.StartGame(0);

    return 0;

    
}
