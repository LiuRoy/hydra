#include <Python.h>
#include "server.h"


static PyObject*
run(PyObject* self, PyObject* args)
{
  ServerInfo info;

  PyObject* socket;

  if(!PyArg_ParseTuple(args, "OOO:server_run", &socket, &info.service, &info.handler)) {
    return NULL;
  }

  info.sockfd = PyObject_AsFileDescriptor(socket);
  if (info.sockfd < 0) {
    return NULL;
  }

  info.host = NULL;
  if (PyObject_HasAttrString(socket, "getsockname")) {
    PyObject* sockname = PyObject_CallMethod(socket, "getsockname", NULL);
    if (sockname == NULL) {
      return NULL;
    }
    if (PyTuple_CheckExact(sockname) && PyTuple_GET_SIZE(sockname) == 2) {
      /* Standard (ipaddress, port) case */
      info.host = PyTuple_GET_ITEM(sockname, 0);
      info.port = PyTuple_GET_ITEM(sockname, 1);
    }
  }

  //_initialize_request_module();
  server_run(&info);

  Py_RETURN_NONE;
}

static PyMethodDef Hydra_FunctionTable[] = {
  {"server_run", (PyCFunction) run, METH_VARARGS, "run thrift server"},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC init_hydra(void)
{
  //_init_common();
  //_init_filewrapper();

  //PyType_Ready(&FileWrapper_Type);
  //assert(FileWrapper_Type.tp_flags & Py_TPFLAGS_READY);
  //PyType_Ready(&StartResponse_Type);
  //assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);

  PyObject* hydra_module = Py_InitModule("_hydra", Hydra_FunctionTable);
  PyModule_AddObject(hydra_module, "version", Py_BuildValue("(iii)", 0, 1, 0));
}
