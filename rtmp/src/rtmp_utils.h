
#ifndef __RTMP_UTILS_H__
#define __RTMP_UTILS_H__

#include <assert.h>
#include <stdio.h>



#define PRId64 "lld"

#define srs_min(a, b) (((a) < (b))? (a) : (b))
#define srs_max(a, b) (((a) < (b))? (b) : (a))


// free the p and set to NULL.
// p must be a T*.
#define srs_freep(p) \
    if (p) { \
        free(p); \
        p = NULL; \
    } \
    (void)0

#define srs_assert(expression) assert(expression)

#if 0
#ifndef SRS_AUTO_VERBOSE
    #undef srs_verbose
    #define srs_verbose(msg, ...) (void)0
#endif

#ifndef SRS_AUTO_WARN
    #undef srs_warn
    #define srs_warn(msg, ...) (void)0
#endif

#ifndef SRS_AUTO_ERROR
    #undef srs_error
    #define srs_error(msg, ...) (void)0
#endif

#ifndef SRS_AUTO_INFO
    #undef srs_info
    #define srs_info(msg, ...) (void)0
#endif

#ifndef SRS_AUTO_TRACE
    #undef srs_trace
    #define srs_trace(msg, ...) (void)0
#endif
#endif

#define DEBUG 1
#define printf(fmt, args...) if(DEBUG) print_msg_queue((fmt), ##args)


const char* srs_human_format_time();



// The log function for librtmp.
// User can disable it by define macro SRS_DISABLE_LOG.
// Or user can directly use them, or define the alias by:
//      #define trace(msg, ...) srs_human_trace(msg, ##__VA_ARGS__)
//      #define warn(msg, ...) srs_human_warn(msg, ##__VA_ARGS__)
//      #define error(msg, ...) srs_human_error(msg, ##__VA_ARGS__)
#ifdef SRS_DISABLE_LOG
    #define srs_human_trace(msg, ...) (void)0
    #define srs_human_warn(msg, ...) (void)0
    #define srs_human_error(msg, ...) (void)0
    #define srs_human_verbose(msg, ...) (void)0
    #define srs_human_raw(msg, ...) (void)0
#else
#if 0

    #include <string.h>
    #include <errno.h>
    #define srs_human_trace(msg, ...) \
        //fprintf(stdout, "[T][%d][%s] ", getpid(), srs_human_format_time());\
        fprintf(stdout, msg, ##__VA_ARGS__); fprintf(stdout, "\n")
    #define srs_human_warn(msg, ...) \
        //fprintf(stdout, "[W][%d][%s][%d] ", getpid(), srs_human_format_time(), errno); \
        fprintf(stdout, msg, ##__VA_ARGS__); \
        fprintf(stdout, "\n")
    #define srs_human_error(msg, ...) \
        //fprintf(stderr, "[E][%d][%s][%d] ", getpid(), srs_human_format_time(), errno);\
        fprintf(stderr, msg, ##__VA_ARGS__); \
        fprintf(stderr, " (%s)\n", strerror(errno))
    #define srs_human_verbose(msg, ...) (void)0
    #define srs_human_raw(msg, ...) printf(msg, ##__VA_ARGS__)




#define srs_trace(msg, ...) \
        //fprintf(stdout, "[T][%d][%s] ", getpid(), srs_human_format_time());\
        fprintf(stdout, msg, ##__VA_ARGS__); fprintf(stdout, "\n")

#define srs_info(msg, ...) \
        //fprintf(stdout, "[T][%d][%s] ", getpid(), srs_human_format_time());\
        fprintf(stdout, msg, ##__VA_ARGS__); fprintf(stdout, "\n")

#define srs_verbose(msg, ...) \
        //fprintf(stdout, "[T][%d][%s] ", getpid(), srs_human_format_time());\
        fprintf(stdout, msg, ##__VA_ARGS__); fprintf(stdout, "\n")

#define srs_warn(msg, ...) \
        //fprintf(stdout, "[T][%d][%s] ", getpid(), srs_human_format_time());\
        fprintf(stdout, msg, ##__VA_ARGS__); fprintf(stdout, "\n")

#define srs_error(msg, ...) \
        //fprintf(stdout, "[T][%d][%s] ", getpid(), srs_human_format_time());\
        fprintf(stdout, msg, ##__VA_ARGS__); fprintf(stdout, "\n")
#endif

#define srs_trace print_msg
#define srs_info print_msg
#define srs_verbose print_msg
#define srs_warn print_msg
#define srs_error print_msg
#define srs_human_trace print_msg
#define srs_human_warn  print_msg
#define srs_human_error print_msg
#define srs_human_verbose print_msg
#define srs_human_raw print_msg


    
#endif

#endif
