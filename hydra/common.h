#ifndef __common_h__
#define __common_h__

#include <Python.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define TYPE_ERROR_INNER(what, expected, ...) \
  PyErr_Format(PyExc_TypeError, what " must be " expected " " __VA_ARGS__)
#define TYPE_ERROR(what, expected, got) \
  TYPE_ERROR_INNER(what, expected, "(got '%.200s' object instead)", Py_TYPE(got)->tp_name)

typedef struct { char* data; size_t len; } string;

enum thrift_status { THRIFT_BAD_REQUEST = 1, THRIFT_SERVER_ERROR };

#ifdef DEBUG
  #define DBG_REQ(request, ...) \
    do { \
      printf("[DEBUG Req %ld] ", request->id); \
      DBG(__VA_ARGS__); \
    } while(0)
  #define DBG(...) \
    do { \
      printf(__VA_ARGS__); \
      printf("\n"); \
    } while(0)
#else
  #define DBG(...) do{}while(0)
  #define DBG_REQ(...) DBG(__VA_ARGS__)
#endif

#define DBG_REFCOUNT(obj) \
  DBG(#obj "->obj_refcnt: %d", obj->ob_refcnt)

#define DBG_REFCOUNT_REQ(request, obj) \
  DBG_REQ(request, #obj "->ob_refcnt: %d", obj->ob_refcnt)

#ifdef WITHOUT_ASSERTS
  #undef assert
  #define assert(...) do{}while(0)
#endif

#endif
