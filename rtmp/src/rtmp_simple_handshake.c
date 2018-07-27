#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <time.h>
#include "rtmp_error.h"
#include "rtmp_string.h"
#include "rtmp_mbuffer.h"
#include "rtmp_utils.h"
#include "rtmp_socket.h"
#include "rtmp_simple_handshake.h"


struct SrsHandshakeBytes*  srs_handshakebytes_init(){

     struct SrsHandshakeBytes* srshandshakebytes = (struct SrsHandshakeBytes*)malloc(sizeof(struct SrsHandshakeBytes));
     memset(srshandshakebytes,0,sizeof(struct SrsHandshakeBytes));
     srshandshakebytes->c0c1=NULL;
     srshandshakebytes->s0s1s2=NULL;
     srshandshakebytes->c2=NULL;
     return srshandshakebytes;
}
 
void srs_handshakebytes_uinit(struct SrsHandshakeBytes* srshandshakebytes)
{
    srs_freep(srshandshakebytes->c0c1);
    srs_freep(srshandshakebytes->s0s1s2);
    srs_freep(srshandshakebytes->c2);
    srs_freep(srshandshakebytes);
}


static int read_c0c1(struct SrsHandshakeBytes* srshandshakebytes, struct  SimpleSocketStream* simplesocketstream){

    int ret = ERROR_SUCCESS;   
    if (srshandshakebytes->c0c1) {
        return ret;
    }
    ssize_t nsize;    
    srshandshakebytes->c0c1=(char*)malloc(1537);
    memset(srshandshakebytes->c0c1,0,1537);
    if ((ret = srs_simple_socket_read_fully(simplesocketstream,srshandshakebytes->c0c1, 1537, &nsize)) != ERROR_SUCCESS){
        srs_warn("read c0c1 failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("read c0c1 success.\n");   
    return ret;
}

static int read_s0s1s2(struct SrsHandshakeBytes* srshandshakebytes, struct  SimpleSocketStream* simplesocketstream){

    int ret = ERROR_SUCCESS;
    if (srshandshakebytes->s0s1s2) {
        return ret;
    }
    ssize_t nsize;
    srshandshakebytes->s0s1s2=(char*)malloc(3073);
    memset(srshandshakebytes->s0s1s2,0,3073);
    if ((ret = srs_simple_socket_read_fully(simplesocketstream,srshandshakebytes->s0s1s2, 3073, &nsize)) != ERROR_SUCCESS){
        srs_warn("read s0s1s2 failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("read s0s1s2 success.\n"); 
    return ret;
}

static int read_c2(struct SrsHandshakeBytes* srshandshakebytes,struct  SimpleSocketStream* simplesocketstream){

    int ret = ERROR_SUCCESS;    
    if (srshandshakebytes->c2) {
        return ret;
    }
    ssize_t nsize;    
    srshandshakebytes->c2 = (char*)malloc(1536);
    memset(srshandshakebytes->c2,0,1536);
    if ((ret = srs_simple_socket_read_fully(simplesocketstream,srshandshakebytes->c2, 1536, &nsize)) != ERROR_SUCCESS){
        srs_warn("read c2 failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("read c2 success.\n");
    return ret;
}

static void srs_random_generate(char* bytes, int size){

    static bool _random_initialized = false;
    int i;
    if (!_random_initialized) {
        srand(0);
        _random_initialized = true;
        srs_trace("srand initialized the random.\n");
    }
    for (i = 0; i < size; i++) {
        // the common value in [0x0f, 0xf0]
        bytes[i] = 0x0f + (rand() % (256 - 0x0f - 0x0f));
    }
}


static int create_c0c1(struct SrsHandshakeBytes* srshandshakebytes){

    int ret = ERROR_SUCCESS;
    struct timeval tv;

    if (srshandshakebytes->c0c1) {
        return ret;
    }
    srshandshakebytes->c0c1 =(char*)malloc(1537);
    srs_random_generate(srshandshakebytes->c0c1, 1537);
    // plain text required.
    struct SrsBuffer stream;
    if ((ret = srs_buffer_initialize(&stream,srshandshakebytes->c0c1, 9)) != ERROR_SUCCESS){
        return ret;
    }
    srs_buffer_write_1bytes(&stream,0x03);
    gettimeofday(&tv,NULL);
    srs_buffer_write_4bytes(&stream,(int32_t)(tv.tv_sec));
    srs_buffer_write_4bytes(&stream,0x00);
    return ret;
}


static int create_s0s1s2(struct SrsHandshakeBytes* srshandshakebytes,const char* c1){

    int ret = ERROR_SUCCESS; 
    struct timeval tv;
    if (srshandshakebytes->s0s1s2) {
        return ret;
    }    
    srshandshakebytes->s0s1s2 =(char*)malloc(3073);
    memset(srshandshakebytes->s0s1s2,0,3073);
    srs_random_generate(srshandshakebytes->s0s1s2, 3073);  
    // plain text required.
    struct SrsBuffer stream;
    if ((ret = srs_buffer_initialize(&stream,srshandshakebytes->s0s1s2, 9)) != ERROR_SUCCESS){
        return ret;
    }
    srs_buffer_write_1bytes(&stream,0x03);
    gettimeofday(&tv,NULL);
    srs_buffer_write_4bytes(&stream,(int32_t)(tv.tv_sec));
    // s1 time2 copy from c1
    if (srshandshakebytes->c0c1) {
        srs_buffer_write_bytes(&stream,srshandshakebytes->c0c1 + 1, 4);
    }
    // if c1 specified, copy c1 to s2.
    // @see: https://github.com/ossrs/srs/issues/46
    if (c1) {
        memcpy(srshandshakebytes->s0s1s2 + 1537, c1, 1536);
    }
    return ret;
}

static int create_c2(struct SrsHandshakeBytes* srshandshakebytes){

    int ret = ERROR_SUCCESS; 
    struct timeval tv;
    if (srshandshakebytes->c2) {
        return ret;
    }
    srshandshakebytes->c2 =(char*)malloc(1536);
    memset(srshandshakebytes->c2,0,1536);
    srs_random_generate(srshandshakebytes->c2, 1536);
    // time
    struct SrsBuffer stream;
    if ((ret = srs_buffer_initialize(&stream,srshandshakebytes->c2, 8)) != ERROR_SUCCESS){
        return ret;
    }
    gettimeofday(&tv,NULL);
    srs_buffer_write_4bytes(&stream,(int32_t)(tv.tv_sec));
    // c2 time2 copy from s1
    if (srshandshakebytes->s0s1s2) {
        srs_buffer_write_bytes(&stream,srshandshakebytes->s0s1s2 + 1, 4);
    } 
    return ret;
}


int srs_simple_handshake_with_server(struct SrsHandshakeBytes* hs_bytes, struct  SimpleSocketStream* simplesocketstream){

    int ret = ERROR_SUCCESS;   
    ssize_t nsize;
    // simple handshake
    if ((ret = create_c0c1(hs_bytes)) != ERROR_SUCCESS) {
        return ret;
    }
    if ((ret = srs_simple_socket_write(simplesocketstream,hs_bytes->c0c1,1537,&nsize)) != ERROR_SUCCESS){
        srs_warn("write c0c1 failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("write c0c1 success.\n");
    if ((ret = read_s0s1s2(hs_bytes,simplesocketstream)) != ERROR_SUCCESS) {
        return ret;
    }
    // plain text required.
    if (hs_bytes->s0s1s2[0] != 0x03) {
        ret = ERROR_RTMP_HANDSHAKE;
        srs_warn("handshake failed, plain text required. ret=%d\n", ret);
        return ret;
    }
    
    if ((ret = create_c2(hs_bytes)) != ERROR_SUCCESS) {
        return ret;
    }  
    // for simple handshake, copy s1 to c2.
    // @see https://github.com/ossrs/srs/issues/418
    memcpy(hs_bytes->c2, hs_bytes->s0s1s2 + 1, 1536);  
    if ((ret = srs_simple_socket_write(simplesocketstream,hs_bytes->c2, 1536, &nsize)) != ERROR_SUCCESS){
        srs_warn("simple handshake write c2 failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("simple handshake write c2 success.\n");    
    srs_trace("simple handshake success.\n");  
    return ret;
}


