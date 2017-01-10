#include <arpa/inet.h>
#include <fcntl.h>
#include <ev.h>

#ifdef WANT_SIGINT_HANDLING
# include <sys/signal.h>
#endif

#include "common.h"
#include "server.h"
#include "request.h"

#define BUFFER_SIZE 64*1024
#define GIL_LOCK(n) PyGILState_STATE _gilstate_##n = PyGILState_Ensure()
#define GIL_UNLOCK(n) PyGILState_Release(_gilstate_##n)

enum _rw_state {
    not_yet_done = 1,
    done,
    aborted,
};
typedef enum _rw_state read_state;
typedef enum _rw_state write_state;

typedef struct {
    ServerInfo* server_info;
    ev_io accept_watcher;
} ThreadInfo;

typedef void ev_io_callback(struct ev_loop*, ev_io*, const int);

#if WANT_SIGINT_HANDLING
typedef void ev_signal_callback(struct ev_loop*, ev_signal*, const int);
static ev_signal_callback ev_signal_on_sigint;
#endif

static ev_io_callback ev_io_on_request;
static ev_io_callback ev_io_on_read;
static ev_io_callback ev_io_on_write;
static void close_connection(struct ev_loop*, Request*);


void server_run(ServerInfo* server_info) {
    struct ev_loop* mainloop = ev_loop_new(0);

    ThreadInfo thread_info;
    thread_info.server_info = server_info;
    ev_set_userdata(mainloop, &thread_info);

    ev_io_init(&thread_info.accept_watcher, ev_io_on_request, server_info->sock_fd, EV_READ);
    ev_io_start(mainloop, &thread_info.accept_watcher);

#if WANT_SIGINT_HANDLING
    ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, ev_signal_on_sigint, SIGINT);
    ev_signal_start(mainloop, &signal_watcher);
#endif

    /* This is the program main loop */
    Py_BEGIN_ALLOW_THREADS
    ev_run(mainloop, 0);
    ev_loop_destroy(mainloop);
    Py_END_ALLOW_THREADS
}

#if WANT_SIGINT_HANDLING
static void
pyerr_set_interrupt(struct ev_loop* mainloop, struct ev_cleanup* watcher, const int events) {
  PyErr_SetInterrupt();
  free(watcher);
}

static void
ev_signal_on_sigint(struct ev_loop* mainloop, ev_signal* watcher, const int events) {
  /* Clean up and shut down this thread.
   * (Shuts down the Python interpreter if this is the main thread) */
  ev_cleanup* cleanup_watcher = malloc(sizeof(ev_cleanup));
  ev_cleanup_init(cleanup_watcher, pyerr_set_interrupt);
  ev_cleanup_start(mainloop, cleanup_watcher);

  ev_io_stop(mainloop, &((ThreadInfo*)ev_userdata(mainloop))->accept_watcher);
  ev_signal_stop(mainloop, watcher);
}
#endif

static void
ev_io_on_request(struct ev_loop* mainloop, ev_io* watcher, const int events) {
    int client_fd;
    struct sockaddr_in sock_address;
    socklen_t address_len;

    address_len = sizeof(struct sockaddr_in);
    client_fd = accept(watcher->fd, (struct sockaddr*)&sock_address, &address_len);
    if(client_fd < 0) {
        DBG("Could not accept() client: errno %d", errno);
        return;
    }

    int flags = fcntl(client_fd, F_GETFL, 0);
    if(fcntl(client_fd, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK) == -1) {
        DBG("Could not set_nonblocking() client %d: errno %d", client_fd, errno);
        return;
    }

    Request* request = Request_new(
            ((ThreadInfo*)ev_userdata(mainloop))->server_info,
            client_fd,
            inet_ntoa(sock_address.sin_addr)
    );

    DBG_REQ(request, "Accepted client %s:%d on fd %d",
            inet_ntoa(sock_address.sin_addr), ntohs(sock_address.sin_port), client_fd);

    ev_io_init(&request->ev_watcher, &ev_io_on_read,
               client_fd, EV_READ);
    ev_io_start(mainloop, &request->ev_watcher);
}

static void
ev_io_on_read(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    static char read_buf[BUFFER_SIZE];

    Request* request = REQUEST_FROM_WATCHER(watcher);
    read_state read_state;

    ssize_t read_bytes = read(
            request->client_fd,
            read_buf,
            BUFFER_SIZE
    );

    GIL_LOCK(0);

    if (read_bytes == 0) {
        /* Client disconnected */
        read_state = aborted;
        DBG_REQ(request, "Client disconnected");
    }
    else if (read_bytes < 0) {
        /* Would block or error */
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            read_state = not_yet_done;
        }
        else {
            read_state = aborted;
            DBG_REQ(request, "Hit errno %d while reading", errno);
        }
    }
    else {
        /* OK, either expect more data or done reading */
        Request_decode(request, read_buf, (size_t)read_bytes);
        if(request->state.error_code) {
            read_state = done;
            DBG_REQ(request, "encode error");
            // todo 根据错误码生成报错
        }
        else if(request->state.parse_finished) {
            read_state = done;
            // todo 调用dispatcher中对应的方法
        }
        else {
            /* Wait for more data */
            read_state = not_yet_done;
        }
    }

    switch (read_state) {
        case not_yet_done:
            break;
        case done:
            DBG_REQ(request, "Stop read watcher, start write watcher");
            ev_io_stop(mainloop, &request->ev_watcher);
            ev_io_init(&request->ev_watcher, &ev_io_on_write,
                       request->client_fd, EV_WRITE);
            ev_io_start(mainloop, &request->ev_watcher);
            break;
        case aborted:
            close_connection(mainloop, request);
            break;
    }

    GIL_UNLOCK(0);
}

static void
ev_io_on_write(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    Request* request = REQUEST_FROM_WATCHER(watcher);
    DBG_REQ(request, "ev_io_on_write called");
}

static bool
handle_nonzero_errno(Request* request)
{
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
        /* Try again later */
        return true;
    }
    else {
        /* Serious transmission failure. Hang up. */
        fprintf(stderr, "Client %d hit errno %d\n", request->client_fd, errno);
        request->state.keep_alive = false;
        return false;
    }
}

static void
close_connection(struct ev_loop *mainloop, Request* request)
{
    DBG_REQ(request, "Closing socket");
    ev_io_stop(mainloop, &request->ev_watcher);
    close(request->client_fd);
    Request_free(request);
}
