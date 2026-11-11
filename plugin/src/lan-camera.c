/* OBS LAN Camera 0.1. GPL-2.0-or-later. */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <bcrypt.h>
#include <iphlpapi.h>
#include <shellapi.h>
#include <obs-module.h>
#include <util/platform.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "release.h"
#include "updater.h"
#include "pairing.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-lan-camera", "en-US")
MODULE_EXPORT const char *obs_module_description(void) { return "OBS LAN Camera " OLC_VERSION ": encrypted Android camera, GitHub downloads and updates"; }

#define MAX_RECORD (4*1024*1024)
#define MAGIC "OLC1"
struct camera {
    obs_source_t *source;
    CRITICAL_SECTION lock;
    HANDLE listener_thread, video_thread;
    SOCKET listener, client;
    volatile LONG stopping, connected,pairing_shown;
    int port;
    bool enabled;
    char token[33], address[64], status[256];
    unsigned width,height;
    uint64_t frames,audio_frames,last_audio_ns;
    char last_error[2049];
};
struct session { struct camera *cam; SOCKET socket; };
struct crypto { BCRYPT_ALG_HANDLE alg; BCRYPT_KEY_HANDLE key; };

static bool random_bytes(void *p, ULONG n) { return BCryptGenRandom(NULL,p,n,BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0; }
static bool hash_key(const uint8_t *data, size_t len, uint8_t out[32])
{
    BCRYPT_ALG_HANDLE a=NULL; BCRYPT_HASH_HANDLE h=NULL;
    bool ok=BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0;
    if(ok) ok=BCryptCreateHash(a,&h,NULL,0,NULL,0,0)>=0;
    if(ok) ok=BCryptHashData(h,(PUCHAR)data,(ULONG)len,0)>=0 && BCryptFinishHash(h,out,32,0)>=0;
    if(h) BCryptDestroyHash(h); if(a) BCryptCloseAlgorithmProvider(a,0); return ok;
}
static bool init_crypto(struct crypto *c,const uint8_t key[32])
{
    memset(c,0,sizeof(*c));
    if(BCryptOpenAlgorithmProvider(&c->alg,BCRYPT_AES_ALGORITHM,NULL,0)<0)return false;
    if(BCryptSetProperty(c->alg,BCRYPT_CHAINING_MODE,(PUCHAR)BCRYPT_CHAIN_MODE_GCM,sizeof(BCRYPT_CHAIN_MODE_GCM),0)<0)return false;
    return BCryptGenerateSymmetricKey(c->alg,&c->key,NULL,0,(PUCHAR)key,32,0)>=0;
}
static void close_crypto(struct crypto *c) { if(c->key)BCryptDestroyKey(c->key); if(c->alg)BCryptCloseAlgorithmProvider(c->alg,0); SecureZeroMemory(c,sizeof(*c)); }
static void put64(uint8_t *p,uint64_t n) { for(int i=7;i>=0;i--){p[i]=(uint8_t)n;n>>=8;} }
static uint64_t get64(const uint8_t *p) { uint64_t n=0;for(int i=0;i<8;i++)n=(n<<8)|p[i];return n; }
static bool crypt_record(struct crypto *c,bool encrypt,uint8_t prefix,uint64_t seq,const uint8_t *in,ULONG len,uint8_t *out,uint8_t *tag)
{
    uint8_t nonce[12]={ 'O','L','C',prefix }; put64(nonce+4,seq);
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth; BCRYPT_INIT_AUTH_MODE_INFO(auth);
    auth.pbNonce=nonce;auth.cbNonce=12;auth.pbTag=tag;auth.cbTag=16;
    ULONG done=0;
    NTSTATUS result=encrypt?BCryptEncrypt(c->key,(PUCHAR)in,len,&auth,NULL,0,out,len,&done,0):BCryptDecrypt(c->key,(PUCHAR)in,len,&auth,NULL,0,out,len,&done,0);
    return result>=0 && done==len;
}
static bool transfer(SOCKET s,void *data,int len,bool sending)
{
    char *p=data;
    while(len>0){int n=sending?send(s,p,len,0):recv(s,p,len,0);if(n<=0)return false;p+=n;len-=n;}return true;
}
static bool private_address(uint32_t net)
{
    uint32_t a=ntohl(net);return (a>>24)==127 || (a>>24)==10 || (a>>20)==0xac1 || (a>>16)==0xc0a8;
}
static void find_address(char out[64])
{
    strcpy(out,"127.0.0.1"); ULONG size=16384; IP_ADAPTER_ADDRESSES *buf=bmalloc(size);
    ULONG r=GetAdaptersAddresses(AF_INET,GAA_FLAG_INCLUDE_GATEWAYS,NULL,buf,&size);
    if(r==ERROR_BUFFER_OVERFLOW){bfree(buf);buf=bmalloc(size);r=GetAdaptersAddresses(AF_INET,GAA_FLAG_INCLUDE_GATEWAYS,NULL,buf,&size);}
    if(r==NO_ERROR) for(IP_ADAPTER_ADDRESSES *a=buf;a;a=a->Next){
        if(a->OperStatus!=IfOperStatusUp || (a->IfType!=IF_TYPE_ETHERNET_CSMACD && a->IfType!=IF_TYPE_IEEE80211))continue;
        bool gateway=false;for(IP_ADAPTER_GATEWAY_ADDRESS_LH *g=a->FirstGatewayAddress;g;g=g->Next)if(g->Address.lpSockaddr->sa_family==AF_INET && ((struct sockaddr_in*)g->Address.lpSockaddr)->sin_addr.s_addr)gateway=true;
        if(!gateway)continue;
        for(IP_ADAPTER_UNICAST_ADDRESS *u=a->FirstUnicastAddress;u;u=u->Next)if(u->Address.lpSockaddr->sa_family==AF_INET){
            struct sockaddr_in *v=(struct sockaddr_in*)u->Address.lpSockaddr;
            if(private_address(v->sin_addr.s_addr) && ntohl(v->sin_addr.s_addr)>>24!=127){inet_ntop(AF_INET,&v->sin_addr,out,64);goto done;}
        }
    }
done:bfree(buf);
}
static bool valid_token(const char *s)
{
    if(strlen(s)!=32)return false;for(int i=0;i<32;i++)if(!((s[i]>='0'&&s[i]<='9')||(s[i]>='a'&&s[i]<='f')))return false;return true;
}
static void new_token(char out[33])
{
    uint8_t bytes[16]; if(!random_bytes(bytes,16)){out[0]=0;return;}
    for(int i=0;i<16;i++)sprintf(out+2*i,"%02x",bytes[i]);SecureZeroMemory(bytes,16);
}
static void state(struct camera *c,const char *status)
{
    EnterCriticalSection(&c->lock);snprintf(c->status,sizeof(c->status),"%s",status);LeaveCriticalSection(&c->lock);
    obs_source_update_properties(c->source);
}
static void blank(struct camera *c)
{
    struct obs_source_frame *f=obs_source_frame_create(VIDEO_FORMAT_I420,1280,720);
    if(!f)return;
    memset(f->data[0],16,(size_t)f->linesize[0]*720);memset(f->data[1],128,(size_t)f->linesize[1]*360);memset(f->data[2],128,(size_t)f->linesize[2]*360);
    f->timestamp=os_gettime_ns();f->full_range=false;
    video_format_get_parameters(VIDEO_CS_709,VIDEO_RANGE_PARTIAL,f->color_matrix,f->color_range_min,f->color_range_max);
    obs_source_output_video(c->source,f);obs_source_frame_destroy(f);
}
static void rotate_plane(uint8_t *dst,const uint8_t *src,int stride,int w,int h,int rot)
{
    if(rot==0){for(int y=0;y<h;y++)memcpy(dst+y*w,src+y*stride,w);return;}
    int dw=(rot==90||rot==270)?h:w;
    for(int y=0;y<h;y++)for(int x=0;x<w;x++){
        int dx,dy;if(rot==90){dx=h-1-y;dy=x;}else if(rot==180){dx=w-1-x;dy=h-1-y;}else{dx=y;dy=w-1-x;}
        dst[dy*dw+dx]=src[y*stride+x];
    }
}
static bool output_frame(struct camera *c,AVFrame *av,int rotation,uint64_t timestamp)
{
    if(av->width<2||av->height<2||av->width>8192||av->height>8192||(uint64_t)av->width*av->height>33177600||(av->width&1)||(av->height&1))return false;
    if(av->format!=AV_PIX_FMT_YUV420P && av->format!=AV_PIX_FMT_YUVJ420P)return false;
    int w=av->width,h=av->height,dw=(rotation==90||rotation==270)?h:w,dh=(rotation==90||rotation==270)?w:h;
    size_t pixels=(size_t)dw*dh;uint8_t *buffer=bmalloc(pixels+pixels/2);
    struct obs_source_frame f={0};f.format=VIDEO_FORMAT_I420;f.width=dw;f.height=dh;f.timestamp=timestamp;
    f.data[0]=buffer;f.data[1]=buffer+pixels;f.data[2]=buffer+pixels+pixels/4;
    f.linesize[0]=dw;f.linesize[1]=f.linesize[2]=dw/2;
    for(int i=0;i<3;i++)rotate_plane(f.data[i],av->data[i],av->linesize[i],i?w/2:w,i?h/2:h,rotation);
    f.full_range=av->format==AV_PIX_FMT_YUVJ420P;
    video_format_get_parameters(VIDEO_CS_709,f.full_range?VIDEO_RANGE_FULL:VIDEO_RANGE_PARTIAL,f.color_matrix,f.color_range_min,f.color_range_max);
    obs_source_output_video(c->source,&f);bfree(buffer);
    EnterCriticalSection(&c->lock);c->width=dw;c->height=dh;c->frames++;LeaveCriticalSection(&c->lock);return true;
}
/* Video and audio share the handset's BOOTTIME clock and one session offset. */
static uint64_t media_time(uint64_t us,int64_t *offset,bool *ready)
{
    uint64_t now=os_gettime_ns();if(us>INT64_MAX/1000)return now;int64_t sender=(int64_t)us*1000;
    if(!*ready){*offset=(int64_t)now-sender;*ready=true;}
    int64_t mapped=sender+*offset;if(mapped<0||llabs(mapped-(int64_t)now)>5000000000LL){*offset=(int64_t)now-sender;mapped=(int64_t)now;}
    return (uint64_t)mapped;
}
static bool output_audio(struct camera *c,const uint8_t *body,size_t bytes,uint64_t timestamp)
{
    if(bytes<7)return false;uint32_t rate=((uint32_t)body[0]<<24)|((uint32_t)body[1]<<16)|((uint32_t)body[2]<<8)|body[3];unsigned channels=body[4];
    if((rate!=48000&&rate!=44100&&rate!=16000)||(channels!=1&&channels!=2)||(bytes-5)%(channels*2))return false;
    size_t frames=(bytes-5)/(channels*2);if(!frames||frames>rate/5)return false;
    obs_data_t *settings=obs_source_get_settings(c->source);
    if(!obs_data_get_bool(settings,"audio_routing_initialized")){
        if(!obs_source_get_audio_mixers(c->source))obs_source_set_audio_mixers(c->source,1);
        obs_data_set_bool(settings,"audio_routing_initialized",true);
    }obs_data_release(settings);
    struct obs_source_audio audio={0};audio.data[0]=body+5;audio.frames=(uint32_t)frames;audio.speakers=channels==1?SPEAKERS_MONO:SPEAKERS_STEREO;audio.format=AUDIO_FORMAT_16BIT;audio.samples_per_sec=rate;audio.timestamp=timestamp;obs_source_output_audio(c->source,&audio);
    EnterCriticalSection(&c->lock);c->audio_frames+=frames;c->last_audio_ns=os_gettime_ns();LeaveCriticalSection(&c->lock);return true;
}
static bool read_record(SOCKET s,struct crypto *crypto,uint64_t seq,uint8_t **plain,ULONG *len)
{
    uint32_t n;if(!transfer(s,&n,4,false))return false;n=ntohl(n);
    if(n<24||n>MAX_RECORD)return false;
    uint8_t *wire=bmalloc(n);bool ok=transfer(s,wire,n,false)&&get64(wire)==seq;
    *len=n-24;*plain=bmalloc(*len+AV_INPUT_BUFFER_PADDING_SIZE);memset(*plain+*len,0,AV_INPUT_BUFFER_PADDING_SIZE);
    if(ok)ok=crypt_record(crypto,false,1,seq,wire+8,*len,*plain,wire+n-16);
    bfree(wire);if(!ok){bfree(*plain);*plain=NULL;}return ok;
}
static DWORD WINAPI video_thread(void *data)
{
    struct session *session=data;struct camera *c=session->cam;SOCKET s=session->socket;bfree(session);
    struct crypto crypto={0};AVCodecContext *decoder=NULL;AVFrame *frame=NULL;AVPacket *packet=NULL;uint8_t *plain=NULL,*config=NULL;size_t config_len=0;
    uint8_t hello[20],server_nonce[16],material[48],key[32];int64_t clock_offset=0;bool clock_ready=false,media_started=false;
    memcpy(hello,MAGIC,4);
    if(!transfer(s,hello+4,16,false)||!random_bytes(server_nonce,16))goto done;
    for(int i=0;i<16;i++){unsigned x;sscanf(c->token+2*i,"%2x",&x);material[i]=(uint8_t)x;}
    memcpy(material+16,hello+4,16);memcpy(material+32,server_nonce,16);
    if(!hash_key(material,48,key)||!init_crypto(&crypto,key)||!transfer(s,server_nonce,16,true))goto done;
    SecureZeroMemory(material,sizeof(material));SecureZeroMemory(key,sizeof(key));
    ULONG length;if(!read_record(s,&crypto,0,&plain,&length))goto done;
    if(length!=6||memcmp(plain,"\0HELLO",6))goto done;bfree(plain);plain=NULL;
    uint8_t ack[18],tag[16];if(!crypt_record(&crypto,true,2,0,(const uint8_t*)"OK",2,ack,tag))goto done;
    memcpy(ack+2,tag,16);if(!transfer(s,ack,18,true))goto done;
    const AVCodec *codec=avcodec_find_decoder(AV_CODEC_ID_H264);if(!codec)goto done;
    decoder=avcodec_alloc_context3(codec);if(!decoder)goto done;
    av_opt_set_int(decoder,"threads",1,0);av_opt_set(decoder,"flags","+low_delay",0);
    if(avcodec_open2(decoder,codec,NULL)<0)goto done;
    frame=av_frame_alloc();packet=av_packet_alloc();if(!frame||!packet)goto done;
    InterlockedExchange(&c->connected,1);state(c,obs_module_text("Connected"));
    for(uint64_t seq=1;!c->stopping;seq++){
        if(!read_record(s,&crypto,seq,&plain,&length))break;
        if(length<11)break;
        if(plain[0]==3){if(length>2059)break;EnterCriticalSection(&c->lock);memcpy(c->last_error,plain+11,length-11);c->last_error[length-11]=0;LeaveCriticalSection(&c->lock);blog(LOG_WARNING,"[OBS LAN Camera] Android: %s",c->last_error);bfree(plain);plain=NULL;continue;}
        uint64_t pts=get64(plain+3);
        if(plain[0]==4){if(!output_audio(c,plain+11,length-11,media_time(pts,&clock_offset,&clock_ready)))break;bfree(plain);plain=NULL;if(!media_started){DWORD timeout=5000;setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));media_started=true;}continue;}
        if(plain[0]!=1&&plain[0]!=2)break;
        int rotation=(plain[1]<<8)|plain[2];if(rotation!=0&&rotation!=90&&rotation!=180&&rotation!=270)break;
        int bytes=(int)length-11;if(bytes<=0)break;
        if(plain[0]==1){if(config_len+(size_t)bytes>65536)break;config=brealloc(config,config_len+bytes);memcpy(config+config_len,plain+11,bytes);config_len+=bytes;bfree(plain);plain=NULL;continue;}
        if(av_new_packet(packet,bytes+(int)config_len)<0)break;
        if(config_len)memcpy(packet->data,config,config_len);memcpy(packet->data+config_len,plain+11,bytes);config_len=0;bfree(plain);plain=NULL;
        packet->pts=pts<=INT64_MAX?(int64_t)pts:AV_NOPTS_VALUE;packet->dts=AV_NOPTS_VALUE;
        int result=avcodec_send_packet(decoder,packet);av_packet_unref(packet);
        if(result<0 && result!=AVERROR(EAGAIN)){continue;}
        while(avcodec_receive_frame(decoder,frame)==0){uint64_t frame_pts=frame->pts==AV_NOPTS_VALUE?pts:(uint64_t)frame->pts;if(!output_frame(c,frame,rotation,media_time(frame_pts,&clock_offset,&clock_ready))){av_frame_unref(frame);goto done;}av_frame_unref(frame);if(!media_started){DWORD timeout=5000;setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));media_started=true;}}
    }
done:
    bfree(config);bfree(plain);av_packet_free(&packet);av_frame_free(&frame);avcodec_free_context(&decoder);close_crypto(&crypto);
    SecureZeroMemory(material,sizeof(material));SecureZeroMemory(key,sizeof(key));
    EnterCriticalSection(&c->lock);if(c->client==s)c->client=INVALID_SOCKET;LeaveCriticalSection(&c->lock);closesocket(s);
    InterlockedExchange(&c->connected,0);if(!c->stopping){blank(c);state(c,obs_module_text("Waiting"));}return 0;
}
static void http_send(SOCKET s,int code,const char *mime,const void *body,size_t len)
{
    char header[700];int n=snprintf(header,sizeof(header),"HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\nConnection: close\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nContent-Security-Policy: default-src 'self'; script-src 'self'; style-src 'self' 'unsafe-inline'; img-src 'self' data:; connect-src 'self'; frame-ancestors 'none'\r\n\r\n",code,code==200?"OK":"Forbidden",mime,len);
    if(transfer(s,header,n,true)&&len)transfer(s,(void*)body,(int)len,true);
}
static void http_file(SOCKET s,const char *name,const char *mime)
{
    char *path=obs_module_file(name);if(!path){http_send(s,404,"text/plain","Not found",9);return;}
    FILE *file=os_fopen(path,"rb");bfree(path);if(!file){http_send(s,404,"text/plain","Not found",9);return;}
    fseek(file,0,SEEK_END);long len=ftell(file);rewind(file);
    if(len<=0||len>16*1024*1024){fclose(file);http_send(s,404,"text/plain","Not found",9);return;}
    uint8_t *bytes=bmalloc((size_t)len);size_t got=fread(bytes,1,(size_t)len,file);fclose(file);
    http_send(s,got==(size_t)len?200:500,mime,bytes,got);bfree(bytes);
}
static void http_request(struct camera *c,SOCKET s,bool local,const char first[4])
{
    char req[8193]={0};int used=4;memcpy(req,first,4);
    while(used<8192){int n=recv(s,req+used,8192-used,0);if(n<=0)return;used+=n;req[used]=0;if(strstr(req,"\r\n\r\n"))break;}
    if(!strstr(req,"\r\n\r\n"))return;
    char method[16],path[300],host[128]={0};if(sscanf(req,"%15s %299s",method,path)!=2||strcmp(method,"GET"))return;
    char *h=strstr(req,"\r\nHost: ");if(!h)h=strstr(req,"\r\nhost: ");if(h)sscanf(h+8,"%127[^\r\n]",host);
    char allowed[128],loop[128],localhost[128];snprintf(allowed,sizeof(allowed),"%s:%d",c->address,c->port);snprintf(loop,sizeof(loop),"127.0.0.1:%d",c->port);snprintf(localhost,sizeof(localhost),"localhost:%d",c->port);
    if(strcmp(host,allowed)&&strcmp(host,loop)&&strcmp(host,localhost)){http_send(s,403,"text/plain","Invalid Host",12);return;}
    if(!strcmp(path,"/api/pair")){
        if(!local){http_send(s,403,"text/plain","Open this page on the OBS computer",34);return;}
        char json[1100];snprintf(json,sizeof(json),"{\"address\":\"%s\",\"port\":%d,\"token\":\"%s\",\"connected\":%s,\"apk_url\":\"" OLC_APK_URL "\",\"version\":\"" OLC_VERSION "\"}",c->address,c->port,c->token,c->connected?"true":"false");http_send(s,200,"application/json; charset=utf-8",json,strlen(json));
    }else if(!strcmp(path,"/api/status")){
        if(!local){http_send(s,403,"text/plain","Local only",10);return;}
        obs_data_t *status=obs_data_create();obs_data_set_string(status,"version",OLC_VERSION);obs_data_set_bool(status,"connected",c->connected!=0);
        EnterCriticalSection(&c->lock);obs_data_set_int(status,"frames",c->frames);obs_data_set_int(status,"width",c->width);obs_data_set_int(status,"height",c->height);obs_data_set_int(status,"audio_frames",c->audio_frames);obs_data_set_bool(status,"audio_active",c->connected&&c->last_audio_ns&&os_gettime_ns()-c->last_audio_ns<200000000ULL);obs_data_set_string(status,"last_error",c->last_error);LeaveCriticalSection(&c->lock);
        const char *json=obs_data_get_json(status);http_send(s,200,"application/json; charset=utf-8",json,strlen(json));obs_data_release(status);
    }else if(!strcmp(path,"/")||!strcmp(path,"/pair"))http_file(s,local?"pair.html":"download.html","text/html; charset=utf-8");
    else if(!strcmp(path,"/pair.js") && local)http_file(s,"pair.js","text/javascript; charset=utf-8");
    else if(!strcmp(path,"/qrcode.js") && local)http_file(s,"qrcode.js","text/javascript; charset=utf-8");
    else if(!strcmp(path,"/app.apk"))http_file(s,"app.apk","application/vnd.android.package-archive");
    else http_send(s,404,"text/plain","Not found",9);
}
static DWORD WINAPI server_thread(void *data)
{
    struct camera *c=data;
    while(!c->stopping){
        struct sockaddr_in peer;int size=sizeof(peer);SOCKET s=accept(c->listener,(struct sockaddr*)&peer,&size);
        if(s==INVALID_SOCKET)break;if(!private_address(peer.sin_addr.s_addr)){closesocket(s);continue;}
        DWORD timeout=2000;setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));setsockopt(s,SOL_SOCKET,SO_SNDTIMEO,(char*)&timeout,sizeof(timeout));
        char peek[4];if(!transfer(s,peek,4,false)){closesocket(s);continue;}
        if(!memcmp(peek,MAGIC,4)){
            if(c->video_thread && WaitForSingleObject(c->video_thread,0)==WAIT_OBJECT_0){CloseHandle(c->video_thread);c->video_thread=NULL;}
            if(c->video_thread){closesocket(s);continue;}
            timeout=20000;setsockopt(s,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
            EnterCriticalSection(&c->lock);c->client=s;LeaveCriticalSection(&c->lock);
            struct session *session=bmalloc(sizeof(*session));session->cam=c;session->socket=s;
            c->video_thread=CreateThread(NULL,0,video_thread,session,0,NULL);
            if(!c->video_thread){bfree(session);closesocket(s);c->client=INVALID_SOCKET;}
        }else{http_request(c,s,(ntohl(peer.sin_addr.s_addr)>>24)==127,peek);closesocket(s);}
    }return 0;
}
static void stop_server(struct camera *c)
{
    InterlockedExchange(&c->stopping,1);
    if(c->listener!=INVALID_SOCKET){closesocket(c->listener);c->listener=INVALID_SOCKET;}
    EnterCriticalSection(&c->lock);if(c->client!=INVALID_SOCKET)shutdown(c->client,SD_BOTH);LeaveCriticalSection(&c->lock);
    if(c->listener_thread){WaitForSingleObject(c->listener_thread,INFINITE);CloseHandle(c->listener_thread);c->listener_thread=NULL;}
    /* Accept may have handed off a client just before stopping. */
    EnterCriticalSection(&c->lock);if(c->client!=INVALID_SOCKET)shutdown(c->client,SD_BOTH);LeaveCriticalSection(&c->lock);
    if(c->video_thread){WaitForSingleObject(c->video_thread,INFINITE);CloseHandle(c->video_thread);c->video_thread=NULL;}
    InterlockedExchange(&c->connected,0);
}
static void start_server(struct camera *c)
{
    if(!c->enabled){state(c,obs_module_text("Disabled"));return;}
    if(!valid_token(c->token)){state(c,obs_module_text("InvalidToken"));return;}
    c->listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(c->listener==INVALID_SOCKET){state(c,obs_module_text("BindError"));return;}
    BOOL exclusive=TRUE;setsockopt(c->listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(char*)&exclusive,sizeof(exclusive));
    struct sockaddr_in address={0};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_ANY);address.sin_port=htons((u_short)c->port);
    if(bind(c->listener,(struct sockaddr*)&address,sizeof(address))||listen(c->listener,8)){
        closesocket(c->listener);c->listener=INVALID_SOCKET;state(c,obs_module_text("BindError"));return;
    }
    InterlockedExchange(&c->stopping,0);c->listener_thread=CreateThread(NULL,0,server_thread,c,0,NULL);
    if(!c->listener_thread){closesocket(c->listener);c->listener=INVALID_SOCKET;state(c,obs_module_text("BindError"));return;}
    state(c,obs_module_text("Waiting"));
}
static void update(void *data,obs_data_t *settings)
{
    struct camera *c=data;int port=(int)obs_data_get_int(settings,"port");if(port<1024||port>65535)port=34871;
    const char *token=obs_data_get_string(settings,"token");bool enabled=obs_data_get_bool(settings,"listen");
    if(c->port==port && !strcmp(c->token,token) && c->enabled==enabled)return;
    stop_server(c);c->port=port;c->enabled=enabled;snprintf(c->token,sizeof(c->token),"%s",token);find_address(c->address);start_server(c);
}
static void *create(obs_data_t *settings,obs_source_t *source)
{
    struct camera *c=bzalloc(sizeof(*c));c->source=source;c->listener=c->client=INVALID_SOCKET;InitializeCriticalSection(&c->lock);
    if(!*obs_data_get_string(settings,"token")){char token[33];new_token(token);obs_data_set_string(settings,"token",token);}
    obs_source_set_async_unbuffered(source,true);update(c,settings);blank(c);return c;
}
static void destroy(void *data)
{
    struct camera *c=data;stop_server(c);DeleteCriticalSection(&c->lock);SecureZeroMemory(c->token,sizeof(c->token));bfree(c);
}
static const char *name(void *unused) { UNUSED_PARAMETER(unused);return obs_module_text("SourceName"); }
static void defaults(obs_data_t *s) { obs_data_set_default_int(s,"port",34871);obs_data_set_default_bool(s,"listen",true); }
static bool is_connected(void *data){return ((struct camera*)data)->connected!=0;}
static void show_pairing(struct camera *c){olc_pairing_show(c->source,c->address,c->port,c->token,is_connected,c);}
static void video_tick(void *data,float seconds){UNUSED_PARAMETER(seconds);struct camera *c=data;if(!c->pairing_shown&&olc_pairing_available()&&InterlockedCompareExchange(&c->pairing_shown,1,0)==0)show_pairing(c);}
static bool open_pair(obs_properties_t *p,obs_property_t *property,void *data)
{
    UNUSED_PARAMETER(p);UNUSED_PARAMETER(property);show_pairing(data);return false;
}
static bool renew(obs_properties_t *p,obs_property_t *property,void *data)
{
    UNUSED_PARAMETER(p);UNUSED_PARAMETER(property);struct camera *c=data;char token[33];new_token(token);
    obs_data_t *settings=obs_source_get_settings(c->source);obs_data_set_string(settings,"token",token);obs_source_update(c->source,settings);obs_data_release(settings);return true;
}
static bool check_updates(obs_properties_t *p,obs_property_t *property,void *data)
{
    UNUSED_PARAMETER(p);UNUSED_PARAMETER(property);UNUSED_PARAMETER(data);olc_updater_check(false);return false;
}
static obs_properties_t *properties(void *data)
{
    struct camera *c=data;obs_properties_t *p=obs_properties_create();
    char text[700];if(c){EnterCriticalSection(&c->lock);snprintf(text,sizeof(text),"%s\n%s: %s:%d",c->status,obs_module_text("Address"),c->address,c->port);LeaveCriticalSection(&c->lock);}else snprintf(text,sizeof(text),"%s",obs_module_text("Waiting"));
    obs_properties_add_text(p,"state",text,OBS_TEXT_INFO);
    obs_properties_add_bool(p,"listen",obs_module_text("Listen"));
    obs_properties_add_button2(p,"pair",obs_module_text("Pair"),open_pair,c);
    obs_properties_add_button2(p,"updates",obs_module_text("CheckUpdates"),check_updates,NULL);
    obs_properties_add_int(p,"port",obs_module_text("Port"),1024,65535,1);
    obs_property_t *token=obs_properties_add_text(p,"token",obs_module_text("Token"),OBS_TEXT_PASSWORD);obs_property_set_long_description(token,obs_module_text("TokenHint"));
    obs_properties_add_button2(p,"renew",obs_module_text("Renew"),renew,c);
    obs_properties_add_text(p,"hint",obs_module_text("Hint"),OBS_TEXT_INFO);return p;
}
static struct obs_source_info source_info={.id="obs_lan_camera",.type=OBS_SOURCE_TYPE_INPUT,.output_flags=OBS_SOURCE_ASYNC_VIDEO|OBS_SOURCE_AUDIO,
    .get_name=name,.create=create,.destroy=destroy,.get_defaults=defaults,.get_properties=properties,.update=update,.video_tick=video_tick};
bool obs_module_load(void)
{
    WSADATA data;if(WSAStartup(MAKEWORD(2,2),&data))return false;olc_updater_init();olc_pairing_init();obs_register_source(&source_info);blog(LOG_INFO,"[OBS LAN Camera] " OLC_VERSION " loaded (H.264, PCM audio, AES-256-GCM, native pairing)");return true;
}
MODULE_EXPORT void obs_module_post_load(void) { olc_pairing_frontend_ready();olc_updater_check(true); }
void obs_module_unload(void) { olc_pairing_shutdown();olc_updater_shutdown();WSACleanup(); }
