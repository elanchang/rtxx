#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/time.h>
#include <sys/uio.h>  //for iovec


const char* srs_human_format_time(){
#if 0
    static char buf[24];


    struct timeval tv;
    static char buf[24];  
    memset(buf, 0, sizeof(buf));   
    // clock time
    if (gettimeofday(&tv, NULL) == -1) {
        return buf;
    }    
    // to calendar time
    struct tm* tm;
    if ((tm = localtime((const time_t*)&tv.tv_sec)) == NULL) {
        return buf;
    }   
    snprintf(buf, sizeof(buf), 
        "%d-%02d-%02d %02d:%02d:%02d.%03d", 
        1900 + tm->tm_year, 1 + tm->tm_mon, tm->tm_mday, 
        tm->tm_hour, tm->tm_min, tm->tm_sec, 
        (int)(tv.tv_usec / 1000));
   
    buf[sizeof(buf) - 1] = 0;   

    return buf;
#endif
}

