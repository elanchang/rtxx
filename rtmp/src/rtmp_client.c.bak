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
#include "rtmp_socket.h"
#include "rtmp_msg.h"
#include "rtmp_chunk.h"
#include "rtmp_cmd.h"
#include "rtmp_protocol.h"
#include "rtmp_client.h"


struct SrsRtmpClient* srs_rtmp_client_init(struct SimpleSocketStream* skt){

    struct SrsRtmpClient* srsrtmpclient = (struct SrsRtmpClient*)malloc(sizeof(struct SrsRtmpClient));
    memset(srsrtmpclient,0,sizeof(struct SrsRtmpClient));
    srsrtmpclient->skt=skt;
    srsrtmpclient->hs_bytes =srs_handshakebytes_init();
    srsrtmpclient->protocol =srs_protocol_init();
    srsrtmpclient->protocol->skt=skt;
    return srsrtmpclient;
}

void srs_rtmp_client_uinit(struct SrsRtmpClient* srsrtmpclient){

    srs_handshakebytes_uinit(srsrtmpclient->hs_bytes);
    srs_protocol_uinit(srsrtmpclient->protocol);
    srsrtmpclient->skt=NULL;
    srs_freep(srsrtmpclient);
}


int srs_rtmp_client_simple_handshake(struct SrsRtmpClient* srsrtmpclient,struct  SimpleSocketStream* simplesocketstream){

    int ret = ERROR_SUCCESS;    
    srs_assert(srsrtmpclient->hs_bytes);    
    if ((ret = srs_simple_handshake_with_server(srsrtmpclient->hs_bytes, simplesocketstream)) != ERROR_SUCCESS) {
        return ret;
    }
    return ret;
}

static int expect_message(struct SrsProtocol* srsprotocol,struct SrsCommonMessage** pmsg, void** ppacket,enum pkt_type expect_type){

    *pmsg = NULL;
    *ppacket = NULL;
    
    int ret = ERROR_SUCCESS;
    while (true) {
        struct SrsCommonMessage* msg = NULL;
        if ((ret = srs_protocol_recv_message(srsprotocol,&msg)) != ERROR_SUCCESS) {
            if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
                srs_error("recv message failed. ret=%d\n", ret);
            }
            return ret;
        }
        srs_verbose("recv message success.\n");     
        void* packet = NULL;
        enum pkt_type return_type;
        if ((ret = srs_protocol_decode_message(msg, &packet,&return_type)) != ERROR_SUCCESS) {
            srs_error("decode message failed. ret=%d\n", ret);
            srs_freep(msg->payload);
            srs_freep(msg);
            srs_freep(packet);
            return ret;
        }
        if(expect_type!=return_type)
        {
             srs_info("drop message(type=%d, size=%d, time=%ld, sid=%d).\n", 
                     msg->header.message_type, msg->header.payload_length,
                     msg->header.timestamp, msg->header.stream_id);
             srs_freep(msg->payload);
             srs_freep(msg);
             srs_freep(packet);
             continue;
         }        
        *pmsg = msg;
        *ppacket = packet;           
        break;
    }

    return ret;
}

int srs_rtmp_client_connect_app(struct SrsRtmpClient* srsrtmpclient, AVal app, AVal tcUrl){

    int ret = ERROR_SUCCESS;
    if (true) {
        struct SrsConnectAppPacket* pkt=srs_connect_app_packt_init(app,tcUrl);
        kvs_put(pkt->command_object,"app",&pkt->app_value);
        kvs_put(pkt->command_object,"flashVer",&pkt->flashver_value);
        kvs_put(pkt->command_object,"swfUrl",&pkt->swfurl_alue);
        kvs_put(pkt->command_object,"tcUrl",&pkt->tcurl_value);
        kvs_put(pkt->command_object,"fpad",&pkt->fpad_value);
        kvs_put(pkt->command_object,"capabilities",&pkt->capabilities_value);
        kvs_put(pkt->command_object,"audioCodecs",&pkt->audiocodecs_value);
        kvs_put(pkt->command_object,"videoCodecs",&pkt->videocodecs_value);
        kvs_put(pkt->command_object,"videoFunction",&pkt->videofunction_value);
        kvs_put(pkt->command_object,"pageUrl",&pkt->pageUrl_value);
        kvs_put(pkt->command_object,"objectEncoding",&pkt->objectEncoding_value);
        if ((ret = srs_protocol_send_and_free_packet(srsrtmpclient->protocol,srsrtmpclient->skt,pkt,0,CONNECT_APP_PACKET_TYPE)) != ERROR_SUCCESS) {
            srs_connect_app_packt_uinit(pkt);
            return ret;
        }
        srs_connect_app_packt_uinit(pkt);
    }
    if (true) {
        struct SrsSetWindowAckSizePacket* pkt=srs_set_window_acksize_packet_init();
        if ((ret = srs_protocol_send_and_free_packet(srsrtmpclient->protocol,srsrtmpclient->skt,pkt, 0,SET_WINDOW_ACKSIZE_PACKET_TYPE)) != ERROR_SUCCESS) {
            srs_set_window_acksize_packet_uinit(pkt);
            return ret;
        }
        srs_set_window_acksize_packet_uinit(pkt);
    }

    struct SrsCommonMessage* msg = NULL;
    void *pkt=NULL;
    enum pkt_type expect_type=CONNECT_APP_RES_TYPE;
    if ((ret = expect_message(srsrtmpclient->protocol,&msg, &pkt,expect_type)) != ERROR_SUCCESS) {
        srs_error("expect connect app response message failed. ret=%d", ret);
        return ret;
    }
    srs_freep(msg->payload);
    srs_freep(msg);
    srs_freep(pkt);
    return ret;
}


int srs_rtmp_client_fmle_publish(struct SrsRtmpClient* srsrtmpclient,AVal stream, int* stream_id){

    int ret = ERROR_SUCCESS;
    
    if (true) {
         struct SrsReleaseStreamPacket* pkt = srs_release_stream_packet_init();
         av_val_copy_str(&pkt->stream_name,stream.av_val);
        if ((ret = srs_protocol_send_and_free_packet(srsrtmpclient->protocol,srsrtmpclient->skt,pkt, 0,RELEASE_STREAM_PACKET_TYPE)) != ERROR_SUCCESS) {
            srs_release_stream_packet_uinit(pkt); 
            return ret;
        }
        srs_release_stream_packet_uinit(pkt); 
    }
    if (true) {
        struct SrsFcPublishPacket* pkt = srs_fcpublish_packet_init();
        av_val_copy_str(&pkt->stream_name,stream.av_val);
        if ((ret = srs_protocol_send_and_free_packet(srsrtmpclient->protocol,srsrtmpclient->skt,pkt, 0,FCPUBLISH_PACKET_TYPE)) != ERROR_SUCCESS) {
            srs_fcpublish_packet_uinit(pkt); 
            return ret;
        }
        srs_fcpublish_packet_uinit(pkt); 
    } 
    if (true) {
        struct SrsCreateStreamPacket* pkt=srs_createstream_packet_init();
        pkt->transaction_id=4;
        if ((ret = srs_protocol_send_and_free_packet(srsrtmpclient->protocol,srsrtmpclient->skt,pkt, 0,CREATESTREAM_PACKET_TYPE)) != ERROR_SUCCESS) {
            srs_createstream_packet_uinit(pkt);
            return ret;
        }
        srs_createstream_packet_uinit(pkt);
    }
    // expect result of CreateStream
    if (true) {
        struct SrsCommonMessage* msg = NULL;
        void* pkt = NULL;
        enum pkt_type expect_type=CREATESTREAMRES_TYPE;
        if ((ret = expect_message(srsrtmpclient->protocol,&msg, &pkt,expect_type)) != ERROR_SUCCESS) {
            srs_error("expect connect app response message failed. ret=%d", ret);
            return ret;
        }
        srs_info("get create stream response message\n");
        struct SrsCreateStreamResPacket* createstreamrespacket = (struct SrsCreateStreamResPacket*)pkt;
        (*stream_id) = (int)createstreamrespacket->stream_id;
        srs_freep(msg->payload);
        srs_freep(msg);
        srs_freep(pkt);  
    }


    if (true) {
        struct SrsPublishPacket* pkt = srs_publish_packet_init();
        av_val_copy_str(&pkt->stream_name,stream.av_val);
        if ((ret = srs_protocol_send_and_free_packet(srsrtmpclient->protocol,srsrtmpclient->skt,pkt, 0,PUBLISH_PACKET_TYPE)) != ERROR_SUCCESS) {
            srs_publish_packet_uinit(pkt);
            return ret;
        }
        srs_publish_packet_uinit(pkt);
    }


    if(false){
        struct SrsSetChunkSizePacket* pkt = srs_set_chunksize_packet_init(SRS_MODIFY_RTMP_OUT_CHUNK_SIZE);
        if ((ret = srs_protocol_send_and_free_packet(srsrtmpclient->protocol,srsrtmpclient->skt,pkt, 0,SET_CHUNK_SIZE_TYPE)) != ERROR_SUCCESS) {
            srs_set_chunksize_packet_uinit(pkt);
            return ret;
        }
        srs_set_chunksize_packet_uinit(pkt);
     }
     return ret;
}



#if 0
void srs_rtmp_client_set_recv_timeout(struct SrsRtmpClient* srsrtmpclient,int64_t tm)
{
    srs_protocol_set_recv_timeout(srsrtmpclient->protocol,tm);
}

void srs_rtmp_client_set_send_timeout(struct SrsRtmpClient* srsrtmpclient,int64_t tm)
{
     srs_protocol_set_send_timeout(srsrtmpclient->protocol,tm);
}

int64_t srs_rtmp_client_get_recv_bytes(struct SrsRtmpClient* srsrtmpclient)
{
      return  srs_protocol_get_recv_bytes(srsrtmpclient->protocol);
}

int64_t srs_rtmp_client_get_send_bytes(struct SrsRtmpClient* srsrtmpclient)
{
      return  srs_protocol_get_send_bytes(srsrtmpclient->protocol);
}

int srs_rtmp_client_recv_message(struct SrsRtmpClient* srsrtmpclient,struct SrsCommonMessage** pmsg)
{
      return  srs_protocol_recv_message(srsrtmpclient->protocol,pmsg); 
}

int srs_rtmp_client_decode_message(struct SrsRtmpClient* srsrtmpclient, struct SrsCommonMessage* msg, void** ppacket,enum pkt_type *return_type)
{
       return  srs_protocol_decode_message(msg,ppacket,return_type);
}

int srs_rtmp_client_send_and_free_message(struct SrsRtmpClient* srsrtmpclient, struct SrsSharedPtrMessage* msg, int stream_id)
{
     return  srs_protocol_send_and_free_message(srsrtmpclient->protocol,msg,stream_id);
}

int srs_rtmp_client_send_and_free_messages(struct SrsRtmpClient* srsrtmpclient,struct SrsSharedPtrMessage** msgs, int nb_msgs, int stream_id)
{
     return  srs_protocol_send_and_free_messages(srsrtmpclient->protocol,msgs,nb_msgs,stream_id);
}

int srs_rtmp_client_send_and_free_packet(struct SrsRtmpClient* srsrtmpclient,void* packet, int stream_id,enum pkt_type packetype)
{
    return  srs_protocol_send_and_free_packet(srsrtmpclient->protocol,srsrtmpclient->skt,packet, stream_id,packetype);
}
#endif
