#include <iostream>
#include "Card.cpp"
#include "Deck.cpp"

class Player {

    private: 
        std::vector<Card> hand;
        int wins = 0;
        int losses = 0;

    public:
        Player() {}

        void addCard(const Card& card) {
            hand.push_back(card);
        }

        std::vector<Card> getHand() const {
            return hand;
        }

        int getWins() const {
            return wins;
        }

        int getLosses() const {
            return losses;
        }

        void addWin() {
            wins++;
        }

        void addLoss() {
            losses++;
        }
        /*
        int getScore() const {
            return score;
        }
            */
};