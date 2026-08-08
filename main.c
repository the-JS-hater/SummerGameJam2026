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
  Vec3 camera_up, camera_front, camera_pos;
} Camera;

INITIALIZE_VECTOR_TEMPLATE(Model);

typedef struct {
  Vec3 position;
  Vec3 velocity;
} Beer;

INITIALIZE_VECTOR_TEMPLATE(Beer);

typedef struct {
  ModelVec models;
  BeerVec  beers;
  Model    beerModel;
} Scene;

// ============================================================================
// GLOBALS
// ============================================================================

static struct timespec last_frame;
Texture                tex0;
Texture                tex1;
Texture                texbottle;

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

void update_beers(Scene *scene, float dt)
{
  for (size_t l = 0; l < scene->beers.size; ++l)
  {
    Beer *beer  = &scene->beers.data[l];
    Vec3  delta = vec3_mult_val(beer->velocity, dt);

    float closest = 2.0;
    for (size_t i = 0; i < scene->models.size; ++i)
    {
      Model const *model = &scene->models.data[i];
      Mesh const  *mesh  = &model->mesh;
      for (size_t j = 0; j < mesh->index_count; j += 3)
      {
        Vec4 verts[3] = {mesh->verts[mesh->indices[j]].pos,
                         mesh->verts[mesh->indices[j + 1]].pos,
                         mesh->verts[mesh->indices[j + 2]].pos};
        for (size_t k = 0; k < 3; k++)
        {
          verts[k] = transform(model->mtw, verts[k]);
        }
        Vec3 positions[3] = {
          {verts[0].x, verts[0].y, verts[0].z},
          {verts[1].x, verts[1].y, verts[1].z},
          {verts[2].x, verts[2].y, verts[2].z}
        };

        float dist;
        if (ray_triangle_intersection(beer->position, delta, positions, &dist))
        {
          closest = fmin(dist, closest);
        }
      }
    }

    beer->position = vec3_add(beer->position, delta);

    // if (closest <= 1.0) {

    // }

    // grenade_pos =
    //   vec3_add(camera.camera_pos, vec3_mult_val(camera.camera_front, t));
  }
}

void spawn_beer(BeerVec *beers, Vec3 const pos, Vec3 const dir,
                float const speed)
{
  Beer new_beer     = {0};
  new_beer.position = pos;
  new_beer.velocity = vec3_mult_val(dir, speed);
  Beer_append(beers, new_beer);
}

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

    Model transformed = scene->beerModel;
    transformed.mtw =
      mat4_mult(translate(pos.x, pos.y, pos.z), scene->beerModel.mtw);
    draw_model(&transformed, view, projection, &camera->camera_pos, fb, true);
  }
}


// ============================================================================
// INPUT
// ============================================================================

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

  Model grenade = load_model("models/bottle.obj");
  grenade.mtw   = scale(0.03);
  grenade.tex   = &texbottle;

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
  grenade.material      = bottle_material;

  Model ground    = {0};
  ground.mesh     = generate_ground_mesh(60.0f, 1.5f, 12.0f);
  ground.mtw      = identity();
  ground.tex      = &tex1;
  ground.material = ground_material;

  Scene scene = {
    .beerModel = grenade,
  };
  Model_append(&scene.models, ground);
  Model_append(&scene.models, bottle_model);
  Model_append(&scene.models, bar_box);

  Beer_append(&scene.beers, (Beer){
                              .position = new_vec3(0.0, 4.0, 0.0),
                              .velocity = new_vec3(0.5, 0.0, 0.0),
                            });

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

  InputState   input_state         = {0};
  static float beer_spawn_cooldown = 0.0f;
  static bool  quit                = false;
  while (!quit)
  {
    // WARN: only call once per frame
    double const dt = get_frame_delta();
    if (cfg->fps)
      printf("frame time: %.4f seconds => FPS: %d\n", dt, (int)(1.0 / dt));

    poll_input(cfg, &quit, &input_state);
    beer_spawn_cooldown =
      beer_spawn_cooldown > dt ? beer_spawn_cooldown - dt : 0.0f;
    if (input_state.space && beer_spawn_cooldown < dt)
    {
      beer_spawn_cooldown = 2.0f;
      float const speed   = 0.5f;
      spawn_beer(&scene.beers, camera.camera_pos, camera.camera_front, speed);
    }

    update_camera(&camera, &input_state, dt);

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
    update_window(cfg, render_img, disp_img, db, fb);
  };
  close_window(cfg);
  return 0;
}
