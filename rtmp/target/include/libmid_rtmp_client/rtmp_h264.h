#ifndef __RTMP_H264_H__
#define __RTMP_H264_H__

struct Context;

#define SRS_RTMP_TYPE_AUDIO 8
// 9 = video
#define SRS_RTMP_TYPE_VIDEO 9
// 18 = script data
#define SRS_RTMP_TYPE_SCRIPT 18

enum SrsVideoCodecId
{
    // set to the zero to reserved, for array map.
    SrsVideoCodecIdReserved = 0,
    SrsVideoCodecIdForbidden = 0,
    SrsVideoCodecIdReserved1 = 1,
    SrsVideoCodecIdReserved2 = 9,    
    // for user to disable video, for example, use pure audio hls.
    SrsVideoCodecIdDisabled = 8,
    SrsVideoCodecIdSorensonH263 = 2,
    SrsVideoCodecIdScreenVideo = 3,
    SrsVideoCodecIdOn2VP6 = 4,
    SrsVideoCodecIdOn2VP6WithAlphaChannel = 5,
    SrsVideoCodecIdScreenVideoVersion2 = 6,
    SrsVideoCodecIdAVC = 7,
};

enum SrsVideoAvcFrameType
{
    // set to the zero to reserved, for array map.
    SrsVideoAvcFrameTypeReserved = 0,
    SrsVideoAvcFrameTypeForbidden = 0,
    SrsVideoAvcFrameTypeReserved1 = 6,
    
    SrsVideoAvcFrameTypeKeyFrame = 1,
    SrsVideoAvcFrameTypeInterFrame = 2,
    SrsVideoAvcFrameTypeDisposableInterFrame = 3,
    SrsVideoAvcFrameTypeGeneratedKeyFrame = 4,
    SrsVideoAvcFrameTypeVideoInfoFrame = 5,
};

enum SrsVideoAvcFrameTrait
{
    // set to the max value to reserved, for array map.
    SrsVideoAvcFrameTraitReserved = 3,
    SrsVideoAvcFrameTraitForbidden = 3,
    
    SrsVideoAvcFrameTraitSequenceHeader = 0,
    SrsVideoAvcFrameTraitNALU = 1,
    SrsVideoAvcFrameTraitSequenceHeaderEOF = 2,
};


enum SrsAvcNaluType
{
    // Unspecified
    SrsAvcNaluTypeReserved = 0,
    SrsAvcNaluTypeForbidden = 0,
    
    // Coded slice of a non-IDR picture slice_layer_without_partitioning_rbsp( )
    SrsAvcNaluTypeNonIDR = 1,
    // Coded slice data partition A slice_data_partition_a_layer_rbsp( )
    SrsAvcNaluTypeDataPartitionA = 2,
    // Coded slice data partition B slice_data_partition_b_layer_rbsp( )
    SrsAvcNaluTypeDataPartitionB = 3,
    // Coded slice data partition C slice_data_partition_c_layer_rbsp( )
    SrsAvcNaluTypeDataPartitionC = 4,
    // Coded slice of an IDR picture slice_layer_without_partitioning_rbsp( )
    SrsAvcNaluTypeIDR = 5,
    // Supplemental enhancement information (SEI) sei_rbsp( )
    SrsAvcNaluTypeSEI = 6,
    // Sequence parameter set seq_parameter_set_rbsp( )
    SrsAvcNaluTypeSPS = 7,
    // Picture parameter set pic_parameter_set_rbsp( )
    SrsAvcNaluTypePPS = 8,
    // Access unit delimiter access_unit_delimiter_rbsp( )
    SrsAvcNaluTypeAccessUnitDelimiter = 9,
    // End of sequence end_of_seq_rbsp( )
    SrsAvcNaluTypeEOSequence = 10,
    // End of stream end_of_stream_rbsp( )
    SrsAvcNaluTypeEOStream = 11,
    // Filler data filler_data_rbsp( )
    SrsAvcNaluTypeFilterData = 12,
    // Sequence parameter set extension seq_parameter_set_extension_rbsp( )
    SrsAvcNaluTypeSPSExt = 13,
    // Prefix NAL unit prefix_nal_unit_rbsp( )
    SrsAvcNaluTypePrefixNALU = 14,
    // Subset sequence parameter set subset_seq_parameter_set_rbsp( )
    SrsAvcNaluTypeSubsetSPS = 15,
    // Coded slice of an auxiliary coded picture without partitioning slice_layer_without_partitioning_rbsp( )
    SrsAvcNaluTypeLayerWithoutPartition = 19,
    // Coded slice extension slice_layer_extension_rbsp( )
    SrsAvcNaluTypeCodedSliceExt = 20,
};




typedef int srs_bool;

static srs_bool srs_h264_is_dvbsp_error(int error_code){
    
    return error_code == ERROR_H264_DROP_BEFORE_SPS_PPS;
}

static srs_bool srs_h264_is_duplicated_sps_error(int error_code){

    return error_code == ERROR_H264_DUPLICATED_SPS;
}

static srs_bool srs_h264_is_duplicated_pps_error(int error_code){

    return error_code == ERROR_H264_DUPLICATED_PPS;
}

//int snx_h264_write_raw_frames(struct Context* rtmp, char* frames, int frames_size, uint32_t dts, uint32_t pts); 
int snx_h264_write_raw_frames(struct Context* rtmp, char* frames, int frames_size, uint32_t dts, uint32_t pts); 


#endif
