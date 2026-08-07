#include "linalg.h"

Vec3 vec3(Vec4 v) { return (Vec3){v.x, v.y, v.z}; }

Vec4 vec4(Vec3 v) { return (Vec4){v.x, v.y, v.z, 1.0}; }

Vec3 new_vec3(float x, float y, float z) { return (Vec3){x, y, z}; }

Vec4 new_vec4(float x, float y, float z, float w) { return (Vec4){x, y, z, w}; }

Vec3 vec3_add_val(Vec3 v, float x) { return (Vec3){v.x + x, v.y + x, v.z + x}; }

Vec3 vec3_sub_val(Vec3 v, float x) { return (Vec3){v.x - x, v.y - x, v.z - x}; }

Vec3 vec3_mult_val(Vec3 v, float x)
{
  return (Vec3){v.x * x, v.y * x, v.z * x};
}

Vec3 vec3_div_val(Vec3 v, float x) { return (Vec3){v.x / x, v.y / x, v.z / x}; }

Vec3 vec3_neg(Vec3 v) { return (Vec3){-v.x, -v.y, -v.z}; }

Vec3 vec3_add(Vec3 v, Vec3 u)
{
  return (Vec3){v.x + u.x, v.y + u.y, v.z + u.z};
}

Vec3 vec3_sub(Vec3 v, Vec3 u)
{
  return (Vec3){v.x - u.x, v.y - u.y, v.z - u.z};
}

float vec3_length(Vec3 v) { return sqrt(v.x * v.x + v.y * v.y + v.z * v.z); }

Vec3 vec3_norm(Vec3 v) { return vec3_div_val(v, vec3_length(v)); }

Vec3 cross(Vec3 v, Vec3 u)
{
  return (
    Vec3){v.y * u.z - v.z * u.y, v.z * u.x - v.x * u.z, v.x * u.y - v.y * u.x};
}

float vec3_dist(Vec3 v, Vec3 u) { return vec3_length(vec3_sub(v, u)); }

float dot3(Vec3 v, Vec3 u) { return (v.x * u.x + v.y * u.y + v.z * u.z); }

Vec3 vec3_mult(Vec3 v, Vec3 u)
{
  return new_vec3(v.x * u.x, v.y * u.y, v.z * u.z);
}


float vec3_angle(Vec3 v, Vec3 u)
{
  return acosf(dot3(v, u) / (vec3_length(v) * vec3_length(u)));
}

Vec4 vec4_add_val(Vec4 v, float x)
{
  return (Vec4){v.x + x, v.y + x, v.z + x, v.w + x};
}

Vec4 vec4_sub_val(Vec4 v, float x)
{
  return (Vec4){v.x - x, v.y - x, v.z - x, v.w - x};
}

Vec4 vec4_mult_val(Vec4 v, float x)
{
  return (Vec4){v.x * x, v.y * x, v.z * x, v.w * x};
}

Vec4 vec4_div_val(Vec4 v, float x)
{
  return (Vec4){v.x / x, v.y / x, v.z / x, v.w / x};
}

Vec4 vec4_neg(Vec4 v) { return (Vec4){-v.x, -v.y, -v.z, -v.w}; }

Vec4 vec4_add(Vec4 v, Vec4 u)
{
  return (Vec4){v.x + u.x, v.y + u.y, v.z + u.z, v.w + u.w};
}

Vec4 vec4_sub(Vec4 v, Vec4 u)
{
  return (Vec4){v.x - u.x, v.y - u.y, v.z - u.z, v.w - u.w};
}

float vec4_length(Vec4 v)
{
  return sqrt(v.x * v.x + v.y * v.y + v.z * v.z + v.w * v.w);
}

Vec4 vec4_norm(Vec4 v) { return vec4_div_val(v, vec4_length(v)); }

float vec4_dist(Vec4 v, Vec4 u) { return vec4_length(vec4_sub(v, u)); }

float dot4(Vec4 v, Vec4 u)
{
  return (v.x * u.x + v.y * u.y + v.z * u.z + v.w * u.w);
}

Vec4 vec4_lerp(Vec4 v, Vec4 u, float t)
{
  Vec4 out;
  out.x = v.x + t * (u.x - v.x);
  out.y = v.y + t * (u.y - v.y);
  out.z = v.z + t * (u.z - v.z);
  out.w = v.w + t * (u.w - v.w);
  return out;
}

Mat3 mat4_to_mat3(Mat4 m)
{
  return (Mat3){
    m.m[0],
    m.m[1],
    m.m[2],
    m.m[4],
    m.m[5],
    m.m[6],
    m.m[8],
    m.m[9],
    m.m[10],
  };
}

Mat3 mat3_add(Mat3 m1, Mat3 m2)
{
  return (Mat3){
    m1.m[0] + m2.m[0],
    m1.m[1] + m2.m[1],
    m1.m[2] + m2.m[2],
    m1.m[3] + m2.m[3],
    m1.m[4] + m2.m[4],
    m1.m[5] + m2.m[5],
    m1.m[6] + m2.m[6],
    m1.m[7] + m2.m[7],
    m1.m[8] + m2.m[8],
  };
}

Mat3 mat3_sub(Mat3 m1, Mat3 m2)
{
  return (Mat3){
    m1.m[0] - m2.m[0],
    m1.m[1] - m2.m[1],
    m1.m[2] - m2.m[2],
    m1.m[3] - m2.m[3],
    m1.m[4] - m2.m[4],
    m1.m[5] - m2.m[5],
    m1.m[6] - m2.m[6],
    m1.m[7] - m2.m[7],
    m1.m[8] - m2.m[8],
  };
}

Mat3 mat3_scale(Mat3 m, float x)
{
  return (Mat3){
    m.m[0] * x,
    m.m[1] * x,
    m.m[2] * x,
    m.m[3] * x,
    m.m[4] * x,
    m.m[5] * x,
    m.m[6] * x,
    m.m[7] * x,
    m.m[8] * x,
  };
}

Mat3 mat3_mult(Mat3 m1, Mat3 m2)
{
  return (Mat3){
    m1.m[0] * m2.m[0] + m1.m[1] * m2.m[3] + m1.m[2] * m2.m[6],
    m1.m[0] * m2.m[1] + m1.m[1] * m2.m[4] + m1.m[2] * m2.m[7],
    m1.m[0] * m2.m[2] + m1.m[1] * m2.m[5] + m1.m[2] * m2.m[8],

    m1.m[3] * m2.m[0] + m1.m[4] * m2.m[3] + m1.m[5] * m2.m[6],
    m1.m[3] * m2.m[1] + m1.m[4] * m2.m[4] + m1.m[5] * m2.m[7],
    m1.m[3] * m2.m[2] + m1.m[4] * m2.m[5] + m1.m[5] * m2.m[8],

    m1.m[6] * m2.m[0] + m1.m[7] * m2.m[3] + m1.m[8] * m2.m[6],
    m1.m[6] * m2.m[1] + m1.m[7] * m2.m[4] + m1.m[8] * m2.m[7],
    m1.m[6] * m2.m[2] + m1.m[7] * m2.m[5] + m1.m[8] * m2.m[8],
  };
}


Mat3 mat3_transpose(Mat3 m)
{
  return (Mat3){
    m.m[0],
    m.m[3],
    m.m[6],
    m.m[1],
    m.m[4],
    m.m[7],
    m.m[2],
    m.m[5],
    m.m[8],
  };
}

Mat3 mat3_inverse(Mat3 m)
{
  float a00 = m.m[0], a01 = m.m[3], a02 = m.m[6];
  float a10 = m.m[1], a11 = m.m[4], a12 = m.m[7];
  float a20 = m.m[2], a21 = m.m[5], a22 = m.m[8];

  float det = a00 * (a11 * a22 - a12 * a21) - a01 * (a10 * a22 - a12 * a20) +
              a02 * (a10 * a21 - a11 * a20);

  if (det == 0.0f) return (Mat3){0};

  float invDet = 1.0f / det;
  return (Mat3){
    (a11 * a22 - a12 * a21) * invDet,
    -(a10 * a22 - a12 * a20) * invDet,
    (a10 * a21 - a11 * a20) * invDet,
    -(a01 * a22 - a02 * a21) * invDet,
    (a00 * a22 - a02 * a20) * invDet,
    -(a00 * a21 - a01 * a20) * invDet,
    (a01 * a12 - a02 * a11) * invDet,
    -(a00 * a12 - a02 * a10) * invDet,
    (a00 * a11 - a01 * a10) * invDet,
  };
}


Mat4 mat4_add(Mat4 m1, Mat4 m2)
{
  return (Mat4){
    m1.m[0] + m2.m[0],
    m1.m[1] + m2.m[1],
    m1.m[2] + m2.m[2],
    m1.m[3] + m2.m[3],
    m1.m[4] + m2.m[4],
    m1.m[5] + m2.m[5],
    m1.m[6] + m2.m[6],
    m1.m[7] + m2.m[7],
    m1.m[8] + m2.m[8],
    m1.m[9] + m2.m[9],
    m1.m[10] + m2.m[10],
    m1.m[11] + m2.m[11],
    m1.m[12] + m2.m[12],
    m1.m[13] + m2.m[13],
    m1.m[14] + m2.m[14],
    m1.m[15] + m2.m[15],
  };
}

Mat4 mat4_sub(Mat4 m1, Mat4 m2)
{
  return (Mat4){
    m1.m[0] - m2.m[0],
    m1.m[1] - m2.m[1],
    m1.m[2] - m2.m[2],
    m1.m[3] - m2.m[3],
    m1.m[4] - m2.m[4],
    m1.m[5] - m2.m[5],
    m1.m[6] - m2.m[6],
    m1.m[7] - m2.m[7],
    m1.m[8] - m2.m[8],
    m1.m[9] - m2.m[9],
    m1.m[10] - m2.m[10],
    m1.m[11] - m2.m[11],
    m1.m[12] - m2.m[12],
    m1.m[13] - m2.m[13],
    m1.m[14] - m2.m[14],
    m1.m[15] - m2.m[15],
  };
}

Mat4 mat4_scale(Mat4 m, float x)
{
  return (Mat4){
    m.m[0] * x,
    m.m[1] * x,
    m.m[2] * x,
    m.m[3] * x,
    m.m[4] * x,
    m.m[5] * x,
    m.m[6] * x,
    m.m[7] * x,
    m.m[8] * x,
    m.m[9] * x,
    m.m[10] * x,
    m.m[11] * x,
    m.m[12] * x,
    m.m[13] * x,
    m.m[14] * x,
    m.m[15] * x,
  };
}

Mat4 mat4_mult(Mat4 m1, Mat4 m2)
{
  return (Mat4){
    m1.m[0] * m2.m[0] + m1.m[4] * m2.m[1] + m1.m[8] * m2.m[2] +
      m1.m[12] * m2.m[3],
    m1.m[1] * m2.m[0] + m1.m[5] * m2.m[1] + m1.m[9] * m2.m[2] +
      m1.m[13] * m2.m[3],
    m1.m[2] * m2.m[0] + m1.m[6] * m2.m[1] + m1.m[10] * m2.m[2] +
      m1.m[14] * m2.m[3],
    m1.m[3] * m2.m[0] + m1.m[7] * m2.m[1] + m1.m[11] * m2.m[2] +
      m1.m[15] * m2.m[3],
    m1.m[0] * m2.m[4] + m1.m[4] * m2.m[5] + m1.m[8] * m2.m[6] +
      m1.m[12] * m2.m[7],
    m1.m[1] * m2.m[4] + m1.m[5] * m2.m[5] + m1.m[9] * m2.m[6] +
      m1.m[13] * m2.m[7],
    m1.m[2] * m2.m[4] + m1.m[6] * m2.m[5] + m1.m[10] * m2.m[6] +
      m1.m[14] * m2.m[7],
    m1.m[3] * m2.m[4] + m1.m[7] * m2.m[5] + m1.m[11] * m2.m[6] +
      m1.m[15] * m2.m[7],
    m1.m[0] * m2.m[8] + m1.m[4] * m2.m[9] + m1.m[8] * m2.m[10] +
      m1.m[12] * m2.m[11],
    m1.m[1] * m2.m[8] + m1.m[5] * m2.m[9] + m1.m[9] * m2.m[10] +
      m1.m[13] * m2.m[11],
    m1.m[2] * m2.m[8] + m1.m[6] * m2.m[9] + m1.m[10] * m2.m[10] +
      m1.m[14] * m2.m[11],
    m1.m[3] * m2.m[8] + m1.m[7] * m2.m[9] + m1.m[11] * m2.m[10] +
      m1.m[15] * m2.m[11],
    m1.m[0] * m2.m[12] + m1.m[4] * m2.m[13] + m1.m[8] * m2.m[14] +
      m1.m[12] * m2.m[15],
    m1.m[1] * m2.m[12] + m1.m[5] * m2.m[13] + m1.m[9] * m2.m[14] +
      m1.m[13] * m2.m[15],
    m1.m[2] * m2.m[12] + m1.m[6] * m2.m[13] + m1.m[10] * m2.m[14] +
      m1.m[14] * m2.m[15],
    m1.m[3] * m2.m[12] + m1.m[7] * m2.m[13] + m1.m[11] * m2.m[14] +
      m1.m[15] * m2.m[15],
  };
}

Mat4 mat4_transpose(Mat4 m)
{
  return (Mat4){
    m.m[0],
    m.m[4],
    m.m[8],
    m.m[12],
    m.m[1],
    m.m[5],
    m.m[9],
    m.m[13],
    m.m[2],
    m.m[6],
    m.m[10],
    m.m[14],
    m.m[3],
    m.m[7],
    m.m[11],
    m.m[15],
  };
}

Mat4 mat4_inverse(Mat4 m)
{
  float a00 = m.m[0], a01 = m.m[4], a02 = m.m[8], a03 = m.m[12];
  float a10 = m.m[1], a11 = m.m[5], a12 = m.m[9], a13 = m.m[13];
  float a20 = m.m[2], a21 = m.m[6], a22 = m.m[10], a23 = m.m[14];
  float a30 = m.m[3], a31 = m.m[7], a32 = m.m[11], a33 = m.m[15];

  float b00 = a00 * a11 - a01 * a10;
  float b01 = a00 * a12 - a02 * a10;
  float b02 = a00 * a13 - a03 * a10;
  float b03 = a01 * a12 - a02 * a11;
  float b04 = a01 * a13 - a03 * a11;
  float b05 = a02 * a13 - a03 * a12;
  float b06 = a20 * a31 - a21 * a30;
  float b07 = a20 * a32 - a22 * a30;
  float b08 = a20 * a33 - a23 * a30;
  float b09 = a21 * a32 - a22 * a31;
  float b10 = a21 * a33 - a23 * a31;
  float b11 = a22 * a33 - a23 * a32;

  float det =
    b00 * b11 - b01 * b10 + b02 * b09 + b03 * b08 - b04 * b07 + b05 * b06;

  if (det == 0.0f) return (Mat4){0};

  float invDet = 1.0f / det;
  return (Mat4){
    (a11 * b11 - a12 * b10 + a13 * b09) * invDet,
    (-a10 * b11 + a12 * b08 - a13 * b07) * invDet,
    (a10 * b10 - a11 * b08 + a13 * b06) * invDet,
    (-a10 * b09 + a11 * b07 - a12 * b06) * invDet,
    (-a01 * b11 + a02 * b10 - a03 * b09) * invDet,
    (a00 * b11 - a02 * b08 + a03 * b07) * invDet,
    (-a00 * b10 + a01 * b08 - a03 * b06) * invDet,
    (a00 * b09 - a01 * b07 + a02 * b06) * invDet,
    (a31 * b05 - a32 * b04 + a33 * b03) * invDet,
    (-a30 * b05 + a32 * b02 - a33 * b01) * invDet,
    (a30 * b04 - a31 * b02 + a33 * b00) * invDet,
    (-a30 * b03 + a31 * b01 - a32 * b00) * invDet,
    (-a21 * b05 + a22 * b04 - a23 * b03) * invDet,
    (a20 * b05 - a22 * b02 + a23 * b01) * invDet,
    (-a20 * b04 + a21 * b02 - a23 * b00) * invDet,
    (a20 * b03 - a21 * b01 + a22 * b00) * invDet,
  };
}

Mat4 identity()
{
  return (Mat4){
    1.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
  };
}

Mat4 scale(float x)
{
  return (Mat4){
    x,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    x,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    x,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
  };
}

Mat4 translate(float x, float y, float z)
{
  return (Mat4){1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.0f,
                0.0f,
                0.0f,
                0.0f,
                1.0f,
                0.0f,
                x,
                y,
                z,
                1.0f};
}

// NOTE: math.h cos/sin/etc expect radians
Mat4 rotate_x(float r)
{
  return (Mat4){
    1.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    cos(r),
    -sin(r),
    0.0f,
    0.0f,
    sin(r),
    cos(r),
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
  };
}

Mat4 rotate_y(float r)
{
  return (Mat4){
    cos(r),
    0.0f,
    sin(r),
    0.0f,
    0.0f,
    1.0f,
    0.0f,
    0.0f,
    -sin(r),
    0.0f,
    cos(r),
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
  };
}

Mat4 rotate_z(float r)
{
  return (Mat4){
    cos(r),
    -sin(r),
    0.0f,
    0.0f,
    sin(r),
    cos(r),
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
    0.0f,
    0.0f,
    0.0f,
    0.0f,
    1.0f,
  };
}

Vec4 transform(Mat4 m, Vec4 v)
{
  return (Vec4){
    m.m[0] * v.x + m.m[4] * v.y + m.m[8] * v.z + m.m[12] * v.w,
    m.m[1] * v.x + m.m[5] * v.y + m.m[9] * v.z + m.m[13] * v.w,
    m.m[2] * v.x + m.m[6] * v.y + m.m[10] * v.z + m.m[14] * v.w,
    m.m[3] * v.x + m.m[7] * v.y + m.m[11] * v.z + m.m[15] * v.w,
  };
}

Vec3 transform_mat3(Mat3 m, Vec3 v)
{
  return (Vec3){
    m.m[0] * v.x + m.m[3] * v.y + m.m[6] * v.z,
    m.m[1] * v.x + m.m[4] * v.y + m.m[7] * v.z,
    m.m[2] * v.x + m.m[5] * v.y + m.m[8] * v.z,
  };
}

Vec4 transform_vec3(Mat4 m, Vec3 v) { return transform(m, vec4(v)); }

Mat4 perspective(float fov, float aspect, float near, float far)
{
  Mat4 result = {0};

  double top    = near * tan(fov * 0.5);
  double bottom = -top;
  double right  = top * aspect;
  double left   = -right;

  float rl = (float)(right - left);
  float tb = (float)(top - bottom);
  float fn = (float)(far - near);

  result.m[0]  = ((float)near * 2.0f) / rl;
  result.m[5]  = ((float)near * 2.0f) / tb;
  result.m[8]  = ((float)right + (float)left) / rl;
  result.m[9]  = ((float)top + (float)bottom) / tb;
  result.m[10] = -((float)far + (float)near) / fn;
  result.m[11] = -1.0f;
  result.m[14] = -((float)far * (float)near * 2.0f) / fn;

  return result;
}
