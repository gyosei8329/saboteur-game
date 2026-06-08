/*
 * game.c
 * Saboteur / 矮人礦坑（基礎版）數位化遊戲：整局與每輪初始化流程
 *
 * 本模組負責：
 *   - 取得玩家姓名並建立一局遊戲
 *   - 隨機排列固定遊玩順序
 *   - 初始化第 1 至第 3 輪
 *   - 建立並發放角色牌與玩法牌
 *   - 放置起點與三張隱藏終點牌
 *   - 在單輪結算後進入下一輪或總結算
 *
 * 本模組不負責：
 *   - 出牌是否合法與出牌效果（rule.c）
 *   - 淘金矮人選取金塊牌的結算流程（score.c）
 *   - 按鈕、畫面與私人資訊遮罩（ui_raylib.c）
 */

#include "game.h"
#include "card.h"
#include "rule.h"
#include "score.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

/* ---------- 固定版圖起點與終點位置 ---------- */

/*
 * 起點到中間終點相隔七個可放置位置：
 * 起點 col = 2，中間終點 col = 10。
 *
 * 三張終點上下排列，每兩張終點之間留一格：
 * row = 10、12、14。
 */
static const BoardPosition START_POSITION = {12, 2};
static const BoardPosition GOAL_POSITIONS[GOAL_CARD_COUNT] = {
    {10, 10},
    {12, 10},
    {14, 10}
};

_Static_assert(
    MAX_PLAY_CARDS >= BASE_PLAY_DECK_COUNT,
    "MAX_PLAY_CARDS must hold the entire base play deck."
);

_Static_assert(
    GOAL_CARD_COUNT == BASE_GOAL_CARD_COUNT,
    "Goal-card count in game.h and card.h must match."
);

/* ---------- 內部工具函式 ---------- */

static bool is_valid_player_count(int player_count)
{
    return player_count >= MIN_PLAYERS && player_count <= MAX_PLAYERS;
}

static bool position_is_inside_board(BoardPosition position)
{
    return position.row >= 0 && position.row < BOARD_H
        && position.col >= 0 && position.col < BOARD_W;
}

static bool player_name_is_valid(const char name[MAX_NAME_LEN])
{
    if (name == NULL) {
        return false;
    }

    bool found_visible_character = false;
    bool found_terminator = false;

    for (int i = 0; i < MAX_NAME_LEN; ++i) {
        unsigned char character = (unsigned char)name[i];

        if (character == '\0') {
            found_terminator = true;
            break;
        }

        if (!isspace(character)) {
            found_visible_character = true;
        }
    }

    return found_terminator && found_visible_character;
}

static bool player_names_are_valid(
    int player_count,
    const char names[][MAX_NAME_LEN]
)
{
    if (names == NULL || !is_valid_player_count(player_count)) {
        return false;
    }

    for (int i = 0; i < player_count; ++i) {
        if (!player_name_is_valid(names[i])) {
            return false;
        }

        for (int j = 0; j < i; ++j) {
            if (strncmp(names[i], names[j], MAX_NAME_LEN) == 0) {
                /*
                 * 重複名稱會讓單機交接畫面無法清楚識別玩家，
                 * 因此第一版直接拒絕建立遊戲。
                 */
                return false;
            }
        }
    }

    return true;
}

static void clear_round_player_state(Player *player)
{
    if (player == NULL) {
        return;
    }

    player->hand_count = 0;
    memset(player->hand, 0, sizeof(player->hand));
    memset(player->broken_tools, 0, sizeof(player->broken_tools));
    memset(player->active_broken_tool_cards, 0,
           sizeof(player->active_broken_tool_cards));
    player->checked_role = false;
}

static void shuffle_players(Player *players, int count)
{
    if (players == NULL || count <= 1) {
        return;
    }

    for (int i = count - 1; i > 0; --i) {
        int target = rand() % (i + 1);
        Player temporary = players[i];
        players[i] = players[target];
        players[target] = temporary;
    }
}

static void shuffle_roles(Role *roles, int count)
{
    if (roles == NULL || count <= 1) {
        return;
    }

    for (int i = count - 1; i > 0; --i) {
        int target = rand() % (i + 1);
        Role temporary = roles[i];
        roles[i] = roles[target];
        roles[target] = temporary;
    }
}

static bool deal_one_card(GameState *game, int player_id)
{
    if (game == NULL
        || player_id < 0
        || player_id >= game->player_count
        || game->draw_count <= 0
        || game->players[player_id].hand_count >= MAX_HAND_SIZE) {
        return false;
    }

    Player *player = &game->players[player_id];
    player->hand[player->hand_count++] = game->draw_pile[--game->draw_count];
    return true;
}

static bool deal_initial_hands(GameState *game)
{
    int hand_size = initial_hand_size(game->player_count);
    if (hand_size <= 0 || hand_size > MAX_HAND_SIZE) {
        return false;
    }

    /* 依本輪順序一圈一張輪流發牌。 */
    for (int card_number = 0; card_number < hand_size; ++card_number) {
        for (int offset = 0; offset < game->player_count; ++offset) {
            int player_id =
                (game->starting_player + offset) % game->player_count;

            if (!deal_one_card(game, player_id)) {
                return false;
            }
        }
    }

    return true;
}

static bool place_initial_board(GameState *game)
{
    if (game == NULL || !position_is_inside_board(START_POSITION)) {
        return false;
    }

    for (int i = 0; i < GOAL_CARD_COUNT; ++i) {
        if (!position_is_inside_board(GOAL_POSITIONS[i])) {
            return false;
        }
    }

    memset(game->board, 0, sizeof(game->board));

    Cell *start = &game->board[START_POSITION.row][START_POSITION.col];
    start->occupied = true;
    start->is_start = true;
    start->card = card_make_start();

    Card goal_cards[GOAL_CARD_COUNT];
    card_build_goal_cards(goal_cards);

    int gold_goal_index = rand() % GOAL_CARD_COUNT;

    for (int i = 0; i < GOAL_CARD_COUNT; ++i) {
        Cell *goal =
            &game->board[GOAL_POSITIONS[i].row][GOAL_POSITIONS[i].col];

        goal->occupied = true;
        goal->is_goal = true;
        goal->goal_revealed = false;
        goal->contains_gold = (i == gold_goal_index);
        goal->card = goal_cards[i];
    }

    return true;
}

/* ---------- 對外公開函式 ---------- */

void game_seed_random(unsigned int seed)
{
    srand(seed);
}

BoardPosition game_get_start_position(void)
{
    return START_POSITION;
}

BoardPosition game_get_goal_position(int goal_index)
{
    if (goal_index < 0 || goal_index >= GOAL_CARD_COUNT) {
        BoardPosition invalid = {-1, -1};
        return invalid;
    }

    return GOAL_POSITIONS[goal_index];
}

bool game_init(
    GameState *game,
    int player_count,
    const char names[][MAX_NAME_LEN]
)
{
    /* 預設為多人模式，所有玩家皆為人類。 */
    bool no_ai[MAX_PLAYERS] = {false};
    return game_init_with_mode(game, player_count, names,
                               MODE_MULTIPLAYER, no_ai);
}

bool game_init_with_mode(
    GameState *game,
    int player_count,
    const char names[][MAX_NAME_LEN],
    GameMode mode,
    const bool *is_ai
)
{
    if (game == NULL || !player_names_are_valid(player_count, names)) {
        return false;
    }

    memset(game, 0, sizeof(*game));

    game->player_count = player_count;
    game->round_number = 1;
    game->starting_player = 0;
    game->current_player = 0;
    game->last_player_connected_goal = NO_PLAYER;
    game->last_action_player = NO_PLAYER;
    game->phase = PHASE_SETUP;
    game->mode = mode;
    game->winner_is_gold_diggers = false;

    for (int i = 0; i < player_count; ++i) {
        (void)strncpy(game->players[i].name, names[i], MAX_NAME_LEN - 1);
        game->players[i].name[MAX_NAME_LEN - 1] = '\0';
        game->players[i].gold_total = 0;
        game->is_ai[i] = (is_ai != NULL) ? is_ai[i] : false;
    }

    score_initialize_gold_deck(game);

    /*
     * 名稱確認後，只在整局開始時隨機決定一次順序。
     * 為了讓人機模式中人類玩家有可預期的位置（players[0]），
     * 人機模式不打亂玩家順序；多人模式才打亂。
     */
    if (mode == MODE_MULTIPLAYER) {
        shuffle_players(game->players, game->player_count);
    }

    return game_setup_round(game);
}

bool game_setup_round(GameState *game)
{
    if (game == NULL
        || !is_valid_player_count(game->player_count)
        || game->round_number < 1
        || game->round_number > MAX_ROUNDS) {
        return false;
    }

    if (!card_base_deck_is_valid()) {
        return false;
    }

    game->phase = PHASE_SETUP;
    game->current_player = game->starting_player;
    game->last_player_connected_goal = NO_PLAYER;
    game->last_action_player = NO_PLAYER;
    game->round_won_by_gold_diggers = false;

    memset(game->draw_pile, 0, sizeof(game->draw_pile));
    memset(game->discard_pile, 0, sizeof(game->discard_pile));
    game->draw_count = 0;
    game->discard_count = 0;

    for (int i = 0; i < game->player_count; ++i) {
        clear_round_player_state(&game->players[i]);
    }

    if (!build_role_pool(
            game->player_count,
            game->role_pool,
            &game->role_pool_count)) {
        return false;
    }

    shuffle_roles(game->role_pool, game->role_pool_count);

    /* 一張未發出的角色牌保留在 role_pool 中，輪結束前不可公開。 */
    for (int i = 0; i < game->player_count; ++i) {
        game->players[i].role = game->role_pool[i];
    }

    size_t play_deck_count =
        card_build_play_deck(game->draw_pile, MAX_PLAY_CARDS);
    if (play_deck_count != BASE_PLAY_DECK_COUNT) {
        return false;
    }

    game->draw_count = (int)play_deck_count;
    shuffle_cards(game->draw_pile, game->draw_count);

    if (!deal_initial_hands(game) || !place_initial_board(game)) {
        return false;
    }

    game->phase = PHASE_ROLE_CHECK;
    game->current_player = game->starting_player;
    return true;
}

bool game_begin_next_round(GameState *game)
{
    if (game == NULL || game->phase != PHASE_ROUND_RESULT) {
        return false;
    }

    if (game->round_number >= MAX_ROUNDS) {
        game->phase = PHASE_GAME_RESULT;
        game->current_player = NO_PLAYER;
        return true;
    }

    ++game->round_number;

    /*
     * 官方規則：新一輪由前一輪最後出牌玩家左側的玩家開始。
     * 本程式的出牌方向以 index + 1 前進，因此左側即為 +1。
     * 測試或特殊結束流程若沒有紀錄最後出牌者，才退回輪替起始者。
     */
    if (game->last_action_player != NO_PLAYER) {
        game->starting_player =
            (game->last_action_player + 1) % game->player_count;
    } else {
        game->starting_player =
            (game->starting_player + 1) % game->player_count;
    }

    return game_setup_round(game);
}

int game_next_player_to_check_role(const GameState *game)
{
    if (game == NULL
        || game->phase != PHASE_ROLE_CHECK
        || !is_valid_player_count(game->player_count)) {
        return NO_PLAYER;
    }

    for (int offset = 0; offset < game->player_count; ++offset) {
        int player_id =
            (game->starting_player + offset) % game->player_count;

        if (!game->players[player_id].checked_role) {
            return player_id;
        }
    }

    return NO_PLAYER;
}
