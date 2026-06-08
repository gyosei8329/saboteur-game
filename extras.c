/*
 * extras.c
 * Saboteur / 矮人礦坑 Phase 1：人機模式 AI
 *
 * AI 策略（Phase 1 用，刻意保留改進空間）：
 *
 *   - 礦工 AI：
 *       * 嘗試把路徑牌放在距離金礦終點最近的位置。
 *
 *   - 破壞者 AI：
 *       * 嘗試把路徑牌放在距離金礦終點最遠的位置，
 *         並偏好擺死路（connected_mask 全為 0 的牌）。
 *
 *   - 兩種角色都會在工具壞掉時優先找修理牌；找不到合法放牌時棄一張牌。
 *
 * 行動牌處理目前簡化：
 *   - 破壞、地圖、落石牌：Phase 1 暫不主動使用，直接視為手牌一張，
 *     若沒位置放路徑就會被棄掉。
 *   - 修理牌：只用在自己身上。
 */

#include "extras.h"
#include "rule.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- 工具函式 ---------- */

static int absolute_value(int value)
{
    return value < 0 ? -value : value;
}

static int manhattan_distance_to_gold(int row, int col)
{
    /*
     * 我們不知道哪一張終點是金礦（這是 AI 不應該偷看的資料）。
     * 因此用「距離最近終點」估算，當作粗略的方向指引。
     */
    int best = -1;
    for (int g = 0; g < GOAL_CARD_COUNT; ++g) {
        BoardPosition goal = game_get_goal_position(g);
        int distance =
            absolute_value(goal.row - row) + absolute_value(goal.col - col);
        if (best < 0 || distance < best) {
            best = distance;
        }
    }
    return best;
}

static bool card_is_dead_end(const Card *card)
{
    if (card == NULL || card->kind != CARD_PATH) {
        return false;
    }
    for (int side = 0; side < PATH_SIDE_COUNT; ++side) {
        if (card->connected_mask[side] != 0) {
            return false;
        }
    }
    return true;
}

static const char *tool_name(int tool)
{
    switch (tool) {
        case TOOL_PICKAXE: return "Pickaxe";
        case TOOL_LANTERN: return "Lantern";
        case TOOL_CART:    return "Cart";
        default:           return "Tool";
    }
}

static void set_log(char *log, size_t log_size, const char *format, ...)
{
    if (log == NULL || log_size == 0 || format == NULL) {
        return;
    }

    va_list args;
    va_start(args, format);
    (void)vsnprintf(log, log_size, format, args);
    va_end(args);
}

/* ---------- 子策略 ---------- */

static bool ai_try_play_path_card(
    GameState *game, int player_id, char *log, size_t log_size)
{
    if (!can_play_path_card(game, player_id)) {
        return false;
    }

    Player *player = &game->players[player_id];
    Role role = player->role;
    bool is_miner = (role == ROLE_GOLD_DIGGER);

    int best_score = is_miner ? 100000 : -100000;
    int best_hand_index = -1;
    int best_row = -1;
    int best_col = -1;
    bool best_rotated = false;

    for (int hand_index = 0; hand_index < player->hand_count; ++hand_index) {
        if (player->hand[hand_index].kind != CARD_PATH) {
            continue;
        }
        Card original = player->hand[hand_index];

        for (int rotation = 0; rotation < 2; ++rotation) {
            Card candidate =
                (rotation == 0) ? original : rotate_path_card_180(original);

            for (int row = 0; row < BOARD_H; ++row) {
                for (int col = 0; col < BOARD_W; ++col) {
                    if (!is_valid_path_placement(
                            game, candidate, row, col, NULL, 0)) {
                        continue;
                    }
                    int score = manhattan_distance_to_gold(row, col);

                    /*
                     * 礦工想接近金礦（分數越小越好）。
                     * 破壞者想遠離金礦並偏好死路（分數越大越好）。
                     */
                    if (!is_miner && card_is_dead_end(&candidate)) {
                        score += 5;
                    }

                    bool better = is_miner
                        ? (score < best_score)
                        : (score > best_score);

                    if (better) {
                        best_score = score;
                        best_hand_index = hand_index;
                        best_row = row;
                        best_col = col;
                        best_rotated = (rotation == 1);
                    }
                }
            }
        }
    }

    if (best_hand_index < 0) {
        return false;
    }
    if (play_path_card(game, player_id, best_hand_index,
                       best_row, best_col, best_rotated, NULL, 0)) {
        set_log(log, log_size, "%s placed a path at (%d, %d)%s.",
                player->name, best_row, best_col,
                best_rotated ? " rotated" : "");
        return true;
    }
    return false;
}

static bool ai_try_repair_self(
    GameState *game, int player_id, char *log, size_t log_size)
{
    Player *player = &game->players[player_id];
    if (!player_has_broken_tool(game, player_id)) {
        return false;
    }

    for (int hand_index = 0; hand_index < player->hand_count; ++hand_index) {
        Card *card = &player->hand[hand_index];
        if (card->kind != CARD_ACTION
            || card->action_kind != ACTION_REPAIR_TOOL) {
            continue;
        }

        for (int tool = 0; tool < TOOL_COUNT; ++tool) {
            ToolType tool_type = (ToolType)tool;
            if ((card->tool_mask & (uint8_t)(1u << tool)) == 0) {
                continue;
            }
            if (!can_repair_tool(game, player_id, tool_type)) {
                continue;
            }
            if (play_repair_tool(
                    game, player_id, hand_index,
                    player_id, tool_type, NULL, 0)) {
                set_log(log, log_size, "%s repaired their %s.",
                        player->name, tool_name(tool));
                return true;
            }
        }
    }
    return false;
}

static bool ai_discard_first_card(
    GameState *game, int player_id, char *log, size_t log_size)
{
    Player *player = &game->players[player_id];
    if (player->hand_count <= 0) {
        return false;
    }
    if (discard_card(game, player_id, 0, NULL, 0)) {
        set_log(log, log_size, "%s discarded a card.", player->name);
        return true;
    }
    return false;
}

/* ---------- 對外公開函式 ---------- */

bool ai_take_turn_with_log(GameState *game, char *log, size_t log_size)
{
    if (game == NULL || game->phase != PHASE_PLAYER_TURN) {
        return false;
    }
    int player_id = game->current_player;
    if (player_id < 0 || player_id >= game->player_count) {
        return false;
    }
    if (!game->is_ai[player_id]) {
        return false;
    }

    /*
     * 1. 自己工具壞了優先修理，否則接下來什麼都不能做。
     */
    if (ai_try_repair_self(game, player_id, log, log_size)) {
        return true;
    }

    /*
     * 2. 嘗試放置路徑牌（按角色偏好接近或遠離金礦）。
     */
    if (ai_try_play_path_card(game, player_id, log, log_size)) {
        return true;
    }

    /*
     * 3. 最後手段：棄掉第一張手牌。
     */
    return ai_discard_first_card(game, player_id, log, log_size);
}

void ai_take_turn(GameState *game)
{
    (void)ai_take_turn_with_log(game, NULL, 0);
}
