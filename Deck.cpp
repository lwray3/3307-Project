#include <iostream>
#include <vector>
#include <random>
#include "Card.cpp"


class Deck {
    
    private:
        std::vector<Card> cards;
        int currCard; // Index of the current card to deal


    public:
        Deck() : currCard(0) {
            for (const auto& suit : {Suit::hearts, Suit::diamonds, Suit::clubs, Suit::spades}) {
                for (const auto& rank : {Rank::two, Rank::three, Rank::four, Rank::five, Rank::six, Rank::seven, Rank::eight, Rank::nine, Rank::ten, Rank::jack, Rank::queen, Rank::king, Rank::ace}) {
                    cards.push_back(Card(rank, suit));
                }
            }
        }

        void shuffle() {
            std::random_device rd;
            std::mt19937 g(rd());
            std::shuffle(cards.begin(), cards.end(), g);
            currCard = 0; // Reset current card index after shuffling
        }

        Card Deck::dealCard() {
        
        if (currCard >= cards.size()) {
        throw std::runtime_error("No cards remaining.");
         }

        Card dealt_card = cards.back();
        cards.pop_back();

        return dealt_card;
        }
};