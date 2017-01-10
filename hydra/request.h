#ifndef HYDRA_REQUEST_H
#define HYDRA_REQUEST_H

#include <ev.h>
#include "common.h"
#include "server.h"
#include "parser.h"


typedef struct {
    unsigned error_code : 10;
    unsigned parse_finished : 1;
    unsigned thrift_method_called : 1;
    unsigned thrift_call_done : 1;
    unsigned keep_alive : 1;
} request_state;


typedef struct {
    unsigned sequence_id;        // thrift协议序列号
    PyObject* method_name;       // 调用方法名称
    PyObject* method_args;       // 方法参数
    PyObject* method_result;     // 返回结果
    thrift_parser parser;
} hydra_parser;


typedef struct {
#ifdef DEBUG
    unsigned long id;
#endif
    hydra_parser parser;
    ev_io ev_watcher;

    ServerInfo* server_info;
    int client_fd;
    PyObject* client_address;

    request_state state;
} Request;

#define REQUEST_FROM_WATCHER(watcher) \
  (Request*)((size_t)watcher - (size_t)(&(((Request*)NULL)->ev_watcher)));

Request* Request_new(ServerInfo*, int client_fd, const char* client_address);
void Request_decode(Request*, const char*, const size_t);
void Request_free(Request*);

#endif //HYDRA_REQUEST_H
