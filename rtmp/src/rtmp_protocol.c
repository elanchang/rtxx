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
#include "snx_rtmp.h"
//typedef void* srs_rtmp_t;
//typedef void* srs_amf0_t;
int srs_sharedptrmessage_chunk_header_2(void* rtmp,char* cache, int nb_cache, bool c0);


#if 1
static void hexdump(char *buf, size_t len)
{
    char *ptr = (char*) buf;
    int i;
    printf("len=%d==================\r\n\n",len);

    for (i = 0; i < len; ++i) {
       if (!(i & 0x0F)) printf("\r\n");
        printf("%02X ", ptr[i]);
    }
    printf("==================\r\n\n");
}
#endif

static int srs_protocol_do_send_and_free_packet(struct SrsProtocol* srsprotocol,struct SimpleSocketStream* skt,void* packet, int stream_id,enum pkt_type packetype);
static int srs_protocol_encode(void* packet,enum pkt_type packetype,int* psize, char** ppayload);
static int srs_protocol_do_simple_send(struct SrsProtocol* srsprotocol,struct SimpleSocketStream* skt,struct SrsMessageHeader* mh, char* payload, int size);
bool srs_sharedptrmessage_check(struct SrsSharedPtrMessage* msg,int stream_id);
int srs_sharedptrmessage_chunk_header(struct SrsSharedPtrMessage* msg,char* cache, int nb_cache, bool c0);

struct SrsProtocol* srs_protocol_init()
{
    struct SrsProtocol* srsprotocol = (struct SrsProtocol*)malloc(sizeof(struct SrsProtocol));
    memset(srsprotocol,0,sizeof(struct SrsProtocol));

    srsprotocol->in_buffer = srs_fast_stream_init();
    
    srsprotocol->in_chunk_size = SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE;
    srsprotocol->out_chunk_size = SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE;
    
    srsprotocol->nb_out_iovs = SRS_CONSTS_IOVS_MAX;
    srsprotocol->out_iovs = (struct iovec*)malloc(sizeof(struct iovec) * (srsprotocol->nb_out_iovs));
    // each chunk consumers atleast 2 iovs
    srs_assert(srsprotocol->nb_out_iovs >= 2);
    
    srsprotocol->warned_c0c3_cache_dry = false;
    srsprotocol->auto_response_when_recv = true;
    srsprotocol->show_debug_info = true;
    srsprotocol->in_buffer_length = 0;
    
    srsprotocol->cs_cache = NULL;
    if (SRS_PERF_CHUNK_STREAM_CACHE > 0) {  
        srsprotocol->cs_cache=(struct SrsChunkStream**)malloc(sizeof(struct SrsChunkStream*)*SRS_PERF_CHUNK_STREAM_CACHE);   
    }
    int cid=0;
    for (cid = 0; cid < SRS_PERF_CHUNK_STREAM_CACHE; cid++) {
        struct SrsChunkStream* cs = srs_chunkstream_init(cid);
        cs->header.perfer_cid = cid;
        srsprotocol->cs_cache[cid] = cs;
    }
    return srsprotocol;
}


void srs_protocol_uinit(struct SrsProtocol* srsprotocol)
{
    int i=0;
    srs_fast_stream_uinit(srsprotocol->in_buffer);   
    srs_freep(srsprotocol->out_iovs); 
    for (i = 0; i < SRS_PERF_CHUNK_STREAM_CACHE; i++) {
        struct SrsChunkStream* cs = srsprotocol->cs_cache[i];
        srs_chunkstream_uinit(cs);
    }
    srs_freep(srsprotocol->cs_cache);
    srs_freep(srsprotocol);
}


int srs_protocol_recv_message(struct SrsProtocol* srsprotocol,struct SrsCommonMessage** pmsg)
{
    *pmsg = NULL; 
    int ret = ERROR_SUCCESS;

    while (true) {
        struct SrsCommonMessage* msg = NULL;
        
        if ((ret = srs_protocol_recv_interlaced_message(srsprotocol,&msg)) != ERROR_SUCCESS) {
            if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
                srs_error("recv interlaced message failed. ret=%d\n", ret);
            }
            srs_freep(msg->payload);
            srs_freep(msg);
            return ret;
        }
        srs_verbose("entire msg received\n");

        if (!msg) {
            srs_info("got empty message without error.\n");
            continue;
        }    
        if (msg->size <= 0 || msg->header.payload_length <= 0) {
            srs_trace("ignore empty message(type=%d, size=%d, time=%"PRId64", sid=%d).\n",
                msg->header.message_type, msg->header.payload_length,
                msg->header.timestamp, msg->header.stream_id);
            srs_freep(msg->payload);
            srs_freep(msg);
            continue;
        }    
        if ((ret = srs_protocol_on_recv_message(srsprotocol,msg)) != ERROR_SUCCESS) {
            srs_error("hook the received msg failed. ret=%d\n", ret);
            srs_freep(msg->payload);
            srs_freep(msg);
            return ret;
        }       
        srs_verbose("got a msg, cid=%d, type=%d, size=%d, time=%"PRId64, 
            msg->header.perfer_cid, msg->header.message_type, msg->header.payload_length, 
            msg->header.timestamp);
        
        *pmsg = msg;
        break;
    }
    return ret;
}

int srs_protocol_decode_message(struct SrsCommonMessage* msg, void** ppacket,enum pkt_type* return_type)
{
    *ppacket = NULL;
    
    int ret = ERROR_SUCCESS;
    
    srs_assert(msg != NULL);
    srs_assert(msg->payload != NULL);
    srs_assert(msg->size > 0);
    
    struct SrsBuffer stream;

    // initialize the decode stream for all message,
    // it's ok for the initialize if fast and without memory copy.
    if ((ret = srs_buffer_initialize(&stream,msg->payload,msg->size)) != ERROR_SUCCESS) {
        srs_error("initialize stream failed. ret=%d", ret);
        return ret;
    }    
    srs_verbose("decode stream initialized success");
    // decode the packet.
    void* packet = NULL;
    if ((ret = srs_protocol_do_decode_message(&msg->header, &stream, &packet,return_type)) != ERROR_SUCCESS) {
        srs_freep(packet);
        return ret;
    }
    
    // set to output ppacket only when success.
    *ppacket = packet;
    
    return ret;
}

int srs_protocol_do_decode_message(struct SrsMessageHeader* header, struct SrsBuffer* stream, void** ppacket,enum pkt_type* return_type)
{
    int ret = ERROR_SUCCESS;
  
    void* packet = NULL;  
    if (srs_message_header_is_amf0_command(header) || srs_message_header_is_amf3_command(header) || srs_message_header_is_amf0_data(header) || srs_message_header_is_amf3_data(header)) {
      
        srs_verbose("start to decode AMF0/AMF3 command message.\n");
        // skip 1bytes to decode the amf3 command.    
        if (srs_message_header_is_amf3_command(header) && srs_buffer_require(stream,1)) {
            srs_verbose("skip 1bytes to decode AMF3 command");
            srs_buffer_skip(stream,1);
        }
       
        // amf0 command message.
        // need to read the command name.
        AVal command;
        if ((ret = srs_amf0_read_string(stream, &command)) != ERROR_SUCCESS) {
            srs_freep(command.av_val);
            srs_error("decode AMF0/AMF3 command name failed. ret=%d", ret);
            return ret;
        }
        srs_verbose("AMF0/AMF3 command message, command_name=%s\n", command.av_val);
        // result/error packet
        if ((strcmp(command.av_val, RTMP_AMF0_COMMAND_RESULT) == 0) || (strcmp(command.av_val, RTMP_AMF0_COMMAND_ERROR) == 0)) {
            double transactionId = 0.0;
            if ((ret = srs_amf0_read_number(stream, &transactionId)) != ERROR_SUCCESS) {
                srs_error("decode AMF0/AMF3 transcationId failed. ret=%d\n", ret);
                srs_freep(command.av_val);
                return ret;
            }
            srs_verbose("AMF0/AMF3 command id, transcationId=%.2f\n", transactionId);
            // reset stream, for header read completed.
            srs_buffer_skip(stream,(-1 *srs_buffer_pos(stream)));
            
            if (srs_message_header_is_amf3_command(header)) {
                srs_buffer_skip(stream,1);
            }

            if(transactionId==1)
               (*return_type)=CONNECT_APP_RES_TYPE;
            else if(transactionId==4)
            {
               (*return_type)=CREATESTREAMRES_TYPE;
               struct SrsCreateStreamResPacket* createstreamrespacket =(struct SrsCreateStreamResPacket*)malloc(sizeof(struct SrsCreateStreamResPacket));  
               (*ppacket)=(void*)createstreamrespacket;
               srs_freep(command.av_val);
               return srs_createstreamres_packet_decode(createstreamrespacket,stream);
           }

#if 0
             //find the call name
            if (requests.find(transactionId) == requests.end()) {
                ret = ERROR_RTMP_NO_REQUEST;
                srs_error("decode AMF0/AMF3 request failed. ret=%d", ret);
                return ret;
            }
            
            std::string request_name = requests[transactionId];
            srs_verbose("AMF0/AMF3 request parsed. request_name=%s", request_name.c_str());

            if (request_name == RTMP_AMF0_COMMAND_CONNECT) {
                srs_info("decode the AMF0/AMF3 response command(%s message).", request_name.c_str());
                *ppacket = packet = new SrsConnectAppResPacket();
                return packet->decode(stream);
            } else if (request_name == RTMP_AMF0_COMMAND_CREATE_STREAM) {
                srs_info("decode the AMF0/AMF3 response command(%s message).", request_name.c_str());
                *ppacket = packet = new SrsCreateStreamResPacket(0, 0);
                return packet->decode(stream);
            } else if (request_name == RTMP_AMF0_COMMAND_RELEASE_STREAM
                || request_name == RTMP_AMF0_COMMAND_FC_PUBLISH
                || request_name == RTMP_AMF0_COMMAND_UNPUBLISH) {
                srs_info("decode the AMF0/AMF3 response command(%s message).", request_name.c_str());
                *ppacket = packet = new SrsFMLEStartResPacket(0);
                return packet->decode(stream);
            } else {
                ret = ERROR_RTMP_NO_REQUEST;
                srs_error("decode AMF0/AMF3 request failed. "
                    "request_name=%s, transactionId=%.2f, ret=%d", 
                    request_name.c_str(), transactionId, ret);
                return ret;
                }
#endif          
        }
      
#if 0          
        // reset to zero(amf3 to 1) to restart decode.
        stream->skip(-1 * stream->pos());
        if (header.is_amf3_command()) {
            stream->skip(1);
        }
      
        // decode command object.
        if (command == RTMP_AMF0_COMMAND_CONNECT) {
            srs_info("decode the AMF0/AMF3 command(connect vhost/app message).");
            *ppacket = packet = new SrsConnectAppPacket();
            return packet->decode(stream);
        } else if(command == RTMP_AMF0_COMMAND_CREATE_STREAM) {
            srs_info("decode the AMF0/AMF3 command(createStream message).");
            *ppacket = packet = new SrsCreateStreamPacket();
            return packet->decode(stream);
        } else if(command == RTMP_AMF0_COMMAND_PLAY) {
            srs_info("decode the AMF0/AMF3 command(paly message).");
            *ppacket = packet = new SrsPlayPacket();
            return packet->decode(stream);
        } else if(command == RTMP_AMF0_COMMAND_PAUSE) {
            srs_info("decode the AMF0/AMF3 command(pause message).");
            *ppacket = packet = new SrsPausePacket();
            return packet->decode(stream);
        } else if(command == RTMP_AMF0_COMMAND_RELEASE_STREAM) {
            srs_info("decode the AMF0/AMF3 command(FMLE releaseStream message).");
            *ppacket = packet = new SrsFMLEStartPacket();
            return packet->decode(stream);
        } else if(command == RTMP_AMF0_COMMAND_FC_PUBLISH) {
            srs_info("decode the AMF0/AMF3 command(FMLE FCPublish message).");
            *ppacket = packet = new SrsFMLEStartPacket();
            return packet->decode(stream);
        } else if(command == RTMP_AMF0_COMMAND_PUBLISH) {
            srs_info("decode the AMF0/AMF3 command(publish message).");
            *ppacket = packet = new SrsPublishPacket();
            return packet->decode(stream);
        } else if(command == RTMP_AMF0_COMMAND_UNPUBLISH) {
            srs_info("decode the AMF0/AMF3 command(unpublish message).");
            *ppacket = packet = new SrsFMLEStartPacket();
            return packet->decode(stream);
        } else if(command == SRS_CONSTS_RTMP_SET_DATAFRAME || command == SRS_CONSTS_RTMP_ON_METADATA) {
            srs_info("decode the AMF0/AMF3 data(onMetaData message).");
            *ppacket = packet = new SrsOnMetaDataPacket();
            return packet->decode(stream);
        } else if(command == SRS_BW_CHECK_FINISHED
            || command == SRS_BW_CHECK_PLAYING
            || command == SRS_BW_CHECK_PUBLISHING
            || command == SRS_BW_CHECK_STARTING_PLAY
            || command == SRS_BW_CHECK_STARTING_PUBLISH
            || command == SRS_BW_CHECK_START_PLAY
            || command == SRS_BW_CHECK_START_PUBLISH
            || command == SRS_BW_CHECK_STOPPED_PLAY
            || command == SRS_BW_CHECK_STOP_PLAY
            || command == SRS_BW_CHECK_STOP_PUBLISH
            || command == SRS_BW_CHECK_STOPPED_PUBLISH
            || command == SRS_BW_CHECK_FINAL)
        {
            srs_info("decode the AMF0/AMF3 band width check message.");
            *ppacket = packet = new SrsBandwidthPacket();
            return packet->decode(stream);
        } else if (command == RTMP_AMF0_COMMAND_CLOSE_STREAM) {
            srs_info("decode the AMF0/AMF3 closeStream message.");
            *ppacket = packet = new SrsCloseStreamPacket();
            return packet->decode(stream);
        } else if (header.is_amf0_command() || header.is_amf3_command()) {
            srs_info("decode the AMF0/AMF3 call message.");
            *ppacket = packet = new SrsCallPacket();
            return packet->decode(stream);
        }
        
        // default packet to drop message.
        srs_info("drop the AMF0/AMF3 command message, command_name=%s", command.c_str());
        *ppacket = packet = new SrsPacket();
#endif
        srs_freep(command.av_val);
        return ret;
        
    } 
    else if(srs_message_header_is_user_control_message(header)) {
        srs_verbose("start to decode user control message.");
        (*return_type)=USER_CONTROL_TYPE;

      
    }else if(srs_message_header_is_window_ackledgement_size(header)){
        
        srs_verbose("start to decode set ack window size message.");
        (*return_type)=SET_WINDOW_ACKSIZE_PACKET_TYPE;
        struct SrsSetWindowAckSizePacket* setwindowacksizepacket =(struct SrsSetWindowAckSizePacket*)malloc(sizeof(struct SrsSetWindowAckSizePacket));  
        (*ppacket)=(void*)setwindowacksizepacket;
        return srs_set_window_acksize_packet_decode(setwindowacksizepacket,stream);
        
    }else if(srs_message_header_is_set_chunk_size(header)){
        
        srs_verbose("start to decode set chunk size message.");
        (*return_type)=SET_CHUNK_SIZE_TYPE;
        struct SrsSetChunkSizePacket* setchunksizepacket =(struct SrsSetChunkSizePacket*)malloc(sizeof(struct SrsSetChunkSizePacket));  
        (*ppacket)=(void*)setchunksizepacket;
        return srs_set_chunksize_packet_decode(setchunksizepacket,stream); 
        
    }else{
        
        if (!srs_message_header_is_set_peer_bandwidth(header) && !srs_message_header_is_ackledgement(header)) {
            srs_trace("drop unknown message, type=%d\n", header->message_type);
        }
        if(srs_message_header_is_set_peer_bandwidth(header))
          (*return_type)=SET_PEER_BANDWIDTH_TYPE;  
        else if(srs_message_header_is_ackledgement(header))
          (*return_type)=ACKNOWLEDGEMENT_TYPE;  
        else 
          (*return_type)=UNKNOWN_TYPE;   
    } 
    return ret;
}


int srs_protocol_send_and_free_message(struct SrsProtocol* srsprotocol, struct SrsSharedPtrMessage* msg, int stream_id)
{
    return srs_protocol_send_and_free_messages(srsprotocol,&msg, 1, stream_id);
}



int srs_protocol_send_and_free_message_2(srs_rtmp_t rtmp,struct SrsProtocol* srsprotocol,int stream_id)
{
    return srs_protocol_send_and_free_messages_2(rtmp,srsprotocol, stream_id);
}




int srs_protocol_send_and_free_messages(struct SrsProtocol* srsprotocol, struct SrsSharedPtrMessage** msgs, int nb_msgs, int stream_id)
{
    // always not NULL msg.
    int i=0;
    srs_assert(msgs);
    srs_assert(nb_msgs > 0);
    // update the stream id in header.
    for (i = 0; i < nb_msgs; i++) {
        struct SrsSharedPtrMessage* msg = msgs[i];
        
        if (!msg) {
            continue;
        }
        
        // check perfer cid and stream,
        // when one msg stream id is ok, ignore left.
        if (srs_sharedptrmessage_check(msg,stream_id)) {
            break;
        }
    }
    
    // donot use the auto free to free the msg,
    // for performance issue.
    int ret = srs_protocol_do_send_messages(srsprotocol,msgs, nb_msgs);
    i=0;
    for (i = 0; i < nb_msgs; i++) {
        struct SrsSharedPtrMessage* msg = msgs[i];
        srs_freep(msg->ptr);
        srs_freep(msg->payload);
        srs_freep(msg);
    }
    
    // donot flush when send failed
    if (ret != ERROR_SUCCESS) {
        return ret;
    }
    
    // flush messages in manual queue
   // if ((ret = manual_response_flush()) != ERROR_SUCCESS) {
   //     return ret;
    //}
    
    //print_debug_info();
    
    return ret;

}

int srs_protocol_send_and_free_messages_2(srs_rtmp_t rtmp,struct SrsProtocol* srsprotocol,int stream_id)
{

    int i=0;  

    int ret = srs_protocol_do_send_messages_2(rtmp,srsprotocol);
    if (ret != ERROR_SUCCESS) {
        return ret;
    }
    return ret;

}






int srs_protocol_send_and_free_packet(struct SrsProtocol* srsprotocol,struct SimpleSocketStream* skt,void* packet, int stream_id,enum pkt_type packetype){

    int ret = ERROR_SUCCESS;
    if ((ret = srs_protocol_do_send_and_free_packet(srsprotocol,skt,packet, stream_id,packetype)) != ERROR_SUCCESS) {
        return ret;
    }
    return ret;
}

int srs_protocol_recv_interlaced_message(struct SrsProtocol* srsprotocol,struct SrsCommonMessage** pmsg)
{
    int ret = ERROR_SUCCESS;
 
    char fmt = 0;
    int cid = 0;
    if ((ret = srs_protocol_read_basic_header(srsprotocol,&fmt, &cid)) != ERROR_SUCCESS) {
        if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
            srs_error("read basic header failed. ret=%d\n", ret);
        }
        return ret;
    }
    srs_verbose("read basic header success. fmt=%d, cid=%d\n", fmt, cid);
    // the cid must not negative.
    srs_assert(cid >= 0);
    
    // get the cached chunk stream.
    struct SrsChunkStream* chunk = NULL;
    // use chunk stream cache to get the chunk info.
    // @see https://github.com/ossrs/srs/issues/249
    if (cid < SRS_PERF_CHUNK_STREAM_CACHE) {
        // chunk stream cache hit.
        srs_verbose("cs-cache hit, cid=%d\n", cid);
        // already init, use it direclty
        chunk = srsprotocol->cs_cache[cid];    
        srs_verbose("cached chunk stream: fmt=%d, cid=%d, size=%d, message(type=%d, size=%d, time=%"PRId64", sid=%d)\n",
           chunk->fmt, chunk->cid, (chunk->msg? chunk->msg->size : 0), chunk->header.message_type, chunk->header.payload_length,
           chunk->header.timestamp, chunk->header.stream_id);     
    }
#if 0    
    else {
        // chunk stream cache miss, use map.
        if (chunk_streams.find(cid) == chunk_streams.end()) {
            chunk = chunk_streams[cid] = new SrsChunkStream(cid);
            // set the perfer cid of chunk,
            // which will copy to the message received.
            chunk->header.perfer_cid = cid;
            srs_verbose("cache new chunk stream: fmt=%d, cid=%d", fmt, cid);
        } else {
            chunk = chunk_streams[cid];
            srs_verbose("cached chunk stream: fmt=%d, cid=%d, size=%d, message(type=%d, size=%d, time=%"PRId64", sid=%d)",
                chunk->fmt, chunk->cid, (chunk->msg? chunk->msg->size : 0), chunk->header.message_type, chunk->header.payload_length,
                chunk->header.timestamp, chunk->header.stream_id);
        }
    }
#endif
    // chunk stream message header
    if ((ret = srs_protocol_read_message_header(srsprotocol,chunk, fmt)) != ERROR_SUCCESS) {
        if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
            srs_error("read message header failed. ret=%d\n", ret);
        }
        return ret;
    }
    srs_verbose("read message header success. "
            "fmt=%d, ext_time=%d, size=%d, message(type=%d, size=%d, time=%ld, sid=%d)\n", 
            fmt, chunk->extended_timestamp, (chunk->msg? chunk->msg->size : 0), chunk->header.message_type, 
            chunk->header.payload_length, chunk->header.timestamp, chunk->header.stream_id);  
    // read msg payload from chunk stream.
    
    struct SrsCommonMessage* msg = NULL;
    if ((ret = srs_protocol_read_message_payload(srsprotocol,chunk, &msg)) != ERROR_SUCCESS) {
        if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
            srs_error("read message payload failed. ret=%d\n", ret);
        }
        return ret;
    }  
    // not got an entire RTMP message, try next chunk.
    if (!msg) {
        srs_verbose("get partial message success. size=%d, message(type=%d, size=%d, time=%"PRId64", sid=%d)\n",
                (msg? msg->size : (chunk->msg? chunk->msg->size : 0)), chunk->header.message_type, chunk->header.payload_length,
                chunk->header.timestamp, chunk->header.stream_id);               
        return ret;
    }
    
    *pmsg = msg;
    srs_info("get entire message success. size=%d, message(type=%d, size=%d, time=%"PRId64", sid=%d)\n",
            (msg? msg->size : (chunk->msg? chunk->msg->size : 0)), chunk->header.message_type, chunk->header.payload_length,
            chunk->header.timestamp, chunk->header.stream_id);        
    return ret;
}

/**
* 6.1.1. Chunk Basic Header
* The Chunk Basic Header encodes the chunk stream ID and the chunk
* type(represented by fmt field in the figure below). Chunk type
* determines the format of the encoded message header. Chunk Basic
* Header field may be 1, 2, or 3 bytes, depending on the chunk stream
* ID.
* 
* The bits 0-5 (least significant) in the chunk basic header represent
* the chunk stream ID.
*
* Chunk stream IDs 2-63 can be encoded in the 1-byte version of this
* field.
*    0 1 2 3 4 5 6 7
*   +-+-+-+-+-+-+-+-+
*   |fmt|   cs id   |
*   +-+-+-+-+-+-+-+-+
*   Figure 6 Chunk basic header 1
*
* Chunk stream IDs 64-319 can be encoded in the 2-byte version of this
* field. ID is computed as (the second byte + 64).
*   0                   1
*   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*   |fmt|    0      | cs id - 64    |
*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*   Figure 7 Chunk basic header 2
*
* Chunk stream IDs 64-65599 can be encoded in the 3-byte version of
* this field. ID is computed as ((the third byte)*256 + the second byte
* + 64).
*    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3
*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*   |fmt|     1     |         cs id - 64            |
*   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*   Figure 8 Chunk basic header 3
*
* cs id: 6 bits
* fmt: 2 bits
* cs id - 64: 8 or 16 bits
* 
* Chunk stream IDs with values 64-319 could be represented by both 2-
* byte version and 3-byte version of this field.
*/
int srs_protocol_read_basic_header(struct SrsProtocol* srsprotocol,char* fmt, int* cid)
{
    int ret = ERROR_SUCCESS;
 

    if ((ret =  srs_fast_stream_grow(srsprotocol->in_buffer,srsprotocol->skt,1)) != ERROR_SUCCESS) {
        if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
            srs_error("read 1bytes basic header failed. required_size=%d, ret=%d", 1, ret);
        }
        return ret;
    }
    
    (*fmt) = srs_fast_stream_read_1byte(srsprotocol->in_buffer);
    (*cid) = (*fmt) & 0x3f;
    (*fmt) = ((*fmt) >> 6) & 0x03;
    
    // 2-63, 1B chunk header
    if ((*cid) > 1) {
        srs_verbose("basic header parsed. fmt=%d, cid=%d", (*fmt), (*cid));
        return ret;
    }

    // 64-319, 2B chunk header
    if ((*cid) == 0) {
        if ((ret = srs_fast_stream_grow(srsprotocol->in_buffer,srsprotocol->skt,1)) != ERROR_SUCCESS) {
            if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
                srs_error("read 2bytes basic header failed. required_size=%d, ret=%d", 1, ret);
            }
            return ret;
        }
        
        (*cid) = 64;
        (*cid) += (uint8_t)srs_fast_stream_read_1byte(srsprotocol->in_buffer);
        srs_verbose("2bytes basic header parsed. fmt=%d, cid=%d", (*fmt) , (*cid));
    // 64-65599, 3B chunk header
    } else if ((*cid) == 1) {
        if ((ret = srs_fast_stream_grow(srsprotocol->in_buffer,srsprotocol->skt,2)) != ERROR_SUCCESS) {
            if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
                srs_error("read 3bytes basic header failed. required_size=%d, ret=%d", 2, ret);
            }
            return ret;
        }
        
        (*cid) = 64;
        (*cid) += (uint8_t)srs_fast_stream_read_1byte(srsprotocol->in_buffer);
        (*cid) += ((uint8_t)srs_fast_stream_read_1byte(srsprotocol->in_buffer)) * 256;
        srs_verbose("3bytes basic header parsed. fmt=%d, cid=%d", (*fmt), (*cid));
    } else {
        srs_error("invalid path, impossible basic header.");
        srs_assert(false);
    }
  
    return ret;
}

/**
* parse the message header.
*   3bytes: timestamp delta,    fmt=0,1,2
*   3bytes: payload length,     fmt=0,1
*   1bytes: message type,       fmt=0,1
*   4bytes: stream id,          fmt=0
* where:
*   fmt=0, 0x0X
*   fmt=1, 0x4X
*   fmt=2, 0x8X
*   fmt=3, 0xCX
*/

int srs_protocol_read_message_header(struct SrsProtocol* srsprotocol,struct SrsChunkStream* chunk, char fmt)
{
    int ret = ERROR_SUCCESS;

    /**
    * we should not assert anything about fmt, for the first packet.
    * (when first packet, the chunk->msg is NULL).
    * the fmt maybe 0/1/2/3, the FMLE will send a 0xC4 for some audio packet.
    * the previous packet is:
    *     04                // fmt=0, cid=4
    *     00 00 1a          // timestamp=26
    *     00 00 9d          // payload_length=157
    *     08                // message_type=8(audio)
    *     01 00 00 00       // stream_id=1
    * the current packet maybe:
    *     c4             // fmt=3, cid=4
    * it's ok, for the packet is audio, and timestamp delta is 26.
    * the current packet must be parsed as:
    *     fmt=0, cid=4
    *     timestamp=26+26=52
    *     payload_length=157
    *     message_type=8(audio)
    *     stream_id=1
    * so we must update the timestamp even fmt=3 for first packet.
    */
    // fresh packet used to update the timestamp even fmt=3 for first packet.
    // fresh packet always means the chunk is the first one of message.
    bool is_first_chunk_of_msg = !chunk->msg;
    
    // but, we can ensure that when a chunk stream is fresh, 
    // the fmt must be 0, a new stream.
    if (chunk->msg_count == 0 && fmt != RTMP_FMT_TYPE0) {
        // for librtmp, if ping, it will send a fresh stream with fmt=1,
        // 0x42             where: fmt=1, cid=2, protocol contorl user-control message
        // 0x00 0x00 0x00   where: timestamp=0
        // 0x00 0x00 0x06   where: payload_length=6
        // 0x04             where: message_type=4(protocol control user-control message)
        // 0x00 0x06            where: event Ping(0x06)
        // 0x00 0x00 0x0d 0x0f  where: event data 4bytes ping timestamp.
        // @see: https://github.com/ossrs/srs/issues/98
        if (chunk->cid == RTMP_CID_ProtocolControl && fmt == RTMP_FMT_TYPE1) {
            srs_warn("accept cid=2, fmt=1 to make librtmp happy.\n");
        } else {
            // must be a RTMP protocol level error.
            ret = ERROR_RTMP_CHUNK_START;
            srs_error("chunk stream is fresh, fmt must be %d, actual is %d. cid=%d, ret=%d\n", 
                RTMP_FMT_TYPE0, fmt, chunk->cid, ret);
            return ret;
        }
    }

    // when exists cache msg, means got an partial message,
    // the fmt must not be type0 which means new message.
    if (chunk->msg && fmt == RTMP_FMT_TYPE0) {
        ret = ERROR_RTMP_CHUNK_START;
        srs_error("chunk stream exists, \n"
            "fmt must not be %d, actual is %d. ret=%d\n", RTMP_FMT_TYPE0, fmt, ret);
        return ret;
    }
    // create msg when new chunk stream start
    if (!chunk->msg) {
        chunk->msg=(struct SrsCommonMessage*)malloc(sizeof(struct SrsCommonMessage)); 
        memset(chunk->msg,0,sizeof(struct SrsCommonMessage));
        srs_verbose("create message for new chunk, fmt=%d, cid=%d\n", fmt, chunk->cid);
    }
    // read message header from socket to buffer.
    static char mh_sizes[] = {11, 7, 3, 0};
    int mh_size = mh_sizes[(int)fmt];
    srs_verbose("calc chunk message header size. fmt=%d, mh_size=%d\n", fmt, mh_size);
 
    if (mh_size > 0 && (ret = srs_fast_stream_grow(srsprotocol->in_buffer,srsprotocol->skt,mh_size)) != ERROR_SUCCESS) {
        if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
            srs_error("read %dbytes message header failed. ret=%d\n", mh_size, ret);
        }
        return ret;
    }
    
    /**
    * parse the message header.
    *   3bytes: timestamp delta,    fmt=0,1,2
    *   3bytes: payload length,     fmt=0,1
    *   1bytes: message type,       fmt=0,1
    *   4bytes: stream id,          fmt=0
    * where:
    *   fmt=0, 0x0X
    *   fmt=1, 0x4X
    *   fmt=2, 0x8X
    *   fmt=3, 0xCX
    */
    // see also: ngx_rtmp_recv
    if (fmt <= RTMP_FMT_TYPE2) {
        char* p = srs_fast_stream_read_slice(srsprotocol->in_buffer,mh_size);
        char* pp = (char*)&chunk->header.timestamp_delta;
        pp[2] = *p++;
        pp[1] = *p++;
        pp[0] = *p++;
        pp[3] = 0;
        
        // fmt: 0
        // timestamp: 3 bytes
        // If the timestamp is greater than or equal to 16777215
        // (hexadecimal 0x00ffffff), this value MUST be 16777215, and the
        // 'extended timestamp header' MUST be present. Otherwise, this value
        // SHOULD be the entire timestamp.
        //
        // fmt: 1 or 2
        // timestamp delta: 3 bytes
        // If the delta is greater than or equal to 16777215 (hexadecimal
        // 0x00ffffff), this value MUST be 16777215, and the 'extended
        // timestamp header' MUST be present. Otherwise, this value SHOULD be
        // the entire delta.
        chunk->extended_timestamp = (chunk->header.timestamp_delta >= RTMP_EXTENDED_TIMESTAMP);
        if (!chunk->extended_timestamp) {
            // Extended timestamp: 0 or 4 bytes
            // This field MUST be sent when the normal timsestamp is set to
            // 0xffffff, it MUST NOT be sent if the normal timestamp is set to
            // anything else. So for values less than 0xffffff the normal
            // timestamp field SHOULD be used in which case the extended timestamp
            // MUST NOT be present. For values greater than or equal to 0xffffff
            // the normal timestamp field MUST NOT be used and MUST be set to
            // 0xffffff and the extended timestamp MUST be sent.
            if (fmt == RTMP_FMT_TYPE0) {
                // 6.1.2.1. Type 0
                // For a type-0 chunk, the absolute timestamp of the message is sent
                // here.
                chunk->header.timestamp = chunk->header.timestamp_delta;
            } else {
                // 6.1.2.2. Type 1
                // 6.1.2.3. Type 2
                // For a type-1 or type-2 chunk, the difference between the previous
                // chunk's timestamp and the current chunk's timestamp is sent here.
                chunk->header.timestamp += chunk->header.timestamp_delta;
            }
        }
        
        if (fmt <= RTMP_FMT_TYPE1) {
            int32_t payload_length = 0;
            pp = (char*)&payload_length;
            pp[2] = *p++;
            pp[1] = *p++;
            pp[0] = *p++;
            pp[3] = 0;
            
            // for a message, if msg exists in cache, the size must not changed.
            // always use the actual msg size to compare, for the cache payload length can changed,
            // for the fmt type1(stream_id not changed), user can change the payload 
            // length(it's not allowed in the continue chunks).
            if (!is_first_chunk_of_msg && chunk->header.payload_length != payload_length) {
                ret = ERROR_RTMP_PACKET_SIZE;
                srs_error("msg exists in chunk cache, "
                    "size=%d cannot change to %d, ret=%d\n", 
                    chunk->header.payload_length, payload_length, ret);
                return ret;
            }
            
            chunk->header.payload_length = payload_length;
            chunk->header.message_type = *p++;
            
            if (fmt == RTMP_FMT_TYPE0) {
                pp = (char*)&chunk->header.stream_id;
                pp[0] = *p++;
                pp[1] = *p++;
                pp[2] = *p++;
                pp[3] = *p++;
                srs_verbose("header read completed. fmt=%d, mh_size=%d, ext_time=%d, time=%"PRId64", payload=%d, type=%d, sid=%d\n", 
                    fmt, mh_size, chunk->extended_timestamp, chunk->header.timestamp, chunk->header.payload_length, 
                    chunk->header.message_type, chunk->header.stream_id);
            } else {
                srs_verbose("header read completed. fmt=%d, mh_size=%d, ext_time=%d, time=%"PRId64", payload=%d, type=%d\n", 
                    fmt, mh_size, chunk->extended_timestamp, chunk->header.timestamp, chunk->header.payload_length, 
                    chunk->header.message_type);
            }
        } else {
            srs_verbose("header read completed. fmt=%d, mh_size=%d, ext_time=%d, time=%"PRId64"\n", 
                fmt, mh_size, chunk->extended_timestamp, chunk->header.timestamp);
        }
    } else {
        // update the timestamp even fmt=3 for first chunk packet
        if (is_first_chunk_of_msg && !chunk->extended_timestamp) {
            chunk->header.timestamp += chunk->header.timestamp_delta;
        }
        srs_verbose("header read completed. fmt=%d, size=%d, ext_time=%d\n", 
            fmt, mh_size, chunk->extended_timestamp);
    }
    
    // read extended-timestamp
    if (chunk->extended_timestamp) {
        mh_size += 4;
        srs_verbose("read header ext time. fmt=%d, ext_time=%d, mh_size=%d\n", fmt, chunk->extended_timestamp, mh_size);
        if ((ret = srs_fast_stream_grow(srsprotocol->in_buffer,srsprotocol->skt,4)) != ERROR_SUCCESS) {
            if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
                srs_error("read %dbytes message header failed. required_size=%d, ret=%d\n", mh_size, 4, ret);
            }
            return ret;
        }
        // the ptr to the slice maybe invalid when grow()
        // reset the p to get 4bytes slice.
        char* p = srs_fast_stream_read_slice(srsprotocol->in_buffer,4);
        
        uint32_t timestamp = 0x00;
        char* pp = (char*)&timestamp;
        pp[3] = *p++;
        pp[2] = *p++;
        pp[1] = *p++;
        pp[0] = *p++;

        // always use 31bits timestamp, for some server may use 32bits extended timestamp.
        // @see https://github.com/ossrs/srs/issues/111
        timestamp &= 0x7fffffff;
        
        /**
        * RTMP specification and ffmpeg/librtmp is false,
        * but, adobe changed the specification, so flash/FMLE/FMS always true.
        * default to true to support flash/FMLE/FMS.
        * 
        * ffmpeg/librtmp may donot send this filed, need to detect the value.
        * @see also: http://blog.csdn.net/win_lin/article/details/13363699
        * compare to the chunk timestamp, which is set by chunk message header
        * type 0,1 or 2.
        *
        * @remark, nginx send the extended-timestamp in sequence-header,
        * and timestamp delta in continue C1 chunks, and so compatible with ffmpeg,
        * that is, there is no continue chunks and extended-timestamp in nginx-rtmp.
        *
        * @remark, srs always send the extended-timestamp, to keep simple,
        * and compatible with adobe products.
        */
        uint32_t chunk_timestamp = (uint32_t)chunk->header.timestamp;
        
        /**
        * if chunk_timestamp<=0, the chunk previous packet has no extended-timestamp,
        * always use the extended timestamp.
        */
        /**
        * about the is_first_chunk_of_msg.
        * @remark, for the first chunk of message, always use the extended timestamp.
        */
        if (!is_first_chunk_of_msg && chunk_timestamp > 0 && chunk_timestamp != timestamp) {
            mh_size -= 4;
            srs_fast_stream_skip(srsprotocol->in_buffer,-4);
            srs_info("no 4bytes extended timestamp in the continued chunk\n");
        } else {
            chunk->header.timestamp = timestamp;
        }
        srs_verbose("header read ext_time completed. time=%"PRId64"\n", chunk->header.timestamp);
    }
    
    // the extended-timestamp must be unsigned-int,
    //         24bits timestamp: 0xffffff = 16777215ms = 16777.215s = 4.66h
    //         32bits timestamp: 0xffffffff = 4294967295ms = 4294967.295s = 1193.046h = 49.71d
    // because the rtmp protocol says the 32bits timestamp is about "50 days":
    //         3. Byte Order, Alignment, and Time Format
    //                Because timestamps are generally only 32 bits long, they will roll
    //                over after fewer than 50 days.
    // 
    // but, its sample says the timestamp is 31bits:
    //         An application could assume, for example, that all 
    //        adjacent timestamps are within 2^31 milliseconds of each other, so
    //        10000 comes after 4000000000, while 3000000000 comes before
    //        4000000000.
    // and flv specification says timestamp is 31bits:
    //        Extension of the Timestamp field to form a SI32 value. This
    //        field represents the upper 8 bits, while the previous
    //        Timestamp field represents the lower 24 bits of the time in
    //        milliseconds.
    // in a word, 31bits timestamp is ok.
    // convert extended timestamp to 31bits.
    chunk->header.timestamp &= 0x7fffffff;
    
    // valid message, the payload_length is 24bits,
    // so it should never be negative.
    srs_assert(chunk->header.payload_length >= 0);
    
    // copy header to msg  ??
    //chunk->msg->header = chunk->header;
    memcpy(&(chunk->msg->header), &(chunk->header), sizeof(chunk->header));
    // increase the msg count, the chunk stream can accept fmt=1/2/3 message now.
    chunk->msg_count++;
    return ret;

}

int srs_protocol_read_message_payload(struct SrsProtocol* srsprotocol,struct SrsChunkStream* chunk,struct  SrsCommonMessage** pmsg)
{
    int ret = ERROR_SUCCESS;

 
    // empty message
    if (chunk->header.payload_length <= 0) {
        srs_trace("get an empty RTMP "
                "message(type=%d, size=%d, time=%"PRId64", sid=%d)\n", chunk->header.message_type, 
                chunk->header.payload_length, chunk->header.timestamp, chunk->header.stream_id);
        
        *pmsg = chunk->msg;
        chunk->msg = NULL;
                
        return ret;
    }
    srs_assert(chunk->header.payload_length > 0);
    // the chunk payload size.
    int payload_size = chunk->header.payload_length - chunk->msg->size;
    payload_size = srs_min(payload_size,srsprotocol->in_chunk_size);
    srs_verbose("chunk payload size is %d, message_size=%d, received_size=%d, in_chunk_size=%d\n", 
        payload_size, chunk->header.payload_length, chunk->msg->size, srsprotocol->in_chunk_size);
    // create msg payload if not initialized
    if (!chunk->msg->payload) {
       srs_common_message_create_payload(chunk->msg,chunk->header.payload_length);

    }
    // read payload to buffer
    if ((ret = srs_fast_stream_grow(srsprotocol->in_buffer,srsprotocol->skt,payload_size)) != ERROR_SUCCESS) {
        if (ret != ERROR_SOCKET_TIMEOUT && !srs_is_client_gracefully_close(ret)) {
            srs_error("read payload failed. required_size=%d, ret=%d", payload_size, ret);
        }
        return ret;
    }
    
    memcpy(chunk->msg->payload + chunk->msg->size, srs_fast_stream_read_slice(srsprotocol->in_buffer,payload_size), payload_size);
    chunk->msg->size += payload_size;
    
    //srs_verbose("chunk payload read completed. payload_size=%d\n", payload_size);
    // got entire RTMP message?
    if (chunk->header.payload_length == chunk->msg->size) {
        *pmsg = chunk->msg;
        chunk->msg = NULL;
        srs_verbose("get entire RTMP message(type=%d, size=%d, time=%ld, sid=%d)\n", 
                chunk->header.message_type, chunk->header.payload_length, 
                chunk->header.timestamp, chunk->header.stream_id);             
        return ret;
    }
    
    srs_verbose("get partial RTMP message(type=%d, size=%d, time=%ld, sid=%d), partial size=%d\n", 
            chunk->header.message_type, chunk->header.payload_length, 
            chunk->header.timestamp, chunk->header.stream_id,
            chunk->msg->size);   
    return ret;
}


int srs_protocol_on_recv_message(struct SrsProtocol* srsprotocol,struct SrsCommonMessage* msg)
{
    int ret = ERROR_SUCCESS;

    srs_assert(msg != NULL);
        
    // try to response acknowledgement
    if ((ret = srs_protocol_response_acknowledgement_message(srsprotocol)) != ERROR_SUCCESS) {
        return ret;
    }
    void* packet = NULL;
    enum pkt_type return_type;
    switch (msg->header.message_type) {
        
        case RTMP_MSG_SetChunkSize:
        case RTMP_MSG_UserControlMessage:
        case RTMP_MSG_WindowAcknowledgementSize:
            if ((ret = srs_protocol_decode_message(msg, &packet,&return_type)) != ERROR_SUCCESS) {
                srs_error("decode packet from message payload failed. ret=%d\n", ret);
                return ret;
            }
#if 1  //only for debug
//            if(RTMP_MSG_SetChunkSize==msg->header.message_type)
//                srsprotocol->in_chunk_size=4000;
#endif            
            srs_verbose("decode packet from message payload success.\n");
            break;
        case RTMP_MSG_VideoMessage:
        case RTMP_MSG_AudioMessage:
           // print_debug_info();
        default:
            return ret;
    }

   
    srs_assert(packet);
    
    // always free the packet.
    //SrsAutoFree(SrsPacket, packet);
 #if 1   
    switch (msg->header.message_type) {
        case RTMP_MSG_WindowAcknowledgementSize: {
            struct SrsSetWindowAckSizePacket* pkt = (struct SrsSetWindowAckSizePacket*)(packet);
            srs_assert(pkt != NULL);
            
            if (pkt->ackowledgement_window_size > 0) {
                srsprotocol->in_ack_size.window = (uint32_t)pkt->ackowledgement_window_size;
                // @remark, we ignore this message, for user noneed to care.
                // but it's important for dev, for client/server will block if required 
                // ack msg not arrived.
                srs_info("set ack window size to %d\n", pkt->ackowledgement_window_size);
            } else {
                srs_warn("ignored. set ack window size is %d\n", pkt->ackowledgement_window_size);
            }
            break;
        }
        case RTMP_MSG_SetChunkSize: {
            
            struct SrsSetChunkSizePacket* pkt = (struct SrsSetChunkSizePacket*)(packet);
            srs_assert(pkt != NULL);

            // for some server, the actual chunk size can greater than the max value(65536),
            // so we just warning the invalid chunk size, and actually use it is ok,
            // @see: https://github.com/ossrs/srs/issues/160
            if (pkt->chunk_size < SRS_CONSTS_RTMP_MIN_CHUNK_SIZE 
                || pkt->chunk_size > SRS_CONSTS_RTMP_MAX_CHUNK_SIZE) 
            {
                srs_warn("accept chunk=%d, should in [%d, %d], please see #160\n",
                    pkt->chunk_size, SRS_CONSTS_RTMP_MIN_CHUNK_SIZE,  SRS_CONSTS_RTMP_MAX_CHUNK_SIZE);
            }

            // @see: https://github.com/ossrs/srs/issues/541
            if (pkt->chunk_size < SRS_CONSTS_RTMP_MIN_CHUNK_SIZE) {
                ret = ERROR_RTMP_CHUNK_SIZE;
                srs_error("chunk size should be %d+, value=%d. ret=%d\n",
                    SRS_CONSTS_RTMP_MIN_CHUNK_SIZE, pkt->chunk_size, ret);
                return ret;
            }
            
            srsprotocol->in_chunk_size = pkt->chunk_size;
            srs_info("in.chunk=%d\n", pkt->chunk_size);
            break;
        }
#if 0        
        case RTMP_MSG_UserControlMessage: {
            struct SrsUserControlPacket* pkt = (struct SrsUserControlPacket*)(packet);     
            srs_assert(pkt != NULL);
            
            if (pkt->event_type == SrcPCUCSetBufferLength) {
                srsprotocol->in_buffer_length = pkt->extra_data;
                srs_info("buffer=%d, in.ack=%d, out.ack=%d, in.chunk=%d, out.chunk=%d", pkt->extra_data,
                    srsprotocol->in_ack_size.window, srsprotocol->out_ack_size.window, srsprotocol->in_chunk_size, srsprotocol->out_chunk_size);
            }
            if (pkt->event_type == SrcPCUCPingRequest) {
                //if ((ret = response_ping_message(pkt->event_data)) != ERROR_SUCCESS) {
                //    return ret;
                //}
            }
            break;
        }
#endif        
        default:
            break;
    }  
#endif
    srs_freep(packet);
    return ret;
}


int srs_protocol_do_send_messages(struct SrsProtocol* srsprotocol, struct SrsSharedPtrMessage** msgs, int nb_msgs)
{
    int ret = ERROR_SUCCESS;
   
#ifdef SRS_PERF_COMPLEX_SEND
    int iov_index = 0;
    iovec* iovs = out_iovs + iov_index;
    
    int c0c3_cache_index = 0;
    char* c0c3_cache = out_c0c3_caches + c0c3_cache_index;

    // try to send use the c0c3 header cache,
    // if cache is consumed, try another loop.
    for (int i = 0; i < nb_msgs; i++) {
        SrsSharedPtrMessage* msg = msgs[i];
        
        if (!msg) {
            continue;
        }
    
        // ignore empty message.
        if (!msg->payload || msg->size <= 0) {
            srs_info("ignore empty message.\n");
            continue;
        }
    
        // p set to current write position,
        // it's ok when payload is NULL and size is 0.
        char* p = msg->payload;
        char* pend = msg->payload + msg->size;
        
        // always write the header event payload is empty.
        while (p < pend) {
            // always has header
            int nb_cache = SRS_CONSTS_C0C3_HEADERS_MAX - c0c3_cache_index;
            int nbh = msg->chunk_header(c0c3_cache, nb_cache, p == msg->payload);
            srs_assert(nbh > 0);
            
            // header iov
            iovs[0].iov_base = c0c3_cache;
            iovs[0].iov_len = nbh;
            
            // payload iov
            int payload_size = srs_min(out_chunk_size, (int)(pend - p));
            iovs[1].iov_base = p;
            iovs[1].iov_len = payload_size;
            
            // consume sendout bytes.
            p += payload_size;
            
            // realloc the iovs if exceed,
            // for we donot know how many messges maybe to send entirely,
            // we just alloc the iovs, it's ok.
            if (iov_index >= nb_out_iovs - 2) {
                srs_warn("resize iovs %d => %d, max_msgs=%d\n", 
                    nb_out_iovs, nb_out_iovs + SRS_CONSTS_IOVS_MAX, 
                    SRS_PERF_MW_MSGS);
                    
                nb_out_iovs += SRS_CONSTS_IOVS_MAX;
                int realloc_size = sizeof(iovec) * nb_out_iovs;
                out_iovs = (iovec*)realloc(out_iovs, realloc_size);
            }
            
            // to next pair of iovs
            iov_index += 2;
            iovs = out_iovs + iov_index;

            // to next c0c3 header cache
            c0c3_cache_index += nbh;
            c0c3_cache = out_c0c3_caches + c0c3_cache_index;
            
            // the cache header should never be realloc again,
            // for the ptr is set to iovs, so we just warn user to set larger
            // and use another loop to send again.
            int c0c3_left = SRS_CONSTS_C0C3_HEADERS_MAX - c0c3_cache_index;
            if (c0c3_left < SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE) {
                // only warn once for a connection.
                if (!warned_c0c3_cache_dry) {
                    srs_warn("c0c3 cache header too small, recoment to %d\n", 
                        SRS_CONSTS_C0C3_HEADERS_MAX + SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE);
                    warned_c0c3_cache_dry = true;
                }
                
                // when c0c3 cache dry,
                // sendout all messages and reset the cache, then send again.
                if ((ret = do_iovs_send(out_iovs, iov_index)) != ERROR_SUCCESS) {
                    return ret;
                }
    
                // reset caches, while these cache ensure 
                // atleast we can sendout a chunk.
                iov_index = 0;
                iovs = out_iovs + iov_index;
                
                c0c3_cache_index = 0;
                c0c3_cache = out_c0c3_caches + c0c3_cache_index;
            }
        }
    }
    
    // maybe the iovs already sendout when c0c3 cache dry,
    // so just ignore when no iovs to send.
    if (iov_index <= 0) {
        return ret;
    }
    srs_info("mw %d msgs in %d iovs, max_msgs=%d, nb_out_iovs=%d\n",
        nb_msgs, iov_index, SRS_PERF_MW_MSGS, nb_out_iovs);

    return do_iovs_send(out_iovs, iov_index);
#else
    // try to send use the c0c3 header cache,
    // if cache is consumed, try another loop.
    int i=0;
    struct SrsBlockSyncSocket* skt = (struct SrsBlockSyncSocket*)srsprotocol->skt->io;  

    xSemaphoreTake(skt->mutex, portMAX_DELAY); 
    for (i = 0; i < nb_msgs; i++) {
        struct SrsSharedPtrMessage* msg = msgs[i];
        
        if (!msg) {
             srs_info("!msg!msg!msg!msg!msg!msg\n");
            continue;
        }
    
        // ignore empty message.
        if (!msg->payload || msg->size <= 0) {
            srs_info("ignore empty message.\n");
            continue;
        }
    
        // p set to current write position,
        // it's ok when payload is NULL and size is 0.
        char* p = msg->payload;
        char* pend = msg->payload + msg->size;
   
         
        
        // always write the header event payload is empty.
        while (p < pend) {
            // for simple send, send each chunk one by one
            struct iovec* iovs = srsprotocol->out_iovs;
            char* c0c3_cache = srsprotocol->out_c0c3_caches;
            int nb_cache = SRS_CONSTS_C0C3_HEADERS_MAX;
            
            // always has header
            int nbh = srs_sharedptrmessage_chunk_header(msg,c0c3_cache, nb_cache, p == msg->payload);
            srs_assert(nbh > 0);
            
            // header iov
            iovs[0].iov_base = c0c3_cache;
            iovs[0].iov_len = nbh;
            hexdump(c0c3_cache, nbh);
            // payload iov
            int payload_size = srs_min(srsprotocol->out_chunk_size, pend - p);
            iovs[1].iov_base = p;
            iovs[1].iov_len = payload_size;
            //srs_info("p=0x%2x,payload_size=%d\n",p,payload_size);
            hexdump(p,30);
            // consume sendout bytes.
            p += payload_size;  
            if ((ret = srs_simple_socket_writev(srsprotocol->skt,iovs, 2, NULL)) != ERROR_SUCCESS) {
                //if (!srs_is_client_gracefully_close(ret)) {
                //    srs_error("send packet with writev failed. ret=%d", ret);
                //}
                srs_info("nonoononononoonignore empty message.\n");
                xSemaphoreGive(skt->mutex);
                return ret;
            }
        }
        xSemaphoreGive(skt->mutex);
    }
    
    return ret;
#endif
}


int srs_protocol_do_send_messages_2(srs_rtmp_t rtmp,struct SrsProtocol* srsprotocol)
{
    int ret = ERROR_SUCCESS;
  
#if 1
    // try to send use the c0c3 header cache,
    // if cache is consumed, try another loop.
    int i=0;
    struct SrsBlockSyncSocket* skt = (struct SrsBlockSyncSocket*)srsprotocol->skt->io;
    struct Context* context = (struct Context*)rtmp;

    xSemaphoreTake(skt->mutex, portMAX_DELAY); 

    char* p = context->video_header.payload;
    char* pend = context->video_header.payload+context->video_header.size; 

         
        
        // always write the header event payload is empty.
        while (p < pend) {
            // for simple send, send each chunk one by one
            struct iovec* iovs = srsprotocol->out_iovs;
            char* c0c3_cache = srsprotocol->out_c0c3_caches;
            int nb_cache = SRS_CONSTS_C0C3_HEADERS_MAX;
            
            // always has header
            int nbh = srs_sharedptrmessage_chunk_header_2(rtmp,c0c3_cache, nb_cache, p == context->video_header.payload);
            srs_assert(nbh > 0);
            
            // header iov
            iovs[0].iov_base = c0c3_cache;
            iovs[0].iov_len = nbh;
            //hexdump(c0c3_cache, nbh);
            // payload iov
            int payload_size = srs_min(srsprotocol->out_chunk_size, pend - p);
            iovs[1].iov_base = p;
            iovs[1].iov_len = payload_size;
            //srs_info("p=0x%2x,payload_size=%d\n",p,payload_size);
            //hexdump(p,30);
            // consume sendout bytes.
            p += payload_size;  
            if ((ret = srs_simple_socket_writev(srsprotocol->skt,iovs, 2, NULL)) != ERROR_SUCCESS) {
                //if (!srs_is_client_gracefully_close(ret)) {
                //    srs_error("send packet with writev failed. ret=%d", ret);
                //}
                srs_info("nonoononononoonignore empty message.\n");
                xSemaphoreGive(skt->mutex);
                return ret;
            }
        }
        xSemaphoreGive(skt->mutex);

    return ret;
#endif
}




int srs_protocol_do_iovs_send(struct SrsProtocol* srsprotocol,struct iovec* iovs, int size)
{
 //   return srs_write_large_iovs(skt, iovs, size);
}



static int srs_protocol_encode(void* packet,enum pkt_type packetype,int* psize, char** ppayload)
{
    int ret = ERROR_SUCCESS;    
    char* payload = NULL;
    int size=256;
    
    if(packetype==CONNECT_APP_PACKET_TYPE)
             size=srs_connect_app_packet_get_size(packet);
    else if(packetype==SET_WINDOW_ACKSIZE_PACKET_TYPE)
             size=srs_set_window_acksize_packet_get_size(packet);
    else if(packetype==RELEASE_STREAM_PACKET_TYPE)
             size=srs_release_stream_packet_get_size(packet);
    else if(packetype==CREATESTREAM_PACKET_TYPE)
             size=srs_createstream_packet_get_size(packet);
    else if(packetype==PUBLISH_PACKET_TYPE)
             size=srs_publish_packet_get_size(packet);
    else if(packetype==FCPUBLISH_PACKET_TYPE)
            size=srs_fcpublish_packet_get_size(packet);
    else if(packetype==SET_CHUNK_SIZE_TYPE)
            size=srs_set_chunksize_packet_get_size(packet);

    struct SrsBuffer stream;
    if (size > 0) {
        payload=(char*)malloc(size);
        memset(payload,0,size);
        if ((ret = srs_buffer_initialize(&stream,payload, size)) != ERROR_SUCCESS) {
            srs_error("initialize the stream failed. ret=%d", ret);
            srs_freep(payload);
            return ret;
        }
    }
    if(packetype==CONNECT_APP_PACKET_TYPE){
        
        struct SrsConnectAppPacket* connectapppacket = (struct SrsConnectAppPacket*)packet;  
        if ((ret = srs_connect_app_packet_encode_packet(connectapppacket,&stream)) != ERROR_SUCCESS) {
            srs_error("encode the packet failed. ret=%d", ret);
            srs_freep(payload);
            return ret;
        }
        
    }else if(packetype==SET_WINDOW_ACKSIZE_PACKET_TYPE){
    
        struct SrsSetWindowAckSizePacket* setwindowacksizepacket = (struct SrsSetWindowAckSizePacket*)packet;  
        if ((ret = srs_set_window_acksize_packet_encode_packet(setwindowacksizepacket,&stream)) != ERROR_SUCCESS) {
            srs_error("encode the packet failed. ret=%d", ret);
            srs_freep(payload);
            return ret;
        }
        
    }else if(packetype==CREATESTREAM_PACKET_TYPE){
        struct SrsCreateStreamPacket* createstreampacket = (struct SrsCreateStreamPacket*)packet;  
        if ((ret = srs_createstream_packet_encode_packet(createstreampacket,&stream)) != ERROR_SUCCESS) {
            srs_error("encode the packet failed. ret=%d", ret);
            srs_freep(payload);
            return ret;
        }
        
    }else if(packetype==PUBLISH_PACKET_TYPE)
    {
        struct SrsPublishPacket* publishpacket = (struct SrsPublishPacket*)packet;  
        if ((ret = srs_publish_packet_encode_packet(publishpacket,&stream)) != ERROR_SUCCESS) {
            srs_error("encode the packet failed. ret=%d", ret);
            srs_freep(payload);
            return ret;
         }
        
    }else if(packetype==RELEASE_STREAM_PACKET_TYPE){
    

        struct SrsReleaseStreamPacket* releasestreampacket = (struct SrsReleaseStreamPacket*)packet;  
 ;
        if ((ret = srs_release_stream_packet_encode_packet(releasestreampacket,&stream)) != ERROR_SUCCESS) {
            srs_error("encode the packet failed. ret=%d", ret);
            srs_freep(payload);
            return ret;
         }
        
    }else if(packetype==FCPUBLISH_PACKET_TYPE)
    {
        struct SrsFcPublishPacket* srsfcpublishpacket = (struct SrsFcPublishPacket*)packet;  
        if ((ret = srs_fcpublish_packet_encode_packet(srsfcpublishpacket,&stream)) != ERROR_SUCCESS) {
            srs_error("encode the packet failed. ret=%d", ret);
            srs_freep(payload);
            return ret;
         }
        
    }else if(packetype==SET_CHUNK_SIZE_TYPE)
    {
        struct SrsSetChunkSizePacket* srssetchunksizepacket = (struct SrsSetChunkSizePacket*)packet;  
        if ((ret = srs_set_chunksize_packet_encode_packet(srssetchunksizepacket,&stream)) != ERROR_SUCCESS) {
            srs_error("encode the packet failed. ret=%d", ret);
            srs_freep(payload);
            return ret;
        }
    }
    (*psize) = size;
    (*ppayload) = payload;
    srs_verbose("encode the packet success. size=%d", size);
    return ret;
}

int srs_protocol_response_acknowledgement_message(struct SrsProtocol* srsprotocol)
{
    int ret = ERROR_SUCCESS;
    
  
    if (srsprotocol->in_ack_size.window <= 0) {
        return ret;
    }
    
    // ignore when delta bytes not exceed half of window(ack size).
    uint32_t delta = (uint32_t)(srs_simple_socket_get_recv_bytes(srsprotocol->skt) - (srsprotocol->in_ack_size.nb_recv_bytes));
    if (delta < srsprotocol->in_ack_size.window / 2) {
        return ret;
    }
    srsprotocol->in_ack_size.nb_recv_bytes = srs_simple_socket_get_recv_bytes(srsprotocol->skt);
    
    // when the sequence number overflow, reset it.
    uint32_t sequence_number = srsprotocol->in_ack_size.sequence_number + delta;
    if (sequence_number > 0xf0000000) {
        sequence_number = delta;
    }
    srsprotocol->in_ack_size.sequence_number = sequence_number;
   
    struct SrsAcknowledgementPacket* pkt = (struct SrsAcknowledgementPacket*)malloc(sizeof(struct SrsAcknowledgementPacket));
    memset(pkt,0,sizeof(struct SrsAcknowledgementPacket));
    pkt->sequence_number = sequence_number;
    
    // cache the message and use flush to send.
    if (!srsprotocol->auto_response_when_recv) {
        //manual_response_queue.push_back(pkt);
        srs_freep(pkt);
        return ret;
    }
    
    // use underlayer api to send, donot flush again.
    if ((ret = srs_protocol_do_send_and_free_packet(srsprotocol,srsprotocol->skt,pkt, 0,ACKNOWLEDGEMENT_TYPE)) != ERROR_SUCCESS) {
       srs_error("send acknowledgement failed. ret=%d", ret);
       srs_freep(pkt);
       return ret;
    }
    srs_freep(pkt);
    srs_verbose("send acknowledgement success.");  
    return ret;
}

int srs_protocol_response_ping_message(struct SrsProtocol* srsprotocol,int32_t timestamp)
{
    int ret = ERROR_SUCCESS;
 #if 0   
    srs_trace("get a ping request, response it. timestamp=%d\n", timestamp);
    
    SrsUserControlPacket* pkt = new SrsUserControlPacket();
    
    pkt->event_type = SrcPCUCPingResponse;
    pkt->event_data = timestamp;
    
    // cache the message and use flush to send.
    if (!auto_response_when_recv) {
        manual_response_queue.push_back(pkt);
        return ret;
    }
    
    // use underlayer api to send, donot flush again.
    if ((ret = do_send_and_free_packet(pkt, 0)) != ERROR_SUCCESS) {
        srs_error("send ping response failed. ret=%d", ret);
        return ret;
    }
    srs_verbose("send ping response success.");
#endif    
    return ret;
}

void srs_protocol_print_debug_info(struct SrsProtocol* srsprotocol)
{
#if 0 

    //if (show_debug_info) {
     if(1){
        //show_debug_info = false;
        srs_trace("protocol in.buffer=%d, in.ack=%d, out.ack=%d, in.chunk=%d, out.chunk=%d\n", in_buffer_length,
            in_ack_size.window, out_ack_size.window, in_chunk_size, out_chunk_size);
    }
#endif     
}



int srs_chunk_header_c0(
    int perfer_cid, uint32_t timestamp, int32_t payload_length,
    int8_t message_type, int32_t stream_id,
    char* cache, int nb_cache
) {
    // to directly set the field.
    char* pp = NULL;
    // generate the header.
    char* p = cache;

    // no header.
    if (nb_cache < SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE) {
        return 0;
    }
    
    // write new chunk stream header, fmt is 0
    *p++ = 0x00 | (perfer_cid & 0x3F);
    //print_msg("timestamp=%ld\n",timestamp);
    // chunk message header, 11 bytes
    // timestamp, 3bytes, big-endian
    if (timestamp < RTMP_EXTENDED_TIMESTAMP) {
        pp = (char*)&timestamp;
        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    } else {
        *p++ = 0xFF;
        *p++ = 0xFF;
        *p++ = 0xFF;
    }
    
    // message_length, 3bytes, big-endian
    pp = (char*)&payload_length;
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
    
    // message_type, 1bytes
    *p++ = message_type;
    
    // stream_id, 4bytes, little-endian
    pp = (char*)&stream_id;
    *p++ = pp[0];
    *p++ = pp[1];
    *p++ = pp[2];
    *p++ = pp[3];
    
    // for c0
    // chunk extended timestamp header, 0 or 4 bytes, big-endian
    //
    // for c3:
    // chunk extended timestamp header, 0 or 4 bytes, big-endian
    // 6.1.3. Extended Timestamp
    // This field is transmitted only when the normal time stamp in the
    // chunk message header is set to 0x00ffffff. If normal time stamp is
    // set to any value less than 0x00ffffff, this field MUST NOT be
    // present. This field MUST NOT be present if the timestamp field is not
    // present. Type 3 chunks MUST NOT have this field.
    // adobe changed for Type3 chunk:
    //        FMLE always sendout the extended-timestamp,
    //        must send the extended-timestamp to FMS,
    //        must send the extended-timestamp to flash-player.
    // @see: ngx_rtmp_prepare_message
    // @see: http://blog.csdn.net/win_lin/article/details/13363699
    // TODO: FIXME: extract to outer.
    if (timestamp >= RTMP_EXTENDED_TIMESTAMP) {
        pp = (char*)&timestamp;
        *p++ = pp[3];
        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    }
    //hexdump(cache, p - cache);
    // always has header
    return p - cache;
}

int srs_chunk_header_c3(
    int perfer_cid, uint32_t timestamp,
    char* cache, int nb_cache
) {
    // to directly set the field.
    char* pp = NULL;
    // generate the header.
    char* p = cache;

    // no header.
    if (nb_cache < SRS_CONSTS_RTMP_MAX_FMT3_HEADER_SIZE) {
        return 0;
    }
    
    // write no message header chunk stream, fmt is 3
    // @remark, if perfer_cid > 0x3F, that is, use 2B/3B chunk header,
    // SRS will rollback to 1B chunk header.
    *p++ = 0xC0 | (perfer_cid & 0x3F);
    
    // for c0
    // chunk extended timestamp header, 0 or 4 bytes, big-endian
    //
    // for c3:
    // chunk extended timestamp header, 0 or 4 bytes, big-endian
    // 6.1.3. Extended Timestamp
    // This field is transmitted only when the normal time stamp in the
    // chunk message header is set to 0x00ffffff. If normal time stamp is
    // set to any value less than 0x00ffffff, this field MUST NOT be
    // present. This field MUST NOT be present if the timestamp field is not
    // present. Type 3 chunks MUST NOT have this field.
    // adobe changed for Type3 chunk:
    //        FMLE always sendout the extended-timestamp,
    //        must send the extended-timestamp to FMS,
    //        must send the extended-timestamp to flash-player.
    // @see: ngx_rtmp_prepare_message
    // @see: http://blog.csdn.net/win_lin/article/details/13363699
    // TODO: FIXME: extract to outer.
    if (timestamp >= RTMP_EXTENDED_TIMESTAMP) {
        pp = (char*)&timestamp;
        *p++ = pp[3];
        *p++ = pp[2];
        *p++ = pp[1];
        *p++ = pp[0];
    }
     //hexdump(cache, p - cache);
    // always has header
    return p - cache;
}

static int srs_protocol_do_simple_send(struct SrsProtocol* srsprotocol,struct SimpleSocketStream* skt,struct SrsMessageHeader* mh, char* payload, int size)
{
    int ret = ERROR_SUCCESS;
    struct SrsBlockSyncSocket* sktv = (struct SrsBlockSyncSocket*)srsprotocol->skt->io; 

    // we directly send out the packet,
    // use very simple algorithm, not very fast,
    // but it's ok.
    char* p = payload;
    char* end = p + size;
    char c0c3[SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE];
      
    xSemaphoreTake(sktv->mutex, portMAX_DELAY); 
    while (p < end) {
        int nbh = 0;
        if (p == payload) {
            nbh = srs_chunk_header_c0(
                mh->perfer_cid, (uint32_t)mh->timestamp, mh->payload_length,
                mh->message_type, mh->stream_id,
                c0c3, sizeof(c0c3));
        } else {
            nbh = srs_chunk_header_c3(
                mh->perfer_cid, (uint32_t)mh->timestamp,
                c0c3, sizeof(c0c3));
        }
        srs_assert(nbh > 0);;
        
        struct iovec iovs[2];
        iovs[0].iov_base = c0c3;
        iovs[0].iov_len = nbh;
        int out_chunk_size=srsprotocol->out_chunk_size;
        int payload_size = srs_min((int)(end - p), out_chunk_size);
        iovs[1].iov_base = p;
        iovs[1].iov_len = payload_size;
        p += payload_size;
        
        if ((ret = srs_simple_socket_writev(skt,iovs, 2, NULL)) != ERROR_SUCCESS) {
            if (!srs_is_client_gracefully_close(ret)) {
                srs_error("send packet with writev failed. ret=%d", ret);
            }
            xSemaphoreGive(sktv->mutex);
            return ret;
        }
       
    } 
    xSemaphoreGive(sktv->mutex);
    return ret;
}



int srs_protocol_on_send_packet(struct SrsProtocol* srsprotocol,struct SrsMessageHeader* mh, void* packet)
{
    int ret = ERROR_SUCCESS;
    if (packet == NULL) {
        return ret;
    } 
    switch (mh->message_type) {
        case RTMP_MSG_SetChunkSize: {
            struct SrsSetChunkSizePacket* srssetchunksizepacket = (struct SrsSetChunkSizePacket*)packet; 
            srsprotocol->out_chunk_size = srssetchunksizepacket->chunk_size;
            srs_info("out.chunk=%d", srsprotocol->out_chunk_size);
            printf("out.chunk=%d", srsprotocol->out_chunk_size);
            break;
        }
        default:
            break;
    }
    return ret;
}



static int srs_protocol_do_send_and_free_packet(struct SrsProtocol* srsprotocol,struct SimpleSocketStream* skt,void* packet, int stream_id,enum pkt_type packetype){

    int ret = ERROR_SUCCESS;
    int size = 0;
    int8_t message_type;
    int perfer_cid;
    char* payload = NULL;
  
    if ((ret = srs_protocol_encode(packet,packetype,&size,&payload)) != ERROR_SUCCESS) {
        srs_error("encode RTMP packet to bytes oriented RTMP message failed. ret=%d", ret);
        return ret;
    }

    if(packetype==CONNECT_APP_PACKET_TYPE){

        struct SrsConnectAppPacket* connectapppacket = (struct SrsConnectAppPacket*)packet;  
        message_type=srs_connect_app_packet_get_message_type();
        perfer_cid=srs_connect_app_packet_get_prefer_cid();

    }else if(packetype==SET_WINDOW_ACKSIZE_PACKET_TYPE){
        
        struct SrsSetWindowAckSizePacket* setwindowacksizepacket = (struct SrsSetWindowAckSizePacket*)packet;
        message_type=srs_set_window_acksize_packet_get_message_type();
        perfer_cid=srs_set_window_acksize_packet_get_prefer_cid();

    }else if(packetype==CREATESTREAM_PACKET_TYPE){
        
        struct SrsCreateStreamPacket* createstreampacket = (struct SrsCreateStreamPacket*)packet; 
        message_type=srs_createstream_packet_get_message_type();
        perfer_cid=srs_createstream_packet_get_prefer_cid();
            
    }else if(packetype==PUBLISH_PACKET_TYPE){

        struct SrsPublishPacket* publishpacket = (struct SrsPublishPacket*)packet; 
        message_type=srs_publish_packet_get_message_type();
        perfer_cid=srs_publish_packet_get_prefer_cid();
        
    }else if(packetype==RELEASE_STREAM_PACKET_TYPE){

        struct SrsReleaseStreamPacket* releasestreampacket = (struct SrsReleaseStreamPacket*)packet; 
        message_type=srs_release_stream_packet_get_message_type();
        perfer_cid=srs_release_stream_packet_get_prefer_cid();

    }else if(packetype==FCPUBLISH_PACKET_TYPE){
        
        struct SrsFcPublishPacket* srsfcpublishpacket = (struct SrsFcPublishPacket*)packet; 
        message_type=srs_fcpublish_packet_get_message_type();
        perfer_cid=srs_fcpublish_packet_get_prefer_cid();
    }else if(packetype==SET_CHUNK_SIZE_TYPE){

        struct SrsSetChunkSizePacket* srssetchunksizepacket = (struct SrsSetChunkSizePacket*)packet; 
        message_type=srs_set_chunksize_packet_get_message_type();
        perfer_cid=srs_set_chunksize_packet_get_prefer_cid();
    }
    // encode packet to payload and size.
    if (size <= 0 || payload == NULL) {
        srs_warn("packet is empty, ignore empty message.\n");
        return ret;
    }
    // to message
    struct SrsMessageHeader header;
    header.payload_length = size;
    header.message_type = message_type;
    header.stream_id = stream_id;
    header.perfer_cid = perfer_cid;
    
    ret = srs_protocol_do_simple_send(srsprotocol,skt,&header, payload, size);
    srs_freep(payload);
    if (ret == ERROR_SUCCESS) {
       ret = srs_protocol_on_send_packet(srsprotocol,&header, packet);
    }  
    return ret;
}


int srs_sharedptrmessage_create(struct SrsSharedPtrMessage* msg,struct SrsMessageHeader* pheader, char* payload, int size)
{
    int ret = ERROR_SUCCESS;
    if (msg->ptr) {
        ret = ERROR_SYSTEM_ASSERT_FAILED;
        srs_error("should not set the payload twice. ret=%d", ret);
        
        srs_assert(false);
        
        return ret;
    } 
    msg->ptr = (struct SrsSharedPtrPayload*)malloc(sizeof(struct SrsSharedPtrPayload)); 
    
    // direct attach the data.
    if (pheader) {
     
        msg->ptr->header.message_type = pheader->message_type;
        msg->ptr->header.payload_length = size;
        msg->ptr->header.perfer_cid = pheader->perfer_cid;
        msg->timestamp = pheader->timestamp;
        //print_msg("msg->timestamp=%ld\n",msg->timestamp);
        msg->stream_id = pheader->stream_id;
    }
    msg->ptr->payload = payload;
    msg->ptr->size = size;    
    // message can access it.
    msg->payload = msg->ptr->payload;
    msg->size = msg->ptr->size;
    return ret;
}



int srs_sharedptrmessage_count(struct SrsSharedPtrMessage* msg)
{
    srs_assert(msg->ptr);
    return msg->ptr->shared_count;
}

bool srs_sharedptrmessage_check(struct SrsSharedPtrMessage* msg,int stream_id)
{
    // we donot use the complex basic header,
    // ensure the basic header is 1bytes.
    if (msg->ptr->header.perfer_cid < 2) {
        srs_info("change the chunk_id=%d to default=%d\n",
            msg->ptr->header.perfer_cid, RTMP_CID_ProtocolControl);
        msg->ptr->header.perfer_cid = RTMP_CID_ProtocolControl;
    }
    
    // we assume that the stream_id in a group must be the same.
    if (msg->stream_id == stream_id) {
        return true;
    }
    msg->stream_id = stream_id;
    return false;
}

bool srs_sharedptrmessage_is_av(struct SrsSharedPtrMessage* msg)
{
    return msg->ptr->header.message_type == RTMP_MSG_AudioMessage
        || msg->ptr->header.message_type == RTMP_MSG_VideoMessage;
}

bool srs_sharedptrmessage_is_audio(struct SrsSharedPtrMessage* msg)
{
    return msg->ptr->header.message_type == RTMP_MSG_AudioMessage;
}

bool srs_sharedptrmessage_is_video(struct SrsSharedPtrMessage* msg)
{
    return msg->ptr->header.message_type == RTMP_MSG_VideoMessage;
}

int srs_sharedptrmessage_chunk_header(struct SrsSharedPtrMessage* msg,char* cache, int nb_cache, bool c0)
{
    if (c0) {
        return srs_chunk_header_c0(
            msg->ptr->header.perfer_cid, msg->timestamp, msg->ptr->header.payload_length,
            msg->ptr->header.message_type, msg->stream_id,
            cache, nb_cache);
    } else {
        return srs_chunk_header_c3(
            msg->ptr->header.perfer_cid, msg->timestamp,
            cache, nb_cache);
    }
}


int srs_sharedptrmessage_chunk_header_2(void* rtmp,char* cache, int nb_cache, bool c0)
{
    struct Context* context = (struct Context*)rtmp;

    if (c0) {
        return srs_chunk_header_c0(
            context->video_header.perfer_cid,context->video_header.timestamp, context->video_header.payload_length,
            context->video_header.message_type, context->video_header.stream_id,
            cache, nb_cache);
    } else {
        return srs_chunk_header_c3(
            context->video_header.perfer_cid, context->video_header.timestamp,
            cache, nb_cache);
    }
}



struct SrsSharedPtrMessage* srs_sharedptrmessage_copy(struct SrsSharedPtrMessage* msg)
{
    srs_assert(msg->ptr);
    
    struct SrsSharedPtrMessage* copy = (struct SrsSharedPtrMessage*)malloc(sizeof(struct SrsSharedPtrMessage));     
    copy->ptr = msg->ptr;
    msg->ptr->shared_count++;
    
    copy->timestamp = msg->timestamp;
    copy->stream_id = msg->stream_id;
    copy->payload = msg->ptr->payload;
    copy->size = msg->ptr->size;
    
    return copy;
}

