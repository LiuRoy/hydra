#include <Python.h>
#include <cStringIO.h>
#include "request.h"

static inline void PyDict_ReplaceKey(PyObject* dict, PyObject* k1, PyObject* k2);
static PyObject* wsgi_http_header(string header);
static PyObject* wsgi_base_dict = NULL;

/* Non-public type from cStringIO I abuse in on_body */
typedef struct {
  PyObject_HEAD
  char *buf;
  Py_ssize_t pos, string_size;
  PyObject *pbuf;
} Iobject;

Request* Request_new(ServerInfo* server_info, int client_fd, const char* client_addr)
{
  Request* request = malloc(sizeof(Request));
#ifdef DEBUG
  static unsigned long request_id = 0;
  request->id = request_id++;
#endif
  request->server_info = server_info;
  request->client_fd = client_fd;
  request->client_addr = PyString_FromString(client_addr);
  http_parser_init((http_parser*)&request->parser, HTTP_REQUEST);
  request->parser.parser.data = request;
  Request_reset(request);
  return request;
}

void Request_free(Request* request)
{
  Request_clean(request);
  Py_DECREF(request->client_addr);
  free(request);
}

/* Parse stuff */

void Request_decode(Request* request, const char* data, const size_t data_len)
{
  assert(data_len);
  size_t nparsed = thrift_binary_decode((thrift_parser*)&request->parser,
                                        data, data_len);
  if(nparsed != data_len)
    request->state.error_code = THRIFT_BAD_REQUEST;
}
