// Boti szoba panel – színkerék a spot lámpához (LVGL 9 canvas)
// A YAML-ben lévő canvasra rajzolja a HS színkört, kezeli az érintést,
// és elengedéskor visszaadja a választott színárnyalatot/telítettséget.
#pragma once

#include <cmath>
#include <functional>
#include "lvgl.h"

namespace spot_picker {

static lv_obj_t *wheel = nullptr;
static lv_obj_t *marker = nullptr;
static int size = 0;
static float cur_h = 0, cur_s = 0;
static bool dragging = false;
static bool has_value = false;  // van-e érvényes szín a HA-ból (kikapcsolt spotnál nincs)
static std::function<void(float, float)> on_pick;

static void place_marker() {
  if (marker == nullptr)
    return;
  if (!has_value) {
    lv_obj_add_flag(marker, LV_OBJ_FLAG_HIDDEN);
    return;
  }
  lv_obj_remove_flag(marker, LV_OBJ_FLAG_HIDDEN);
  // A még meg nem jelenített oldalon a koordináták 0-k, előbb ki kell számoltatni
  lv_obj_update_layout(wheel);
  float r = size / 2.0f;
  float a = cur_h * (float) M_PI / 180.0f;
  float d = cur_s / 100.0f * (r - 2);
  int x = (int) (r + d * cosf(a));
  int y = (int) (r + d * sinf(a));
  lv_obj_set_pos(marker, lv_obj_get_x(wheel) + x - 13, lv_obj_get_y(wheel) + y - 13);
}

// Érintési pontból színárnyalat és telítettség (0° = piros jobbra, óramutató szerint)
static void point_to_hs(lv_event_t *e) {
  lv_point_t p;
  lv_indev_get_point(lv_indev_active(), &p);
  lv_area_t area;
  lv_obj_get_coords(wheel, &area);
  float r = size / 2.0f;
  float dx = p.x - (area.x1 + r), dy = p.y - (area.y1 + r);
  float h = atan2f(dy, dx) * 180.0f / (float) M_PI;
  if (h < 0)
    h += 360;
  float s = sqrtf(dx * dx + dy * dy) / r * 100.0f;
  cur_h = h;
  cur_s = s > 100 ? 100 : s;
  has_value = true;
  place_marker();
}

static void on_event(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED || code == LV_EVENT_PRESSING) {
    dragging = true;
    point_to_hs(e);
  } else if (code == LV_EVENT_RELEASED) {
    dragging = false;
    if (on_pick)
      on_pick(cur_h, cur_s);
  }
}

static void draw() {
  lv_draw_buf_t *buf = lv_canvas_get_draw_buf(wheel);
  float r = size / 2.0f;
  for (int y = 0; y < size; y++) {
    uint8_t *row = buf->data + y * buf->header.stride;
    for (int x = 0; x < size; x++) {
      float dx = x + 0.5f - r, dy = y + 0.5f - r;
      float d = sqrtf(dx * dx + dy * dy);
      uint8_t *px = row + x * 4;  // ARGB8888: B, G, R, A
      if (d > r) {
        px[0] = px[1] = px[2] = px[3] = 0;
        continue;
      }
      float h = atan2f(dy, dx) * 180.0f / (float) M_PI;
      if (h < 0)
        h += 360;
      lv_color_t c = lv_color_hsv_to_rgb((uint16_t) h, (uint8_t) (d / r * 100), 100);
      px[0] = c.blue;
      px[1] = c.green;
      px[2] = c.red;
      // A kör szélét 1 px-en elsimítjuk
      px[3] = d > r - 1 ? (uint8_t) ((r - d) * 255) : 255;
    }
  }
  lv_obj_invalidate(wheel);
}

// A HA-ból érkező aktuális szín; húzás közben nem írjuk felül.
// NAN = nincs szín (pl. kikapcsolt spot): a jelölő eltűnik.
static void set_hs(float h, float s) {
  if (dragging)
    return;
  has_value = !std::isnan(h) && !std::isnan(s);
  if (has_value) {
    cur_h = h;
    cur_s = s > 100 ? 100 : s;
  }
  place_marker();
}

static void setup(lv_obj_t *canvas, int canvas_size, std::function<void(float, float)> cb) {
  if (wheel != nullptr)
    return;
  wheel = canvas;
  size = canvas_size;
  on_pick = cb;
  draw();
  lv_obj_add_flag(wheel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(wheel, LV_OBJ_FLAG_SCROLL_CHAIN);
  lv_obj_add_event_cb(wheel, on_event, LV_EVENT_ALL, nullptr);

  // Jelölő: fehér keretes kör, ami nem veszi el az érintést a keréktől
  marker = lv_obj_create(lv_obj_get_parent(wheel));
  lv_obj_set_size(marker, 26, 26);
  lv_obj_set_style_radius(marker, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_opa(marker, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_color(marker, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_border_width(marker, 4, 0);
  lv_obj_set_style_shadow_width(marker, 6, 0);
  lv_obj_remove_flag(marker, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_remove_flag(marker, LV_OBJ_FLAG_SCROLLABLE);
  place_marker();
}

}  // namespace spot_picker
