#include "../src/app.h"
#include <mfapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <sddl.h>
#include "../src/protocol.h"
int main(void){WSADATA wsa;WSAStartup(MAKEWORD(2,2),&wsa);CoInitializeEx(NULL,COINIT_MULTITHREADED);MFStartup(MF_VERSION,MFSTARTUP_FULL);struct mc_hub *hub=calloc(1,sizeof(*hub));if(!mc_hub_init(hub))return 2;
 for(int i=0;i<5;i++)snprintf(hub->slot[i].token,33,"%032x",i+1);if(!mc_hub_start(hub,34880))return 3;if(!mc_ipc_start(hub))return 5;
#ifdef MC_RELAY_TEST
 struct mc_app relay_app={0};relay_app.config.internet=1;wcscpy(relay_app.config.relay,L"wss://localhost:8765");wcscpy(relay_app.config.host_key,L"integration-test-only");strcpy(relay_app.config.room,"11111111111111111111111111111111");mc_relay_start(&relay_app);for(int n=0;n<100&&mc_relay_status(&relay_app)!=5;n++)Sleep(100);if(mc_relay_status(&relay_app)!=5)return 8;puts("FIVE_RELAYS_READY");
#endif
 puts("HUB_READY");fflush(stdout);
 bool overlap=false;for(int n=0;n<80;n++){int active=0;for(int i=0;i<5;i++)if(hub->slot[i].connected&&hub->slot[i].frames>10)active++;if(active==5)overlap=true;if(n==10||n==30){for(int j=0;j<5;j++){wchar_t name[256];swprintf(name,256,L"%ls.%d",hub->pipe_base,j+1);HANDLE pipe=CreateFileW(name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);if(pipe==INVALID_HANDLE_VALUE)return 6;struct mc_query query={MC_PIPE_QUERY,MC_PIPE_AUDIO,0,0};struct mc_reply reply={0};uint8_t *pixels=malloc(MC_BYTES);bool valid=mc_pipe_io(pipe,&query,sizeof(query),true)&&mc_pipe_io(pipe,&reply,sizeof(reply),false)&&reply.magic==MC_PIPE_REPLY&&reply.bytes==MC_BYTES&&reply.connected&&mc_pipe_io(pipe,pixels,MC_BYTES,false);free(pixels);if(valid&&n==10&&!reply.audio_count)return 9;if(valid&&n==30&&reply.audio_count)return 10;for(unsigned k=0;k<reply.audio_count&&valid;k++){struct mc_audio_header header={0};uint8_t pcm[19200];valid=mc_pipe_io(pipe,&header,sizeof(header),false)&&header.rate==48000&&header.frames==960&&header.bytes==1920&&header.timestamp_us>=(uint64_t)j*10000000&&mc_pipe_io(pipe,pcm,header.bytes,false);}CloseHandle(pipe);if(!valid)return 7;}puts(n==10?"FIVE_IPC_VIDEO_AUDIO_PASS":"FIVE_IPC_VIDEO_MUTED_PASS");fflush(stdout);}Sleep(100);}
 bool ok=overlap;for(int i=0;i<5;i++){struct mc_slot *slot=&hub->slot[i];EnterCriticalSection(&slot->lock);printf("SLOT %d frames=%llu pts=%llu input=%ux%u audio=%llu\n",i+1,(unsigned long long)slot->frames,(unsigned long long)slot->timestamp_us,slot->input_width,slot->input_height,(unsigned long long)slot->audio_frames);if(slot->frames<50||slot->audio_frames<960*20||slot->timestamp_us<(uint64_t)i*10000000||slot->timestamp_us>=(uint64_t)(i+1)*10000000||slot->input_width!=(i==1||i==3?480:640))ok=false;LeaveCriticalSection(&slot->lock);}
 printf("FIVE_SIMULTANEOUS=%s\n",overlap?"true":"false");
#ifdef MC_RELAY_TEST
 mc_relay_stop(&relay_app);
#endif
 mc_ipc_stop(hub);mc_hub_stop(hub);mc_hub_free(hub);free(hub);MFShutdown();CoUninitialize();WSACleanup();return ok?0:4;
}
