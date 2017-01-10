#ifndef HYDRA_PARSER_H
#define HYDRA_PARSER_H

#include <Python.h>

typedef struct buffer_node {
    char* data;
    unsigned size;
    struct buffer_node* next;
} buffer_node;

typedef struct {
    buffer_node* head;
    buffer_node* tail;
    unsigned int buffer_sum;

    void* data;
} thrift_parser;

unsigned binary_decode(thrift_parser* parser, char* buffer, unsigned size);

#endif //HYDRA_PARSER_H
