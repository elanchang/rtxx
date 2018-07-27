#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "rtmp_error.h"
#include "rtmp_string.h"
#include "rtmp_mbuffer.h"
#include "rtmp_utils.h"
#include "rtmp_amf.h"

int srs_amf0_write_utf8(struct SrsBuffer* stream, AVal value){

    int ret = ERROR_SUCCESS; 
    // len
    srs_buffer_write_2bytes(stream,value.av_len);
    srs_verbose("amf0 write string length success. len=%d\n", (int)value.av_len);
    
    // empty string
    if (value.av_len <= 0) {
        srs_verbose("amf0 write empty string. ret=%d\n", ret);
        return ret;
    } 
    // data
    if (!srs_buffer_require(stream,value.av_len)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
       srs_error("amf0 write string data failed. ret=%d\n", ret);
        return ret;
    }
    srs_buffer_write_string(stream,value);
    srs_verbose("amf0 write string data success. str=%s\n", value.av_val);
    
    return ret;
  
}


int srs_amf0_read_utf8(struct SrsBuffer* stream, struct AVal* value){

    int ret = ERROR_SUCCESS;   
    // len
    if (!srs_buffer_require(stream,2)) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 read string length failed. ret=%d\n", ret);
        return ret;
    }
    int16_t len = srs_buffer_read_2bytes(stream);
    srs_verbose("amf0 read string length success. len=%d\n", len);
   
    // empty string
    if (len <= 0) {
        srs_verbose("amf0 read empty string. ret=%d\n", ret);
        return ret;
    }
    // data
    if (!srs_buffer_require(stream,len)) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 read string data failed. ret=%d\n", ret);
        return ret;
    }  
    srs_buffer_read_string(stream,len,value);  
 
    return ret;
}


int srs_amf0_write_boolean(struct SrsBuffer* stream, bool value){

    int ret = ERROR_SUCCESS;
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write bool marker failed. ret=%d\n", ret);
        return ret;
    }
    srs_buffer_write_1bytes(stream,RTMP_AMF0_Boolean);
    srs_verbose("amf0 write bool marker success\n");

    // value
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write bool value failed. ret=%d\n", ret);
        return ret;
    }

    if (value) {
        srs_buffer_write_1bytes(stream,0x01);
    } else {
        srs_buffer_write_1bytes(stream,0x00);
    }
    
    srs_verbose("amf0 write bool value success. value=%d\n", value);
    
    return ret;
}


int srs_amf0_read_boolean(struct SrsBuffer* stream, bool* value){

    int ret = ERROR_SUCCESS;  
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 read bool marker failed. ret=%d\n", ret);
        return ret;
    }
    
    char marker = srs_buffer_read_1bytes(stream);
    if (marker != RTMP_AMF0_Boolean) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 check bool marker failed.\n"
            "marker=%#x, required=%#x, ret=%d\n", marker, RTMP_AMF0_Boolean, ret);
        return ret;
    }
    srs_verbose("amf0 read bool marker success\n");

    // value
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 read bool value failed. ret=%d\n", ret);
        return ret;
    }
    (*value) = (srs_buffer_read_1bytes(stream) != 0);  
    srs_verbose("amf0 read bool value success. value=%d\n", value);
    
    return ret;
}


int srs_amf0_write_string(struct SrsBuffer* stream, AVal value){

    int ret = ERROR_SUCCESS; 
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write string marker failed. ret=%d\n", ret);
        return ret;
    }
    srs_buffer_write_1bytes(stream,RTMP_AMF0_String);
    srs_verbose("amf0 write string marker success\n");
    
    return srs_amf0_write_utf8(stream, value);
}


int srs_amf0_read_string(struct SrsBuffer* stream, AVal* value){

    int ret = ERROR_SUCCESS;
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write string marker failed. ret=%d\n", ret);
        return ret;
    }
    char marker = srs_buffer_read_1bytes(stream);
    if (marker != RTMP_AMF0_String) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 check string marker failed.\n"
            "marker=%#x, required=%#x, ret=%d\n", marker, RTMP_AMF0_String, ret);
        return ret;
    }
    return srs_amf0_read_utf8(stream, value);

}

int srs_amf0_write_number(struct SrsBuffer* stream, double value){

    int ret = ERROR_SUCCESS;
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write number marker failed. ret=%d\n", ret);
        return ret;
    }
    
    srs_buffer_write_1bytes(stream,RTMP_AMF0_Number);
    srs_verbose("amf0 write number marker success\n");

    // value
    if (!srs_buffer_require(stream,8)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write number value failed. ret=%d\n", ret);
        return ret;
    }
    int64_t temp = 0x00;
    memcpy(&temp, &value, 8);
    srs_buffer_write_8bytes(stream,temp);
    srs_verbose("amf0 write number value success. value=%.2f\n", value);
    
    return ret;
}


int srs_amf0_read_number(struct SrsBuffer* stream, double* value){

    int ret = ERROR_SUCCESS; 
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 read number marker failed. ret=%d\n", ret);
        return ret;
    }
    
    char marker = srs_buffer_read_1bytes(stream);
    if (marker != RTMP_AMF0_Number) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 check number marker failed.\n"
            "marker=%#x, required=%#x, ret=%d\n", marker, RTMP_AMF0_Number, ret);
        return ret;
    }
    srs_verbose("amf0 read number marker success\n");

    // value
    if (!srs_buffer_require(stream,8)) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 read number value failed. ret=%d\n", ret);
        return ret;
    }

    int64_t temp = srs_buffer_read_8bytes(stream);
    memcpy(value, &temp, 8);
    srs_verbose("amf0 read number value success. value=%.2f\n", value);
    
    return ret;
}


int srs_amf0_write_null(struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write null marker failed. ret=%d\n", ret);
        return ret;
    }
    srs_buffer_write_1bytes(stream,RTMP_AMF0_Null);
    srs_verbose("amf0 write null marker success\n");    
    return ret;
}


int srs_amf0_read_null(struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 read null marker failed. ret=%d\n", ret);
        return ret;
    }
    
    char marker = srs_buffer_read_1bytes(stream);
    if (marker != RTMP_AMF0_Null) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 check null marker failed.\n "
            "marker=%#x, required=%#x, ret=%d\n", marker, RTMP_AMF0_Null, ret);
        return ret;
    }
    srs_verbose("amf0 read null success\n");
    
    return ret;
}


int srs_amf0_write_undefined(struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS; 
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write undefined marker failed. ret=%d\n", ret);
        return ret;
    }
    srs_buffer_write_1bytes(stream,RTMP_AMF0_Undefined);
    srs_verbose("amf0 write undefined marker success\n");
    return ret;
}

int srs_amf0_read_undefined(struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 read undefined marker failed. ret=%d\n", ret);
        return ret;
    }
    
    char marker = srs_buffer_read_1bytes(stream);
    if (marker != RTMP_AMF0_Undefined) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 check undefined marker failed.\n"
            "marker=%#x, required=%#x, ret=%d\n", marker, RTMP_AMF0_Undefined, ret);
        return ret;
    }
    srs_verbose("amf0 read undefined success\n");
    return ret;
}


int srs_amf0_size_utf8(AVal value)
{
    return 2 + value.av_len;
}


int srs_amf0_size_str(AVal value)
{
    return 1 + srs_amf0_size_utf8(value);
}

int srs_amf0_size_number()
{
    return 1 + 8;
}

int srs_amf0_size_null()
{
    return 1;
}

int srs_amf0_size_undefined()
{
    return 1;
}

int srs_amf0_size_object_eof()
{
    return 2 + 1;
}

int srs_amf0_size_boolean()
{
    return 1 + 1;
}

