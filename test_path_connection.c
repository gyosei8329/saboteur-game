#include "game.h"
#include "card.h"
#include "rule.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static Card make_connected_test_path(uint8_t edge_mask)
{
    Card card;
    memset(&card, 0, sizeof(card));
    card.kind = CARD_PATH;
    card.edge_mask = edge_mask;
    card.action_kind = ACTION_NONE;

    for (int side = 0; side < PATH_SIDE_COUNT; ++side) {
        uint8_t bit = (uint8_t)(1u << side);
        if ((edge_mask & bit) != 0u) {
            card.connected_mask[side] =
                (uint8_t)(edge_mask & (uint8_t)(~bit));
        }
    }

    return card;
}

static Card make_dead_end_test_path(uint8_t edge_mask)
{
    Card card;
    memset(&card, 0, sizeof(card));
    card.kind = CARD_PATH;
    card.edge_mask = edge_mask;
    card.action_kind = ACTION_NONE;
    return card;
}

static void put_start(GameState *game)
{
    memset(game, 0, sizeof(*game));
    game->board[12][2].occupied = true;
    game->board[12][2].is_start = true;
    game->board[12][2].card = card_make_start();
}

static void put_path(GameState *game, int row, int col, Card card)
{
    game->board[row][col].occupied = true;
    game->board[row][col].card = card;
}

int main(void)
{
    char reason[160];
    GameState game;
    Card horizontal = make_connected_test_path(
        PATH_EDGE_LEFT | PATH_EDGE_RIGHT
    );

    put_start(&game);
    bool extend_from_start = is_valid_path_placement(
        &game, horizontal, 12, 3, reason, sizeof(reason)
    );
    bool outside_play_area_is_rejected = !is_valid_path_placement(
        &game, horizontal, 12, 1, reason, sizeof(reason)
    );

    put_path(&game, 12, 3, horizontal);
    bool wall_touch_only_is_rejected = !is_valid_path_placement(
        &game, horizontal, 11, 3, reason, sizeof(reason)
    );

    put_path(&game, 12, 6, horizontal);
    bool isolated_extension_is_rejected = !is_valid_path_placement(
        &game, horizontal, 12, 7, reason, sizeof(reason)
    );

    bool continued_real_path_is_allowed = is_valid_path_placement(
        &game, horizontal, 12, 4, reason, sizeof(reason)
    );

    put_start(&game);
    Card terminal_dead_end = make_dead_end_test_path(PATH_EDGE_LEFT);
    bool attached_dead_end_is_allowed = is_valid_path_placement(
        &game, terminal_dead_end, 12, 3, reason, sizeof(reason)
    );

    printf("extend from start: %s\n", extend_from_start ? "PASS" : "FAIL");
    printf("outside 9x5 area rejected: %s\n",
           outside_play_area_is_rejected ? "PASS" : "FAIL");
    printf("wall-only adjacency rejected: %s\n",
           wall_touch_only_is_rejected ? "PASS" : "FAIL");
    printf("isolated extension rejected: %s\n",
           isolated_extension_is_rejected ? "PASS" : "FAIL");
    printf("continue connected route: %s\n",
           continued_real_path_is_allowed ? "PASS" : "FAIL");
    printf("dead end attached to route allowed: %s\n",
           attached_dead_end_is_allowed ? "PASS" : "FAIL");

    return (extend_from_start
            && outside_play_area_is_rejected
            && wall_touch_only_is_rejected
            && isolated_extension_is_rejected
            && continued_real_path_is_allowed
            && attached_dead_end_is_allowed) ? 0 : 1;
}
