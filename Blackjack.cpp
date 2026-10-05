#include <iostream>
#include "Card.cpp"
#include "Deck.cpp"
#include "Player.cpp"
#include <Game.h>

class Blackjack : public Game {
    private:
        std::vector<Card> hand;
        int score;
    public:
        Deck deck;
        Player player;
        Player dealer;
        Blackjack();

        
        void hit() {
            Card new_card = deck.dealCard();
            hand.push_back(new_card);
            score += static_cast<int>(new_card.getRank());
        }

        void stand() {
            
        }

        void explain_rules() override {
            std::cout << "In Blackjack, the goal is to have a hand value as close to 21 as possible without going over. Face cards are worth 10 points, and an Ace can be worth either 1 or 11 points." << std::endl;
            std::cout << "You are dealt 2 cards at the beginning of each game. Once you have your initial hand, you can choose to 'hit' (take another card) or 'stand' (keep your current hand)." << std::endl;

        }
 
        int calcScore(std::vector<Card> hand) override {
            int aces = 0;
            for (const auto& card : hand) {
                score += static_cast<int>(card.getRank());
                if (card.getRank() == Rank::ace) {
                    if (score + 10 <= 21) {
                        score += 10; // Count Ace as 11 if it doesn't bust
                    }
                    aces++;
                }
                while (score > 21 && aces > 0) {
                    score -= 10; // Count Ace as 1 if it busts
                    aces--;
                }

                
            }
            return score;
        }

        void reset() {
            hand.clear();
            score = 0;
            deck.shuffle();
        }

        void play() {
            reset();
            std::cout << "Welcome to Blackjack!" << std::endl;
            std::cout << "Would you like to hear the rules? (y/n): ";
            char choice;
            if (std::cin >> choice && (choice == 'y' || choice == 'Y')) {
                explain_rules();
            }
            else {
                std::cout << "Let's start the game!" << std::endl;
            }

            // deal the cards
            for (int i = 0; i < 2; ++i) {
                player.addCard(deck.dealCard());
                dealer.addCard(deck.dealCard());

            }

            while (true) {
                std::cout << "Would you like to hit or stand? (h/s): ";
                if (std::cin >> choice && (choice == 'h' || choice == 'H')) {
                    player.addCard(deck.dealCard());
                    int playerScore = calcScore(player.getHand());
                    
                    if (playerScore > 21) {
                        std::cout << "You busted! Your score: " << playerScore << std::endl;
                        return;
                    }
                } else if (choice == 's' || choice == 'S') {
                    break;
                } else {
                    std::cout << "Invalid input. Please enter 'h' to hit or 's' to stand." << std::endl;
                }
            }

            std::cout << "Dealer's turn." << std::endl;
            while (calcScore(dealer.getHand()) < 17) {
                dealer.addCard(deck.dealCard());
                int dealerScore = calcScore(dealer.getHand());
                if (dealerScore > 21) {
                    std::cout << "Dealer busted! Dealer's score: " << dealerScore << std::endl;
                    return;
                }
                else {
                    std::cout << "Dealer's score: " << dealerScore << std::endl;
                }
                
            }

            std::cout << "Final Scores - Player: " << calcScore(player.getHand()) << ", Dealer: " << calcScore(dealer.getHand()) << std::endl;
            if (calcScore(player.getHand()) > calcScore(dealer.getHand())) {
                std::cout << "You win!" << std::endl;
                player.addWin();

            } else if (calcScore(player.getHand()) < calcScore(dealer.getHand()) || calcScore(player.getHand()) > 21) {
                std::cout << "Dealer wins!" << std::endl;
                player.addLoss();
            } else {
                std::cout << "It's a tie!" << std::endl;
            }
        }



};
