#pragma once

#include "math.h"


typedef struct {
  union {
    struct {
      float x, y, z;
    };
    float v[3];
  };
} Vec3;

typedef struct {
  union {
    struct {
      float x, y, z, w;
    };
    float v[4];
  };
} Vec4;

Vec3 vec3(Vec4);
Vec4 vec4(Vec3);
Vec3 new_vec3(float, float, float);
Vec4 new_vec4(float, float, float, float);

Vec3  vec3_add_val(Vec3, float);
Vec3  vec3_sub_val(Vec3, float);
Vec3  vec3_mult_val(Vec3, float);
Vec3  vec3_div_val(Vec3, float);
Vec3  vec3_neg(Vec3);
Vec3  vec3_add(Vec3, Vec3);
Vec3  vec3_sub(Vec3, Vec3);
Vec3  vec3_norm(Vec3);
float vec3_length(Vec3);
float vec3_dist(Vec3, Vec3);
Vec3  cross(Vec3, Vec3);
float dot3(Vec3, Vec3);
Vec3  vec3_mult(Vec3, Vec3);
float vec3_angle(Vec3, Vec3);

Vec4  vec4_add_val(Vec4, float);
Vec4  vec4_sub_val(Vec4, float);
Vec4  vec4_mult_val(Vec4, float);
Vec4  vec4_div_val(Vec4, float);
Vec4  vec4_neg(Vec4);
Vec4  vec4_add(Vec4, Vec4);
Vec4  vec4_sub(Vec4, Vec4);
Vec4  vec4_norm(Vec4);
float vec4_length(Vec4);
float vec4_dist(Vec4, Vec4);
float dot4(Vec4, Vec4);
Vec4  vec4_lerp(Vec4, Vec4, float);

typedef struct {
  float m[9];
} Mat3;

typedef struct {
  float m[16];
} Mat4;

Mat3 mat4_to_mat3(Mat4);
Mat3 mat3_add(Mat3, Mat3);
Mat3 mat3_sub(Mat3, Mat3);
Mat3 mat3_scale(Mat3, float);
Mat3 mat3_mult(Mat3, Mat3);
Mat3 mat3_transpose(Mat3);
Mat3 mat3_inverse(Mat3);

Mat4 mat4_add(Mat4, Mat4);
Mat4 mat4_sub(Mat4, Mat4);
Mat4 mat4_scale(Mat4, float);
Mat4 mat4_mult(Mat4, Mat4);
Mat4 mat4_transpose(Mat4);
Mat4 mat4_inverse(Mat4);

Mat4 identity();
Mat4 scale(float);
Mat4 rotate_x(float);
Mat4 rotate_y(float);
Mat4 rotate_z(float);
Mat4 translate(float, float, float);
Vec4 transform(Mat4, Vec4);
Vec3 transform_mat3(Mat3, Vec3);
Vec4 transform_vec3(Mat4, Vec3);
Mat4 ortho(float, float, float, float, float, float);
Mat4 perspective(float, float, float, float);
