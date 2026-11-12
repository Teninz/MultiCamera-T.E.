/* Small C boundary: native transport stays independent of the UI runtime. */
#include "app.h"
#include "usb.h"
#include "../../plugin/src/qrcodegen.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define API __declspec(dllexport)
static struct mc_app *app;
static struct mc_usb_adapter usb[32];static int usb_count;
API int mc_open(void){
 if(app)return 0;WSADATA wsa;if(WSAStartup(MAKEWORD(2,2),&wsa))return 1;
 if(FAILED(MFStartup(MF_VERSION,MFSTARTUP_FULL))){WSACleanup();return 2;}
 app=calloc(1,sizeof(*app));if(!app){MFShutdown();WSACleanup();return 2;}
 if(!mc_config_load(&app->config)){free(app);app=NULL;MFShutdown();WSACleanup();return 3;}
 if(!mc_hub_init(&app->hub)){free(app);app=NULL;MFShutdown();WSACleanup();return 4;}
 for(int i=0;i<5;i++){memcpy(app->hub.slot[i].token,app->config.tokens[i],33);memcpy(app->hub.slot[i].name,app->config.names[i],96);}
 if(!mc_ipc_start(&app->hub)||!mc_hub_start(&app->hub,MC_PORT)){mc_ipc_stop(&app->hub);mc_hub_stop(&app->hub);mc_hub_free(&app->hub);SecureZeroMemory(app,sizeof(*app));free(app);app=NULL;MFShutdown();WSACleanup();return 5;}
 mc_cameras_start(app);if(app->config.internet)mc_relay_start(app);return 0;
}
API void mc_close(void){if(!app)return;InterlockedExchange(&app->closing,1);mc_relay_stop(app);mc_cameras_stop(app);mc_ipc_stop(&app->hub);mc_hub_stop(&app->hub);mc_hub_free(&app->hub);SecureZeroMemory(app,sizeof(*app));free(app);app=NULL;MFShutdown();WSACleanup();}
/* field: camera status, relay status, language, motion, internet; slot fields 10+. */
API int mc_value(int field,int slot){if(!app)return -1;switch(field){case 0:return app->camera_status;case 1:return mc_relay_status(app);case 2:return app->config.language;case 3:return app->config.animation;case 4:return app->config.internet;case 5:return usb_count;}if(slot<0||slot>=5)return -1;struct mc_slot *s=&app->hub.slot[slot];int result=0;EnterCriticalSection(&s->lock);switch(field){case 10:result=s->connected;break;case 11:result=s->input_width;break;case 12:result=s->input_height;break;case 13:result=s->last_audio_ms&&GetTickCount64()-s->last_audio_ms<500;break;}LeaveCriticalSection(&s->lock);return result;}
API void mc_text_value(int field,int index,wchar_t *out,int capacity){if(!out||capacity<1)return;out[0]=0;if(!app)return;switch(field){case 0:MultiByteToWideChar(CP_UTF8,0,app->hub.address,-1,out,capacity);break;case 1:wcsncpy(out,app->config.relay,capacity-1);break;case 2:wcsncpy(out,app->config.host_key,capacity-1);break;case 3:if(index>=0&&index<usb_count)swprintf(out,capacity,L"%hs · %ls",usb[index].address,usb[index].name);break;case 4:if(index>=0&&index<usb_count)swprintf(out,capacity,L"%lu %hs",usb[index].index,usb[index].address);break;}out[capacity-1]=0;}
API int mc_refresh_usb(void){usb_count=mc_usb_adapters(usb,32);return usb_count;}
API int mc_preferences(int language,int animation){if(!app||language<0||language>4)return 0;struct mc_config next=app->config;next.language=language;next.animation=animation!=0;if(!mc_config_save(&next))return 0;app->config=next;return 1;}
API int mc_settings(const wchar_t *server,const wchar_t *key,int enabled){
 if(!app||!server||!key||wcslen(server)>511||wcslen(key)>255)return 0;
 if((enabled&&!*server)||(*server&&wcsncmp(server,L"wss://",6))||wcspbrk(server,L" #?@\r\n")||wcspbrk(key,L"\r\n"))return 0;
 struct mc_config next=app->config;wcscpy(next.relay,server);wcscpy(next.host_key,key);next.internet=enabled!=0;if(!mc_config_save(&next))return 0;mc_relay_stop(app);app->config=next;if(enabled)mc_relay_start(app);SecureZeroMemory(&next,sizeof(next));return 1;
}
API int mc_pair_uri(int slot,int mode,int adapter,char *uri,int capacity){
 if(!app||!uri||capacity<8192||slot<0||slot>=5)return 0;uri[0]=0;
 if(mode==0)snprintf(uri,capacity,"obslan://%s:%d#%s",app->hub.address,MC_PORT,app->hub.slot[slot].token);
 else if(mode==2){if(adapter<0||adapter>=usb_count)return 0;snprintf(uri,capacity,"obslan://%s:%d?transport=usb#%s",usb[adapter].address,MC_PORT,app->hub.slot[slot].token);}
 else if(mode==1){if(!app->config.internet||!*app->config.relay)return 0;char server[2048],target[2304],escaped[6912];WideCharToMultiByte(CP_UTF8,0,app->config.relay,-1,server,sizeof(server),NULL,NULL);size_t n=strlen(server);while(n&&server[n-1]=='/')server[--n]=0;snprintf(target,sizeof(target),"%s/v1/phone/%s/%d",server,app->config.room,slot+1);size_t at=0;for(const unsigned char *p=(const unsigned char*)target;*p&&at+4<sizeof(escaped);p++){if((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||strchr("-._~",*p))escaped[at++]=*p;else{snprintf(escaped+at,4,"%%%02X",*p);at+=3;}}escaped[at]=0;snprintf(uri,capacity,"multicamera://connect?relay=%s#%s",escaped,app->hub.slot[slot].token);}
 return *uri!=0;
}
API int mc_qr(const char *text,unsigned char *modules,int capacity){uint8_t scratch[qrcodegen_BUFFER_LEN_MAX],qr[qrcodegen_BUFFER_LEN_MAX];if(!text||!modules||!qrcodegen_encodeText(text,scratch,qr,qrcodegen_Ecc_MEDIUM,1,qrcodegen_VERSION_MAX,qrcodegen_Mask_AUTO,true))return 0;int n=qrcodegen_getSize(qr);if(capacity<n*n)return 0;for(int y=0;y<n;y++)for(int x=0;x<n;x++)modules[y*n+x]=qrcodegen_getModule(qr,x,y)?1:0;return n;}
static int clamp(int n){return n<0?0:n>255?255:n;}
API int mc_thumbnail(int index,unsigned long long *sequence,unsigned char *pixels,int capacity){
 if(!app||index<0||index>=5||!sequence||!pixels||capacity<320*180*4)return 0;struct mc_slot *s=&app->hub.slot[index];EnterCriticalSection(&s->lock);if(!s->connected||s->sequence==*sequence){LeaveCriticalSection(&s->lock);return 0;}
 for(int y=0;y<180;y++)for(int x=0;x<320;x++){int sx=x*4,sy=y*4,yy=s->pixels[sy*MC_WIDTH+sx]-16,uv=MC_WIDTH*MC_HEIGHT+(sy/2)*MC_WIDTH+(sx&~1),u=s->pixels[uv]-128,v=s->pixels[uv+1]-128;size_t at=(y*320+x)*4;pixels[at]=clamp((298*yy+541*u+128)>>8);pixels[at+1]=clamp((298*yy-55*u-136*v+128)>>8);pixels[at+2]=clamp((298*yy+459*v+128)>>8);pixels[at+3]=255;}*sequence=s->sequence;LeaveCriticalSection(&s->lock);return 1;
}
