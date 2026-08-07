#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "include/stb_image.h"  //MAYBE: handroll png loader
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <math.h>

#include "linalg.h"
#include "obj_loader.h"

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

typedef struct {
  uint32_t *color_buffer[2];
  float    *depth_buffer[2];
  uint32_t  width, height;
  uint8_t   draw_idx;
} FrameBuffer;

typedef struct {
  uint32_t *pixels;
  unsigned  width, height;
} DisplayBuffer;

typedef struct {
  Vec3 camera_up, camera_front, camera_pos;
} Camera;

typedef uint32_t Color;

typedef enum {
  COLOR_R,
  COLOR_G,
  COLOR_B,
  COLOR_A,
  NORMAL_X,
  NORMAL_Y,
  NORMAL_Z,
  SURFACE_X,
  SURFACE_Y,
  SURFACE_Z,
  UV_U,
  UV_V,
  MAX_VARYING_ATTRS,
} AttributeEnum;

typedef struct {
  Vec4  pos;
  float varying[MAX_VARYING_ATTRS];
  float inv_w;
} Vertex;

typedef struct {
  int            width, height, channels;  // stb_image uses int
  unsigned char *data;
} Texture;

typedef struct {
  float ambient_coeff, diffuse_coeff;
  float shininess, specular_strength;
  Vec3  specular_color;
} Material;

typedef struct {
  Vec3 pos, color_vec;
} Light;

typedef enum { CLAMP, WRAP } SampleMode;

typedef struct {
  Vertex *verts;
  size_t  vertex_count;
  size_t *indices;
  size_t  index_count;
} Mesh;

typedef struct {
  Mesh     mesh;
  Mat4     mtw;
  Texture *tex;
  Material material;
} Model;

typedef struct {
  KeyCode esc, w, a, s, d, r, q, f;
  KeyCode left_shift, left_ctrl;
} KeyMap;

// TODO: replace with bitfield, or something else, later on
typedef struct {
  bool w;
  bool a;
  bool s;
  bool d;
  bool shift;
  bool ctrl;
  int  mouse_dx;
  int  mouse_dy;
} InputState;

// ============================================================================
// GLOBALS
// ============================================================================

static KeyMap          keymap;
static struct timespec last_frame;
Texture                tex0;
Texture                tex1;
Texture                texbottle;
static Light           light0;
static Vec3            ambient_light_color;

// ============================================================================
// PLUMBING & MISC
// ============================================================================

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

double get_frame_delta()
{
  struct timespec current_frame;
  clock_gettime(CLOCK_MONOTONIC, &current_frame);
  double dt = (current_frame.tv_sec - last_frame.tv_sec) +
              (current_frame.tv_nsec - last_frame.tv_nsec) / 1e9;


  last_frame = current_frame;
  return dt;
}

void parse_args(AppConfig *cfg, int argc, char *argv[])
{
  for (unsigned i = 1; i < argc; ++i)
  {
    if (!strcmp(argv[i], "-w"))
      sscanf(argv[++i], "%dx%d", &cfg->win_w, &cfg->win_h);
    else if (!strcmp(argv[i], "-r"))
      sscanf(argv[++i], "%dx%d", &cfg->res_w, &cfg->res_h);
    else if (!strcmp(argv[i], "-b"))
      cfg->borderless = true;
    else if (!strcmp(argv[i], "--wireframe"))
      cfg->wireframe = true;
    else if (!strcmp(argv[i], "--fps"))
      cfg->fps = true;
    else
    {
      fprintf(stderr, "Unknown argument: %s\n", argv[i]);
      exit(1);
    }
  }
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

FrameBuffer *init_framebuffer(uint32_t width, uint32_t height)
{
  FrameBuffer *frame_buffer  = calloc(1, sizeof(FrameBuffer));
  uint32_t    *color_buffer0 = calloc(1, width * height * sizeof(uint32_t));
  uint32_t    *color_buffer1 = calloc(1, width * height * sizeof(uint32_t));
  float       *depth_buffer0 = calloc(1, width * height * sizeof(float));
  float       *depth_buffer1 = calloc(1, width * height * sizeof(float));

  frame_buffer->width           = width;
  frame_buffer->height          = height;
  frame_buffer->draw_idx        = 0;
  frame_buffer->color_buffer[0] = color_buffer0;
  frame_buffer->depth_buffer[0] = depth_buffer0;
  frame_buffer->color_buffer[1] = color_buffer1;
  frame_buffer->depth_buffer[1] = depth_buffer1;

  return frame_buffer;
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

Mesh mesh_from_obj(ObjObject const *obj)
{
  Mesh mesh         = {0};
  mesh.vertex_count = obj->face_count * 3;
  mesh.index_count  = obj->face_count * 3;
  mesh.verts        = calloc(mesh.vertex_count, sizeof(Vertex));
  mesh.indices      = calloc(mesh.index_count, sizeof(size_t));

  if (!mesh.verts || !mesh.indices)
  {
    free(mesh.verts);
    free(mesh.indices);
    return (Mesh){0};
  }

  size_t out = 0;
  for (size_t i = 0; i < obj->face_count; i++)
  {
    obj_Face const *face = &obj->faces[i];
    for (int j = 0; j < 3; j++)
    {
      obj_FaceElement const *e   = &face->triangles[j];
      obj_Vertex const      *pos = &obj->verts[e->v_i - 1];
      mesh.verts[out].pos        = new_vec4(pos->x, pos->y, pos->z, pos->w);
      mesh.verts[out].varying[SURFACE_X] = pos->x;
      mesh.verts[out].varying[SURFACE_Y] = pos->y;
      mesh.verts[out].varying[SURFACE_Z] = pos->z;

      if (e->vn_i > 0)
      {
        obj_Normal const *n = &obj->normals[e->vn_i - 1];

        mesh.verts[out].varying[NORMAL_X] = n->x;
        mesh.verts[out].varying[NORMAL_Y] = n->y;
        mesh.verts[out].varying[NORMAL_Z] = n->z;
      }
      if (e->vt_i > 0)
      {
        obj_TexCoord const *t = &obj->uvs[e->vt_i - 1];

        mesh.verts[out].varying[UV_U] = t->u;
        mesh.verts[out].varying[UV_V] = t->v;
      }
      mesh.indices[out] = out;
      ++out;
    }
  }
  return mesh;
}

Model load_model(char const *filename)
{
  ObjObject obj = {0};
  if (!load_obj_file(filename, &obj))
  {
    fprintf(stderr, "Failed to load model: %s\n", filename);
    return (Model){0};
  }
  Model model = {.mesh = mesh_from_obj(&obj), .mtw = identity()};
  free_obj_object(&obj);
  return model;
}

void load_texture(Texture *tex, char const *filename)
{
  stbi_set_flip_vertically_on_load(true);
  tex->data = stbi_load(filename, &tex->width, &tex->height, &tex->channels, 0);
  if (tex->channels < 4)
  {
    fprintf(stderr, "Incompatible PNG file %s\n", filename);
    exit(1);
  }
}

// NOTE: Temporary, but might be repurposed to generate mesh from image
// data/heightmap?
Mesh generate_ground_mesh(float const size, float const target_quad_size,
                          float const uv_repeat)
{
  static uint32_t const MAX_SUBDIVISIONS = 256;

  uint32_t subdivisions = (uint32_t)(size / target_quad_size);
  if (subdivisions < 1) subdivisions = 1;
  if (subdivisions > MAX_SUBDIVISIONS) subdivisions = MAX_SUBDIVISIONS;

  uint32_t const verts_per_side = subdivisions + 1;
  uint32_t const quad_count     = subdivisions * subdivisions;

  Mesh mesh         = {0};
  mesh.vertex_count = (size_t)quad_count * 6;
  mesh.index_count  = mesh.vertex_count;
  mesh.verts        = calloc(mesh.vertex_count, sizeof(Vertex));
  mesh.indices      = calloc(mesh.index_count, sizeof(size_t));

  if (!mesh.verts || !mesh.indices)
  {
    free(mesh.verts);
    free(mesh.indices);
    return (Mesh){0};
  }
  float const half = size * 0.5f;
  float const step = size / (float)subdivisions;

  size_t out = 0;
  for (uint32_t row = 0; row < subdivisions; ++row)
  {
    for (uint32_t col = 0; col < subdivisions; ++col)
    {
      float const x0 = -half + (float)col * step;
      float const x1 = x0 + step;
      float const z0 = -half + (float)row * step;
      float const z1 = z0 + step;

      float const u0 = (float)col / (float)subdivisions * uv_repeat;
      float const u1 = (float)(col + 1) / (float)subdivisions * uv_repeat;
      float const v0 = (float)row / (float)subdivisions * uv_repeat;
      float const v1 = (float)(row + 1) / (float)subdivisions * uv_repeat;

      float const px[4] = {x0, x1, x1, x0};
      float const pz[4] = {z0, z0, z1, z1};
      float const pu[4] = {u0, u1, u1, u0};
      float const pv[4] = {v0, v0, v1, v1};

      static int const tri_a[3] = {0, 2, 1};
      static int const tri_b[3] = {0, 3, 2};

      for (int t = 0; t < 2; ++t)
      {
        int const *tri = (t == 0) ? tri_a : tri_b;
        for (int k = 0; k < 3; ++k)
        {
          int const c = tri[k];
          Vertex   *v = &mesh.verts[out];
          v->pos      = new_vec4(px[c], 0.0f, pz[c], 1.0f);

          v->varying[SURFACE_X] = px[c];
          v->varying[SURFACE_Y] = 0.0f;
          v->varying[SURFACE_Z] = pz[c];
          v->varying[NORMAL_X]  = 0.0f;
          v->varying[NORMAL_Y]  = 1.0f;
          v->varying[NORMAL_Z]  = 0.0f;
          v->varying[UV_U]      = pu[c];
          v->varying[UV_V]      = pv[c];

          mesh.indices[out] = out;
          ++out;
        }
      }
    }
  }
  return mesh;
}

// ============================================================================
// DRAWING
// ============================================================================

void draw_pixel(uint32_t const x, uint32_t const y, Color const color,
                FrameBuffer *fb)
{
  size_t const idx                    = y * fb->width + x;
  fb->color_buffer[fb->draw_idx][idx] = color;
}

void draw_line(FrameBuffer *fb, Vec4 const s, Vec4 const e, Color const color)
{
  int32_t x0 = (int32_t)roundf(s.x);
  int32_t y0 = (int32_t)roundf(s.y);
  int32_t x1 = (int32_t)roundf(e.x);
  int32_t y1 = (int32_t)roundf(e.y);

  if ((x0 < 0 && x1 < 0) || (y0 < 0 && y1 < 0) ||
      (x0 >= (int32_t)fb->width && x1 >= (int32_t)fb->width) ||
      (y0 >= (int32_t)fb->height && y1 >= (int32_t)fb->height))
  {
    return;
  }
  x0 = x0 < 0 ? 0 : (x0 >= (int32_t)fb->width ? fb->width - 1 : x0);
  y0 = y0 < 0 ? 0 : (y0 >= (int32_t)fb->height ? fb->height - 1 : y0);
  x1 = x1 < 0 ? 0 : (x1 >= (int32_t)fb->width ? fb->width - 1 : x1);
  y1 = y1 < 0 ? 0 : (y1 >= (int32_t)fb->height ? fb->height - 1 : y1);

  int32_t dx  = abs(x1 - x0);
  int32_t sx  = x0 < x1 ? 1 : -1;
  int32_t dy  = -abs(y1 - y0);
  int32_t sy  = y0 < y1 ? 1 : -1;
  int32_t err = dx + dy;

  while (true)
  {
    draw_pixel(x0, y0, color, fb);
    if (x0 == x1 && y0 == y1) break;

    int e2 = 2 * err;
    if (e2 >= dy)
    {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx)
    {
      err += dx;
      y0 += sy;
    }
  }
}

static inline uint32_t pack_color(float const in_r, float const in_g,
                                  float const in_b, float const in_a)
{
  uint32_t r = (uint32_t)(in_r);
  uint32_t g = (uint32_t)(in_g);
  uint32_t b = (uint32_t)(in_b);
  uint32_t a = (uint32_t)(in_a);
  return (a << 24) | (r << 16) | (g << 8) | b;
}

uint32_t sample_texture(Texture const *tex, SampleMode const mode,
                        float const u, float const v)
{
  // NOTE: getting rid of clamped and using % for wrapping idx this might be
  // alot more vectorizable, or somehow convice the compiler mode will be same
  // for whole loop

  int32_t x = (int32_t)(u * tex->width);
  int32_t y = (int32_t)(v * tex->height);

  if (mode == CLAMP)
  {
    x = x < 0 ? 0 : (x >= tex->width ? tex->width - 1 : x);
    y = y < 0 ? 0 : (y >= tex->height ? tex->height - 1 : y);
  }
  if (mode == WRAP)
  {
    float wu = u - floorf(u);
    float wv = v - floorf(v);

    x = (int32_t)(wu * tex->width);
    y = (int32_t)(wv * tex->height);

    x = (x >= tex->width) ? tex->width - 1 : x;
    y = (y >= tex->height) ? tex->height - 1 : y;
  }
  size_t idx = (y * tex->width + x) * tex->channels;
  return pack_color(tex->data[idx], tex->data[idx + 1], tex->data[idx + 2],
                    tex->data[idx + 3]);
}

static inline float near_distance(Vertex const *v)
{
  return v->pos.z + v->pos.w;
}

Vertex lerp_vertex(Vertex const *restrict v, Vertex const *restrict u,
                   float const t)
{
  Vertex out;
  out.pos = vec4_lerp(v->pos, u->pos, t);
  for (int i = 0; i < MAX_VARYING_ATTRS; ++i)
    out.varying[i] = v->varying[i] + t * (u->varying[i] - v->varying[i]);

  return out;
}

int triangulate_fan(Vertex const *poly, int const poly_count,
                    Vertex tris_out[][3])
{
  if (poly_count < 3) return 0;
  int tri_count = 0;
  for (int i = 1; i < poly_count - 1; ++i)
  {
    tris_out[tri_count][0] = poly[0];
    tris_out[tri_count][1] = poly[i];
    tris_out[tri_count][2] = poly[i + 1];
    tri_count++;
  }
  return tri_count;
}

int32_t clip_triangle_near(Vertex const in[3], Vertex out[4])
{
  int32_t count = 0;
  for (int i = 0; i < 3; ++i)
  {
    Vertex const *curr   = &in[i];
    Vertex const *prev   = &in[(i + 2) % 3];
    float         d_curr = near_distance(curr);
    float         d_prev = near_distance(prev);

    static float const NEAR_EPS = 1e-4f;

    bool curr_in = d_curr >= NEAR_EPS;
    bool prev_in = d_prev >= NEAR_EPS;

    if (curr_in != prev_in)
    {
      float t      = d_prev / (d_prev - d_curr);
      out[count++] = lerp_vertex(prev, curr, t);
    }
    if (curr_in) out[count++] = *curr;
  }
  return count;
}

void vertex_to_screen(Vertex *verts, uint32_t const fb_width,
                      uint32_t const fb_height)
{
  for (uint8_t i = 0; i < 3; i++)
  {
    float const inv_w = 1.0f / verts[i].pos.w;
    verts[i].inv_w    = inv_w;

    for (size_t j = 0; j < MAX_VARYING_ATTRS; ++j) verts[i].varying[j] *= inv_w;

    verts[i].pos.x *= inv_w;
    verts[i].pos.y *= inv_w;
    verts[i].pos.z *= inv_w;

    verts[i].pos.x = (verts[i].pos.x + 1.0f) * 0.5f * fb_width;
    verts[i].pos.y = (1.0f - verts[i].pos.y) * 0.5f * fb_height;
  }
}

Color shade_pixel(Texture const *tex, float const *varying,
                  Vec3 const *camera_pos, Material const *material)
{
  Color texture_color = sample_texture(tex, WRAP, varying[UV_U], varying[UV_V]);
  float tex_r         = ((texture_color >> 16) & 0xFF) / 255.0f;
  float tex_g         = ((texture_color >> 8) & 0xFF) / 255.0f;
  float tex_b         = (texture_color & 0xFF) / 255.0f;
  float tex_a         = ((texture_color >> 24) & 0xFF) / 255.0f;

  Vec3 normal = vec3_norm(
    new_vec3(varying[NORMAL_X], varying[NORMAL_Y], varying[NORMAL_Z]));
  Vec3 surface =
    new_vec3(varying[SURFACE_X], varying[SURFACE_Y], varying[SURFACE_Z]);
  Vec3  view_dir         = vec3_norm(vec3_sub(*camera_pos, surface));
  Vec3  surface_to_light = vec3_norm(vec3_sub(light0.pos, surface));
  float dot_nl           = dot3(normal, surface_to_light);
  float angle            = fmaxf(0.0f, dot_nl);
  Vec3  reflect_dir =
    vec3_norm(vec3_sub(vec3_mult_val(normal, 2.0f * dot_nl), surface_to_light));
  float spec_angle = fmaxf(0.0f, dot3(reflect_dir, view_dir));
  float spec_factor =
    (dot_nl > 0.0f) ? powf(spec_angle, material->shininess) : 0.0f;

  Vec3 ambient_light =
    vec3_mult_val(ambient_light_color, material->ambient_coeff);
  Vec3 diffuse_light = vec3_mult_val(
    vec3_mult_val(light0.color_vec, material->diffuse_coeff), angle);
  Vec3 specular_light =
    vec3_mult_val(vec3_mult(light0.color_vec, material->specular_color),
                  material->specular_strength * spec_factor);
  Vec3 total_light = vec3_add(ambient_light, diffuse_light);

  specular_light.x = fminf(1.0f, specular_light.x);
  specular_light.y = fminf(1.0f, specular_light.y);
  specular_light.z = fminf(1.0f, specular_light.z);

  total_light.x = fminf(1.0f, total_light.x);
  total_light.y = fminf(1.0f, total_light.y);
  total_light.z = fminf(1.0f, total_light.z);

  return pack_color(
    fminf(1.0f, tex_r * total_light.x + specular_light.x) * 255.0f,
    fminf(1.0f, tex_g * total_light.y + specular_light.y) * 255.0f,
    fminf(1.0f, tex_b * total_light.z + specular_light.z) * 255.0f,
    tex_a * 255.0f);
}

void draw_triangle_wireframe(Vertex const *verts, size_t const idx1,
                             size_t const idx2, size_t const idx3,
                             FrameBuffer *fb, bool triangle, bool bbox)
{
  Vertex v[3] = {
    verts[idx1],
    verts[idx2],
    verts[idx3],
  };

  static int const MAX_CLIP_VERTS = 4;

  Vertex clipped[MAX_CLIP_VERTS];
  int    clipped_count = clip_triangle_near(v, clipped);

  // fully behind near plane
  if (clipped_count < 3) return;
  Vertex tris[MAX_CLIP_VERTS - 2][3];
  int    tri_count = triangulate_fan(clipped, clipped_count, tris);

  for (int t = 0; t < tri_count; ++t)
  {
    Vertex tv[3] = {tris[t][0], tris[t][1], tris[t][2]};

    // Clip space -> NDC -> screen space
    vertex_to_screen(tv, fb->width, fb->height);

    Vec4 v1 = tv[0].pos;
    Vec4 v2 = tv[1].pos;
    Vec4 v3 = tv[2].pos;

    float area = (v2.x - v1.x) * (v3.y - v1.y) - (v2.y - v1.y) * (v3.x - v1.x);
    if (area >= 0) continue;

    float fxmin = fmin(v1.x, fmin(v2.x, v3.x));
    float fymin = fmin(v1.y, fmin(v2.y, v3.y));
    float fxmax = fmax(v1.x, fmax(v2.x, v3.x));
    float fymax = fmax(v1.y, fmax(v2.y, v3.y));

    fxmin = fmaxf(fxmin, 0.0f);
    fymin = fmaxf(fymin, 0.0f);
    fxmax = fminf(fxmax, (float)fb->width - 1.0f);
    fymax = fminf(fymax, (float)fb->height - 1.0f);

    int32_t xmin = (int32_t)fxmin;
    int32_t ymin = (int32_t)fymin;
    int32_t xmax = (int32_t)fxmax;
    int32_t ymax = (int32_t)fymax;

    if (triangle)
    {
      draw_line(fb, v1, v2, 0xFFFF0000);
      draw_line(fb, v1, v3, 0xFFFF0000);
      draw_line(fb, v2, v3, 0xFFFF0000);
    }
    if (bbox)
    {
      Vec4 tl_corner = new_vec4(xmin, ymax, 0, 0);
      Vec4 tr_corner = new_vec4(xmax, ymax, 0, 0);
      Vec4 bl_corner = new_vec4(xmin, ymin, 0, 0);
      Vec4 br_corner = new_vec4(xmax, ymin, 0, 0);

      draw_line(fb, tl_corner, tr_corner, 0xFF0000FF);
      draw_line(fb, tr_corner, br_corner, 0xFF0000FF);
      draw_line(fb, br_corner, bl_corner, 0xFF0000FF);
      draw_line(fb, bl_corner, tl_corner, 0xFF0000FF);
    }
  }
}

static inline void
interpolate_attributes(float *restrict src, float const *restrict dst0,
                       float const *restrict dst1, float const *restrict dst2,
                       float const w0, float const w1, float const w2)
{
  for (size_t i = 0; i < MAX_VARYING_ATTRS; ++i)
    src[i] = w0 * dst0[i] + w1 * dst1[i] + w2 * dst2[i];
}

void draw_triangle(Vertex const *verts, size_t const idx1, size_t const idx2,
                   size_t const idx3, Vec3 const *camera_pos,
                   Material const *material, Texture const *tex,
                   FrameBuffer *fb, bool const backface_culling)
{
  Vertex v[3] = {
    verts[idx1],
    verts[idx2],
    verts[idx3],
  };

  static int const MAX_CLIP_VERTS = 4;

  Vertex clipped[MAX_CLIP_VERTS];
  int    clipped_count = clip_triangle_near(v, clipped);

  if (clipped_count < 3) return;

  Vertex tris[MAX_CLIP_VERTS - 2][3];
  int    tri_count = triangulate_fan(clipped, clipped_count, tris);

  for (int t = 0; t < tri_count; ++t)
  {
    Vertex tv[3] = {tris[t][0], tris[t][1], tris[t][2]};

    vertex_to_screen(tv, fb->width, fb->height);

    Vec4 const v1 = tv[0].pos;
    Vec4 const v2 = tv[1].pos;
    Vec4 const v3 = tv[2].pos;

    float const area =
      (v2.x - v1.x) * (v3.y - v1.y) - (v2.y - v1.y) * (v3.x - v1.x);

    if (backface_culling && area >= 0) return;

    float fymin = fminf(v1.y, fminf(v2.y, v3.y));
    float fymax = fmaxf(v1.y, fmaxf(v2.y, v3.y));

    fymin = fmaxf(fymin, 0.0f);
    fymax = fminf(fymax, (float)fb->height - 1.0f);

    int32_t const ymin = (int32_t)fymin;
    int32_t const ymax = (int32_t)fymax;

    float const screen_xmax = (float)fb->width - 1.0f;

    float const inv_dy12 = 1.0f / (v2.y - v1.y);
    float const inv_dy23 = 1.0f / (v3.y - v2.y);
    float const inv_dy31 = 1.0f / (v1.y - v3.y);

    for (int32_t y = ymin; y <= ymax; ++y)
    {
      float const yc       = (float)y + 0.5f;
      float       row_xmin = screen_xmax;
      float       row_xmax = 0.0f;

      {  // v1 -> v2
        float const t  = (yc - v1.y) * inv_dy12;
        float const xi = v1.x + t * (v2.x - v1.x);

        float const ymin_edge = fminf(v1.y, v2.y);
        float const ymax_edge = fmaxf(v1.y, v2.y);

        if (yc >= ymin_edge && yc <= ymax_edge)
        {
          row_xmin = fminf(row_xmin, xi);
          row_xmax = fmaxf(row_xmax, xi);
        }
      }
      {  // v2 -> v3
        float const t = (yc - v2.y) * inv_dy23;

        float const xi = v2.x + t * (v3.x - v2.x);

        float const ymin_edge = fminf(v2.y, v3.y);
        float const ymax_edge = fmaxf(v2.y, v3.y);

        if (yc >= ymin_edge && yc <= ymax_edge)
        {
          row_xmin = fminf(row_xmin, xi);
          row_xmax = fmaxf(row_xmax, xi);
        }
      }
      {  // v3 -> v1
        float const t  = (yc - v3.y) * inv_dy31;
        float const xi = v3.x + t * (v1.x - v3.x);

        float const ymin_edge = fminf(v3.y, v1.y);
        float const ymax_edge = fmaxf(v3.y, v1.y);

        if (yc >= ymin_edge && yc <= ymax_edge)
        {
          row_xmin = fminf(row_xmin, xi);
          row_xmax = fmaxf(row_xmax, xi);
        }
      }
      int32_t const x0 = (int32_t)fmaxf(0.0f, row_xmin);
      int32_t const x1 = (int32_t)fminf(screen_xmax, row_xmax);

      for (int32_t x = x0; x <= x1; ++x)
      {
        Vec3 const  p = {(float)x + 0.5f, yc, 0.0f};
        float const w0 =
          (v3.x - v2.x) * (p.y - v2.y) - (v3.y - v2.y) * (p.x - v2.x);
        float const w1 =
          (v1.x - v3.x) * (p.y - v3.y) - (v1.y - v3.y) * (p.x - v3.x);
        float const w2 =
          (v2.x - v1.x) * (p.y - v1.y) - (v2.y - v1.y) * (p.x - v1.x);

        float const  bw0 = w0 / area;
        float const  bw1 = w1 / area;
        float const  bw2 = w2 / area;
        float const  z   = bw0 * v1.z + bw1 * v2.z + bw2 * v3.z;
        size_t const idx = y * fb->width + x;

        if (z >= fb->depth_buffer[fb->draw_idx][idx]) continue;

        float const inv_w =
          bw0 * tv[0].inv_w + bw1 * tv[1].inv_w + bw2 * tv[2].inv_w;

        float varying[MAX_VARYING_ATTRS];
        interpolate_attributes(varying, tv[0].varying, tv[1].varying,
                               tv[2].varying, bw0, bw1, bw2);

        for (size_t i = 0; i < MAX_VARYING_ATTRS; ++i) varying[i] /= inv_w;

        Color const phong_color =
          shade_pixel(tex, varying, camera_pos, material);

        fb->depth_buffer[fb->draw_idx][idx] = z;
        draw_pixel(x, y, phong_color, fb);
      }
    }
  }
}

void draw_model_wireframe(Model const *model, Mat4 const *view,
                          Mat4 const *projection, Vec3 *camera_pos,
                          FrameBuffer *fb, bool triangle, bool bbox)
{
  Vertex transformed[model->mesh.vertex_count];
  Mat3   normal_mat = mat3_transpose(mat3_inverse(mat4_to_mat3(model->mtw)));

  for (size_t i = 0; i < model->mesh.vertex_count; ++i)
  {
    transformed[i] = model->mesh.verts[i];

    Vec4 world_pos = transform(model->mtw, transformed[i].pos);

    Vec3 local_normal = new_vec3(transformed[i].varying[NORMAL_X],
                                 transformed[i].varying[NORMAL_Y],
                                 transformed[i].varying[NORMAL_Z]);

    Vec3 world_normal = transform_mat3(normal_mat, local_normal);

    transformed[i].varying[NORMAL_X]  = world_normal.x;
    transformed[i].varying[NORMAL_Y]  = world_normal.y;
    transformed[i].varying[NORMAL_Z]  = world_normal.z;
    transformed[i].varying[SURFACE_X] = world_pos.x;
    transformed[i].varying[SURFACE_Y] = world_pos.y;
    transformed[i].varying[SURFACE_Z] = world_pos.z;

    transformed[i].pos = transform(*projection, transform(*view, world_pos));
  }
  for (size_t offset = 0; offset < model->mesh.index_count; offset += 3)
  {
    size_t idx1 = model->mesh.indices[offset];
    size_t idx2 = model->mesh.indices[offset + 1];
    size_t idx3 = model->mesh.indices[offset + 2];

    draw_triangle_wireframe(transformed, idx1, idx2, idx3, fb, triangle, bbox);
  }
}

// TODO: move this somewhere sensible and rename it. Also probably use a dynamic
// array Arbitrary number, := nr. of vertices is Martin
static Vertex transformed[1000 * 15];

void draw_model(Model const *model, Mat4 const *view, Mat4 const *projection,
                Vec3 const *camera_pos, FrameBuffer *fb,
                bool const backface_culling)
{
  // NOTE: maybe move transformed buffer to the heap and reuse it
  // Vertex transformed[model->mesh.vertex_count];

  // Vertex transformed[model->mesh.vertex_count];

  Mat3 const normal_mat =
    mat3_transpose(mat3_inverse(mat4_to_mat3(model->mtw)));

  for (size_t i = 0; i < model->mesh.vertex_count; ++i)
  {
    transformed[i] = model->mesh.verts[i];

    Vec4 const world_pos = transform(model->mtw, transformed[i].pos);

    Vec3 const local_normal = new_vec3(transformed[i].varying[NORMAL_X],
                                       transformed[i].varying[NORMAL_Y],
                                       transformed[i].varying[NORMAL_Z]);

    Vec3 const world_normal = transform_mat3(normal_mat, local_normal);

    transformed[i].varying[NORMAL_X]  = world_normal.x;
    transformed[i].varying[NORMAL_Y]  = world_normal.y;
    transformed[i].varying[NORMAL_Z]  = world_normal.z;
    transformed[i].varying[SURFACE_X] = world_pos.x;
    transformed[i].varying[SURFACE_Y] = world_pos.y;
    transformed[i].varying[SURFACE_Z] = world_pos.z;

    transformed[i].pos = transform(*projection, transform(*view, world_pos));
  }
  for (size_t offset = 0; offset < model->mesh.index_count; offset += 3)
  {
    size_t const idx1 = model->mesh.indices[offset];
    size_t const idx2 = model->mesh.indices[offset + 1];
    size_t const idx3 = model->mesh.indices[offset + 2];
    draw_triangle(transformed, idx1, idx2, idx3, camera_pos, &model->material,
                  model->tex, fb, backface_culling);
  }
}

void draw_scene(Model const *scene, size_t const nr_models, Mat4 const *view,
                Mat4 const *projection, Camera const *camera, FrameBuffer *fb,
                bool const backface_culling)
{
  for (size_t i = 0; i < nr_models; ++i)
  {
    draw_model(&scene[i], view, projection, &camera->camera_pos, fb,
               backface_culling);
  }
}

Mat4 look_at(Vec3 const pos, Vec3 const target, Vec3 const up)
{
  Vec3 const cam_dir   = vec3_norm(vec3_sub(pos, target));
  Vec3 const cam_right = vec3_norm(cross(up, cam_dir));
  Vec3 const cam_up    = cross(cam_dir, cam_right);
  Mat4 const a         = (Mat4){
    cam_right.x, cam_up.x, cam_dir.x,   0.0f,     cam_right.y, cam_up.y,
    cam_dir.y,   0.0f,     cam_right.z, cam_up.z, cam_dir.z,   0.0f,
    0.0f,        0.0f,     0.0f,        1.0f,
  };
  Mat4 const b = (Mat4){1.0f, 0.0f, 0.0f, 0.0f, 0.0f,   1.0f,   0.0f,   0.0f,
                        0.0f, 0.0f, 1.0f, 0.0f, -pos.x, -pos.y, -pos.z, 1.0f};
  return mat4_mult(a, b);
}

void clear_background(FrameBuffer *fb, Color const color)
{
  uint32_t const size = fb->width * fb->height;
  for (uint32_t i = 0; i < size; ++i) fb->color_buffer[fb->draw_idx][i] = color;
  for (uint32_t i = 0; i < size; ++i)
    fb->depth_buffer[fb->draw_idx][i] = INFINITY;
}

// ============================================================================
// INPUT
// ============================================================================

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

void update_camera(Camera *camera, InputState const *input, double const dt)
{
  float const speed             = 5.0f * (float)dt;
  float const mouse_sensitivity = 0.00025f;
  Vec3 const  world_up          = {0.0f, 1.0f, 0.0f};
  float const pitch_limit       = 89.0f * (M_PI / 180.0f);

  Vec3 const f     = camera->camera_front;
  float      yaw   = atan2f(f.z, f.x);
  float      pitch = asinf(f.y);

  yaw += input->mouse_dx * mouse_sensitivity;
  pitch -= input->mouse_dy * mouse_sensitivity;

  if (pitch > pitch_limit) pitch = pitch_limit;
  if (pitch < -pitch_limit) pitch = -pitch_limit;

  camera->camera_front = vec3_norm((Vec3){
    cosf(pitch) * cosf(yaw),
    sinf(pitch),
    cosf(pitch) * sinf(yaw),
  });

  Vec3 const right  = vec3_norm(cross(camera->camera_front, world_up));
  camera->camera_up = vec3_norm(cross(right, camera->camera_front));

  Vec3 move = {0};
  if (input->w) move = vec3_add(move, camera->camera_front);
  if (input->s) move = vec3_sub(move, camera->camera_front);
  if (input->d) move = vec3_add(move, right);
  if (input->a) move = vec3_sub(move, right);

  if (vec3_length(move) > 0.0001f)
    camera->camera_pos =
      vec3_add(camera->camera_pos, vec3_mult_val(vec3_norm(move), speed));

  if (input->shift) camera->camera_pos.y += speed;
  if (input->ctrl) camera->camera_pos.y -= speed;
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char *argv[])
{
  AppConfig *cfg = calloc(1, sizeof(*cfg));

  cfg->event_mask = ButtonPressMask | ButtonReleaseMask | KeyPressMask |
                    KeyReleaseMask | PointerMotionMask;

  parse_args(cfg, argc, argv);
  create_window(cfg, "CPU RENDERING PROTOTYPE V2");
  init_keymap(cfg->display);

  FrameBuffer   *fb = init_framebuffer(cfg->res_w, cfg->res_h);
  DisplayBuffer *db = init_display_buffer(cfg->win_w, cfg->win_h);

  XImage *render_img = XCreateImage(
    cfg->display, cfg->visual, cfg->depth, ZPixmap, 0,
    (char *)fb->color_buffer[fb->draw_idx], cfg->res_w, cfg->res_h, 32, 0);
  XImage *disp_img =
    XCreateImage(cfg->display, cfg->visual, cfg->depth, ZPixmap, 0,
                 (char *)db->pixels, cfg->win_w, cfg->win_h, 32, 0);
  printf("X11 pixel format\n\tR: %08lx \n\tG: %08lx \n\tB: %08lx\n",
         render_img->red_mask, render_img->green_mask, render_img->blue_mask);

  // load_texture(&tex0, "textures/martin.png");
  load_texture(&tex1, "textures/placeholder16x16.png");
  load_texture(&texbottle, "textures/bottle.png");

  light0 = (Light){.pos       = new_vec3(0.0f, 3.0f, 5.0f),
                   .color_vec = new_vec3(1.0f, 0.95f, 0.85f)};

  ambient_light_color = new_vec3(0.5f, 0.5f, 0.5f);

  Model bottle_model = load_model("models/bottle.obj");
  bottle_model.mtw   = mat4_mult(translate(0, 0.25, 0), scale(0.03));
  bottle_model.tex   = &texbottle;

  Model bar_box = load_model("models/walk_area.obj");
  bar_box.mtw   = identity();
  bar_box.tex   = &tex1;

  Material ground_material = {
    .ambient_coeff     = 0.12f,
    .diffuse_coeff     = 0.45f,
    .specular_strength = 0.85f,
    .shininess         = 120.0f,
    .specular_color    = {1.0f, 1.0f, 1.0f},
  };
  Material bottle_material = {
    .ambient_coeff     = 0.5,
    .diffuse_coeff     = 0.45f,
    .specular_strength = 0.85f,
    .shininess         = 50.0f,
    .specular_color    = {1.0f, 1.0f, 1.0f},
  };
  bottle_model.material = bottle_material;

  Model ground    = {0};
  ground.mesh     = generate_ground_mesh(60.0f, 1.5f, 12.0f);
  ground.mtw      = identity();
  ground.tex      = &tex1;
  ground.material = ground_material;

#define NR_MODELS 3
  Model scene[NR_MODELS] = {ground, bottle_model, bar_box};

  Camera camera = {
    .camera_up    = (Vec3){0.0f, 1.0f, 0.0f },
    .camera_front = (Vec3){0.0f, 0.0f, -1.0f},
    .camera_pos   = (Vec3){0.0f, 2.5f, 10.0f},
  };

  // projection
  float const fov = 65.0, near = 0.05, far = 100.0;
  float const aspect    = ((float)cfg->win_w / (float)cfg->win_h);
  Mat4 const projection = perspective(fov * (M_PI / 180.0f), aspect, near, far);

  clock_gettime(CLOCK_MONOTONIC, &last_frame);

  InputState  input_state = {0};
  static bool quit        = false;
  while (!quit)
  {
    // WARN: only call once per frame
    double const dt = get_frame_delta();
    if (cfg->fps)
      printf("frame time: %.4f seconds => FPS: %d\n", dt, (int)(1.0 / dt));

    poll_input(cfg, &quit, &input_state);

    update_camera(&camera, &input_state, dt);

    clear_background(fb, 0xFFFFFFFF);

    // model-to-world
    static float angle = 0.0f;
    angle += dt;

    // world-to-view
    Mat4 view = look_at(camera.camera_pos,
                        vec3_add(camera.camera_pos, camera.camera_front),
                        camera.camera_up);
    if (cfg->wireframe)
    {
      // Draw wireframe debugging
      for (unsigned i = 0; i < NR_MODELS; ++i)
      {
        bool triangle = true, bbox = false;
        draw_model_wireframe(&scene[i], &view, &projection, &camera.camera_pos,
                             fb, triangle, bbox);
      }
    }
    else
    {
      // Draw all the models
      draw_scene(scene, NR_MODELS, &view, &projection, &camera, fb, true);
    }
    update_window(cfg, render_img, disp_img, db, fb);
  };
  close_window(cfg);
  return 0;
}
