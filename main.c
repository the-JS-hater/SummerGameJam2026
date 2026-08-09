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
#include "render.h"
#include "x11_util.h"

typedef struct {
  Vec3 camera_up, camera_front, camera_pos, camera_vel;
} Camera;

INITIALIZE_VECTOR_TEMPLATE(Model);

typedef struct {
  Vec3    position;
  Vec3    velocity;
  float   angle;
  float   lifespan;
  int32_t accumulated_score;
} Beer;

INITIALIZE_VECTOR_TEMPLATE(Beer);

typedef struct {
  ModelVec models;
  BeerVec  beers;
  Model    beer_model;
  int32_t  score;
} Scene;

// ============================================================================
// GLOBALS
// ============================================================================

static struct timespec last_frame;

Texture tex0;
Texture tex1;
Texture texfloor;
Texture texceiling;
Texture texwall;
Texture texbottle;
Texture textable;

// ============================================================================
// PLUMBING & MISC
// ============================================================================

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
// GAME LOGIC
// ============================================================================

void update_models(Scene *scene, float dt)
{
  for (size_t i = 0; i < scene->models.size; ++i)
  {
    scene->models.data[i].cooldown -= dt;
  }
}

void update_beers(Scene *scene, float dt)
{
  for (size_t l = 0; l < scene->beers.size; ++l)
  {
    Beer *beer = &scene->beers.data[l];

    beer->velocity.y -= 9.82 * dt;
    beer->angle += 10.0 * dt;

    Vec3 delta = vec3_mult_val(beer->velocity, dt);

    float  closest = 2.0;
    Vec3   closest_normal;
    Model *closest_model = NULL;

    for (size_t i = 0; i < scene->models.size; ++i)
    {
      Model *model = &scene->models.data[i];
      if (model->collision_type == NONE) continue;
      Mesh const *mesh = &model->mesh;
      for (size_t j = 0; j < mesh->index_count; j += 3)
      {
        Vec4 verts[3] = {mesh->verts[mesh->indices[j]].pos,
                         mesh->verts[mesh->indices[j + 1]].pos,
                         mesh->verts[mesh->indices[j + 2]].pos};
        for (size_t k = 0; k < 3; k++)
        {
          verts[k] = transform(model->mtw, verts[k]);
        }
        Vec3 positions[3] = {vec3(verts[0]), vec3(verts[1]), vec3(verts[2])};

        float dist;
        Vec3  normal;
        if (ray_triangle_intersection(beer->position, delta, positions, &dist,
                                      &normal))
        {
          if (dist < closest)
          {
            closest        = dist;
            closest_normal = normal;

            closest_model = model;
          }
        }
      }
    }

    if (closest <= 1.0)
    {

      // Score doesn't count if it isn't the top side
      CollisionType collision_type = closest_model->collision_type;
      if (collision_type == SCORE &&
          (closest_normal.y < 0.9 || closest_model->cooldown > 0))
      {
        collision_type = BOUNCE;
      }

      switch (collision_type)
      {
        case BOUNCE:
        {
          delta = vec3_mult_val(delta, closest);

          float bounciness = 0.8;

          beer->velocity =
            vec3_sub(beer->velocity,
                     vec3_mult_val(closest_normal,
                                   (1.0 + bounciness) *
                                     dot3(beer->velocity, closest_normal)));
          beer->accumulated_score += 1;
          break;
        }
        case STOP:
        {
          beer->velocity = new_vec3(0, 0, 0);
          delta          = new_vec3(0, 0, 0);
          break;
        }
        case SCORE:
        {
          beer->lifespan = 0.0;
          scene->score += 1 + beer->accumulated_score;
          closest_model->cooldown = 5.0;
          printf("score: %d\n", scene->score);
          break;
        }
        case NONE: break;
      }
    }

    beer->position = vec3_add(beer->position, delta);
  }
}

void spawn_beer(BeerVec *beers, Vec3 const pos, Vec3 const extra_vel,
                Vec3 const dir, float const speed)
{
  Beer new_beer              = {0};
  new_beer.position          = pos;
  new_beer.velocity          = vec3_add(extra_vel, vec3_mult_val(dir, speed));
  new_beer.lifespan          = 3.0f;
  new_beer.accumulated_score = 0;
  Beer_append(beers, new_beer);
}

bool remove_beer(Beer beer) { return beer.lifespan > 0.0f; }

// ============================================================================
// DRAWING
// ============================================================================

void draw_scene(Scene const *scene, Mat4 const *view, Mat4 const *projection,
                Camera const *camera, FrameBuffer *fb,
                bool const backface_culling)
{
  for (size_t i = 0; i < scene->models.size; ++i)
  {
    draw_model(&scene->models.data[i], view, projection, &camera->camera_pos,
               fb, backface_culling);
  }


  for (size_t i = 0; i < scene->beers.size; i++)
  {
    Beer const *beer = &scene->beers.data[i];
    Vec3        pos  = beer->position;

    Model transformed = scene->beer_model;
    transformed.mtw   = mat4_mult(rotate_x(beer->angle), transformed.mtw);
    transformed.mtw =
      mat4_mult(translate(pos.x, pos.y, pos.z), transformed.mtw);
    draw_model(&transformed, view, projection, &camera->camera_pos, fb, true);
  }
}


// ============================================================================
// INPUT
// ============================================================================

void update_camera(Camera *camera, InputState const *input, double const dt)
{
  float const speed             = 6.2f;
  float const mouse_sensitivity = 0.00025f;
  Vec3 const  world_up          = new_vec3(0.0f, 1.0f, 0.0f);
  float const pitch_limit       = 89.0f * (M_PI / 180.0f);

  Vec3 const f     = camera->camera_front;
  float      yaw   = atan2f(f.z, f.x);
  float      pitch = asinf(f.y);

  yaw += input->mouse_dx * mouse_sensitivity;
  pitch -= input->mouse_dy * mouse_sensitivity;

  if (pitch > pitch_limit) pitch = pitch_limit;
  if (pitch < -pitch_limit) pitch = -pitch_limit;

  camera->camera_front =
    new_vec3(cosf(pitch) * cosf(yaw), sinf(pitch), cosf(pitch) * sinf(yaw));

  Vec3 const right  = vec3_norm(cross(camera->camera_front, world_up));
  camera->camera_up = vec3_norm(cross(right, camera->camera_front));

  Vec3 ground_forward =
    vec3_norm(new_vec3(camera->camera_front.x, 0, camera->camera_front.z));
  Vec3 ground_right =
    new_vec3(-ground_forward.z, ground_forward.y, ground_forward.x);

  Vec3 move = {0};
  if (input->w) move = vec3_add(move, ground_forward);
  if (input->s) move = vec3_sub(move, ground_forward);
  if (input->d) move = vec3_add(move, ground_right);
  if (input->a) move = vec3_sub(move, ground_right);

  Vec3 target_vel = new_vec3(0, 0, 0);
  if (vec3_length(move) > 0.0001f)
  {
    target_vel = vec3_mult_val(vec3_norm(move), speed);
  }

  Vec3 accel = vec3_mult_val(vec3_sub(target_vel, camera->camera_vel), 10.0);
  camera->camera_vel = vec3_add(camera->camera_vel, vec3_mult_val(accel, dt));

  camera->camera_pos =
    vec3_add(camera->camera_pos, vec3_mult_val(camera->camera_vel, dt));

  camera->camera_pos.x =
    fmin(fmax(camera->camera_pos.x, -(2.0 - 0.8)), 2.0 - 0.8);
  camera->camera_pos.z =
    fmin(fmax(camera->camera_pos.z, -(1.5 - 0.8)), 1.5 - 0.8);

  // if (input->shift) camera->camera_pos.y += speed;
  // if (input->ctrl) camera->camera_pos.y -= speed;
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
  create_window(cfg, "Corona Chucker");
  init_keymap(cfg->display);

  FrameBuffer   *fb = init_framebuffer(cfg->res_w, cfg->res_h);
  DisplayBuffer *db = init_display_buffer(cfg->win_w, cfg->win_h);

  XImage *disp_img =
    XCreateImage(cfg->display, cfg->visual, cfg->depth, ZPixmap, 0,
                 (char *)db->pixels, cfg->win_w, cfg->win_h, 32, 0);

  XFontStruct *font = XLoadQueryFont(
    cfg->display, "-misc-fixed-medium-r-normal--75-*-*-*-*-*-iso8859-15");
  if (font)
  {
    XSetFont(cfg->display, DefaultGC(cfg->display, cfg->screen), font->fid);
  }
  XSetForeground(cfg->display, DefaultGC(cfg->display, cfg->screen),
                 0xFFFFFFFF);
  printf("X11 pixel format\n\tR: %08lx \n\tG: %08lx \n\tB: %08lx\n",
         disp_img->red_mask, disp_img->green_mask, disp_img->blue_mask);

  // load_texture(&tex0, "textures/martin.png");
  load_texture(&tex1, "textures/placeholder16x16.png");
  load_texture(&texfloor, "textures/wood_floor.png");
  load_texture(&texceiling, "textures/ceiling.png");
  load_texture(&texwall, "textures/wall.png");
  load_texture(&textable, "textures/table.png");
  load_texture(&texbottle, "textures/bottle.png");

  light0 = (Light){.pos       = new_vec3(0.0f, 2.0f, 1.0f),
                   .color_vec = new_vec3(1.0f, 0.95f, 0.85f)};

  ambient_light_color = new_vec3(0.5f, 0.5f, 0.5f);

  Material ground_material = {
    .ambient_coeff     = 0.12f,
    .diffuse_coeff     = 0.65f,
    .specular_strength = 0.15f,
    .shininess         = 120.0f,
    .specular_color    = new_vec3(1.0f, 1.0f, 1.0f),
  };
  Material bottle_material = {
    .ambient_coeff     = 0.5,
    .diffuse_coeff     = 0.45f,
    .specular_strength = 0.85f,
    .shininess         = 50.0f,
    .specular_color    = new_vec3(1.0f, 1.0f, 1.0f),
  };

  Model ceiling_model    = load_model("models/ceiling.obj");
  ceiling_model.mtw      = identity();
  ceiling_model.tex      = &texceiling;
  ceiling_model.material = ground_material;

  Model bar_ceiling_model    = load_model("models/bar_ceiling.obj");
  bar_ceiling_model.mtw      = identity();
  bar_ceiling_model.tex      = &texwall;
  bar_ceiling_model.material = ground_material;

  Model floor_model    = load_model("models/floor.obj");
  floor_model.mtw      = identity();
  floor_model.tex      = &texfloor;
  floor_model.material = ground_material;

  Model wall_model    = load_model("models/wall.obj");
  wall_model.mtw      = identity();
  wall_model.tex      = &texwall;
  wall_model.material = ground_material;

  Model counter_model    = load_model("models/counter.obj");
  counter_model.mtw      = identity();
  counter_model.tex      = &textable;
  counter_model.material = ground_material;

  Model table_model    = load_model("models/table.obj");
  table_model.mtw      = identity();
  table_model.tex      = &textable;
  table_model.material = ground_material;

  Model beer_model    = load_model("models/bottle.obj");
  beer_model.mtw      = mat4_mult(scale(0.05), translate(0, -3.0, 0));
  beer_model.tex      = &texbottle;
  beer_model.material = bottle_material;

  ceiling_model.collision_type = BOUNCE;
  bar_ceiling_model.collision_type = BOUNCE;
  wall_model.collision_type    = BOUNCE;
  floor_model.collision_type   = BOUNCE;
  counter_model.collision_type = BOUNCE;
  table_model.collision_type   = SCORE;

  Scene scene = {
    .beer_model = beer_model,
  };
  // Model_append(&scene.models, ground);
  // Model_append(&scene.models, bottle_model);
  Model_append(&scene.models, ceiling_model);
  Model_append(&scene.models, bar_ceiling_model);
  Model_append(&scene.models, floor_model);
  Model_append(&scene.models, wall_model);
  Model_append(&scene.models, counter_model);

  table_model.mtw =
    mat4_mult(translate(4.84439, 0, -7.94403), rotate_y(1.57079));
  Model_append(&scene.models, table_model);
  table_model.mtw =
    mat4_mult(translate(4.84439, 0, -5.38836), rotate_y(1.57079));
  Model_append(&scene.models, table_model);
  table_model.mtw =
    mat4_mult(translate(-2.90822, 0, -3.98068), rotate_y(1.57079));
  Model_append(&scene.models, table_model);
  table_model.mtw = mat4_mult(translate(-5.41128, 0, -6.46436), rotate_y(0));
  Model_append(&scene.models, table_model);
  table_model.mtw = mat4_mult(translate(0.790692, 0, -8.3214), rotate_y(0));
  Model_append(&scene.models, table_model);
  table_model.mtw = mat4_mult(translate(0.790692, 0, -6.22508), rotate_y(0));
  Model_append(&scene.models, table_model);
  table_model.mtw = mat4_mult(translate(-1.94653, 0, -8.3214), rotate_y(0));
  Model_append(&scene.models, table_model);
  table_model.mtw = mat4_mult(translate(4.5322, 0, 0.497361), rotate_y(0));
  Model_append(&scene.models, table_model);

  Camera camera = {
    .camera_up    = new_vec3(0.0f, 1.0f, 0.0f),
    .camera_front = new_vec3(0.0f, 0.0f, -1.0f),
    .camera_pos   = new_vec3(0.0f, 1.6f, 0.0f),
    .camera_vel   = new_vec3(0.0f, 0.0f, 0.0f),
  };

  // projection
  float const fov = 65.0, near = 0.05, far = 100.0;
  float const aspect    = ((float)cfg->win_w / (float)cfg->win_h);
  Mat4 const projection = perspective(fov * (M_PI / 180.0f), aspect, near, far);

  clock_gettime(CLOCK_MONOTONIC, &last_frame);

  InputState   input_state         = {0};
  static float beer_spawn_cooldown = 0.0f;
  static bool  quit                = false;
  while (!quit)
  {
    handle_resize(cfg, db, &disp_img);

    // WARN: only call once per frame
    double const dt = get_frame_delta();
    if (cfg->fps)
      printf("frame time: %.4f seconds => FPS: %d\n", dt, (int)(1.0 / dt));

    for (size_t i = 0; i < scene.beers.size; ++i)
    {
      scene.beers.data[i].lifespan -= dt;
    }
    Beer_filter(&scene.beers, remove_beer);

    poll_input(cfg, &quit, &input_state);

    beer_spawn_cooldown =
      beer_spawn_cooldown > dt ? beer_spawn_cooldown - dt : 0.0f;
    if (input_state.space && beer_spawn_cooldown < dt)
    {
      beer_spawn_cooldown = 0.5f;
      float const speed   = 16.0f;
      spawn_beer(&scene.beers, camera.camera_pos, camera.camera_vel,
                 camera.camera_front, speed);
    }

    update_camera(&camera, &input_state, dt);

    update_models(&scene, dt);
    update_beers(&scene, dt);

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
      for (size_t i = 0; i < scene.models.size; ++i)
      {
        bool triangle = true, bbox = false;
        draw_model_wireframe(&scene.models.data[i], &view, &projection,
                             &camera.camera_pos, fb, triangle, bbox);
      }
    }
    else
    {
      // Draw all the models
      draw_scene(&scene, &view, &projection, &camera, fb, true);
    }

    draw_crosshair(fb, 1, 3, 0xFF00FF00);

    update_window(cfg, disp_img, db, fb, scene.score);
  };
  close_window(cfg);
  return 0;
}
