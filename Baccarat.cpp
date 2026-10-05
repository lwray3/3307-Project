#include <iostream>
#include "Card.cpp"
#include "Deck.cpp"
#include "Game.h"

class Baccarat : public Game {
    private:
        Deck deck;
        std::vector<Card> playerHand;
        std::vector<Card> bankerHand;

    public:
        Baccarat() {
            deck.shuffle();
        }

        void dealInitialHands() {
            playerHand.clear();
            bankerHand.clear();
            playerHand.push_back(deck.dealCard());
            bankerHand.push_back(deck.dealCard());
            playerHand.push_back(deck.dealCard());
            bankerHand.push_back(deck.dealCard());
        }

        int calculateScore(const std::vector<Card>& hand) {
            int score = 0;
            for (const auto& card : hand) {
                int rankValue = static_cast<int>(card.getRank());
                if (rankValue >= 10) rankValue = 0; // Face cards are worth 0
                score += rankValue;
            }
            return score % 10; // Baccarat scores are modulo 10
        }

        void play() override {
            dealInitialHands();
            int playerScore = calculateScore(playerHand);
            int bankerScore = calculateScore(bankerHand);

            std::cout << "Player Hand: ";
            for (const auto& card : playerHand) {
                std::cout << static_cast<int>(card.getRank()) << " ";
            }
            std::cout << "Score: " << playerScore << std::endl;

            std::cout << "Banker Hand: ";
            for (const auto& card : bankerHand) {
                std::cout << static_cast<int>(card.getRank()) << " ";
            }
            std::cout << "Score: " << bankerScore << std::endl;

            if (playerScore > bankerScore) {
                std::cout << "Player wins!" << std::endl;
            } else if (bankerScore > playerScore) {
                std::cout << "Banker wins!" << std::endl;
            } else {
                std::cout << "It's a tie!" << std::endl;
            }
        }
};