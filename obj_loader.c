#include "obj_loader.h"

INITIALIZE_VECTOR_TEMPLATE(obj_Vertex);
INITIALIZE_VECTOR_TEMPLATE(obj_TexCoord);
INITIALIZE_VECTOR_TEMPLATE(obj_Normal);
INITIALIZE_VECTOR_TEMPLATE(obj_Face);

int parse_v(char const *line, obj_VertexVec *vertices)
{
  obj_Vertex v = {0};

  int n = sscanf(line, "v %f %f %f %f", &v.x, &v.y, &v.z, &v.w);

  if (n < 3) return -1;
  if (n == 3) v.w = 1.0f;

  obj_Vertex_append(vertices, v);
  return 0;
}

int parse_vt(char const *line, obj_TexCoordVec *uvs)
{
  obj_TexCoord uv = {0};

  int n = sscanf(line, "vt %f %f %f", &uv.u, &uv.v, &uv.w);
  if (n < 1) return -1;
  if (n < 3) uv.w = 0.0f;
  if (n < 2) uv.v = 0.0f;

  obj_TexCoord_append(uvs, uv);
  return 0;
}

int parse_vn(char const *line, obj_NormalVec *normals)
{
  (void)line;
  (void)normals;
  obj_Normal norm = {0};

  int n = sscanf(line, "vn %f %f %f", &norm.x, &norm.y, &norm.z);
  if (n < 3) return -1;

  obj_Normal_append(normals, norm);
  return 0;
}

int parse_f(char const *line, obj_FaceVec *faces)
{
  obj_FaceElement f1 = {0};
  obj_FaceElement f2 = {0};
  obj_FaceElement f3 = {0};
  obj_Face        f  = {0};
  // NOTE: assume all faces are triangles, implement triangulation in the future
  int n = sscanf(line,
                 "f %d/%d/%d %d/%d/%d %d/%d/%d",
                 &f1.v_i,
                 &f1.vt_i,
                 &f1.vn_i,
                 &f2.v_i,
                 &f2.vt_i,
                 &f2.vn_i,
                 &f3.v_i,
                 &f3.vt_i,
                 &f3.vn_i);
  if (n < 9) return -1;
  f.triangles[0] = f1;
  f.triangles[1] = f2;
  f.triangles[2] = f3;
  obj_Face_append(faces, f);
  return 0;
}

bool load_obj_file(char const *filename, ObjObject *obj)
{
  FILE *f_ptr = fopen(filename, "r");
  if (!f_ptr)
  {
    perror("fopen");
    return false;
  }

  obj_VertexVec   vertices = {0};
  obj_TexCoordVec uvs      = {0};
  obj_NormalVec   normals  = {0};
  obj_FaceVec     faces    = {0};

  char line[256];

  while (fgets(line, sizeof(line), f_ptr))
  {
    if (*line == '\0' || *line == '#' || *line == '\n') continue;

    if (line[0] == 'v')
    {
      if (line[1] == ' ')
      {
        if (parse_v(line, &vertices) < 0) goto error;
      }
      else if (line[1] == 't')
      {
        if (parse_vt(line, &uvs) < 0) goto error;
      }
      else if (line[1] == 'n')
      {
        if (parse_vn(line, &normals) < 0) goto error;
      }
    }
    else if (line[0] == 'f')
    {
      if (parse_f(line, &faces) < 0) goto error;
    }
  }

  fclose(f_ptr);

  obj->verts        = vertices.data;
  obj->vertex_count = vertices.size;
  obj->uvs          = uvs.data;
  obj->uv_count     = uvs.size;
  obj->normals      = normals.data;
  obj->normal_count = normals.size;
  obj->faces        = faces.data;
  obj->face_count   = faces.size;

  return true;

error:
  fclose(f_ptr);

  free(vertices.data);
  free(uvs.data);
  free(normals.data);
  free(faces.data);

  return false;
}

void free_obj_object(ObjObject *obj)
{
  free(obj->verts);
  free(obj->uvs);
  free(obj->normals);
  free(obj->faces);
}
