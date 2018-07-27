#ifndef __RTMP_STRING_H__
#define __RTMP_STRING_H__


typedef struct AVal
{
    char *av_val;
    int av_len;
} AVal;

#define AVC(str)	{str,sizeof(str)-1}
#define AVMATCH(a1,a2)	((a1)->av_len == (a2)->av_len && !memcmp((a1)->av_val,(a2)->av_val,(a1)->av_len))


static void av_val_copy_str(AVal* string,const char* url)
{
    string->av_val =strdup(url);
    string->av_len=strlen(string->av_val);
}

#endif
