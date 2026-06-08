/*
 * extras.h
 * Saboteur / 矮人礦坑 Phase 1 額外功能
 *
 * 這個檔案目前只放人機模式專用的簡易 AI。
 * AI 模式仍使用原版 Miner / Saboteur 身分。
 */

#ifndef SABOTEUR_EXTRAS_H
#define SABOTEUR_EXTRAS_H

#include "game.h"

#include <stdbool.h>
#include <stddef.h>

/* ---------- 人機模式 AI ---------- */

/**
 * 讓目前 current_player 由 AI 控制者執行一個回合。
 *
 * 策略很簡單（Phase 1）：
 *   1. 礦工角色：找一個能讓路徑往金礦方向延伸的放牌動作。
 *   2. 破壞者角色：找一個能擺死路或遠離金礦的放牌動作。
 *   3. 找不到合法放牌就棄一張牌。
 *
 * 函式只負責出牌；UI 動畫、提示與下一輪切換交由呼叫端處理。
 */
void ai_take_turn(GameState *game);

/**
 * 與 ai_take_turn 相同，但成功行動時會回填公開日誌文字。
 */
bool ai_take_turn_with_log(GameState *game, char *log, size_t log_size);

#endif /* SABOTEUR_EXTRAS_H */
