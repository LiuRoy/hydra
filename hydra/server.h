#ifndef HYDRA_SERVER_H
#define HYDRA_SERVER_H

typedef struct {
    int sock_fd;
    PyObject* service;
    PyObject* handler;
    PyObject* host;
    PyObject* port;
} ServerInfo;

void server_run(ServerInfo*);

#endif // HYDRA_SERVER_H
