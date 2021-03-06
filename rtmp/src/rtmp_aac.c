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
#include "rtmp_aac.h"
#include "snx_rtmp.h"

static bool srs_aac_startswith_adts(struct SrsBuffer* stream){

    char* bytes = srs_buffer_data(stream) + srs_buffer_pos(stream);
    char* p = bytes;
    
    if (!srs_buffer_require(stream,(int)(p - bytes) + 2)) {
        return false;
    }
    
    // matched 12bits 0xFFF,
    // @remark, we must cast the 0xff to char to compare.
    if (p[0] != (char)0xff || (char)(p[1] & 0xf0) != (char)0xf0) {
        return false;
    }
    return true;
}


int snx_aac_is_adts(char* aac_raw_data, int ac_raw_size){

    struct SrsBuffer stream;    
    int ret = ERROR_SUCCESS;
    if((ret =srs_buffer_initialize(&stream,aac_raw_data, ac_raw_size)) != ERROR_SUCCESS){
        return ret;
    }    
    return srs_aac_startswith_adts(&stream);
}


int snx_aac_adts_frame_size(char* aac_raw_data, int ac_raw_size){

    int size = -1;    
    if (!snx_aac_is_adts(aac_raw_data, ac_raw_size)) {
        return size;
    }  
    // adts always 7bytes.
    if (ac_raw_size <= 7) {
        return size;
    }   
    // last 2bits
    int16_t ch3 = aac_raw_data[3];
    // whole 8bits
    int16_t ch4 = aac_raw_data[4];
    // first 3bits
    int16_t ch5 = aac_raw_data[5];
    
    size = ((ch3 << 11) & 0x1800) | ((ch4 << 3) & 0x07f8) | ((ch5 >> 5) & 0x0007);
    
    return size;
}


const char* snx_human_flv_tag_type2string(char type){

    static const char* audio = "Audio";
    static const char* video = "Video";
    static const char* data = "Data";
    static const char* unknown = "Unknown";
    
    switch (type) {
        case SRS_RTMP_TYPE_AUDIO: return audio;
        case SRS_RTMP_TYPE_VIDEO: return video;
        case SRS_RTMP_TYPE_SCRIPT: return data;
        default: return unknown;
    }
    
    return unknown;
}



static int srs_raw_aac_stream_mux_aac2flv(char* frame, int nb_frame, struct SrsRawAacStreamCodec* codec, uint32_t dts, char** flv, int* nb_flv){

    int ret = ERROR_SUCCESS;

    char sound_format = codec->sound_format;
    char sound_type = codec->sound_type;
    char sound_size = codec->sound_size;
    char sound_rate = codec->sound_rate;
    char aac_packet_type = codec->aac_packet_type;

    // for audio frame, there is 1 or 2 bytes header:
    //      1bytes, SoundFormat|SoundRate|SoundSize|SoundType
    //      1bytes, AACPacketType for SoundFormat == 10, 0 is sequence header.
    int size = nb_frame + 1;
    if (sound_format == SrsAudioCodecIdAAC) {
        size += 1;
    }

    char* data =(char*)malloc(size);
    memset(data,0,size);

    char* p = data;
    
   uint8_t audio_header = sound_type & 0x01;
    audio_header |= (sound_size << 1) & 0x02;
    audio_header |= (sound_rate << 2) & 0x0c;
    audio_header |= (sound_format << 4) & 0xf0;
   
    *p++ = audio_header;
    
    if (sound_format == SrsAudioCodecIdAAC) {
        *p++ = aac_packet_type;
    }
    
    memcpy(p, frame, nb_frame);
    
    *flv = data;
    *nb_flv = size;

    return ret;
}


static int srs_raw_aac_stream_mux_sequence_header(struct SrsRawAacStreamCodec* codec, struct AVal* sh){

    int ret = ERROR_SUCCESS;
    char* p;
    // only support aac profile 1-4.
    if (codec->aac_object == SrsAacObjectTypeReserved) {
        return ERROR_AAC_DATA_INVALID;
    }
    
    enum SrsAacObjectType audioObjectType = codec->aac_object;
    char channelConfiguration = codec->channel_configuration;
    char samplingFrequencyIndex = codec->sampling_frequency_index;

    // override the aac samplerate by user specified.
    // @see https://github.com/ossrs/srs/issues/212#issuecomment-64146899
    switch (codec->sound_rate) {
        case SrsAudioSampleRate11025: 
            samplingFrequencyIndex = 0x0a; break;
        case SrsAudioSampleRate22050: 
            samplingFrequencyIndex = 0x07; break;
        case SrsAudioSampleRate44100: 
            samplingFrequencyIndex = 0x04; break;
        default:
            break;
    }
    
    sh->av_val=(char*)malloc(2);
    sh->av_len=2;
    memset(sh->av_val,0,2);
    p= sh->av_val;
    char ch = 0;
    // @see ISO_IEC_14496-3-AAC-2001.pdf
    // AudioSpecificConfig (), page 33
    // 1.6.2.1 AudioSpecificConfig
    // audioObjectType; 5 bslbf
    ch = (audioObjectType << 3) & 0xf8;
    // 3bits left.   
    // samplingFrequencyIndex; 4 bslbf
    ch |= (samplingFrequencyIndex >> 1) & 0x07;
    *(p++)=ch;
    ch = (samplingFrequencyIndex << 7) & 0x80;
    if (samplingFrequencyIndex == 0x0f) {
        return ERROR_AAC_DATA_INVALID;
    }
    // 7bits left.
    // channelConfiguration; 4 bslbf
    ch |= (channelConfiguration << 3) & 0x78;
    // 3bits left.
    *(p++)=ch;    
    // GASpecificConfig(), page 451
    // 4.4.1 Decoder configuration (GASpecificConfig)
    // frameLengthFlag; 1 bslbf
    // dependsOnCoreCoder; 1 bslbf
    // extensionFlag; 1 bslbf
    return ret;
}


static enum SrsAacObjectType srs_aac_ts2rtmp(enum SrsAacProfile profile){

    switch (profile) {
        case SrsAacProfileMain: return SrsAacObjectTypeAacMain;
        case SrsAacProfileLC: return SrsAacObjectTypeAacLC;
        case SrsAacProfileSSR: return SrsAacObjectTypeAacSSR;
        default: return SrsAacObjectTypeReserved;
    }
}



static int srs_raw_aac_stream_adts_demux(struct SrsBuffer* stream, char** pframe, int* pnb_frame, struct SrsRawAacStreamCodec* codec){

    int ret = ERROR_SUCCESS;
    while (!srs_buffer_empty(stream)) {
        int adts_header_start = srs_buffer_pos(stream);
        
        // decode the ADTS.
        // @see aac-iso-13818-7.pdf, page 26
        //      6.2 Audio Data Transport Stream, ADTS
        // @see https://github.com/ossrs/srs/issues/212#issuecomment-64145885
        // byte_alignment()
        
        // adts_fixed_header:
        //      12bits syncword,
        //      16bits left.
        // adts_variable_header:
        //      28bits
        //      12+16+28=56bits
        // adts_error_check:
        //      16bits if protection_absent
        //      56+16=72bits
        // if protection_absent:
        //      require(7bytes)=56bits
        // else
        //      require(9bytes)=72bits
        if (!srs_buffer_require(stream,7)) {
            return ERROR_AAC_ADTS_HEADER;
        }        
        // for aac, the frame must be ADTS format.
        if (!srs_aac_startswith_adts(stream)) {
            return ERROR_AAC_REQUIRED_ADTS;
        }       
        // syncword 12 bslbf
        srs_buffer_read_1bytes(stream);
        // 4bits left.
        // adts_fixed_header(), 1.A.2.2.1 Fixed Header of ADTS
        // ID 1 bslbf
        // layer 2 uimsbf
        // protection_absent 1 bslbf
        int8_t pav = (srs_buffer_read_1bytes(stream) & 0x0f);
        int8_t id = (pav >> 3) & 0x01;
        /*int8_t layer = (pav >> 1) & 0x03;*/
        int8_t protection_absent = pav & 0x01;
        
        /**
        * ID: MPEG identifier, set to '1' if the audio data in the ADTS stream are MPEG-2 AAC (See ISO/IEC 13818-7)
        * and set to '0' if the audio data are MPEG-4. See also ISO/IEC 11172-3, subclause 2.4.2.3.
        */
        if (id != 0x01) {
            //srs_info("adts: id must be 1(aac), actual 0(mp4a). ret=%d", ret);
            
            // well, some system always use 0, but actually is aac format.
            // for example, houjian vod ts always set the aac id to 0, actually 1.
            // we just ignore it, and alwyas use 1(aac) to demux.
            id = 0x01;
        }        
        int16_t sfiv = srs_buffer_read_2bytes(stream);
        // profile 2 uimsbf
        // sampling_frequency_index 4 uimsbf
        // private_bit 1 bslbf
        // channel_configuration 3 uimsbf
        // original/copy 1 bslbf
        // home 1 bslbf
        int8_t profile = (sfiv >> 14) & 0x03;
        int8_t sampling_frequency_index = (sfiv >> 10) & 0x0f;
        /*int8_t private_bit = (sfiv >> 9) & 0x01;*/
        int8_t channel_configuration = (sfiv >> 6) & 0x07;
        /*int8_t original = (sfiv >> 5) & 0x01;*/
        /*int8_t home = (sfiv >> 4) & 0x01;*/
        //int8_t Emphasis; @remark, Emphasis is removed, @see https://github.com/ossrs/srs/issues/212#issuecomment-64154736
        // 4bits left.
        // adts_variable_header(), 1.A.2.2.2 Variable Header of ADTS
        // copyright_identification_bit 1 bslbf
        // copyright_identification_start 1 bslbf
        /*int8_t fh_copyright_identification_bit = (fh1 >> 3) & 0x01;*/
        /*int8_t fh_copyright_identification_start = (fh1 >> 2) & 0x01;*/
        // frame_length 13 bslbf: Length of the frame including headers and error_check in bytes.
        // use the left 2bits as the 13 and 12 bit,
        // the frame_length is 13bits, so we move 13-2=11.
        int16_t frame_length = (sfiv << 11) & 0x1800;
        
        int32_t abfv = srs_buffer_read_3bytes(stream);
        // frame_length 13 bslbf: consume the first 13-2=11bits
        // the fh2 is 24bits, so we move right 24-11=13.
        frame_length |= (abfv >> 13) & 0x07ff;
        // adts_buffer_fullness 11 bslbf
        /*int16_t fh_adts_buffer_fullness = (abfv >> 2) & 0x7ff;*/
        // number_of_raw_data_blocks_in_frame 2 uimsbf
        /*int16_t number_of_raw_data_blocks_in_frame = abfv & 0x03;*/
        // adts_error_check(), 1.A.2.2.3 Error detection
        if (!protection_absent) {
            if (!srs_buffer_require(stream,2)) {
                return ERROR_AAC_ADTS_HEADER;
            }
            // crc_check 16 Rpchof
            /*int16_t crc_check = */srs_buffer_read_2bytes(stream);
        }
        
        // TODO: check the sampling_frequency_index
        // TODO: check the channel_configuration
        
        // raw_data_blocks
        int adts_header_size = srs_buffer_pos(stream) - adts_header_start;
        int raw_data_size = frame_length - adts_header_size;
        if (!srs_buffer_require(stream,raw_data_size)) {
            return ERROR_AAC_ADTS_HEADER;
        }
        
        // the codec info.
        codec->protection_absent = protection_absent;
        codec->aac_object = srs_aac_ts2rtmp((enum SrsAacProfile)profile);
        codec->sampling_frequency_index = sampling_frequency_index;
        codec->channel_configuration = channel_configuration;
        codec->frame_length = frame_length;
        // @see srs_audio_write_raw_frame().
        // TODO: FIXME: maybe need to resample audio.
        codec->sound_format = 10; // AAC
        if (sampling_frequency_index <= 0x0c && sampling_frequency_index > 0x0a) {
            codec->sound_rate = SrsAudioSampleRate5512;
        } else if (sampling_frequency_index <= 0x0a && sampling_frequency_index > 0x07) {
            codec->sound_rate = SrsAudioSampleRate11025;
        } else if (sampling_frequency_index <= 0x07 && sampling_frequency_index > 0x04) {
            codec->sound_rate = SrsAudioSampleRate22050;
        } else if (sampling_frequency_index <= 0x04) {
            codec->sound_rate = SrsAudioSampleRate44100;
        } else {
            codec->sound_rate = SrsAudioSampleRate44100;
            srs_warn("adts invalid sample rate for flv, rate=%#x", sampling_frequency_index);
        }
        codec->sound_type = srs_max(0, srs_min(1, channel_configuration - 1));
        // TODO: FIXME: finger it out the sound size by adts.
        codec->sound_size = 1; // 0(8bits) or 1(16bits).

        // frame data.
        *pframe = srs_buffer_data(stream) + srs_buffer_pos(stream);
        *pnb_frame = raw_data_size;
        srs_buffer_skip(stream,raw_data_size);
        break;
    }
    return ret;
}



static int srs_write_audio_raw_frame(struct Context* context,char* frame, int frame_size, struct SrsRawAacStreamCodec* codec, uint32_t timestamp) {

    int ret = ERROR_SUCCESS;
    char* data = NULL;
    int size = 0;
    if ((ret = srs_raw_aac_stream_mux_aac2flv(frame, frame_size, codec, timestamp, &data, &size)) != ERROR_SUCCESS) {
        return ret;
    }    
    return srs_rtmp_write_packet(context, SRS_RTMP_TYPE_AUDIO, timestamp, data, size);
}

static int srs_write_aac_adts_frame(struct Context* context, struct SrsRawAacStreamCodec* codec, char* frame, int frame_size, uint32_t timestamp) {

    int ret = ERROR_SUCCESS;
    // send out aac sequence header if not sent.
    if (!(context->aac_specific_config.av_len)){
        AVal sh;
        if ((ret = srs_raw_aac_stream_mux_sequence_header(codec, &sh)) != ERROR_SUCCESS) {
            return ret;
        }
        context->aac_specific_config.av_val=(char*)malloc(sh.av_len);
        memset(context->aac_specific_config.av_val,0,sh.av_len);
        memcpy(context->aac_specific_config.av_val,sh.av_val,sh.av_len);
        context->aac_specific_config.av_len=sh.av_len;

        codec->aac_packet_type = 0;

        if ((ret = srs_write_audio_raw_frame(context, (char*)sh.av_val, (int)sh.av_len, codec, timestamp)) != ERROR_SUCCESS) {
            return ret;
        }
        srs_freep(sh.av_val);  
    }
    codec->aac_packet_type = 1;
    return srs_write_audio_raw_frame(context, frame, frame_size, codec, timestamp);
}



static int srs_write_aac_adts_frames(struct Context* context,char sound_format, char sound_rate, char sound_size, char sound_type,
    char* frames, int frames_size, uint32_t timestamp) {

    int ret = ERROR_SUCCESS;
    
    struct SrsBuffer* stream = &context->aac_raw_stream;
    if ((ret = srs_buffer_initialize(stream,frames, frames_size)) != ERROR_SUCCESS) {
        return ret;
    }  
    while (!srs_buffer_empty(stream)) {
        char* frame = NULL;
        int frame_size = 0;
        struct SrsRawAacStreamCodec codec;
        if ((ret = srs_raw_aac_stream_adts_demux(stream, &frame, &frame_size, &codec)) != ERROR_SUCCESS) {
            return ret;
        }
        // override by user specified.
        codec.sound_format = sound_format;
        codec.sound_rate = sound_rate;
        codec.sound_size = sound_size;
        codec.sound_type = sound_type;

        if ((ret = srs_write_aac_adts_frame(context, &codec, frame, frame_size, timestamp)) != ERROR_SUCCESS) {
            return ret;
        }
    }    
    return ret;
}



int snx_audio_write_raw_frame(srs_rtmp_t rtmp, char sound_format, char sound_rate, char sound_size, char sound_type,
    char* frame, int frame_size, uint32_t timestamp) {
    
    int ret = ERROR_SUCCESS;
    
    struct Context* context = (struct Context*)rtmp;
    srs_assert(context);
    
    if (sound_format == SrsAudioCodecIdAAC) {
        // for aac, the frame must be ADTS format.
        if (!snx_aac_is_adts(frame, frame_size)) {
            return ERROR_AAC_REQUIRED_ADTS;
        }        
        // for aac, demux the ADTS to RTMP format.
        return srs_write_aac_adts_frames(context, 
            sound_format, sound_rate, sound_size, sound_type, 
            frame, frame_size, timestamp);
    } else {
        // use codec info for aac.
        struct SrsRawAacStreamCodec codec;
        codec.sound_format = sound_format;
        codec.sound_rate = sound_rate;
        codec.sound_size = sound_size;
        codec.sound_type = sound_type;
        codec.aac_packet_type = 0;

        // for other data, directly write frame.
        return srs_write_audio_raw_frame(context, frame, frame_size, &codec, timestamp);
    }
    return ret;
}


