#ifndef __RTMP_PROTOCOL_H__
#define __RTMP_PROTOCOL_H__

struct SimpleSocketStream;
struct SrsChunkStream;


#define SRS_MODIFY_RTMP_OUT_CHUNK_SIZE      12800



#define SRS_CONSTS_RTMP_PROTOCOL_CHUNK_SIZE 128
/**
* 6. Chunking
* The chunk size is configurable. It can be set using a control
* message(Set Chunk Size) as described in section 7.1. The maximum
* chunk size can be 65536 bytes and minimum 128 bytes. Larger values
* reduce CPU usage, but also commit to larger writes that can delay
* other content on lower bandwidth connections. Smaller chunks are not
* good for high-bit rate streaming. Chunk size is maintained
* independently for each direction.
*/
#define SRS_CONSTS_RTMP_MIN_CHUNK_SIZE 128
#define SRS_CONSTS_RTMP_MAX_CHUNK_SIZE 65536
// the following is the timeout for rtmp protocol, 
// to avoid death connection.
// Never timeout in ms
// @remake Rename from SRS_CONSTS_NO_TIMEOUT
// @see ST_UTIME_NO_TIMEOUT
#define SRS_CONSTS_NO_TMMS ((int64_t) -1LL)
// the common io timeout, for both recv and send.
// TODO: FIXME: use ms for timeout.
#define SRS_CONSTS_RTMP_TMMS (30*1000)
// the timeout to wait for client control message,
// if timeout, we generally ignore and send the data to client,
// generally, it's the pulse time for data seding.
// @remark, recomment to 500ms.
#define SRS_CONSTS_RTMP_PULSE_TMMS (500)
/**
* max rtmp header size:
*     1bytes basic header,
*     11bytes message header,
*     4bytes timestamp header,
* that is, 1+11+4=16bytes.
*/
#define SRS_CONSTS_RTMP_MAX_FMT0_HEADER_SIZE 16
/**
* max rtmp header size:
*     1bytes basic header,
*     4bytes timestamp header,
* that is, 1+4=5bytes.
*/
// always use fmt0 as cache.
#define SRS_CONSTS_RTMP_MAX_FMT3_HEADER_SIZE 5


#define SRS_PERF_MW_MSGS 128
/**
* for performance issue, 
* the iovs cache, @see https://github.com/ossrs/srs/issues/194
* iovs cache for multiple messages for each connections.
* suppose the chunk size is 64k, each message send in a chunk which needs only 2 iovec,
* so the iovs max should be (SRS_PERF_MW_MSGS * 2)
*
* @remark, SRS will realloc when the iovs not enough.
*/
#define SRS_CONSTS_IOVS_MAX (SRS_PERF_MW_MSGS * 2)
/**
* for performance issue, 
* the c0c3 cache, @see https://github.com/ossrs/srs/issues/194
* c0c3 cache for multiple messages for each connections.
* each c0 <= 16byes, suppose the chunk size is 64k,
* each message send in a chunk which needs only a c0 header,
* so the c0c3 cache should be (SRS_PERF_MW_MSGS * 16)
*
* @remark, SRS will try another loop when c0c3 cache dry, for we cannot realloc it.
*       so we use larger c0c3 cache, that is (SRS_PERF_MW_MSGS * 32)
*/
#define SRS_CONSTS_C0C3_HEADERS_MAX (SRS_PERF_MW_MSGS * 32)

#define SRS_PERF_CHUNK_STREAM_CACHE 16


/****************************************************************************
*****************************************************************************
****************************************************************************/
/**
* 6.1.2. Chunk Message Header
* There are four different formats for the chunk message header,
* selected by the "fmt" field in the chunk basic header.
*/
// 6.1.2.1. Type 0
// Chunks of Type 0 are 11 bytes long. This type MUST be used at the
// start of a chunk stream, and whenever the stream timestamp goes
// backward (e.g., because of a backward seek).
#define RTMP_FMT_TYPE0                          0
// 6.1.2.2. Type 1
// Chunks of Type 1 are 7 bytes long. The message stream ID is not
// included; this chunk takes the same stream ID as the preceding chunk.
// Streams with variable-sized messages (for example, many video
// formats) SHOULD use this format for the first chunk of each new
// message after the first.
#define RTMP_FMT_TYPE1                          1
// 6.1.2.3. Type 2
// Chunks of Type 2 are 3 bytes long. Neither the stream ID nor the
// message length is included; this chunk has the same stream ID and
// message length as the preceding chunk. Streams with constant-sized
// messages (for example, some audio and data formats) SHOULD use this
// format for the first chunk of each message after the first.
#define RTMP_FMT_TYPE2                          2
// 6.1.2.4. Type 3
// Chunks of Type 3 have no header. Stream ID, message length and
// timestamp delta are not present; chunks of this type take values from
// the preceding chunk. When a single message is split into chunks, all
// chunks of a message except the first one, SHOULD use this type. Refer
// to example 2 in section 6.2.2. Stream consisting of messages of
// exactly the same size, stream ID and spacing in time SHOULD use this
// type for all chunks after chunk of Type 2. Refer to example 1 in
// section 6.2.1. If the delta between the first message and the second
// message is same as the time stamp of first message, then chunk of
// type 3 would immediately follow the chunk of type 0 as there is no
// need for a chunk of type 2 to register the delta. If Type 3 chunk
// follows a Type 0 chunk, then timestamp delta for this Type 3 chunk is
// the same as the timestamp of Type 0 chunk.
#define RTMP_FMT_TYPE3                          3


#define RTMP_AMF0_COMMAND_CONNECT               "connect"
#define RTMP_AMF0_COMMAND_CREATE_STREAM         "createStream"
#define RTMP_AMF0_COMMAND_CLOSE_STREAM          "closeStream"
#define RTMP_AMF0_COMMAND_PLAY                  "play"
#define RTMP_AMF0_COMMAND_PAUSE                 "pause"
#define RTMP_AMF0_COMMAND_ON_BW_DONE            "onBWDone"
#define RTMP_AMF0_COMMAND_ON_STATUS             "onStatus"
#define RTMP_AMF0_COMMAND_RESULT                "_result"
#define RTMP_AMF0_COMMAND_ERROR                 "_error"
#define RTMP_AMF0_COMMAND_RELEASE_STREAM        "releaseStream"
#define RTMP_AMF0_COMMAND_FC_PUBLISH            "FCPublish"
#define RTMP_AMF0_COMMAND_UNPUBLISH             "FCUnpublish"
#define RTMP_AMF0_COMMAND_PUBLISH               "publish"
#define RTMP_AMF0_DATA_SAMPLE_ACCESS            "|RtmpSampleAccess"

enum pkt_type{

    CONNECT_APP_PACKET_TYPE = 0,
    SET_WINDOW_ACKSIZE_PACKET_TYPE=1,
    CREATESTREAM_PACKET_TYPE=2,
    PUBLISH_PACKET_TYPE=3,
    RELEASE_STREAM_PACKET_TYPE=4,
    FCPUBLISH_PACKET_TYPE=5,
    ACKNOWLEDGEMENT_TYPE=6,
    CONNECT_APP_RES_TYPE=7,
    CREATESTREAMRES_TYPE=8,
    SET_CHUNK_SIZE_TYPE=9,
    SET_PEER_BANDWIDTH_TYPE=10,
    USER_CONTROL_TYPE=11,
    UNKNOWN_TYPE=20,     
};

struct AckWindowSize{
    
    uint32_t window;
    int64_t nb_recv_bytes;
    uint32_t sequence_number;    
};


struct SrsSharedMessageHeader{
    
    int32_t payload_length;
    int8_t message_type;
    int perfer_cid;
};

struct SrsSharedPtrPayload{
    
    struct SrsSharedMessageHeader header;
    char* payload;
    int size;
    int shared_count;
};

struct  SrsSharedPtrMessage{
    
    int64_t timestamp;
    int32_t stream_id;
    int size;
    char* payload;
    struct SrsSharedPtrPayload* ptr;
};


struct  SrsSharedPtrMessage_2
{
    int32_t payload_length;
    int8_t message_type;
    int perfer_cid;   
    int64_t timestamp;
    int32_t stream_id;
    char* payload;
    int size;
};






struct SrsProtocol{
    
    struct SimpleSocketStream* skt;  //use other no free in this struct
    struct SrsChunkStream** cs_cache;

    int32_t in_chunk_size;
    struct AckWindowSize in_ack_size;
    int32_t out_chunk_size;
    struct AckWindowSize out_ack_size;

    struct SrsFastStream* in_buffer;
    int32_t in_buffer_length;
    
    bool show_debug_info;
    bool auto_response_when_recv;

    struct iovec* out_iovs;
    int nb_out_iovs;
    
    char out_c0c3_caches[SRS_CONSTS_C0C3_HEADERS_MAX];
    bool warned_c0c3_cache_dry;
};


void srs_protocol_set_auto_response(struct SrsProtocol* srsprotocol,bool v);
int srs_protocol_manual_response_flush(struct SrsProtocol* srsprotocol);

#ifdef SRS_PERF_MERGED_READ
void srs_protocol_set_merge_read(struct SrsProtocol* srsprotocol,bool v, IMergeReadHandler* handler);
void srs_protocol_set_recv_buffer(struct SrsProtocol* srsprotocol,int buffer_size);
#endif
void srs_protocol_set_recv_timeout(struct SrsProtocol* srsprotocol,int64_t tm);
int64_t srs_protocol_get_recv_timeout(struct SrsProtocol* srsprotocol);
void srs_protocol_set_send_timeout(struct SrsProtocol* srsprotocol,int64_t tm);
int64_t srs_protocol_get_send_timeout(struct SrsProtocol* srsprotocol);
int64_t srs_protocol_get_recv_bytes(struct SrsProtocol* srsprotocol);
int64_t srs_protocol_get_send_bytes(struct SrsProtocol* srsprotocol);
int srs_protocol_set_in_window_ack_size(struct SrsProtocol* srsprotocol,int ack_size);
int srs_protocol_recv_message(struct SrsProtocol* srsprotocol,struct SrsCommonMessage** pmsg);
int srs_protocol_decode_message(struct SrsCommonMessage* msg, void** ppacket,enum pkt_type* return_type);
int srs_protocol_do_decode_message(struct SrsMessageHeader* header, struct SrsBuffer* stream, void** ppacket,enum pkt_type* return_type);
int srs_protocol_send_and_free_message(struct SrsProtocol* srsprotocol, struct SrsSharedPtrMessage* msg, int stream_id);
int srs_protocol_send_and_free_messages(struct SrsProtocol* srsprotocol, struct SrsSharedPtrMessage** msgs, int nb_msgs, int stream_id);
int srs_protocol_recv_interlaced_message(struct SrsProtocol* srsprotocol,struct SrsCommonMessage** pmsg);
int srs_protocol_read_basic_header(struct SrsProtocol* srsprotocol,char* fmt, int* cid);
int srs_protocol_read_message_header(struct SrsProtocol* srsprotocol,struct SrsChunkStream* chunk, char fmt);
int srs_protocol_read_message_payload(struct SrsProtocol* srsprotocol,struct SrsChunkStream* chunk,struct  SrsCommonMessage** pmsg);
int srs_protocol_on_recv_message(struct SrsProtocol* srsprotocol,struct SrsCommonMessage* msg);
int srs_protocol_on_send_packet(struct SrsProtocol* srsprotocol,struct SrsMessageHeader* mh, void* packet);
int srs_protocol_do_send_messages(struct SrsProtocol* srsprotocol, struct SrsSharedPtrMessage** msgs, int nb_msgs);
int srs_protocol_do_iovs_send(struct SrsProtocol* srsprotocol,struct iovec* iovs, int size);
int srs_protocol_response_acknowledgement_message(struct SrsProtocol* srsprotocol);
int srs_protocol_response_ping_message(struct SrsProtocol* srsprotocol,int32_t timestamp);
void srs_protocol_print_debug_info(struct SrsProtocol* srsprotocol);


int srs_chunk_header_c0(int perfer_cid, uint32_t timestamp, int32_t payload_length,int8_t message_type, int32_t stream_id,char* cache, int nb_cache); 
int srs_chunk_header_c3(int perfer_cid, uint32_t timestamp,char* cache, int nb_cache);





struct SrsProtocol* srs_protocol_init();
void srs_protocol_uinit(struct SrsProtocol* srsprotocol);
int srs_protocol_send_and_free_packet(struct SrsProtocol* srsprotocol,struct SimpleSocketStream* skt,void* packet, int stream_id,enum pkt_type packetype);


#endif
