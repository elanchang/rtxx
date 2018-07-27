#ifndef __RTMP_SIMPLE_HANDSHAKE_H__
#define __RTMP_SIMPLE_HANDSHAKE_H__

struct  SimpleSocketStream;

struct SrsHandshakeBytes{
 
    // [1+1536]
    char* c0c1;
    // [1+1536+1536]
    char* s0s1s2;
    // [1536]
    char* c2;
};

struct SrsHandshakeBytes*  srs_handshakebytes_init();
void srs_handshakebytes_uinit(struct SrsHandshakeBytes* srshandshakebytes);
int srs_simple_handshake_with_server(struct SrsHandshakeBytes* hs_bytes, struct  SimpleSocketStream* simplesocketstream);

#endif

