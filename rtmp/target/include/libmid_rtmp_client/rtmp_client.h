#ifndef __RTMP_CLIENT_H__
#define __RTMP_CLIENT_H__

struct SrsHandshakeBytes;
struct SrsProtocol;
struct SimpleSocketStream;


struct SrsRtmpClient{

    struct SrsHandshakeBytes* hs_bytes;
    struct SrsProtocol* protocol;
    struct SimpleSocketStream* skt;  //use other no free in this struct
};

struct SrsServerInfo{
    char* ip;
    char* sig;
    int pid;
    int cid;
    int major;
    int minor;
    int revision;
    int build;
};


/**
 * the original request from client.
 */
struct SrsRequest{
    char* ip;
    char* tcUrl;
    char* pageUrl;
    char* swfUrl;
    double objectEncoding;
    char* schema;
    char* vhost;
    char* host;
    int port;
    char* app;
    char* param;
    char* stream;
    double duration;
};

struct SrsRtmpClient* srs_rtmp_client_init(struct SimpleSocketStream* skt);
void srs_rtmp_client_uinit(struct SrsRtmpClient* srsrtmpclient);
int srs_rtmp_client_simple_handshake(struct SrsRtmpClient* srsrtmpclient,struct  SimpleSocketStream* simplesocketstream);
int srs_rtmp_client_connect_app(struct SrsRtmpClient* srsrtmpclient,AVal app, AVal tcUrl);
int srs_rtmp_client_fmle_publish(struct SrsRtmpClient* srsrtmpclient,AVal stream, int* stream_id);


#if 0
void srs_rtmp_client_set_recv_timeout(struct SrsRtmpClient* srsrtmpclient,int64_t tm);
void srs_rtmp_client_set_send_timeout(struct SrsRtmpClient* srsrtmpclient,int64_t tm);
int64_t srs_rtmp_client_get_recv_bytes(struct SrsRtmpClient* srsrtmpclient);
int64_t srs_rtmp_client_get_send_bytes(struct SrsRtmpClient* srsrtmpclient);
int srs_rtmp_client_recv_message(struct SrsRtmpClient* srsrtmpclient,struct SrsCommonMessage** pmsg);
int srs_rtmp_client_decode_message(struct SrsRtmpClient* srsrtmpclient, struct SrsCommonMessage* msg, void** ppacket,enum pkt_type *return_type);
int srs_rtmp_client_send_and_free_message(struct SrsRtmpClient* srsrtmpclient, struct SrsSharedPtrMessage* msg, int stream_id);
int srs_rtmp_client_send_and_free_messages(struct SrsRtmpClient* srsrtmpclient,struct SrsSharedPtrMessage** msgs, int nb_msgs, int stream_id);
int srs_rtmp_client_send_and_free_packet(struct SrsRtmpClient* srsrtmpclient,void* packet, int stream_id,enum pkt_type packetype);
#endif


#endif

