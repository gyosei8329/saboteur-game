/*
 * game.h
 * Saboteur / 矮人礦坑（基礎版）數位化遊戲：共用資料型別與常數
 *
 * 此標頭檔提供 rule.c、game.c、card.c、score.c 與 UI 模組共用的
 * 遊戲資料模型。設計重點：
 *
 *   1. 角色與玩家手牌為私人資料，UI 不可在公開畫面直接呈現。
 *   2. 坑道牌同時記錄牌邊出口與牌內部實際連通關係，
 *      以正確處理「有出口但不相通」的死路牌。
 *   3. 版圖第一版使用足夠大的固定陣列；之後可替換為動態座標容器。
 */

#ifndef SABOTEUR_GAME_H
#define SABOTEUR_GAME_H

#include <stdbool.h>
#include <stdint.h>

/* ---------- 遊戲規模常數 ---------- */

#define MIN_PLAYERS        3
#define MAX_PLAYERS        10
#define MAX_ROUNDS         3

#define MAX_NAME_LEN       32
#define MAX_HAND_SIZE      6

#define BOARD_W            25
#define BOARD_H            25
#define GOAL_CARD_COUNT    3

/* 可放牌場地：起點到三張終點所在的 9 x 5 網格。 */
#define PLAY_AREA_ROW_MIN  10
#define PLAY_AREA_ROW_MAX  15
#define PLAY_AREA_COL_MIN  2
#define PLAY_AREA_COL_MAX  11
#define PLAY_AREA_ROWS     (PLAY_AREA_ROW_MAX - PLAY_AREA_ROW_MIN)
#define PLAY_AREA_COLS     (PLAY_AREA_COL_MAX - PLAY_AREA_COL_MIN)

/*
 * 基礎版玩法牌的實際數量會在 card.c 建立牌組時設定。
 * 第一版保留 80 格，可涵蓋完整玩法牌並保留少量測試空間。
 */
#define MAX_PLAY_CARDS     80
#define MAX_ROLE_CARDS     11

/* 基礎版金塊牌：一整局三輪共用，不會在每輪重置。 */
#define GOLD_DECK_COUNT     28

#define TOOL_COUNT         3
#define PATH_SIDE_COUNT    4

/* 無有效玩家或尚未有人完成連接時使用。 */
#define NO_PLAYER          (-1)

/* ---------- 通道方向與遮罩 ---------- */

/*
 * rule.c 目前有其內部方向列舉 SIDE_UP 等名稱，因此此處使用
 * PATH_SIDE_* 前綴，避免重複宣告造成編譯衝突。
 */
typedef enum {
    PATH_SIDE_UP = 0,
    PATH_SIDE_RIGHT = 1,
    PATH_SIDE_DOWN = 2,
    PATH_SIDE_LEFT = 3
} PathSide;

#define PATH_EDGE_UP       ((uint8_t)(1u << PATH_SIDE_UP))
#define PATH_EDGE_RIGHT    ((uint8_t)(1u << PATH_SIDE_RIGHT))
#define PATH_EDGE_DOWN     ((uint8_t)(1u << PATH_SIDE_DOWN))
#define PATH_EDGE_LEFT     ((uint8_t)(1u << PATH_SIDE_LEFT))

/* ---------- 玩家角色與遊戲階段 ---------- */

typedef enum {
    ROLE_GOLD_DIGGER = 0,   /* 淘金矮人 */
    ROLE_SABOTEUR           /* 破壞者 */
} Role;

typedef enum {
    PHASE_SETUP = 0,        /* 輸入玩家、初始化遊戲 */
    PHASE_ROLE_CHECK,       /* 每輪開始，玩家依序私密查看角色 */
    PHASE_PLAYER_TURN,      /* 正式輪流出牌 */
    PHASE_ROUND_RESULT,     /* 本輪角色公開與金塊結算 */
    PHASE_GAME_RESULT       /* 三輪完成，總分與勝者畫面 */
} GamePhase;

/* ---------- 遊戲模式 ---------- */

typedef enum {
    MODE_AI = 0,            /* 人機模式：1 名人類 + 2 名 AI */
    MODE_MULTIPLAYER        /* 多人模式：3 到 10 名人類玩家 */
} GameMode;

/* ---------- 卡牌型別 ---------- */

typedef enum {
    CARD_PATH = 0,          /* 坑道牌 */
    CARD_ACTION             /* 行動牌 */
} CardKind;

typedef enum {
    TOOL_PICKAXE = 0,       /* 十字鎬 */
    TOOL_LANTERN,           /* 礦燈 */
    TOOL_CART               /* 推車 */
} ToolType;

#define TOOL_MASK_PICKAXE   ((uint8_t)(1u << TOOL_PICKAXE))
#define TOOL_MASK_LANTERN   ((uint8_t)(1u << TOOL_LANTERN))
#define TOOL_MASK_CART      ((uint8_t)(1u << TOOL_CART))

typedef enum {
    ACTION_NONE = 0,        /* 坑道牌或無行動效果時使用 */
    ACTION_BREAK_TOOL,      /* 破壞一種工具 */
    ACTION_REPAIR_TOOL,     /* 修理一種，或雙工具牌選修其中一種 */
    ACTION_MAP,             /* 私密查看一張未翻開終點 */
    ACTION_ROCKFALL         /* 移除一張普通坑道牌 */
} ActionKind;

/*
 * Card 的表示方式
 * -----------------
 * id:
 *   卡牌唯一 ID，供 UI 找圖檔、測試與紀錄使用。
 *
 * kind:
 *   CARD_PATH 或 CARD_ACTION。
 *
 * edge_mask:
 *   僅對坑道牌、起點與終點有效。bit 表示卡牌該方向是否有出口。
 *
 * connected_mask[side]:
 *   僅對坑道牌、起點與終點有效。表示從 side 進入後，
 *   在該張牌內部真正能到達哪些出口。
 *
 *   範例：左右直線坑道
 *     edge_mask = PATH_EDGE_LEFT | PATH_EDGE_RIGHT;
 *     connected_mask[PATH_SIDE_LEFT]  = PATH_EDGE_RIGHT;
 *     connected_mask[PATH_SIDE_RIGHT] = PATH_EDGE_LEFT;
 *
 *   範例：左右都有出口，但各自為死路
 *     edge_mask = PATH_EDGE_LEFT | PATH_EDGE_RIGHT;
 *     connected_mask[PATH_SIDE_LEFT]  = 0;
 *     connected_mask[PATH_SIDE_RIGHT] = 0;
 *
 * action_kind:
 *   僅對 CARD_ACTION 有效。
 *
 * tool_mask:
 *   僅對破壞/修理工具牌有效。
 *   單工具修理牌設定一個 bit；雙工具修理牌設定兩個 bit，
 *   玩家使用時仍只能選擇修理其中一項。
 */
typedef struct {
    int id;
    CardKind kind;

    uint8_t edge_mask;
    uint8_t connected_mask[PATH_SIDE_COUNT];

    ActionKind action_kind;
    uint8_t tool_mask;
} Card;

/* ---------- 玩家資料 ---------- */

typedef struct {
    char name[MAX_NAME_LEN];

    /*
     * 私人資料：
     * 只有該玩家於遮罩後的私人操作畫面可查看。
     */
    Role role;
    Card hand[MAX_HAND_SIZE];
    int hand_count;

    /*
     * 公開資料：
     * 工具被破壞後應顯示於公開版圖旁的玩家狀態區。
     * active_broken_tool_cards[] 保存正面放在玩家面前的破壞牌。
     * 使用修理牌時，修理牌與對應破壞牌才會一起進入棄牌堆。
     */
    bool broken_tools[TOOL_COUNT];
    Card active_broken_tool_cards[TOOL_COUNT];

    /* 跨三輪累計的金塊總數。 */
    int gold_total;

    /* 該輪是否已完成私人角色確認。 */
    bool checked_role;
} Player;

/* ---------- 版圖資料 ---------- */

typedef struct {
    int row;
    int col;
} BoardPosition;

typedef struct {
    bool occupied;

    bool is_start;
    bool is_goal;

    /*
     * goal_revealed 為公開狀態。
     * contains_gold 在終點未公開前屬於系統隱藏資料，
     * 地圖牌只可私密回傳給使用該牌的玩家。
     */
    bool goal_revealed;
    bool contains_gold;

    Card card;
} Cell;

/* ---------- 整局與整輪狀態 ---------- */

typedef struct {
    /* 遊戲整體流程 */
    int player_count;
    int round_number;             /* 建議使用 1 到 MAX_ROUNDS */
    int current_player;           /* players[] index */
    int starting_player;          /* 本輪起始玩家 */
    GamePhase phase;

    /*
     * mode：人機 vs 多人。
     * is_ai[i]：第 i 位玩家是否由 AI 控制（多人模式時全為 false）。
     */
    GameMode mode;
    bool is_ai[MAX_PLAYERS];

    /*
     * winner_is_gold_diggers：遊戲結束時，礦工方是否獲勝。
     * 只有 phase == PHASE_GAME_RESULT 時有效。
     */
    bool winner_is_gold_diggers;

    Player players[MAX_PLAYERS];

    /*
     * 版圖：
     * 第一版採固定大小矩陣。建議 game.c 將起點與三個終點安排在中央，
     * 並保留足夠外圍空間供坑道延伸。
     */
    Cell board[BOARD_H][BOARD_W];

    /*
     * 玩法牌堆：
     * draw_count 代表仍可抽取的張數。
     * rule.c 的抽牌方式為 draw_pile[--draw_count]，
     * 因此牌堆頂端位於目前有效陣列的最後一格。
     */
    Card draw_pile[MAX_PLAY_CARDS];
    int draw_count;

    Card discard_pile[MAX_PLAY_CARDS];
    int discard_count;

    /*
     * 角色資料：
     * 每輪建立角色池後，發給玩家的角色寫入 Player.role；
     * 因角色牌總數比玩家多一張，未發出的角色仍保存在此處，
     * 方便結算與除錯，但 UI 不可在輪結束前公開。
     */
    Role role_pool[MAX_ROLE_CARDS];
    int role_pool_count;

    /*
     * 回合結果：
     * last_player_connected_goal 用於淘金矮人成功後，
     * 決定第一位挑選金塊牌的玩家。
     */
    int last_player_connected_goal;

    /*
     * 本輪最後一位完成合法出牌/棄牌的玩家。依官方規則，下一輪由
     * 該玩家左邊的玩家開始；本程式中玩家順序以 +1 表示左邊。
     */
    int last_action_player;
    bool round_won_by_gold_diggers;

    /*
     * 金塊牌橫跨三輪保存。已獲得的牌只累加為玩家的私密 gold_total，
     * 尚未取用的金塊牌保留於 gold_deck[0..gold_draw_count-1]。
     */
    int gold_deck[GOLD_DECK_COUNT];
    int gold_draw_count;
} GameState;


/* ---------- 遊戲初始化與每輪流程公開介面 ---------- */

/**
 * 初始化亂數來源。
 *
 * 正式遊戲可於程式啟動時呼叫：
 *
 *     game_seed_random((unsigned int)time(NULL));
 *
 * 單元測試可傳入固定 seed 以取得可重現結果。
 */
void game_seed_random(unsigned int seed);

/**
 * 依玩家輸入名稱建立一局遊戲，隨機排列遊玩順序，並初始化第 1 輪。
 *
 * names 必須提供 player_count 個非空且互不重複的名稱。
 * 成功後 phase 會位於 PHASE_ROLE_CHECK。
 */
bool game_init(
    GameState *game,
    int player_count,
    const char names[][MAX_NAME_LEN]
);

/**
 * 帶有遊戲模式的初始化。is_ai 為長度 player_count 的陣列，
 * 標記哪些玩家由 AI 控制。
 */
bool game_init_with_mode(
    GameState *game,
    int player_count,
    const char names[][MAX_NAME_LEN],
    GameMode mode,
    const bool *is_ai
);

/**
 * 初始化目前 round_number 所表示的一輪：
 * 重設版圖、工具與手牌，建立角色池、牌庫並發初始手牌。
 *
 * 此函式保留玩家名稱、玩家順序與累積 gold_total。
 */
bool game_setup_round(GameState *game);

/**
 * 完成本輪結算後進入下一輪；第三輪結束後改為 PHASE_GAME_RESULT。
 *
 * 呼叫前，score.c 或 UI 流程應已完成本輪金塊分配。
 */
bool game_begin_next_round(GameState *game);

/**
 * 回傳固定版圖中的起點位置。
 */
BoardPosition game_get_start_position(void);

/**
 * 回傳第 goal_index 張終點位置；索引無效時回傳 {-1, -1}。
 */
BoardPosition game_get_goal_position(int goal_index);

/**
 * 在角色確認階段，按照本輪遊玩順序尋找下一位尚未確認角色的玩家。
 *
 * 所有人皆已確認、或不處於角色確認階段時，回傳 NO_PLAYER。
 */
int game_next_player_to_check_role(const GameState *game);

#endif /* SABOTEUR_GAME_H */
