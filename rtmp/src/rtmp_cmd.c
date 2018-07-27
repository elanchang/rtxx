#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "rtmp_error.h"
#include "rtmp_string.h"
#include "rtmp_amf.h"
#include "rtmp_mbuffer.h"
#include "rtmp_msg.h"
#include "rtmp_utils.h"
#include "rtmp_cmd.h"
#include "rtmp_protocol.h"

/*cmd :connect app */
struct SrsConnectAppPacket * srs_connect_app_packt_init(AVal app, AVal tcUrl){

    struct SrsConnectAppPacket* srsconnectapppacket = (struct SrsConnectAppPacket*)malloc(sizeof(struct SrsConnectAppPacket));
    memset(srsconnectapppacket,0,sizeof(struct SrsConnectAppPacket));
    av_val_copy_str(&srsconnectapppacket->command_name,RTMP_AMF0_COMMAND_CONNECT);
    
    srsconnectapppacket->transaction_id = 1;
    srsconnectapppacket->args=NULL;
    srsconnectapppacket->command_object=kvs_create(20, 5);
    srsconnectapppacket->args = NULL;

    srsconnectapppacket->app_value.type=str_type;
    srsconnectapppacket->flashver_value.type=str_type;
    srsconnectapppacket->swfurl_alue.type=str_type;
    srsconnectapppacket->tcurl_value.type=str_type;
    srsconnectapppacket->fpad_value.type=bool_type;
    srsconnectapppacket->capabilities_value.type=int_type;
    srsconnectapppacket->audiocodecs_value.type=int_type;
    srsconnectapppacket->videocodecs_value.type=int_type;
    srsconnectapppacket->videofunction_value.type=int_type;
    srsconnectapppacket->pageUrl_value.type=str_type;
    srsconnectapppacket->objectEncoding_value.type=int_type;
    
    av_val_copy_str(&(srsconnectapppacket->app_value.u.str),app.av_val);
    av_val_copy_str(&(srsconnectapppacket->flashver_value.u.str),"WIN 15,0,0,239");
    av_val_copy_str(&(srsconnectapppacket->swfurl_alue.u.str),"");
    av_val_copy_str(&(srsconnectapppacket->tcurl_value.u.str),tcUrl.av_val);
    srsconnectapppacket->fpad_value.u.bool_value=false;
    srsconnectapppacket->capabilities_value.u.int_value=239;
    srsconnectapppacket->audiocodecs_value.u.int_value=3575;
    srsconnectapppacket->videocodecs_value.u.int_value=252;
    srsconnectapppacket->videofunction_value.u.int_value=1;
    av_val_copy_str(&(srsconnectapppacket->pageUrl_value.u.str),"");
    srsconnectapppacket->objectEncoding_value.u.int_value=0;
    
    return srsconnectapppacket;
}

void srs_connect_app_packt_uinit(struct        SrsConnectAppPacket *srsconnectapppacket){

    srs_freep(srsconnectapppacket->command_name.av_val);
    srs_freep(srsconnectapppacket->app_value.u.str.av_val);
    srs_freep(srsconnectapppacket->flashver_value.u.str.av_val);
    srs_freep(srsconnectapppacket->swfurl_alue.u.str.av_val);
    srs_freep(srsconnectapppacket->tcurl_value.u.str.av_val);
    srs_freep(srsconnectapppacket->pageUrl_value.u.str.av_val);
    kvs_clear_destroy(srsconnectapppacket->command_object);
    kvs_clear_destroy(srsconnectapppacket->args);
    srs_freep(srsconnectapppacket);
}


int object_write_end(struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;  
    // value
    if (!srs_buffer_require(stream,2)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write object eof value failed. ret=%d\n", ret);
        return ret;
    }
    srs_buffer_write_2bytes(stream,0x00);
    srs_verbose("amf0 write object eof value success\n");
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write object eof marker failed. ret=%d\n", ret);
        return ret;
    }
    srs_buffer_write_1bytes(stream,RTMP_AMF0_ObjectEnd);    
    srs_verbose("amf0 read object eof success\n");
    return ret;
}



static int connect_app_object_write(struct     SrsConnectAppPacket *srsconnectapppacket ,struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;  
    // marker
    if (!srs_buffer_require(stream,1)) {
        ret = ERROR_RTMP_AMF0_ENCODE;
        srs_error("amf0 write object marker failed. ret=%d\n", ret);
        return ret;
    }
    srs_buffer_write_1bytes(stream,RTMP_AMF0_Object);
    srs_verbose("amf0 write object marker success\n");
     
    struct info_t info,*cached;

    cached=kvs_get(srsconnectapppacket->command_object,"app");
    info = *cached;

    AVal app=AVC("app");
    srs_amf0_write_utf8(stream, app);
    srs_amf0_write_string(stream, info.u.str);

    cached=kvs_get(srsconnectapppacket->command_object,"flashVer");
    info = *cached;
    AVal flashVer=AVC("flashVer");
    srs_amf0_write_utf8(stream, flashVer);
    srs_amf0_write_string(stream, info.u.str);
    
    cached=kvs_get(srsconnectapppacket->command_object,"swfUrl");
    info = *cached;
    AVal swfUrl=AVC("swfUrl");
    srs_amf0_write_utf8(stream, swfUrl);
    srs_amf0_write_string(stream, info.u.str);

    
    cached=kvs_get(srsconnectapppacket->command_object,"tcUrl");
    info = *cached;
    AVal tcUrl=AVC("tcUrl");
    srs_amf0_write_utf8(stream, tcUrl);
    srs_amf0_write_string(stream, info.u.str);

    cached=kvs_get(srsconnectapppacket->command_object,"fpad");
    info = *cached;
    AVal fpad=AVC("fpad");
    srs_amf0_write_utf8(stream, fpad);
    srs_amf0_write_boolean(stream, info.u.bool_value);
    

    cached=kvs_get(srsconnectapppacket->command_object,"capabilities");
    info = *cached;
    AVal capabilities=AVC("capabilities");
    srs_amf0_write_utf8(stream, capabilities);
    srs_amf0_write_number(stream, info.u.int_value);

  
    cached=kvs_get(srsconnectapppacket->command_object,"audioCodecs");
    info = *cached;
    AVal audioCodecs=AVC("audioCodecs");
    srs_amf0_write_utf8(stream, audioCodecs);
    srs_amf0_write_number(stream, info.u.int_value);

   
    cached=kvs_get(srsconnectapppacket->command_object,"videoCodecs");
    info = *cached;
    AVal videoCodecs=AVC("videoCodecs");
    srs_amf0_write_utf8(stream, videoCodecs);
    srs_amf0_write_number(stream, info.u.int_value);

    
    cached=kvs_get(srsconnectapppacket->command_object,"videoFunction");
    info = *cached;
    AVal videoFunction=AVC("videoFunction");
    srs_amf0_write_utf8(stream, videoFunction);
    srs_amf0_write_number(stream, info.u.int_value);

 
    cached=kvs_get(srsconnectapppacket->command_object,"pageUrl");
    info = *cached;
    AVal pageUrl=AVC("pageUrl");
    srs_amf0_write_utf8(stream, pageUrl);
    srs_amf0_write_string(stream, info.u.str);

    cached=kvs_get(srsconnectapppacket->command_object,"objectEncoding");
    info = *cached;
    AVal objectEncoding=AVC("objectEncoding");
    srs_amf0_write_utf8(stream, objectEncoding);
    srs_amf0_write_number(stream, info.u.int_value);

    object_write_end(stream);
    srs_verbose("write amf0 object success.\n");    
    return ret;
}

int srs_connect_app_packet_get_prefer_cid(){

    return RTMP_CID_OverConnection;
}

int srs_connect_app_packet_get_message_type(){

    return RTMP_MSG_AMF0CommandMessage;
}


int connect_app_object_total_size(void *packet)
{
    struct SrsConnectAppPacket* srsconnectapppacket = (struct SrsConnectAppPacket*)packet; 

    struct info_t info,*cached;

    int size = 1;

    cached=kvs_get(srsconnectapppacket->command_object,"app");
    info = *cached;
    AVal app=AVC("app");
    size+=srs_amf0_size_utf8(app);
    size+=srs_amf0_size_str(info.u.str);

    cached=kvs_get(srsconnectapppacket->command_object,"flashVer");
    info = *cached;
    AVal flashVer=AVC("flashVer");
    size+=srs_amf0_size_utf8(flashVer);
    size+=srs_amf0_size_str(info.u.str);
    
    cached=kvs_get(srsconnectapppacket->command_object,"swfUrl");
    info = *cached;
    AVal swfUrl=AVC("swfUrl");
    size+=srs_amf0_size_utf8(swfUrl);
    size+=srs_amf0_size_str(info.u.str);
    
    cached=kvs_get(srsconnectapppacket->command_object,"tcUrl");
    info = *cached;
    AVal tcUrl=AVC("tcUrl");
    size+=srs_amf0_size_utf8(tcUrl);
    size+=srs_amf0_size_str(info.u.str);

    AVal fpad=AVC("fpad");
    size+=srs_amf0_size_utf8(fpad);
    size+=srs_amf0_size_boolean();
  
    AVal capabilities=AVC("capabilities");
    size+=srs_amf0_size_utf8(capabilities);
    size+=srs_amf0_size_number();

    AVal audioCodecs=AVC("audioCodecs");
    size+=srs_amf0_size_utf8(audioCodecs);
    size+=srs_amf0_size_number();
     
    AVal videoCodecs=AVC("videoCodecs");
    size+=srs_amf0_size_utf8(videoCodecs);
    size+=srs_amf0_size_number();

    AVal videoFunction=AVC("videoFunction");
    size+=srs_amf0_size_utf8(videoFunction);
    size+=srs_amf0_size_number();

    cached=kvs_get(srsconnectapppacket->command_object,"pageUrl");
    info = *cached;
    AVal pageUrl=AVC("pageUrl");
    size+=srs_amf0_size_utf8(pageUrl);
    size+=srs_amf0_size_str(info.u.str);
    
    AVal objectEncoding=AVC("objectEncoding");
    size+=srs_amf0_size_utf8(objectEncoding);
    size+=srs_amf0_size_number();
    
    size += srs_amf0_size_object_eof();  
    return size;
}




int srs_connect_app_packet_get_size(void *packet){

    struct SrsConnectAppPacket* srsconnectapppacket = (struct SrsConnectAppPacket*)packet; 
    int size = 0;
    size += srs_amf0_size_str(srsconnectapppacket->command_name);
    size += srs_amf0_size_number();
    size += connect_app_object_total_size(packet);
    return size;
}

int srs_connect_app_packet_encode_packet(struct           SrsConnectAppPacket *srsconnectapppacket ,struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;
    if ((ret = srs_amf0_write_string(stream, srsconnectapppacket->command_name)) != ERROR_SUCCESS) {
        srs_error("encode command_name failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode command_name success.\n"); 
    if ((ret = srs_amf0_write_number(stream, srsconnectapppacket->transaction_id)) != ERROR_SUCCESS) {
        srs_error("encode transaction_id failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode transaction_id success.\n");
    connect_app_object_write(srsconnectapppacket,stream);
    srs_info("encode connect app request packet success.\n");   
    return ret;
}


/*cmd :window ackesize */
struct SrsSetWindowAckSizePacket * srs_set_window_acksize_packet_init(){
    
    struct SrsSetWindowAckSizePacket* srssetwindowacksizepacket = (struct SrsSetWindowAckSizePacket*)malloc(sizeof(struct SrsSetWindowAckSizePacket)); 
    memset(srssetwindowacksizepacket,0,sizeof(struct SrsSetWindowAckSizePacket));
    srssetwindowacksizepacket->ackowledgement_window_size = 2500000;
    return srssetwindowacksizepacket;
}

void srs_set_window_acksize_packet_uinit(struct         SrsSetWindowAckSizePacket *srssetwindowacksizepacket){

    srs_freep(srssetwindowacksizepacket);
}


int srs_set_window_acksize_packet_decode(struct           SrsSetWindowAckSizePacket *srssetwindowacksizepacket ,struct SrsBuffer* stream)
{
    int ret = ERROR_SUCCESS;
    
    if (!srs_buffer_require(stream,4)) {
        ret = ERROR_RTMP_MESSAGE_DECODE;
        srs_error("decode ack window size failed. ret=%d\n", ret);
        return ret;
    }
    
    srssetwindowacksizepacket->ackowledgement_window_size =srs_buffer_read_4bytes(stream);;
    srs_info("decode ack window size success\n");
    return ret;
}

int srs_set_window_acksize_packet_encode_packet(struct             SrsSetWindowAckSizePacket *srssetwindowacksizepacket ,struct SrsBuffer* stream){
        int ret = ERROR_SUCCESS;
        
        if (!srs_buffer_require(stream,4)) {
            ret = ERROR_RTMP_MESSAGE_ENCODE;
            srs_error("encode ack size packet failed. ret=%d\n", ret);
            return ret;
        }
        srs_buffer_write_4bytes(stream,srssetwindowacksizepacket->ackowledgement_window_size);       
        return ret;
}


int srs_set_window_acksize_packet_get_prefer_cid()
{
    return RTMP_CID_ProtocolControl;
}

int srs_set_window_acksize_packet_get_message_type()
{
    return RTMP_MSG_WindowAcknowledgementSize;
}


int srs_set_window_acksize_packet_get_size(void *packet)
{
    return 4;
}

/*cmd :release stream */

struct SrsReleaseStreamPacket* srs_release_stream_packet_init(){
    
    struct SrsReleaseStreamPacket* srsreleasestreampacket = (struct SrsReleaseStreamPacket*)malloc(sizeof(struct SrsReleaseStreamPacket)); 
    memset(srsreleasestreampacket,0,sizeof(struct SrsReleaseStreamPacket));
    av_val_copy_str(&srsreleasestreampacket->command_name,RTMP_AMF0_COMMAND_RELEASE_STREAM);
    srsreleasestreampacket->transaction_id=2;
    return srsreleasestreampacket;
}

void srs_release_stream_packet_uinit(struct          SrsReleaseStreamPacket *srsreleasestreampacket){

    srs_freep(srsreleasestreampacket->command_name.av_val);
    srs_freep(srsreleasestreampacket->stream_name.av_val);
    srs_freep(srsreleasestreampacket);
}



int srs_release_stream_packet_encode_packet(struct             SrsReleaseStreamPacket *srsreleasestreampacket ,struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;   
    if ((ret = srs_amf0_write_string(stream, srsreleasestreampacket->command_name)) != ERROR_SUCCESS) {
        srs_error("encode command_name failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode command_name success.\n"); 
    if ((ret = srs_amf0_write_number(stream, srsreleasestreampacket->transaction_id)) != ERROR_SUCCESS) {
        srs_error("encode transaction_id failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode transaction_id success.\n");
    
    if ((ret = srs_amf0_write_null(stream)) != ERROR_SUCCESS) {
        srs_error("encode command_object failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode command_object success.\n"); 
    if ((ret = srs_amf0_write_string(stream, srsreleasestreampacket->stream_name)) != ERROR_SUCCESS) {
        srs_error("encode stream_name failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode args success.\n"); 
    srs_info("encode FMLE start response packet success.\n");   
    return ret;
}


int srs_release_stream_packet_get_prefer_cid(){

    return RTMP_CID_OverConnection;
}

int srs_release_stream_packet_get_message_type(){

    return RTMP_MSG_AMF0CommandMessage;
}

int srs_release_stream_packet_get_size(void *packet){

    struct SrsReleaseStreamPacket* srsreleasestreampacket = (struct SrsReleaseStreamPacket*)packet; 
    return srs_amf0_size_str(srsreleasestreampacket->command_name) + srs_amf0_size_number()
        + srs_amf0_size_null() + srs_amf0_size_str(srsreleasestreampacket->stream_name);
}





/*cmd :fcpublish */
struct SrsFcPublishPacket * srs_fcpublish_packet_init(){
    
    struct SrsFcPublishPacket* srsfcpublishpacket = (struct SrsFcPublishPacket*)malloc(sizeof(struct SrsFcPublishPacket)); 
    memset(srsfcpublishpacket,0,sizeof(struct SrsFcPublishPacket));
    av_val_copy_str(&srsfcpublishpacket->command_name,RTMP_AMF0_COMMAND_FC_PUBLISH);
    srsfcpublishpacket->transaction_id=3;
    return srsfcpublishpacket;
}

void srs_fcpublish_packet_uinit(struct          SrsFcPublishPacket *srsfcpublishpacket){

    srs_freep(srsfcpublishpacket->command_name.av_val);
    srs_freep(srsfcpublishpacket->stream_name.av_val);
    srs_freep(srsfcpublishpacket);
}


int srs_fcpublish_packet_encode_packet(struct           SrsFcPublishPacket *srsfcpublishpacket ,struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;   
    if ((ret = srs_amf0_write_string(stream, srsfcpublishpacket->command_name)) != ERROR_SUCCESS) {
        srs_error("encode command_name failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode command_name success."); 
    if ((ret = srs_amf0_write_number(stream, srsfcpublishpacket->transaction_id)) != ERROR_SUCCESS) {
        srs_error("encode transaction_id failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode transaction_id success.\n");
    
    if ((ret = srs_amf0_write_null(stream)) != ERROR_SUCCESS) {
        srs_error("encode command_object failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode command_object success.\n"); 
    if ((ret = srs_amf0_write_string(stream, srsfcpublishpacket->stream_name)) != ERROR_SUCCESS) {
        srs_error("encode stream_name failed. ret=%d\n", ret);
        return ret;
    }  
    srs_verbose("encode args success.\n"); 
    srs_info("encode FMLE start response packet success.\n");   
    return ret;
}


int srs_fcpublish_packet_get_prefer_cid(){

    return RTMP_CID_OverConnection;
}

int srs_fcpublish_packet_get_message_type(){

    return RTMP_MSG_AMF0CommandMessage;
}

int srs_fcpublish_packet_get_size(void *packet){

    struct SrsFcPublishPacket* srsfcpublishpacket = (struct SrsFcPublishPacket*)packet; 
    return srs_amf0_size_str(srsfcpublishpacket->command_name) + srs_amf0_size_number()
        + srs_amf0_size_null() + srs_amf0_size_str(srsfcpublishpacket->stream_name);
}

/*cmd :createstream */
struct SrsCreateStreamPacket * srs_createstream_packet_init(){
    
    struct SrsCreateStreamPacket* srscreatestreampacket = (struct SrsCreateStreamPacket*)malloc(sizeof(struct SrsCreateStreamPacket)); 
    memset(srscreatestreampacket,0,sizeof(struct SrsCreateStreamPacket));
    av_val_copy_str(&srscreatestreampacket->command_name,RTMP_AMF0_COMMAND_CREATE_STREAM);
    srscreatestreampacket->transaction_id=2;
    return srscreatestreampacket;
}

void srs_createstream_packet_uinit(struct          SrsCreateStreamPacket *srscreatestreampacket){

    srs_freep(srscreatestreampacket->command_name.av_val);
    srs_freep(srscreatestreampacket);
}



int srs_createstream_packet_encode_packet(struct             SrsCreateStreamPacket *srscreatestreampacket ,struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;
    if ((ret = srs_amf0_write_string(stream,srscreatestreampacket->command_name)) != ERROR_SUCCESS) {
        srs_error("encode command_name failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode command_name success.\n");
    
    if ((ret = srs_amf0_write_number(stream, srscreatestreampacket->transaction_id)) != ERROR_SUCCESS) {
        srs_error("encode transaction_id failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode transaction_id success.\n");
    
    if ((ret = srs_amf0_write_null(stream)) != ERROR_SUCCESS) {
        srs_error("encode command_object failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode command_object success.\n");
    srs_info("encode create stream request packet success.\n");
    return ret;
}


int srs_createstream_packet_get_prefer_cid(){

    return RTMP_CID_OverConnection;
}

int srs_createstream_packet_get_message_type(){

    return RTMP_MSG_AMF0CommandMessage;
}

int srs_createstream_packet_get_size(void *packet){

    struct SrsCreateStreamPacket* createstreampacket = (struct SrsCreateStreamPacket*)packet;  
    return (srs_amf0_size_str(createstreampacket->command_name) + srs_amf0_size_number()
        + srs_amf0_size_null());
}


/*cmd :publish */
struct SrsPublishPacket * srs_publish_packet_init(){
    
    struct SrsPublishPacket* srspublishpacket = (struct SrsPublishPacket*)malloc(sizeof(struct SrsPublishPacket)); 
    memset(srspublishpacket,0,sizeof(struct SrsPublishPacket));
    av_val_copy_str(&srspublishpacket->command_name,RTMP_AMF0_COMMAND_PUBLISH);
    av_val_copy_str(&srspublishpacket->type,"live");
    srspublishpacket->transaction_id=0;
    return srspublishpacket;
}


void srs_publish_packet_uinit(struct         SrsPublishPacket *srspublishpacket){

    srs_freep(srspublishpacket->command_name.av_val);
    srs_freep(srspublishpacket->stream_name.av_val);
    srs_freep(srspublishpacket->type.av_val);
    srs_freep(srspublishpacket);
}


int srs_publish_packet_encode_packet(struct          SrsPublishPacket *srspublishpacket ,struct SrsBuffer* stream){
    int ret = ERROR_SUCCESS;
    
    if ((ret = srs_amf0_write_string(stream, srspublishpacket->command_name)) != ERROR_SUCCESS) {
        srs_error("encode command_name failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode command_name success.\n");  
    if ((ret = srs_amf0_write_number(stream, srspublishpacket->transaction_id)) != ERROR_SUCCESS) {
        srs_error("encode transaction_id failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode transaction_id success.\n");   
    if ((ret = srs_amf0_write_null(stream)) != ERROR_SUCCESS) {
        srs_error("encode command_object failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode command_object success.\n");  
    if ((ret = srs_amf0_write_string(stream, srspublishpacket->stream_name)) != ERROR_SUCCESS) {
        srs_error("encode stream_name failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode stream_name success.\n");
   
    if ((ret = srs_amf0_write_string(stream,srspublishpacket->type)) != ERROR_SUCCESS) {
        srs_error("encode type failed. ret=%d\n", ret);
        return ret;
    }
    srs_verbose("encode type success.\n");    
    srs_info("encode play request packet success.\n");    
    return ret;

}


int srs_publish_packet_get_prefer_cid(){

    return RTMP_CID_OverStream;
}

int srs_publish_packet_get_message_type(){

    return RTMP_MSG_AMF0CommandMessage;
}


int srs_publish_packet_get_size(void *packet){

    struct SrsPublishPacket* srspublishpacket = (struct SrsPublishPacket*)packet;  
    return srs_amf0_size_str(srspublishpacket->command_name) + srs_amf0_size_number()
        + srs_amf0_size_null() + srs_amf0_size_str(srspublishpacket->stream_name)
        + srs_amf0_size_str(srspublishpacket->type);
}


/*cmd :createstream_res packet */

int srs_createstreamres_packet_decode(struct SrsCreateStreamResPacket* createstreamrespacket ,struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;
    if ((ret = srs_amf0_read_string(stream, &(createstreamrespacket->command_name))) != ERROR_SUCCESS) {
        srs_error("amf0 decode createStream command_name failed. ret=%d\n", ret);
        return ret;
    }
     
    if ((strcmp(createstreamrespacket->command_name.av_val, RTMP_AMF0_COMMAND_RESULT)!= 0)) {
        ret = ERROR_RTMP_AMF0_DECODE;
        srs_error("amf0 decode createStream command_name failed. "
           "command_name=%s, ret=%d\n",createstreamrespacket->command_name.av_val, ret);
        return ret;
    }
    if ((ret = srs_amf0_read_number(stream, &(createstreamrespacket->transaction_id)))!= ERROR_SUCCESS) {
        srs_error("amf0 decode createStream transaction_id failed. ret=%d\n", ret);
        return ret;
    }
    
    if ((ret = srs_amf0_read_null(stream)) != ERROR_SUCCESS) {
        srs_error("amf0 decode createStream command_object failed. ret=%d\n", ret);
        return ret;
    }
    
    if ((ret = srs_amf0_read_number(stream, &(createstreamrespacket->stream_id))) != ERROR_SUCCESS) {
        srs_error("amf0 decode createStream stream_id failed. ret=%d\n", ret);
        return ret;
    }    
    return ret;
}



/*cmd :set chunk size */

struct SrsSetChunkSizePacket * srs_set_chunksize_packet_init(int out_chunk_size){
    
    struct SrsSetChunkSizePacket* srssetchunksizepacket = (struct SrsSetChunkSizePacket*)malloc(sizeof(struct SrsSetChunkSizePacket)); 
    memset(srssetchunksizepacket,0,sizeof(struct SrsSetChunkSizePacket));
    srssetchunksizepacket->chunk_size=out_chunk_size;
    return srssetchunksizepacket;
}


void  srs_set_chunksize_packet_uinit(struct         SrsSetChunkSizePacket *srssetchunksizepacket){

    srs_freep(srssetchunksizepacket);
}


int srs_set_chunksize_packet_decode(struct SrsSetChunkSizePacket* srssetchunksizepacket,struct SrsBuffer* stream){

    int ret = ERROR_SUCCESS;
    if (!srs_buffer_require(stream,4)) {
        ret = ERROR_RTMP_MESSAGE_DECODE;
        srs_error("decode chunk size failed. ret=%d\n", ret);
        return ret;
    } 
    srssetchunksizepacket->chunk_size = srs_buffer_read_4bytes(stream);
    srs_info("decode chunk size success. chunk_size=%d\n", srssetchunksizepacket->chunk_size);
    return ret;
}

int srs_set_chunksize_packet_get_prefer_cid()
{
    return RTMP_CID_ProtocolControl;
}

int srs_set_chunksize_packet_get_message_type()
{
    return RTMP_MSG_SetChunkSize;
}

int srs_set_chunksize_packet_get_size()
{
    return 4;
}

int srs_set_chunksize_packet_encode_packet(struct SrsSetChunkSizePacket* srssetchunksizepacket,struct SrsBuffer* stream)
{
    int ret = ERROR_SUCCESS;
    
    if (!srs_buffer_require(stream,4)) {
        ret = ERROR_RTMP_MESSAGE_ENCODE;
        srs_error("encode chunk packet failed. ret=%d\n", ret);
        return ret;
    }
    srs_buffer_write_4bytes(stream,srssetchunksizepacket->chunk_size);  
    srs_verbose("encode chunk packet success. ack_size=%d\n", srssetchunksizepacket->chunk_size);   
    return ret;
}


#if 0
int srsacknowledgementpacket_get_prefer_cid(){
    return RTMP_CID_ProtocolControl;
}

int srsacknowledgementpacket_get_message_type()
{
    return RTMP_MSG_Acknowledgement;
}

int srsacknowledgementpacket_get_size()
{
    return 4;
}

int srsacknowledgementpacket_encode_packet(struct             SrsAcknowledgementPacket *srsacknowledgementpacket ,struct SrsBuffer* stream)
{
    int ret = ERROR_SUCCESS;
    
    if (!srs_buffer_require(stream,4)) {
        ret = ERROR_RTMP_MESSAGE_ENCODE;
        srs_error("encode acknowledgement packet failed. ret=%d", ret);
        return ret;
    }
    srs_buffer_write_4bytes(stream,srsacknowledgementpacket->sequence_number);  
    srs_verbose("encode acknowledgement packet "
        "success. sequence_number=%d", srsacknowledgementpacket->sequence_number);
    
    return ret;
}
#endif

