#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIALIZE_VECTOR_TEMPLATE(TYPE)                                       \
                                                                               \
  typedef struct {                                                             \
    size_t size, capacity;                                                     \
    TYPE  *data;                                                               \
  } TYPE##Vec;                                                                 \
                                                                               \
  static inline void TYPE##_append(TYPE##Vec *vec, TYPE x)                     \
  {                                                                            \
    if (vec->size >= vec->capacity)                                            \
    {                                                                          \
      vec->capacity = vec->capacity ? vec->capacity * 2 : 256;                 \
                                                                               \
      TYPE *tmp = realloc(vec->data, vec->capacity * sizeof(*vec->data));      \
      if (!tmp)                                                                \
      {                                                                        \
        fprintf(stderr, "Vector allocation failed\n");                         \
        abort();                                                               \
      }                                                                        \
                                                                               \
      vec->data = tmp;                                                         \
    }                                                                          \
                                                                               \
    vec->data[vec->size++] = x;                                                \
  }                                                                            \
                                                                               \
  static inline void TYPE##_clear(TYPE##Vec *vec) { vec->size = 0; }           \
                                                                               \
  static inline bool TYPE##_empty(const TYPE##Vec *vec)                        \
  {                                                                            \
    return vec->size == 0;                                                     \
  }                                                                            \
                                                                               \
  static inline void TYPE##_remove(TYPE##Vec *vec, size_t i)                   \
  {                                                                            \
    if (i >= vec->size) return;                                                \
                                                                               \
    size_t nr_of_elems = vec->size - i - 1;                                    \
                                                                               \
    memmove(&vec->data[i], &vec->data[i + 1],                                  \
            nr_of_elems * sizeof(*vec->data));                                 \
                                                                               \
    vec->size--;                                                               \
  }                                                                            \
                                                                               \
  static inline TYPE TYPE##_pop(TYPE##Vec *vec)                                \
  {                                                                            \
    if (vec->size == 0)                                                        \
    {                                                                          \
      fprintf(stderr, "Tried to pop() empty vector\n");                        \
      abort();                                                                 \
    }                                                                          \
                                                                               \
    return vec->data[--vec->size];                                             \
  }                                                                            \
                                                                               \
  static inline void TYPE##_insert(TYPE##Vec *vec, TYPE elem, size_t i)        \
  {                                                                            \
    if (i > vec->size) return;                                                 \
                                                                               \
    if (vec->size >= vec->capacity)                                            \
    {                                                                          \
      vec->capacity = vec->capacity ? vec->capacity * 2 : 256;                 \
                                                                               \
      TYPE *tmp = realloc(vec->data, vec->capacity * sizeof(*vec->data));      \
      if (!tmp)                                                                \
      {                                                                        \
        fprintf(stderr, "Vector allocation failed\n");                         \
        abort();                                                               \
      }                                                                        \
                                                                               \
      vec->data = tmp;                                                         \
    }                                                                          \
                                                                               \
    size_t nr_of_elems = vec->size - i;                                        \
                                                                               \
    memmove(&vec->data[i + 1], &vec->data[i],                                  \
            nr_of_elems * sizeof(*vec->data));                                 \
                                                                               \
    vec->data[i] = elem;                                                       \
    vec->size++;                                                               \
  }                                                                            \
                                                                               \
  static inline void TYPE##_sort(TYPE##Vec *vec,                               \
                                 int (*cmp)(const void *, const void *))       \
  {                                                                            \
    qsort(vec->data, vec->size, sizeof(*vec->data), cmp);                      \
  }                                                                            \
                                                                               \
  static inline bool TYPE##_contains(const TYPE##Vec *vec, TYPE elem,          \
                                     int (*cmp)(const void *, const void *))   \
  {                                                                            \
    for (size_t i = 0; i < vec->size; i++)                                     \
    {                                                                          \
      if (cmp(&vec->data[i], &elem) == 0) return true;                         \
    }                                                                          \
                                                                               \
    return false;                                                              \
  }                                                                            \
                                                                               \
  static inline TYPE##Vec TYPE##_copy(const TYPE##Vec *vec)                    \
  {                                                                            \
    TYPE##Vec copy = {0};                                                      \
                                                                               \
    copy.capacity = vec->capacity;                                             \
    copy.size     = vec->size;                                                 \
                                                                               \
    if (copy.capacity)                                                         \
    {                                                                          \
      copy.data = malloc(copy.capacity * sizeof(*copy.data));                  \
                                                                               \
      if (!copy.data)                                                          \
      {                                                                        \
        fprintf(stderr, "Vector allocation failed\n");                         \
        abort();                                                               \
      }                                                                        \
    }                                                                          \
                                                                               \
    if (copy.size)                                                             \
      memcpy(copy.data, vec->data, copy.size * sizeof(*copy.data));            \
                                                                               \
    return copy;                                                               \
  }                                                                            \
                                                                               \
  static inline void TYPE##_free(TYPE##Vec *vec)                               \
  {                                                                            \
    free(vec->data);                                                           \
    vec->data     = NULL;                                                      \
    vec->size     = 0;                                                         \
    vec->capacity = 0;                                                         \
  }                                                                            \
                                                                               \
  static inline TYPE TYPE##_reduce(const TYPE##Vec *vec,                       \
                                   TYPE (*func)(TYPE, TYPE), TYPE init_val)    \
  {                                                                            \
    TYPE ret_val = init_val;                                                   \
                                                                               \
    for (size_t i = 0; i < vec->size; i++)                                     \
      ret_val = func(ret_val, vec->data[i]);                                   \
                                                                               \
    return ret_val;                                                            \
  }                                                                            \
                                                                               \
  static inline TYPE##Vec TYPE##_filter(TYPE##Vec *vec, bool (*func)(TYPE))    \
  {                                                                            \
    size_t curr_write = 0;                                                     \
    for (size_t curr_read = 0; curr_read < vec->size; ++curr_read)             \
    {                                                                          \
      if (func(vec->data[curr_read]))                                          \
      {                                                                        \
        vec->data[curr_write++] = vec->data[curr_read];                        \
      }                                                                        \
    }                                                                          \
    vec->size = curr_write;                                                    \
  }                                                                            \
                                                                               \
  static inline void TYPE##_for_each(const TYPE##Vec *vec,                     \
                                     void (*func)(TYPE, void *), void *ctx)    \
  {                                                                            \
    for (size_t i = 0; i < vec->size; i++) func(vec->data[i], ctx);            \
  }                                                                            \
                                                                               \
  static inline void TYPE##_map(TYPE##Vec *vec, void (*func)(TYPE *, void *),  \
                                void      *ctx)                                \
  {                                                                            \
    for (size_t i = 0; i < vec->size; i++) func(&vec->data[i], ctx);           \
  }                                                                            \
                                                                               \
  static inline void TYPE##_swap_remove(TYPE##Vec *vec, size_t idx)            \
  {                                                                            \
    vec->data[idx] = vec->data[--vec->size];                                   \
  }
