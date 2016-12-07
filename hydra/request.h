#ifndef __request_h__
#define __request_h__

#include <ev.h>
#include "common.h"
#include "server.h"


typedef struct {
  unsigned error_code : 10;
  unsigned parse_finished : 1;
  unsigned thrift_method_called : 1;
  unsigned thrift_call_done : 1;
  unsigned keep_alive : 1;
} request_state;

typedef struct {
  size_t sequence_id;
  PyObject* api;
  PyObject* api_args;
  PyObject* api_result
} call_data;

typedef struct {
#ifdef DEBUG
  unsigned long id;
#endif
  ev_io ev_watcher;

  ServerInfo* server_info;
  int client_fd;
  PyObject* client_addr;

  request_state state;
  call_data single_call;
} Request;

#define REQUEST_FROM_WATCHER(watcher) \
  (Request*)((size_t)watcher - (size_t)(&(((Request*)NULL)->ev_watcher)));

Request* Request_new(ServerInfo*, int client_fd, const char* client_addr);
void Request_decode(Request*, const char*, const size_t);
void Request_free(Request*);

#endif
