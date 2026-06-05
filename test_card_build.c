
#include "card.h"
#include <stdio.h>

int main(void)
{
    Card regular[BASE_REGULAR_PATH_CARD_COUNT];
    Card actions[BASE_ACTION_CARD_COUNT];
    Card play_deck[BASE_PLAY_DECK_COUNT];

    size_t regular_count = card_build_regular_path_deck(
        regular, BASE_REGULAR_PATH_CARD_COUNT);
    size_t action_count = card_build_action_deck(
        actions, BASE_ACTION_CARD_COUNT);
    size_t play_count = card_build_play_deck(
        play_deck, BASE_PLAY_DECK_COUNT);

    printf("regular path cards: %zu\n", regular_count);
    printf("action cards: %zu\n", action_count);
    printf("round play deck: %zu\n", play_count);
    printf("deck validation: %s\n",
           card_base_deck_is_valid() ? "PASS" : "FAIL");

    return card_base_deck_is_valid() ? 0 : 1;
}
