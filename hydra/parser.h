#ifndef __request_h__
#define __request_h__

struct buffer_node {
    char* data;
    unsigned size;
    buffer_node* next;
};

typedef struct {
    buffer_node* head;
    buffer_node* tail;
    unsigned buffer_sum;

    void* data；
} thrift_parser;

unsigned binary_decode(thrift_parser* parser, char* buffer, unsigned size);

#endif