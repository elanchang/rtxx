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
#include "rtmp_msg.h"

void srs_message_header_init(struct SrsMessageHeader *srcmessageheader){
    srcmessageheader->message_type = 0;
    srcmessageheader->payload_length = 0;
    srcmessageheader->timestamp_delta = 0;
    srcmessageheader->stream_id = 0;
    srcmessageheader->timestamp = 0;
    // we always use the connection chunk-id
    srcmessageheader->perfer_cid = RTMP_CID_OverConnection;
}

void srs_message_header_uinit(struct SrsMessageHeader *srcmessageheader){

}

bool srs_message_header_is_audio(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_AudioMessage;
}

bool srs_message_header_is_video(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_VideoMessage;
}

bool srs_message_header_is_amf0_command(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_AMF0CommandMessage;
}

bool srs_message_header_is_amf0_data(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_AMF0DataMessage;
}

bool srs_message_header_is_amf3_command(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_AMF3CommandMessage;
}

bool srs_message_header_is_amf3_data(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_AMF3DataMessage;
}

bool srs_message_header_is_window_ackledgement_size(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_WindowAcknowledgementSize;
}

bool srs_message_header_is_ackledgement(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_Acknowledgement;
}

bool srs_message_header_is_set_chunk_size(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_SetChunkSize;
}

bool srs_message_header_is_user_control_message(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_UserControlMessage;
}

bool srs_message_header_is_set_peer_bandwidth(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_SetPeerBandwidth;
}


bool srs_message_header_is_aggregate(struct SrsMessageHeader *srcmessageheader){

    return (srcmessageheader->message_type) == RTMP_MSG_AggregateMessage;
}


void srs_message_header_initialize_amf0_script(struct SrsMessageHeader *srcmessageheader,int size, int stream_id){

    srcmessageheader->message_type = RTMP_MSG_AMF0DataMessage;
    srcmessageheader->payload_length = (int32_t)size;
    srcmessageheader->timestamp_delta = (int32_t)0;
    srcmessageheader->timestamp = (int64_t)0;
    srcmessageheader->stream_id = (int32_t)stream_id;    
    // amf0 script use connection2 chunk-id
    srcmessageheader->perfer_cid = RTMP_CID_OverConnection2;
}

void srs_message_header_initialize_audio(struct SrsMessageHeader *srcmessageheader,int size, uint32_t time, int stream_id){

    srcmessageheader->message_type = RTMP_MSG_AudioMessage;
    srcmessageheader->payload_length = (int32_t)size;
    srcmessageheader->timestamp_delta = (int32_t)time;
    srcmessageheader->timestamp = (int64_t)time;
    srcmessageheader->stream_id = (int32_t)stream_id;   
    // audio chunk-id
    srcmessageheader->perfer_cid = RTMP_CID_Audio;
}

void srs_message_header_initialize_video(struct SrsMessageHeader *srcmessageheader,int size, uint32_t time, int stream_id){

    srcmessageheader->message_type = RTMP_MSG_VideoMessage;
    srcmessageheader->payload_length = (int32_t)size;
    srcmessageheader->timestamp_delta = (int32_t)time;
    srcmessageheader->timestamp = (int64_t)time;
    //print_msg("time=%d\n",time);
    //print_msg("srcmessageheader->timestamp=%ld\n",srcmessageheader->timestamp);
    srcmessageheader->stream_id = (int32_t)stream_id;
    // video chunk-id
    srcmessageheader->perfer_cid = RTMP_CID_Video;
}


void srs_common_message_init(struct SrsCommonMessage* srscommonmessage){

    srscommonmessage->payload = NULL;
    srscommonmessage->size = 0;
}

void srs_common_message_uinit(struct SrsCommonMessage* srscommonmessage){

    srs_freep(srscommonmessage->payload);
}

void srs_common_message_create_payload(struct SrsCommonMessage* srscommonmessage,int size){

    srs_freep(srscommonmessage->payload);
    srscommonmessage->payload = (char*)malloc(size);
    memset(srscommonmessage->payload,0,size);
    srs_verbose("create payload for RTMP message. size=%d", size);
}

int srs_common_message_create(struct SrsCommonMessage* srscommonmessage,struct SrsMessageHeader* pheader, char* body, int size){

    int ret = ERROR_SUCCESS;    
    srs_freep(srscommonmessage->payload);    
    srscommonmessage->header = (*pheader);
    srscommonmessage->payload = body;
    srscommonmessage->size = size;
    return ret;
}


