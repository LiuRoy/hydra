# -*- coding=utf8 -*-
"""thrift server"""

import os
import socket
from _hydra import (
    run_server,
    ThriftApplication
)

LISTEN_BACKLOG = 1024


class ThriftServer(object):

    """thrift server

    Attributes:
        app (ThriftApplication): thrift应用
        sock (socket)
    """
    def __init__(self, service, handler):
        self.sock = None
        self.app = ThriftApplication(service, handler)

    def bind_and_listen(self, host, port, reuse_port):
        if host.startswith("unix:@"):
            # Abstract UNIX socket: "unix:@foobar"
            self.sock = socket.socket(socket.AF_UNIX)
            self.sock.bind('\0' + host[6:])
        elif host.startswith("unix:"):
            # UNIX socket: "unix:/tmp/foobar.sock"
            self.sock = socket.socket(socket.AF_UNIX)
            self.sock.bind(host[5:])
        else:
            # IP socket
            self.sock = socket.socket(socket.AF_INET)
            # Set SO_REUSEADDR to make the IP address available for reuse
            self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
            if reuse_port:
                self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEPORT, 1)
            self.sock.bind((host, port))

        self.sock.listen(LISTEN_BACKLOG)

    def serve(self):
        try:
            server_run(self.sock, self.app)
        finally:
            if self.sock.family == socket.AF_UNIX:
                filename = self.sock.getsockname()
                if filename[0] != '\0':
                    os.unlink(self.sock.getsockname())
            self.sock.close()


def maker_server(service, handler, host, port, reuse_port=False):
    """生成thrift server

    Args:
        service: thrift service
        handler: thrift handler
        host (basestring): 服务地址
        port (int): 服务端口
        reuse_port (bool): 是否多个进程监听同一端口

    Returns:
        ThriftServer
    """
    server = ThriftServer(service, handler)
    server.bind_and_listen(host, port, reuse_port)
    return server
