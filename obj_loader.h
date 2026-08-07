#pragma once

#include "vector.h"
#include <stdio.h>

typedef struct {
  float x, y, z, w;
} obj_Vertex;

typedef struct {
  float u, v, w;
} obj_TexCoord;

typedef struct {
  float x, y, z;
} obj_Normal;

typedef struct {
  int v_i, vt_i, vn_i;
} obj_FaceElement;

typedef struct {
  obj_FaceElement triangles[3];
} obj_Face;

typedef struct {
  obj_Vertex   *verts;
  size_t        vertex_count;
  obj_TexCoord *uvs;
  size_t        uv_count;
  obj_Normal   *normals;
  size_t        normal_count;
  obj_Face     *faces;
  size_t        face_count;
} ObjObject;

bool load_obj_file(char const *filename, ObjObject *obj);
void free_obj_object(ObjObject *obj);
