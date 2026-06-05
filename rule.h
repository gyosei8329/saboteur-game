/*
 * rule.h
 * Saboteur / 矮人礦坑（基礎版）數位化遊戲：規則核心公開介面
 *
 * 此檔案宣告 rule.c 對其他模組公開的函式。
 *
 * 模組責任：
 *   - game.c：建立牌堆、洗牌、初始化每輪與管理三輪流程
 *   - rule.c：判斷玩家行動是否合法，並修改行動後的 GameState
 *   - ui_raylib.c：取得玩家輸入、顯示提示；不得自行跳過規則判斷
 *
 * 使用原則：
 *   - 所有會改變遊戲狀態的出牌動作，均應透過本介面呼叫。
 *   - reason / reason_size 用於將成功或失敗原因回傳給 UI 顯示。
 *   - 私密資訊（角色、手牌、地圖牌查看結果）不得寫入公開事件紀錄。
 */

#ifndef SABOTEUR_RULE_H
#define SABOTEUR_RULE_H

#include "game.h"

#include <stdbool.h>
#include <stddef.h>

/* ---------- 遊戲設置規則 ---------- */

/**
 * 根據玩家數回傳每位玩家的初始手牌張數。
 *
 * @param player_count 玩家人數，合法範圍為 3 至 10。
 * @return 3–5 人回傳 6；6–7 人回傳 5；8–10 人回傳 4；
 *         非法人數回傳 0。
 */
int initial_hand_size(int player_count);

/**
 * 建立一輪使用的角色牌池。
 *
 * 角色牌池總張數會比玩家人數多一張。完成後仍須由 game.c 洗牌，
 * 並只將 player_count 張發給玩家。
 *
 * @param player_count 玩家人數，合法範圍為 3 至 10。
 * @param pool          由呼叫端提供的角色牌陣列，容量至少 MAX_ROLE_CARDS。
 * @param pool_count    回傳角色牌池實際張數。
 * @return 成功時為 true；參數無效或玩家數非法時為 false。
 */
bool build_role_pool(int player_count, Role *pool, int *pool_count);

/* ---------- 私密角色確認階段 ---------- */

/**
 * 將指定玩家標記為已完成本輪角色私密確認。
 *
 * 若所有玩家均已確認，函式會將階段切換為 PHASE_PLAYER_TURN，
 * 並令 starting_player 開始第一個回合。
 */
void mark_role_checked(GameState *game, int player_id);

/**
 * 判斷所有玩家是否都完成本輪角色確認。
 */
bool all_players_checked_role(const GameState *game);

/* ---------- 坑道牌與放置規則 ---------- */

/**
 * 將一張坑道牌旋轉 180 度。
 *
 * 非坑道牌將原樣回傳。此函式同時旋轉 edge_mask 與
 * connected_mask，以保留牌內部真實連通關係。
 */
Card rotate_path_card_180(Card card);

/**
 * 判斷指定玩家目前是否具有任一損壞工具。
 */
bool player_has_broken_tool(const GameState *game, int player_id);

/**
 * 判斷指定玩家目前是否可放置坑道牌。
 *
 * 玩家必須正處於自己的回合，且沒有任何損壞工具。
 */
bool can_play_path_card(const GameState *game, int player_id);

/**
 * 只檢查指定坑道牌可否放在版圖位置，不會修改 GameState。
 * 除了所有相鄰接邊須吻合之外，新牌至少須有一個通道出口接上
 * 已可由起點抵達的既有坑道出口，不能建立或延伸孤立礦道。
 *
 * @param reason      UI 顯示用訊息緩衝區，可傳 NULL。
 * @param reason_size 緩衝區長度；reason 為 NULL 時可傳 0。
 */
bool is_valid_path_placement(
    const GameState *game,
    Card card,
    int row,
    int col,
    char *reason,
    size_t reason_size
);

/**
 * 從玩家手牌放置一張坑道牌。
 *
 * 成功時會依序將坑道加入版圖、將牌移入棄牌堆、判斷終點翻開或
 * 金礦連通、補牌，最後進入結算或換下一位玩家。
 */
bool play_path_card(
    GameState *game,
    int player_id,
    int hand_index,
    int row,
    int col,
    bool rotated_180,
    char *reason,
    size_t reason_size
);

/* ---------- 回合推進 ---------- */

/**
 * 若抽牌堆尚有卡牌，讓指定玩家補一張牌。
 */
void draw_card_if_available(GameState *game, int player_id);

/**
 * 將 current_player 移至下一位玩家。
 */
void advance_turn(GameState *game);

/**
 * 判斷本輪是否已結束。
 *
 * 淘金矮人成功連到金礦，或抽牌堆與所有玩家手牌皆耗盡時結束。
 */
bool is_round_over(const GameState *game);

/* ---------- 破壞與修理工具 ---------- */

/**
 * 判斷目標玩家是否可被指定工具的破壞牌影響。
 *
 * 相同工具已損壞時不可再破壞。
 */
bool can_break_tool(
    const GameState *game,
    int target_player,
    ToolType tool
);

/**
 * 使用破壞工具牌。
 */
bool play_break_tool(
    GameState *game,
    int player_id,
    int hand_index,
    int target_player,
    ToolType tool,
    char *reason,
    size_t reason_size
);

/**
 * 判斷目標玩家是否有可由指定工具修理的損壞狀態。
 */
bool can_repair_tool(
    const GameState *game,
    int target_player,
    ToolType tool
);

/**
 * 使用修理工具牌。
 *
 * 雙工具修理牌仍需由 UI 指定本次實際修理的其中一種工具。
 */
bool play_repair_tool(
    GameState *game,
    int player_id,
    int hand_index,
    int target_player,
    ToolType tool,
    char *reason,
    size_t reason_size
);

/* ---------- 地圖、落石與棄牌 ---------- */

/**
 * 使用地圖牌私密查看一張尚未公開的終點牌。
 *
 * @param out_contains_gold 成功時回傳該終點是否含有金礦。
 *
 * 注意：out_contains_gold 只可於目前玩家的私人畫面短暫顯示，
 * 不得寫入公開版面或公開行動紀錄。
 */
bool play_map_card(
    GameState *game,
    int player_id,
    int hand_index,
    int goal_row,
    int goal_col,
    bool *out_contains_gold,
    char *reason,
    size_t reason_size
);

/**
 * 使用落石牌移除一張普通坑道牌。起點牌與終點牌不可被移除。
 */
bool play_rockfall_card(
    GameState *game,
    int player_id,
    int hand_index,
    int row,
    int col,
    char *reason,
    size_t reason_size
);

/**
 * 將當前玩家的一張手牌背面棄置。
 *
 * UI 公開紀錄只能顯示「棄置一張牌」，不得公開牌面。
 */
bool discard_card(
    GameState *game,
    int player_id,
    int hand_index,
    char *reason,
    size_t reason_size
);

/* ---------- 回合結算與最終分數 ---------- */

/**
 * 計算實際獲得指定角色的玩家數量。
 *
 * 因每輪有一張未發出的角色牌，本函式只統計 players[] 中的角色。
 */
int count_players_with_role(const GameState *game, Role role);

/**
 * 回傳破壞者獲勝時每位破壞者可得的金塊數。
 *
 * 1 位破壞者得 4；2 或 3 位各得 3；4 位各得 2；
 * 其他數量回傳 0。
 */
int saboteur_reward_per_player(int saboteur_count);

/**
 * 在破壞者成功阻止金礦連通時，將固定金塊獎勵加入總分。
 *
 * 淘金矮人成功時的選金塊牌流程需由 score.c 另行實作。
 */
bool award_saboteur_score_if_applicable(GameState *game);

/**
 * 判斷是否已進入完成三輪後的遊戲結果階段。
 */
bool is_game_over(const GameState *game);

/**
 * 回傳所有玩家目前累積金塊的最高值。
 *
 * 若 game 為 NULL 或沒有玩家，回傳 0。
 */
int find_highest_score(const GameState *game);

#endif /* SABOTEUR_RULE_H */
