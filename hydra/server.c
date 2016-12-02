#include <arpa/inet.h>
#include <fcntl.h>
#include <ev.h>

#if defined __FreeBSD__
# include <netinet/in.h> /* for struct sockaddr_in */
# include <sys/types.h>
# include <sys/socket.h>
#endif

#ifdef WANT_SIGINT_HANDLING
# include <sys/signal.h>
#endif

#include "common.h"
#include "server.h"

#define BUFFER_SIZE 4*1024
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


void server_run(ServerInfo* server_info)
{
  struct ev_loop* mainloop = ev_loop_new(0);

  ThreadInfo thread_info;
  thread_info.server_info = server_info;
  ev_set_userdata(mainloop, &thread_info);

  ev_io_init(&thread_info.accept_watcher, ev_io_on_request, server_info->sockfd, EV_READ);
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
pyerr_set_interrupt(struct ev_loop* mainloop, struct ev_cleanup* watcher, const int events)
{
  PyErr_SetInterrupt();
  free(watcher);
}

static void
ev_signal_on_sigint(struct ev_loop* mainloop, ev_signal* watcher, const int events)
{
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
ev_io_on_request(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
  int client_fd;
  struct sockaddr_in sockaddr;
  socklen_t addrlen;

  addrlen = sizeof(struct sockaddr_in);
  client_fd = accept(watcher->fd, (struct sockaddr*)&sockaddr, &addrlen);
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
    inet_ntoa(sockaddr.sin_addr)
  );

  DBG_REQ(request, "Accepted client %s:%d on fd %d",
          inet_ntoa(sockaddr.sin_addr), ntohs(sockaddr.sin_port), client_fd);

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
  } else if (read_bytes < 0) {
    /* Would block or error */
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
      read_state = not_yet_done;
    } else {
      read_state = aborted;
      DBG_REQ(request, "Hit errno %d while reading", errno);
    }
  } else {
    /* OK, either expect more data or done reading */
    Request_decode(request, read_buf, (size_t)read_bytes);
    if(request->state.error_code) {
      read_state = done;
      DBG_REQ(request, "encode error");
      //todo 生成一个encode error异常返回给客户端
//      request->current_chunk = PyString_FromString(
//        http_error_messages[request->state.error_code]);
//      assert(request->iterator == NULL);
    } else if(request->state.parse_finished) {
      read_state = done;
      //todo 调用thrift_call_application, 判断是否发生异常并写入结果
//      bool call_ok = thrift_call_application(request);
//      if (!call_ok) {
//        DBG_REQ(request, "thrift app error");
//        assert(PyErr_Occurred());
//        PyErr_Print();
//        assert(!request->state.chunked_response);
//        Py_XCLEAR(request->iterator);
//        request->current_chunk = PyString_FromString(
//          http_error_messages[HTTP_SERVER_ERROR]);
//      }
    } else {
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
//  static char write_buf[BUFFER_SIZE];
  Request* request = REQUEST_FROM_WATCHER(watcher);
  DBG_REQ(request, "ev_io_on_write called");
//
//  write_state write_state;
//  ssize_t bytes_sent = write(
//    request->client_fd,
//    read_buf,
//    BUFFER_SIZE
//  );
//
//  GIL_LOCK(0);
//
//  if (bytes_sent == 0) {
//    /* Client disconnected */
//    write_state = aborted;
//    DBG_REQ(request, "Client disconnected");
//  } else if (bytes_sent < 0) {
//    /* Would block or error */
//    if(errno == EAGAIN || errno == EWOULDBLOCK) {
//      write_state = not_yet_done;
//    } else {
//      write_state = aborted;
//      DBG_REQ(request, "Hit errno %d while writing", errno);
//    }
//  } else {
//    Request_encode(request, read_buf, (size_t)read_bytes);
//    if(request->state.encode_error) {
//      write_state = aborted;
//      DBG_REQ(request, "encode error");
//    } else if(request->state.encode_finished) {
//      write_state = done;
//    } else {
//      /* Wait for more data */
//      write_state = not_yet_done;
//    }
//  }
//
//  switch(write_state) {
//  case not_yet_done:
//    break;
//  case done:
//    DBG_REQ(request, "done, close");
//    close_connection(mainloop, request);
//    break;
//  case aborted:
//    /* Response was aborted due to an error. We can't do anything graceful here
//     * because at least one chunk is already sent... just close the connection. */
//    close_connection(mainloop, request);
//    break;
//  }
//
//  GIL_UNLOCK(0);
}

static void
close_connection(struct ev_loop *mainloop, Request* request)
{
  DBG_REQ(request, "Closing socket");
  ev_io_stop(mainloop, &request->ev_watcher);
  close(request->client_fd);
  Request_free(request);
}
