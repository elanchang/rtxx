#include <FreeRTOS.h>
#include <bsp.h>
#include <task.h>
#include <queue.h>
#include <nonstdlib.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include "rtmp_error.h"
#include "rtmp_string.h"
#include "rtmp_mbuffer.h"
#include "rtmp_utils.h"

bool srs_is_little_endian(){
    // convert to network(big-endian) order, if not equals, 
    // the system is little-endian, so need to convert the int64
    static int little_endian_check = -1;
    
    if(little_endian_check == -1) {
        union {
            int32_t i;
            int8_t c;
        } little_check_union;
        
        little_check_union.i = 0x01;
        little_endian_check = little_check_union.c;
    }
    return (little_endian_check == 1);
}

void srs_buffer_set_value(struct SrsBuffer* srcbuffer,char* b, int nb_b){

    srcbuffer->bytes=b;
    srcbuffer->p=b;
    srcbuffer->nb_bytes = nb_b;
    
    // TODO: support both little and big endian.
    srs_assert(srs_is_little_endian());
}


void srs_buffer_init(struct SrsBuffer* srcbuffer){

    srs_buffer_set_value(srcbuffer,NULL, 0);
}

void srs_buffer_init_default(struct SrsBuffer* srcbuffer,char* b, int nb_b){

    srs_buffer_set_value(srcbuffer,b, nb_b);
}

void srs_buffer_uinit(struct SrsBuffer* srcbuffer){
}


int srs_buffer_initialize(struct SrsBuffer* srcbuffer,char* b, int nb){
    int ret = ERROR_SUCCESS;
    if (!b) {
        ret = ERROR_KERNEL_STREAM_INIT;
        srs_error("stream param bytes must not be NULL.ret=%d\n", ret);
        return ret;
    }
    
    if (nb <= 0) {
        ret = ERROR_KERNEL_STREAM_INIT;
        srs_error("stream param size must be positive. ret=%d", ret);
        return ret;
    }
    srcbuffer->nb_bytes = nb;
    srcbuffer->p=b;
    srcbuffer->bytes=b;
    return ret;
}

char* srs_buffer_data(struct SrsBuffer* srcbuffer){

    return srcbuffer->bytes;
}

int srs_buffer_size(struct SrsBuffer* srcbuffer){

    return srcbuffer->nb_bytes;
}

int srs_buffer_pos(struct SrsBuffer* srcbuffer){

    return (int)((srcbuffer->p)-(srcbuffer->bytes));
}

bool srs_buffer_empty(struct SrsBuffer* srcbuffer){

    return !(srcbuffer->bytes) || ((srcbuffer->p) >= (srcbuffer->bytes) + (srcbuffer->nb_bytes));
}

bool srs_buffer_require(struct SrsBuffer* srcbuffer,int required_size){

    srs_assert(required_size >= 0);    
    return required_size <= (srcbuffer->nb_bytes) - ((srcbuffer->p) -(srcbuffer->bytes));
}

void srs_buffer_skip(struct SrsBuffer* srcbuffer, int size){

    srs_assert(srcbuffer->p);
    (srcbuffer->p) += size;
}

int8_t srs_buffer_read_1bytes(struct SrsBuffer* srcbuffer){

    srs_assert(srs_buffer_require(srcbuffer,1));    
    return (int8_t)*(srcbuffer->p)++;
}

int16_t srs_buffer_read_2bytes(struct SrsBuffer* srcbuffer){

    srs_assert(srs_buffer_require(srcbuffer,2));    
    int16_t value;
    char* pp = (char*)&value;
    pp[1] = *(srcbuffer->p)++;
    pp[0] = *(srcbuffer->p)++;   
    return value;
}

int32_t srs_buffer_read_3bytes(struct SrsBuffer* srcbuffer){

    srs_assert(srs_buffer_require(srcbuffer,3));    
    int32_t value = 0x00;
    char* pp = (char*)&value;
    pp[2] = *(srcbuffer->p)++;
    pp[1] = *(srcbuffer->p)++;
    pp[0] = *(srcbuffer->p)++;   
    return value;
}

int32_t srs_buffer_read_4bytes(struct SrsBuffer* srcbuffer){

    srs_assert(srs_buffer_require(srcbuffer,4));    
    int32_t value;
    char* pp = (char*)&value;
    pp[3] = *(srcbuffer->p)++;
    pp[2] = *(srcbuffer->p)++;
    pp[1] = *(srcbuffer->p)++;
    pp[0] = *(srcbuffer->p)++;    
    return value;
}

int64_t srs_buffer_read_8bytes(struct SrsBuffer* srcbuffer){

    srs_assert(srs_buffer_require(srcbuffer,8));   
    int64_t value;
    char* pp = (char*)&value;
    pp[7] = *(srcbuffer->p)++;
    pp[6] = *(srcbuffer->p)++;
    pp[5] = *(srcbuffer->p)++;
    pp[4] = *(srcbuffer->p)++;
    pp[3] = *(srcbuffer->p)++;
    pp[2] = *(srcbuffer->p)++;
    pp[1] = *(srcbuffer->p)++;
    pp[0] = *(srcbuffer->p)++;  
    return value;
}

int srs_buffer_read_string(struct SrsBuffer* srcbuffer,int len,struct AVal* value){

    srs_assert(srs_buffer_require(srcbuffer,len));
    char data[256]={0};
    memcpy(data, srcbuffer->p, len);  
    value->av_val=strdup(data);
    value->av_len=len;
    srcbuffer->p += len;
    return 0;
}

void srs_buffer_read_bytes(struct SrsBuffer* srcbuffer,char* data, int size){

    srs_assert(srs_buffer_require(srcbuffer,size));    
    memcpy(data, srcbuffer->p, size);    
    (srcbuffer->p) += size;
}

void srs_buffer_write_1bytes(struct SrsBuffer* srcbuffer,int8_t value){

    srs_assert(srs_buffer_require(srcbuffer,1));    
    *(srcbuffer->p)++ = value;
}

void srs_buffer_write_2bytes(struct SrsBuffer* srcbuffer,int16_t value){

    srs_assert(srs_buffer_require(srcbuffer,2));  
    char* pp = (char*)&value;
    *(srcbuffer->p)++ = pp[1];
    *(srcbuffer->p)++ = pp[0];
}

void srs_buffer_write_4bytes(struct SrsBuffer* srcbuffer,int32_t value){

    srs_assert(srs_buffer_require(srcbuffer,4));  
    char* pp = (char*)&value;
    *(srcbuffer->p)++ = pp[3];
    *(srcbuffer->p)++ = pp[2];
    *(srcbuffer->p)++ = pp[1];
    *(srcbuffer->p)++ = pp[0];
}

void srs_buffer_write_3bytes(struct SrsBuffer* srcbuffer,int32_t value){

    srs_assert(srs_buffer_require(srcbuffer,3));  
    char* pp = (char*)&value;
    *(srcbuffer->p)++ = pp[2];
    *(srcbuffer->p)++ = pp[1];
    *(srcbuffer->p)++ = pp[0];
}

void srs_buffer_write_8bytes(struct SrsBuffer* srcbuffer,int64_t value){

    srs_assert(srs_buffer_require(srcbuffer,8));
    char* pp = (char*)&value;
    *(srcbuffer->p)++ = pp[7];
    *(srcbuffer->p)++ = pp[6];
    *(srcbuffer->p)++ = pp[5];
    *(srcbuffer->p)++ = pp[4];
    *(srcbuffer->p)++ = pp[3];
    *(srcbuffer->p)++ = pp[2];
    *(srcbuffer->p)++ = pp[1];
    *(srcbuffer->p)++ = pp[0];
}

void srs_buffer_write_string(struct SrsBuffer* srcbuffer,struct AVal value)
{
    srs_assert(srs_buffer_require(srcbuffer,(int)value.av_len));
    memcpy(srcbuffer->p, value.av_val, value.av_len);
    srcbuffer->p += value.av_len;
}

void srs_buffer_write_bytes(struct SrsBuffer* srcbuffer,char* data, int size){

    srs_assert(srs_buffer_require(srcbuffer,size)); 
    memcpy(srcbuffer->p, data, size);
    (srcbuffer->p) += size;
}

