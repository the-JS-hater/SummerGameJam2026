#pragma once

#include <stdint.h>
#include <stddef.h>

#include "linalg.h"
#include "obj_loader.h"

typedef struct {
  uint32_t *color_buffer[2];
  float    *depth_buffer[2];
  uint32_t  width, height;
  uint8_t   draw_idx;
} FrameBuffer;

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

extern Light light0;
extern Vec3  ambient_light_color;

FrameBuffer *init_framebuffer(uint32_t width, uint32_t height);
Mesh mesh_from_obj(ObjObject const *obj);
Model load_model(char const *filename);
void load_texture(Texture *tex, char const *filename);

void draw_pixel(uint32_t const x, uint32_t const y, Color const color,
                FrameBuffer *fb);
void draw_line(FrameBuffer *fb, Vec4 const s, Vec4 const e, Color const color);
uint32_t sample_texture(Texture const *tex, SampleMode const mode,
                        float const u, float const v);
Vertex lerp_vertex(Vertex const *restrict v, Vertex const *restrict u,
                   float const t);
int triangulate_fan(Vertex const *poly, int const poly_count,
                    Vertex tris_out[][3]);
int32_t clip_triangle_near(Vertex const in[3], Vertex out[4]);
void vertex_to_screen(Vertex *verts, uint32_t const fb_width,
                      uint32_t const fb_height);
Color shade_pixel(Texture const *tex, float const *varying,
                  Vec3 const *camera_pos, Material const *material);

void draw_triangle_wireframe(Vertex const *verts, size_t const idx1,
                             size_t const idx2, size_t const idx3,
                             FrameBuffer *fb, bool triangle, bool bbox);
void draw_triangle(Vertex const *verts, size_t const idx1, size_t const idx2,
                   size_t const idx3, Vec3 const *camera_pos,
                   Material const *material, Texture const *tex,
                   FrameBuffer *fb, bool const backface_culling);

void draw_model_wireframe(Model const *model, Mat4 const *view,
                          Mat4 const *projection, Vec3 *camera_pos,
                          FrameBuffer *fb, bool triangle, bool bbox);
void draw_model(Model const *model, Mat4 const *view, Mat4 const *projection,
                Vec3 const *camera_pos, FrameBuffer *fb,
                bool const backface_culling);

void clear_background(FrameBuffer *fb, Color const color);
