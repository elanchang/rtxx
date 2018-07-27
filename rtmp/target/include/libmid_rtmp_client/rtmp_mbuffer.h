
#ifndef __RTMP_M_BUFFER_H__
#define __RTMP_M_BUFFER_H__



struct AVal;

struct SrsBuffer{
    // current position at bytes.
    char* p;
    // the bytes data for stream to read or write.
    char* bytes;
    // the total number of bytes.
    int nb_bytes;
};


int srs_buffer_initialize(struct SrsBuffer* srcbuffer,char* b, int nb);
char* srs_buffer_data(struct SrsBuffer* srcbuffer);
int srs_buffer_size(struct SrsBuffer* srcbuffer);
int srs_buffer_pos(struct SrsBuffer* srcbuffer);
bool srs_buffer_empty(struct SrsBuffer* srcbuffer);
bool srs_buffer_require(struct SrsBuffer* srcbuffer,int required_size);
void srs_buffer_skip(struct SrsBuffer* srcbuffer, int size);
int8_t srs_buffer_read_1bytes(struct SrsBuffer* srcbuffer);
int16_t srs_buffer_read_2bytes(struct SrsBuffer* srcbuffer);
int32_t srs_buffer_read_3bytes(struct SrsBuffer* srcbuffer);
int32_t srs_buffer_read_4bytes(struct SrsBuffer* srcbuffer);
int64_t srs_buffer_read_8bytes(struct SrsBuffer* srcbuffer);
int srs_buffer_read_string(struct SrsBuffer* srcbuffer,int len,struct AVal* value);
void srs_buffer_read_bytes(struct SrsBuffer* srcbuffer,char* data, int size);
void srs_buffer_write_1bytes(struct SrsBuffer* srcbuffer,int8_t value);
void srs_buffer_write_2bytes(struct SrsBuffer* srcbuffer,int16_t value);
void srs_buffer_write_4bytes(struct SrsBuffer* srcbuffer,int32_t value);
void srs_buffer_write_3bytes(struct SrsBuffer* srcbuffer,int32_t value);
void srs_buffer_write_8bytes(struct SrsBuffer* srcbuffer,int64_t value);
void srs_buffer_write_string(struct SrsBuffer* srcbuffer,struct AVal value);
void srs_buffer_write_bytes(struct SrsBuffer* srcbuffer,char* data, int size);




#endif
