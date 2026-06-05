#include "game.h"
#include "rule.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static Card make_action(int id, ActionKind action, uint8_t tool_mask)
{
    Card card;
    memset(&card, 0, sizeof(card));
    card.id = id;
    card.kind = CARD_ACTION;
    card.action_kind = action;
    card.tool_mask = tool_mask;
    return card;
}

int main(void)
{
    GameState game;
    char reason[200];

    memset(&game, 0, sizeof(game));
    game.player_count = 3;
    game.phase = PHASE_PLAYER_TURN;
    game.current_player = 0;
    game.draw_count = 0;

    game.players[0].hand[0] = make_action(100, ACTION_BREAK_TOOL, TOOL_MASK_PICKAXE);
    game.players[0].hand_count = 1;
    game.players[1].hand[0] = make_action(200, ACTION_REPAIR_TOOL, TOOL_MASK_PICKAXE);
    game.players[1].hand_count = 1;

    bool break_ok = play_break_tool(
        &game, 0, 0, 1, TOOL_PICKAXE, reason, sizeof(reason));

    bool break_state_ok = break_ok
        && game.players[1].broken_tools[TOOL_PICKAXE]
        && game.players[1].active_broken_tool_cards[TOOL_PICKAXE].id == 100
        && game.discard_count == 0
        && game.current_player == 1;

    bool repair_ok = play_repair_tool(
        &game, 1, 0, 1, TOOL_PICKAXE, reason, sizeof(reason));

    bool repair_state_ok = repair_ok
        && !game.players[1].broken_tools[TOOL_PICKAXE]
        && game.players[1].active_broken_tool_cards[TOOL_PICKAXE].id == 0
        && game.discard_count == 2
        && game.discard_pile[0].id == 100
        && game.discard_pile[1].id == 200;

    printf("sabotage card stays active: %s\n", break_state_ok ? "PASS" : "FAIL");
    printf("repair discards both cards: %s\n", repair_state_ok ? "PASS" : "FAIL");

    return (break_state_ok && repair_state_ok) ? 0 : 1;
}
