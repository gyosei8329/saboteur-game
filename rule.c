/*
 * rule.c
 * Saboteur / 矮人礦坑（基礎版）數位化遊戲：規則核心初稿
 *
 * 此檔負責：
 *   - 人數對應的角色池與初始手牌數
 *   - 私密角色確認狀態
 *   - 坑道牌放置合法性
 *   - 坑道連通搜尋與終點翻開判定
 *   - 基礎行動牌：破壞、修理、地圖、落石
 *   - 棄牌、補牌、輪到下一位、回合結束判定
 *   - 破壞者獲勝時的固定金塊計分
 *
 * 重要資料設計：
 *   單用 edge_mask（牌四邊是否有洞）不足以判斷連通，因為死路牌的
 *   多個出口可能在牌內部互不相連。因此 Card 需要：
 *
 *       uint8_t edge_mask;
 *       uint8_t connected_mask[4];
 *
 *   connected_mask[side] 表示：從該 side 進入卡牌後，可在卡牌內部
 *   抵達哪些其他 side。例如普通左右直線牌：
 *
 *       edge_mask = EDGE_LEFT | EDGE_RIGHT;
 *       connected_mask[SIDE_LEFT]  = EDGE_RIGHT;
 *       connected_mask[SIDE_RIGHT] = EDGE_LEFT;
 *
 *   若一張死路牌左、右都有開口但彼此不連通，則：
 *
 *       edge_mask = EDGE_LEFT | EDGE_RIGHT;
 *       connected_mask[SIDE_LEFT]  = 0;
 *       connected_mask[SIDE_RIGHT] = 0;
 *
 * 相依資料型別預期由 game.h 宣告；公開函式原型預期由 rule.h 宣告。
 */

#include "game.h"
#include "rule.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ---------- 本檔內部常數 ---------- */

enum {
    SIDE_UP = 0,
    SIDE_RIGHT = 1,
    SIDE_DOWN = 2,
    SIDE_LEFT = 3,
    SIDE_COUNT = 4
};

#define EDGE(side) ((uint8_t)(1u << (side)))

static const int ROW_DELTA[SIDE_COUNT] = {-1, 0, 1, 0};
static const int COL_DELTA[SIDE_COUNT] = {0, 1, 0, -1};
static const int OPPOSITE[SIDE_COUNT] = {
    SIDE_DOWN, SIDE_LEFT, SIDE_UP, SIDE_RIGHT
};

typedef struct {
    int row;
    int col;
    int side;
} PortalState;

/* ---------- 通用小工具 ---------- */

static bool is_valid_player_id(const GameState *game, int player_id)
{
    return game != NULL
        && player_id >= 0
        && player_id < game->player_count;
}

static bool is_in_board(int row, int col)
{
    return row >= 0 && row < BOARD_H && col >= 0 && col < BOARD_W;
}

static bool is_in_play_area(int row, int col)
{
    return row >= PLAY_AREA_ROW_MIN && row < PLAY_AREA_ROW_MAX
        && col >= PLAY_AREA_COL_MIN && col < PLAY_AREA_COL_MAX;
}

static bool is_tool_valid(ToolType tool)
{
    return tool == TOOL_PICKAXE
        || tool == TOOL_LANTERN
        || tool == TOOL_CART;
}

static uint8_t tool_bit(ToolType tool)
{
    return (uint8_t)(1u << (unsigned int)tool);
}

static bool card_has_edge(const Card *card, int side)
{
    return card != NULL
        && side >= 0
        && side < SIDE_COUNT
        && (card->edge_mask & EDGE(side)) != 0;
}

static bool is_goal_hidden(const Cell *cell)
{
    return cell != NULL && cell->is_goal && !cell->goal_revealed;
}

static bool is_traversable_cell(const Cell *cell)
{
    if (cell == NULL || !cell->occupied) {
        return false;
    }

    /* 尚未翻開的終點不能作為已知坑道穿越。 */
    return !is_goal_hidden(cell);
}

static bool is_path_card_in_hand(const Player *player, int hand_index)
{
    return player != NULL
        && hand_index >= 0
        && hand_index < player->hand_count
        && player->hand[hand_index].kind == CARD_PATH;
}

static bool is_action_card_in_hand(
    const Player *player,
    int hand_index,
    ActionKind action_kind
)
{
    return player != NULL
        && hand_index >= 0
        && hand_index < player->hand_count
        && player->hand[hand_index].kind == CARD_ACTION
        && player->hand[hand_index].action_kind == action_kind;
}

static bool is_current_turn(const GameState *game, int player_id)
{
    return is_valid_player_id(game, player_id)
        && game->phase == PHASE_PLAYER_TURN
        && game->current_player == player_id;
}

static void set_reason(char *reason, size_t reason_size, const char *text)
{
    if (reason == NULL || reason_size == 0) {
        return;
    }

    (void)snprintf(reason, reason_size, "%s", text);
}

static size_t discard_capacity(const GameState *game)
{
    return sizeof(game->discard_pile) / sizeof(game->discard_pile[0]);
}

static bool discard_has_space_for(const GameState *game, size_t cards_needed)
{
    return game != NULL
        && game->discard_count >= 0
        && (size_t)game->discard_count + cards_needed <= discard_capacity(game);
}

static bool discard_has_space(const GameState *game)
{
    return discard_has_space_for(game, 1u);
}

static Card remove_card_from_hand(Player *player, int hand_index)
{
    Card removed = player->hand[hand_index];

    for (int i = hand_index; i < player->hand_count - 1; ++i) {
        player->hand[i] = player->hand[i + 1];
    }

    player->hand_count--;
    return removed;
}

static void move_used_card_to_discard(GameState *game, Card card)
{
    game->discard_pile[game->discard_count++] = card;
}

static uint8_t rotate_edge_mask_180(uint8_t mask)
{
    uint8_t rotated = 0;

    for (int side = 0; side < SIDE_COUNT; ++side) {
        if ((mask & EDGE(side)) != 0) {
            rotated |= EDGE(OPPOSITE[side]);
        }
    }

    return rotated;
}

/* ---------- 遊戲設置規則 ---------- */

int initial_hand_size(int player_count)
{
    if (player_count >= 3 && player_count <= 5) {
        return 6;
    }
    if (player_count >= 6 && player_count <= 7) {
        return 5;
    }
    if (player_count >= 8 && player_count <= 10) {
        return 4;
    }

    return 0;
}

bool build_role_pool(int player_count, Role *pool, int *pool_count)
{
    int gold_diggers = 0;
    int saboteurs = 0;

    if (pool == NULL || pool_count == NULL) {
        return false;
    }

    switch (player_count) {
        case 3:  gold_diggers = 3; saboteurs = 1; break;
        case 4:  gold_diggers = 4; saboteurs = 1; break;
        case 5:  gold_diggers = 4; saboteurs = 2; break;
        case 6:  gold_diggers = 5; saboteurs = 2; break;
        case 7:  gold_diggers = 5; saboteurs = 3; break;
        case 8:  gold_diggers = 6; saboteurs = 3; break;
        case 9:  gold_diggers = 7; saboteurs = 3; break;
        case 10: gold_diggers = 7; saboteurs = 4; break;
        default:
            *pool_count = 0;
            return false;
    }

    int index = 0;
    for (int i = 0; i < gold_diggers; ++i) {
        pool[index++] = ROLE_GOLD_DIGGER;
    }
    for (int i = 0; i < saboteurs; ++i) {
        pool[index++] = ROLE_SABOTEUR;
    }

    *pool_count = index; /* 此數量會比實際玩家數多一張。 */
    return true;
}

/* ---------- 私密角色查看階段 ---------- */

void mark_role_checked(GameState *game, int player_id)
{
    if (!is_valid_player_id(game, player_id)
        || game->phase != PHASE_ROLE_CHECK) {
        return;
    }

    game->players[player_id].checked_role = true;

    if (all_players_checked_role(game)) {
        game->phase = PHASE_PLAYER_TURN;
        game->current_player = game->starting_player;
    }
}

bool all_players_checked_role(const GameState *game)
{
    if (game == NULL || game->player_count <= 0) {
        return false;
    }

    for (int i = 0; i < game->player_count; ++i) {
        if (!game->players[i].checked_role) {
            return false;
        }
    }

    return true;
}

/* ---------- 坑道牌旋轉與放置合法性 ---------- */

Card rotate_path_card_180(Card card)
{
    if (card.kind != CARD_PATH) {
        return card;
    }

    Card rotated = card;
    rotated.edge_mask = rotate_edge_mask_180(card.edge_mask);

    for (int new_side = 0; new_side < SIDE_COUNT; ++new_side) {
        int old_side = OPPOSITE[new_side];
        rotated.connected_mask[new_side] =
            rotate_edge_mask_180(card.connected_mask[old_side]);
    }

    return rotated;
}

bool player_has_broken_tool(const GameState *game, int player_id)
{
    if (!is_valid_player_id(game, player_id)) {
        return false;
    }

    const Player *player = &game->players[player_id];
    for (int tool = 0; tool < 3; ++tool) {
        if (player->broken_tools[tool]) {
            return true;
        }
    }

    return false;
}

bool can_play_path_card(const GameState *game, int player_id)
{
    return is_current_turn(game, player_id)
        && !player_has_broken_tool(game, player_id);
}

/*
 * 此搜尋函式實作位於下方；放置合法性需先查詢既有版圖中
 * 哪些坑道出口已能由起點抵達。
 */
static void compute_reachable_ports(
    const GameState *game,
    bool reached[BOARD_H][BOARD_W][SIDE_COUNT]
);

bool is_valid_path_placement(
    const GameState *game,
    Card card,
    int row,
    int col,
    char *reason,
    size_t reason_size
)
{
    if (game == NULL) {
        set_reason(reason, reason_size, "遊戲資料不存在。");
        return false;
    }
    if (card.kind != CARD_PATH) {
        set_reason(reason, reason_size, "選取的牌不是坑道牌。");
        return false;
    }
    if (!is_in_board(row, col)) {
        set_reason(reason, reason_size, "放置位置超出版圖範圍。");
        return false;
    }
    if (!is_in_play_area(row, col)) {
        set_reason(reason, reason_size, "放置位置不在 9x5 場地內。");
        return false;
    }
    if (game->board[row][col].occupied) {
        set_reason(reason, reason_size, "該位置已經有卡牌。");
        return false;
    }

    /*
     * 官方規則要求新放置的坑道必須延伸自起點形成的不間斷路徑。
     * 先只針對「放牌前」的既有版圖計算可由起點抵達的出口。
     */
    bool reached[BOARD_H][BOARD_W][SIDE_COUNT];
    compute_reachable_ports(game, reached);

    bool has_existing_path_neighbor = false;
    bool joins_reachable_path_from_start = false;

    for (int side = 0; side < SIDE_COUNT; ++side) {
        int neighbor_row = row + ROW_DELTA[side];
        int neighbor_col = col + COL_DELTA[side];

        if (!is_in_board(neighbor_row, neighbor_col)) {
            continue;
        }

        const Cell *neighbor = &game->board[neighbor_row][neighbor_col];

        if (!neighbor->occupied || is_goal_hidden(neighbor)) {
            /*
             * 面朝下的終點牌尚未公開通道方向：
             * 玩家可放到終點旁邊，真正從起點連通時再翻開終點。
             */
            continue;
        }

        has_existing_path_neighbor = true;

        bool new_card_open = card_has_edge(&card, side);
        bool neighbor_open = card_has_edge(&neighbor->card, OPPOSITE[side]);

        if (new_card_open != neighbor_open) {
            set_reason(reason, reason_size,
                       "坑道接邊不吻合：通道必須接通道，牆面必須接牆面。");
            return false;
        }

        /*
         * 有出口相接仍不夠：鄰牌面向新牌的出口必須已可由起點抵達。
         * 此判斷會拒絕接在落石後形成的孤立礦道旁，也會拒絕只用牆面
         * 貼住現有卡牌而未延伸實際坑道的放法。
         */
        if (new_card_open
            && neighbor_open
            && reached[neighbor_row][neighbor_col][OPPOSITE[side]]) {
            joins_reachable_path_from_start = true;
        }
    }

    if (!has_existing_path_neighbor) {
        set_reason(reason, reason_size, "坑道牌必須與既有坑道牌相鄰。");
        return false;
    }

    if (!joins_reachable_path_from_start) {
        set_reason(reason, reason_size,
                   "新坑道必須透過通道連回起點，不可只以牆面相鄰或接在孤立坑道旁。");
        return false;
    }

    set_reason(reason, reason_size, "可放置：坑道可連回起點。");
    return true;
}

/* ---------- 從起點出發的真正連通搜尋 ---------- */

static bool find_start_cell(const GameState *game, int *out_row, int *out_col)
{
    for (int row = 0; row < BOARD_H; ++row) {
        for (int col = 0; col < BOARD_W; ++col) {
            if (game->board[row][col].occupied
                && game->board[row][col].is_start) {
                *out_row = row;
                *out_col = col;
                return true;
            }
        }
    }

    return false;
}

static void enqueue_portal(
    PortalState *queue,
    int *tail,
    bool reached[BOARD_H][BOARD_W][SIDE_COUNT],
    int row,
    int col,
    int side
)
{
    if (!reached[row][col][side]) {
        reached[row][col][side] = true;
        queue[(*tail)++] = (PortalState){row, col, side};
    }
}

static void compute_reachable_ports(
    const GameState *game,
    bool reached[BOARD_H][BOARD_W][SIDE_COUNT]
)
{
    memset(reached, 0, sizeof(bool) * BOARD_H * BOARD_W * SIDE_COUNT);

    int start_row = 0;
    int start_col = 0;
    if (!find_start_cell(game, &start_row, &start_col)) {
        return;
    }

    PortalState queue[BOARD_H * BOARD_W * SIDE_COUNT];
    int head = 0;
    int tail = 0;

    const Card *start_card = &game->board[start_row][start_col].card;

    /*
     * 起點視為礦工所在位置，起點牌的所有出口均從一開始可抵達。
     */
    for (int side = 0; side < SIDE_COUNT; ++side) {
        if (card_has_edge(start_card, side)) {
            enqueue_portal(queue, &tail, reached, start_row, start_col, side);
        }
    }

    while (head < tail) {
        PortalState current = queue[head++];
        const Cell *cell = &game->board[current.row][current.col];

        /*
         * 1. 沿同一張牌的內部真實通道移動。
         *    死路牌 connected_mask 為 0，搜尋不會穿過該牌。
         */
        uint8_t internal_targets = cell->card.connected_mask[current.side];
        for (int next_side = 0; next_side < SIDE_COUNT; ++next_side) {
            if ((internal_targets & EDGE(next_side)) != 0) {
                enqueue_portal(queue, &tail, reached,
                               current.row, current.col, next_side);
            }
        }

        /*
         * 2. 從目前已抵達的出口穿越至相鄰已翻開坑道。
         *    尚未翻開的終點不在此處穿越；另由 reveal 函式處理。
         */
        int neighbor_row = current.row + ROW_DELTA[current.side];
        int neighbor_col = current.col + COL_DELTA[current.side];

        if (!is_in_board(neighbor_row, neighbor_col)) {
            continue;
        }

        const Cell *neighbor = &game->board[neighbor_row][neighbor_col];
        int entering_side = OPPOSITE[current.side];

        if (is_traversable_cell(neighbor)
            && card_has_edge(&neighbor->card, entering_side)) {
            enqueue_portal(queue, &tail, reached,
                           neighbor_row, neighbor_col, entering_side);
        }
    }
}

static bool neighbor_path_reaches_goal(
    const GameState *game,
    bool reached[BOARD_H][BOARD_W][SIDE_COUNT],
    int goal_row,
    int goal_col,
    int *out_goal_entry_side
)
{
    for (int goal_side = 0; goal_side < SIDE_COUNT; ++goal_side) {
        int neighbor_row = goal_row + ROW_DELTA[goal_side];
        int neighbor_col = goal_col + COL_DELTA[goal_side];

        if (!is_in_board(neighbor_row, neighbor_col)) {
            continue;
        }

        const Cell *neighbor = &game->board[neighbor_row][neighbor_col];
        int neighbor_side_facing_goal = OPPOSITE[goal_side];

        if (is_traversable_cell(neighbor)
            && card_has_edge(&neighbor->card, neighbor_side_facing_goal)
            && reached[neighbor_row][neighbor_col][neighbor_side_facing_goal]) {
            *out_goal_entry_side = goal_side;
            return true;
        }
    }

    return false;
}

static void orient_revealed_goal_for_entry(Cell *goal, int entry_side)
{
    /*
     * 終點翻開後，若目前方向無法接上到達的坑道，將終點牌旋轉 180 度。
     * 卡牌圖樣資料應確保原方向或 180 度方向至少有一個可配合入口。
     */
    if (!card_has_edge(&goal->card, entry_side)) {
        Card rotated = rotate_path_card_180(goal->card);
        if (card_has_edge(&rotated, entry_side)) {
            goal->card = rotated;
        }
    }
}

static void reveal_reached_goals_and_update_result(
    GameState *game,
    int connecting_player
)
{
    bool newly_revealed = false;

    do {
        newly_revealed = false;

        bool reached[BOARD_H][BOARD_W][SIDE_COUNT];
        compute_reachable_ports(game, reached);

        for (int row = 0; row < BOARD_H; ++row) {
            for (int col = 0; col < BOARD_W; ++col) {
                Cell *goal = &game->board[row][col];

                if (!goal->occupied || !is_goal_hidden(goal)) {
                    continue;
                }

                int entry_side = SIDE_LEFT;
                if (!neighbor_path_reaches_goal(
                        game, reached, row, col, &entry_side)) {
                    continue;
                }

                goal->goal_revealed = true;
                orient_revealed_goal_for_entry(goal, entry_side);
                newly_revealed = true;

                if (goal->contains_gold) {
                    game->round_won_by_gold_diggers = true;
                    game->last_player_connected_goal = connecting_player;
                    return;
                }
            }
        }
    } while (newly_revealed);
}

/* ---------- 完成動作、抽牌與換回合 ---------- */

void draw_card_if_available(GameState *game, int player_id)
{
    if (!is_valid_player_id(game, player_id)
        || game->draw_count <= 0) {
        return;
    }

    Player *player = &game->players[player_id];

    if (player->hand_count >= MAX_HAND_SIZE) {
        /*
         * 正常流程不應發生；保留防呆避免陣列超界。
         * 若未來加入特殊規則，可改為動態手牌容器。
         */
        return;
    }

    player->hand[player->hand_count++] = game->draw_pile[--game->draw_count];
}

void advance_turn(GameState *game)
{
    if (game == NULL
        || game->phase != PHASE_PLAYER_TURN
        || game->player_count <= 0) {
        return;
    }

    game->current_player = (game->current_player + 1) % game->player_count;
}

bool is_round_over(const GameState *game)
{
    if (game == NULL) {
        return false;
    }

    if (game->round_won_by_gold_diggers) {
        return true;
    }

    if (game->draw_count > 0) {
        return false;
    }

    for (int i = 0; i < game->player_count; ++i) {
        if (game->players[i].hand_count > 0) {
            return false;
        }
    }

    return true;
}

static void finish_successful_action(GameState *game, int player_id)
{
    /* 供下一輪決定起始玩家；所有成功的出牌或棄牌都會更新。 */
    game->last_action_player = player_id;
    draw_card_if_available(game, player_id);

    if (is_round_over(game)) {
        game->phase = PHASE_ROUND_RESULT;
    } else {
        advance_turn(game);
    }
}

/* ---------- 坑道牌出牌 ---------- */

bool play_path_card(
    GameState *game,
    int player_id,
    int hand_index,
    int row,
    int col,
    bool rotated_180,
    char *reason,
    size_t reason_size
)
{
    if (!is_current_turn(game, player_id)) {
        set_reason(reason, reason_size, "目前不是該玩家的回合。");
        return false;
    }
    if (!can_play_path_card(game, player_id)) {
        set_reason(reason, reason_size, "工具損壞時不可放置坑道牌。");
        return false;
    }

    Player *player = &game->players[player_id];

    if (!is_path_card_in_hand(player, hand_index)) {
        set_reason(reason, reason_size, "選取的手牌不是坑道牌。");
        return false;
    }
    Card card = player->hand[hand_index];
    if (rotated_180) {
        card = rotate_path_card_180(card);
    }

    if (!is_valid_path_placement(game, card, row, col, reason, reason_size)) {
        return false;
    }

    Cell *target = &game->board[row][col];
    memset(target, 0, sizeof(*target));
    target->occupied = true;
    target->card = card;

    /* 坑道牌留在版圖上，不進入棄牌堆。 */
    (void)remove_card_from_hand(player, hand_index);

    reveal_reached_goals_and_update_result(game, player_id);
    finish_successful_action(game, player_id);

    set_reason(reason, reason_size, "坑道牌已放置。");
    return true;
}

/* ---------- 工具破壞與修理 ---------- */

bool can_break_tool(
    const GameState *game,
    int target_player,
    ToolType tool
)
{
    return is_valid_player_id(game, target_player)
        && is_tool_valid(tool)
        && !game->players[target_player].broken_tools[tool];
}

bool play_break_tool(
    GameState *game,
    int player_id,
    int hand_index,
    int target_player,
    ToolType tool,
    char *reason,
    size_t reason_size
)
{
    if (!is_current_turn(game, player_id)) {
        set_reason(reason, reason_size, "目前不是該玩家的回合。");
        return false;
    }

    Player *player = &game->players[player_id];
    if (!is_action_card_in_hand(player, hand_index, ACTION_BREAK_TOOL)) {
        set_reason(reason, reason_size, "選取的手牌不是破壞工具牌。");
        return false;
    }
    if (!is_tool_valid(tool)
        || (player->hand[hand_index].tool_mask & tool_bit(tool)) == 0) {
        set_reason(reason, reason_size, "這張牌不能破壞所選工具。");
        return false;
    }
    if (!can_break_tool(game, target_player, tool)) {
        set_reason(reason, reason_size, "指定玩家的該工具已損壞，不能重複破壞。");
        return false;
    }
    Card sabotage_card = remove_card_from_hand(player, hand_index);
    game->players[target_player].broken_tools[tool] = true;
    game->players[target_player].active_broken_tool_cards[tool] = sabotage_card;
    finish_successful_action(game, player_id);

    set_reason(reason, reason_size, "工具破壞成功，破壞牌已公開放在目標玩家面前。");
    return true;
}

bool can_repair_tool(
    const GameState *game,
    int target_player,
    ToolType tool
)
{
    return is_valid_player_id(game, target_player)
        && is_tool_valid(tool)
        && game->players[target_player].broken_tools[tool];
}

bool play_repair_tool(
    GameState *game,
    int player_id,
    int hand_index,
    int target_player,
    ToolType tool,
    char *reason,
    size_t reason_size
)
{
    if (!is_current_turn(game, player_id)) {
        set_reason(reason, reason_size, "目前不是該玩家的回合。");
        return false;
    }

    Player *player = &game->players[player_id];
    if (!is_action_card_in_hand(player, hand_index, ACTION_REPAIR_TOOL)) {
        set_reason(reason, reason_size, "選取的手牌不是修理工具牌。");
        return false;
    }
    if (!is_tool_valid(tool)
        || (player->hand[hand_index].tool_mask & tool_bit(tool)) == 0) {
        set_reason(reason, reason_size, "這張牌不能修理所選工具。");
        return false;
    }
    if (!can_repair_tool(game, target_player, tool)) {
        set_reason(reason, reason_size, "指定玩家沒有該項損壞工具。");
        return false;
    }
    if (!discard_has_space_for(game, 2u)) {
        set_reason(reason, reason_size,
                   "棄牌堆空間不足，無法同時棄置修理牌與被修理的破壞牌。");
        return false;
    }

    Card broken_card = game->players[target_player].active_broken_tool_cards[tool];
    Card repair_card = remove_card_from_hand(player, hand_index);

    game->players[target_player].broken_tools[tool] = false;
    memset(&game->players[target_player].active_broken_tool_cards[tool],
           0, sizeof(game->players[target_player].active_broken_tool_cards[tool]));

    move_used_card_to_discard(game, broken_card);
    move_used_card_to_discard(game, repair_card);
    finish_successful_action(game, player_id);

    set_reason(reason, reason_size, "工具修理成功，修理牌與對應破壞牌已一起棄置。");
    return true;
}

/* ---------- 地圖牌與落石牌 ---------- */

bool play_map_card(
    GameState *game,
    int player_id,
    int hand_index,
    int goal_row,
    int goal_col,
    bool *out_contains_gold,
    char *reason,
    size_t reason_size
)
{
    if (!is_current_turn(game, player_id)) {
        set_reason(reason, reason_size, "目前不是該玩家的回合。");
        return false;
    }
    if (out_contains_gold == NULL) {
        set_reason(reason, reason_size, "缺少私人查看結果的輸出位置。");
        return false;
    }

    Player *player = &game->players[player_id];
    if (!is_action_card_in_hand(player, hand_index, ACTION_MAP)) {
        set_reason(reason, reason_size, "選取的手牌不是地圖牌。");
        return false;
    }
    if (!is_in_board(goal_row, goal_col)
        || !game->board[goal_row][goal_col].occupied
        || !game->board[goal_row][goal_col].is_goal
        || game->board[goal_row][goal_col].goal_revealed) {
        set_reason(reason, reason_size, "請選擇一張尚未翻開的終點牌。");
        return false;
    }
    if (!discard_has_space(game)) {
        set_reason(reason, reason_size, "棄牌堆已滿，無法完成動作。");
        return false;
    }

    /*
     * 此值只能由 UI 私密畫面短暫顯示給 player_id；
     * 絕不可寫入公開出牌紀錄。
     */
    *out_contains_gold = game->board[goal_row][goal_col].contains_gold;

    move_used_card_to_discard(game, remove_card_from_hand(player, hand_index));
    finish_successful_action(game, player_id);

    set_reason(reason, reason_size, "地圖牌已使用，請私密顯示查看結果後立即遮蔽。");
    return true;
}

bool play_rockfall_card(
    GameState *game,
    int player_id,
    int hand_index,
    int row,
    int col,
    char *reason,
    size_t reason_size
)
{
    if (!is_current_turn(game, player_id)) {
        set_reason(reason, reason_size, "目前不是該玩家的回合。");
        return false;
    }

    Player *player = &game->players[player_id];
    if (!is_action_card_in_hand(player, hand_index, ACTION_ROCKFALL)) {
        set_reason(reason, reason_size, "選取的手牌不是落石牌。");
        return false;
    }
    if (!is_in_board(row, col)) {
        set_reason(reason, reason_size, "選取位置超出版圖範圍。");
        return false;
    }

    const Cell *target = &game->board[row][col];
    if (!target->occupied
        || target->is_start
        || target->is_goal
        || target->card.kind != CARD_PATH) {
        set_reason(reason, reason_size, "落石只能移除普通坑道牌，不可移除起點或終點。");
        return false;
    }
    if (!discard_has_space_for(game, 2u)) {
        set_reason(reason, reason_size, "棄牌堆空間不足，無法移除坑道牌。");
        return false;
    }

    Card removed_path = game->board[row][col].card;
    memset(&game->board[row][col], 0, sizeof(game->board[row][col]));
    move_used_card_to_discard(game, removed_path);
    move_used_card_to_discard(game, remove_card_from_hand(player, hand_index));
    finish_successful_action(game, player_id);

    set_reason(reason, reason_size, "坑道牌已被落石移除。");
    return true;
}

/* ---------- 棄牌 ---------- */

bool discard_card(
    GameState *game,
    int player_id,
    int hand_index,
    char *reason,
    size_t reason_size
)
{
    if (!is_current_turn(game, player_id)) {
        set_reason(reason, reason_size, "目前不是該玩家的回合。");
        return false;
    }

    Player *player = &game->players[player_id];
    if (hand_index < 0 || hand_index >= player->hand_count) {
        set_reason(reason, reason_size, "未選取有效手牌。");
        return false;
    }
    if (!discard_has_space(game)) {
        set_reason(reason, reason_size, "棄牌堆已滿，無法完成動作。");
        return false;
    }

    /*
     * UI 公開紀錄只應顯示「玩家棄了一張牌」，不可顯示牌面內容。
     */
    move_used_card_to_discard(game, remove_card_from_hand(player, hand_index));
    finish_successful_action(game, player_id);

    set_reason(reason, reason_size, "已棄置一張手牌。");
    return true;
}

/* ---------- 結算輔助 ---------- */

int count_players_with_role(const GameState *game, Role role)
{
    if (game == NULL) {
        return 0;
    }

    int count = 0;
    for (int i = 0; i < game->player_count; ++i) {
        if (game->players[i].role == role) {
            ++count;
        }
    }

    return count;
}

int saboteur_reward_per_player(int saboteur_count)
{
    switch (saboteur_count) {
        case 1: return 4;
        case 2:
        case 3: return 3;
        case 4: return 2;
        default: return 0;
    }
}

bool award_saboteur_score_if_applicable(GameState *game)
{
    if (game == NULL
        || game->phase != PHASE_ROUND_RESULT
        || game->round_won_by_gold_diggers) {
        return false;
    }

    int saboteur_count = count_players_with_role(game, ROLE_SABOTEUR);
    int reward = saboteur_reward_per_player(saboteur_count);

    if (reward == 0) {
        /*
         * 例如低人數局剛好沒有玩家抽到破壞者：
         * 淘金矮人未成功時也不會有人獲得金塊。
         */
        return true;
    }

    for (int i = 0; i < game->player_count; ++i) {
        if (game->players[i].role == ROLE_SABOTEUR) {
            game->players[i].gold_total += reward;
        }
    }

    return true;
}

bool is_game_over(const GameState *game)
{
    return game != NULL
        && game->phase == PHASE_GAME_RESULT
        && game->round_number >= 3;
}

int find_highest_score(const GameState *game)
{
    if (game == NULL || game->player_count <= 0) {
        return 0;
    }

    int highest = game->players[0].gold_total;
    for (int i = 1; i < game->player_count; ++i) {
        if (game->players[i].gold_total > highest) {
            highest = game->players[i].gold_total;
        }
    }

    return highest;
}

/*
 * 尚未放入本檔的部分：
 *
 * 1. setup_round()
 *    需要 card.c 提供完整玩法牌清單、終點牌及洗牌函式。
 *
 * 2. 淘金矮人成功後的金塊選牌分配
 *    其流程需要 GoldCard 與 UI 選牌狀態，建議放到 score.c。
 *
 * 3. 公開事件文字與私密畫面遮罩
 *    屬於 UI Layer，rule.c 只回傳成功/失敗及提示文字。
 */
