#include <iostream>

class Game {

public: 
    virtual ~Game() = default;
    virtual int calcScore(std::vector<Card> hand) = 0;
    virtual void explain_rules() = 0;
    virtual void play() = 0;

}; 