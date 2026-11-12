#pragma once
#include <windows.h>
#include <stdint.h>
#define MC_PIPE_QUERY 0x3151434d
#define MC_PIPE_REPLY 0x3152434d
#define MC_PIPE_AUDIO 1
#define MC_PIPE_WAIT 2
struct mc_query {uint32_t magic,flags;uint64_t after_video,after_audio;};
struct mc_reply {uint32_t magic,width,height,bytes;uint64_t sequence,timestamp_us;uint32_t connected,audio_count;};
struct mc_audio_header {uint64_t sequence,timestamp_us;uint32_t rate,frames,bytes,reserved;};
/* Overlapped I/O permits shutdown and bounded recovery from a stalled consumer. */
static inline bool mc_pipe_io(HANDLE pipe,void *data,DWORD bytes,bool writing){
 uint8_t *p=data;while(bytes){OVERLAPPED operation={0};operation.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);if(!operation.hEvent)return false;DWORD done=0;BOOL ok=writing?WriteFile(pipe,p,bytes,&done,&operation):ReadFile(pipe,p,bytes,&done,&operation);
  if(!ok&&GetLastError()==ERROR_IO_PENDING){DWORD wait=WaitForSingleObject(operation.hEvent,3000);if(wait!=WAIT_OBJECT_0){CancelIoEx(pipe,&operation);WaitForSingleObject(operation.hEvent,INFINITE);ok=FALSE;}else ok=GetOverlappedResult(pipe,&operation,&done,FALSE);}CloseHandle(operation.hEvent);if(!ok||!done)return false;p+=done;bytes-=done;
 }return true;
}
static inline bool mc_user_pipe(wchar_t *output,size_t capacity){
 HANDLE token=NULL;DWORD bytes=0;if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return false;GetTokenInformation(token,TokenUser,NULL,0,&bytes);TOKEN_USER *user=HeapAlloc(GetProcessHeap(),0,bytes);bool ok=user&&GetTokenInformation(token,TokenUser,user,bytes,&bytes);LPWSTR sid=NULL;if(ok)ok=ConvertSidToStringSidW(user->User.Sid,&sid)!=0;if(ok)swprintf(output,capacity,L"\\\\.\\pipe\\MultiCameraTE.%ls",sid);if(sid)LocalFree(sid);if(user)HeapFree(GetProcessHeap(),0,user);CloseHandle(token);return ok;
}
