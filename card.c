/*
 * card.c
 * Saboteur / 矮人礦坑（基礎版）數位化遊戲：卡牌建立與牌庫實作
 *
 * 規則張數依據：
 *   官方 AMIGO 基礎版規則確認共有 44 張坑道牌與 27 張行動牌，
 *   起點及 3 張終點先取出後，每輪抽牌堆使用 40 張普通坑道牌
 *   混合所有 27 張行動牌。
 *
 * 牌型資料說明：
 *   官方簡易規則頁確認總張數與規則，但沒有逐張列出 40 張普通坑道
 *   的每一種圖樣數量。本實作的坑道圖樣分布依公開牌型示意表建立；
 *   若專案需百分之百對齊你手上的特定出版版本，建議日後以實體牌
 *   逐張比對此檔的 add_connected_path() / add_dead_end_path() 清單。
 */

#include "card.h"

#include <stdlib.h>
#include <string.h>

/* ---------- 內部工具函式 ---------- */

static Card empty_card(int id, CardKind kind)
{
    Card card;
    memset(&card, 0, sizeof(card));
    card.id = id;
    card.kind = kind;
    card.action_kind = ACTION_NONE;
    return card;
}

static Card make_connected_path(int id, uint8_t edge_mask)
{
    Card card = empty_card(id, CARD_PATH);
    card.edge_mask = edge_mask;

    /*
     * 此類牌上的所有出口都在同一條連通坑道中：
     * 從任何出口進入，都能到達其他出口。
     */
    for (int side = 0; side < PATH_SIDE_COUNT; ++side) {
        uint8_t edge = (uint8_t)(1u << side);
        if ((edge_mask & edge) != 0u) {
            card.connected_mask[side] = (uint8_t)(edge_mask & (uint8_t)(~edge));
        }
    }

    return card;
}

static Card make_dead_end_path(int id, uint8_t edge_mask)
{
    Card card = empty_card(id, CARD_PATH);
    card.edge_mask = edge_mask;

    /*
     * 死路牌每一個出口皆通往封閉端，不會連到牌上的另一出口。
     * connected_mask 保持全為 0。
     */
    return card;
}

static Card make_action_card(int id, ActionKind action_kind, uint8_t tool_mask)
{
    Card card = empty_card(id, CARD_ACTION);
    card.action_kind = action_kind;
    card.tool_mask = tool_mask;
    return card;
}

static bool append_copies(
    Card *out_cards,
    size_t capacity,
    size_t *count,
    Card prototype,
    int *next_id,
    size_t copies
)
{
    if (out_cards == NULL || count == NULL || next_id == NULL
        || *count + copies > capacity) {
        return false;
    }

    for (size_t i = 0; i < copies; ++i) {
        prototype.id = (*next_id)++;
        out_cards[(*count)++] = prototype;
    }

    return true;
}

static bool append_connected_path(
    Card *out_cards,
    size_t capacity,
    size_t *count,
    int *next_id,
    uint8_t edge_mask,
    size_t copies
)
{
    return append_copies(
        out_cards, capacity, count,
        make_connected_path(0, edge_mask), next_id, copies
    );
}

static bool append_dead_end_path(
    Card *out_cards,
    size_t capacity,
    size_t *count,
    int *next_id,
    uint8_t edge_mask,
    size_t copies
)
{
    return append_copies(
        out_cards, capacity, count,
        make_dead_end_path(0, edge_mask), next_id, copies
    );
}

static bool append_action(
    Card *out_cards,
    size_t capacity,
    size_t *count,
    int *next_id,
    ActionKind action_kind,
    uint8_t tool_mask,
    size_t copies
)
{
    return append_copies(
        out_cards, capacity, count,
        make_action_card(0, action_kind, tool_mask), next_id, copies
    );
}

/* ---------- 特殊坑道牌 ---------- */

Card card_make_start(void)
{
    return make_connected_path(
        CARD_ID_START,
        PATH_EDGE_UP | PATH_EDGE_RIGHT | PATH_EDGE_DOWN | PATH_EDGE_LEFT
    );
}

void card_build_goal_cards(Card out_goal_cards[GOAL_CARD_COUNT])
{
    if (out_goal_cards == NULL) {
        return;
    }

    /*
     * 終點正面以三向連通坑道表示。終點是金礦或石頭應由 Cell 的
     * contains_gold 欄位記錄，避免玩家從 Card 型態提前取得答案。
     *
     * goal 被連通時，rule.c 可依實際接近方向旋轉 180 度。
     */
    const uint8_t goal_edges =
        PATH_EDGE_UP | PATH_EDGE_DOWN | PATH_EDGE_LEFT;

    for (int i = 0; i < GOAL_CARD_COUNT; ++i) {
        out_goal_cards[i] = make_connected_path(CARD_ID_GOAL_FIRST + i, goal_edges);
    }
}

/* ---------- 40 張普通坑道牌 ---------- */

size_t card_build_regular_path_deck(Card *out_cards, size_t capacity)
{
    if (out_cards == NULL || capacity < BASE_REGULAR_PATH_CARD_COUNT) {
        return 0;
    }

    size_t count = 0;
    int next_id = CARD_ID_REGULAR_PATH_FIRST;

    /*
     * A. 一條連通坑道的牌，共 33 張
     *
     * 單出口死路雖然沒有第二個出口可連通，但仍屬於可合法接上的
     * 普通坑道牌；它的 connected_mask 為 0。
     */
    if (!append_dead_end_path(out_cards, capacity, &count, &next_id,
                              PATH_EDGE_DOWN, 1)
        || !append_dead_end_path(out_cards, capacity, &count, &next_id,
                                 PATH_EDGE_LEFT, 1)

        /* 兩出口、真正相通：直線與轉彎 */
        || !append_connected_path(out_cards, capacity, &count, &next_id,
                                  PATH_EDGE_UP | PATH_EDGE_DOWN, 4)
        || !append_connected_path(out_cards, capacity, &count, &next_id,
                                  PATH_EDGE_LEFT | PATH_EDGE_RIGHT, 3)
        || !append_connected_path(out_cards, capacity, &count, &next_id,
                                  PATH_EDGE_RIGHT | PATH_EDGE_DOWN, 4)
        || !append_connected_path(out_cards, capacity, &count, &next_id,
                                  PATH_EDGE_DOWN | PATH_EDGE_LEFT, 5)

        /* 三出口、全部相通的 T 型 */
        || !append_connected_path(out_cards, capacity, &count, &next_id,
                                  PATH_EDGE_UP | PATH_EDGE_RIGHT | PATH_EDGE_DOWN, 5)
        || !append_connected_path(out_cards, capacity, &count, &next_id,
                                  PATH_EDGE_RIGHT | PATH_EDGE_DOWN | PATH_EDGE_LEFT, 5)

        /* 四出口、全部相通 */
        || !append_connected_path(out_cards, capacity, &count, &next_id,
                                  PATH_EDGE_UP | PATH_EDGE_RIGHT
                                  | PATH_EDGE_DOWN | PATH_EDGE_LEFT, 5)) {
        return 0;
    }

    /*
     * B. 多出口但彼此全為死路的牌，共 7 張
     *
     * 這些牌具有多個 edge，但從任何 edge 進入都不能抵達另一 edge。
     * 此處正是 game.h 中 connected_mask 設計的必要原因。
     */
    if (!append_dead_end_path(out_cards, capacity, &count, &next_id,
                              PATH_EDGE_UP | PATH_EDGE_DOWN, 1)
        || !append_dead_end_path(out_cards, capacity, &count, &next_id,
                                 PATH_EDGE_LEFT | PATH_EDGE_RIGHT, 1)
        || !append_dead_end_path(out_cards, capacity, &count, &next_id,
                                 PATH_EDGE_RIGHT | PATH_EDGE_DOWN, 1)
        || !append_dead_end_path(out_cards, capacity, &count, &next_id,
                                 PATH_EDGE_DOWN | PATH_EDGE_LEFT, 1)
        || !append_dead_end_path(out_cards, capacity, &count, &next_id,
                                 PATH_EDGE_UP | PATH_EDGE_RIGHT | PATH_EDGE_DOWN, 1)
        || !append_dead_end_path(out_cards, capacity, &count, &next_id,
                                 PATH_EDGE_RIGHT | PATH_EDGE_DOWN | PATH_EDGE_LEFT, 1)
        || !append_dead_end_path(out_cards, capacity, &count, &next_id,
                                 PATH_EDGE_UP | PATH_EDGE_RIGHT
                                 | PATH_EDGE_DOWN | PATH_EDGE_LEFT, 1)) {
        return 0;
    }

    return count == BASE_REGULAR_PATH_CARD_COUNT ? count : 0;
}

/* ---------- 27 張行動牌 ---------- */

size_t card_build_action_deck(Card *out_cards, size_t capacity)
{
    if (out_cards == NULL || capacity < BASE_ACTION_CARD_COUNT) {
        return 0;
    }

    size_t count = 0;
    int next_id = CARD_ID_ACTION_FIRST;

    /*
     * 破壞工具：各工具 3 張，共 9 張。
     */
    if (!append_action(out_cards, capacity, &count, &next_id,
                       ACTION_BREAK_TOOL, TOOL_MASK_PICKAXE, 3)
        || !append_action(out_cards, capacity, &count, &next_id,
                          ACTION_BREAK_TOOL, TOOL_MASK_LANTERN, 3)
        || !append_action(out_cards, capacity, &count, &next_id,
                          ACTION_BREAK_TOOL, TOOL_MASK_CART, 3)) {
        return 0;
    }

    /*
     * 修理工具：每種單工具修理各 2 張，以及三種雙工具組合各 1 張，
     * 共 9 張。雙工具修理牌使用時只修理其中一項。
     */
    if (!append_action(out_cards, capacity, &count, &next_id,
                       ACTION_REPAIR_TOOL, TOOL_MASK_PICKAXE, 2)
        || !append_action(out_cards, capacity, &count, &next_id,
                          ACTION_REPAIR_TOOL, TOOL_MASK_LANTERN, 2)
        || !append_action(out_cards, capacity, &count, &next_id,
                          ACTION_REPAIR_TOOL, TOOL_MASK_CART, 2)
        || !append_action(out_cards, capacity, &count, &next_id,
                          ACTION_REPAIR_TOOL,
                          TOOL_MASK_PICKAXE | TOOL_MASK_LANTERN, 1)
        || !append_action(out_cards, capacity, &count, &next_id,
                          ACTION_REPAIR_TOOL,
                          TOOL_MASK_PICKAXE | TOOL_MASK_CART, 1)
        || !append_action(out_cards, capacity, &count, &next_id,
                          ACTION_REPAIR_TOOL,
                          TOOL_MASK_LANTERN | TOOL_MASK_CART, 1)) {
        return 0;
    }

    /* 地圖牌 6 張、落石牌 3 張。 */
    if (!append_action(out_cards, capacity, &count, &next_id,
                       ACTION_MAP, 0u, BASE_MAP_CARD_COUNT)
        || !append_action(out_cards, capacity, &count, &next_id,
                          ACTION_ROCKFALL, 0u, BASE_ROCKFALL_CARD_COUNT)) {
        return 0;
    }

    return count == BASE_ACTION_CARD_COUNT ? count : 0;
}

size_t card_build_play_deck(Card *out_cards, size_t capacity)
{
    if (out_cards == NULL || capacity < BASE_PLAY_DECK_COUNT) {
        return 0;
    }

    size_t path_count =
        card_build_regular_path_deck(out_cards, BASE_REGULAR_PATH_CARD_COUNT);
    if (path_count != BASE_REGULAR_PATH_CARD_COUNT) {
        return 0;
    }

    size_t action_count =
        card_build_action_deck(out_cards + path_count, BASE_ACTION_CARD_COUNT);
    if (action_count != BASE_ACTION_CARD_COUNT) {
        return 0;
    }

    return path_count + action_count;
}

/* ---------- 洗牌 ---------- */

void shuffle_cards(Card *cards, int count)
{
    if (cards == NULL || count <= 1) {
        return;
    }

    for (int i = count - 1; i > 0; --i) {
        int swap_index = rand() % (i + 1);
        Card temporary = cards[i];
        cards[i] = cards[swap_index];
        cards[swap_index] = temporary;
    }
}

/* ---------- 內部一致性驗證 ---------- */

static bool path_card_data_is_valid(const Card *card)
{
    if (card == NULL || card->kind != CARD_PATH || card->action_kind != ACTION_NONE) {
        return false;
    }

    for (int side = 0; side < PATH_SIDE_COUNT; ++side) {
        uint8_t side_bit = (uint8_t)(1u << side);
        uint8_t connections = card->connected_mask[side];

        if ((card->edge_mask & side_bit) == 0u && connections != 0u) {
            return false;
        }
        if ((connections & (uint8_t)(~card->edge_mask)) != 0u) {
            return false;
        }
        if ((connections & side_bit) != 0u) {
            return false;
        }

        for (int target = 0; target < PATH_SIDE_COUNT; ++target) {
            uint8_t target_bit = (uint8_t)(1u << target);
            if ((connections & target_bit) != 0u
                && (card->connected_mask[target] & side_bit) == 0u) {
                return false;
            }
        }
    }

    return true;
}

static bool count_action_cards_is_valid(const Card *cards, size_t count)
{
    int break_pickaxe = 0;
    int break_lantern = 0;
    int break_cart = 0;
    int repairs = 0;
    int maps = 0;
    int rockfalls = 0;

    for (size_t i = 0; i < count; ++i) {
        const Card *card = &cards[i];
        if (card->kind != CARD_ACTION) {
            return false;
        }

        switch (card->action_kind) {
            case ACTION_BREAK_TOOL:
                if (card->tool_mask == TOOL_MASK_PICKAXE) {
                    ++break_pickaxe;
                } else if (card->tool_mask == TOOL_MASK_LANTERN) {
                    ++break_lantern;
                } else if (card->tool_mask == TOOL_MASK_CART) {
                    ++break_cart;
                } else {
                    return false;
                }
                break;

            case ACTION_REPAIR_TOOL:
                ++repairs;
                break;

            case ACTION_MAP:
                if (card->tool_mask != 0u) {
                    return false;
                }
                ++maps;
                break;

            case ACTION_ROCKFALL:
                if (card->tool_mask != 0u) {
                    return false;
                }
                ++rockfalls;
                break;

            default:
                return false;
        }
    }

    return break_pickaxe == 3
        && break_lantern == 3
        && break_cart == 3
        && repairs == BASE_REPAIR_TOOL_CARD_COUNT
        && maps == BASE_MAP_CARD_COUNT
        && rockfalls == BASE_ROCKFALL_CARD_COUNT;
}

bool card_base_deck_is_valid(void)
{
    Card start = card_make_start();
    Card goals[GOAL_CARD_COUNT];
    Card regular_paths[BASE_REGULAR_PATH_CARD_COUNT];
    Card actions[BASE_ACTION_CARD_COUNT];
    Card play_deck[BASE_PLAY_DECK_COUNT];

    card_build_goal_cards(goals);

    if (!path_card_data_is_valid(&start)) {
        return false;
    }

    for (int i = 0; i < GOAL_CARD_COUNT; ++i) {
        if (!path_card_data_is_valid(&goals[i])) {
            return false;
        }
    }

    if (card_build_regular_path_deck(
            regular_paths, BASE_REGULAR_PATH_CARD_COUNT)
        != BASE_REGULAR_PATH_CARD_COUNT) {
        return false;
    }

    for (size_t i = 0; i < BASE_REGULAR_PATH_CARD_COUNT; ++i) {
        if (!path_card_data_is_valid(&regular_paths[i])) {
            return false;
        }
    }

    if (card_build_action_deck(actions, BASE_ACTION_CARD_COUNT)
        != BASE_ACTION_CARD_COUNT
        || !count_action_cards_is_valid(actions, BASE_ACTION_CARD_COUNT)) {
        return false;
    }

    return card_build_play_deck(play_deck, BASE_PLAY_DECK_COUNT)
        == BASE_PLAY_DECK_COUNT;
}
