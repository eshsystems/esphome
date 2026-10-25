// Boti szoba panel – zsebpénz-gyűjtés csillagokkal (LVGL 9, ESPHome lambdákból hívva)
// Mint a HA-s kids-savings-card: 10 csillag, mindegyik a cél árának 10%-a; a megkezdett
// csillag részben sárga. A kártya keretét a YAML adja, itt csak a csillagok és a számok
// készülnek. A csillagok betűkészletét a YAML állítja a csillag-konténeren (text_font).
#pragma once

#include <cmath>
#include <cstdio>
#include <cstring>
#include "lvgl.h"

namespace savings {

static const int STARS = 10;
static const int COLS = 5;
static const int CELL_W = 52, CELL_H = 46;
static const uint32_t STAR_CP = 0xF04CE;          // mdi-star
static const char *STAR = "\xF3\xB0\x93\x8E";     // ugyanez UTF-8-ban
static const uint32_t EMPTY_COLOR = 0x3A4560, FILL_COLOR = 0xF5B700;

struct Kid {
  lv_obj_t *fill[STARS] = {};  // a sárga csillagokat szélességre vágó keretek
  lv_obj_t *pct = nullptr, *saved_lbl = nullptr, *target_lbl = nullptr;
  int star_w = 0;
  float saved = NAN, target = NAN;
};
static Kid kids[2];

// 7490 -> "7 490 Ft"
static void money(char *buf, size_t n, const char *prefix, float v) {
  if (std::isnan(v)) {
    snprintf(buf, n, "%s–", prefix);
    return;
  }
  long x = lroundf(v);
  char digits[16];
  int len = snprintf(digits, sizeof(digits), "%ld", x < 0 ? -x : x);
  char grouped[24], *p = grouped;
  for (int i = 0; i < len; i++) {
    if (i > 0 && (len - i) % 3 == 0)
      *p++ = ' ';
    *p++ = digits[i];
  }
  *p = 0;
  snprintf(buf, n, "%s%s%s Ft", prefix, x < 0 ? "-" : "", grouped);
}

static void refresh(int k) {
  Kid &d = kids[k];
  if (d.pct == nullptr)
    return;
  bool ok = !std::isnan(d.saved) && !std::isnan(d.target) && d.target > 0;
  float ratio = ok ? d.saved / d.target : 0;
  ratio = ratio < 0 ? 0 : (ratio > 1 ? 1 : ratio);
  for (int i = 0; i < STARS; i++) {
    float part = ratio * STARS - i;
    part = part < 0 ? 0 : (part > 1 ? 1 : part);
    lv_obj_set_width(d.fill[i], (int) lroundf(part * d.star_w));
  }
  char buf[32];
  // Lefelé kerekít: 100% csak akkor látszik, ha tényleg összegyűlt
  snprintf(buf, sizeof(buf), "%d%%", (int) floorf(ratio * 100));
  lv_label_set_text(d.pct, ok ? buf : "–");
  money(buf, sizeof(buf), "", d.saved);
  lv_label_set_text(d.saved_lbl, buf);
  money(buf, sizeof(buf), "/ ", d.target);
  lv_label_set_text(d.target_lbl, buf);
}

// A HA-értékek bármikor jöhetnek, a beállítás előtt is: eltároljuk, és ha kész a kártya, frissítünk
static void set_saved(int k, float v) {
  kids[k].saved = v;
  refresh(k);
}
static void set_target(int k, float v) {
  kids[k].target = v;
  refresh(k);
}

static void setup(int k, lv_obj_t *stars, lv_obj_t *pct, lv_obj_t *saved_lbl, lv_obj_t *target_lbl) {
  Kid &d = kids[k];
  if (d.pct != nullptr)
    return;
  lv_obj_remove_flag(stars, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(stars, 0, 0);
  const lv_font_t *font = lv_obj_get_style_text_font(stars, LV_PART_MAIN);
  d.star_w = lv_font_get_glyph_width(font, STAR_CP, 0);
  int star_h = lv_font_get_line_height(font);
  for (int i = 0; i < STARS; i++) {
    int x = (i % COLS) * CELL_W, y = (i / COLS) * CELL_H;
    // Szürke csillag, fölötte a sárga, amit a keret csak a gyűjtött részig enged látni
    lv_obj_t *empty = lv_label_create(stars);
    lv_label_set_text(empty, STAR);
    lv_obj_set_style_text_color(empty, lv_color_hex(EMPTY_COLOR), 0);
    lv_obj_set_pos(empty, x, y);
    lv_obj_t *clip = lv_obj_create(stars);
    lv_obj_remove_style_all(clip);
    lv_obj_remove_flag(clip, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(clip, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(clip, x, y);
    lv_obj_set_size(clip, 0, star_h);
    lv_obj_t *full = lv_label_create(clip);
    lv_label_set_text(full, STAR);
    lv_obj_set_style_text_color(full, lv_color_hex(FILL_COLOR), 0);
    lv_obj_set_pos(full, 0, 0);
    d.fill[i] = clip;
  }
  d.pct = pct;
  d.saved_lbl = saved_lbl;
  d.target_lbl = target_lbl;
  refresh(k);
}

}  // namespace savings
