#include "hub.h"
#include <sddl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"
struct ipc_server {struct mc_hub *hub;CRITICAL_SECTION lock;HANDLE listeners[5],pending[5],clients[32],clients_done;volatile LONG stopping,count;PSECURITY_DESCRIPTOR security;};
struct pipe_client {struct ipc_server *server;HANDLE pipe;int slot,index;};
static DWORD WINAPI serve_pipe(void *arg){
 struct pipe_client *client=arg;struct ipc_server *server=client->server;struct mc_slot *slot=&server->hub->slot[client->slot];HANDLE pipe=client->pipe;int index=client->index;free(client);uint8_t *pixels=malloc(MC_BYTES);struct mc_audio *audio=malloc(sizeof(*audio)*16);
 if(!pixels||!audio)goto done;
 while(!server->stopping){struct mc_query query;if(!mc_pipe_io(pipe,&query,sizeof(query),false)||query.magic!=MC_PIPE_QUERY||(query.flags&~3u))break;
  EnterCriticalSection(&slot->lock);bool wait=(query.flags&MC_PIPE_WAIT)&&slot->sequence==query.after_video;if(wait)ResetEvent(slot->changed);LeaveCriticalSection(&slot->lock);if(wait)WaitForSingleObject(slot->changed,40);if(server->stopping)break;
  struct mc_reply reply={MC_PIPE_REPLY,MC_WIDTH,MC_HEIGHT,MC_BYTES,0,0,0,0};EnterCriticalSection(&slot->lock);reply.sequence=slot->sequence;reply.timestamp_us=slot->timestamp_us;reply.connected=slot->connected!=0;memcpy(pixels,slot->pixels,MC_BYTES);
  if((query.flags&MC_PIPE_AUDIO)&&slot->connected&&GetTickCount64()-slot->last_audio_ms<200){uint64_t first=slot->audio_sequence>15?slot->audio_sequence-15:1;if(query.after_audio>=first&&query.after_audio<UINT64_MAX)first=query.after_audio+1;for(uint64_t seq=first;seq<=slot->audio_sequence&&reply.audio_count<16;seq++){struct mc_audio *packet=&slot->audio[seq%16];if(packet->sequence==seq)audio[reply.audio_count++]=*packet;}}
  LeaveCriticalSection(&slot->lock);if(!mc_pipe_io(pipe,&reply,sizeof(reply),true)||!mc_pipe_io(pipe,pixels,MC_BYTES,true))break;
  bool ok=true;for(unsigned i=0;i<reply.audio_count;i++){struct mc_audio_header header={audio[i].sequence,audio[i].timestamp_us,audio[i].rate,audio[i].frames,audio[i].bytes,0};if(!mc_pipe_io(pipe,&header,sizeof(header),true)||!mc_pipe_io(pipe,audio[i].pcm,audio[i].bytes,true)){ok=false;break;}}if(!ok)break;
 }
done:
 free(pixels);free(audio);EnterCriticalSection(&server->lock);server->clients[index]=INVALID_HANDLE_VALUE;DisconnectNamedPipe(pipe);CloseHandle(pipe);if(InterlockedDecrement(&server->count)==0)SetEvent(server->clients_done);LeaveCriticalSection(&server->lock);return 0;
}
struct listen_context {struct ipc_server *server;int slot;};
static DWORD WINAPI listen_pipe(void *arg){struct listen_context *context=arg;struct ipc_server *server=context->server;int slot=context->slot;free(context);wchar_t name[256];swprintf(name,256,L"%ls.%d",server->hub->pipe_base,slot+1);SECURITY_ATTRIBUTES sa={sizeof(sa),server->security,FALSE};
 while(!server->stopping){HANDLE pipe=CreateNamedPipeW(name,PIPE_ACCESS_DUPLEX|FILE_FLAG_OVERLAPPED,PIPE_TYPE_BYTE|PIPE_READMODE_BYTE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,16,MC_BYTES+32768,32768,0,&sa);if(pipe==INVALID_HANDLE_VALUE)break;OVERLAPPED operation={0};operation.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);if(!operation.hEvent){CloseHandle(pipe);break;}
  EnterCriticalSection(&server->lock);server->pending[slot]=pipe;LeaveCriticalSection(&server->lock);BOOL ok=ConnectNamedPipe(pipe,&operation);DWORD error=GetLastError();if(!ok&&error==ERROR_PIPE_CONNECTED)ok=TRUE;else if(!ok&&error==ERROR_IO_PENDING){while(!server->stopping&&WaitForSingleObject(operation.hEvent,200)==WAIT_TIMEOUT){}if(server->stopping)CancelIoEx(pipe,&operation);DWORD transferred;ok=GetOverlappedResult(pipe,&operation,&transferred,TRUE);}CloseHandle(operation.hEvent);
  EnterCriticalSection(&server->lock);server->pending[slot]=INVALID_HANDLE_VALUE;int index=-1;if(ok&&!server->stopping)for(int i=0;i<32;i++)if(server->clients[i]==INVALID_HANDLE_VALUE){index=i;break;}
  if(index>=0){struct pipe_client *client=malloc(sizeof(*client));if(client){*client=(struct pipe_client){server,pipe,slot,index};server->clients[index]=pipe;InterlockedIncrement(&server->count);ResetEvent(server->clients_done);HANDLE thread=CreateThread(NULL,0,serve_pipe,client,0,NULL);if(thread)CloseHandle(thread);else{server->clients[index]=INVALID_HANDLE_VALUE;if(InterlockedDecrement(&server->count)==0)SetEvent(server->clients_done);free(client);CloseHandle(pipe);}}else CloseHandle(pipe);}else CloseHandle(pipe);LeaveCriticalSection(&server->lock);
 }return 0;}
bool mc_ipc_start(struct mc_hub *hub){
 struct ipc_server *server=calloc(1,sizeof(*server));if(!server)return false;server->hub=hub;InitializeCriticalSection(&server->lock);for(int i=0;i<5;i++)server->pending[i]=INVALID_HANDLE_VALUE;for(int i=0;i<32;i++)server->clients[i]=INVALID_HANDLE_VALUE;server->clients_done=CreateEventW(NULL,TRUE,TRUE,NULL);
 if(!mc_user_pipe(hub->pipe_base,192))goto failed;const wchar_t *sid=wcsrchr(hub->pipe_base,L'.');if(!sid)goto failed;wchar_t sddl[512];swprintf(sddl,512,L"D:P(A;;GA;;;SY)(A;;GA;;;LS)(A;;GA;;;%ls)",sid+1);if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,SDDL_REVISION_1,&server->security,NULL))goto failed;hub->ipc=server;
 for(int i=0;i<5;i++){struct listen_context *context=malloc(sizeof(*context));if(!context)goto failed;*context=(struct listen_context){server,i};server->listeners[i]=CreateThread(NULL,0,listen_pipe,context,0,NULL);if(!server->listeners[i]){free(context);goto failed;}}return true;
failed:hub->ipc=server;mc_ipc_stop(hub);return false;
}
void mc_ipc_stop(struct mc_hub *hub){struct ipc_server *server=hub->ipc;if(!server)return;InterlockedExchange(&server->stopping,1);EnterCriticalSection(&server->lock);for(int i=0;i<5;i++)if(server->pending[i]!=INVALID_HANDLE_VALUE)CancelIoEx(server->pending[i],NULL);for(int i=0;i<32;i++)if(server->clients[i]!=INVALID_HANDLE_VALUE)CancelIoEx(server->clients[i],NULL);LeaveCriticalSection(&server->lock);for(int i=0;i<5;i++)if(server->listeners[i]){WaitForSingleObject(server->listeners[i],INFINITE);CloseHandle(server->listeners[i]);}WaitForSingleObject(server->clients_done,INFINITE);CloseHandle(server->clients_done);if(server->security)LocalFree(server->security);DeleteCriticalSection(&server->lock);free(server);hub->ipc=NULL;}
