// Boti szoba panel – a négy játék logikája (LVGL 9, ESPHome lambdákból hívva)
// Párkereső, Simon mondja, amőba (1 vagy 2 játékos), vakondos (olvasás nélkül is játszható)
// A játéktér gombjait itt hozzuk létre; a YAML csak a keretet adja
// (játéktér konténer, állapotfelirat, Vissza/Újra gombok).
#pragma once

#include <cstdio>
#include <functional>
#include "esp_random.h"
#include "lvgl.h"

namespace boti_games {

static uint32_t rnd(uint32_t n) { return esp_random() % n; }

static lv_obj_t *make_btn(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color, lv_event_cb_t cb,
                          intptr_t idx) {
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_set_pos(b, x, y);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(b, 16, 0);
  lv_obj_set_style_shadow_width(b, 0, 0);
  lv_obj_set_style_border_width(b, 0, 0);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, (void *) idx);
  return b;
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font) {
  lv_obj_t *l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
  lv_label_set_text(l, "");
  lv_obj_center(l);
  return l;
}

static void prep_area(lv_obj_t *area) {
  lv_obj_remove_flag(area, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(area, 0, 0);
}

static void set_status(lv_obj_t *status, const char *txt, uint32_t color = 0xFFFFFF) {
  lv_label_set_text(status, txt);
  lv_obj_set_style_text_color(status, lv_color_hex(color), 0);
}

static int idx_of(lv_event_t *e) { return (int) (intptr_t) lv_event_get_user_data(e); }

// Hangjelzések a buzzeren. A YAML állítja be: (hányszor, be ms, szünet ms)
static std::function<void(int, int, int)> sound;
enum Sound { SND_TICK, SND_OK, SND_BAD, SND_WIN };
static void play(Sound s) {
  if (!sound)
    return;
  switch (s) {
    case SND_TICK: sound(1, 40, 0); break;      // gombnyomás, villanás
    case SND_OK: sound(1, 150, 0); break;       // talált
    case SND_BAD: sound(1, 600, 0); break;      // hiba: hosszú búgás
    case SND_WIN: sound(3, 120, 100); break;    // győzelem: három csipogás
  }
}

// Egyszeri késleltetett hívás. A játszma sorszámát is magával viszi, hogy egy
// közben indított új játékba ne nyúljon bele (pl. „Újra” a gép gondolkodása alatt).
static void after(uint32_t ms, lv_timer_cb_t cb, uint32_t game = 0) {
  lv_timer_t *t = lv_timer_create(cb, ms, (void *) (uintptr_t) game);
  lv_timer_set_repeat_count(t, 1);
}
static uint32_t game_of(lv_timer_t *t) { return (uint32_t) (uintptr_t) lv_timer_get_user_data(t); }

// ============================================================ 1. Párkereső (4×4)
namespace memory {

static const char *const SYMBOLS[8] = {LV_SYMBOL_AUDIO, LV_SYMBOL_HOME,   LV_SYMBOL_BELL, LV_SYMBOL_GPS,
                                       LV_SYMBOL_CHARGE, LV_SYMBOL_TINT, LV_SYMBOL_EYE_OPEN, LV_SYMBOL_SETTINGS};
static const uint32_t COLORS[8] = {0xE0533D, 0x3FA34D, 0x2D7DD2, 0xF5B700, 0x7B4FB8, 0xFF7A00, 0x14B8A6, 0xD946EF};
static const uint32_t BACK = 0x3A4A6B;

static lv_obj_t *status = nullptr;
static lv_obj_t *cards[16];
static lv_obj_t *labels[16];
static int val[16];
static bool done[16];
static int first = -1, second = -1, pairs = 0, moves = 0;
static bool busy = false;
static uint32_t game = 0;

static void show(int i, bool face) {
  lv_obj_set_style_bg_color(cards[i], lv_color_hex(face ? COLORS[val[i]] : BACK), 0);
  lv_label_set_text(labels[i], face ? SYMBOLS[val[i]] : "?");
}

static void update_status() {
  char buf[48];
  if (pairs == 8) {
    snprintf(buf, sizeof(buf), "Ügyes vagy! %d lépés", moves);
    set_status(status, buf, 0xF5B700);
  } else {
    snprintf(buf, sizeof(buf), "Lépések: %d", moves);
    set_status(status, buf);
  }
}

static void hide_pair(lv_timer_t *t) {
  if (game_of(t) != game)
    return;
  show(first, false);
  show(second, false);
  first = second = -1;
  busy = false;
}

static void on_card(lv_event_t *e) {
  int i = idx_of(e);
  if (busy || done[i] || i == first)
    return;
  show(i, true);
  play(SND_TICK);
  if (first < 0) {
    first = i;
    return;
  }
  second = i;
  moves++;
  if (val[first] == val[second]) {
    done[first] = done[second] = true;
    pairs++;
    first = second = -1;
    play(pairs == 8 ? SND_WIN : SND_OK);
  } else {
    busy = true;
    play(SND_BAD);
    after(900, hide_pair, game);
  }
  update_status();
}

static void new_game() {
  game++;
  for (int i = 0; i < 16; i++)
    val[i] = i / 2;
  for (int i = 15; i > 0; i--) {
    int j = rnd(i + 1);
    int t = val[i];
    val[i] = val[j];
    val[j] = t;
  }
  for (int i = 0; i < 16; i++) {
    done[i] = false;
    show(i, false);
  }
  first = second = -1;
  pairs = moves = 0;
  busy = false;
  update_status();
}

static void setup(lv_obj_t *area, lv_obj_t *status_label) {
  if (status != nullptr)
    return;
  status = status_label;
  prep_area(area);
  for (int i = 0; i < 16; i++) {
    cards[i] = make_btn(area, 67 + (i % 4) * 88, 5 + (i / 4) * 88, 82, 82, BACK, on_card, i);
    labels[i] = make_label(cards[i], &lv_font_montserrat_40);
  }
  new_game();
}

}  // namespace memory

// ============================================================ 2. Simon mondja
namespace simon {

static const uint32_t DIM[4] = {0x1E7B34, 0x8B1E1E, 0x8A7A12, 0x1C3F8F};
static const uint32_t LIT[4] = {0x4CFF6A, 0xFF4D4D, 0xFFE23D, 0x4D8DFF};
static const int MAX_LEN = 64;

static lv_obj_t *status = nullptr;
static lv_obj_t *pads[4];
static int seq[MAX_LEN];
static int len = 0, pos = 0, tick = 0;
static bool showing = false, over = false;
static lv_timer_t *show_timer = nullptr;
static uint32_t game = 0;

static void light(int p, bool on) { lv_obj_set_style_bg_color(pads[p], lv_color_hex(on ? LIT[p] : DIM[p]), 0); }
static void all_off() {
  for (int p = 0; p < 4; p++)
    light(p, false);
}

static void show_tick(lv_timer_t *) {
  int step = tick / 2;
  if (step >= len) {
    lv_timer_delete(show_timer);
    show_timer = nullptr;
    showing = false;
    pos = 0;
    set_status(status, "Te jössz!", 0x4CFF6A);
    return;
  }
  if (tick % 2 == 0) {
    light(seq[step], true);
    // Ha közben elnavigáltak a játékról, ne csipogjon a háttérben
    if (lv_obj_get_screen(pads[0]) == lv_screen_active())
      play(SND_TICK);
  }
  else
    all_off();
  tick++;
}

static void start_show() {
  if (show_timer != nullptr)
    lv_timer_delete(show_timer);
  showing = true;
  tick = 0;
  all_off();
  set_status(status, "Figyelj!");
  show_timer = lv_timer_create(show_tick, 450, nullptr);
}

static void add_step() {
  if (len < MAX_LEN)
    seq[len++] = rnd(4);
}

static void next_round(lv_timer_t *t) {
  if (game_of(t) == game)
    start_show();
}

static void unflash(lv_timer_t *) {
  if (!showing)
    all_off();
}

static void on_pad(lv_event_t *e) {
  int p = idx_of(e);
  if (showing || over)
    return;
  light(p, true);
  after(250, unflash);
  char buf[48];
  if (p != seq[pos]) {
    over = true;
    play(SND_BAD);
    snprintf(buf, sizeof(buf), "Hoppá! Pont: %d", len - 1);
    set_status(status, buf, 0xFF4D4D);
    return;
  }
  if (++pos != len) {
    play(SND_TICK);
  } else {
    play(SND_OK);
    snprintf(buf, sizeof(buf), "Ügyes! Pont: %d", len);
    set_status(status, buf, 0xF5B700);
    add_step();
    showing = true;  // a következő bemutatásig ne lehessen nyomkodni
    after(1000, next_round, game);
  }
}

static void new_game() {
  game++;
  len = 0;
  over = false;
  add_step();
  start_show();
}

static void setup(lv_obj_t *area, lv_obj_t *status_label) {
  if (status != nullptr)
    return;
  status = status_label;
  prep_area(area);
  for (int p = 0; p < 4; p++)
    pads[p] = make_btn(area, 64 + (p % 2) * 182, 2 + (p / 2) * 176, 170, 170, DIM[p], on_pad, p);
  new_game();
}

}  // namespace simon

// ============================================================ 3. Amőba (3×3, gép ellen vagy két játékos)
namespace tictactoe {

static const int LINES[8][3] = {{0, 1, 2}, {3, 4, 5}, {6, 7, 8}, {0, 3, 6},
                                {1, 4, 7}, {2, 5, 8}, {0, 4, 8}, {2, 4, 6}};
static const uint32_t CELL = 0x2A3550, X_COLOR = 0xF5B700, O_COLOR = 0x22D3EE, WIN = 0x3FA34D;

static lv_obj_t *status = nullptr;
static lv_obj_t *cells[9];
static lv_obj_t *labels[9];
static lv_obj_t *mode_label = nullptr;
static char board[9];
static char turn = 'X';           // két játékosnál: ki következik
static bool two_players = false;
static bool busy = false, over = false;
static uint32_t game = 0;

static void ask_turn() {
  if (two_players)
    set_status(status, turn == 'X' ? "X jön" : "O jön", turn == 'X' ? X_COLOR : O_COLOR);
  else
    set_status(status, "Te jössz (X)");
}

static int winner_line(char who) {
  for (int l = 0; l < 8; l++)
    if (board[LINES[l][0]] == who && board[LINES[l][1]] == who && board[LINES[l][2]] == who)
      return l;
  return -1;
}

static bool full() {
  for (int i = 0; i < 9; i++)
    if (board[i] == ' ')
      return false;
  return true;
}

static void place(int i, char who) {
  board[i] = who;
  lv_label_set_text(labels[i], who == 'X' ? "X" : "O");
  lv_obj_set_style_text_color(labels[i], lv_color_hex(who == 'X' ? X_COLOR : O_COLOR), 0);
}

// Véget ért-e a játék; ha igen, kiírja az eredményt
static bool check_end() {
  int l = winner_line('X');
  if (l < 0)
    l = winner_line('O');
  if (l >= 0) {
    for (int k = 0; k < 3; k++)
      lv_obj_set_style_bg_color(cells[LINES[l][k]], lv_color_hex(WIN), 0);
    bool x_won = board[LINES[l][0]] == 'X';
    if (two_players)
      set_status(status, x_won ? "X nyert!" : "O nyert!", x_won ? X_COLOR : O_COLOR);
    else
      set_status(status, x_won ? "Nyertél!" : "Most én nyertem!", x_won ? 0xF5B700 : 0x22D3EE);
    // Két játékosnál mindig ünneplés; gép ellen a vereség búgás
    play(two_players || x_won ? SND_WIN : SND_BAD);
    over = true;
  } else if (full()) {
    set_status(status, "Döntetlen!");
    play(SND_OK);
    over = true;
  }
  return over;
}

// Kettőt már lerakott a 'who' egy sorban, a harmadik üres: ott a lépés
static int finishing_move(char who) {
  for (int l = 0; l < 8; l++) {
    int cnt = 0, empty = -1;
    for (int k = 0; k < 3; k++) {
      char c = board[LINES[l][k]];
      if (c == who)
        cnt++;
      else if (c == ' ')
        empty = LINES[l][k];
    }
    if (cnt == 2 && empty >= 0)
      return empty;
  }
  return -1;
}

static void computer_move(lv_timer_t *t) {
  if (game_of(t) != game)
    return;
  int m = finishing_move('O');
  // Nem tökéletes ellenfél: csak az esetek 3/4-ében véd, hogy a gyerek is nyerhessen
  if (m < 0 && rnd(4) != 0)
    m = finishing_move('X');
  if (m < 0 && board[4] == ' ' && rnd(2) == 0)
    m = 4;
  if (m < 0) {
    int free_cells[9], n = 0;
    for (int i = 0; i < 9; i++)
      if (board[i] == ' ')
        free_cells[n++] = i;
    m = free_cells[rnd(n)];
  }
  place(m, 'O');
  play(SND_TICK);
  busy = false;
  if (!check_end())
    ask_turn();
}

static void on_cell(lv_event_t *e) {
  int i = idx_of(e);
  if (busy || over || board[i] != ' ')
    return;
  if (two_players) {
    place(i, turn);
    play(SND_TICK);
    turn = turn == 'X' ? 'O' : 'X';
    if (!check_end())
      ask_turn();
    return;
  }
  place(i, 'X');
  play(SND_TICK);
  if (check_end())
    return;
  busy = true;
  set_status(status, "Gondolkodom…");
  after(600, computer_move, game);
}

static void new_game() {
  game++;
  for (int i = 0; i < 9; i++) {
    board[i] = ' ';
    lv_label_set_text(labels[i], "");
    lv_obj_set_style_bg_color(cells[i], lv_color_hex(CELL), 0);
  }
  busy = over = false;
  turn = 'X';
  ask_turn();
}

static void show_mode() { lv_label_set_text(mode_label, two_players ? "2 fő" : "1 fő"); }

// A fejléc „1 fő / 2 fő” gombja: vált, és új játékot kezd
static void toggle_mode() {
  two_players = !two_players;
  show_mode();
  new_game();
}

static void setup(lv_obj_t *area, lv_obj_t *status_label, lv_obj_t *mode_lbl) {
  if (status != nullptr)
    return;
  status = status_label;
  mode_label = mode_lbl;
  show_mode();
  prep_area(area);
  for (int i = 0; i < 9; i++) {
    cells[i] = make_btn(area, 67 + (i % 3) * 118, 5 + (i / 3) * 118, 110, 110, CELL, on_cell, i);
    labels[i] = make_label(cells[i], &lv_font_montserrat_48);
  }
  new_game();
}

}  // namespace tictactoe

// ============================================================ 4. Vakondos (olvasás nélkül is megy)
// A lyukakból felbukkanó vakondra kell bökni, mielőtt elbújik. Körönként 20 vakond,
// egyre gyorsabban. Találatnál csippan, a végén a pontszám látszik.
namespace mole {

static const int ROUND = 20;
static const uint32_t HOLE = 0x3B2A1E, MOLE = 0x9C6B3C, HIT = 0xF5B700;

static lv_obj_t *status = nullptr;
static lv_obj_t *area_obj = nullptr;
static lv_obj_t *holes[9];
static lv_obj_t *faces[9];
static int cur = -1, last = -1, ups = 0, hits = 0, ticks = 0;
static bool running = false, was_hit = false;

static lv_obj_t *dot(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color) {
  lv_obj_t *o = lv_obj_create(parent);
  lv_obj_remove_style_all(o);
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
  return o;
}

static void show_mole(int i, bool up, uint32_t color = MOLE) {
  lv_obj_set_style_bg_color(holes[i], lv_color_hex(up ? color : HOLE), 0);
  if (up)
    lv_obj_remove_flag(faces[i], LV_OBJ_FLAG_HIDDEN);
  else
    lv_obj_add_flag(faces[i], LV_OBJ_FLAG_HIDDEN);
}

static void update_status() {
  char buf[32];
  snprintf(buf, sizeof(buf), "Pont: %d", hits);
  set_status(status, buf);
}

static void finish() {
  running = false;
  char buf[48];
  snprintf(buf, sizeof(buf), "Vége! %d pont", hits);
  set_status(status, buf, 0xF5B700);
  play(hits >= ROUND * 3 / 4 ? SND_WIN : SND_OK);
}

static void pop() {
  int h;
  do {
    h = rnd(9);
  } while (h == last);
  cur = last = h;
  ups++;
  was_hit = false;
  // Kezdetben 1,4 mp-ig látszik, a kör végére 0,7 mp-re gyorsul
  int up = 14 - ups / 3;
  ticks = up < 7 ? 7 : up;
  show_mole(h, true);
}

// 100 ms-onként fut; csak akkor halad, ha a játék oldala látszik
static void tick(lv_timer_t *) {
  if (!running || lv_obj_get_screen(area_obj) != lv_screen_active())
    return;
  if (--ticks > 0)
    return;
  if (cur >= 0) {
    show_mole(cur, false);
    cur = -1;
    if (ups >= ROUND) {
      finish();
      return;
    }
    ticks = 3 + rnd(5);  // 0,3–0,7 mp szünet
  } else {
    pop();
  }
}

static void on_hole(lv_event_t *e) {
  int i = idx_of(e);
  if (!running || i != cur || was_hit)
    return;
  was_hit = true;
  hits++;
  show_mole(i, true, HIT);
  play(SND_OK);
  update_status();
  ticks = 3;  // a talált vakond még 0,3 mp-ig sárgán látszik
}

static void new_game() {
  for (int i = 0; i < 9; i++)
    show_mole(i, false);
  cur = last = -1;
  ups = hits = 0;
  ticks = 10;
  running = true;
  update_status();
}

static void setup(lv_obj_t *area, lv_obj_t *status_label) {
  if (status != nullptr)
    return;
  status = status_label;
  area_obj = area;
  prep_area(area);
  for (int i = 0; i < 9; i++) {
    holes[i] = make_btn(area, 67 + (i % 3) * 118, 5 + (i / 3) * 118, 110, 110, HOLE, on_hole, i);
    lv_obj_set_style_radius(holes[i], LV_RADIUS_CIRCLE, 0);
    // Arc: két szem és egy rózsaszín orr
    faces[i] = lv_obj_create(holes[i]);
    lv_obj_remove_style_all(faces[i]);
    lv_obj_set_size(faces[i], 70, 56);
    lv_obj_center(faces[i]);
    lv_obj_remove_flag(faces[i], LV_OBJ_FLAG_CLICKABLE);
    dot(faces[i], 8, 4, 18, 18, 0xFFFFFF);
    dot(faces[i], 44, 4, 18, 18, 0xFFFFFF);
    dot(faces[i], 14, 10, 8, 8, 0x111111);
    dot(faces[i], 50, 10, 8, 8, 0x111111);
    dot(faces[i], 24, 30, 22, 16, 0xF28BA8);
  }
  lv_timer_create(tick, 100, nullptr);
  new_game();
}

}  // namespace mole

}  // namespace boti_games
