#ifndef __RTMP_CHUNK_H__
#define __RTMP_CHUNK_H__

struct SrsMessageHeader;
struct SrsCommonMessage;


struct SrsChunkStream{
    char fmt;
    int cid;
    struct SrsMessageHeader header;
    bool extended_timestamp;
    struct SrsCommonMessage* msg;
    int64_t msg_count;
};

struct SrsChunkStream* srs_chunkstream_init(int cid);
void srs_chunkstream_uinit(struct SrsChunkStream* srschunkstream);


#endif

