#include <FreeRTOS.h>
#include <bsp.h>
#include <task.h>
#include <queue.h>
#include <nonstdlib.h>
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
#include "snx_rtmp.h"

#define printf(fmt, args...) print_msg_queue((fmt), ##args)

static void context_param_init(struct Context *context){

    context->rtmp=NULL;
    context->skt=NULL;
    context->stream_id=0;
    context->h264_sps_pps_sent=false;
    context->h264_sps_changed=false;
    context->h264_pps_changed=false;
    context->rtimeout=SRS_CONSTS_NO_TMMS;
    context->stimeout=SRS_CONSTS_NO_TMMS;
    context->schema=srs_url_schema_normal;
}


#define H264_DATA_SIZE 1280*720*1
srs_rtmp_t snx_rtmp_create(const char* url){

    int ret = ERROR_SUCCESS;
    struct Context* context = (struct Context*)malloc(sizeof(struct Context)); 
    memset(context,0,sizeof(struct Context));
    context_param_init(context);
    av_val_copy_str(&context->url,url);  

    srs_freep(context->skt);
    context->skt=srs_simple_socket_init();
    if ((ret = srs_simple_socket_create_socket(context->skt)) != ERROR_SUCCESS) {
        srs_human_error("Create socket failed, ret=%d\n", ret);
        srs_freep(context);
        return NULL;
    } 
    srs_freep(context->rtmp);
    context->rtmp = srs_rtmp_client_init(context->skt);  


    context->h264_data=(char*)malloc(H264_DATA_SIZE); 
    context->h264_malloc_size=H264_DATA_SIZE;

    return context;
}

int srs_rtmp_set_timeout(srs_rtmp_t rtmp, int recv_timeout_ms, int send_timeout_ms){

    int ret = ERROR_SUCCESS;
    if (!rtmp) {
        return ret;
    }
    struct Context* context = (struct Context*)rtmp;    
    context->stimeout = send_timeout_ms;
    context->rtimeout = recv_timeout_ms;
    
    srs_simple_socket_set_recv_timeout(context->skt,context->rtimeout);
    srs_simple_socket_set_send_timeout(context->skt,context->stimeout);
    return ret;
}

void snx_rtmp_destroy(srs_rtmp_t rtmp){

    if (!rtmp) { 
        return;
    }
    struct Context* context = (struct Context*)rtmp;
    srs_simple_socket_uinit(context->skt);
    srs_rtmp_client_uinit(context->rtmp); 
    
    srs_freep(context->url.av_val);
    srs_freep(context->schema_str.av_val);
    srs_freep(context->host.av_val);
    srs_freep(context->app.av_val);
    srs_freep(context->stream.av_val);   
    srs_freep(context->tcUrl.av_val);  
    srs_freep(context->vhost.av_val);
    srs_freep(context->ip.av_val); 
    srs_freep(context->h264_sps.av_val); 
    srs_freep(context->h264_pps.av_val);
    srs_freep(context->aac_specific_config.av_val);

    srs_freep(context->h264_data);
    srs_freep(context);
}


static int srs_parse_rtmp_url(const char *url, struct Context* context){

    char *p, *end, *col, *ques, *slash;
    int ret = ERROR_SUCCESS;
    
	context->port = 0;
	/* look for usual :// pattern */
	p = strstr(url, "://");
	if(!p) {
		return ERROR_SYSTEM_URL_RESOLVE;
	}
	{
	int len = (int)(p-url);

	if(len == 4 && strncasecmp(url, "rtmp", 4)==0)
         av_val_copy_str(&context->schema_str,"rtmp");
	else if(len == 5 && strncasecmp(url, "rtmpt", 5)==0)
         av_val_copy_str(&context->schema_str,"rtmpt");
	else if(len == 5 && strncasecmp(url, "rtmps", 5)==0)
         av_val_copy_str(&context->schema_str,"rtmps");
	else if(len == 5 && strncasecmp(url, "rtmpe", 5)==0)
	     av_val_copy_str(&context->schema_str,"rtmpe");
	else if(len == 5 && strncasecmp(url, "rtmfp", 5)==0)
	     av_val_copy_str(&context->schema_str,"rtmfp");
	else if(len == 6 && strncasecmp(url, "rtmpte", 6)==0)
	     av_val_copy_str(&context->schema_str,"rtmte");
	else if(len == 6 && strncasecmp(url, "rtmpts", 6)==0)
	      av_val_copy_str(&context->schema_str,"rtmts");
	else {
		goto parsehost;
	}
	}
parsehost:
	/* let's get the hostname */
	p+=3;

	/* check for sudden death */
	if(*p==0) {
		return ERROR_SYSTEM_URL_RESOLVE;
	}

	end   = p + strlen(p);
	col   = strchr(p, ':');
	ques  = strchr(p, '?');
	slash = strchr(p, '/');

	{
	int hostlen;
	if(slash)
		hostlen = slash - p;
	else
		hostlen = end - p;
	if(col && col -p < hostlen)
		hostlen = col - p;

	if(hostlen < 256) {
        char host[256]={0};
        strncpy(host,p,hostlen); 
        av_val_copy_str(&context->host,host);
	} else {
		printf("Hostname exceeds 255 characters!\n");
	}

	p+=hostlen;
	}

	/* get the port number if available */
	if(*p == ':') {
		unsigned int p2;
		p++;
		p2 = atoi(p);
		if(p2 > 65535) {
		} else {
		   context->port = p2;
		}
	}

	if(!slash) {
		return ERROR_SUCCESS;
	}
	p = slash+1;

	{
	/* parse application
	 *
	 * rtmp://host[:port]/app[/appinstance][/...]
	 * application = app[/appinstance]
	 */

	char *slash2, *slash3 = NULL, *slash4 = NULL;
	int applen, appnamelen;

	slash2 = strchr(p, '/');
	if(slash2)
		slash3 = strchr(slash2+1, '/');
	if(slash3)
		slash4 = strchr(slash3+1, '/');

	applen = end-p; /* ondemand, pass all parameters as app */
	appnamelen = applen; /* ondemand length */

	if(ques && strstr(p, "slist=")) { /* whatever it is, the '?' and slist= means we need to use everything as app and parse plapath from slist= */
		appnamelen = ques-p;
	}
	else if(strncmp(p, "ondemand/", 9)==0) {
                /* app = ondemand/foobar, only pass app=ondemand */
                applen = 8;
                appnamelen = 8;
        }
	else { /* app!=ondemand, so app is app[/appinstance] */
		if(slash4)
			appnamelen = slash4-p;
		else if(slash3)
			appnamelen = slash3-p;
		else if(slash2)
			appnamelen = slash2-p;

		applen = appnamelen;
	}

	char app[256]={0};
    strncpy(app,p,applen);
    av_val_copy_str(&context->app,app);
	p += appnamelen;
	}

	if (*p == '/')
		p++;

	if (end-p){
        char stream[256]={0};
        strncpy(stream,p,end-p);
        av_val_copy_str(&context->stream,stream);  
	}
	return ERROR_SUCCESS;
}


static int srs_librtmp_context_parse_uri(struct Context* context){

    int ret = ERROR_SUCCESS; 
    srs_parse_rtmp_url(context->url.av_val, context);    
}


static int srs_librtmp_context_resolve_host(struct Context* context){

    int ret = ERROR_SUCCESS;
    av_val_copy_str(&context->ip,context->host.av_val);    
    return ret;
}



static int srs_rtmp_dns_resolve(srs_rtmp_t rtmp){

    int ret = ERROR_SUCCESS;  
    srs_assert(rtmp != NULL);
    struct Context* context = (struct Context*)rtmp;
    // parse uri
    if ((ret = srs_librtmp_context_parse_uri(context)) != ERROR_SUCCESS){
        return ret;
    }
    // resolve host  //no implementation,only copy host to ip 
    if ((ret = srs_librtmp_context_resolve_host(context)) != ERROR_SUCCESS){
        return ret;
    }
    return ret;

}

static int srs_librtmp_context_connect(struct Context* context){

    int ret = ERROR_SUCCESS;
    
    srs_assert(context->skt);
    
    const char* server_ip = context->ip.av_val;
    if ((ret = srs_simple_socket_connect(context->skt,server_ip, context->port)) != ERROR_SUCCESS) {
        return ret;
    }  
    return ret;  
}


static int srs_rtmp_connect_server(srs_rtmp_t rtmp){

    int ret = ERROR_SUCCESS;   
    srs_assert(rtmp != NULL);
    struct Context* context = (struct Context*)rtmp;
    
    // set timeout if user not set.
    if (context->stimeout == SRS_CONSTS_NO_TMMS) {
        context->stimeout = SRS_SOCKET_DEFAULT_TMMS;
        srs_simple_socket_set_send_timeout(context->skt,context->stimeout);
    }
    if (context->rtimeout == SRS_CONSTS_NO_TMMS) {
        context->rtimeout = SRS_SOCKET_DEFAULT_TMMS;
        srs_simple_socket_set_recv_timeout(context->skt,context->stimeout);
    }
    
    if ((ret = srs_librtmp_context_connect(context)) != ERROR_SUCCESS) {
        return ret;
    }
    
    return ret;
}


static int srs_rtmp_do_simple_handshake(srs_rtmp_t rtmp){

    int ret = ERROR_SUCCESS;
    srs_assert(rtmp != NULL);
    struct Context* context = (struct Context*)rtmp;    
    srs_assert(context->skt != NULL);   
    srs_assert(context->rtmp != NULL);  
    if ((ret =srs_rtmp_client_simple_handshake(context->rtmp,context->skt)) != ERROR_SUCCESS) {
       return ret;
    }
    return ret;    
}


int snx_rtmp_handshake(srs_rtmp_t rtmp){

    int ret = ERROR_SUCCESS;
    if ((ret = srs_rtmp_dns_resolve(rtmp)) != ERROR_SUCCESS) {
        return ret;
    } 
    if ((ret = srs_rtmp_connect_server(rtmp)) != ERROR_SUCCESS) {
        return ret;
    }
    if ((ret = srs_rtmp_do_simple_handshake(rtmp)) != ERROR_SUCCESS) {
        return ret;
    } 
    return ret;
}



static char* srs_generate_normal_tc_url(char* ip, char* app, int port){

    static char tc_url[128]={0};
    char port_str[10]={0};    
    memset(tc_url,0,128);
    sprintf(port_str, "%d",port);    
    strcat(tc_url,"rtmp://");
    strcat(tc_url,ip);
    strcat(tc_url,":");
    strcat(tc_url,port_str);
    strcat(tc_url,"/");
    strcat(tc_url,app);
    return tc_url;
}


int snx_rtmp_connect_app(srs_rtmp_t rtmp){

    int ret = ERROR_SUCCESS;  
    srs_assert(rtmp != NULL);
    struct Context* context = (struct Context*)rtmp;   
    switch(context->schema) {
        case srs_url_schema_normal:
            context->tcUrl.av_val=strdup(srs_generate_normal_tc_url(context->ip.av_val, context->app.av_val, context->port));
            context->tcUrl.av_len=strlen(context->tcUrl.av_val);
            break;
        default:
            break;
    }  
    struct Context* c = context;
    if ((ret = srs_rtmp_client_connect_app(context->rtmp,c->app, context->tcUrl)) != ERROR_SUCCESS){
        return ret;
    }  
    return ret;
}


int snx_rtmp_publish_stream(srs_rtmp_t rtmp){

    int ret = ERROR_SUCCESS;    
    srs_assert(rtmp != NULL);
    struct Context* context = (struct Context*)rtmp;
    if ((ret =srs_rtmp_client_fmle_publish(context->rtmp,context->stream, &(context->stream_id))) != ERROR_SUCCESS){
         return ret;
    }
    return ret;
}



int srs_do_rtmp_create_msg(char type, uint32_t timestamp, char* data, int size, int stream_id, struct SrsSharedPtrMessage** ppmsg)
{
    int ret = ERROR_SUCCESS;
    
    *ppmsg = NULL;
     struct SrsSharedPtrMessage* msg = NULL;

 
    if (type == SrsFrameTypeAudio) {
        struct SrsMessageHeader header;
        srs_message_header_initialize_audio(&header,size,timestamp, stream_id);
        msg = (struct SrsSharedPtrMessage*)malloc(sizeof(struct SrsSharedPtrMessage)); 
        memset(msg,0,sizeof(struct SrsSharedPtrMessage));
        if ((ret = srs_sharedptrmessage_create(msg,&header, data, size)) != ERROR_SUCCESS) {
            srs_freep(msg);
            return ret;
        }
    } else if (type == SrsFrameTypeVideo) {
        struct SrsMessageHeader header;
        srs_message_header_initialize_video(&header,size,timestamp, stream_id);
        msg = (struct SrsSharedPtrMessage*)malloc(sizeof(struct SrsSharedPtrMessage));
        memset(msg,0,sizeof(struct SrsSharedPtrMessage));
        if ((ret = srs_sharedptrmessage_create(msg,&header, data, size)) != ERROR_SUCCESS) {
            srs_freep(msg);
            srs_error("srs_do_rtmp_create_msg");
            return ret;
        }
    } else if (type == SrsFrameTypeScript) {
        struct SrsMessageHeader header;
        srs_message_header_initialize_amf0_script(&header,size, stream_id);
         msg = (struct SrsSharedPtrMessage*)malloc(sizeof(struct SrsSharedPtrMessage));  
        if ((ret = srs_sharedptrmessage_create(msg,&header, data, size)) != ERROR_SUCCESS) {
            srs_freep(msg);
            return ret;
        }
    } else {
        ret = ERROR_STREAM_CASTER_FLV_TAG;
        srs_error("rtmp unknown tag type=%#x. ret=%d", type, ret);
        return ret;
    }

    *ppmsg = msg;

    return ret;
}


int srs_do_rtmp_create_msg_2(srs_rtmp_t rtmp,char type, uint32_t timestamp, char* data, int size, int stream_id)
{
    int ret = ERROR_SUCCESS;
    struct Context* context = (struct Context*)rtmp;

    if (type == SrsFrameTypeVideo) {
        struct SrsMessageHeader header;
        srs_message_header_initialize_video(&header,size,timestamp, stream_id);
        context->video_header.payload_length=header.payload_length;
        context->video_header.message_type=header.message_type;
        context->video_header.perfer_cid=header.perfer_cid;
        context->video_header.timestamp=header.timestamp;
        context->video_header.stream_id=header.stream_id;
        context->video_header.payload=data;
        context->video_header.size=size;
 
    }  else {
        ret = ERROR_STREAM_CASTER_FLV_TAG;
        srs_error("rtmp unknown tag type=%#x. ret=%d", type, ret);
        return ret;
    }
    return ret;
}






static int srs_rtmp_create_msg(char type, uint32_t timestamp, char* data, int size, int stream_id, struct SrsSharedPtrMessage** ppmsg)
{
    int ret = ERROR_SUCCESS;

    // only when failed, we must free the data.
    if ((ret = srs_do_rtmp_create_msg(type, timestamp, data, size, stream_id, ppmsg)) != ERROR_SUCCESS) {
        srs_freep(data);
        srs_error("srs_rtmp_create_msg");
        return ret;
    }
    return ret;
}

static int srs_rtmp_create_msg_2(srs_rtmp_t rtmp,char type, uint32_t timestamp, char* data, int size, int stream_id)
{
    int ret = ERROR_SUCCESS;
    // only when failed, we must free the data.
    if ((ret = srs_do_rtmp_create_msg_2(rtmp,type, timestamp, data, size, stream_id)) != ERROR_SUCCESS) {
        srs_error("srs_rtmp_create_msg");
        return ret;
    }
    return ret;
}






int srs_rtmp_write_packet(srs_rtmp_t rtmp, char type, uint32_t timestamp, char* data, int size)
{
#if 1
    int ret = ERROR_SUCCESS;
    
    srs_assert(rtmp != NULL);
    struct Context* context = (struct Context*)rtmp;
    
    struct SrsSharedPtrMessage* msg = NULL;
    if ((ret = srs_rtmp_create_msg(type, timestamp, data, size, context->stream_id, &msg)) != ERROR_SUCCESS) {
        return ret;
    }

    srs_assert(msg);

    // send out encoded msg.
    if ((ret = srs_protocol_send_and_free_message(context->rtmp->protocol,msg, context->stream_id)) != ERROR_SUCCESS) {
        return ret;
    }
    
    return ret;
#endif
}


int srs_rtmp_write_packet_2(srs_rtmp_t rtmp, char type, uint32_t timestamp, char* data, int size)
{
#if 1
    int ret = ERROR_SUCCESS;
    
    srs_assert(rtmp != NULL);
    struct Context* context = (struct Context*)rtmp;
    
    if ((ret = srs_rtmp_create_msg_2(rtmp,type, timestamp, data, size, context->stream_id)) != ERROR_SUCCESS) {
        return ret;
    }

    // send out encoded msg.
    if ((ret = srs_protocol_send_and_free_message_2(rtmp,context->rtmp->protocol,context->stream_id)) != ERROR_SUCCESS) {
        return ret;
    }
    
    return ret;
#endif
}







