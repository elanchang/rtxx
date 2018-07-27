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
#include "rtmp_chunk.h"

struct SrsChunkStream* srs_chunkstream_init(int cid){
    
    struct SrsChunkStream* srschunkstream = (struct SrsChunkStream*)malloc(sizeof(struct SrsChunkStream));
    memset(srschunkstream,0,sizeof(struct SrsChunkStream));
    srschunkstream->fmt = 0;
    srschunkstream->cid = cid;
    srschunkstream->extended_timestamp = false;
    srschunkstream->msg = NULL;
    srschunkstream->msg_count = 0;
    return srschunkstream;
}

void srs_chunkstream_uinit(struct SrsChunkStream* srschunkstream){

    srs_freep(srschunkstream->msg->payload);
    srs_freep(srschunkstream->msg);
    srs_freep(srschunkstream);
}

