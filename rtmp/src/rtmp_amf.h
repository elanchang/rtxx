#ifndef __RTMP_AMF_H__
#define __RTMP_AMF_H__

struct SrsBuffer;

// AMF0 marker
#define RTMP_AMF0_Number                      0x00
#define RTMP_AMF0_Boolean                     0x01
#define RTMP_AMF0_String                      0x02
#define RTMP_AMF0_Object                      0x03
#define RTMP_AMF0_MovieClip                   0x04 // reserved, not supported
#define RTMP_AMF0_Null                        0x05
#define RTMP_AMF0_Undefined                   0x06
#define RTMP_AMF0_Reference                   0x07
#define RTMP_AMF0_EcmaArray                   0x08
#define RTMP_AMF0_ObjectEnd                   0x09
#define RTMP_AMF0_StrictArray                 0x0A
#define RTMP_AMF0_Date                        0x0B
#define RTMP_AMF0_LongString                  0x0C
#define RTMP_AMF0_UnSupported                 0x0D
#define RTMP_AMF0_RecordSet                   0x0E // reserved, not supported
#define RTMP_AMF0_XmlDocument                 0x0F
#define RTMP_AMF0_TypedObject                 0x10
// AVM+ object is the AMF3 object.
#define RTMP_AMF0_AVMplusObject               0x11
// origin array whos data takes the same form as LengthValueBytes
#define RTMP_AMF0_OriginStrictArray           0x20
// User defined
#define RTMP_AMF0_Invalid                     0x3F

int srs_amf0_write_utf8(struct SrsBuffer* stream, struct AVal value);
int srs_amf0_read_utf8(struct SrsBuffer* stream, struct AVal* value);
int srs_amf0_write_boolean(struct SrsBuffer* stream, bool value);
int srs_amf0_read_boolean(struct SrsBuffer* stream, bool* value);
int srs_amf0_write_string(struct SrsBuffer* stream,struct AVal value);
int srs_amf0_read_string(struct SrsBuffer* stream, struct AVal* value);
int srs_amf0_write_number(struct SrsBuffer* stream, double value);
int srs_amf0_read_number(struct SrsBuffer* stream, double* value);
int srs_amf0_write_null(struct SrsBuffer* stream);
int srs_amf0_read_null(struct SrsBuffer* stream);
int srs_amf0_write_undefined(struct SrsBuffer* stream);
int srs_amf0_read_undefined(struct SrsBuffer* stream);
int srs_amf0_size_utf8(AVal value);
int srs_amf0_size_str(AVal value);
int srs_amf0_size_number();
int srs_amf0_size_null();
int srs_amf0_size_undefined();
int srs_amf0_size_object_eof();
int srs_amf0_size_boolean();


#endif
