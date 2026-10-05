#include <iostream>
#include <vector>
#include <algorithm>
#include <sstream>
#include <string>
#include <random>
#include <cctype>
#include "Card.cpp"
#include "Deck.cpp"
#include "Player.cpp"
#include "Game.h"

// Heads-up (2 player) Texas Hold'em: you vs. the dealer.
// Blinds, hole cards, flop / turn / river, fold / check / call / raise / all-in,
// best 5-of-7 showdown, split pots, dealer button rotation.
class Poker : public Game {
    private:
        enum : int {
            POW5 = 759375,      // 15^5, used to encode hand scores
            START_CHIPS = 1000,
            SMALL_BLIND = 10,
            BIG_BLIND = 20
        };

        enum class ActionType { Fold, Check, Call, Raise };
        struct Decision {
            ActionType type;
            int raiseTo;        // total street bet when type == Raise
        };

        Deck deck;
        std::vector<Card> playerHole;
        std::vector<Card> dealerHole;
        std::vector<Card> board;

        int playerChips = START_CHIPS;
        int dealerChips = START_CHIPS;
        int pot = 0;
        int streetPlayerBet = 0;    // chips each side has put in on the current street
        int streetDealerBet = 0;
        int lastRaiseSize = BIG_BLIND;

        bool buttonIsPlayer = true; // the button is the small blind and acts first preflop
        bool playerFolded = false;
        bool dealerFolded = false;
        bool inputClosed = false;
        std::mt19937 rng{std::random_device{}()};

        // ---------- display helpers ----------

        static std::string rankStr(Rank r) {
            int v = static_cast<int>(r);
            if (v <= 10) return std::to_string(v);
            switch (r) {
                case Rank::jack:  return "J";
                case Rank::queen: return "Q";
                case Rank::king:  return "K";
                default:          return "A";
            }
        }

        static std::string suitStr(Suit s) {
            switch (s) {
                case Suit::hearts:   return "H";
                case Suit::diamonds: return "D";
                case Suit::clubs:    return "C";
                default:             return "S";
            }
        }

        static std::string cardStr(const Card& c) {
            return rankStr(c.getRank()) + suitStr(c.getSuit());
        }

        static std::string handStr(const std::vector<Card>& cards) {
            std::string out;
            for (const auto& c : cards) out += "[" + cardStr(c) + "] ";
            return out.empty() ? "(none)" : out;
        }

        static std::string categoryName(int score) {
            static const char* names[] = {
                "High Card", "Pair", "Two Pair", "Three of a Kind", "Straight",
                "Flush", "Full House", "Four of a Kind", "Straight Flush"
            };
            return names[score / POW5];
        }

        void showTable() {
            std::cout << "\n----------------------------------------\n";
            std::cout << "Board:      " << handStr(board) << "\n";
            std::cout << "Pot:        " << pot << "\n";
            std::cout << "Your hand:  " << handStr(playerHole) << "\n";
            std::cout << "Your chips: " << playerChips << " (bet this round: " << streetPlayerBet << ")\n";
            std::cout << "Dealer:     " << dealerChips << " chips (bet this round: " << streetDealerBet << ")\n";
            std::cout << "----------------------------------------" << std::endl;
        }

        std::string readLine() {
            std::string line;
            if (!std::getline(std::cin, line)) {
                inputClosed = true;
                return "";
            }
            return line;
        }

        // ---------- hand evaluation ----------

        // Score for exactly 5 cards: category * 15^5 + tiebreak ranks (base 15).
        static int evaluate5(const std::vector<Card>& hand) {
            int counts[15] = {0};
            bool flush = true;
            for (const auto& card : hand) {
                counts[static_cast<int>(card.getRank())]++;
                if (card.getSuit() != hand[0].getSuit()) flush = false;
            }

            // Distinct ranks ordered by (count desc, rank desc)
            std::vector<int> ranks;
            for (int r = 14; r >= 2; --r) {
                if (counts[r] > 0) ranks.push_back(r);
            }
            std::stable_sort(ranks.begin(), ranks.end(),
                [&](int a, int b) { return counts[a] > counts[b]; });

            bool straight = false;
            if (ranks.size() == 5) {
                if (ranks[0] - ranks[4] == 4) {
                    straight = true;
                } else if (ranks[0] == 14 && ranks[1] == 5 && ranks[4] == 2) {
                    straight = true;            // wheel: A-2-3-4-5
                    ranks = {5, 4, 3, 2, 1};
                }
            }

            int c0 = counts[ranks[0]];
            int c1 = ranks.size() > 1 ? counts[ranks[1]] : 0;

            int category;
            if (straight && flush)       category = 8;
            else if (c0 == 4)            category = 7;
            else if (c0 == 3 && c1 == 2) category = 6;
            else if (flush)              category = 5;
            else if (straight)           category = 4;
            else if (c0 == 3)            category = 3;
            else if (c0 == 2 && c1 == 2) category = 2;
            else if (c0 == 2)            category = 1;
            else                         category = 0;

            int score = category;
            for (size_t i = 0; i < 5; ++i) {
                score = score * 15 + (i < ranks.size() ? ranks[i] : 0);
            }
            return score;
        }

        // ---------- chip movement ----------

        void putIn(bool isPlayer, int amount) {
            int& chips = isPlayer ? playerChips : dealerChips;
            int& bet = isPlayer ? streetPlayerBet : streetDealerBet;
            amount = std::min(amount, chips);
            chips -= amount;
            bet += amount;
            pot += amount;
        }

        // ---------- dealer AI ----------

        double handStrength() {
            if (board.empty()) {
                int a = static_cast<int>(dealerHole[0].getRank());
                int b = static_cast<int>(dealerHole[1].getRank());
                int hi = std::max(a, b), lo = std::min(a, b);
                if (a == b) return 0.5 + (hi - 2) / 12.0 * 0.45;
                double s = 0.15 + (hi - 2) / 12.0 * 0.3 + (lo - 2) / 12.0 * 0.2;
                if (dealerHole[0].getSuit() == dealerHole[1].getSuit()) s += 0.06;
                int gap = hi - lo;
                if (gap == 1) s += 0.04;
                else if (gap <= 3) s += 0.02;
                return s;
            }

            std::vector<Card> all = dealerHole;
            all.insert(all.end(), board.begin(), board.end());
            int cat = calcScore(all) / POW5;

            static const double base[] = {0.15, 0.45, 0.7, 0.8, 0.88, 0.9, 0.95, 0.98, 1.0};
            double s = base[cat];

            if (cat == 0) {
                int hi = std::max(static_cast<int>(dealerHole[0].getRank()),
                                  static_cast<int>(dealerHole[1].getRank()));
                if (hi >= 12) s = 0.25;
            } else if (cat == 1) {
                bool usesHole = dealerHole[0].getRank() == dealerHole[1].getRank();
                for (const auto& h : dealerHole)
                    for (const auto& b : board)
                        if (h.getRank() == b.getRank()) usesHole = true;
                s = usesHole ? 0.5 : 0.3;
            }

            // Draws (only matter before the river)
            if (board.size() < 5 && cat < 4) {
                int suits[4] = {0};
                bool present[16] = {false};
                for (const auto& c : all) {
                    suits[static_cast<int>(c.getSuit())]++;
                    present[static_cast<int>(c.getRank())] = true;
                }
                present[1] = present[14];
                if (*std::max_element(suits, suits + 4) >= 4) s += 0.15;
                for (int start = 1; start <= 10; ++start) {
                    int n = 0;
                    for (int r = start; r < start + 5; ++r) n += present[r] ? 1 : 0;
                    if (n >= 4) { s += 0.12; break; }
                }
            }
            return s;
        }

        Decision dealerDecision() {
            int mine = streetDealerBet, opp = streetPlayerBet;
            int toCall = opp - mine;
            int maxTotal = std::min(mine + dealerChips, opp + playerChips);
            bool canRaise = maxTotal > opp;

            std::uniform_real_distribution<double> u(0.0, 1.0);
            double s = handStrength() + (u(rng) - 0.5) * 0.16;
            bool bluff = u(rng) < 0.08;

            auto sizedRaise = [&](double frac) {
                int size = std::max<int>(lastRaiseSize, BIG_BLIND);
                int target = opp + std::max(size, static_cast<int>(frac * (pot + toCall)));
                return std::min(target, maxTotal);
            };

            if (toCall == 0) {
                if (canRaise && (s > 0.65 || (bluff && s < 0.4))) {
                    return {ActionType::Raise, sizedRaise(s > 0.8 ? 0.75 : 0.5)};
                }
                return {ActionType::Check, 0};
            }

            double odds = static_cast<double>(toCall) / (pot + toCall);
            double need = odds + 0.05;
            if (toCall >= dealerChips) need += 0.1; // calling off the whole stack
            if (canRaise && s > 0.8) {
                return {ActionType::Raise, sizedRaise(0.75)};
            }
            if (s > need || (bluff && s > 0.3)) {
                return {ActionType::Call, 0};
            }
            return {ActionType::Fold, 0};
        }

        // ---------- human input ----------

        Decision humanDecision() {
            int mine = streetPlayerBet, opp = streetDealerBet;
            int toCall = opp - mine;
            int maxTotal = std::min(mine + playerChips, opp + dealerChips);
            bool canRaise = maxTotal > opp;
            int minTotal = std::min(opp + std::max<int>(lastRaiseSize, BIG_BLIND), maxTotal);

            showTable();
            if (toCall > 0) {
                std::cout << "It costs " << std::min(toCall, playerChips) << " to call.\n";
                std::cout << "Options: (f)old, (c)all";
            } else {
                std::cout << "Options: (c)heck";
            }
            if (canRaise) {
                std::cout << ", (r)aise <total " << minTotal << "-" << maxTotal << ">";
                std::cout << ", (a)ll-in";
            }
            std::cout << std::endl;

            while (true) {
                std::cout << "Your move: ";
                std::string line = readLine();
                if (inputClosed) return {toCall > 0 ? ActionType::Fold : ActionType::Check, 0};

                std::istringstream iss(line);
                std::string cmd;
                iss >> cmd;
                std::transform(cmd.begin(), cmd.end(), cmd.begin(),
                               [](unsigned char ch) { return std::tolower(ch); });

                if (cmd == "f" || cmd == "fold") {
                    if (toCall > 0) return {ActionType::Fold, 0};
                    std::cout << "Nothing to call, you can check instead." << std::endl;
                } else if (cmd == "c" || cmd == "call" || cmd == "check") {
                    return {toCall > 0 ? ActionType::Call : ActionType::Check, 0};
                } else if (cmd == "a" || cmd == "allin" || cmd == "all-in") {
                    if (canRaise) return {ActionType::Raise, maxTotal};
                    return {toCall > 0 ? ActionType::Call : ActionType::Check, 0};
                } else if (cmd == "r" || cmd == "raise" || cmd == "b" || cmd == "bet") {
                    int amount;
                    if (!canRaise) {
                        std::cout << "You can't raise here." << std::endl;
                    } else if (!(iss >> amount)) {
                        std::cout << "Give a total amount, e.g. \"r " << minTotal << "\"." << std::endl;
                    } else if (amount < minTotal || amount > maxTotal) {
                        std::cout << "Raise must be between " << minTotal << " and " << maxTotal << "." << std::endl;
                    } else {
                        return {ActionType::Raise, amount};
                    }
                } else {
                    std::cout << "Unrecognized input." << std::endl;
                }
            }
        }

        // ---------- betting ----------

        // Applies a decision and narrates it. Returns false if the actor folded.
        bool applyDecision(bool isPlayer, const Decision& d) {
            std::string name = isPlayer ? "You" : "Dealer";
            std::string s = isPlayer ? "" : "s";
            int& chips = isPlayer ? playerChips : dealerChips;
            int myBet = isPlayer ? streetPlayerBet : streetDealerBet;
            int oppBet = isPlayer ? streetDealerBet : streetPlayerBet;

            switch (d.type) {
                case ActionType::Fold:
                    (isPlayer ? playerFolded : dealerFolded) = true;
                    std::cout << name << " fold" << s << "." << std::endl;
                    return false;
                case ActionType::Check:
                    std::cout << name << " check" << s << "." << std::endl;
                    break;
                case ActionType::Call: {
                    int amount = std::min(oppBet - myBet, chips);
                    putIn(isPlayer, amount);
                    std::cout << name << " call" << s << " " << amount
                              << (chips == 0 ? " (all-in)" : "") << "." << std::endl;
                    break;
                }
                case ActionType::Raise: {
                    int raiseSize = d.raiseTo - oppBet;
                    if (raiseSize > lastRaiseSize) lastRaiseSize = raiseSize;
                    putIn(isPlayer, d.raiseTo - myBet);
                    std::cout << name << (oppBet == 0 ? " bet" : " raise") << s << " to " << d.raiseTo
                              << (chips == 0 ? " (all-in)" : "") << "." << std::endl;
                    break;
                }
            }
            return true;
        }

        // Runs one street of betting. Returns false if someone folded.
        bool bettingRound(bool playerActsFirst) {
            bool playerTurn = playerActsFirst;
            bool pActed = false, dActed = false;

            while (true) {
                bool pCan = playerChips > 0, dCan = dealerChips > 0;
                bool equal = streetPlayerBet == streetDealerBet;
                if (equal && (!pCan || !dCan)) break;
                if (equal && pActed && dActed) break;
                if (!equal && streetPlayerBet < streetDealerBet && !pCan) break;
                if (!equal && streetDealerBet < streetPlayerBet && !dCan) break;

                Decision d = playerTurn ? humanDecision() : dealerDecision();
                if (!applyDecision(playerTurn, d)) return false;

                (playerTurn ? pActed : dActed) = true;
                if (d.type == ActionType::Raise) (playerTurn ? dActed : pActed) = false;
                playerTurn = !playerTurn;
            }

            // Return any uncalled chips (only possible when someone is all-in)
            int diff = streetPlayerBet - streetDealerBet;
            if (diff > 0) { playerChips += diff; pot -= diff; }
            else if (diff < 0) { dealerChips -= diff; pot += diff; }
            return true;
        }

        void startStreet() {
            streetPlayerBet = 0;
            streetDealerBet = 0;
            lastRaiseSize = BIG_BLIND;
        }

        void awardPot(bool toPlayer) {
            if (toPlayer) playerChips += pot; else dealerChips += pot;
            pot = 0;
        }

    public:
        Player player;   // tracks hands won / lost
        Player dealer;

        Poker() = default;

        void explain_rules() override {
            std::cout << "\nTexas Hold'em, heads-up (you vs. the dealer).\n"
                      << "- Everyone starts with " << START_CHIPS << " chips. Blinds are "
                      << SMALL_BLIND << "/" << BIG_BLIND << ".\n"
                      << "- Each hand: 2 private cards each, then a betting round. The flop (3 shared cards),\n"
                      << "  turn (1) and river (1) follow, each with its own betting round.\n"
                      << "- On your turn you can fold, check (if no bet), call, raise, or go all-in.\n"
                      << "  Raise amounts are the TOTAL you want your bet to be for that round.\n"
                      << "- At showdown the best 5-card hand from your 2 cards + the 5 shared cards wins.\n"
                      << "- Hand ranks, low to high: High Card, Pair, Two Pair, Three of a Kind, Straight,\n"
                      << "  Flush, Full House, Four of a Kind, Straight Flush.\n"
                      << "- The button (small blind, acts first preflop and last after) alternates each hand.\n"
                      << "- Win all the dealer's chips to win the match." << std::endl;
        }

        // Best 5-card hand score from 5, 6 or 7 cards.
        int calcScore(std::vector<Card> cards) override {
            int n = static_cast<int>(cards.size());
            if (n < 5) return 0;
            if (n == 5) return evaluate5(cards);

            int best = 0;
            for (int mask = 0; mask < (1 << n); ++mask) {
                int bits = 0;
                for (int i = 0; i < n; ++i) bits += (mask >> i) & 1;
                if (bits != 5) continue;

                std::vector<Card> combo;
                for (int i = 0; i < n; ++i) {
                    if ((mask >> i) & 1) combo.push_back(cards[i]);
                }
                best = std::max(best, evaluate5(combo));
            }
            return best;
        }

        void play() override {
            deck = Deck();
            deck.shuffle();
            playerHole.clear();
            dealerHole.clear();
            board.clear();
            pot = 0;
            playerFolded = dealerFolded = false;
            startStreet();

            std::cout << "\n========== NEW HAND ==========\n";
            std::cout << (buttonIsPlayer ? "You are the button (small blind)."
                                         : "Dealer is the button (small blind).") << std::endl;

            // Blinds
            if (buttonIsPlayer) { putIn(true, SMALL_BLIND); putIn(false, BIG_BLIND); }
            else                { putIn(false, SMALL_BLIND); putIn(true, BIG_BLIND); }
            std::cout << "Blinds posted: small " << SMALL_BLIND << ", big " << BIG_BLIND << "." << std::endl;

            for (int i = 0; i < 2; ++i) {
                playerHole.push_back(deck.dealCard());
                dealerHole.push_back(deck.dealCard());
            }

            // Preflop: button acts first heads-up
            bool folded = !bettingRound(buttonIsPlayer);

            static const char* streetNames[] = {"FLOP", "TURN", "RIVER"};
            bool revealed = false;
            for (int stage = 0; stage < 3 && !folded; ++stage) {
                int cardsToDeal = (stage == 0) ? 3 : 1;
                for (int i = 0; i < cardsToDeal; ++i) board.push_back(deck.dealCard());
                startStreet();
                std::cout << "\n*** " << streetNames[stage] << ": " << handStr(board) << " ***" << std::endl;

                if (playerChips > 0 && dealerChips > 0) {
                    // Post-flop the big blind (non-button) acts first
                    folded = !bettingRound(!buttonIsPlayer);
                } else if (!revealed) {
                    revealed = true;
                    std::cout << "All-in! Dealer shows: " << handStr(dealerHole) << std::endl;
                }
            }

            if (folded) {
                if (playerFolded) {
                    std::cout << "\nDealer wins the pot of " << pot << "." << std::endl;
                    awardPot(false);
                    player.addLoss();
                } else {
                    std::cout << "\nYou win the pot of " << pot << "!" << std::endl;
                    awardPot(true);
                    player.addWin();
                }
            } else {
                std::vector<Card> pAll = playerHole, dAll = dealerHole;
                pAll.insert(pAll.end(), board.begin(), board.end());
                dAll.insert(dAll.end(), board.begin(), board.end());
                int pScore = calcScore(pAll);
                int dScore = calcScore(dAll);

                std::cout << "\n=========== SHOWDOWN ===========\n";
                std::cout << "Board:       " << handStr(board) << "\n";
                std::cout << "Your hand:   " << handStr(playerHole) << "-> " << categoryName(pScore) << "\n";
                std::cout << "Dealer hand: " << handStr(dealerHole) << "-> " << categoryName(dScore) << std::endl;

                if (pScore > dScore) {
                    std::cout << "You win the pot of " << pot << "!" << std::endl;
                    awardPot(true);
                    player.addWin();
                } else if (pScore < dScore) {
                    std::cout << "Dealer wins the pot of " << pot << "." << std::endl;
                    awardPot(false);
                    player.addLoss();
                } else {
                    std::cout << "It's a tie! The pot of " << pot << " is split." << std::endl;
                    int half = pot / 2;
                    playerChips += pot - half;
                    dealerChips += half;
                    pot = 0;
                }
            }

            buttonIsPlayer = !buttonIsPlayer;
        }

        void play() overrided {
            std::cout << "Welcome to Texas Hold'em Poker!" << std::endl;
            std::cout << "Would you like to hear the rules? (y/n): ";
            std::string line = readLine();
            if (!line.empty() && (line[0] == 'y' || line[0] == 'Y')) {
                explain_rules();
            }

            playerChips = dealerChips = START_CHIPS;
            buttonIsPlayer = true;

            while (playerChips > 0 && dealerChips > 0 && !inputClosed) {
                play();

                std::cout << "\nChips - You: " << playerChips << ", Dealer: " << dealerChips
                          << "  |  Hands won: " << player.getWins()
                          << ", lost: " << player.getLosses() << std::endl;

                if (playerChips == 0 || dealerChips == 0) break;
                std::cout << "Press Enter for the next hand, or type q to quit: ";
                line = readLine();
                if (inputClosed || (!line.empty() && (line[0] == 'q' || line[0] == 'Q'))) break;
            }

            if (playerChips == 0) {
                std::cout << "\nYou're out of chips. The dealer wins the match!" << std::endl;
            } else if (dealerChips == 0) {
                std::cout << "\nYou busted the dealer. You win the match!" << std::endl;
            } else {
                std::cout << "\nThanks for playing! Final chips - You: " << playerChips
                          << ", Dealer: " << dealerChips << std::endl;
            }
        }
};