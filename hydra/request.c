#include <Python.h>
#include "request.h"
#include "common.h"

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

  return request;
}

void Request_free(Request* request)
{
  Py_DECREF(request->client_addr);
  free(request);
}

void Request_decode(Request* request, const char* data, const size_t data_len)
{
  assert(data_len);
  DBG_REQ(request, "decoding... data_length: %s", data_len);

  request->state.parse_finished = true;
}

