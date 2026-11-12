/* Independent OBS sources consuming MultiCamera T.E. frames. GPL-2.0-or-later. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <sddl.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <obs-module.h>
#include <util/platform.h>
#include "../../hub/src/protocol.h"
struct hub_camera {obs_source_t *source;CRITICAL_SECTION lock;HANDLE thread,pipe;volatile LONG stopping,connected;int slot;int64_t offset;bool clock_ready;};
static uint64_t timestamp(struct hub_camera *c,uint64_t us){uint64_t now=os_gettime_ns();if(us>INT64_MAX/1000)return now;int64_t remote=us*1000;if(!c->clock_ready){c->offset=(int64_t)now-remote;c->clock_ready=true;}int64_t mapped=remote+c->offset;if(mapped<0||llabs(mapped-(int64_t)now)>5000000000LL){c->offset=(int64_t)now-remote;mapped=now;}return mapped;}
static void output_black(struct hub_camera *c){uint8_t *pixels=bmalloc(1280*720*3/2);memset(pixels,16,1280*720);memset(pixels+1280*720,128,1280*720/2);struct obs_source_frame frame={0};frame.format=VIDEO_FORMAT_NV12;frame.width=1280;frame.height=720;frame.data[0]=pixels;frame.data[1]=pixels+1280*720;frame.linesize[0]=frame.linesize[1]=1280;frame.timestamp=os_gettime_ns();video_format_get_parameters(VIDEO_CS_709,VIDEO_RANGE_PARTIAL,frame.color_matrix,frame.color_range_min,frame.color_range_max);obs_source_output_video(c->source,&frame);bfree(pixels);}
static DWORD WINAPI reader(void *data){struct hub_camera *c=data;wchar_t base[192],name[256];if(!mc_user_pipe(base,192))return 0;swprintf(name,256,L"%ls.%d",base,c->slot+1);uint8_t *pixels=bmalloc(1280*720*3/2),pcm[19200];uint64_t video=UINT64_MAX,audio=0;
 while(!c->stopping){HANDLE pipe=CreateFileW(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);if(pipe==INVALID_HANDLE_VALUE){for(int i=0;i<10&&!c->stopping;i++)Sleep(100);continue;}EnterCriticalSection(&c->lock);c->pipe=pipe;LeaveCriticalSection(&c->lock);c->clock_ready=false;video=UINT64_MAX;audio=0;
  while(!c->stopping){struct mc_query query={MC_PIPE_QUERY,MC_PIPE_AUDIO|MC_PIPE_WAIT,video,audio};struct mc_reply reply={0};if(!mc_pipe_io(pipe,&query,sizeof(query),true)||!mc_pipe_io(pipe,&reply,sizeof(reply),false)||reply.magic!=MC_PIPE_REPLY||reply.width!=1280||reply.height!=720||reply.bytes!=1280*720*3/2||reply.audio_count>16||!mc_pipe_io(pipe,pixels,reply.bytes,false))break;
   InterlockedExchange(&c->connected,reply.connected!=0);
   if(reply.sequence!=video){struct obs_source_frame frame={0};frame.format=VIDEO_FORMAT_NV12;frame.width=1280;frame.height=720;frame.data[0]=pixels;frame.data[1]=pixels+1280*720;frame.linesize[0]=frame.linesize[1]=1280;frame.timestamp=timestamp(c,reply.timestamp_us);video_format_get_parameters(VIDEO_CS_709,VIDEO_RANGE_PARTIAL,frame.color_matrix,frame.color_range_min,frame.color_range_max);obs_source_output_video(c->source,&frame);video=reply.sequence;}
   bool ok=true;for(unsigned i=0;i<reply.audio_count;i++){struct mc_audio_header header;if(!mc_pipe_io(pipe,&header,sizeof(header),false)||header.bytes>sizeof(pcm)||header.frames*2!=header.bytes||(header.rate!=48000&&header.rate!=44100&&header.rate!=16000)||!mc_pipe_io(pipe,pcm,header.bytes,false)){ok=false;break;}audio=header.sequence;struct obs_source_audio sound={0};sound.data[0]=pcm;sound.frames=header.frames;sound.samples_per_sec=header.rate;sound.speakers=SPEAKERS_MONO;sound.format=AUDIO_FORMAT_16BIT;sound.timestamp=timestamp(c,header.timestamp_us);obs_source_output_audio(c->source,&sound);}if(!ok)break;
  }
  EnterCriticalSection(&c->lock);c->pipe=INVALID_HANDLE_VALUE;CloseHandle(pipe);LeaveCriticalSection(&c->lock);if(InterlockedExchange(&c->connected,0)&&!c->stopping)output_black(c);
 }bfree(pixels);return 0;
}
static void stop(struct hub_camera *c){InterlockedExchange(&c->stopping,1);EnterCriticalSection(&c->lock);if(c->pipe!=INVALID_HANDLE_VALUE)CancelIoEx(c->pipe,NULL);LeaveCriticalSection(&c->lock);if(c->thread){WaitForSingleObject(c->thread,INFINITE);CloseHandle(c->thread);c->thread=NULL;}}
static void update(void *data,obs_data_t *settings){struct hub_camera *c=data;int slot=(int)obs_data_get_int(settings,"device")-1;if(slot<0||slot>=5)slot=0;if(c->thread&&slot==c->slot)return;stop(c);c->slot=slot;InterlockedExchange(&c->stopping,0);c->thread=CreateThread(NULL,0,reader,c,0,NULL);}
static void *create(obs_data_t *settings,obs_source_t *source){struct hub_camera *c=bzalloc(sizeof(*c));c->source=source;c->pipe=INVALID_HANDLE_VALUE;InitializeCriticalSection(&c->lock);obs_source_set_async_unbuffered(source,true);output_black(c);update(c,settings);return c;}
static void destroy(void *data){struct hub_camera *c=data;stop(c);DeleteCriticalSection(&c->lock);bfree(c);}
static void defaults(obs_data_t *settings){obs_data_set_default_int(settings,"device",1);}
static const char *get_name(void *unused){UNUSED_PARAMETER(unused);return obs_module_text("HubSourceName");}
static bool open_hub(obs_properties_t *properties,obs_property_t *property,void *data){UNUSED_PARAMETER(properties);UNUSED_PARAMETER(property);UNUSED_PARAMETER(data);wchar_t folder[1024];DWORD size=sizeof(folder);if(RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\MultiCameraTE",L"InstallDir",RRF_RT_REG_SZ,NULL,folder,&size)==ERROR_SUCCESS){wchar_t exe[1200];swprintf(exe,1200,L"%ls\\MultiCameraTE.exe",folder);ShellExecuteW(NULL,L"open",exe,NULL,folder,SW_SHOWNORMAL);}return false;}
static obs_properties_t *properties(void *data){struct hub_camera *c=data;obs_properties_t *p=obs_properties_create();obs_properties_add_text(p,"state",obs_module_text(c&&c->connected?"HubConnected":"HubWaiting"),OBS_TEXT_INFO);obs_property_t *list=obs_properties_add_list(p,"device",obs_module_text("HubDevice"),OBS_COMBO_TYPE_LIST,OBS_COMBO_FORMAT_INT);for(int i=0;i<5;i++){char label[192];snprintf(label,sizeof(label),"MultiCamera T.E. %d",i+1);obs_property_list_add_int(list,label,i+1);}obs_properties_add_button2(p,"open_hub",obs_module_text("HubOpen"),open_hub,NULL);obs_properties_add_text(p,"hint",obs_module_text("HubHint"),OBS_TEXT_INFO);return p;}
void mc_obs_register(void){static struct obs_source_info info={.id="multicamera_te",.type=OBS_SOURCE_TYPE_INPUT,.output_flags=OBS_SOURCE_ASYNC_VIDEO|OBS_SOURCE_AUDIO,.get_name=get_name,.create=create,.destroy=destroy,.get_defaults=defaults,.get_properties=properties,.update=update};obs_register_source(&info);}
