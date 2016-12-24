#include "parser.h"
#include <stdint.h>

#include <stdbool.h>
#include <netinet/in.h>

/* Fix endianness issues on Solaris */
#if defined (__SVR4) && defined (__sun)
#if defined(__i386) && !defined(__i386__)
  #define __i386__
 #endif

 #ifndef BIG_ENDIAN
  #define BIG_ENDIAN (4321)
 #endif
 #ifndef LITTLE_ENDIAN
  #define LITTLE_ENDIAN (1234)
 #endif

 /* I386 is LE, even on Solaris */
 #if !defined(BYTE_ORDER) && defined(__i386__)
  #define BYTE_ORDER LITTLE_ENDIAN
 #endif
#endif

typedef enum TType {
    T_STOP       = 0,
    T_VOID       = 1,
    T_BOOL       = 2,
    T_BYTE       = 3,
    T_I08        = 3,
    T_I16        = 6,
    T_I32        = 8,
    T_U64        = 9,
    T_I64        = 10,
    T_DOUBLE     = 4,
    T_STRING     = 11,
    T_UTF7       = 11,
    T_STRUCT     = 12,
    T_MAP        = 13,
    T_SET        = 14,
    T_LIST       = 15,
    T_UTF8       = 16,
    T_UTF16      = 17
} TType;

typedef enum TError {
    NO_ERROR    = 0,
    BAD_VERSION = 1
} TError;

typedef enum Status {
    P_BEGIN    = 0,
    P_PARSING  = 1,
    P_FINISH   = 2
};

#ifndef __BYTE_ORDER
# if defined(BYTE_ORDER) && defined(LITTLE_ENDIAN) && defined(BIG_ENDIAN)
#  define __BYTE_ORDER BYTE_ORDER
#  define __LITTLE_ENDIAN LITTLE_ENDIAN
#  define __BIG_ENDIAN BIG_ENDIAN
# else
#  error "Cannot determine endianness"
# endif
#endif

#if __BYTE_ORDER == __BIG_ENDIAN
# define ntohll(n) (n)
# define htonll(n) (n)
#elif __BYTE_ORDER == __LITTLE_ENDIAN
# if defined(__GNUC__) && defined(__GLIBC__)
#  include <byteswap.h>
#  define ntohll(n) bswap_64(n)
#  define htonll(n) bswap_64(n)
# else /* GNUC & GLIBC */
#  define ntohll(n) ( (((unsigned long long)ntohl(n)) << 32) + ntohl(n >> 32) )
#  define htonll(n) ( (((unsigned long long)htonl(n)) << 32) + htonl(n >> 32) )
# endif /* GNUC & GLIBC */
#else /* __BYTE_ORDER */
# error "Can't define htonll or ntohll!"
#endif


#define INT_CONV_ERROR_OCCURRED(v) ( ((v) == -1) && PyErr_Occurred() )
#define CHECK_RANGE(v, min, max) ( ((v) <= (max)) && ((v) >= (min)) )
#define FREE_BUF(buf, bf_info, len) \
    do { \
        if (buf != bf_info->buffer - len) {\
             free(buf); \
        } \
   } while(0)

#define VERSION_MASK -65536
#define VERSION_1 -2147418112


typedef struct {
    thrift_parser* parser;
    char* buffer;
    unsigned  buffer_size;
    unsigned  parsed_len;
} buffer_info;


char* read_bytes(buffer_info* bf_info, unsigned len) {
    if (bf_info->parsed_len >= bf_info->buffer_size)
        return NULL;

    if (bf_info->parsed_len + len > bf_info->buffer_size) {
        buffer_node* new_node = malloc(sizeof(buffer_node));

        unsigned copy_size = bf_info->buffer_size - bf_info->parsed_len;
        new_node->data = malloc(copy_size);
        memcpy(bf_info->buffer, new_node->data, copy_size);
        new_node->next = NULL;

        if (bf_info->parser->head && bf_info->parser->tail) {
            bf_info->parser->tail->next = new_node;
            bf_info->parser->tail = new_node;
        }
        else {
            bf_info->parser->head = new_node;
            bf_info->parser->tail = new_node;
        }
        bf_info->parser->buffer_sum += copy_size;
        bf_info->buffer += copy_size;
        bf_info->parsed_len = bf_info->buffer_size;
        return NULL;
    }
    else {
        if (bf_info->parser->head && bf_info->parser->tail) {
            unsigned result_len = bf_info->parser->buffer_sum + len;
            char* result = malloc(result_len);

            buffer_node* iter = bf_info->parser->head;
            char* buffer_ptr = result;
            while(iter != NULL) {
                memcpy(buffer_ptr, iter->data, iter->size);
                buffer_ptr += iter->size;

                buffer_node* next = iter->next;
                free(iter->data);
                free(iter);
                iter = next;
            }
            memcpy(buffer_ptr, bf_info->buffer, len);
            bf_info->parser->head = bf_info->parser->tail = NULL;
            bf_info->parser->buffer_sum = 0;

            bf_info->buffer += len;
            bf_info->parsed_len += len;
            return result;
        }
        else {
            char* result = bf_info->buffer;
            bf_info->buffer += len;
            bf_info->parsed_len += len;
            return result;
        }
    }
}

static int8_t read_i8(buffer_info* bf_info) {
    unsigned len = sizeof(int8_t);
    char* buf = read_bytes(bf_info, len);
    if (buf == NULL) {
        return -1;
    }

    int8_t result = *(int8_t*) buf;
    FREE_BUF(buf, bf_info, len);
    return result;
}


static int16_t read_i16(buffer_info* bf_info) {
    unsigned len = sizeof(int16_t);
    char* buf = read_bytes(bf_info, len);
    if (buf == NULL) {
        return -1;
    }

    int16_t result = (int16_t) ntohs(*(int16_t*) buf);
    FREE_BUF(buf, bf_info, len);
    return result;
}

static int32_t read_i32(buffer_info* bf_info) {
    unsigned len = sizeof(int32_t);
    char* buf = read_bytes(bf_info, len);
    if (buf == NULL) {
        return -1;
    }
    int32_t result = (int32_t) ntohl(*(int32_t*) buf);
    FREE_BUF(buf, bf_info, len);
    return result;
}


static int64_t read_i64(buffer_info* bf_info) {
    unsigned len = sizeof(int64_t);
    char* buf = read_bytes(bf_info, len);
    if (buf == NULL) {
        return -1;
    }

    int64_t result = (int64_t) ntohll(*(int64_t*) buf);
    FREE_BUF(buf, bf_info, len);
    return result;
}

static double read_double(buffer_info* bf_info) {
    union {
        int64_t f;
        double t;
    } transfer;

    transfer.f = read_i64(bf_info);
    if (transfer.f == -1) {
        return -1;
    }
    return transfer.t;
}

// 默认这些信息可以放在一个buffer中
static bool read_message_begin(buffer_info* bf_info, bool strict) {
    int32_t sz = read_i32(bf_info);
    if (sz < 0) {
        int32_t version = sz & VERSION_MASK;
        if (version != VERSION_1) {
            bf_info->parser->error_code = BAD_VERSION;
            return false;
        }

        int32_t name_sz = read_i32(bf_info);
        char* name = read_bytes(bf_info, (unsigned)name_sz);
        bf_info->parser->api_name = PyString_FromStringAndSize(name, name_sz);
    }
    else {
        if (strict) {
            bf_info->parser->error_code = BAD_VERSION;
            return false;
        }
        char* name = read_bytes(bf_info, (unsigned)sz);
        bf_info->parser->api_name = PyString_FromStringAndSize(name, sz);
    }

    bf_info->parser->sequence_id = (unsigned)read_i32(bf_info);
    return true;
}

unsigned binary_decode(thrift_parser* parser,
                       char* buffer,
                       unsigned buffer_size) {
    buffer_info bf_info;
    bf_info.parser = parser;
    bf_info.buffer = buffer;
    bf_info.buffer_size = buffer_size;
    bf_info.parsed_len = 0;

    return 1;
}
