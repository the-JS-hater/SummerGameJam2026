#include "render.h"

#include "include/stb_image.h"  //MAYBE: handroll png loader

Light light0;
Vec3  ambient_light_color;

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

  int32_t dx  = abs(x1 - x0);
  int32_t sx  = x0 < x1 ? 1 : -1;
  int32_t dy  = -abs(y1 - y0);
  int32_t sy  = y0 < y1 ? 1 : -1;
  int32_t err = dx + dy;

  while (true)
  {
    if (y0 < fb->height && x0 < fb->width && y0 > 0 && x0 > 0)
    {
      draw_pixel(x0, y0, color, fb);
    }

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
        Vec3 const p = {
          {(float)x + 0.5f, yc, 0.0f}
        };
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

// TODO: move this somewhere sensible and rename it. Also probably use a dynamic
// array Arbitrary number, := nr. of vertices is Martin
static Vertex transformed[1000 * 15];

void draw_model_wireframe(Model const *model, Mat4 const *view,
                          Mat4 const *projection, Vec3 *camera_pos,
                          FrameBuffer *fb, bool triangle, bool bbox)
{
  // Vertex transformed[model->mesh.vertex_count];
  Mat3 normal_mat = mat3_transpose(mat3_inverse(mat4_to_mat3(model->mtw)));

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

void clear_background(FrameBuffer *fb, Color const color)
{
  uint32_t const size = fb->width * fb->height;
  for (uint32_t i = 0; i < size; ++i) fb->color_buffer[fb->draw_idx][i] = color;
  for (uint32_t i = 0; i < size; ++i)
    fb->depth_buffer[fb->draw_idx][i] = INFINITY;
}

void draw_crosshair(FrameBuffer *fb, int32_t size, int32_t length, Color color)
{
  uint32_t mid_x       = fb->width / 2;
  uint32_t mid_y       = fb->height / 2;
  int32_t  low_offset  = -size / 2;
  int32_t  high_offset = size / 2;
  for (int32_t offset = low_offset; offset <= high_offset; ++offset)
  {
    draw_line(fb, new_vec4(mid_x + offset, mid_y + length, 0, 0),
              new_vec4(mid_x + offset, mid_y - length, 0, 0), color);

    draw_line(fb, new_vec4(mid_x + length, mid_y + offset, 0, 0),
              new_vec4(mid_x - length, mid_y + offset, 0, 0), color);
  }
}
