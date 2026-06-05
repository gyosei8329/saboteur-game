/*
 * score.c
 * Saboteur / 矮人礦坑（基礎版）：金塊牌與三輪計分實作
 *
 * 標準基礎版金塊牌共有 28 張：16 張價值 1、8 張價值 2、
 * 4 張價值 3。金塊牌堆跨三輪共用；玩家取得的分數在最終結算前
 * 應視為私密資訊。
 */

#include "score.h"
#include "rule.h"

#include <stdlib.h>
#include <string.h>

static void shuffle_gold_values(int *values, int count)
{
    if (values == NULL || count <= 1) {
        return;
    }

    for (int i = count - 1; i > 0; --i) {
        int other = rand() % (i + 1);
        int temporary = values[i];
        values[i] = values[other];
        values[other] = temporary;
    }
}

static bool is_valid_player(const GameState *game, int player_id)
{
    return game != NULL
        && player_id >= 0
        && player_id < game->player_count;
}

static int count_value(const GameState *game, int value)
{
    int count = 0;
    for (int i = 0; i < game->gold_draw_count; ++i) {
        if (game->gold_deck[i] == value) {
            ++count;
        }
    }
    return count;
}

static bool remove_one_gold_value(GameState *game, int value)
{
    for (int i = 0; i < game->gold_draw_count; ++i) {
        if (game->gold_deck[i] == value) {
            --game->gold_draw_count;
            game->gold_deck[i] = game->gold_deck[game->gold_draw_count];
            return true;
        }
    }
    return false;
}

static bool consume_exact_gold_value(GameState *game, int required_value)
{
    int available_one = count_value(game, 1);
    int available_two = count_value(game, 2);
    int available_three = count_value(game, 3);

    for (int threes = 0; threes <= available_three; ++threes) {
        for (int twos = 0; twos <= available_two; ++twos) {
            int ones = required_value - 3 * threes - 2 * twos;
            if (ones < 0 || ones > available_one) {
                continue;
            }

            for (int i = 0; i < threes; ++i) {
                (void)remove_one_gold_value(game, 3);
            }
            for (int i = 0; i < twos; ++i) {
                (void)remove_one_gold_value(game, 2);
            }
            for (int i = 0; i < ones; ++i) {
                (void)remove_one_gold_value(game, 1);
            }
            return true;
        }
    }

    return false;
}

void score_initialize_gold_deck(GameState *game)
{
    if (game == NULL) {
        return;
    }

    int index = 0;
    for (int i = 0; i < GOLD_VALUE_ONE_COUNT; ++i) {
        game->gold_deck[index++] = 1;
    }
    for (int i = 0; i < GOLD_VALUE_TWO_COUNT; ++i) {
        game->gold_deck[index++] = 2;
    }
    for (int i = 0; i < GOLD_VALUE_THREE_COUNT; ++i) {
        game->gold_deck[index++] = 3;
    }

    game->gold_draw_count = GOLD_DECK_COUNT;
    shuffle_gold_values(game->gold_deck, game->gold_draw_count);
}

int score_count_gold_miners(const GameState *game)
{
    return count_players_with_role(game, ROLE_GOLD_DIGGER);
}

int score_draw_cards_for_gold_miners(
    GameState *game,
    int *drawn_values,
    size_t capacity
)
{
    if (game == NULL
        || drawn_values == NULL
        || game->phase != PHASE_ROUND_RESULT
        || !game->round_won_by_gold_diggers) {
        return 0;
    }

    int needed = score_count_gold_miners(game);
    if (needed <= 0
        || capacity < (size_t)needed
        || game->gold_draw_count < needed) {
        return 0;
    }

    for (int i = 0; i < needed; ++i) {
        drawn_values[i] = game->gold_deck[--game->gold_draw_count];
    }

    return needed;
}

int score_next_gold_miner_counterclockwise(
    const GameState *game,
    int from_player
)
{
    if (!is_valid_player(game, from_player)) {
        return NO_PLAYER;
    }

    for (int step = 1; step <= game->player_count; ++step) {
        int candidate =
            (from_player - step + game->player_count) % game->player_count;
        if (game->players[candidate].role == ROLE_GOLD_DIGGER) {
            return candidate;
        }
    }

    return NO_PLAYER;
}

int score_first_gold_miner_recipient(const GameState *game)
{
    if (game == NULL
        || !game->round_won_by_gold_diggers
        || !is_valid_player(game, game->last_player_connected_goal)) {
        return NO_PLAYER;
    }

    int connector = game->last_player_connected_goal;
    if (game->players[connector].role == ROLE_GOLD_DIGGER) {
        return connector;
    }

    /* 若破壞者接通金礦，由其右側第一位淘金矮人開始選牌。 */
    return score_next_gold_miner_counterclockwise(game, connector);
}

bool score_award_gold_card(GameState *game, int player_id, int gold_value)
{
    if (!is_valid_player(game, player_id)
        || game->players[player_id].role != ROLE_GOLD_DIGGER
        || gold_value < 1
        || gold_value > 3) {
        return false;
    }

    game->players[player_id].gold_total += gold_value;
    return true;
}

bool score_award_saboteur_round(
    GameState *game,
    int awarded_values[MAX_PLAYERS]
)
{
    if (game == NULL
        || game->phase != PHASE_ROUND_RESULT
        || game->round_won_by_gold_diggers) {
        return false;
    }

    if (awarded_values != NULL) {
        memset(awarded_values, 0, sizeof(int) * MAX_PLAYERS);
    }

    int saboteur_count = count_players_with_role(game, ROLE_SABOTEUR);
    int reward = saboteur_reward_per_player(saboteur_count);

    /* 低人數局可能沒有實際破壞者，此輪不分金塊。 */
    if (reward == 0) {
        return true;
    }

    for (int i = 0; i < game->player_count; ++i) {
        if (game->players[i].role != ROLE_SABOTEUR) {
            continue;
        }
        if (!consume_exact_gold_value(game, reward)) {
            return false;
        }
        game->players[i].gold_total += reward;
        if (awarded_values != NULL) {
            awarded_values[i] = reward;
        }
    }

    return true;
}
