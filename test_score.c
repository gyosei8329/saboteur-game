#include "game.h"
#include "rule.h"
#include "score.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool gold_deck_initialization_test(void)
{
    GameState game;
    memset(&game, 0, sizeof(game));
    game_seed_random(1u);
    score_initialize_gold_deck(&game);

    int sum = 0;
    int count_one = 0;
    int count_two = 0;
    int count_three = 0;
    for (int i = 0; i < game.gold_draw_count; ++i) {
        sum += game.gold_deck[i];
        if (game.gold_deck[i] == 1) ++count_one;
        if (game.gold_deck[i] == 2) ++count_two;
        if (game.gold_deck[i] == 3) ++count_three;
    }

    return game.gold_draw_count == GOLD_DECK_COUNT
        && count_one == GOLD_VALUE_ONE_COUNT
        && count_two == GOLD_VALUE_TWO_COUNT
        && count_three == GOLD_VALUE_THREE_COUNT
        && sum == 44;
}

static bool miner_reward_test(void)
{
    GameState game;
    int drawn[MAX_PLAYERS] = {0};
    memset(&game, 0, sizeof(game));
    game.player_count = 4;
    game.phase = PHASE_ROUND_RESULT;
    game.round_won_by_gold_diggers = true;
    game.last_player_connected_goal = 2;
    game.players[0].role = ROLE_GOLD_DIGGER;
    game.players[1].role = ROLE_SABOTEUR;
    game.players[2].role = ROLE_GOLD_DIGGER;
    game.players[3].role = ROLE_GOLD_DIGGER;
    score_initialize_gold_deck(&game);

    int count = score_draw_cards_for_gold_miners(&game, drawn, MAX_PLAYERS);
    int first = score_first_gold_miner_recipient(&game);
    int next = score_next_gold_miner_counterclockwise(&game, first);

    return count == 3 && game.gold_draw_count == 25
        && first == 2 && next == 0;
}

static bool saboteur_reward_test(void)
{
    GameState game;
    int awards[MAX_PLAYERS];
    memset(&game, 0, sizeof(game));
    game.player_count = 5;
    game.phase = PHASE_ROUND_RESULT;
    game.round_won_by_gold_diggers = false;
    game.players[0].role = ROLE_SABOTEUR;
    game.players[1].role = ROLE_GOLD_DIGGER;
    game.players[2].role = ROLE_GOLD_DIGGER;
    game.players[3].role = ROLE_SABOTEUR;
    game.players[4].role = ROLE_GOLD_DIGGER;
    score_initialize_gold_deck(&game);

    if (!score_award_saboteur_round(&game, awards)) {
        return false;
    }

    return awards[0] == 3 && awards[3] == 3
        && game.players[0].gold_total == 3
        && game.players[3].gold_total == 3;
}

int main(void)
{
    bool deck = gold_deck_initialization_test();
    bool miners = miner_reward_test();
    bool saboteurs = saboteur_reward_test();

    printf("gold deck setup: %s\n", deck ? "PASS" : "FAIL");
    printf("gold miner reward: %s\n", miners ? "PASS" : "FAIL");
    printf("saboteur reward: %s\n", saboteurs ? "PASS" : "FAIL");
    return (deck && miners && saboteurs) ? 0 : 1;
}
