import thriftpy
from hydra import make_server


class Dispatcher(object):
    def ping(self):
        return "pong"


pingpong_thrift = thriftpy.load("pingpong.thrift", module_name="pingpong_thrift")
server = make_server(pingpong_thrift.PingPong, Dispatcher(), '127.0.0.1', 6000)
server.serve()
