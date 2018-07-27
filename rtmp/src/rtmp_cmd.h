#ifndef __RTMP_CMD_H__
#define __RTMP_CMD_H__

#include "kvs.h"

#define int_type 0
#define str_type 1
#define bool_type 2

struct info_t{
  union attribute
  {
    double   int_value;
    AVal  str;
    bool  bool_value;
  }u;
  int type;
};

struct  SrsConnectAppPacket{

    AVal command_name;
    double transaction_id;
    KVstore command_object;
    KVstore args;
    struct info_t app_value;
    struct info_t flashver_value;
    struct info_t swfurl_alue;
    struct info_t tcurl_value;
    struct info_t fpad_value;
    struct info_t capabilities_value;
    struct info_t audiocodecs_value;
    struct info_t videocodecs_value;
    struct info_t videofunction_value;
    struct info_t pageUrl_value;
    struct info_t objectEncoding_value;
};

struct SrsSetWindowAckSizePacket{

    int32_t ackowledgement_window_size;
};

struct SrsReleaseStreamPacket{

    AVal command_name;
    double transaction_id;
    AVal stream_name;
};

struct SrsFcPublishPacket {
    
    AVal command_name;
    double transaction_id;
    AVal stream_name;
};

struct SrsCreateStreamPacket {
    AVal command_name;
    double transaction_id;
    //KVstore command_object; // null
};

struct SrsPublishPacket{
    
    AVal command_name;
    double transaction_id;
    AVal stream_name;
    AVal type;
    //KVstore command_object; // null
};

struct SrsSetChunkSizePacket{
    
    int32_t chunk_size;
};


struct SrsConnectAppResPacket{
    
    struct AVal command_name;
    double transaction_id;
    ///KVstore props;
    //KVstore info;
};

struct SrsCreateStreamResPacket{
    
    struct AVal command_name;
    double transaction_id;
    //KVstore command_object; // null
    double stream_id;
};

struct SrsAcknowledgementPacket{
    
    uint32_t sequence_number;
};

struct SrsUserControlPacket{
    
    int16_t event_type;
    int32_t event_data;
    int32_t extra_data;

};


struct SrsConnectAppPacket * srs_connect_app_packt_init(AVal app, AVal tcUrl);
void srs_connect_app_packt_uinit(struct        SrsConnectAppPacket *srsconnectapppacket);
int srs_connect_app_packet_get_prefer_cid();
int srs_connect_app_packet_get_message_type();
int srs_connect_app_packet_encode_packet(struct SrsConnectAppPacket *srsconnectapppacket ,struct SrsBuffer* stream);
int srs_connect_app_packet_get_size(void *packet);


struct SrsSetWindowAckSizePacket * srs_set_window_acksize_packet_init();
void srs_set_window_acksize_packet_uinit(struct SrsSetWindowAckSizePacket *srssetwindowacksizepacket);
int srs_set_window_acksize_packet_decode(struct SrsSetWindowAckSizePacket *srssetwindowacksizepacket,struct SrsBuffer* stream);
int srs_set_window_acksize_packet_encode_packet(struct SrsSetWindowAckSizePacket *srssetwindowacksizepacket ,struct SrsBuffer* stream);
int srs_set_window_acksize_packet_get_prefer_cid();
int srs_set_window_acksize_packet_get_message_type();
int srs_set_window_acksize_packet_get_size(void *packet);


struct SrsReleaseStreamPacket* srs_release_stream_packet_init();
void srs_release_stream_packet_uinit(struct          SrsReleaseStreamPacket *srsreleasestreampacket);
int srs_release_stream_packet_encode_packet(struct SrsReleaseStreamPacket *srsreleasestreampacket ,struct SrsBuffer* stream);
int srs_release_stream_packet_get_prefer_cid();
int srs_release_stream_packet_get_message_type();
int srs_release_stream_packet_get_size(void *packet);


struct SrsFcPublishPacket * srs_fcpublish_packet_init();
void srs_fcpublish_packet_uinit(struct SrsFcPublishPacket *srsfcpublishpacket);
int srs_fcpublish_packet_encode_packet(struct           SrsFcPublishPacket *srsfcpublishpacket ,struct SrsBuffer* stream);
int srs_fcpublish_packet_get_prefer_cid();
int srs_fcpublish_packet_get_message_type();
int srs_fcpublish_packet_get_size(void *packet);


struct SrsCreateStreamPacket * srs_createstream_packet_init();
void srs_createstream_packet_uinit(struct SrsCreateStreamPacket *srscreatestreampacket);
int srs_createstream_packet_encode_packet(struct            SrsCreateStreamPacket *srscreatestreampacket ,struct SrsBuffer* stream);
int srs_createstream_packet_get_prefer_cid();
int srs_createstream_packet_get_message_type();
int srs_createstream_packet_get_size(void *packet);


struct SrsPublishPacket * srs_publish_packet_init();
void srs_publish_packet_uinit(struct         SrsPublishPacket *srspublishpacket);
int srs_publish_packet_encode_packet(struct          SrsPublishPacket *srspublishpacket ,struct SrsBuffer* stream);
int srs_publish_packet_get_prefer_cid();
int srs_publish_packet_get_message_type();
int srs_publish_packet_get_size(void *packet);


struct SrsSetChunkSizePacket * srs_set_chunksize_packet_init(int out_chunk_size);
void  srs_set_chunksize_packet_uinit(struct  SrsSetChunkSizePacket *srssetchunksizepacket);
int srs_set_chunksize_packet_decode(struct SrsSetChunkSizePacket* srssetchunksizepacket,struct SrsBuffer* stream);
int srs_set_chunksize_packet_get_prefer_cid();
int srs_set_chunksize_packet_get_message_type();
int srs_set_chunksize_packet_get_size();
int srs_set_chunksize_packet_encode_packet(struct SrsSetChunkSizePacket* srssetchunksizepacket,struct SrsBuffer* stream);



int srs_createstreamres_packet_decode(struct SrsCreateStreamResPacket* createstreamrespacket ,struct SrsBuffer* stream);



#endif

