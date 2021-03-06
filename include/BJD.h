#pragma once
#include <map>
#include "Hand.hpp"
#include "BJP.h"

#define cardsTotalLimit 21

using namespace std;

class BJD
{

    public:
        BJD();
        bool check_win(BJP);
        void deal_cards(BJP);
        Hand current_hand_dealer();
        void reveal();


    private:
        map<int, BJP> players;
        Hand hand;

};
