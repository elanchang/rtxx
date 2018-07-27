#ifndef __RTMP_SOCKET_TO_BUFFER_H__
#define __RTMP_SOCKET_TO_BUFFER_H__


struct SrsFastStream{
#ifdef SRS_PERF_MERGED_READ
    bool merged_read;
    IMergeReadHandler* _handler;
#endif
    char* p;
    char* end;
    char* buffer;
    int nb_buffer;
};

struct SrsFastStream* srs_fast_stream_init();
void srs_fast_stream_uinit(struct SrsFastStream* srsfaststream);
int srs_fast_stream_size(struct SrsFastStream* srsfaststream);
char* srs_fast_stream_bytes(struct SrsFastStream* srsfaststream);
void srs_fast_stream_set_buffer(struct SrsFastStream* srsfaststream,int buffer_size);
char srs_fast_stream_read_1byte(struct SrsFastStream* srsfaststream);
char* srs_fast_stream_read_slice(struct SrsFastStream* srsfaststream,int size);
void srs_fast_stream_skip(struct SrsFastStream* srsfaststream,int size);
int srs_fast_stream_grow(struct SrsFastStream* srsfaststream,struct SimpleSocketStream* simplesocketstream, int required_size);

#endif


