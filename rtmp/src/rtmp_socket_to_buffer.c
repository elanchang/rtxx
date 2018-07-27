#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include "rtmp_error.h"
#include "rtmp_string.h"
#include "rtmp_mbuffer.h"
#include "rtmp_utils.h"
#include "rtmp_socket.h"
#include "rtmp_socket_to_buffer.h"


// the default recv buffer size, 128KB.
//#define SRS_DEFAULT_RECV_BUFFER_SIZE 131072
// limit user-space buffer to 256KB, for 3Mbps stream delivery.
//      800*2000/8=200000B(about 195KB).
// @remark it's ok for higher stream, the buffer is ok for one chunk is 256KB.
//#define SRS_MAX_SOCKET_BUFFER 262144

#define SRS_DEFAULT_RECV_BUFFER_SIZE 1310720

#define SRS_MAX_SOCKET_BUFFER 2621440

struct SrsFastStream* srs_fast_stream_init(){

   struct SrsFastStream* srsfaststream = (struct SrsFastStream *)malloc(sizeof(struct SrsFastStream));
   memset(srsfaststream,0,sizeof(struct SrsFastStream));
#ifdef SRS_PERF_MERGED_READ
    srsfaststream->merged_read = false;
    srsfaststream->_handler = NULL;
#endif 
    srsfaststream->nb_buffer = SRS_DEFAULT_RECV_BUFFER_SIZE;
    srsfaststream->buffer = (char*)malloc(srsfaststream->nb_buffer);
    srsfaststream->p=srsfaststream->buffer;
    srsfaststream->end= srsfaststream->buffer;
    return srsfaststream;
}

void srs_fast_stream_uinit(struct SrsFastStream* srsfaststream){

    srs_freep(srsfaststream->buffer);
    srsfaststream->buffer = NULL;
    srs_freep(srsfaststream);
}

int srs_fast_stream_size(struct SrsFastStream* srsfaststream){

    return (int)((srsfaststream->end)-(srsfaststream->p));
}

char* srs_fast_stream_bytes(struct SrsFastStream* srsfaststream){

    return srsfaststream->p;
}


void srs_fast_stream_set_buffer(struct SrsFastStream* srsfaststream,int buffer_size){

    // never exceed the max size.
    if (buffer_size > SRS_MAX_SOCKET_BUFFER) {
        srs_warn("limit the user-space buffer from %d to %d\n", 
            buffer_size, SRS_MAX_SOCKET_BUFFER);
    }  
    // the user-space buffer size limit to a max value.
    int nb_resize_buf = srs_min(buffer_size, SRS_MAX_SOCKET_BUFFER);
    // only realloc when buffer changed bigger
    if (nb_resize_buf <= srsfaststream->nb_buffer) {
        return;
    } 
    // realloc for buffer change bigger.
    int start = (int)((srsfaststream->p) - (srsfaststream->buffer));
    int nb_bytes = (int)((srsfaststream->end) - (srsfaststream->p));
    
    srsfaststream->buffer = (char*)realloc(srsfaststream->buffer, nb_resize_buf);
    srsfaststream->nb_buffer = nb_resize_buf;
    srsfaststream->p = srsfaststream->buffer + start;
    srsfaststream->end = srsfaststream->p + nb_bytes;
}

char srs_fast_stream_read_1byte(struct SrsFastStream* srsfaststream){

    srs_assert((srsfaststream->end)-(srsfaststream->p) >= 1);
    return *(srsfaststream->p)++;
}

char* srs_fast_stream_read_slice(struct SrsFastStream* srsfaststream,int size){

    srs_assert(size >= 0);
    srs_assert((srsfaststream->end) -(srsfaststream->p) >= size);
    srs_assert((srsfaststream->p) + size >= (srsfaststream->buffer));  
    char* ptr = srsfaststream->p;
    (srsfaststream->p) += size;
    return ptr;
}

void srs_fast_stream_skip(struct SrsFastStream* srsfaststream,int size){

    srs_assert((srsfaststream->end)-(srsfaststream->p) >= size);
    srs_assert((srsfaststream->p) + size >= (srsfaststream->buffer));
    (srsfaststream->p) += size;
}


int srs_fast_stream_grow(struct SrsFastStream* srsfaststream,struct SimpleSocketStream* simplesocketstream, int required_size){

    int ret = ERROR_SUCCESS;
    // already got required size of bytes.
    if ((srsfaststream->end) -(srsfaststream->p) >= required_size) {
        return ret;
    }
    // must be positive.
    srs_assert(required_size > 0);

    // the free space of buffer, 
    //      buffer = consumed_bytes + exists_bytes + free_space.
    int nb_free_space = (int)((srsfaststream->buffer) +(srsfaststream->nb_buffer)-(srsfaststream->end));
    
    // the bytes already in buffer
    int nb_exists_bytes = (int)((srsfaststream->end) -(srsfaststream->p));
    srs_assert(nb_exists_bytes >= 0);
    
    // resize the space when no left space.
    if (nb_free_space < required_size - nb_exists_bytes) {
        srs_verbose("move fast buffer %d bytes", nb_exists_bytes);

        // reset or move to get more space.
        if (!nb_exists_bytes) {
            // reset when buffer is empty.
            (srsfaststream->p) =(srsfaststream->end) =(srsfaststream->buffer);
            srs_verbose("all consumed, reset fast buffer");
        } else {
            // move the left bytes to start of buffer.
            srs_assert(nb_exists_bytes < (srsfaststream->nb_buffer));
            srsfaststream->buffer = (char*)memmove(srsfaststream->buffer,srsfaststream->p, nb_exists_bytes);
            srsfaststream->p = srsfaststream->buffer;
            srsfaststream->end = (srsfaststream->p) + nb_exists_bytes;
        }
        
        // check whether enough free space in buffer.
        nb_free_space = (int)((srsfaststream->buffer) + (srsfaststream->nb_buffer) - (srsfaststream->end));
        if (nb_free_space < required_size - nb_exists_bytes) {
            ret = ERROR_READER_BUFFER_OVERFLOW;
            srs_error("buffer overflow, required=%d, max=%d, left=%d, ret=%d", 
                required_size, srsfaststream->nb_buffer, nb_free_space, ret);
            return ret;
        }
    }
    // buffer is ok, read required size of bytes.
    while (((srsfaststream->end) -(srsfaststream->p)) < required_size) {
        ssize_t nread;
        if ((ret = srs_simple_socket_read(simplesocketstream,(srsfaststream->end), nb_free_space, &nread)) != ERROR_SUCCESS) {
            return ret;
        }       
#ifdef SRS_PERF_MERGED_READ
        /**
        * to improve read performance, merge some packets then read,
        * when it on and read small bytes, we sleep to wait more data.,
        * that is, we merge some data to read together.
        * @see https://github.com/ossrs/srs/issues/241
        */
        if (srsfaststream->merged_read && srsfaststream->_handler) {
            srsfaststream->_handler->on_read(nread);
        }
#endif     
        // we just move the ptr to next.
        srs_assert((int)nread > 0);
        (srsfaststream->end) += nread;
        nb_free_space -= nread;
    }   
    return ret;
}


#ifdef SRS_PERF_MERGED_READ
void srs_fast_stream_set_merge_read(struct SrsFastStream* srsfaststream,bool v, IMergeReadHandler* handler){

    srsfaststream->merged_read = v;
    srsfaststream->_handler = handler;
}
#endif

