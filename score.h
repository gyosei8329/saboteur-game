/*
 * score.h
 * Saboteur / 矮人礦坑（基礎版）：金塊牌與三輪計分公開介面
 */

#ifndef SABOTEUR_SCORE_H
#define SABOTEUR_SCORE_H

#include "game.h"

#include <stdbool.h>
#include <stddef.h>

#define GOLD_VALUE_ONE_COUNT    16
#define GOLD_VALUE_TWO_COUNT     8
#define GOLD_VALUE_THREE_COUNT   4

/** 建立並洗亂整局共用的 28 張金塊牌堆。 */
void score_initialize_gold_deck(GameState *game);

/** 回傳本輪實際抽到 ROLE_GOLD_DIGGER 的玩家數。 */
int score_count_gold_miners(const GameState *game);

/**
 * 淘金矮人獲勝時，從金塊牌堆抽出與實際淘金矮人數相同的金塊牌。
 * 回傳實際抽出的張數；容量或遊戲狀態無效時回傳 0。
 */
int score_draw_cards_for_gold_miners(
    GameState *game,
    int *drawn_values,
    size_t capacity
);

/**
 * 回傳第一位選取金塊牌的淘金矮人。
 * 若完成金礦路線者是破壞者，則從其右側第一位淘金矮人開始。
 */
int score_first_gold_miner_recipient(const GameState *game);

/** 由目前玩家沿逆時鐘方向尋找下一位淘金矮人。 */
int score_next_gold_miner_counterclockwise(
    const GameState *game,
    int from_player
);

/** 將一張已抽出的金塊牌分配給指定淘金矮人。 */
bool score_award_gold_card(GameState *game, int player_id, int gold_value);

/**
 * 破壞者獲勝時分配固定價值金塊並從金塊牌堆移除對應價值。
 * awarded_values 若不為 NULL，會回填各玩家本輪取得的金塊數。
 */
bool score_award_saboteur_round(
    GameState *game,
    int awarded_values[MAX_PLAYERS]
);

#endif /* SABOTEUR_SCORE_H */
