# RIPoker_Solver
Solver for the simplified Poker variant of Rhode Island Poker

Website: https://hastyhippo.github.io/RIPoker_Solver/

Rhode Island Poker is a modified version of Texas Hold'em, and this solver works on a slightly modified version. Game rules:

# Preflop 
The first player to act has an ante of 1 dollar, and the second player to act has an ante of 2 dollars. 

Each player starts with one card only with a shortdeck from 2-7.

# Flop
The flop is only one card.

# Turn
The turn is also only one card with essentially the same 

# Showdown
Hand Rankings are modified to account for the fact that only 3 card hands are possible. THe best possible hand wins, by comparing their hand strength through these rankings from best to worst:
Straight flush: 3 consecutive cards with the same suit. eg 7s, 6s, 5s
3 of a kind: 3 cards that have the same value. eg 7s 7d 7h
Straight: 3 consecutive cards. eg 4d 3s 2h
Flush: 3 cards of the same suit. eg 2h 4h 6h
Pair: 2 paired cards and one unpaired card. Ties of the paired cards are broken by the unpaired card value. eg 4h 4d 2h loses to 4h 4d 7d
High card: The top card out of the 3. Ties are broken by first point of difference when comparing cards in descending value order


<img width="1920" height="992" alt="image" src="https://github.com/user-attachments/assets/10cc7644-a480-4ed5-97c1-ad534a80879a" />
