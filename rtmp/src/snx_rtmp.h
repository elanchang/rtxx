#ifndef __SNX_RTMP_H__
#define __SNX_RTMP_H__

struct AVal;
struct SrsServerInfo;
struct SrsRtmpClient;
struct SrsBuffer;


typedef void* srs_rtmp_t;
typedef void* srs_amf0_t;

enum srs_url_schema{

    // Forbidden.
    srs_url_schema_forbidden = 0,
    // Normal RTMP URL, the vhost put in host field, using DNS to resolve the server ip.
    // For example, rtmp://vhost:port/app/stream
    srs_url_schema_normal,
    // VIA(vhost in app), the vhost put in app field.
    // For example, rtmp://ip:port/vhost/app/stream
    srs_url_schema_via,
    // VIS(vhost in stream), the vhost put in query string, keyword use vhost=xxx.
    // For example, rtmp://ip:port/app/stream?vhost=xxx
    srs_url_schema_vis,
    // VIS, keyword use domain=xxx.
    // For example, rtmp://ip:port/app/stream?domain=xxx
    srs_url_schema_vis2
};

struct  h264_video_header
{
    int32_t payload_length;
    int8_t message_type;
    int perfer_cid;   
    int64_t timestamp;
    int32_t stream_id;
    char* payload;
    int size;
};

struct Context{
    // The original RTMP url.
    AVal url;
    AVal schema_str;
    AVal host;
    AVal app;
    AVal stream;
    AVal tcUrl;
    AVal vhost;
    // Parse ip:port from host.
    AVal ip;
    int port;

    AVal h264_sps;
    AVal h264_pps;

    struct SimpleSocketStream* skt;
    struct SrsRtmpClient* rtmp;

    int stream_id;
    struct SrsBuffer h264_raw_stream;
    struct SrsBuffer aac_raw_stream;
    
    bool h264_sps_pps_sent;
    bool h264_sps_changed;
    bool h264_pps_changed;
    int64_t stimeout;
    int64_t rtimeout;
    char buffer[1024];
    enum srs_url_schema schema;
    
    AVal aac_specific_config;


    char *h264_data;
    int h264_malloc_size;
    int h264_data_size;
    struct h264_video_header video_header;

    
};


enum SrsFrameType
{
    // set to the zero to reserved, for array map.
    SrsFrameTypeReserved = 0,
    SrsFrameTypeForbidden = 0,

    // 8 = audio
    SrsFrameTypeAudio = 8,
    // 9 = video
    SrsFrameTypeVideo = 9,
    // 18 = script data
    SrsFrameTypeScript = 18,
};


srs_rtmp_t snx_rtmp_create(const char* url);
void snx_rtmp_destroy(srs_rtmp_t rtmp);
int snx_rtmp_handshake(srs_rtmp_t rtmp);
int snx_rtmp_connect_app(srs_rtmp_t rtmp);
int snx_rtmp_publish_stream(srs_rtmp_t rtmp);
int srs_rtmp_set_timeout(srs_rtmp_t rtmp, int recv_timeout_ms, int send_timeout_ms);

int srs_rtmp_write_packet(srs_rtmp_t rtmp, char type, uint32_t timestamp, char* data, int size);


#endif

