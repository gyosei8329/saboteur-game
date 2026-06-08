/*
 * main.c
 * Saboteur / 矮人礦坑 Phase 1：程式進入點
 *
 * 程式架構：
 *   - render.c 負責 SDL2 視窗與所有畫面、輸入處理
 *   - rule.c / game.c / card.c / score.c / extras.c 負責遊戲邏輯
 *   - main.c 只做事件迴圈與 AI 自動出牌觸發
 */

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "game.h"
#include "rule.h"
#include "extras.h"
#include "render.h"

int main(void)
{
    AppContext ctx;
    if (!render_init(&ctx)) {
        fprintf(stderr, "Failed to initialize SDL2 / TTF.\n");
        return 1;
    }

    GameState game;
    /*
     * 一進來只到主選單，遊戲狀態尚未初始化。
     * 真正的 game_init 由 render.c 中模式選擇後呼叫。
     */

     
     
    srand((unsigned int)time(NULL));
    memset(&game, 0, sizeof(game));

    bool running = true;
    Uint32 last_ai_time = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (!render_handle_event(&ctx, &game, &e)) {
                running = false;
                break;
            }
        }
        if (!running) break;

        /*
         * AI 自動出牌：
         *   - 只有在 SCREEN_PLAY 且當前玩家是 AI 時觸發。
         *   - 為了讓畫面看得到 AI 動作，每 500 ms 才出一次手。
         */
        if (ctx.screen == SCREEN_PLAY
            && game.phase == PHASE_PLAYER_TURN
            && game.current_player >= 0
            && game.current_player < game.player_count
            && game.is_ai[game.current_player]) {
            Uint32 now = SDL_GetTicks();
            if (now - last_ai_time > 500) {
                char ai_log[180];
                if (ai_take_turn_with_log(&game, ai_log, sizeof(ai_log))) {
                    render_add_public_log(&ctx, "%s", ai_log);
                }
                render_resolve_round_if_needed(&ctx, &game);
                last_ai_time = now;
            }
        }

        render_frame(&ctx, &game);
    }

    render_destroy(&ctx);
    return 0;
}
