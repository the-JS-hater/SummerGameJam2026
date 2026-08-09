#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <X11/Xlib.h>
#include <X11/keysym.h>

#include "render.h"

typedef struct {
  Window       window;
  Display     *display;
  Visual      *visual;
  long         event_mask;
  unsigned int win_w, win_h;
  unsigned int res_w, res_h;
  int          screen, depth;
  bool         borderless;
  bool         wireframe;
  bool         fps;
} AppConfig;

// TODO: replace with bitfield, or something else, later on
typedef struct {
  bool w;
  bool a;
  bool s;
  bool d;
  bool shift;
  bool ctrl;
  bool space;
  int  mouse_dx;
  int  mouse_dy;
} InputState;

typedef struct {
  uint32_t *pixels;
  unsigned  width, height;
} DisplayBuffer;

typedef struct {
  KeyCode esc, w, a, s, d, r, q, f;
  KeyCode left_shift, left_ctrl, space;
} KeyMap;


void init_keymap(Display *display);

void hide_cursor(AppConfig *cfg);


void create_window(AppConfig *cfg, char const *title);
void close_window(AppConfig *cfg);

DisplayBuffer *init_display_buffer(unsigned width, unsigned height);

void handle_resize(AppConfig *cfg, DisplayBuffer *db, XImage **disp_img);

void resample_nearest(uint32_t const *restrict src, uint32_t *restrict dst,
                      uint32_t const db_width, uint32_t const db_height,
                      uint32_t const fb_width, uint32_t const fb_height);


void update_window(AppConfig const *cfg, XImage *disp_img, DisplayBuffer *db,
                   FrameBuffer *fb);


void poll_input(AppConfig *cfg, bool *quit, InputState *input);
