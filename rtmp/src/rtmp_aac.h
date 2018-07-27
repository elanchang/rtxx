#ifndef __RTMP_AAC_H__
#define __RTMP_AAC_H__

/**
 * The audio sample rate.
 * @see srs_flv_srates and srs_aac_srates.
 * @doc video_file_format_spec_v10_1.pdf, page 76, E.4.2 Audio Tags
 *      0 = 5.5 kHz = 5512 Hz
 *      1 = 11 kHz = 11025 Hz
 *      2 = 22 kHz = 22050 Hz
 *      3 = 44 kHz = 44100 Hz
 * However, we can extends this table.
 */
enum SrsAudioSampleRate
{
    // set to the max value to reserved, for array map.
    SrsAudioSampleRateReserved = 4,
    SrsAudioSampleRateForbidden = 4,
    
    SrsAudioSampleRate5512 = 0,
    SrsAudioSampleRate11025 = 1,
    SrsAudioSampleRate22050 = 2,
    SrsAudioSampleRate44100 = 3,
};


enum SrsAudioCodecId
{
    // set to the max value to reserved, for array map.
    SrsAudioCodecIdReserved1 = 16,
    SrsAudioCodecIdForbidden = 16,
    
    // for user to disable audio, for example, use pure video hls.
    SrsAudioCodecIdDisabled = 17,
    
    SrsAudioCodecIdLinearPCMPlatformEndian = 0,
    SrsAudioCodecIdADPCM = 1,
    SrsAudioCodecIdMP3 = 2,
    SrsAudioCodecIdLinearPCMLittleEndian = 3,
    SrsAudioCodecIdNellymoser16kHzMono = 4,
    SrsAudioCodecIdNellymoser8kHzMono = 5,
    SrsAudioCodecIdNellymoser = 6,
    SrsAudioCodecIdReservedG711AlawLogarithmicPCM = 7,
    SrsAudioCodecIdReservedG711MuLawLogarithmicPCM = 8,
    SrsAudioCodecIdReserved = 9,
    SrsAudioCodecIdAAC = 10,
    SrsAudioCodecIdSpeex = 11,
    SrsAudioCodecIdReservedMP3_8kHz = 14,
    SrsAudioCodecIdReservedDeviceSpecificSound = 15,
};
    

enum SrsAacProfile
{
    SrsAacProfileReserved = 3,
    
    // @see 7.1 Profiles, aac-iso-13818-7.pdf, page 40
    SrsAacProfileMain = 0,
    SrsAacProfileLC = 1,
    SrsAacProfileSSR = 2,
};

enum SrsAacObjectType
{
    SrsAacObjectTypeReserved = 0,
    SrsAacObjectTypeForbidden = 0,
    
    // Table 1.1 - Audio Object Type definition
    // @see @see ISO_IEC_14496-3-AAC-2001.pdf, page 23
    SrsAacObjectTypeAacMain = 1,
    SrsAacObjectTypeAacLC = 2,
    SrsAacObjectTypeAacSSR = 3,
    
    // AAC HE = LC+SBR
    SrsAacObjectTypeAacHE = 5,
    // AAC HEv2 = LC+SBR+PS
    SrsAacObjectTypeAacHEV2 = 29,
};


struct SrsRawAacStreamCodec
{
    int8_t protection_absent;
    enum SrsAacObjectType aac_object;
    int8_t sampling_frequency_index;
    int8_t channel_configuration;
    int16_t frame_length;

    char sound_format;
    char sound_rate;
    char sound_size;
    char sound_type;
    // 0 for sh; 1 for raw data.
    int8_t aac_packet_type;
};


int snx_aac_is_adts(char* aac_raw_data, int ac_raw_size);
int snx_aac_adts_frame_size(char* aac_raw_data, int ac_raw_size);
int snx_audio_write_raw_frame(void* rtmp, char sound_format, char sound_rate, char sound_size, char sound_type,char* frame, int frame_size, uint32_t timestamp);
const char* snx_human_flv_tag_type2string(char type);





#endif
