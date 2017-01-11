#include "request.h"

Request* Request_new(ServerInfo* server_info, int client_fd, const char* client_address) {
    Request* request = malloc(sizeof(Request));
#ifdef DEBUG
    static unsigned long request_id = 0;
    request->id = request_id++;
#endif
    request->server_info = server_info;
    request->client_fd = client_fd;
    request->client_address = PyString_FromString(client_address);

    Request_reset(request);
    return request;
}

void Request_reset(Request* request) {
    memset(&request->state, 0, sizeof(request_state));
    request->state.parse_begin = true;
    memset(&request->parser, 0, sizeof(thrift_parser));
    request->parser.data = request;

    request->sequence_id = 0;
    request->method_name = NULL;
    request->method_args = NULL;
    request->method_result = NULL;
}

void Request_free(Request* request) {
    Py_DECREF(request->client_address);
    Py_DECREF(request->method_name);
    Py_DECREF(request->method_args);
    Py_DECREF(request->method_result);
    free(request);
}

void Request_decode(Request* request, const char* data, const size_t data_len) {
    assert(data_len);

    binary_decode(&request->parser, data, data_len);
}
