#include "game.h"
#include "card.h"
#include "rule.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static bool initial_board_is_valid(const GameState *game)
{
    BoardPosition start = game_get_start_position();
    int occupied_count = 0;
    int hidden_goal_count = 0;
    int gold_count = 0;

    if (!game->board[start.row][start.col].occupied
        || !game->board[start.row][start.col].is_start) {
        return false;
    }

    for (int row = 0; row < BOARD_H; ++row) {
        for (int col = 0; col < BOARD_W; ++col) {
            const Cell *cell = &game->board[row][col];

            if (cell->occupied) {
                ++occupied_count;
            }
            if (cell->is_goal) {
                ++hidden_goal_count;
                if (cell->goal_revealed) {
                    return false;
                }
                if (cell->contains_gold) {
                    ++gold_count;
                }
            }
        }
    }

    return occupied_count == 4
        && hidden_goal_count == GOAL_CARD_COUNT
        && gold_count == 1;
}

int main(void)
{
    const char names[5][MAX_NAME_LEN] = {
        "Alice", "Bob", "Carol", "David", "Eve"
    };
    GameState game;

    game_seed_random(1234u);

    if (!game_init(&game, 5, names)) {
        puts("game_init: FAIL");
        return 1;
    }

    bool hands_are_valid = true;
    for (int i = 0; i < game.player_count; ++i) {
        hands_are_valid =
            hands_are_valid && game.players[i].hand_count == 6;
    }

    bool round_one_ok =
        game.round_number == 1
        && game.phase == PHASE_ROLE_CHECK
        && game.role_pool_count == 6
        && game.draw_count == BASE_PLAY_DECK_COUNT - 5 * 6
        && hands_are_valid
        && initial_board_is_valid(&game);

    for (;;) {
        int next = game_next_player_to_check_role(&game);
        if (next == NO_PLAYER) {
            break;
        }
        mark_role_checked(&game, next);
    }

    bool role_check_ok =
        game.phase == PHASE_PLAYER_TURN
        && game.current_player == game.starting_player;

    game.players[0].gold_total = 3;
    game.phase = PHASE_ROUND_RESULT;
    int first_round_starter = game.starting_player;

    bool second_round_ok =
        game_begin_next_round(&game)
        && game.round_number == 2
        && game.phase == PHASE_ROLE_CHECK
        && game.starting_player == (first_round_starter + 1) % 5
        && game.players[0].gold_total == 3
        && initial_board_is_valid(&game);

    game.round_number = 3;
    game.phase = PHASE_ROUND_RESULT;

    bool finish_ok =
        game_begin_next_round(&game)
        && game.phase == PHASE_GAME_RESULT
        && game.current_player == NO_PLAYER;

    printf("round 1 setup: %s\n", round_one_ok ? "PASS" : "FAIL");
    printf("private role sequence: %s\n", role_check_ok ? "PASS" : "FAIL");
    printf("round transition: %s\n", second_round_ok ? "PASS" : "FAIL");
    printf("game finish: %s\n", finish_ok ? "PASS" : "FAIL");

    return (round_one_ok && role_check_ok && second_round_ok && finish_ok)
        ? 0 : 1;
}
