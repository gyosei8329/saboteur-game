/*
 * render.h
 * Saboteur / 矮人礦坑 Phase 1：SDL2 顯示與輸入處理
 *
 * 這個檔案把整個遊戲的 UI 切成幾個畫面（Screen）：
 *   - 主選單：選擇人機或多人模式
 *   - 玩家設定：多人模式輸入人數與名字
 *   - 角色查看：每位玩家輪流確認自己的角色
 *   - 玩家交接：人機交替前或多人換手前的緩衝畫面
 *   - 遊戲畫面：放牌、行動牌、棄牌
 *   - 結束畫面：宣告勝負，提供重新開始或關閉
 *
 * 同一個 SDL window 在不同 Screen 之間切換畫面內容。
 */

#ifndef SABOTEUR_RENDER_H
#define SABOTEUR_RENDER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>

#include "game.h"

#define WINDOW_W      1280
#define WINDOW_H      800

#define CELL_SIZE     64
#define VIEW_ROW_MIN  PLAY_AREA_ROW_MIN
#define VIEW_ROW_MAX  PLAY_AREA_ROW_MAX
#define VIEW_COL_MIN  PLAY_AREA_COL_MIN
#define VIEW_COL_MAX  PLAY_AREA_COL_MAX
#define PUBLIC_LOG_LIMIT 10
#define PUBLIC_LOG_MESSAGE_SIZE 180

typedef enum {
    SCREEN_MENU = 0,
    SCREEN_MP_SETUP,        /* 多人模式：輸入人數與名字 */
    SCREEN_AI_SETUP,        /* 人機模式：輸入自己的名字 */
    SCREEN_ROLE_REVEAL,     /* 將要私下查看角色的玩家提示 */
    SCREEN_HANDOFF,         /* 換手畫面（多人模式輪到下一位前） */
    SCREEN_PRIVATE_RESULT,  /* 私密結果畫面：例如地圖牌結果 */
    SCREEN_PLAY,            /* 主遊戲畫面 */
    SCREEN_ROUND_RESULT,    /* 本輪勝利方與金塊分配 */
    SCREEN_END              /* 遊戲結束 */
} Screen;

typedef enum {
    PENDING_NONE = 0,        /* 沒有選取手牌或特殊模式 */
    PENDING_PLACE_PATH,      /* 已選一張路徑牌，等待點選版圖位置 */
    PENDING_BREAK_TOOL,      /* 已選破壞牌，等待點選目標玩家的工具 */
    PENDING_REPAIR_TOOL,     /* 已選修理牌，等待點選玩家的損壞工具 */
    PENDING_USE_MAP,         /* 已選地圖牌，等待點選未公開終點 */
    PENDING_ROCKFALL,        /* 已選落石牌，等待點選普通路徑牌 */
    PENDING_DISCARD          /* 已選棄牌目標 */
} PendingAction;

typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font_big;
    TTF_Font *font_mid;
    TTF_Font *font_small;

    /* 目前 UI 狀態 */
    Screen screen;

    /*
     * Pending action：玩家點了哪一張手牌、或進入了哪個次要選擇模式。
     * selected_hand 是目前 current_player 的手牌索引，-1 表示尚未選取。
     * preview_rotated_180 表示放牌前是否旋轉。
     */
    PendingAction pending;
    int selected_hand;
    bool preview_rotated_180;

    /*
     * 角色查看／交接畫面用：要呈現給「哪位玩家」的私人畫面。
     * private_view_revealed 表示是否已按下「顯示」（避免一進畫面就洩漏角色）。
     */
    int private_view_player;
    bool private_view_revealed;

    /* 多人模式輸入用 */
    int mp_player_count;
    char mp_names[MAX_PLAYERS][MAX_NAME_LEN];
    int mp_name_focus;   /* 0 = 輸入人數，1..N = 輸入第 i 個玩家名字 */

    /* 人機模式輸入用 */
    char ai_human_name[MAX_NAME_LEN];

    /* 主畫面下方狀態列文字 */
    char message[256];

    /* 私密結果畫面文字，例如 Map card 查看終點結果。 */
    char private_result_message[256];

    /* 公開行動日誌。 */
    char public_log[PUBLIC_LOG_LIMIT][PUBLIC_LOG_MESSAGE_SIZE];
    int public_log_count;

    /* 回合結算畫面狀態。 */
    bool round_result_prepared;
    bool round_score_settled;
    int pending_gold_values[MAX_PLAYERS];
    int pending_gold_count;
    int gold_choice_player;
} AppContext;

/* ---------- 初始化與釋放 ---------- */

bool render_init(AppContext *ctx);
void render_destroy(AppContext *ctx);

/* ---------- 主要繪製入口 ---------- */

void render_frame(AppContext *ctx, GameState *game);

/*
 * 讓非 UI 互動造成的回合結束（例如 AI 出牌）也能走同一套
 * 金塊結算、下一輪與終局畫面切換流程。
 */
void render_resolve_round_if_needed(AppContext *ctx, GameState *game);

void render_add_public_log(AppContext *ctx, const char *format, ...);

/* ---------- 事件處理入口 ---------- */

/*
 * 處理單一 SDL 事件。
 * 回傳 false 表示玩家按了關閉視窗或 Esc，外層 main loop 應結束。
 */
bool render_handle_event(AppContext *ctx, GameState *game, const SDL_Event *e);

#endif /* SABOTEUR_RENDER_H */
