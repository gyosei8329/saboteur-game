/*
 * main.c
 * Saboteur / 矮人礦坑（基礎版）：終端機可遊玩版本
 *
 * 編譯：
 *   gcc -std=c11 -Wall -Wextra -Wpedantic -I. \
 *       main.c game.c card.c rule.c score.c -o saboteur
 *
 * 執行：
 *   ./saboteur
 */

#include "card.h"
#include "game.h"
#include "rule.h"
#include "score.h"

#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define INPUT_BUFFER_SIZE 128
#define REASON_SIZE 200
#define PUBLIC_LOG_LIMIT 8
#define PUBLIC_LOG_MESSAGE_SIZE 180

static void clear_screen(void)
{
    /*
     * 2J 清除畫面、3J 清除 scrollback buffer、H 回到左上角。
     * 多數 Linux/macOS 終端機支援 3J；若終端機不支援，仍需玩家遵守
     * 私人資訊不可向上捲動查看的遊玩約定。
     */
    (void)printf("\033[2J\033[3J\033[H");
    (void)fflush(stdout);
}

typedef struct {
    char entries[PUBLIC_LOG_LIMIT][PUBLIC_LOG_MESSAGE_SIZE];
    int count;
} PublicLog;

static PublicLog public_log;

static void reset_public_log(void)
{
    memset(&public_log, 0, sizeof(public_log));
}

static void add_public_log(const char *format, ...)
{
    va_list args;

    if (public_log.count == PUBLIC_LOG_LIMIT) {
        memmove(public_log.entries[0], public_log.entries[1],
                (PUBLIC_LOG_LIMIT - 1u) * sizeof(public_log.entries[0]));
        memset(public_log.entries[PUBLIC_LOG_LIMIT - 1], 0,
               sizeof(public_log.entries[PUBLIC_LOG_LIMIT - 1]));
        public_log.count = PUBLIC_LOG_LIMIT - 1;
    }

    va_start(args, format);
    (void)vsnprintf(public_log.entries[public_log.count],
                    sizeof(public_log.entries[public_log.count]),
                    format, args);
    va_end(args);
    ++public_log.count;
}

static void print_public_log(void)
{
    (void)printf("\n公開行動紀錄（最近 %d 筆）：\n", PUBLIC_LOG_LIMIT);
    if (public_log.count == 0) {
        (void)printf("  尚無公開行動。\n");
        return;
    }

    for (int i = 0; i < public_log.count; ++i) {
        (void)printf("  - %s\n", public_log.entries[i]);
    }
}

static bool read_line(char *buffer, size_t size)
{
    if (buffer == NULL || size == 0u || fgets(buffer, (int)size, stdin) == NULL) {
        return false;
    }

    size_t length = strlen(buffer);
    if (length > 0u && buffer[length - 1u] == '\n') {
        buffer[length - 1u] = '\0';
    } else {
        int character = 0;
        while ((character = getchar()) != '\n' && character != EOF) {
            /* discard overly long line */
        }
    }
    return true;
}

static void wait_for_enter(const char *message)
{
    char buffer[INPUT_BUFFER_SIZE];
    (void)printf("%s", message);
    (void)fflush(stdout);
    if (!read_line(buffer, sizeof(buffer))) {
        exit(EXIT_SUCCESS);
    }
}

static int read_int_range(const char *prompt, int minimum, int maximum)
{
    char input[INPUT_BUFFER_SIZE];

    for (;;) {
        char *end = NULL;
        long value = 0;

        (void)printf("%s", prompt);
        (void)fflush(stdout);
        if (!read_line(input, sizeof(input))) {
            exit(EXIT_SUCCESS);
        }

        value = strtol(input, &end, 10);
        if (end != input && *end == '\0'
            && value >= minimum && value <= maximum) {
            return (int)value;
        }

        (void)printf("請輸入 %d 到 %d 之間的整數。\n", minimum, maximum);
    }
}

static const char *role_name(Role role)
{
    return role == ROLE_SABOTEUR ? "破壞者" : "淘金矮人";
}

static const char *tool_name(ToolType tool)
{
    switch (tool) {
        case TOOL_PICKAXE: return "十字鎬";
        case TOOL_LANTERN: return "礦燈";
        case TOOL_CART: return "推車";
        default: return "未知工具";
    }
}

static void append_text(char *out, size_t capacity, const char *text)
{
    size_t used = strlen(out);
    if (used + 1u < capacity) {
        (void)snprintf(out + used, capacity - used, "%s", text);
    }
}

static void path_edge_text(uint8_t mask, char *out, size_t capacity)
{
    out[0] = '\0';
    if ((mask & PATH_EDGE_UP) != 0u) {
        append_text(out, capacity, "上 ");
    }
    if ((mask & PATH_EDGE_RIGHT) != 0u) {
        append_text(out, capacity, "右 ");
    }
    if ((mask & PATH_EDGE_DOWN) != 0u) {
        append_text(out, capacity, "下 ");
    }
    if ((mask & PATH_EDGE_LEFT) != 0u) {
        append_text(out, capacity, "左 ");
    }
    if (out[0] == '\0') {
        append_text(out, capacity, "無出口");
    }
}

static bool path_has_internal_connection(const Card *card)
{
    for (int side = 0; side < PATH_SIDE_COUNT; ++side) {
        if (card->connected_mask[side] != 0u) {
            return true;
        }
    }
    return false;
}

static void tool_mask_text(uint8_t mask, char *out, size_t capacity)
{
    out[0] = '\0';
    if ((mask & TOOL_MASK_PICKAXE) != 0u) {
        append_text(out, capacity, "十字鎬 ");
    }
    if ((mask & TOOL_MASK_LANTERN) != 0u) {
        append_text(out, capacity, "礦燈 ");
    }
    if ((mask & TOOL_MASK_CART) != 0u) {
        append_text(out, capacity, "推車 ");
    }
}

static void print_card_description(const Card *card)
{
    char details[64];

    if (card->kind == CARD_PATH) {
        path_edge_text(card->edge_mask, details, sizeof(details));
        (void)printf("坑道牌：出口[%s] %s", details,
                     path_has_internal_connection(card) ? "連通" : "死路");
        return;
    }

    switch (card->action_kind) {
        case ACTION_BREAK_TOOL:
            tool_mask_text(card->tool_mask, details, sizeof(details));
            (void)printf("破壞工具牌：%s", details);
            break;
        case ACTION_REPAIR_TOOL:
            tool_mask_text(card->tool_mask, details, sizeof(details));
            (void)printf("修理工具牌：%s", details);
            break;
        case ACTION_MAP:
            (void)printf("地圖牌：私密查看一張終點牌");
            break;
        case ACTION_ROCKFALL:
            (void)printf("落石牌：移除一張普通坑道牌");
            break;
        default:
            (void)printf("未知行動牌");
            break;
    }
}

static const char *center_glyph_for_mask(uint8_t mask)
{
    switch (mask) {
        case PATH_EDGE_LEFT | PATH_EDGE_RIGHT: return "─";
        case PATH_EDGE_UP | PATH_EDGE_DOWN: return "│";
        case PATH_EDGE_UP | PATH_EDGE_RIGHT: return "└";
        case PATH_EDGE_RIGHT | PATH_EDGE_DOWN: return "┌";
        case PATH_EDGE_DOWN | PATH_EDGE_LEFT: return "┐";
        case PATH_EDGE_LEFT | PATH_EDGE_UP: return "┘";
        case PATH_EDGE_UP | PATH_EDGE_RIGHT | PATH_EDGE_DOWN: return "├";
        case PATH_EDGE_RIGHT | PATH_EDGE_DOWN | PATH_EDGE_LEFT: return "┬";
        case PATH_EDGE_DOWN | PATH_EDGE_LEFT | PATH_EDGE_UP: return "┤";
        case PATH_EDGE_LEFT | PATH_EDGE_UP | PATH_EDGE_RIGHT: return "┴";
        case PATH_EDGE_UP | PATH_EDGE_RIGHT | PATH_EDGE_DOWN | PATH_EDGE_LEFT: return "┼";
        case PATH_EDGE_UP: return "╵";
        case PATH_EDGE_RIGHT: return "╶";
        case PATH_EDGE_DOWN: return "╷";
        case PATH_EDGE_LEFT: return "╴";
        default: return "·";
    }
}

static void build_cell_lines(const Cell *cell, char lines[3][16])
{
    (void)snprintf(lines[0], 16, "   ");
    (void)snprintf(lines[1], 16, " . ");
    (void)snprintf(lines[2], 16, "   ");

    if (!cell->occupied) {
        return;
    }

    if (cell->is_goal && !cell->goal_revealed) {
        (void)snprintf(lines[1], 16, " ? ");
        return;
    }

    if (cell->is_goal) {
        (void)snprintf(lines[1], 16, cell->contains_gold ? " $ " : " X ");
        return;
    }

    if (cell->is_start) {
        (void)snprintf(lines[0], 16, " │ ");
        (void)snprintf(lines[1], 16, "─S─");
        (void)snprintf(lines[2], 16, " │ ");
        return;
    }

    const Card *card = &cell->card;
    uint8_t mask = card->edge_mask;

    if (!path_has_internal_connection(card)) {
        (void)snprintf(lines[0], 16, (mask & PATH_EDGE_UP) != 0u ? " ╷ " : "   ");
        (void)snprintf(lines[1], 16, "%s×%s",
                       (mask & PATH_EDGE_LEFT) != 0u ? "╴" : " ",
                       (mask & PATH_EDGE_RIGHT) != 0u ? "╶" : " ");
        (void)snprintf(lines[2], 16, (mask & PATH_EDGE_DOWN) != 0u ? " ╵ " : "   ");
        return;
    }

    (void)snprintf(lines[0], 16, (mask & PATH_EDGE_UP) != 0u ? " │ " : "   ");
    (void)snprintf(lines[1], 16, "%s%s%s",
                   (mask & PATH_EDGE_LEFT) != 0u ? "─" : " ",
                   center_glyph_for_mask(mask),
                   (mask & PATH_EDGE_RIGHT) != 0u ? "─" : " ");
    (void)snprintf(lines[2], 16, (mask & PATH_EDGE_DOWN) != 0u ? " │ " : "   ");
}

static void print_board(const GameState *game)
{
    int min_row = BOARD_H - 1;
    int max_row = 0;
    int min_col = BOARD_W - 1;
    int max_col = 0;

    for (int row = 0; row < BOARD_H; ++row) {
        for (int col = 0; col < BOARD_W; ++col) {
            if (!game->board[row][col].occupied) {
                continue;
            }
            if (row < min_row) min_row = row;
            if (row > max_row) max_row = row;
            if (col < min_col) min_col = col;
            if (col > max_col) max_col = col;
        }
    }

    if (min_row > 0) --min_row;
    if (max_row < BOARD_H - 1) ++max_row;
    if (min_col > 0) --min_col;
    if (max_col < BOARD_W - 1) ++max_col;

    (void)printf("\n礦坑版圖（座標格式：列 row、欄 col；每格以 3×3 顯示開口方向）\n      ");
    for (int col = min_col; col <= max_col; ++col) {
        (void)printf("%3d ", col);
    }
    (void)printf("\n");

    for (int row = min_row; row <= max_row; ++row) {
        char line_cache[BOARD_W][3][16];
        for (int col = min_col; col <= max_col; ++col) {
            build_cell_lines(&game->board[row][col], line_cache[col]);
        }

        for (int sub = 0; sub < 3; ++sub) {
            if (sub == 1) {
                (void)printf("%2d   ", row);
            } else {
                (void)printf("     ");
            }
            for (int col = min_col; col <= max_col; ++col) {
                (void)printf("%s ", line_cache[col][sub]);
            }
            (void)printf("\n");
        }
    }

    (void)printf("圖例：S=起點  ?=未翻終點  $=金礦  X=石頭  ×=死路中心\n");
    (void)printf("      ─│┌┐└┘├┤┬┴┼ 表示實際相通的坑道；╷╵╴╶ 表示死路出口方向。\n");
}

static void print_public_status(const GameState *game)
{
    (void)printf("\n第 %d / %d 輪｜抽牌堆剩餘 %d 張｜棄牌堆 %d 張\n",
                 game->round_number, MAX_ROUNDS,
                 game->draw_count, game->discard_count);
    (void)printf("公開工具狀態：\n");

    for (int i = 0; i < game->player_count; ++i) {
        const Player *player = &game->players[i];
        bool none = true;
        (void)printf("  %d. %-16s：", i + 1, player->name);
        for (int tool = 0; tool < TOOL_COUNT; ++tool) {
            if (player->broken_tools[tool]) {
                (void)printf("%s損壞(破壞牌#%d) ",
                             tool_name((ToolType)tool),
                             player->active_broken_tool_cards[tool].id);
                none = false;
            }
        }
        if (none) {
            (void)printf("工具正常");
        }
        (void)printf("\n");
    }

    print_public_log();
}

static void print_hand(const Player *player)
{
    (void)printf("\n你的手牌：\n");
    for (int i = 0; i < player->hand_count; ++i) {
        (void)printf("  %d. ", i + 1);
        print_card_description(&player->hand[i]);
        (void)printf("\n");
    }
}

static void show_turn_order(const GameState *game)
{
    clear_screen();
    (void)printf("=== 矮人礦坑：遊玩順序 ===\n\n");
    for (int i = 0; i < game->player_count; ++i) {
        (void)printf("%d. %s%s\n", i + 1, game->players[i].name,
                     i == game->starting_player ? "（第一輪起始玩家）" : "");
    }
    wait_for_enter("\n請記住順序，按 Enter 開始私密角色確認...");
}

static void run_role_check_phase(GameState *game)
{
    while (game->phase == PHASE_ROLE_CHECK) {
        int player_id = game_next_player_to_check_role(game);
        if (player_id == NO_PLAYER) {
            break;
        }

        clear_screen();
        (void)printf("=== 私密角色確認 ===\n\n");
        (void)printf("請將裝置交給：%s\n", game->players[player_id].name);
        (void)printf("其他玩家請勿觀看螢幕。\n");
        wait_for_enter("\n本人準備好後按 Enter 查看角色...");

        clear_screen();
        (void)printf("%s，你本輪的角色是：%s\n\n",
                     game->players[player_id].name,
                     role_name(game->players[player_id].role));
        if (game->players[player_id].role == ROLE_GOLD_DIGGER) {
            (void)printf("目標：連接起點與含有金礦的終點。\n");
        } else {
            (void)printf("目標：阻止淘金矮人找到金礦，並隱藏你的身分。\n");
        }
        wait_for_enter("\n確認後按 Enter 蓋牌...");
        clear_screen();
        mark_role_checked(game, player_id);
    }
}

static int ask_hand_index(const Player *player)
{
    return read_int_range("選擇手牌編號（輸入 0 取消）：", 0, player->hand_count) - 1;
}

static ToolType ask_tool(void)
{
    (void)printf("工具：1. 十字鎬  2. 礦燈  3. 推車\n");
    return (ToolType)(read_int_range("選擇工具：", 1, 3) - 1);
}

static bool attempt_path_action(GameState *game, int player_id)
{
    Player *player = &game->players[player_id];
    char reason[REASON_SIZE];

    print_hand(player);
    int hand_index = ask_hand_index(player);
    if (hand_index < 0) {
        return false;
    }
    if (player->hand[hand_index].kind != CARD_PATH) {
        (void)printf("這不是坑道牌。\n");
        return false;
    }

    int rotation = read_int_range("方向：0. 原方向  1. 旋轉 180 度：", 0, 1);
    Card preview = player->hand[hand_index];
    if (rotation == 1) {
        preview = rotate_path_card_180(preview);
    }
    (void)printf("準備放置：");
    print_card_description(&preview);
    (void)printf("\n");

    int row = read_int_range("放置 row（0-24）：", 0, BOARD_H - 1);
    int col = read_int_range("放置 col（0-24）：", 0, BOARD_W - 1);

    if (!play_path_card(game, player_id, hand_index, row, col,
                        rotation == 1, reason, sizeof(reason))) {
        (void)printf("無法放置：%s\n", reason);
        return false;
    }

    add_public_log("%s 在 (%d, %d) 放置一張坑道牌%s。",
                   player->name, row, col, rotation == 1 ? "（旋轉 180 度）" : "");
    (void)printf("%s\n", reason);
    return true;
}

static bool attempt_action_card(GameState *game, int player_id)
{
    Player *player = &game->players[player_id];
    char reason[REASON_SIZE];
    bool map_result = false;

    print_hand(player);
    int hand_index = ask_hand_index(player);
    if (hand_index < 0) {
        return false;
    }
    if (player->hand[hand_index].kind != CARD_ACTION) {
        (void)printf("這不是行動牌。\n");
        return false;
    }

    Card chosen = player->hand[hand_index];
    bool success = false;

    switch (chosen.action_kind) {
        case ACTION_BREAK_TOOL: {
            int target = read_int_range("破壞哪位玩家？輸入公開列表編號：",
                                        1, game->player_count) - 1;
            ToolType tool = ask_tool();
            success = play_break_tool(game, player_id, hand_index, target, tool,
                                      reason, sizeof(reason));
            if (success) {
                add_public_log("%s 破壞 %s 的%s。",
                               player->name, game->players[target].name,
                               tool_name(tool));
            }
            break;
        }
        case ACTION_REPAIR_TOOL: {
            int target = read_int_range("修理哪位玩家？輸入公開列表編號：",
                                        1, game->player_count) - 1;
            ToolType tool = ask_tool();
            success = play_repair_tool(game, player_id, hand_index, target, tool,
                                       reason, sizeof(reason));
            if (success) {
                add_public_log("%s 修理 %s 的%s。",
                               player->name, game->players[target].name,
                               tool_name(tool));
            }
            break;
        }
        case ACTION_MAP: {
            (void)printf("終點：1=上方、2=中間、3=下方\n");
            int goal = read_int_range("查看哪張終點牌？", 1, GOAL_CARD_COUNT) - 1;
            BoardPosition position = game_get_goal_position(goal);
            success = play_map_card(game, player_id, hand_index,
                                    position.row, position.col, &map_result,
                                    reason, sizeof(reason));
            if (success) {
                const char *goal_name = goal == 0 ? "上方" : (goal == 1 ? "中間" : "下方");
                add_public_log("%s 使用地圖牌查看%s終點（結果保密）。",
                               player->name, goal_name);
                (void)printf("\n私密資訊：該終點是%s。\n",
                             map_result ? "金礦" : "石頭");
            }
            break;
        }
        case ACTION_ROCKFALL: {
            int row = read_int_range("移除坑道 row（0-24）：", 0, BOARD_H - 1);
            int col = read_int_range("移除坑道 col（0-24）：", 0, BOARD_W - 1);
            success = play_rockfall_card(game, player_id, hand_index, row, col,
                                          reason, sizeof(reason));
            if (success) {
                add_public_log("%s 使用落石牌移除 (%d, %d) 的普通坑道牌。",
                               player->name, row, col);
            }
            break;
        }
        default:
            (void)snprintf(reason, sizeof(reason), "未知的行動牌。");
            success = false;
            break;
    }

    (void)printf("%s\n", reason);
    return success;
}

static bool attempt_discard(GameState *game, int player_id)
{
    Player *player = &game->players[player_id];
    char reason[REASON_SIZE];

    print_hand(player);
    int hand_index = ask_hand_index(player);
    if (hand_index < 0) {
        return false;
    }

    if (!discard_card(game, player_id, hand_index, reason, sizeof(reason))) {
        (void)printf("無法棄牌：%s\n", reason);
        return false;
    }
    add_public_log("%s 棄置一張牌（牌面保密）。", player->name);
    (void)printf("%s\n", reason);
    return true;
}

static void show_private_role_once(const Player *player)
{
    clear_screen();
    (void)printf("=== 私密角色確認：%s ===\n\n", player->name);
    (void)printf("你的角色是：%s\n\n", role_name(player->role));
    if (player->role == ROLE_GOLD_DIGGER) {
        (void)printf("目標：連接起點與含有金礦的終點。\n");
    } else {
        (void)printf("目標：阻止淘金矮人找到金礦，並隱藏你的身分。\n");
    }
    wait_for_enter("\n看完後按 Enter 隱藏角色資訊...");
    clear_screen();
}

static void take_private_turn(GameState *game)
{
    int player_id = game->current_player;
    Player *player = &game->players[player_id];
    bool completed = false;

    clear_screen();
    (void)printf("=== 輪到 %s ===\n\n", player->name);
    (void)printf("請將裝置交給該玩家，其他玩家請勿觀看手牌。\n");
    wait_for_enter("本人準備好後按 Enter 進入私人操作畫面...");

    while (!completed && game->phase == PHASE_PLAYER_TURN) {
        clear_screen();
        (void)printf("=== %s 的私人回合 ===\n", player->name);
        print_board(game);
        print_public_status(game);
        print_hand(player);
        (void)printf("\n操作：1. 放置坑道牌  2. 使用行動牌  3. 棄置一張牌  4. 短暫查看角色  5. 重新查看畫面\n");

        int choice = read_int_range("請選擇操作：", 1, 5);
        switch (choice) {
            case 1:
                completed = attempt_path_action(game, player_id);
                break;
            case 2:
                completed = attempt_action_card(game, player_id);
                break;
            case 3:
                completed = attempt_discard(game, player_id);
                break;
            case 4:
                show_private_role_once(player);
                continue;
            case 5:
                continue;
            default:
                break;
        }

        if (!completed) {
            wait_for_enter("\n按 Enter 回到私人操作畫面重新選擇...");
        }
    }

    if (completed) {
        wait_for_enter("\n回合完成。按 Enter 隱藏資訊並將裝置交回中央...");
        clear_screen();
    }
}

static void reveal_round_roles(const GameState *game)
{
    (void)printf("\n本輪角色公開：\n");
    for (int i = 0; i < game->player_count; ++i) {
        (void)printf("  %-16s：%s\n", game->players[i].name,
                     role_name(game->players[i].role));
    }
}

static void remove_drawn_value(int *values, int *count, int chosen_index)
{
    for (int i = chosen_index; i < *count - 1; ++i) {
        values[i] = values[i + 1];
    }
    --(*count);
}

static bool distribute_gold_miner_reward(GameState *game)
{
    int drawn[MAX_PLAYERS];
    int count = score_draw_cards_for_gold_miners(game, drawn, MAX_PLAYERS);
    int recipient = score_first_gold_miner_recipient(game);

    if (count <= 0 || recipient == NO_PLAYER) {
        return false;
    }

    (void)printf("\n淘金矮人成功找到金礦。每位本輪淘金矮人將私密選取一張金塊牌。\n");
    wait_for_enter("按 Enter 開始秘密分配金塊...");

    while (count > 0) {
        clear_screen();
        (void)printf("請將裝置交給淘金矮人：%s\n", game->players[recipient].name);
        wait_for_enter("本人準備好後按 Enter 查看可選金塊牌...");

        clear_screen();
        (void)printf("%s，請秘密選取一張金塊牌：\n", game->players[recipient].name);
        for (int i = 0; i < count; ++i) {
            (void)printf("  %d. %d 個金塊\n", i + 1, drawn[i]);
        }

        int selection = read_int_range("你的選擇：", 1, count) - 1;
        int selected_value = drawn[selection];
        if (!score_award_gold_card(game, recipient, selected_value)) {
            return false;
        }
        (void)printf("你獲得 %d 個金塊。請勿向其他玩家公開。\n", selected_value);
        remove_drawn_value(drawn, &count, selection);
        wait_for_enter("按 Enter 蓋住金塊資訊並傳給下一位玩家...");
        clear_screen();

        if (count > 0) {
            recipient = score_next_gold_miner_counterclockwise(game, recipient);
            if (recipient == NO_PLAYER) {
                return false;
            }
        }
    }

    return true;
}

static bool distribute_saboteur_reward(GameState *game)
{
    int awards[MAX_PLAYERS];
    if (!score_award_saboteur_round(game, awards)) {
        return false;
    }

    if (count_players_with_role(game, ROLE_SABOTEUR) == 0) {
        (void)printf("\n本輪未找到金礦，但沒有玩家抽到破壞者，因此不發金塊。\n");
        return true;
    }

    (void)printf("\n破壞者成功阻止淘金矮人。獎勵將私密通知破壞者。\n");
    wait_for_enter("按 Enter 開始秘密發放獎勵...");

    for (int i = 0; i < game->player_count; ++i) {
        if (awards[i] <= 0) {
            continue;
        }
        clear_screen();
        (void)printf("請將裝置交給：%s\n", game->players[i].name);
        wait_for_enter("本人準備好後按 Enter 查看獎勵...");
        clear_screen();
        (void)printf("你本輪以破壞者身分獲得 %d 個金塊。請保密。\n", awards[i]);
        wait_for_enter("按 Enter 蓋住獎勵資訊...");
    }
    clear_screen();
    return true;
}

static bool settle_round(GameState *game)
{
    clear_screen();
    (void)printf("=== 第 %d 輪結束 ===\n", game->round_number);
    print_board(game);
    reveal_round_roles(game);

    if (game->round_won_by_gold_diggers) {
        return distribute_gold_miner_reward(game);
    }
    return distribute_saboteur_reward(game);
}

static void print_final_result(const GameState *game)
{
    int highest = find_highest_score(game);

    clear_screen();
    (void)printf("=== 三輪結束：最終金塊總數 ===\n\n");
    for (int i = 0; i < game->player_count; ++i) {
        (void)printf("%-16s：%d 個金塊%s\n",
                     game->players[i].name,
                     game->players[i].gold_total,
                     game->players[i].gold_total == highest ? "  ← 勝者" : "");
    }
    (void)printf("\n最高分為 %d 個金塊。若多人同分，則共同獲勝。\n", highest);
}

static int ask_players_and_names(char names[MAX_PLAYERS][MAX_NAME_LEN])
{
    char input[INPUT_BUFFER_SIZE];
    int count = read_int_range("請輸入玩家人數（3-10）：", MIN_PLAYERS, MAX_PLAYERS);

    for (int i = 0; i < count; ++i) {
        for (;;) {
            (void)printf("玩家 %d 名稱：", i + 1);
            (void)fflush(stdout);
            if (!read_line(input, sizeof(input))) {
                exit(EXIT_SUCCESS);
            }
            if (input[0] == '\0') {
                (void)printf("名稱不可為空。\n");
                continue;
            }
            size_t name_length = strlen(input);
            if (name_length >= MAX_NAME_LEN) {
                (void)printf("名稱最多可輸入 %d 個位元組，請縮短名稱。\n",
                             MAX_NAME_LEN - 1);
                continue;
            }
            memcpy(names[i], input, name_length + 1u);
            break;
        }
    }

    return count;
}

int main(void)
{
    GameState game;
    char names[MAX_PLAYERS][MAX_NAME_LEN] = {{0}};

    clear_screen();
    (void)printf("========================================\n");
    (void)printf("       矮人礦坑 Saboteur 基礎版\n");
    (void)printf("          終端機輪流操作版本\n");
    (void)printf("========================================\n\n");

    int player_count = ask_players_and_names(names);
    game_seed_random((unsigned int)time(NULL));

    if (!game_init(&game, player_count,
                   (const char (*)[MAX_NAME_LEN])names)) {
        (void)fprintf(stderr, "遊戲初始化失敗：請確認名稱不可空白或重複。\n");
        return EXIT_FAILURE;
    }
    score_initialize_gold_deck(&game);
    reset_public_log();
    add_public_log("第 1 輪開始。所有玩家已重新發放角色與手牌。");
    show_turn_order(&game);

    while (game.phase != PHASE_GAME_RESULT) {
        run_role_check_phase(&game);

        while (game.phase == PHASE_PLAYER_TURN) {
            clear_screen();
            (void)printf("=== 公開畫面 ===\n");
            print_board(&game);
            print_public_status(&game);
            (void)printf("\n目前輪到：%s\n", game.players[game.current_player].name);
            wait_for_enter("按 Enter 進行裝置交接...");
            take_private_turn(&game);
        }

        if (game.phase != PHASE_ROUND_RESULT || !settle_round(&game)) {
            (void)fprintf(stderr, "結算發生錯誤，遊戲中止。\n");
            return EXIT_FAILURE;
        }

        if (!game_begin_next_round(&game)) {
            (void)fprintf(stderr, "無法開始下一輪，遊戲中止。\n");
            return EXIT_FAILURE;
        }

        if (game.phase != PHASE_GAME_RESULT) {
            reset_public_log();
            add_public_log("第 %d 輪開始。所有玩家已重新發放角色與手牌。",
                           game.round_number);
            wait_for_enter("\n金塊已分配完成。按 Enter 準備下一輪...");
        }
    }

    print_final_result(&game);
    return EXIT_SUCCESS;
}
