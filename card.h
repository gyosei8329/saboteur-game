/*
 * card.h
 * Saboteur / 矮人礦坑（基礎版）數位化遊戲：卡牌建立與牌庫介面
 *
 * 此模組只負責建立卡牌資料及洗牌，不負責：
 *   - 將卡牌發給玩家
 *   - 放置起點與終點到 board
 *   - 判斷出牌是否合法
 *   - 計分或 UI 顯示
 *
 * 基礎版玩法牌配置：
 *   - 44 張坑道牌 = 1 張起點 + 3 張終點 + 40 張普通坑道
 *   - 27 張行動牌
 *   - 實際抽牌堆 = 40 張普通坑道 + 27 張行動牌 = 67 張
 */

#ifndef SABOTEUR_CARD_H
#define SABOTEUR_CARD_H

#include "game.h"

#include <stdbool.h>
#include <stddef.h>

/* ---------- 基礎版牌張數 ---------- */

#define BASE_TOTAL_PATH_CARD_COUNT      44
#define BASE_START_CARD_COUNT            1
#define BASE_GOAL_CARD_COUNT             3
#define BASE_REGULAR_PATH_CARD_COUNT    40
#define BASE_ACTION_CARD_COUNT          27
#define BASE_PLAY_DECK_COUNT            67

/* 行動牌數量 */
#define BASE_BREAK_TOOL_CARD_COUNT       9
#define BASE_REPAIR_TOOL_CARD_COUNT      9
#define BASE_MAP_CARD_COUNT              6
#define BASE_ROCKFALL_CARD_COUNT         3

/* ---------- 固定特殊牌的 ID ---------- */

#define CARD_ID_START                    1
#define CARD_ID_GOAL_FIRST               2
#define CARD_ID_REGULAR_PATH_FIRST      10
#define CARD_ID_ACTION_FIRST           100

/* ---------- 特殊坑道牌 ---------- */

/**
 * 建立起點牌。
 *
 * 起點視為四個方向皆有出口且在牌內全部相通。
 */
Card card_make_start(void);

/**
 * 建立三張終點牌的坑道正面資料。
 *
 * 三張卡牌的 contains_gold 並不儲存在 Card 內；game.c 應在放入 Cell
 * 時隨機指定其中一格 Cell.contains_gold = true，另外兩格為 false。
 */
void card_build_goal_cards(Card out_goal_cards[GOAL_CARD_COUNT]);

/* ---------- 牌堆建立 ---------- */

/**
 * 建立 40 張可抽取的普通坑道牌。
 *
 * @return 成功時回傳 BASE_REGULAR_PATH_CARD_COUNT；容量不足時回傳 0。
 */
size_t card_build_regular_path_deck(Card *out_cards, size_t capacity);

/**
 * 建立 27 張行動牌。
 *
 * @return 成功時回傳 BASE_ACTION_CARD_COUNT；容量不足時回傳 0。
 */
size_t card_build_action_deck(Card *out_cards, size_t capacity);

/**
 * 建立每輪使用的 67 張抽牌堆：普通坑道牌與行動牌。
 *
 * 注意：本函式不洗牌；建立後請呼叫 shuffle_cards()。
 *
 * @return 成功時回傳 BASE_PLAY_DECK_COUNT；容量不足時回傳 0。
 */
size_t card_build_play_deck(Card *out_cards, size_t capacity);

/* ---------- 洗牌與測試 ---------- */

/**
 * 以 Fisher-Yates 演算法原地洗牌。
 *
 * 呼叫此函式前，應由 main.c 或 game.c 在遊戲啟動時呼叫一次：
 *
 *     srand((unsigned int)time(NULL));
 */
void shuffle_cards(Card *cards, int count);

/**
 * 驗證本模組建出的基礎牌堆張數、行動牌配置與坑道連通資料一致性。
 *
 * 此函式可供單元測試或開發期間 assert 使用。
 */
bool card_base_deck_is_valid(void);

#endif /* SABOTEUR_CARD_H */
