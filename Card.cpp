#include <iostream>

enum class Rank {
    two = 2,
    three,
    four,
    five,
    six,
    seven,
    eight,
    nine,
    ten,
    jack,
    queen,
    king,
    ace
};

enum class Suit {
    hearts,
    diamonds,
    clubs,
    spades
};


class Card {
    private:
        Rank rank;
        Suit suit;
    public:
        Card(Rank rank, Suit suit) : rank(rank), suit(suit) {}

        
        Rank getRank() const {
            return rank;
        }

        Suit getSuit() const {
            return suit;
        }
};