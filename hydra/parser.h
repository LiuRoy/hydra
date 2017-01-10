#ifndef HYDRA_PARSER_H
#define HYDRA_PARSER_H

#include <Python.h>

typedef struct buffer_node {
    char* data;
    unsigned size;
    struct buffer_node* next;
} buffer_node;

typedef struct {
    struct buffer_node* head;
    struct buffer_node* tail;
    unsigned int buffer_sum;

    unsigned int status: 8;
    unsigned int error_code: 24;

    unsigned int sequence_id;
    PyObject* thrift_spec;
    PyObject* api_name;
    PyObject* api_args;
    PyObject* api_result;
} thrift_parser;

unsigned binary_decode(thrift_parser* parser, char* buffer, unsigned size);

#endif //HYDRA_PARSER_H
