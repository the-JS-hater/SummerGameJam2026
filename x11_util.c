#include "x11_util.h"


static KeyMap keymap;

void init_keymap(Display *display)
{
  keymap.esc        = XKeysymToKeycode(display, XK_Escape);
  keymap.w          = XKeysymToKeycode(display, XK_w);
  keymap.a          = XKeysymToKeycode(display, XK_a);
  keymap.s          = XKeysymToKeycode(display, XK_s);
  keymap.d          = XKeysymToKeycode(display, XK_d);
  keymap.r          = XKeysymToKeycode(display, XK_r);
  keymap.q          = XKeysymToKeycode(display, XK_q);
  keymap.f          = XKeysymToKeycode(display, XK_f);
  keymap.left_shift = XKeysymToKeycode(display, XK_Shift_L);
  keymap.left_ctrl  = XKeysymToKeycode(display, XK_Control_L);
}

void hide_cursor(AppConfig *cfg)
{
  char   data[1] = {0};
  Pixmap blank_pixmap =
    XCreateBitmapFromData(cfg->display, cfg->window, data, 1, 1);
  XColor dummy     = {0};
  Cursor invisible = XCreatePixmapCursor(cfg->display, blank_pixmap,
                                         blank_pixmap, &dummy, &dummy, 0, 0);
  XDefineCursor(cfg->display, cfg->window, invisible);
  XFreePixmap(cfg->display, blank_pixmap);
  XFreeCursor(cfg->display, invisible);
}

void create_window(AppConfig *cfg, char const *title)
{
  cfg->display = XOpenDisplay(NULL);
  if (!cfg->display)
  {
    fprintf(stderr, "Failed to open display");
    exit(1);
  }
  cfg->screen = DefaultScreen(cfg->display);
  cfg->visual = DefaultVisual(cfg->display, cfg->screen);
  cfg->depth  = DefaultDepth(cfg->display, cfg->screen);
  cfg->window = XCreateSimpleWindow(cfg->display,
                                    XDefaultRootWindow(cfg->display),  // parent
                                    0,                                 // x
                                    0,                                 // y
                                    cfg->win_w, cfg->win_h,
                                    0,           // border width
                                    0x00000000,  // border color
                                    0x00000000   // background color
  );
  if (cfg->borderless)
  {
    Atom motif_hints = XInternAtom(cfg->display, "_MOTIF_WM_HINTS", False);
    long hints[5]    = {(1L << 1), 0, 0, 0, 0};

    XChangeProperty(cfg->display, cfg->window, motif_hints, motif_hints, 32,
                    PropModeReplace, (unsigned char *)hints, 5);
  }
  XStoreName(cfg->display, cfg->window, title);
  XSelectInput(cfg->display, cfg->window, cfg->event_mask);
  XMapWindow(cfg->display, cfg->window);
  hide_cursor(cfg);
}

void close_window(AppConfig *cfg)
{
  XDestroyWindow(cfg->display, cfg->window);
  XCloseDisplay(cfg->display);
}

DisplayBuffer *init_display_buffer(unsigned width, unsigned height)
{
  DisplayBuffer *db = calloc(1, sizeof(*db));
  db->pixels        = calloc(1, width * height * sizeof(uint32_t));
  db->width         = width;
  db->height        = height;
  return db;
}

void resample_nearest(uint32_t const *restrict src, uint32_t *restrict dst,
                      uint32_t const db_width, uint32_t const db_height,
                      uint32_t const fb_width, uint32_t const fb_height)
{
  for (uint32_t y = 0; y < db_height; ++y)
  {
    uint32_t sy = y * fb_height / db_height;
    for (uint32_t x = 0; x < db_width; ++x)
    {
      uint32_t sx           = x * fb_width / db_width;
      dst[y * db_width + x] = src[sy * fb_width + sx];
    }
  }
}

void update_window(AppConfig const *cfg, XImage *render_img, XImage *disp_img,
                   DisplayBuffer *db, FrameBuffer *fb)
{
  resample_nearest(fb->color_buffer[fb->draw_idx], db->pixels, db->width,
                   db->height, fb->width, fb->height);
  XPutImage(cfg->display, cfg->window, DefaultGC(cfg->display, cfg->screen),
            disp_img,
            0,  // src_x
            0,  // src_y
            0,  // dest_x
            0,  // dest_y
            cfg->win_w, cfg->win_h);
  XFlush(cfg->display);
  fb->draw_idx     = !fb->draw_idx;
  render_img->data = (char *)fb->color_buffer[fb->draw_idx];
}

void poll_input(AppConfig *cfg, bool *quit, InputState *input)
{
  int const center_x = cfg->win_w / 2;
  int const center_y = cfg->win_h / 2;

  input->mouse_dx = 0;
  input->mouse_dy = 0;
  while (XPending(cfg->display) > 0)
  {
    XEvent event = {0};
    XNextEvent(cfg->display, &event);
    if (event.type == MotionNotify)
    {
      int const x = event.xmotion.x;
      int const y = event.xmotion.y;

      // XWarpPointer generates BS event
      if (x == center_x && y == center_y) continue;

      input->mouse_dx += x - center_x;
      input->mouse_dy += y - center_y;
    }
    if (event.type == KeyPress)
    {
      KeyCode kc = event.xkey.keycode;
      if (kc == keymap.esc)
        *quit = true;
      else if (kc == keymap.w)
        input->w = true;
      else if (kc == keymap.a)
        input->a = true;
      else if (kc == keymap.s)
        input->s = true;
      else if (kc == keymap.d)
        input->d = true;
      else if (kc == keymap.left_shift)
        input->shift = true;
      else if (kc == keymap.left_ctrl)
        input->ctrl = true;
      else if (kc == keymap.q)
        cfg->wireframe = !cfg->wireframe;
      else if (kc == keymap.f)
        cfg->fps = !cfg->fps;
    }
    if (event.type == KeyRelease)
    {
      KeyCode kc = event.xkey.keycode;
      if (kc == keymap.w)
        input->w = false;
      else if (kc == keymap.a)
        input->a = false;
      else if (kc == keymap.s)
        input->s = false;
      else if (kc == keymap.d)
        input->d = false;
      else if (kc == keymap.left_shift)
        input->shift = false;
      else if (kc == keymap.left_ctrl)
        input->ctrl = false;
    }
    XWarpPointer(cfg->display, None, cfg->window, 0, 0, 0, 0, center_x,
                 center_y);
    XFlush(cfg->display);
  }
}
