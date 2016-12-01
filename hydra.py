# -*- coding=utf8 -*-
"""thrift server"""

import os
import socket
import _hydra

_default_instance = None
LISTEN_BACKLOG = 1024

def maker_server(service, handler, host, port, reuse_port=False):
    """make a thrift server

    Args:
        service: thrift service interface
        handler: implementation
        host (unicode): binded host
        port (int): binded port
        reuse_port (bool): Enable "receive steering" on FreeBSD and Linux >=3.9.
            This allows multiple independent bjoerns to bind to the same port
            (and ideally also set their CPU affinity), resulting in more efficient
            load distribution
    """

