#ifndef HYDRA_PARSER_H
#define HYDRA_PARSER_H

#include <Python.h>

typedef struct buffer_node {
    char* data;
    size_t size;
    struct buffer_node* next;
} buffer_node;

typedef struct {
    buffer_node* head;
    buffer_node* tail;
    size_t buffer_sum;

    void* data;
} thrift_parser;

void binary_decode(thrift_parser* parser, const char* buffer, const size_t size);

#endif //HYDRA_PARSER_H
