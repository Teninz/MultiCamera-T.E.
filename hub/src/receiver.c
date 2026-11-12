/* OLC1 receiver shared with the existing Android sender. GPL-2.0-or-later. */
#include "hub.h"
#include "usb.h"
#include <ws2tcpip.h>
#include <bcrypt.h>
#include <iphlpapi.h>
#include <mfapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define bmalloc malloc
#define bfree free
#define MAX_RECORD (4*1024*1024)
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
        if(mc_usb_description(a->Description)||a->OperStatus!=IfOperStatusUp || (a->IfType!=IF_TYPE_ETHERNET_CSMACD && a->IfType!=IF_TYPE_IEEE80211))continue;
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
static bool read_record(SOCKET socket,struct crypto *crypto,uint64_t sequence,uint8_t **plain,ULONG *length){
 uint32_t size;if(!transfer(socket,&size,4,false))return false;size=ntohl(size);if(size<24||size>MAX_RECORD)return false;uint8_t *wire=malloc(size);if(!wire)return false;bool ok=transfer(socket,wire,size,false)&&get64(wire)==sequence;*length=size-24;*plain=malloc(*length?*length:1);if(!*plain){free(wire);return false;}if(ok)ok=crypt_record(crypto,false,1,sequence,wire+8,*length,*plain,wire+size-16);free(wire);if(!ok){free(*plain);*plain=NULL;}return ok;
}
struct client_context {struct mc_hub *hub;SOCKET socket;int connection;};
static DWORD WINAPI camera_client(void *data){
 struct client_context *client=data;struct mc_hub *hub=client->hub;SOCKET socket=client->socket;int connection=client->connection;free(client);struct mc_slot *slot=NULL;struct mc_decoder *decoder=NULL;struct crypto crypto={0};uint8_t *plain=NULL,*config=NULL;size_t config_size=0;bool initialized=SUCCEEDED(CoInitializeEx(NULL,COINIT_MULTITHREADED));
 uint8_t hello[20],nonce[16],material[48]={0},key[32]={0};if(!transfer(socket,hello,20,false)||memcmp(hello,"OLC1",4)||!random_bytes(nonce,16)||!transfer(socket,nonce,16,true))goto done;
 uint32_t wire_size;if(!transfer(socket,&wire_size,4,false))goto done;wire_size=ntohl(wire_size);if(wire_size!=30)goto done;uint8_t wire[30];if(!transfer(socket,wire,sizeof(wire),false)||get64(wire)!=0)goto done;
 memcpy(material+16,hello+4,16);memcpy(material+32,nonce,16);
 for(int index=0;index<MC_SLOTS;index++){for(int j=0;j<16;j++){unsigned value;sscanf(hub->slot[index].token+2*j,"%2x",&value);material[j]=(uint8_t)value;}if(!hash_key(material,48,key)||!init_crypto(&crypto,key))goto done;uint8_t text[6];bool authenticated=crypt_record(&crypto,false,1,0,wire+8,6,text,wire+14)&&!memcmp(text,"\0HELLO",6);if(authenticated){if(InterlockedCompareExchange(&hub->slot[index].connected,1,0)!=0)goto done;slot=&hub->slot[index];break;}close_crypto(&crypto);}
 SecureZeroMemory(material,sizeof(material));SecureZeroMemory(key,sizeof(key));if(!slot)goto done;
 uint8_t ack[18],tag[16];if(!crypt_record(&crypto,true,2,0,(const uint8_t*)"OK",2,ack,tag))goto done;memcpy(ack+2,tag,16);if(!transfer(socket,ack,sizeof(ack),true))goto done;
 DWORD timeout=20000;setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));decoder=mc_decoder_create();if(!decoder){snprintf(slot->error,sizeof(slot->error),"Windows H.264 decoder unavailable");goto done;}
 EnterCriticalSection(&slot->lock);slot->error[0]=0;LeaveCriticalSection(&slot->lock);
 for(uint64_t sequence=1;!hub->stopping;sequence++){
  ULONG length=0;if(!read_record(socket,&crypto,sequence,&plain,&length))break;if(length<11)break;unsigned type=plain[0],rotation=(plain[1]<<8)|plain[2];uint64_t pts=get64(plain+3);size_t bytes=length-11;
  if(type==3){if(bytes>511)break;EnterCriticalSection(&slot->lock);memcpy(slot->error,plain+11,bytes);slot->error[bytes]=0;LeaveCriticalSection(&slot->lock);}
  else if(type==4){if(bytes<7)break;uint8_t *body=plain+11;unsigned rate=((unsigned)body[0]<<24)|((unsigned)body[1]<<16)|((unsigned)body[2]<<8)|body[3];if(body[4]!=1||(rate!=48000&&rate!=44100&&rate!=16000)||(bytes-5)%2||bytes-5>19200||bytes-5>rate*2/5)break;
   EnterCriticalSection(&slot->lock);uint64_t audio_sequence=++slot->audio_sequence;struct mc_audio *audio=&slot->audio[audio_sequence%16];audio->sequence=audio_sequence;audio->timestamp_us=pts;audio->rate=rate;audio->frames=(uint32_t)((bytes-5)/2);audio->bytes=(uint32_t)(bytes-5);memcpy(audio->pcm,body+5,bytes-5);slot->audio_frames+=audio->frames;slot->last_audio_ms=GetTickCount64();LeaveCriticalSection(&slot->lock);
  }else if(type==1){if(!bytes||config_size+bytes>65536)break;uint8_t *larger=realloc(config,config_size+bytes);if(!larger)break;config=larger;memcpy(config+config_size,plain+11,bytes);config_size+=bytes;}
  else if(type==2){if((rotation!=0&&rotation!=90&&rotation!=180&&rotation!=270)||!bytes)break;uint8_t *packet=malloc(config_size+bytes);if(!packet)break;if(config_size)memcpy(packet,config,config_size);memcpy(packet+config_size,plain+11,bytes);bool ok=mc_decoder_packet(decoder,packet,config_size+bytes,pts,rotation,slot);free(packet);config_size=0;if(!ok){EnterCriticalSection(&slot->lock);snprintf(slot->error,sizeof(slot->error),"H.264 decode failed");LeaveCriticalSection(&slot->lock);break;}
   timeout=5000;setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));
  }else break;free(plain);plain=NULL;
 }
done:
 free(plain);free(config);mc_decoder_destroy(decoder);close_crypto(&crypto);SecureZeroMemory(material,sizeof(material));SecureZeroMemory(key,sizeof(key));
 if(slot){EnterCriticalSection(&slot->lock);mc_black(slot->pixels);memset(slot->audio,0,sizeof(slot->audio));slot->sequence++;LeaveCriticalSection(&slot->lock);InterlockedExchange(&slot->connected,0);SetEvent(slot->changed);}
 EnterCriticalSection(&hub->connections_lock);hub->connections[connection]=INVALID_SOCKET;closesocket(socket);if(InterlockedDecrement(&hub->clients)==0)SetEvent(hub->clients_done);LeaveCriticalSection(&hub->connections_lock);if(initialized)CoUninitialize();return 0;
}
static DWORD WINAPI accept_clients(void *data){struct mc_hub *hub=data;while(!hub->stopping){struct sockaddr_in address;int length=sizeof(address);SOCKET socket=accept(hub->listener,(struct sockaddr*)&address,&length);if(socket==INVALID_SOCKET)break;if(!private_address(address.sin_addr.s_addr)){closesocket(socket);continue;}
 DWORD timeout=5000;setsockopt(socket,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout));setsockopt(socket,SOL_SOCKET,SO_SNDTIMEO,(char*)&timeout,sizeof(timeout));BOOL nodelay=TRUE;setsockopt(socket,IPPROTO_TCP,TCP_NODELAY,(char*)&nodelay,sizeof(nodelay));
 EnterCriticalSection(&hub->connections_lock);int index=-1;if(!hub->stopping)for(int i=0;i<10;i++)if(hub->connections[i]==INVALID_SOCKET){index=i;break;}if(index<0){LeaveCriticalSection(&hub->connections_lock);closesocket(socket);continue;}
 struct client_context *client=malloc(sizeof(*client));if(!client){LeaveCriticalSection(&hub->connections_lock);closesocket(socket);continue;}client->hub=hub;client->socket=socket;client->connection=index;hub->connections[index]=socket;InterlockedIncrement(&hub->clients);ResetEvent(hub->clients_done);HANDLE thread=CreateThread(NULL,0,camera_client,client,0,NULL);if(thread)CloseHandle(thread);else{hub->connections[index]=INVALID_SOCKET;if(InterlockedDecrement(&hub->clients)==0)SetEvent(hub->clients_done);free(client);closesocket(socket);}LeaveCriticalSection(&hub->connections_lock);
 }return 0;}
bool mc_hub_init(struct mc_hub *hub){memset(hub,0,sizeof(*hub));hub->listener=INVALID_SOCKET;InitializeCriticalSection(&hub->connections_lock);for(int i=0;i<10;i++)hub->connections[i]=INVALID_SOCKET;hub->clients_done=CreateEventW(NULL,TRUE,TRUE,NULL);find_address(hub->address);for(int i=0;i<MC_SLOTS;i++){struct mc_slot *slot=&hub->slot[i];InitializeCriticalSection(&slot->lock);slot->changed=CreateEventW(NULL,TRUE,FALSE,NULL);slot->pixels=malloc(MC_BYTES);if(!slot->pixels)return false;mc_black(slot->pixels);new_token(slot->token);snprintf(slot->name,sizeof(slot->name),"Smartphone %d",i+1);}return true;}
#ifndef MC_BIND_ADDRESS
#define MC_BIND_ADDRESS INADDR_ANY
#endif
bool mc_hub_start(struct mc_hub *hub,int port){for(int i=0;i<MC_SLOTS;i++)if(!valid_token(hub->slot[i].token))return false;hub->listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);if(hub->listener==INVALID_SOCKET)return false;BOOL exclusive=TRUE;setsockopt(hub->listener,SOL_SOCKET,SO_EXCLUSIVEADDRUSE,(char*)&exclusive,sizeof(exclusive));struct sockaddr_in address={0};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(MC_BIND_ADDRESS);address.sin_port=htons((u_short)port);if(bind(hub->listener,(struct sockaddr*)&address,sizeof(address))||listen(hub->listener,10)){closesocket(hub->listener);hub->listener=INVALID_SOCKET;return false;}hub->network_thread=CreateThread(NULL,0,accept_clients,hub,0,NULL);return hub->network_thread!=NULL;}
void mc_hub_stop(struct mc_hub *hub){InterlockedExchange(&hub->stopping,1);if(hub->listener!=INVALID_SOCKET){closesocket(hub->listener);hub->listener=INVALID_SOCKET;}EnterCriticalSection(&hub->connections_lock);for(int i=0;i<10;i++)if(hub->connections[i]!=INVALID_SOCKET)shutdown(hub->connections[i],SD_BOTH);LeaveCriticalSection(&hub->connections_lock);if(hub->network_thread){WaitForSingleObject(hub->network_thread,INFINITE);CloseHandle(hub->network_thread);hub->network_thread=NULL;}WaitForSingleObject(hub->clients_done,INFINITE);}
void mc_hub_free(struct mc_hub *hub){for(int i=0;i<MC_SLOTS;i++){if(hub->slot[i].pixels)free(hub->slot[i].pixels);if(hub->slot[i].changed)CloseHandle(hub->slot[i].changed);DeleteCriticalSection(&hub->slot[i].lock);}CloseHandle(hub->clients_done);DeleteCriticalSection(&hub->connections_lock);SecureZeroMemory(hub,sizeof(*hub));}
