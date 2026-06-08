/*
 * render.c
 * Saboteur / 矮人礦坑原版規則：SDL2 顯示與輸入處理
 */

#include "render.h"
#include "rule.h"
#include "card.h"
#include "extras.h"
#include "score.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* ---------- 顏色 ---------- */

static const SDL_Color COLOR_BG       = {  30,  32,  38, 255};
static const SDL_Color COLOR_PANEL    = {  44,  48,  58, 255};
static const SDL_Color COLOR_BORDER   = { 100, 105, 115, 255};
static const SDL_Color COLOR_TEXT     = { 235, 235, 235, 255};
static const SDL_Color COLOR_DIM      = { 165, 165, 165, 255};
static const SDL_Color COLOR_ACCENT   = { 230, 185,  50, 255};   /* 金色強調 */
static const SDL_Color COLOR_GOOD     = {  90, 200, 110, 255};
static const SDL_Color COLOR_BAD      = { 220,  90,  90, 255};
static const SDL_Color COLOR_TILE     = { 120,  80,  50, 255};   /* 路徑磚色 */
static const SDL_Color COLOR_START    = {  70, 120, 220, 255};
static const SDL_Color COLOR_GOAL_GOLD= {230, 185,  50, 255};
static const SDL_Color COLOR_GOAL_COAL= {110, 110, 110, 255};
static const SDL_Color COLOR_BUTTON   = {  70,  80,  95, 255};
static const SDL_Color COLOR_BUTTON_H = {  95, 110, 130, 255};

/* ---------- 字型載入 ---------- */

static TTF_Font *load_font(int size)
{
    /* 兩個常見字型路徑都試一下。 */
    TTF_Font *font = TTF_OpenFont(
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", size);
    if (!font) {
        font = TTF_OpenFont("/usr/share/fonts/dejavu/DejaVuSans.ttf", size);
    }
    return font;
}

/* ---------- 小工具：畫文字、矩形、按鈕 ---------- */

static void set_color(SDL_Renderer *r, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void fill_rect(SDL_Renderer *r, SDL_Rect rect, SDL_Color c)
{
    set_color(r, c);
    SDL_RenderFillRect(r, &rect);
}

static void draw_rect(SDL_Renderer *r, SDL_Rect rect, SDL_Color c)
{
    set_color(r, c);
    SDL_RenderDrawRect(r, &rect);
}

static void draw_text(
    SDL_Renderer *r, TTF_Font *font,
    int x, int y, SDL_Color color, const char *text
)
{
    if (text == NULL || text[0] == '\0' || font == NULL) {
        return;
    }
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_FreeSurface(surf);
    if (!tex) return;
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

static void draw_text_centered(
    SDL_Renderer *r, TTF_Font *font,
    int x, int y, int w, SDL_Color color, const char *text
)
{
    if (text == NULL || text[0] == '\0' || font == NULL) {
        return;
    }
    int tw = 0, th = 0;
    TTF_SizeUTF8(font, text, &tw, &th);
    (void)th;
    draw_text(r, font, x + (w - tw) / 2, y, color, text);
}

static bool point_in_rect(int x, int y, SDL_Rect rect)
{
    return x >= rect.x && x < rect.x + rect.w
        && y >= rect.y && y < rect.y + rect.h;
}

static bool draw_button(
    AppContext *ctx, SDL_Rect rect, const char *label, bool enabled
)
{
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    bool hovered = enabled && point_in_rect(mx, my, rect);
    SDL_Color fill = enabled
        ? (hovered ? COLOR_BUTTON_H : COLOR_BUTTON)
        : COLOR_PANEL;
    fill_rect(ctx->renderer, rect, fill);
    draw_rect(ctx->renderer, rect, COLOR_BORDER);

    int tw = 0, th = 0;
    TTF_SizeUTF8(ctx->font_mid, label, &tw, &th);
    draw_text(ctx->renderer, ctx->font_mid,
              rect.x + (rect.w - tw) / 2,
              rect.y + (rect.h - th) / 2,
              enabled ? COLOR_TEXT : COLOR_DIM,
              label);
    return enabled && hovered;
}

/* ---------- 卡牌圖樣 ---------- */

static bool card_side_connects_anywhere(const Card *card, int side)
{
    return card != NULL
        && side >= 0
        && side < PATH_SIDE_COUNT
        && card->connected_mask[side] != 0u;
}

static void draw_path_lines(
    SDL_Renderer *r, SDL_Rect rect, const Card *card, SDL_Color color
)
{
    /*
     * 路徑牌的視覺必須依 connected_mask 畫出「真實連通」。
     * 多出口死路牌雖然有多個 edge，但各出口不能畫成彼此相通。
     */
    if (card == NULL) {
        return;
    }

    int cx = rect.x + rect.w / 2;
    int cy = rect.y + rect.h / 2;
    int thick = rect.w / 7;

    set_color(r, color);

    bool has_connected_path = false;
    for (int side = 0; side < PATH_SIDE_COUNT; ++side) {
        if (card_side_connects_anywhere(card, side)) {
            has_connected_path = true;
            break;
        }
    }

    if (has_connected_path) {
        SDL_Rect center = {cx - thick / 2, cy - thick / 2, thick, thick};
        SDL_RenderFillRect(r, &center);
    }

    if ((card->edge_mask & PATH_EDGE_UP) != 0u) {
        int end_y = card_side_connects_anywhere(card, PATH_SIDE_UP)
            ? cy
            : cy - rect.h / 6;
        SDL_Rect arm = {cx - thick / 2, rect.y + 3, thick, cy - rect.y - 3};
        arm.h = end_y - arm.y;
        SDL_RenderFillRect(r, &arm);
    }
    if ((card->edge_mask & PATH_EDGE_RIGHT) != 0u) {
        int start_x = card_side_connects_anywhere(card, PATH_SIDE_RIGHT)
            ? cx
            : cx + rect.w / 6;
        SDL_Rect arm = {start_x, cy - thick / 2,
                        rect.x + rect.w - start_x - 3, thick};
        SDL_RenderFillRect(r, &arm);
    }
    if ((card->edge_mask & PATH_EDGE_DOWN) != 0u) {
        int start_y = card_side_connects_anywhere(card, PATH_SIDE_DOWN)
            ? cy
            : cy + rect.h / 6;
        SDL_Rect arm = {cx - thick / 2, start_y, thick,
                        rect.y + rect.h - start_y - 3};
        SDL_RenderFillRect(r, &arm);
    }
    if ((card->edge_mask & PATH_EDGE_LEFT) != 0u) {
        int end_x = card_side_connects_anywhere(card, PATH_SIDE_LEFT)
            ? cx
            : cx - rect.w / 6;
        SDL_Rect arm = {rect.x + 3, cy - thick / 2, end_x - rect.x - 3, thick};
        SDL_RenderFillRect(r, &arm);
    }

    if (!has_connected_path && card->edge_mask != 0u) {
        SDL_Rect cap = {cx - thick / 2, cy - thick / 2, thick, thick};
        SDL_RenderFillRect(r, &cap);
    }
}

static void draw_board_cell(
    AppContext *ctx, SDL_Rect rect, const Cell *cell, bool highlight
)
{
    /* 空格 */
    if (!cell->occupied) {
        fill_rect(ctx->renderer, rect, (SDL_Color){50, 53, 60, 255});
        draw_rect(ctx->renderer, rect, (SDL_Color){70, 75, 82, 255});
        if (highlight) {
            draw_rect(ctx->renderer, rect, COLOR_ACCENT);
        }
        return;
    }

    /* 起點 */
    if (cell->is_start) {
        fill_rect(ctx->renderer, rect, COLOR_START);
        draw_path_lines(ctx->renderer, rect, &cell->card,
                        (SDL_Color){15, 15, 15, 255});
        draw_text(ctx->renderer, ctx->font_small,
                  rect.x + 3, rect.y + 2, COLOR_TEXT, "S");
        if (highlight) draw_rect(ctx->renderer, rect, COLOR_ACCENT);
        return;
    }

    /* 終點 */
    if (cell->is_goal) {
        if (!cell->goal_revealed) {
            fill_rect(ctx->renderer, rect, (SDL_Color){180, 180, 185, 255});
            draw_text_centered(ctx->renderer, ctx->font_mid,
                               rect.x, rect.y + rect.h/2 - 12,
                               rect.w, COLOR_TEXT, "?");
        } else if (cell->contains_gold) {
            fill_rect(ctx->renderer, rect, COLOR_GOAL_GOLD);
            draw_path_lines(ctx->renderer, rect, &cell->card,
                            (SDL_Color){50, 35, 5, 255});
        } else {
            fill_rect(ctx->renderer, rect, COLOR_GOAL_COAL);
            draw_path_lines(ctx->renderer, rect, &cell->card,
                            (SDL_Color){30, 30, 30, 255});
        }
        draw_rect(ctx->renderer, rect, COLOR_BORDER);
        if (highlight) draw_rect(ctx->renderer, rect, COLOR_ACCENT);
        return;
    }

    /* 普通路徑牌：原版規則中道路牌放下後立即公開。 */
    fill_rect(ctx->renderer, rect, COLOR_TILE);
    draw_path_lines(ctx->renderer, rect, &cell->card,
                    (SDL_Color){25, 25, 25, 255});
    draw_rect(ctx->renderer, rect, COLOR_BORDER);
    if (highlight) draw_rect(ctx->renderer, rect, COLOR_ACCENT);
}
static const char *card_label(const Card *card)
{
    if (card == NULL) return "";
    if (card->kind == CARD_PATH) {
        bool dead = true;
        for (int s = 0; s < PATH_SIDE_COUNT; ++s) {
            if (card->connected_mask[s] != 0) { dead = false; break; }
        }
        return dead ? "Path (dead)" : "Path";
    }
    if (card->action_kind == ACTION_BREAK_TOOL) {
        if (card->tool_mask & TOOL_MASK_PICKAXE) return "Break P";
        if (card->tool_mask & TOOL_MASK_LANTERN) return "Break L";
        if (card->tool_mask & TOOL_MASK_CART)    return "Break C";
        return "Break";
    }
    if (card->action_kind == ACTION_REPAIR_TOOL) {
        bool p = (card->tool_mask & TOOL_MASK_PICKAXE) != 0;
        bool l = (card->tool_mask & TOOL_MASK_LANTERN) != 0;
        bool c = (card->tool_mask & TOOL_MASK_CART)    != 0;
        if (p && l) return "Repair P/L";
        if (p && c) return "Repair P/C";
        if (l && c) return "Repair L/C";
        if (p)      return "Repair P";
        if (l)      return "Repair L";
        if (c)      return "Repair C";
        return "Repair";
    }
    switch (card->action_kind) {
        case ACTION_MAP:      return "Map";
        case ACTION_ROCKFALL: return "Rockfall";
        default:              return "?";
    }    

}


static void draw_hand_card(
    AppContext *ctx, SDL_Rect rect, const Card *card,
    bool selected, int hotkey_number
)
{
    if (card == NULL) {
        fill_rect(ctx->renderer, rect, (SDL_Color){55, 58, 65, 255});
        draw_rect(ctx->renderer, rect, COLOR_BORDER);
        return;
    }

    SDL_Color base = COLOR_TILE;
    if (card->kind == CARD_ACTION) {
        switch (card->action_kind) {
            case ACTION_BREAK_TOOL:   base = COLOR_BAD; break;
            case ACTION_REPAIR_TOOL:  base = COLOR_GOOD; break;
            case ACTION_MAP:          base = (SDL_Color){80, 120, 180, 255}; break;
            case ACTION_ROCKFALL:     base = (SDL_Color){150, 100, 80, 255}; break;
            default:                  base = COLOR_PANEL; break;
        }
    }
    fill_rect(ctx->renderer, rect, base);
    draw_rect(ctx->renderer, rect, selected ? COLOR_ACCENT : COLOR_BORDER);

    if (card->kind == CARD_PATH) {
        SDL_Rect inner = {rect.x + 8, rect.y + 28,
                          rect.w - 16, rect.h - 60};
        Card preview = *card;
        if (selected && ctx->preview_rotated_180) {
            preview = rotate_path_card_180(*card);
        }
        draw_path_lines(ctx->renderer, inner, &preview,
                        (SDL_Color){25, 25, 25, 255});
    }

    char num[8];
    snprintf(num, sizeof(num), "%d", hotkey_number);
    draw_text(ctx->renderer, ctx->font_small,
              rect.x + 8, rect.y + 6, COLOR_TEXT, num);
    draw_text(ctx->renderer, ctx->font_small,
              rect.x + 8, rect.y + rect.h - 22, COLOR_TEXT, card_label(card));
}

/* ---------- 版圖座標換算 ---------- */

static SDL_Rect board_origin(void)
{
    return (SDL_Rect){20, 80,
                      (VIEW_COL_MAX - VIEW_COL_MIN) * CELL_SIZE,
                      (VIEW_ROW_MAX - VIEW_ROW_MIN) * CELL_SIZE};
}

static SDL_Rect cell_rect(int row, int col)
{
    SDL_Rect origin = board_origin();
    return (SDL_Rect){
        origin.x + (col - VIEW_COL_MIN) * CELL_SIZE,
        origin.y + (row - VIEW_ROW_MIN) * CELL_SIZE,
        CELL_SIZE - 1, CELL_SIZE - 1
    };
}

static bool screen_to_board_cell(int sx, int sy, int *out_row, int *out_col)
{
    SDL_Rect origin = board_origin();
    if (!point_in_rect(sx, sy, origin)) {
        return false;
    }
    int col = VIEW_COL_MIN + (sx - origin.x) / CELL_SIZE;
    int row = VIEW_ROW_MIN + (sy - origin.y) / CELL_SIZE;
    if (col < 0 || col >= BOARD_W || row < 0 || row >= BOARD_H) {
        return false;
    }
    *out_row = row;
    *out_col = col;
    return true;
}

/* ---------- 右側面板：玩家清單與按鈕 ---------- */

#define PANEL_X       700
#define PANEL_W       560
#define PANEL_Y       80
#define PANEL_H       440

static SDL_Rect player_row_rect(int index)
{
    return (SDL_Rect){PANEL_X + 10, PANEL_Y + 50 + index * 36,
                      PANEL_W - 20, 32};
}

static SDL_Rect tool_icon_rect(int player_index, int tool)
{
    SDL_Rect row = player_row_rect(player_index);
    return (SDL_Rect){row.x + row.w - 110 + tool * 30, row.y + 4, 26, 24};
}

static const char *tool_short(int tool)
{
    switch (tool) {
        case TOOL_PICKAXE: return "P";
        case TOOL_LANTERN: return "L";
        case TOOL_CART:    return "C";
        default:           return "?";
    }
}

static void draw_player_panel(AppContext *ctx, const GameState *game)
{
    SDL_Rect panel = {PANEL_X, PANEL_Y, PANEL_W, PANEL_H};
    fill_rect(ctx->renderer, panel, COLOR_PANEL);
    draw_rect(ctx->renderer, panel, COLOR_BORDER);

    char round_label[32];
    snprintf(round_label, sizeof(round_label), "Round %d / %d",
             game->round_number, MAX_ROUNDS);
    draw_text(ctx->renderer, ctx->font_mid,
              panel.x + 10, panel.y + 10, COLOR_TEXT, "Players");
    draw_text(ctx->renderer, ctx->font_mid,
              panel.x + PANEL_W - 130, panel.y + 10,
              COLOR_ACCENT, round_label);

    for (int i = 0; i < game->player_count; ++i) {
        SDL_Rect row = player_row_rect(i);
        bool is_current = (i == game->current_player);
        SDL_Color row_color = is_current
            ? (SDL_Color){70, 80, 100, 255}
            : (SDL_Color){55, 58, 68, 255};
        fill_rect(ctx->renderer, row, row_color);
        draw_rect(ctx->renderer, row, COLOR_BORDER);

        char label[80];
        snprintf(label, sizeof(label), "%s%s",
                 game->players[i].name,
                 game->is_ai[i] ? " [AI]" : "");
        draw_text(ctx->renderer, ctx->font_small,
                  row.x + 8, row.y + 6,
                  is_current ? COLOR_ACCENT : COLOR_TEXT, label);

        /* 金塊數，顯示在名字右側 */
        char gold_label[16];
        snprintf(gold_label, sizeof(gold_label),
                 "Gold: %d", game->players[i].gold_total);
        draw_text(ctx->renderer, ctx->font_small,
                  row.x + 160, row.y + 6,
                  COLOR_ACCENT, gold_label);

        for (int tool = 0; tool < TOOL_COUNT; ++tool) {
            SDL_Rect ti = tool_icon_rect(i, tool);
            bool broken = game->players[i].broken_tools[tool];
            fill_rect(ctx->renderer, ti,
                      broken ? COLOR_BAD : (SDL_Color){70, 90, 70, 255});
            draw_rect(ctx->renderer, ti, COLOR_BORDER);
            draw_text_centered(ctx->renderer, ctx->font_small,
                               ti.x, ti.y + 4, ti.w,
                               COLOR_TEXT, tool_short(tool));
        }
    }
}

/* ---------- 動作按鈕 ---------- */

#define ACTION_BAR_Y   570
#define ACTION_BTN_W   120
#define ACTION_BTN_H   36


static SDL_Rect action_btn_rect(int index)
{
    return (SDL_Rect){20 + index * (ACTION_BTN_W + 8),
                      ACTION_BAR_Y, ACTION_BTN_W, ACTION_BTN_H};
}

/* ---------- 手牌列 ---------- */
#define HAND_Y         620
#define HAND_CARD_W    105
#define HAND_CARD_H    150
#define HAND_GAP       8

static SDL_Rect hand_card_rect(int index)
{
    return (SDL_Rect){20 + index * (HAND_CARD_W + HAND_GAP),
                      HAND_Y, HAND_CARD_W, HAND_CARD_H};
}

/* ---------- 一些畫面切換用的小函式 ---------- */

static void clear_pending(AppContext *ctx)
{
    ctx->pending = PENDING_NONE;
    ctx->selected_hand = -1;
    ctx->preview_rotated_180 = false;
}

static void set_message(AppContext *ctx, const char *msg)
{
    snprintf(ctx->message, sizeof(ctx->message), "%s", msg);
}

void render_add_public_log(AppContext *ctx, const char *format, ...)
{
    if (ctx == NULL || format == NULL) {
        return;
    }

    if (ctx->public_log_count == PUBLIC_LOG_LIMIT) {
        memmove(ctx->public_log[0], ctx->public_log[1],
                (PUBLIC_LOG_LIMIT - 1u) * sizeof(ctx->public_log[0]));
        memset(ctx->public_log[PUBLIC_LOG_LIMIT - 1], 0,
               sizeof(ctx->public_log[PUBLIC_LOG_LIMIT - 1]));
        ctx->public_log_count = PUBLIC_LOG_LIMIT - 1;
    }

    va_list args;
    va_start(args, format);
    (void)vsnprintf(ctx->public_log[ctx->public_log_count],
                    sizeof(ctx->public_log[ctx->public_log_count]),
                    format, args);
    va_end(args);
    ++ctx->public_log_count;
}

static void reset_public_log(AppContext *ctx)
{
    memset(ctx->public_log, 0, sizeof(ctx->public_log));
    ctx->public_log_count = 0;
}

static void draw_public_log_panel(
    AppContext *ctx, int x, int y, int w, int h, const char *title)
{
    SDL_Rect panel = {x, y, w, h};
    fill_rect(ctx->renderer, panel, COLOR_PANEL);
    draw_rect(ctx->renderer, panel, COLOR_BORDER);
    draw_text(ctx->renderer, ctx->font_mid,
              x + 10, y + 8, COLOR_TEXT, title);

    int first = ctx->public_log_count - 8;
    if (first < 0) first = 0;
    int line_y = y + 42;
    for (int i = first; i < ctx->public_log_count; ++i) {
        char line[PUBLIC_LOG_MESSAGE_SIZE + 8];
        snprintf(line, sizeof(line), "- %s", ctx->public_log[i]);
        draw_text(ctx->renderer, ctx->font_small,
                  x + 12, line_y, COLOR_DIM, line);
        line_y += 21;
        if (line_y > y + h - 22) break;
    }

    if (ctx->public_log_count == 0) {
        draw_text(ctx->renderer, ctx->font_small,
                  x + 12, y + 44, COLOR_DIM, "No public actions yet.");
    }
}

static int current_player_index(const GameState *game)
{
    return game->current_player;
}

static bool is_human_turn(const GameState *game)
{
    int p = current_player_index(game);
    return p >= 0 && p < game->player_count && !game->is_ai[p];
}

/* ---------- 回合結算 ---------- */

static void reset_round_result_state(AppContext *ctx)
{
    ctx->round_result_prepared = false;
    ctx->round_score_settled = false;
    memset(ctx->pending_gold_values, 0, sizeof(ctx->pending_gold_values));
    ctx->pending_gold_count = 0;
    ctx->gold_choice_player = NO_PLAYER;
}

static const char *role_label(Role role)
{
    return role == ROLE_SABOTEUR ? "Saboteur" : "Miner";
}

static const char *tool_label(int tool)
{
    switch (tool) {
        case TOOL_PICKAXE: return "Pickaxe";
        case TOOL_LANTERN: return "Lantern";
        case TOOL_CART:    return "Cart";
        default:           return "Tool";
    }
}

static void remove_pending_gold_value(AppContext *ctx, int index)
{
    if (index < 0 || index >= ctx->pending_gold_count) {
        return;
    }
    for (int i = index; i < ctx->pending_gold_count - 1; ++i) {
        ctx->pending_gold_values[i] = ctx->pending_gold_values[i + 1];
    }
    --ctx->pending_gold_count;
}

static int best_pending_gold_index(const AppContext *ctx)
{
    int best = 0;
    for (int i = 1; i < ctx->pending_gold_count; ++i) {
        if (ctx->pending_gold_values[i] > ctx->pending_gold_values[best]) {
            best = i;
        }
    }
    return best;
}

static void advance_gold_choice_player(AppContext *ctx, GameState *game)
{
    if (ctx->pending_gold_count <= 0) {
        ctx->gold_choice_player = NO_PLAYER;
        ctx->round_score_settled = true;
        return;
    }

    ctx->gold_choice_player =
        score_next_gold_miner_counterclockwise(game, ctx->gold_choice_player);
    if (ctx->gold_choice_player == NO_PLAYER) {
        ctx->round_score_settled = true;
    }
}

static void award_pending_gold_choice(
    AppContext *ctx, GameState *game, int chosen_index)
{
    if (ctx->gold_choice_player == NO_PLAYER
        || chosen_index < 0
        || chosen_index >= ctx->pending_gold_count) {
        return;
    }

    int receiver = ctx->gold_choice_player;
    int value = ctx->pending_gold_values[chosen_index];
    if (score_award_gold_card(game, receiver, value)) {
        render_add_public_log(ctx, "%s received %d gold.",
                              game->players[receiver].name, value);
        remove_pending_gold_value(ctx, chosen_index);
        advance_gold_choice_player(ctx, game);
    }
}

static void auto_award_ai_gold_choices(AppContext *ctx, GameState *game)
{
    while (ctx->pending_gold_count > 0
           && ctx->gold_choice_player != NO_PLAYER
           && game->is_ai[ctx->gold_choice_player]) {
        award_pending_gold_choice(ctx, game, best_pending_gold_index(ctx));
    }
}

static void prepare_round_result(AppContext *ctx, GameState *game)
{
    if (ctx->round_result_prepared || game->phase != PHASE_ROUND_RESULT) {
        return;
    }

    clear_pending(ctx);
    reset_round_result_state(ctx);
    ctx->round_result_prepared = true;
    ctx->screen = SCREEN_ROUND_RESULT;

    if (game->round_won_by_gold_diggers) {
        ctx->pending_gold_count = score_draw_cards_for_gold_miners(
            game, ctx->pending_gold_values, MAX_PLAYERS);
        ctx->gold_choice_player = score_first_gold_miner_recipient(game);
        render_add_public_log(ctx, "Round %d ended: Miners found gold.",
                              game->round_number);
        auto_award_ai_gold_choices(ctx, game);
        if (ctx->pending_gold_count <= 0
            || ctx->gold_choice_player == NO_PLAYER) {
            ctx->round_score_settled = true;
        }
    } else {
        int awards[MAX_PLAYERS] = {0};
        (void)score_award_saboteur_round(game, awards);
        ctx->round_score_settled = true;
        render_add_public_log(ctx, "Round %d ended: Saboteurs stopped the miners.",
                              game->round_number);
        for (int i = 0; i < game->player_count; ++i) {
            if (awards[i] > 0) {
                render_add_public_log(ctx, "%s received %d gold.",
                                      game->players[i].name, awards[i]);
            }
        }
    }
}

static void enter_role_reveal_for_new_round(AppContext *ctx, GameState *game)
{
    for (int i = 0; i < game->player_count; ++i) {
        if (game->is_ai[i]) {
            mark_role_checked(game, i);
        }
    }

    int next = game_next_player_to_check_role(game);
    clear_pending(ctx);
    if (next == NO_PLAYER) {
        ctx->screen = SCREEN_PLAY;
        set_message(ctx, "Next round started.");
    } else {
        ctx->private_view_player = next;
        ctx->private_view_revealed = false;
        ctx->screen = SCREEN_ROLE_REVEAL;
        set_message(ctx, "Next round: confirm your new role.");
    }
}

static void continue_after_round_result(AppContext *ctx, GameState *game)
{
    /*
     * 原版 Saboteur：每局進行三輪。結算畫面完成金塊分配後，
     * 前兩輪重新洗牌與重新發身分，第三輪結束後顯示總分。
     */
    if (game->phase != PHASE_ROUND_RESULT || !ctx->round_score_settled) {
        return;
    }

    if (game->round_number >= MAX_ROUNDS) {
        game->phase = PHASE_GAME_RESULT;
        game->winner_is_gold_diggers = game->round_won_by_gold_diggers;
        ctx->screen = SCREEN_END;
        reset_round_result_state(ctx);
        return;
    }

    if (game_begin_next_round(game)) {
        reset_round_result_state(ctx);
        reset_public_log(ctx);
        render_add_public_log(ctx, "Round %d started.", game->round_number);
        enter_role_reveal_for_new_round(ctx, game);
    } else {
        game->phase = PHASE_GAME_RESULT;
        game->winner_is_gold_diggers = game->round_won_by_gold_diggers;
        ctx->screen = SCREEN_END;
        reset_round_result_state(ctx);
    }
}

void render_resolve_round_if_needed(AppContext *ctx, GameState *game)
{
    if (game != NULL && game->phase == PHASE_ROUND_RESULT) {
        prepare_round_result(ctx, game);
    }
}

/* ---------- 主選單 ---------- */

static SDL_Rect menu_btn_ai(void)
{
    return (SDL_Rect){WINDOW_W/2 - 220, 320, 440, 70};
}
static SDL_Rect menu_btn_mp(void)
{
    return (SDL_Rect){WINDOW_W/2 - 220, 410, 440, 70};
}
static SDL_Rect menu_btn_quit(void)
{
    return (SDL_Rect){WINDOW_W/2 - 220, 500, 440, 70};
}

static void render_menu(AppContext *ctx)
{
    draw_text_centered(ctx->renderer, ctx->font_big,
                       0, 140, WINDOW_W, COLOR_TEXT,
                       "Solo vs AI");
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 220, WINDOW_W, COLOR_DIM,
                       "Original Saboteur rules");

    draw_button(ctx, menu_btn_ai(), "1) Solo vs AI (you + 2 AI)", true);
    draw_button(ctx, menu_btn_mp(), "2) Multiplayer (3-10 humans)", true);
    draw_button(ctx, menu_btn_quit(), "Esc) Quit", true);
}

/* ---------- AI 設定畫面 ---------- */

static SDL_Rect ai_setup_name_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 - 250, 320, 500, 60};
}
static SDL_Rect ai_setup_start_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 - 150, 430, 300, 60};
}
static SDL_Rect ai_setup_back_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 - 150, 510, 300, 50};
}

static void render_ai_setup(AppContext *ctx)
{
    draw_text_centered(ctx->renderer, ctx->font_big,
                       0, 140, WINDOW_W, COLOR_TEXT,
                       "Solo vs AI");
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 220, WINDOW_W, COLOR_DIM,
                       "Enter your name, then start.");

    SDL_Rect name = ai_setup_name_rect();
    fill_rect(ctx->renderer, name, COLOR_PANEL);
    draw_rect(ctx->renderer, name, COLOR_BORDER);
    draw_text(ctx->renderer, ctx->font_mid,
              name.x + 16, name.y + 16, COLOR_TEXT, ctx->ai_human_name);

    /* 閃爍游標 */
    int tw = 0, th = 0;
    TTF_SizeUTF8(ctx->font_mid, ctx->ai_human_name, &tw, &th);
    if ((SDL_GetTicks() / 500) % 2 == 0) {
        draw_text(ctx->renderer, ctx->font_mid,
                  name.x + 16 + tw, name.y + 16, COLOR_TEXT, "|");
    }

    bool can_start = (ctx->ai_human_name[0] != '\0');
    draw_button(ctx, ai_setup_start_rect(), "Start", can_start);
    draw_button(ctx, ai_setup_back_rect(), "Back to menu", true);
}

/* ---------- 多人設定畫面 ---------- */

static SDL_Rect mp_count_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 - 250, 200, 500, 60};
}
static SDL_Rect mp_minus_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 + 115, 210, 44, 40};
}
static SDL_Rect mp_plus_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 + 170, 210, 44, 40};
}
static SDL_Rect mp_name_rect(int index)
{
    int row = index / 2;
    int col = index % 2;
    return (SDL_Rect){WINDOW_W/2 - 510 + col * 520, 290 + row * 60, 480, 50};
}
static SDL_Rect mp_start_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 - 150, 680, 300, 60};
}
static SDL_Rect mp_back_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 - 470, 680, 280, 60};
}

static bool mp_names_ready(const AppContext *ctx)
{
    if (ctx->mp_player_count < 3 || ctx->mp_player_count > MAX_PLAYERS) {
        return false;
    }
    for (int i = 0; i < ctx->mp_player_count; ++i) {
        if (ctx->mp_names[i][0] == '\0') return false;
    }
    return true;
}

static void render_mp_setup(AppContext *ctx)
{
    draw_text_centered(ctx->renderer, ctx->font_big,
                       0, 60, WINDOW_W, COLOR_TEXT,
                       "Multiplayer setup");
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 130, WINDOW_W, COLOR_DIM,
                       "Click a box to focus, type to enter. 3-10 players.");

    SDL_Rect count = mp_count_rect();
    fill_rect(ctx->renderer, count, COLOR_PANEL);
    draw_rect(ctx->renderer, count,
              ctx->mp_name_focus == 0 ? COLOR_ACCENT : COLOR_BORDER);
    char count_label[64];
    snprintf(count_label, sizeof(count_label), "Players: %d",
             ctx->mp_player_count);
    draw_text(ctx->renderer, ctx->font_mid,
              count.x + 16, count.y + 16, COLOR_TEXT, count_label);
    draw_text(ctx->renderer, ctx->font_small,
              count.x + 250, count.y + 22, COLOR_DIM,
              "Use keyboard or buttons:");
    draw_button(ctx, mp_minus_rect(), "-", ctx->mp_player_count > MIN_PLAYERS);
    draw_button(ctx, mp_plus_rect(), "+", ctx->mp_player_count < MAX_PLAYERS);

    for (int i = 0; i < ctx->mp_player_count; ++i) {
        SDL_Rect r = mp_name_rect(i);
        fill_rect(ctx->renderer, r, COLOR_PANEL);
        draw_rect(ctx->renderer, r,
                  ctx->mp_name_focus == i + 1 ? COLOR_ACCENT : COLOR_BORDER);
        char prefix[24];
        snprintf(prefix, sizeof(prefix), "P%d: ", i + 1);
        draw_text(ctx->renderer, ctx->font_mid,
                  r.x + 10, r.y + 12, COLOR_DIM, prefix);
        draw_text(ctx->renderer, ctx->font_mid,
                  r.x + 60, r.y + 12, COLOR_TEXT, ctx->mp_names[i]);
        if (ctx->mp_name_focus == i + 1
            && (SDL_GetTicks() / 500) % 2 == 0) {
            int tw = 0, th = 0;
            TTF_SizeUTF8(ctx->font_mid, ctx->mp_names[i], &tw, &th);
            draw_text(ctx->renderer, ctx->font_mid,
                      r.x + 60 + tw, r.y + 12, COLOR_TEXT, "|");
        }
    }

    draw_button(ctx, mp_back_rect(), "Back", true);
    draw_button(ctx, mp_start_rect(), "Start game", mp_names_ready(ctx));
}

/* ---------- 角色確認畫面 ---------- */

static SDL_Rect role_reveal_btn(void)
{
    return (SDL_Rect){WINDOW_W/2 - 180, 540, 360, 70};
}

static void render_role_reveal(AppContext *ctx, GameState *game)
{
    int pid = ctx->private_view_player;

    draw_text_centered(ctx->renderer, ctx->font_big,
                       0, 120, WINDOW_W, COLOR_TEXT,
                       "Private role reveal");

    char who[160];
    snprintf(who, sizeof(who), "Please pass the computer to %s",
             game->players[pid].name);
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 220, WINDOW_W, COLOR_DIM, who);

    if (!ctx->private_view_revealed) {
        draw_text_centered(ctx->renderer, ctx->font_mid,
                           0, 320, WINDOW_W, COLOR_DIM,
                           "Make sure nobody else is looking,");
        draw_text_centered(ctx->renderer, ctx->font_mid,
                           0, 360, WINDOW_W, COLOR_DIM,
                           "then click the button below to view your role.");
        draw_button(ctx, role_reveal_btn(), "Show my role", true);
    } else {
        const char *role_text = (game->players[pid].role == ROLE_GOLD_DIGGER)
            ? "MINER (gold digger)"
            : "SABOTEUR";
        SDL_Color role_color = (game->players[pid].role == ROLE_GOLD_DIGGER)
            ? COLOR_GOOD : COLOR_BAD;

        draw_text_centered(ctx->renderer, ctx->font_big,
                           0, 320, WINDOW_W, role_color, role_text);
        draw_text_centered(ctx->renderer, ctx->font_mid,
                           0, 410, WINDOW_W, COLOR_DIM,
                           "Memorize it, then click Done.");
        draw_button(ctx, role_reveal_btn(), "Done", true);
    }
}

/* ---------- 換手畫面（多人模式） ---------- */

static SDL_Rect handoff_btn(void)
{
    return (SDL_Rect){WINDOW_W/2 - 180, 480, 360, 70};
}

static void render_handoff(AppContext *ctx, GameState *game)
{
    int pid = current_player_index(game);
    char who[160];
    snprintf(who, sizeof(who), "Pass the computer to: %s",
             game->players[pid].name);
    draw_text_centered(ctx->renderer, ctx->font_big,
                       0, 200, WINDOW_W, COLOR_TEXT, who);
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 300, WINDOW_W, COLOR_DIM,
                       "Other players: please look away.");
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 360, WINDOW_W, COLOR_DIM,
                       "Active player: click Continue when ready.");
    draw_button(ctx, handoff_btn(), "Continue", true);
}

/* ---------- 私密結果畫面，例如地圖牌 ---------- */

static SDL_Rect private_result_btn(void)
{
    return (SDL_Rect){WINDOW_W/2 - 180, 520, 360, 70};
}

static void render_private_result(AppContext *ctx)
{
    draw_text_centered(ctx->renderer, ctx->font_big,
                       0, 160, WINDOW_W, COLOR_TEXT,
                       "Private result");
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 270, WINDOW_W, COLOR_DIM,
                       "Only the current player should read this.");
    draw_text_centered(ctx->renderer, ctx->font_big,
                       0, 360, WINDOW_W, COLOR_ACCENT,
                       ctx->private_result_message);
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 450, WINDOW_W, COLOR_DIM,
                       "Memorize it, then continue.");
    draw_button(ctx, private_result_btn(), "Continue", true);
}

/* ---------- 主遊戲畫面 ---------- */

#define BTN_ROTATE_IDX  0
#define BTN_DISCARD_IDX 1
#define BTN_CANCEL_IDX  2
#define BTN_NEWGAME_IDX 3

static void render_play(AppContext *ctx, GameState *game)
{
    int pid = current_player_index(game);

    /* 標題列 */
    char header[200];
    snprintf(header, sizeof(header),
             "%s  |  Mode: %s  |  Players: %d  |  Cards left: %d",
             "Saboteur Mini",
             game->mode == MODE_AI ? "Solo vs AI" : "Multiplayer",
             game->player_count, game->draw_count);
    draw_text(ctx->renderer, ctx->font_mid, 20, 20, COLOR_TEXT, header);

    char turn_info[160];
    snprintf(turn_info, sizeof(turn_info), "Turn: %s%s",
             game->players[pid].name,
             game->is_ai[pid] ? " [AI thinking...]" : "");
    draw_text(ctx->renderer, ctx->font_mid,
              20, 50, is_human_turn(game) ? COLOR_ACCENT : COLOR_DIM,
              turn_info);

    /* 版圖 */
    for (int row = VIEW_ROW_MIN; row < VIEW_ROW_MAX; ++row) {
        for (int col = VIEW_COL_MIN; col < VIEW_COL_MAX; ++col) {
            const Cell *cell = &game->board[row][col];
            SDL_Rect rect = cell_rect(row, col);

            bool highlight = false;
            if (ctx->pending == PENDING_USE_MAP && cell->occupied
                && cell->is_goal && !cell->goal_revealed) {
                highlight = true;
            } else if (ctx->pending == PENDING_ROCKFALL && cell->occupied
                && !cell->is_start && !cell->is_goal
                && cell->card.kind == CARD_PATH) {
                highlight = true;
            } else if (ctx->pending == PENDING_PLACE_PATH
                && ctx->selected_hand >= 0
                && is_human_turn(game)
                && !cell->occupied) {
                const Player *me = &game->players[pid];
                if (ctx->selected_hand < me->hand_count
                    && me->hand[ctx->selected_hand].kind == CARD_PATH) {
                    Card preview = me->hand[ctx->selected_hand];
                    if (ctx->preview_rotated_180) {
                        preview = rotate_path_card_180(preview);
                    }
                    highlight = is_valid_path_placement(game, preview, row, col, NULL, 0);
                }
            }
            draw_board_cell(ctx, rect, cell, highlight);
        }
    }

    /* 右側面板 */
    draw_player_panel(ctx, game);

    /* 動作按鈕 */
    bool can_act = is_human_turn(game);
    draw_button(ctx, action_btn_rect(BTN_ROTATE_IDX),
                ctx->preview_rotated_180 ? "Rot: 180" : "Rot: 0",
                can_act && ctx->pending == PENDING_PLACE_PATH);
    draw_button(ctx, action_btn_rect(BTN_DISCARD_IDX),
                "Discard",
                can_act && ctx->selected_hand >= 0);
    draw_button(ctx, action_btn_rect(BTN_CANCEL_IDX),
                "Cancel",
                can_act && (ctx->pending != PENDING_NONE
                            || ctx->selected_hand >= 0));
    draw_button(ctx, action_btn_rect(BTN_NEWGAME_IDX),
                "Menu", true);

    /* 手牌（只顯示當前玩家的牌；輪到 AI 時不顯示牌面） */
    if (is_human_turn(game)) {
        const Player *me = &game->players[pid];
        for (int i = 0; i < MAX_HAND_SIZE; ++i) {
            SDL_Rect r = hand_card_rect(i);
            if (i < me->hand_count) {
                draw_hand_card(ctx, r, &me->hand[i],
                               i == ctx->selected_hand, i + 1);
            } else {
                draw_hand_card(ctx, r, NULL, false, i + 1);
            }
        }
    } else {
        for (int i = 0; i < MAX_HAND_SIZE; ++i) {
            SDL_Rect r = hand_card_rect(i);
            fill_rect(ctx->renderer, r, (SDL_Color){50, 53, 60, 255});
            draw_rect(ctx->renderer, r, COLOR_BORDER);
        }
        draw_text(ctx->renderer, ctx->font_mid,
                  30, HAND_Y + 70, COLOR_DIM,
                  "(Hand hidden while it is not your turn.)");
    }

    /* 狀態訊息 */
    draw_text(ctx->renderer, ctx->font_small,
              20, HAND_Y + HAND_CARD_H + 4, COLOR_TEXT, ctx->message);

    draw_public_log_panel(ctx, PANEL_X, PANEL_Y + PANEL_H + 10,
                          PANEL_W, 240, "Public log");

    /* 卡牌說明提示（右下角） */
    // const char *hints[] = {
    //         "Path: place on board",
    //         "Break: sabotage a player's tool (P/L/C)",
    //         "Repair: fix a broken tool",
    //         "Map: peek at a hidden goal (private)",
    //         "Rockfall: remove a placed path card",
    //     };
    //     int hint_y = PANEL_Y + PANEL_H + 10;
    //     for (int i = 0; i < 5; ++i) {
    //         draw_text(ctx->renderer, ctx->font_small,
    //                 PANEL_X + 10, hint_y + i * 20, COLOR_DIM, hints[i]);
    //     }
}

/* ---------- 結束畫面 ---------- */

static SDL_Rect round_continue_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 - 150, 710, 300, 56};
}

static SDL_Rect gold_choice_rect(int index)
{
    return (SDL_Rect){430 + index * 94, 350, 76, 76};
}

static void render_round_result(AppContext *ctx, GameState *game)
{
    const char *winner = game->round_won_by_gold_diggers
        ? "Miners found the gold"
        : "Saboteurs stopped the miners";
    SDL_Color winner_color = game->round_won_by_gold_diggers
        ? COLOR_GOOD
        : COLOR_BAD;

    draw_text_centered(ctx->renderer, ctx->font_big,
                       0, 42, WINDOW_W, winner_color, "ROUND RESULT");
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 96, WINDOW_W, COLOR_TEXT, winner);

    SDL_Rect roles = {40, 150, 330, 420};
    fill_rect(ctx->renderer, roles, COLOR_PANEL);
    draw_rect(ctx->renderer, roles, COLOR_BORDER);
    draw_text(ctx->renderer, ctx->font_mid,
              roles.x + 14, roles.y + 12, COLOR_TEXT, "Roles this round");
    for (int i = 0; i < game->player_count; ++i) {
        char line[128];
        snprintf(line, sizeof(line), "%s%s - %s",
                 game->players[i].name,
                 game->is_ai[i] ? " [AI]" : "",
                 role_label(game->players[i].role));
        draw_text(ctx->renderer, ctx->font_small,
                  roles.x + 16, roles.y + 50 + i * 28,
                  game->players[i].role == ROLE_SABOTEUR ? COLOR_BAD : COLOR_GOOD,
                  line);
    }

    SDL_Rect reward = {400, 150, 500, 420};
    fill_rect(ctx->renderer, reward, COLOR_PANEL);
    draw_rect(ctx->renderer, reward, COLOR_BORDER);
    draw_text(ctx->renderer, ctx->font_mid,
              reward.x + 14, reward.y + 12, COLOR_TEXT, "Gold reward");

    if (game->round_won_by_gold_diggers) {
        if (!ctx->round_score_settled
            && ctx->gold_choice_player != NO_PLAYER) {
            char prompt[180];
            snprintf(prompt, sizeof(prompt), "%s chooses one gold card.",
                     game->players[ctx->gold_choice_player].name);
            draw_text(ctx->renderer, ctx->font_mid,
                      reward.x + 20, reward.y + 72, COLOR_ACCENT, prompt);
            draw_text(ctx->renderer, ctx->font_small,
                      reward.x + 20, reward.y + 108, COLOR_DIM,
                      "Click a value below. The remaining values pass counterclockwise.");

            for (int i = 0; i < ctx->pending_gold_count; ++i) {
                SDL_Rect choice = gold_choice_rect(i);
                char value[16];
                snprintf(value, sizeof(value), "%d", ctx->pending_gold_values[i]);
                draw_button(ctx, choice, value, true);
            }
        } else {
            draw_text(ctx->renderer, ctx->font_mid,
                      reward.x + 20, reward.y + 84, COLOR_GOOD,
                      "All gold cards have been assigned.");
        }
    } else {
        int y = reward.y + 78;
        bool any = false;
        for (int i = 0; i < game->player_count; ++i) {
            if (game->players[i].role != ROLE_SABOTEUR) {
                continue;
            }
            any = true;
            char line[160];
            snprintf(line, sizeof(line), "%s now has %d gold.",
                     game->players[i].name, game->players[i].gold_total);
            draw_text(ctx->renderer, ctx->font_mid,
                      reward.x + 20, y, COLOR_ACCENT, line);
            y += 34;
        }
        if (!any) {
            draw_text(ctx->renderer, ctx->font_mid,
                      reward.x + 20, y, COLOR_DIM,
                      "No saboteur was in play, so no gold was awarded.");
        }
    }

    draw_public_log_panel(ctx, 930, 150, 310, 420, "Public log");

    char summary[160];
    snprintf(summary, sizeof(summary), "Gold deck remaining: %d",
             game->gold_draw_count);
    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 640, WINDOW_W, COLOR_DIM, summary);

    draw_button(ctx, round_continue_rect(),
                game->round_number >= MAX_ROUNDS ? "Final score" : "Next round",
                ctx->round_score_settled);
}

static SDL_Rect end_restart_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 - 320, 500, 280, 70};
}
static SDL_Rect end_menu_rect(void)
{
    return (SDL_Rect){WINDOW_W/2 - 20, 500, 280, 70};
}

static void render_end(AppContext *ctx, GameState *game)
{
    draw_text_centered(ctx->renderer, ctx->font_big,
                       0, 120, WINDOW_W, COLOR_ACCENT,
                       "GAME OVER - FINAL SCORE");

    int highest = 0;
    for (int i = 0; i < game->player_count; ++i) {
        if (i == 0 || game->players[i].gold_total > highest) {
            highest = game->players[i].gold_total;
        }
    }

    draw_text_centered(ctx->renderer, ctx->font_mid,
                       0, 190, WINDOW_W, COLOR_DIM,
                       "After 3 rounds, the player with the most gold wins.");

    int y = 250;
    for (int i = 0; i < game->player_count; ++i) {
        char line[180];
        snprintf(line, sizeof(line), "%s%s  -  Gold: %d  -  Last role: %s%s",
                 game->players[i].gold_total == highest ? "* " : "  ",
                 game->players[i].name,
                 game->players[i].gold_total,
                 game->players[i].role == ROLE_GOLD_DIGGER
                    ? "Miner" : "Saboteur",
                 game->is_ai[i] ? " [AI]" : "");
        draw_text_centered(ctx->renderer, ctx->font_mid,
                           0, y, WINDOW_W,
                           game->players[i].gold_total == highest
                              ? COLOR_ACCENT : COLOR_TEXT,
                           line);
        y += 34;
    }

    draw_button(ctx, end_restart_rect(), "New game (same mode)", true);
    draw_button(ctx, end_menu_rect(), "Back to menu", true);
}

/* ---------- 主繪製入口 ---------- */

void render_frame(AppContext *ctx, GameState *game)
{
    set_color(ctx->renderer, COLOR_BG);
    SDL_RenderClear(ctx->renderer);

    switch (ctx->screen) {
        case SCREEN_MENU:        render_menu(ctx); break;
        case SCREEN_MP_SETUP:    render_mp_setup(ctx); break;
        case SCREEN_AI_SETUP:    render_ai_setup(ctx); break;
        case SCREEN_ROLE_REVEAL: render_role_reveal(ctx, game); break;
        case SCREEN_HANDOFF:     render_handoff(ctx, game); break;
        case SCREEN_PRIVATE_RESULT: render_private_result(ctx); break;
        case SCREEN_PLAY:        render_play(ctx, game); break;
        case SCREEN_ROUND_RESULT: render_round_result(ctx, game); break;
        case SCREEN_END:         render_end(ctx, game); break;
    }
    SDL_RenderPresent(ctx->renderer);
}

/* ---------- 遊戲初始化輔助 ---------- */

static void start_game_ai_mode(AppContext *ctx, GameState *game)
{
    /* 1 名人類 + 2 名 AI = 3 玩家。 */
    char names[3][MAX_NAME_LEN];
    snprintf(names[0], MAX_NAME_LEN, "%s",
             ctx->ai_human_name[0] ? ctx->ai_human_name : "You");
    snprintf(names[1], MAX_NAME_LEN, "AI 1");
    snprintf(names[2], MAX_NAME_LEN, "AI 2");
    bool is_ai[3] = {false, true, true};

    game_seed_random((unsigned int)SDL_GetTicks());
    if (!game_init_with_mode(game, 3, (const char (*)[MAX_NAME_LEN])names,
                             MODE_AI, is_ai)) {
        set_message(ctx, "Failed to init game.");
        return;
    }
    reset_public_log(ctx);
    reset_round_result_state(ctx);
    render_add_public_log(ctx, "Round %d started.", game->round_number);

    /*
     * AI mode 下，AI 玩家不需要交互查看角色，直接視為已查看。
     * 只有人類玩家需要走 SCREEN_ROLE_REVEAL。
     */
    for (int i = 0; i < game->player_count; ++i) {
        if (game->is_ai[i]) {
            mark_role_checked(game, i);
        }
    }
    ctx->private_view_player = 0;
    ctx->private_view_revealed = false;
    ctx->screen = SCREEN_ROLE_REVEAL;
    clear_pending(ctx);
    set_message(ctx, "Confirm your role to begin.");
}

static void start_game_mp_mode(AppContext *ctx, GameState *game)
{
    game_seed_random((unsigned int)SDL_GetTicks());
    if (!game_init_with_mode(
            game, ctx->mp_player_count,
            (const char (*)[MAX_NAME_LEN])ctx->mp_names,
            MODE_MULTIPLAYER, NULL)) {
        set_message(ctx, "Failed to init game.");
        return;
    }
    reset_public_log(ctx);
    reset_round_result_state(ctx);
    render_add_public_log(ctx, "Round %d started.", game->round_number);
    ctx->private_view_player =
        game_next_player_to_check_role(game);
    ctx->private_view_revealed = false;
    ctx->screen = SCREEN_ROLE_REVEAL;
    clear_pending(ctx);
}

static void after_role_confirmed(AppContext *ctx, GameState *game)
{
    mark_role_checked(game, ctx->private_view_player);

    int next = game_next_player_to_check_role(game);
    if (next == NO_PLAYER) {
        /*
         * 所有人都看完角色，進入遊戲。
         * 如果第一位玩家就是 AI（人機模式不打亂順序，所以不會發生，
         * 但 MP 模式中 starting_player 是隨機的），直接呈現 PLAY 畫面。
         */
        ctx->screen = SCREEN_PLAY;
        clear_pending(ctx);
        set_message(ctx, "Game started. Place a path card, use an action card, or discard.");
        return;
    }
    ctx->private_view_player = next;
    ctx->private_view_revealed = false;
}

static void after_human_action(AppContext *ctx, GameState *game)
{
    clear_pending(ctx);
    render_resolve_round_if_needed(ctx, game);
    if (ctx->screen == SCREEN_ROUND_RESULT || ctx->screen == SCREEN_END) return;

    /*
     * 多人模式：每次行動後切到換手畫面。
     * 人機模式：如果下一位是 AI 不切畫面，留在 PLAY 畫面交給主迴圈跑 AI。
     */
    if (game->mode == MODE_MULTIPLAYER) {
        ctx->screen = SCREEN_HANDOFF;
    }
}

/* ---------- 事件處理：選單 ---------- */

static bool handle_menu_event(AppContext *ctx, const SDL_Event *e)
{
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_ESCAPE) return false;
        if (e->key.keysym.sym == SDLK_1) {
            ctx->ai_human_name[0] = '\0';
            ctx->screen = SCREEN_AI_SETUP;
            SDL_StartTextInput();
        }
        if (e->key.keysym.sym == SDLK_2) {
            for (int i = 0; i < MAX_PLAYERS; ++i) ctx->mp_names[i][0] = '\0';
            ctx->mp_player_count = 3;
            ctx->mp_name_focus = 1;
            ctx->screen = SCREEN_MP_SETUP;
            SDL_StartTextInput();
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN
        && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        if (point_in_rect(mx, my, menu_btn_ai())) {
            ctx->ai_human_name[0] = '\0';
            ctx->screen = SCREEN_AI_SETUP;
            SDL_StartTextInput();
        } else if (point_in_rect(mx, my, menu_btn_mp())) {
            for (int i = 0; i < MAX_PLAYERS; ++i) ctx->mp_names[i][0] = '\0';
            ctx->mp_player_count = 3;
            ctx->mp_name_focus = 1;
            ctx->screen = SCREEN_MP_SETUP;
            SDL_StartTextInput();
        } else if (point_in_rect(mx, my, menu_btn_quit())) {
            return false;
        }
    }
    return true;
}

/* ---------- 事件處理：AI 設定 ---------- */

static void append_text(char *dst, size_t cap, const char *text)
{
    size_t len = strlen(dst);
    size_t add = strlen(text);
    if (len + add >= cap) return;
    memcpy(dst + len, text, add + 1);
}

static bool handle_ai_setup_event(
    AppContext *ctx, GameState *game, const SDL_Event *e)
{
    if (e->type == SDL_TEXTINPUT) {
        append_text(ctx->ai_human_name, MAX_NAME_LEN, e->text.text);
        return true;
    }
    if (e->type == SDL_KEYDOWN) {
        if (e->key.keysym.sym == SDLK_BACKSPACE) {
            size_t len = strlen(ctx->ai_human_name);
            if (len > 0) ctx->ai_human_name[len - 1] = '\0';
        }
        if (e->key.keysym.sym == SDLK_RETURN
            && ctx->ai_human_name[0] != '\0') {
            SDL_StopTextInput();
            start_game_ai_mode(ctx, game);
        }
        if (e->key.keysym.sym == SDLK_ESCAPE) {
            SDL_StopTextInput();
            ctx->screen = SCREEN_MENU;
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN
        && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        if (point_in_rect(mx, my, ai_setup_start_rect())
            && ctx->ai_human_name[0] != '\0') {
            SDL_StopTextInput();
            start_game_ai_mode(ctx, game);
        } else if (point_in_rect(mx, my, ai_setup_back_rect())) {
            SDL_StopTextInput();
            ctx->screen = SCREEN_MENU;
        }
    }
    return true;
}

/* ---------- 事件處理：多人設定 ---------- */

static bool handle_mp_setup_event(
    AppContext *ctx, GameState *game, const SDL_Event *e)
{
    if (e->type == SDL_TEXTINPUT) {
        if (ctx->mp_name_focus >= 1
            && ctx->mp_name_focus <= ctx->mp_player_count) {
            append_text(ctx->mp_names[ctx->mp_name_focus - 1],
                        MAX_NAME_LEN, e->text.text);
        }
        return true;
    }
    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode k = e->key.keysym.sym;
        if (k == SDLK_ESCAPE) { SDL_StopTextInput(); ctx->screen = SCREEN_MENU; }
        if (k == SDLK_BACKSPACE) {
            if (ctx->mp_name_focus >= 1
                && ctx->mp_name_focus <= ctx->mp_player_count) {
                char *n = ctx->mp_names[ctx->mp_name_focus - 1];
                size_t len = strlen(n);
                if (len > 0) n[len - 1] = '\0';
            }
        }
        if (k == SDLK_TAB) {
            ctx->mp_name_focus =
                (ctx->mp_name_focus % (ctx->mp_player_count + 1)) + 1;
            if (ctx->mp_name_focus > ctx->mp_player_count) {
                ctx->mp_name_focus = 0;
            }
        }
        if (k == SDLK_PLUS || k == SDLK_EQUALS || k == SDLK_KP_PLUS) {
            if (ctx->mp_player_count < MAX_PLAYERS) ctx->mp_player_count++;
        }
        if (k == SDLK_MINUS || k == SDLK_KP_MINUS) {
            if (ctx->mp_player_count > MIN_PLAYERS) ctx->mp_player_count--;
            if (ctx->mp_name_focus > ctx->mp_player_count) {
                ctx->mp_name_focus = ctx->mp_player_count;
            }
        }
        if (k == SDLK_RETURN && mp_names_ready(ctx)) {
            SDL_StopTextInput();
            start_game_mp_mode(ctx, game);
        }
    }
    if (e->type == SDL_MOUSEBUTTONDOWN
        && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        if (point_in_rect(mx, my, mp_minus_rect())) {
            if (ctx->mp_player_count > MIN_PLAYERS) ctx->mp_player_count--;
            if (ctx->mp_name_focus > ctx->mp_player_count) {
                ctx->mp_name_focus = ctx->mp_player_count;
            }
            return true;
        }
        if (point_in_rect(mx, my, mp_plus_rect())) {
            if (ctx->mp_player_count < MAX_PLAYERS) ctx->mp_player_count++;
            return true;
        }
        if (point_in_rect(mx, my, mp_count_rect())) {
            ctx->mp_name_focus = 0;
        }
        for (int i = 0; i < ctx->mp_player_count; ++i) {
            if (point_in_rect(mx, my, mp_name_rect(i))) {
                ctx->mp_name_focus = i + 1;
            }
        }
        if (point_in_rect(mx, my, mp_start_rect()) && mp_names_ready(ctx)) {
            SDL_StopTextInput();
            start_game_mp_mode(ctx, game);
        }
        if (point_in_rect(mx, my, mp_back_rect())) {
            SDL_StopTextInput();
            ctx->screen = SCREEN_MENU;
        }
    }
    return true;
}

/* ---------- 事件處理：角色查看 ---------- */

static bool handle_role_reveal_event(
    AppContext *ctx, GameState *game, const SDL_Event *e)
{
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) {
        return false;
    }
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_RETURN) {
        if (!ctx->private_view_revealed) {
            ctx->private_view_revealed = true;
        } else {
            after_role_confirmed(ctx, game);
        }
        return true;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN
        && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        if (point_in_rect(mx, my, role_reveal_btn())) {
            if (!ctx->private_view_revealed) {
                ctx->private_view_revealed = true;
            } else {
                after_role_confirmed(ctx, game);
            }
        }
    }
    return true;
}

/* ---------- 事件處理：換手 ---------- */

static bool handle_handoff_event(
    AppContext *ctx, GameState *game, const SDL_Event *e)
{
    (void)game;
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) return false;
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_RETURN) {
        ctx->screen = SCREEN_PLAY;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN
        && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        if (point_in_rect(mx, my, handoff_btn())) {
            ctx->screen = SCREEN_PLAY;
        }
    }
    return true;
}

static bool handle_private_result_event(
    AppContext *ctx, GameState *game, const SDL_Event *e)
{
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) return false;

    bool confirmed = false;
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_RETURN) {
        confirmed = true;
    }
    if (e->type == SDL_MOUSEBUTTONDOWN
        && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        if (point_in_rect(mx, my, private_result_btn())) {
            confirmed = true;
        }
    }

    if (confirmed) {
        render_resolve_round_if_needed(ctx, game);
        if (ctx->screen == SCREEN_ROUND_RESULT || ctx->screen == SCREEN_END) {
            return true;
        }
        ctx->screen = (game->mode == MODE_MULTIPLAYER)
            ? SCREEN_HANDOFF
            : SCREEN_PLAY;
    }
    return true;
}

/* ---------- 事件處理：主遊戲畫面 ---------- */

static void play_select_hand(AppContext *ctx, const GameState *game, int idx)
{
    int pid = current_player_index(game);
    if (idx < 0 || idx >= game->players[pid].hand_count) return;

    ctx->selected_hand = idx;
    ctx->preview_rotated_180 = false;
    Card card = game->players[pid].hand[idx];

    if (card.kind == CARD_PATH) {
        ctx->pending = PENDING_PLACE_PATH;
        set_message(ctx, "Click an empty board cell to place. R = rotate 180.");
    } else if (card.action_kind == ACTION_BREAK_TOOL) {
        ctx->pending = PENDING_BREAK_TOOL;
        set_message(ctx, "Click a target player's tool icon to break it.");
    } else if (card.action_kind == ACTION_REPAIR_TOOL) {
        ctx->pending = PENDING_REPAIR_TOOL;
        set_message(ctx, "Click a broken tool icon (yours or teammate) to repair.");
    } else if (card.action_kind == ACTION_MAP) {
        ctx->pending = PENDING_USE_MAP;
        set_message(ctx, "Map: click a hidden goal card to peek privately.");
    } else if (card.action_kind == ACTION_ROCKFALL) {
        ctx->pending = PENDING_ROCKFALL;
        set_message(ctx, "Rockfall: click a placed path card to remove it.");
    } else {
        ctx->pending = PENDING_NONE;
        set_message(ctx, "Unknown action card. You can discard it.");
    }
}

static void play_try_place(AppContext *ctx, GameState *game, int row, int col)
{
    int pid = current_player_index(game);
    char reason[256];
    if (play_path_card(game, pid, ctx->selected_hand, row, col,
                       ctx->preview_rotated_180, reason, sizeof(reason))) {
        render_add_public_log(ctx, "%s placed a path at (%d, %d)%s.",
                              game->players[pid].name, row, col,
                              ctx->preview_rotated_180 ? " rotated" : "");
        set_message(ctx, reason);
        after_human_action(ctx, game);
    } else {
        set_message(ctx, reason);
    }
}


static void play_try_break(
    AppContext *ctx, GameState *game, int target_player, int tool_idx)
{
    int pid = current_player_index(game);
    char reason[256];
    if (play_break_tool(game, pid, ctx->selected_hand,
                        target_player, (ToolType)tool_idx,
                        reason, sizeof(reason))) {
        render_add_public_log(ctx, "%s broke %s's %s.",
                              game->players[pid].name,
                              game->players[target_player].name,
                              tool_label(tool_idx));
        set_message(ctx, reason);
        after_human_action(ctx, game);
    } else {
        set_message(ctx, reason);
    }
}

static void play_try_repair(
    AppContext *ctx, GameState *game, int target_player, int tool_idx)
{
    int pid = current_player_index(game);
    char reason[256];
    if (play_repair_tool(game, pid, ctx->selected_hand,
                         target_player, (ToolType)tool_idx,
                         reason, sizeof(reason))) {
        render_add_public_log(ctx, "%s repaired %s's %s.",
                              game->players[pid].name,
                              game->players[target_player].name,
                              tool_label(tool_idx));
        set_message(ctx, reason);
        after_human_action(ctx, game);
    } else {
        set_message(ctx, reason);
    }
}

static void play_try_map(AppContext *ctx, GameState *game, int row, int col)
{
    int pid = current_player_index(game);
    bool contains_gold = false;
    char reason[256];

    if (play_map_card(game, pid, ctx->selected_hand, row, col,
                      &contains_gold, reason, sizeof(reason))) {
        render_add_public_log(ctx, "%s used Map on goal (%d, %d).",
                              game->players[pid].name, row, col);
        snprintf(ctx->private_result_message,
                 sizeof(ctx->private_result_message),
                 contains_gold ? "This goal is GOLD." : "This goal is ROCK.");
        set_message(ctx, "Map card used.");
        clear_pending(ctx);
        ctx->screen = SCREEN_PRIVATE_RESULT;
    } else {
        set_message(ctx, reason);
    }
}

static void play_try_rockfall(AppContext *ctx, GameState *game, int row, int col)
{
    int pid = current_player_index(game);
    char reason[256];

    if (play_rockfall_card(game, pid, ctx->selected_hand,
                           row, col, reason, sizeof(reason))) {
        render_add_public_log(ctx, "%s used Rockfall at (%d, %d).",
                              game->players[pid].name, row, col);
        set_message(ctx, reason);
        after_human_action(ctx, game);
    } else {
        set_message(ctx, reason);
    }
}

static void play_try_discard(AppContext *ctx, GameState *game)
{
    int pid = current_player_index(game);
    char reason[256];
    if (discard_card(game, pid, ctx->selected_hand, reason, sizeof(reason))) {
        render_add_public_log(ctx, "%s discarded a card.",
                              game->players[pid].name);
        set_message(ctx, reason);
        after_human_action(ctx, game);
    } else {
        set_message(ctx, reason);
    }
}

static bool handle_play_event(
    AppContext *ctx, GameState *game, const SDL_Event *e)
{
    /* 遊戲結束時不再處理玩家輸入。 */
    if (game->phase == PHASE_GAME_RESULT) return true;

    if (e->type == SDL_KEYDOWN) {
        SDL_Keycode k = e->key.keysym.sym;
        if (k == SDLK_ESCAPE) return false;
        if (k == SDLK_r && ctx->pending == PENDING_PLACE_PATH) {
            ctx->preview_rotated_180 = !ctx->preview_rotated_180;
            set_message(ctx, "Rotated preview.");
        }
        if (is_human_turn(game)) {
            if (k >= SDLK_1 && k <= SDLK_6) {
                play_select_hand(ctx, game, k - SDLK_1);
            }
            if (k == SDLK_d && ctx->selected_hand >= 0) {
                play_try_discard(ctx, game);
            }
        }
    }

    if (e->type == SDL_MOUSEBUTTONDOWN
        && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;

        /* 動作按鈕一律先檢查 */
        if (point_in_rect(mx, my, action_btn_rect(BTN_NEWGAME_IDX))) {
            ctx->screen = SCREEN_MENU;
            return true;
        }
        if (!is_human_turn(game)) return true;

        if (point_in_rect(mx, my, action_btn_rect(BTN_ROTATE_IDX))
            && ctx->pending == PENDING_PLACE_PATH) {
            ctx->preview_rotated_180 = !ctx->preview_rotated_180;
            return true;
        }
        if (point_in_rect(mx, my, action_btn_rect(BTN_DISCARD_IDX))
            && ctx->selected_hand >= 0) {
            play_try_discard(ctx, game);
            return true;
        }
        if (point_in_rect(mx, my, action_btn_rect(BTN_CANCEL_IDX))) {
            clear_pending(ctx);
            set_message(ctx, "Selection cleared.");
            return true;
        }

        /* 手牌 */
        for (int i = 0; i < MAX_HAND_SIZE; ++i) {
            if (point_in_rect(mx, my, hand_card_rect(i))) {
                play_select_hand(ctx, game, i);
                return true;
            }
        }

        /* 工具圖示（用於破壞/修理） */
        if (ctx->pending == PENDING_BREAK_TOOL
            || ctx->pending == PENDING_REPAIR_TOOL) {
            for (int p = 0; p < game->player_count; ++p) {
                for (int t = 0; t < TOOL_COUNT; ++t) {
                    if (point_in_rect(mx, my, tool_icon_rect(p, t))) {
                        if (ctx->pending == PENDING_BREAK_TOOL) {
                            play_try_break(ctx, game, p, t);
                        } else {
                            play_try_repair(ctx, game, p, t);
                        }
                        return true;
                    }
                }
            }
        }

        /* 版圖 */
        int row, col;
        if (screen_to_board_cell(mx, my, &row, &col)) {
            if (ctx->pending == PENDING_PLACE_PATH
                && ctx->selected_hand >= 0) {
                play_try_place(ctx, game, row, col);
            } else if (ctx->pending == PENDING_USE_MAP
                && ctx->selected_hand >= 0) {
                play_try_map(ctx, game, row, col);
            } else if (ctx->pending == PENDING_ROCKFALL
                && ctx->selected_hand >= 0) {
                play_try_rockfall(ctx, game, row, col);
            }
            return true;
        }
    }
    return true;
}

/* ---------- 事件處理：回合結算 ---------- */

static bool handle_round_result_event(
    AppContext *ctx, GameState *game, const SDL_Event *e)
{
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) {
        return false;
    }

    if (e->type == SDL_KEYDOWN
        && e->key.keysym.sym == SDLK_RETURN
        && ctx->round_score_settled) {
        continue_after_round_result(ctx, game);
        return true;
    }

    if (e->type == SDL_MOUSEBUTTONDOWN
        && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x;
        int my = e->button.y;

        if (!ctx->round_score_settled
            && game->round_won_by_gold_diggers
            && ctx->gold_choice_player != NO_PLAYER) {
            for (int i = 0; i < ctx->pending_gold_count; ++i) {
                if (point_in_rect(mx, my, gold_choice_rect(i))) {
                    award_pending_gold_choice(ctx, game, i);
                    auto_award_ai_gold_choices(ctx, game);
                    return true;
                }
            }
        }

        if (ctx->round_score_settled
            && point_in_rect(mx, my, round_continue_rect())) {
            continue_after_round_result(ctx, game);
        }
    }

    return true;
}

/* ---------- 事件處理：結束畫面 ---------- */

static bool handle_end_event(
    AppContext *ctx, GameState *game, const SDL_Event *e)
{
    if (e->type == SDL_KEYDOWN && e->key.keysym.sym == SDLK_ESCAPE) return false;

    if (e->type == SDL_MOUSEBUTTONDOWN
        && e->button.button == SDL_BUTTON_LEFT) {
        int mx = e->button.x, my = e->button.y;
        if (point_in_rect(mx, my, end_restart_rect())) {
            /*
             * 同模式重來：保留玩家名單，重新洗牌、發牌、隨機角色。
             */
            if (game->mode == MODE_AI) {
                start_game_ai_mode(ctx, game);
            } else {
                start_game_mp_mode(ctx, game);
            }
        } else if (point_in_rect(mx, my, end_menu_rect())) {
            ctx->screen = SCREEN_MENU;
        }
    }
    return true;
}

/* ---------- 對外入口 ---------- */

bool render_init(AppContext *ctx)
{
    memset(ctx, 0, sizeof(*ctx));
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) return false;
    if (TTF_Init() != 0) return false;

    ctx->window = SDL_CreateWindow(
        "Saboteur Mini - Original Rules",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_W, WINDOW_H, 0);
    if (!ctx->window) return false;
    ctx->renderer = SDL_CreateRenderer(
        ctx->window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ctx->renderer) return false;

    ctx->font_big   = load_font(36);
    ctx->font_mid   = load_font(22);
    ctx->font_small = load_font(16);
    if (!ctx->font_big || !ctx->font_mid || !ctx->font_small) return false;

    ctx->screen = SCREEN_MENU;
    ctx->selected_hand = -1;
    ctx->mp_player_count = 3;
    return true;
}

void render_destroy(AppContext *ctx)
{
    if (ctx->font_big) TTF_CloseFont(ctx->font_big);
    if (ctx->font_mid) TTF_CloseFont(ctx->font_mid);
    if (ctx->font_small) TTF_CloseFont(ctx->font_small);
    if (ctx->renderer) SDL_DestroyRenderer(ctx->renderer);
    if (ctx->window) SDL_DestroyWindow(ctx->window);
    TTF_Quit();
    SDL_Quit();
}

bool render_handle_event(
    AppContext *ctx, GameState *game, const SDL_Event *e)
{
    if (e->type == SDL_QUIT) return false;
    switch (ctx->screen) {
        case SCREEN_MENU:        return handle_menu_event(ctx, e);
        case SCREEN_MP_SETUP:    return handle_mp_setup_event(ctx, game, e);
        case SCREEN_AI_SETUP:    return handle_ai_setup_event(ctx, game, e);
        case SCREEN_ROLE_REVEAL: return handle_role_reveal_event(ctx, game, e);
        case SCREEN_HANDOFF:     return handle_handoff_event(ctx, game, e);
        case SCREEN_PRIVATE_RESULT: return handle_private_result_event(ctx, game, e);
        case SCREEN_PLAY:        return handle_play_event(ctx, game, e);
        case SCREEN_ROUND_RESULT: return handle_round_result_event(ctx, game, e);
        case SCREEN_END:         return handle_end_event(ctx, game, e);
    }
    return true;
}
