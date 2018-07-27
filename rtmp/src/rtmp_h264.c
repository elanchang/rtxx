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
#include "rtmp_socket.h"
#include "rtmp_msg.h"
#include "rtmp_chunk.h"
#include "rtmp_cmd.h"
#include "rtmp_protocol.h"
#include "rtmp_client.h"
#include "rtmp_h264.h"
#include "snx_rtmp.h"

static bool srs_avc_startswith_annexb(struct SrsBuffer* stream, int* pnb_start_code);

static int srs_raw_h264stream_annexb_demux(struct SrsBuffer* stream, char** pframe, int* pnb_frame)
{

    int ret = ERROR_SUCCESS;
    *pframe = NULL;
    *pnb_frame = 0;

    while (!srs_buffer_empty(stream)) {
        // each frame must prefixed by annexb format.
        // about annexb, @see ISO_IEC_14496-10-AVC-2003.pdf, page 211.
        int pnb_start_code = 0;
        if (!srs_avc_startswith_annexb(stream, &pnb_start_code)) {
            return ERROR_H264_API_NO_PREFIXED;
        }
        int start = srs_buffer_pos(stream)+ pnb_start_code;
        
        // find the last frame prefixed by annexb format.
        srs_buffer_skip(stream,pnb_start_code);
        while (!srs_buffer_empty(stream)) {
            if (srs_avc_startswith_annexb(stream, NULL)) {
                break;
            }
            srs_buffer_skip(stream,1);
        }  
        // demux the frame.
        *pnb_frame = srs_buffer_pos(stream) - start;
        *pframe = srs_buffer_data(stream) + start;
        break;
    }

    return ret;
}

static bool srs_raw_h264stream_is_sps(char* frame, int nb_frame)
{
    srs_assert(nb_frame > 0);
    // 5bits, 7.3.1 NAL unit syntax, 
    // ISO_IEC_14496-10-AVC-2003.pdf, page 44.
    //  7: SPS, 8: PPS, 5: I Frame, 1: P Frame
    uint8_t nal_unit_type = (char)frame[0] & 0x1f;

    return nal_unit_type == 7;
}

static bool srs_raw_h264stream_is_pps(char* frame, int nb_frame)
{
    srs_assert(nb_frame > 0);
    // 5bits, 7.3.1 NAL unit syntax, 
    // ISO_IEC_14496-10-AVC-2003.pdf, page 44.
    //  7: SPS, 8: PPS, 5: I Frame, 1: P Frame
    uint8_t nal_unit_type = (char)frame[0] & 0x1f;

    return nal_unit_type == 8;
}

static int srs_raw_h264stream_sps_demux(char* frame, int nb_frame, AVal* sps)
{
    int ret = ERROR_SUCCESS;
   

    // atleast 1bytes for SPS to decode the type, profile, constrain and level.
    if (nb_frame < 4) {
        return ret;
    }
    if (nb_frame > 0) {
         sps->av_val=(char*)malloc(nb_frame);
         memset(sps->av_val,0,nb_frame);
         memcpy(sps->av_val,frame,nb_frame);
         sps->av_len=nb_frame;
    }    
    return ret;
}

static int srs_raw_h264stream_pps_demux(char* frame, int nb_frame, AVal* pps)
{
    int ret = ERROR_SUCCESS;

    if (nb_frame > 0) {
        pps->av_val=(char*)malloc(nb_frame);
        memset(pps->av_val,0,nb_frame);
        memcpy(pps->av_val,frame,nb_frame);
        pps->av_len=nb_frame;
    }
    return ret;
}

static int srs_raw_h264stream_mux_sequence_header(AVal* sps, AVal* pps, uint32_t dts, uint32_t pts, AVal* sh)
{
    int ret = ERROR_SUCCESS;

    // 5bytes sps/pps header:
    //      configurationVersion, AVCProfileIndication, profile_compatibility,
    //      AVCLevelIndication, lengthSizeMinusOne
    // 3bytes size of sps:
    //      numOfSequenceParameterSets, sequenceParameterSetLength(2B)
    // Nbytes of sps.
    //      sequenceParameterSetNALUnit
    // 3bytes size of pps:
    //      numOfPictureParameterSets, pictureParameterSetLength
    // Nbytes of pps:
    //      pictureParameterSetNALUnit

    int nb_packet = 5 
        + 3 + (int)sps->av_len
        + 3 + (int)pps->av_len;


    //char* packet = new char[nb_packet];
    char* packet=(char*)malloc(nb_packet);
    memset(packet,0,nb_packet);

    // use stream to generate the h264 packet.
    struct SrsBuffer stream;
    if ((ret = srs_buffer_initialize(&stream,packet, nb_packet)) != ERROR_SUCCESS) {
        return ret;
    }
    
    // decode the SPS: 
    // @see: 7.3.2.1.1, ISO_IEC_14496-10-AVC-2012.pdf, page 62
    if (true) {
        srs_assert((int)sps->av_len >= 4);
        char* frame = (char*)sps->av_val;
    
        // @see: Annex A Profiles and levels, ISO_IEC_14496-10-AVC-2003.pdf, page 205
        //      Baseline profile profile_idc is 66(0x42).
        //      Main profile profile_idc is 77(0x4d).
        //      Extended profile profile_idc is 88(0x58).
        uint8_t profile_idc = frame[1];
        //uint8_t constraint_set = frame[2];
        uint8_t level_idc = frame[3];

        // generate the sps/pps header
        // 5.3.4.2.1 Syntax, ISO_IEC_14496-15-AVC-format-2012.pdf, page 16
        // configurationVersion
        srs_buffer_write_1bytes(&stream,0x01);
        // AVCProfileIndication
        srs_buffer_write_1bytes(&stream,profile_idc);
        // profile_compatibility
        srs_buffer_write_1bytes(&stream,0x00);
        // AVCLevelIndication
        srs_buffer_write_1bytes(&stream,level_idc);
        // lengthSizeMinusOne, or NAL_unit_length, always use 4bytes size,
        // so we always set it to 0x03.
        srs_buffer_write_1bytes(&stream,0x03);
    }
    
    // sps
    if (true) {
        // 5.3.4.2.1 Syntax, ISO_IEC_14496-15-AVC-format-2012.pdf, page 16
        // numOfSequenceParameterSets, always 1
        srs_buffer_write_1bytes(&stream,0x01);
        // sequenceParameterSetLength
        srs_buffer_write_2bytes(&stream,sps->av_len);
        // sequenceParameterSetNALUnit   
        srs_buffer_write_string(&stream,(*sps));
    }
    
    // pps
    if (true) {
        // 5.3.4.2.1 Syntax, ISO_IEC_14496-15-AVC-format-2012.pdf, page 16
        // numOfPictureParameterSets, always 1
        srs_buffer_write_1bytes(&stream,0x01);
        // pictureParameterSetLength
        srs_buffer_write_2bytes(&stream,pps->av_len);
        // pictureParameterSetNALUnit
        srs_buffer_write_string(&stream,(*pps));
    }

    // TODO: FIXME: for more profile.
    // 5.3.4.2.1 Syntax, ISO_IEC_14496-15-AVC-format-2012.pdf, page 16
    // profile_idc == 100 || profile_idc == 110 || profile_idc == 122 || profile_idc == 144
    //char sps_pps[1024]={0};
    //hexdump(packet, nb_packet);

    //memcpy(sps_pps,packet,nb_packet);
    //strncpy(sps_pps,packet,nb_packet); //bug 
    //hexdump(sps_pps, nb_packet);
    sh->av_val=packet;
    sh->av_len=nb_packet;
    //hexdump(sh->av_val, sh->av_len);
    //av_val_copy_str(sh,sps_pps);  bug 
   
    return ret;
}

static int srs_raw_h264stream_mux_ipb_frame(char* frame, int nb_frame, AVal* ibp)
{
    int ret = ERROR_SUCCESS;
  
    // 4bytes size of nalu:
    //      NALUnitLength
    // Nbytes of nalu.
    //      NALUnit
    int nb_packet = 4 + nb_frame;
    char* packet = (char*)malloc(nb_packet);
    memset(packet,0,nb_packet);

    //SrsAutoFreeA(char, packet);
    
    // use stream to generate the h264 packet.
    struct SrsBuffer stream;
    if ((ret = srs_buffer_initialize(&stream,packet, nb_packet)) != ERROR_SUCCESS) {
        return ret;
    }

    // 5.3.4.2.1 Syntax, ISO_IEC_14496-15-AVC-format-2012.pdf, page 16
    // lengthSizeMinusOne, or NAL_unit_length, always use 4bytes size
    uint32_t NAL_unit_length = nb_frame;
    
    // mux the avc NALU in "ISO Base Media File Format" 
    // from ISO_IEC_14496-15-AVC-format-2012.pdf, page 20
    // NALUnitLength
    srs_buffer_write_4bytes(&stream,NAL_unit_length);
    // NALUnit
    srs_buffer_write_bytes(&stream,frame, nb_frame);

    ibp->av_val=packet;
    ibp->av_len=nb_packet;
    return ret;
}


static int srs_raw_h264stream_mux_ipb_frame_2(struct Context* context,char* frame, int nb_frame)
{
    int ret = ERROR_SUCCESS;
  
    // 4bytes size of nalu:
    //      NALUnitLength
    // Nbytes of nalu.
    //      NALUnit
    //int nb_packet = 4 + nb_frame;
    //char* packet = (char*)malloc(nb_packet);
    memset(context->h264_data,0,context->h264_malloc_size);

    //SrsAutoFreeA(char, packet);
    
    // use stream to generate the h264 packet.
    struct SrsBuffer stream;
    if ((ret = srs_buffer_initialize(&stream,(context->h264_data+5),(context->h264_malloc_size-5))) != ERROR_SUCCESS) {
        return ret;
    }

    // 5.3.4.2.1 Syntax, ISO_IEC_14496-15-AVC-format-2012.pdf, page 16
    // lengthSizeMinusOne, or NAL_unit_length, always use 4bytes size
    uint32_t NAL_unit_length = nb_frame;
    
    // mux the avc NALU in "ISO Base Media File Format" 
    // from ISO_IEC_14496-15-AVC-format-2012.pdf, page 20
    // NALUnitLength
    srs_buffer_write_4bytes(&stream,NAL_unit_length);
    // NALUnit
    srs_buffer_write_bytes(&stream,frame, nb_frame);
    context->h264_data_size=nb_frame;

    return ret;
}





static int srs_raw_h264stream_mux_avc2flv(AVal* video, int8_t frame_type, int8_t avc_packet_type, uint32_t dts, uint32_t pts, char** flv, int* nb_flv)
{
    int ret = ERROR_SUCCESS;
   
    // for h264 in RTMP video payload, there is 5bytes header:
    //      1bytes, FrameType | CodecID
    //      1bytes, AVCPacketType
    //      3bytes, CompositionTime, the cts.
    // @see: E.4.3 Video Tags, video_file_format_spec_v10_1.pdf, page 78

    int size = (int)video->av_len + 5;
    
    char* data = (char*)malloc(size);
    memset(data,0,size);

    char* p = data;
    
    // @see: E.4.3 Video Tags, video_file_format_spec_v10_1.pdf, page 78
    // Frame Type, Type of video frame.
    // CodecID, Codec Identifier.
    // set the rtmp header
    *p++ = (frame_type << 4) | SrsVideoCodecIdAVC;
    
    // AVCPacketType
    *p++ = avc_packet_type;

    // CompositionTime
    // pts = dts + cts, or 
    // cts = pts - dts.
    // where cts is the header in rtmp video packet payload header.
    uint32_t cts = pts - dts;
    char* pp = (char*)&cts;
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
    
    // h.264 raw data.
    memcpy(p, video->av_val, video->av_len);
    

    *flv = data;
    *nb_flv = size;

    return ret;
}


static int srs_raw_h264stream_mux_avc2flv_2(struct Context* context, int8_t frame_type, int8_t avc_packet_type, uint32_t dts, uint32_t pts, char** flv, int* nb_flv)
{
    int ret = ERROR_SUCCESS;
   
    // for h264 in RTMP video payload, there is 5bytes header:
    //      1bytes, FrameType | CodecID
    //      1bytes, AVCPacketType
    //      3bytes, CompositionTime, the cts.
    // @see: E.4.3 Video Tags, video_file_format_spec_v10_1.pdf, page 78

    //int size = (int)video->av_len + 5;
    
    //char* data = (char*)malloc(size);
    //memset(data,0,size);

    //char* p = data;
    char* p = context->h264_data;
    // @see: E.4.3 Video Tags, video_file_format_spec_v10_1.pdf, page 78
    // Frame Type, Type of video frame.
    // CodecID, Codec Identifier.
    // set the rtmp header
    *p++ = (frame_type << 4) | SrsVideoCodecIdAVC;
    
    // AVCPacketType
    *p++ = avc_packet_type;

    // CompositionTime
    // pts = dts + cts, or 
    // cts = pts - dts.
    // where cts is the header in rtmp video packet payload header.
    uint32_t cts = pts - dts;
    char* pp = (char*)&cts;
    *p++ = pp[2];
    *p++ = pp[1];
    *p++ = pp[0];
    
    // h.264 raw data.
    //memcpy(p, video->av_val, video->av_len);
    

    *flv = context->h264_data;
    *nb_flv = context->h264_data_size+9;

    return ret;
}






static bool srs_avc_startswith_annexb(struct SrsBuffer* stream, int* pnb_start_code)
{
    char* bytes = srs_buffer_data(stream) + srs_buffer_pos(stream);
    char* p = bytes;
    
    for (;;) {
        if (!srs_buffer_require(stream,(int)(p - bytes + 3))) {
            return false;
        }
        
        // not match
        if (p[0] != (char)0x00 || p[1] != (char)0x00) {
            return false;
        }
        
        // match N[00] 00 00 01, where N>=0
        if (p[2] == (char)0x01) {
            if (pnb_start_code) {
                *pnb_start_code = (int)(p - bytes) + 3;
            }
            return true;
        }
        
        p++;
    }    
    return false;
}


int snx_h264_startswith_annexb(char* h264_raw_data, int h264_raw_size, int* pnb_start_code)
{
    struct SrsBuffer stream;
    if (srs_buffer_initialize(&stream,h264_raw_data,h264_raw_size) != ERROR_SUCCESS) {
        return false;
    }
    return srs_avc_startswith_annexb(&stream, pnb_start_code);
}


static int srs_write_h264_ipb_frame(struct Context* context, char* frame, int frame_size, uint32_t dts, uint32_t pts) 
{
    int ret = ERROR_SUCCESS;
    // when sps or pps not sent, ignore the packet.
    // @see https://github.com/ossrs/srs/issues/203
    if (!context->h264_sps_pps_sent) {
        srs_verbose("ERROR_H264_DROP_BEFORE_SPS_PPS\n");
        return ERROR_H264_DROP_BEFORE_SPS_PPS;
    }
    
    // 5bits, 7.3.1 NAL unit syntax,
    // ISO_IEC_14496-10-AVC-2003.pdf, page 44.
    //  5: I Frame, 1: P/B Frame
    // @remark we already group sps/pps to sequence header frame;
    //      for I/P NALU, we send them in isolate frame, each NALU in a frame;
    //      for other NALU, for example, AUD/SEI, we just ignore them, because
    //      AUD used in annexb to split frame, while SEI generally we can ignore it.
    // TODO: maybe we should group all NALUs split by AUD to a frame.
    enum SrsAvcNaluType nut = (enum SrsAvcNaluType)(frame[0] & 0x1f);
    if (nut != SrsAvcNaluTypeIDR && nut != SrsAvcNaluTypeNonIDR) {
         srs_verbose("nut != SrsAvcNaluTypeIDR && nut != SrsAvcNaluTypeNonIDR\n");
        return ret;
    }
    
    // for IDR frame, the frame is keyframe.
    enum SrsVideoAvcFrameType frame_type = SrsVideoAvcFrameTypeInterFrame;
    if (nut == SrsAvcNaluTypeIDR) {
        frame_type = SrsVideoAvcFrameTypeKeyFrame;
    }
    AVal ibp;
    if ((ret = srs_raw_h264stream_mux_ipb_frame(frame, frame_size, &ibp)) != ERROR_SUCCESS) {
        return ret;
    }
    
    int8_t avc_packet_type = SrsVideoAvcFrameTraitNALU;
    char* flv = NULL;
    int nb_flv = 0;
    if ((ret = srs_raw_h264stream_mux_avc2flv(&ibp, frame_type, avc_packet_type, dts, pts, &flv, &nb_flv)) != ERROR_SUCCESS) {
        return ret;
    }
    // the timestamp in rtmp message header is dts.
    uint32_t timestamp = dts;
    ret=srs_rtmp_write_packet(context, SRS_RTMP_TYPE_VIDEO, timestamp, flv, nb_flv);
    //srs_freep(flv);
    srs_freep(ibp.av_val);
    return ret;   
}



static int srs_write_h264_ipb_frame_2(struct Context* context, char* frame, int frame_size, uint32_t dts, uint32_t pts) 
{
    int ret = ERROR_SUCCESS;
    // when sps or pps not sent, ignore the packet.
    // @see https://github.com/ossrs/srs/issues/203
    if (!context->h264_sps_pps_sent) {
        srs_verbose("ERROR_H264_DROP_BEFORE_SPS_PPS\n");
        return ERROR_H264_DROP_BEFORE_SPS_PPS;
    }
    
    // 5bits, 7.3.1 NAL unit syntax,
    // ISO_IEC_14496-10-AVC-2003.pdf, page 44.
    //  5: I Frame, 1: P/B Frame
    // @remark we already group sps/pps to sequence header frame;
    //      for I/P NALU, we send them in isolate frame, each NALU in a frame;
    //      for other NALU, for example, AUD/SEI, we just ignore them, because
    //      AUD used in annexb to split frame, while SEI generally we can ignore it.
    // TODO: maybe we should group all NALUs split by AUD to a frame.
    enum SrsAvcNaluType nut = (enum SrsAvcNaluType)(frame[0] & 0x1f);
    if (nut != SrsAvcNaluTypeIDR && nut != SrsAvcNaluTypeNonIDR) {
         srs_verbose("nut != SrsAvcNaluTypeIDR && nut != SrsAvcNaluTypeNonIDR\n");
        return ret;
    }
    
    // for IDR frame, the frame is keyframe.
    enum SrsVideoAvcFrameType frame_type = SrsVideoAvcFrameTypeInterFrame;
    if (nut == SrsAvcNaluTypeIDR) {
        frame_type = SrsVideoAvcFrameTypeKeyFrame;
    }
    
    if ((ret = srs_raw_h264stream_mux_ipb_frame_2(context,frame, frame_size)) != ERROR_SUCCESS) {
        return ret;
    }
    
    int8_t avc_packet_type = SrsVideoAvcFrameTraitNALU;
    char* flv = NULL;
    int nb_flv = 0;
    if ((ret = srs_raw_h264stream_mux_avc2flv_2(context, frame_type, avc_packet_type, dts, pts, &flv, &nb_flv)) != ERROR_SUCCESS) {
        return ret;
    }
    // the timestamp in rtmp message header is dts.
    uint32_t timestamp = dts;
    ret=srs_rtmp_write_packet_2(context, SRS_RTMP_TYPE_VIDEO, timestamp, flv, nb_flv);
    return ret;   
}






/**
* write the h264 sps/pps in context over RTMP.
*/
static int srs_write_h264_sps_pps(struct Context* context, uint32_t dts, uint32_t pts)
{

    int ret = ERROR_SUCCESS;    
    // send when sps or pps changed.
    if (!context->h264_sps_changed && !context->h264_pps_changed) {
        return ret;
    }
  
    // h264 raw to h264 packet.
    AVal sh;
    if ((ret = srs_raw_h264stream_mux_sequence_header(&context->h264_sps, &context->h264_pps, dts, pts, &sh)) != ERROR_SUCCESS) {
        return ret;
    }
    // h264 packet to flv packet.
    int8_t frame_type = SrsVideoAvcFrameTypeKeyFrame;
    int8_t avc_packet_type = SrsVideoAvcFrameTraitSequenceHeader;
    char* flv = NULL;
    int nb_flv = 0;


    
    if ((ret = srs_raw_h264stream_mux_avc2flv(&sh, frame_type, avc_packet_type, dts, pts, &flv, &nb_flv)) != ERROR_SUCCESS) {
        srs_freep(sh.av_val);
        return ret;
    }
    // reset sps and pps.
    context->h264_sps_changed = false;
    context->h264_pps_changed = false;
    context->h264_sps_pps_sent = true;
    // the timestamp in rtmp message header is dts.
    uint32_t timestamp = dts;
    //print_msg("timestamp=%ld\n",timestamp);

    
    ret=srs_rtmp_write_packet(context, SRS_RTMP_TYPE_VIDEO, timestamp, flv, nb_flv);
    //srs_freep(flv);
    srs_freep(sh.av_val);
    return ret;
    
}

static int sps_count_flag=0;
static int pps_count_flag=0;
static int sps_pps_count_flag=0;

/**
* write h264 raw frame, maybe sps/pps/IPB-frame.
*/
static int srs_write_h264_raw_frame(struct Context* context, char* frame, int frame_size, uint32_t dts, uint32_t pts) {
    int ret = ERROR_SUCCESS;
    if (frame_size <= 0) {
        return ret;
    }

    // for sps
    if (srs_raw_h264stream_is_sps(frame, frame_size)) {

       if(sps_count_flag==0)
       {

            AVal sps;
            sps_count_flag++;
            if ((ret = srs_raw_h264stream_sps_demux(frame, frame_size, &sps)) != ERROR_SUCCESS) {
                srs_freep(sps.av_val);
                return ret;
            }
            //if (strcmp(context->h264_sps.av_val, sps.av_val) == 0) {   //segmenet fault  
            //    return ERROR_H264_DUPLICATED_SPS;
            //}
            context->h264_sps_changed = true;
            srs_freep(context->h264_sps.av_val);
            context->h264_sps.av_val=(char*)malloc(sps.av_len);      
            memset(context->h264_sps.av_val,0,sps.av_len);
            memcpy(context->h264_sps.av_val,sps.av_val,sps.av_len);
            context->h264_sps.av_len=sps.av_len;
            srs_freep(sps.av_val);
        }
        return ret;
    }

    // for pps
    if (srs_raw_h264stream_is_pps(frame, frame_size)) {

      if(pps_count_flag==0)
      {
            AVal pps;
            pps_count_flag++;
            if ((ret = srs_raw_h264stream_pps_demux(frame, frame_size, &pps)) != ERROR_SUCCESS) {
                srs_freep(pps.av_val);
                return ret;
            }
            //if (strcmp(context->h264_pps.av_val, pps.av_val) == 0) {  
            //  return ERROR_H264_DUPLICATED_SPS;
            //}
            context->h264_pps_changed = true;
            srs_freep(context->h264_pps.av_val);
            context->h264_pps.av_val=(char*)malloc(pps.av_len);
            memset(context->h264_pps.av_val,0,pps.av_len);
            memcpy(context->h264_pps.av_val,pps.av_val,pps.av_len);
            context->h264_pps.av_len=pps.av_len;
            srs_freep(pps.av_val);
       }
        return ret;
    }
    // ignore others.
    // 5bits, 7.3.1 NAL unit syntax,
    // ISO_IEC_14496-10-AVC-2003.pdf, page 44.
    //  7: SPS, 8: PPS, 5: I Frame, 1: P Frame, 9: AUD
    enum SrsAvcNaluType nut = (enum SrsAvcNaluType)(frame[0] & 0x1f);
    if (nut != SrsAvcNaluTypeSPS && nut != SrsAvcNaluTypePPS
        && nut != SrsAvcNaluTypeIDR && nut != SrsAvcNaluTypeNonIDR
        && nut != SrsAvcNaluTypeAccessUnitDelimiter
    ) {
        return ret;
    }   
    // send pps+sps before ipb frames when sps/pps changed.
    if(sps_pps_count_flag==0)
    {
        if ((ret = srs_write_h264_sps_pps(context, dts, pts)) != ERROR_SUCCESS) {
             srs_verbose("srs_write_h264_sps_pps return\n");
            return ret;
        }
        sps_pps_count_flag++;
     }
    // ibp frame.
    //return srs_write_h264_ipb_frame(context, frame, frame_size, dts, pts);
    return srs_write_h264_ipb_frame_2(context, frame, frame_size, dts, pts);
}


int snx_h264_write_raw_frames(struct Context* rtmp, char* frames, int frames_size, uint32_t dts, uint32_t pts) 
{
    int ret = ERROR_SUCCESS;
    
    srs_assert(frames != NULL);
    srs_assert(frames_size > 0);
    
    srs_assert(rtmp != NULL);
    struct Context* context = (struct Context*)rtmp;
    
    if ((ret = srs_buffer_initialize(&(context->h264_raw_stream),frames, frames_size)) != ERROR_SUCCESS) {
        return ret;
    }
    
    int error_code_return = ret;
    
    // send each frame.
    while (!srs_buffer_empty(&(context->h264_raw_stream))) {
        char* frame = NULL;
        int frame_size = 0;
        if ((ret = srs_raw_h264stream_annexb_demux(&context->h264_raw_stream,&frame, &frame_size)) != ERROR_SUCCESS) {
            return ret;
        }
       
        // ignore invalid frame,
        // atleast 1bytes for SPS to decode the type
        if (frame_size <= 0) {
            srs_verbose("frame_size <= 0\n");   
            continue;
        }
        // it may be return error, but we must process all packets.
        if ((ret = srs_write_h264_raw_frame(context, frame, frame_size, dts, pts)) != ERROR_SUCCESS) {
            error_code_return = ret;
            
            // ignore known error, process all packets.
            if (srs_h264_is_dvbsp_error(ret)
                || srs_h264_is_duplicated_sps_error(ret)
                || srs_h264_is_duplicated_pps_error(ret)
            ) {
                continue;
            }
            
            return ret;
        }
    }
    
    return error_code_return;
}



