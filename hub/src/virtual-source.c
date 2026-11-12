/* Windows 11 software camera source. CPU NV12 buffers, no kernel driver. GPL-2.0-or-later. */
#define COBJMACROS
#define CONST_VTABLE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mferror.h>
#include <ks.h>
#include <ksmedia.h>
#include <ksproxy.h>
#include <sddl.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "protocol.h"
#include "vcam.h"
#define WIDTH 1280
#define HEIGHT 720
#define FRAME_BYTES (WIDTH*HEIGHT*3/2)
static volatile LONG objects,locks;
struct source;
struct allocator_vtbl {HRESULT(WINAPI *QueryInterface)(void*,REFIID,void**);ULONG(WINAPI *AddRef)(void*);ULONG(WINAPI *Release)(void*);HRESULT(WINAPI *SetDefaultAllocator)(void*,DWORD,IUnknown*);HRESULT(WINAPI *GetAllocatorUsage)(void*,DWORD,DWORD*,int*);};
struct allocator {const struct allocator_vtbl *lpVtbl;};
struct stream_vtbl {IMFMediaStreamVtbl base;HRESULT(WINAPI *SetStreamState)(IMFMediaStream*,int);HRESULT(WINAPI *GetStreamState)(IMFMediaStream*,int*);};
struct stream {IMFMediaStream iface;LONG refs;CRITICAL_SECTION lock;IMFMediaEventQueue *events;IMFStreamDescriptor *descriptor;struct source *parent;HANDLE thread,wake,pipe;volatile LONG closing,state;IUnknown *tokens[64];int begin,count;unsigned fps;LONGLONG next_time;wchar_t pipe_name[256];};
struct source {IMFMediaSourceEx iface;IMFGetService service;IKsControl ks;struct allocator allocator;LONG refs,closing;CRITICAL_SECTION lock;IMFMediaEventQueue *events;IMFAttributes *attributes,*stream_attributes;IMFPresentationDescriptor *presentation;struct stream *stream;int slot;bool announced;};
static HRESULT WINAPI source_shutdown(IMFMediaSourceEx *);
static ULONG WINAPI source_release(IMFMediaSourceEx *);
static HRESULT WINAPI source_query(IMFMediaSourceEx *,REFIID,void**);
static void black(uint8_t *p){memset(p,16,WIDTH*HEIGHT);memset(p+WIDTH*HEIGHT,128,WIDTH*HEIGHT/2);}
static void disconnect_stream(struct stream *s){EnterCriticalSection(&s->lock);HANDLE pipe=s->pipe;s->pipe=INVALID_HANDLE_VALUE;LeaveCriticalSection(&s->lock);if(pipe!=INVALID_HANDLE_VALUE)CloseHandle(pipe);}
static bool snapshot(struct stream *s,uint8_t *pixels){
 EnterCriticalSection(&s->lock);HANDLE pipe=s->pipe;LeaveCriticalSection(&s->lock);
 if(pipe==INVALID_HANDLE_VALUE){pipe=CreateFileW(s->pipe_name,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);if(pipe==INVALID_HANDLE_VALUE)return false;EnterCriticalSection(&s->lock);s->pipe=pipe;LeaveCriticalSection(&s->lock);}
 struct mc_query query={MC_PIPE_QUERY,0,0,0};struct mc_reply reply={0};bool ok=mc_pipe_io(pipe,&query,sizeof(query),true)&&mc_pipe_io(pipe,&reply,sizeof(reply),false)&&reply.magic==MC_PIPE_REPLY&&reply.width==WIDTH&&reply.height==HEIGHT&&reply.bytes==FRAME_BYTES&&reply.audio_count==0&&mc_pipe_io(pipe,pixels,FRAME_BYTES,false);if(!ok)disconnect_stream(s);return ok;
}
static DWORD WINAPI produce_samples(void *data){struct stream *s=data;CoInitializeEx(NULL,COINIT_MULTITHREADED);uint8_t *pixels=malloc(FRAME_BYTES);if(!pixels)return 0;
 while(!s->closing){IUnknown *token=NULL;bool pending=false;EnterCriticalSection(&s->lock);if(s->count){token=s->tokens[s->begin];s->begin=(s->begin+1)%64;s->count--;pending=true;}LeaveCriticalSection(&s->lock);if(!pending){WaitForSingleObject(s->wake,INFINITE);continue;}
  if(s->state!=2){if(token)IUnknown_Release(token);continue;}LONGLONG now=MFGetSystemTime(),duration=10000000/(s->fps?s->fps:30);if(s->next_time>now){DWORD delay=(DWORD)((s->next_time-now)/10000);if(delay>100)delay=100;Sleep(delay);}
  if(!snapshot(s,pixels))black(pixels);if(s->closing||s->state!=2){if(token)IUnknown_Release(token);continue;}
  IMFSample *sample=NULL;IMFMediaBuffer *buffer=NULL;BYTE *bytes=NULL;HRESULT hr=MFCreateSample(&sample);if(SUCCEEDED(hr))hr=MFCreateMemoryBuffer(FRAME_BYTES,&buffer);if(SUCCEEDED(hr))hr=IMFMediaBuffer_Lock(buffer,&bytes,NULL,NULL);if(SUCCEEDED(hr)){memcpy(bytes,pixels,FRAME_BYTES);IMFMediaBuffer_Unlock(buffer);IMFMediaBuffer_SetCurrentLength(buffer,FRAME_BYTES);IMFSample_AddBuffer(sample,buffer);LONGLONG timestamp=MFGetSystemTime();if(s->next_time&&timestamp<s->next_time)timestamp=s->next_time;s->next_time=timestamp+duration;IMFSample_SetSampleTime(sample,timestamp);IMFSample_SetSampleDuration(sample,duration);if(token)IMFSample_SetUnknown(sample,&MFSampleExtension_Token,token);IMFMediaEventQueue_QueueEventParamUnk(s->events,MEMediaSample,&GUID_NULL,S_OK,(IUnknown*)sample);}if(buffer)IMFMediaBuffer_Release(buffer);if(sample)IMFSample_Release(sample);if(token)IUnknown_Release(token);
 }disconnect_stream(s);free(pixels);CoUninitialize();return 0;
}
static HRESULT WINAPI stream_query(IMFMediaStream *iface,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;if(IsEqualIID(iid,&IID_IUnknown)||IsEqualIID(iid,&IID_IMFMediaEventGenerator)||IsEqualIID(iid,&IID_IMFMediaStream)||IsEqualIID(iid,&mc_iid_stream2)){*out=iface;InterlockedIncrement(&((struct stream*)iface)->refs);return S_OK;}return E_NOINTERFACE;}
static ULONG WINAPI stream_addref(IMFMediaStream *iface){return InterlockedIncrement(&((struct stream*)iface)->refs);}
static ULONG WINAPI stream_release(IMFMediaStream *iface){struct stream *s=(struct stream*)iface;LONG refs=InterlockedDecrement(&s->refs);if(!refs){if(s->events)IMFMediaEventQueue_Release(s->events);if(s->descriptor)IMFStreamDescriptor_Release(s->descriptor);if(s->wake)CloseHandle(s->wake);DeleteCriticalSection(&s->lock);free(s);InterlockedDecrement(&objects);}return refs;}
static HRESULT WINAPI stream_getevent(IMFMediaStream *iface,DWORD flags,IMFMediaEvent **event){struct stream *s=(struct stream*)iface;return s->closing?MF_E_SHUTDOWN:IMFMediaEventQueue_GetEvent(s->events,flags,event);}
static HRESULT WINAPI stream_beginevent(IMFMediaStream *iface,IMFAsyncCallback *cb,IUnknown *state){struct stream *s=(struct stream*)iface;return s->closing?MF_E_SHUTDOWN:IMFMediaEventQueue_BeginGetEvent(s->events,cb,state);}
static HRESULT WINAPI stream_endevent(IMFMediaStream *iface,IMFAsyncResult *result,IMFMediaEvent **event){struct stream *s=(struct stream*)iface;return s->closing?MF_E_SHUTDOWN:IMFMediaEventQueue_EndGetEvent(s->events,result,event);}
static HRESULT WINAPI stream_queueevent(IMFMediaStream *iface,MediaEventType type,REFGUID guid,HRESULT status,const PROPVARIANT *value){struct stream *s=(struct stream*)iface;return s->closing?MF_E_SHUTDOWN:IMFMediaEventQueue_QueueEventParamVar(s->events,type,guid,status,value);}
static HRESULT WINAPI stream_getsource(IMFMediaStream *iface,IMFMediaSource **out){if(!out)return E_POINTER;struct stream *s=(struct stream*)iface;EnterCriticalSection(&s->lock);HRESULT hr=s->closing||!s->parent?MF_E_SHUTDOWN:source_query(&s->parent->iface,&IID_IMFMediaSource,(void**)out);LeaveCriticalSection(&s->lock);return hr;}
static HRESULT WINAPI stream_getdescriptor(IMFMediaStream *iface,IMFStreamDescriptor **out){if(!out)return E_POINTER;struct stream *s=(struct stream*)iface;if(s->closing)return MF_E_SHUTDOWN;*out=s->descriptor;IMFStreamDescriptor_AddRef(*out);return S_OK;}
static HRESULT WINAPI stream_request(IMFMediaStream *iface,IUnknown *token){struct stream *s=(struct stream*)iface;EnterCriticalSection(&s->lock);HRESULT hr=S_OK;if(s->closing)hr=MF_E_SHUTDOWN;else if(s->state!=2)hr=MF_E_INVALIDREQUEST;else if(s->count>=64)hr=MF_E_NOTACCEPTING;else{s->tokens[(s->begin+s->count)%64]=token;if(token)IUnknown_AddRef(token);s->count++;SetEvent(s->wake);}LeaveCriticalSection(&s->lock);return hr;}
static HRESULT WINAPI stream_setstate(IMFMediaStream *iface,int value){struct stream *s=(struct stream*)iface;if(value<0||value>2)return E_INVALIDARG;if(s->closing)return MF_E_SHUTDOWN;InterlockedExchange(&s->state,value);SetEvent(s->wake);return S_OK;}
static HRESULT WINAPI stream_getstate(IMFMediaStream *iface,int *value){if(!value)return E_POINTER;struct stream *s=(struct stream*)iface;if(s->closing)return MF_E_SHUTDOWN;*value=s->state;return S_OK;}
static const struct stream_vtbl stream_vtable={{stream_query,stream_addref,stream_release,stream_getevent,stream_beginevent,stream_endevent,stream_queueevent,stream_getsource,stream_getdescriptor,stream_request},stream_setstate,stream_getstate};
static void stream_close(struct stream *s){if(InterlockedExchange(&s->closing,1))return;EnterCriticalSection(&s->lock);if(s->pipe!=INVALID_HANDLE_VALUE)CancelIoEx(s->pipe,NULL);SetEvent(s->wake);LeaveCriticalSection(&s->lock);if(s->thread){WaitForSingleObject(s->thread,INFINITE);CloseHandle(s->thread);s->thread=NULL;}if(s->events)IMFMediaEventQueue_Shutdown(s->events);EnterCriticalSection(&s->lock);while(s->count){IUnknown *token=s->tokens[s->begin];s->begin=(s->begin+1)%64;s->count--;if(token)IUnknown_Release(token);}struct source *parent=s->parent;s->parent=NULL;LeaveCriticalSection(&s->lock);if(parent)source_release(&parent->iface);}
static HRESULT WINAPI source_query(IMFMediaSourceEx *iface,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;struct source *s=(struct source*)iface;
 if(IsEqualIID(iid,&IID_IUnknown)||IsEqualIID(iid,&IID_IMFMediaEventGenerator)||IsEqualIID(iid,&IID_IMFMediaSource)||IsEqualIID(iid,&IID_IMFMediaSourceEx))*out=iface;else if(IsEqualIID(iid,&IID_IMFGetService))*out=&s->service;else if(IsEqualIID(iid,&IID_IKsControl))*out=&s->ks;else if(IsEqualIID(iid,&mc_iid_allocator))*out=&s->allocator;else return E_NOINTERFACE;InterlockedIncrement(&s->refs);return S_OK;}
static ULONG WINAPI source_addref(IMFMediaSourceEx *iface){return InterlockedIncrement(&((struct source*)iface)->refs);}
static ULONG WINAPI source_release(IMFMediaSourceEx *iface){struct source *s=(struct source*)iface;LONG refs=InterlockedDecrement(&s->refs);if(!refs){if(s->stream)IMFMediaStream_Release(&s->stream->iface);if(s->presentation)IMFPresentationDescriptor_Release(s->presentation);if(s->attributes)IMFAttributes_Release(s->attributes);if(s->stream_attributes)IMFAttributes_Release(s->stream_attributes);if(s->events)IMFMediaEventQueue_Release(s->events);DeleteCriticalSection(&s->lock);free(s);InterlockedDecrement(&objects);}return refs;}
static HRESULT WINAPI source_getevent(IMFMediaSourceEx *iface,DWORD flags,IMFMediaEvent **event){struct source *s=(struct source*)iface;return s->closing?MF_E_SHUTDOWN:IMFMediaEventQueue_GetEvent(s->events,flags,event);}
static HRESULT WINAPI source_beginevent(IMFMediaSourceEx *iface,IMFAsyncCallback *callback,IUnknown *state){struct source *s=(struct source*)iface;return s->closing?MF_E_SHUTDOWN:IMFMediaEventQueue_BeginGetEvent(s->events,callback,state);}
static HRESULT WINAPI source_endevent(IMFMediaSourceEx *iface,IMFAsyncResult *result,IMFMediaEvent **event){struct source *s=(struct source*)iface;return s->closing?MF_E_SHUTDOWN:IMFMediaEventQueue_EndGetEvent(s->events,result,event);}
static HRESULT WINAPI source_queueevent(IMFMediaSourceEx *iface,MediaEventType type,REFGUID guid,HRESULT status,const PROPVARIANT *value){struct source *s=(struct source*)iface;return s->closing?MF_E_SHUTDOWN:IMFMediaEventQueue_QueueEventParamVar(s->events,type,guid,status,value);}
static HRESULT WINAPI source_characteristics(IMFMediaSourceEx *iface,DWORD *flags){if(!flags)return E_POINTER;if(((struct source*)iface)->closing)return MF_E_SHUTDOWN;*flags=MFMEDIASOURCE_IS_LIVE;return S_OK;}
static HRESULT WINAPI source_presentation(IMFMediaSourceEx *iface,IMFPresentationDescriptor **out){if(!out)return E_POINTER;struct source *s=(struct source*)iface;if(s->closing)return MF_E_SHUTDOWN;return IMFPresentationDescriptor_Clone(s->presentation,out);}
static HRESULT WINAPI source_start(IMFMediaSourceEx *iface,IMFPresentationDescriptor *presentation,const GUID *format,const PROPVARIANT *position){
 struct source *s=(struct source*)iface;if(s->closing)return MF_E_SHUTDOWN;if(!presentation||!position)return E_POINTER;if(format&&!IsEqualGUID(format,&GUID_NULL))return MF_E_UNSUPPORTED_TIME_FORMAT;if(position->vt!=VT_EMPTY&&position->vt!=VT_I8)return MF_E_UNSUPPORTED_TIME_FORMAT;
 BOOL selected=FALSE;IMFStreamDescriptor *descriptor=NULL;HRESULT hr=IMFPresentationDescriptor_GetStreamDescriptorByIndex(presentation,0,&selected,&descriptor);if(FAILED(hr))return hr;if(!selected){IMFStreamDescriptor_Release(descriptor);return E_INVALIDARG;}IMFMediaTypeHandler *handler=NULL;IMFMediaType *type=NULL;hr=IMFStreamDescriptor_GetMediaTypeHandler(descriptor,&handler);if(SUCCEEDED(hr))hr=IMFMediaTypeHandler_GetCurrentMediaType(handler,&type);if(handler)IMFMediaTypeHandler_Release(handler);IMFStreamDescriptor_Release(descriptor);if(FAILED(hr))return hr;
 GUID subtype;UINT64 size=0,rate=0;hr=IMFMediaType_GetGUID(type,&MF_MT_SUBTYPE,&subtype);IMFMediaType_GetUINT64(type,&MF_MT_FRAME_SIZE,&size);IMFMediaType_GetUINT64(type,&MF_MT_FRAME_RATE,&rate);IMFMediaType_Release(type);unsigned fps=(UINT32)rate?(unsigned)(rate>>32)/(UINT32)rate:30;if(FAILED(hr)||!IsEqualGUID(&subtype,&MFVideoFormat_NV12)||size!=((UINT64)WIDTH<<32|HEIGHT)||(fps!=30&&fps!=60))return MF_E_INVALIDMEDIATYPE;
 EnterCriticalSection(&s->lock);s->stream->fps=fps;s->stream->next_time=0;InterlockedExchange(&s->stream->state,2);IMFMediaEventQueue_QueueEventParamUnk(s->events,s->announced?MEUpdatedStream:MENewStream,&GUID_NULL,S_OK,(IUnknown*)&s->stream->iface);s->announced=true;PROPVARIANT value;PropVariantInit(&value);value.vt=VT_I8;value.hVal.QuadPart=MFGetSystemTime();IMFMediaEventQueue_QueueEventParamVar(s->stream->events,MEStreamStarted,&GUID_NULL,S_OK,&value);hr=IMFMediaEventQueue_QueueEventParamVar(s->events,MESourceStarted,&GUID_NULL,S_OK,&value);LeaveCriticalSection(&s->lock);return hr;
}
static HRESULT WINAPI source_stop(IMFMediaSourceEx *iface){struct source *s=(struct source*)iface;if(s->closing)return MF_E_SHUTDOWN;InterlockedExchange(&s->stream->state,0);SetEvent(s->stream->wake);IMFMediaEventQueue_QueueEventParamVar(s->stream->events,MEStreamStopped,&GUID_NULL,S_OK,NULL);return IMFMediaEventQueue_QueueEventParamVar(s->events,MESourceStopped,&GUID_NULL,S_OK,NULL);}
static HRESULT WINAPI source_pause(IMFMediaSourceEx *iface){return ((struct source*)iface)->closing?MF_E_SHUTDOWN:MF_E_INVALID_STATE_TRANSITION;}
static HRESULT WINAPI source_shutdown(IMFMediaSourceEx *iface){struct source *s=(struct source*)iface;if(InterlockedExchange(&s->closing,1))return S_OK;if(s->stream)stream_close(s->stream);if(s->events)IMFMediaEventQueue_Shutdown(s->events);return S_OK;}
static HRESULT WINAPI source_attributes(IMFMediaSourceEx *iface,IMFAttributes **out){if(!out)return E_POINTER;struct source *s=(struct source*)iface;if(s->closing)return MF_E_SHUTDOWN;*out=s->attributes;IMFAttributes_AddRef(*out);return S_OK;}
static HRESULT WINAPI source_streamattributes(IMFMediaSourceEx *iface,DWORD id,IMFAttributes **out){if(!out)return E_POINTER;if(id)return MF_E_INVALIDSTREAMNUMBER;struct source *s=(struct source*)iface;if(s->closing)return MF_E_SHUTDOWN;*out=s->stream_attributes;IMFAttributes_AddRef(*out);return S_OK;}
static HRESULT WINAPI source_d3d(IMFMediaSourceEx *iface,IUnknown *manager){(void)manager;return ((struct source*)iface)->closing?MF_E_SHUTDOWN:S_OK;}
static const IMFMediaSourceExVtbl source_vtable={source_query,source_addref,source_release,source_getevent,source_beginevent,source_endevent,source_queueevent,source_characteristics,source_presentation,source_start,source_stop,source_pause,source_shutdown,source_attributes,source_streamattributes,source_d3d};
static HRESULT WINAPI service_query(IMFGetService *iface,REFIID iid,void **out){return source_query(&CONTAINING_RECORD(iface,struct source,service)->iface,iid,out);}
static ULONG WINAPI service_addref(IMFGetService *iface){return source_addref(&CONTAINING_RECORD(iface,struct source,service)->iface);}
static ULONG WINAPI service_release(IMFGetService *iface){return source_release(&CONTAINING_RECORD(iface,struct source,service)->iface);}
static HRESULT WINAPI service_get(IMFGetService *iface,REFGUID service,REFIID iid,void **out){(void)iface;(void)service;(void)iid;if(out)*out=NULL;return MF_E_UNSUPPORTED_SERVICE;}
static const IMFGetServiceVtbl service_vtable={service_query,service_addref,service_release,service_get};
static HRESULT WINAPI ks_query(IKsControl *iface,REFIID iid,void **out){return source_query(&CONTAINING_RECORD(iface,struct source,ks)->iface,iid,out);}
static ULONG WINAPI ks_addref(IKsControl *iface){return source_addref(&CONTAINING_RECORD(iface,struct source,ks)->iface);}
static ULONG WINAPI ks_release(IKsControl *iface){return source_release(&CONTAINING_RECORD(iface,struct source,ks)->iface);}
static HRESULT WINAPI ks_property(IKsControl *iface,PKSPROPERTY property,ULONG property_length,void *data,ULONG data_length,ULONG *returned){(void)iface;(void)property;(void)property_length;(void)data;(void)data_length;if(returned)*returned=0;return HRESULT_FROM_WIN32(ERROR_SET_NOT_FOUND);}
static HRESULT WINAPI ks_method(IKsControl *iface,PKSMETHOD method,ULONG length,void *data,ULONG bytes,ULONG *returned){return ks_property(iface,(PKSPROPERTY)method,length,data,bytes,returned);}
static HRESULT WINAPI ks_event(IKsControl *iface,PKSEVENT event,ULONG length,void *data,ULONG bytes,ULONG *returned){return ks_property(iface,(PKSPROPERTY)event,length,data,bytes,returned);}
struct complete_ks_vtbl {HRESULT(WINAPI *query)(IKsControl*,REFIID,void**);ULONG(WINAPI *addref)(IKsControl*);ULONG(WINAPI *release)(IKsControl*);IKsControlVtbl methods;};
static const struct complete_ks_vtbl ks_vtable={ks_query,ks_addref,ks_release,{ks_property,ks_method,ks_event}};
static HRESULT WINAPI allocator_query(void *iface,REFIID iid,void **out){return source_query(&CONTAINING_RECORD(iface,struct source,allocator)->iface,iid,out);}
static ULONG WINAPI allocator_addref(void *iface){return source_addref(&CONTAINING_RECORD(iface,struct source,allocator)->iface);}
static ULONG WINAPI allocator_release(void *iface){return source_release(&CONTAINING_RECORD(iface,struct source,allocator)->iface);}
static HRESULT WINAPI allocator_set(void *iface,DWORD stream,IUnknown *provided){(void)iface;(void)provided;return stream?MF_E_INVALIDSTREAMNUMBER:S_OK;}
static HRESULT WINAPI allocator_usage(void *iface,DWORD stream,DWORD *input,int *usage){(void)iface;if(stream)return MF_E_INVALIDSTREAMNUMBER;if(!input||!usage)return E_POINTER;*input=0;*usage=1;return S_OK;}
static const struct allocator_vtbl allocator_vtable={allocator_query,allocator_addref,allocator_release,allocator_set,allocator_usage};
static HRESULT make_source(int slot,IMFMediaSourceEx **out){
 struct source *s=calloc(1,sizeof(*s));struct stream *stream=calloc(1,sizeof(*stream));if(!s||!stream){free(s);free(stream);return E_OUTOFMEMORY;}InterlockedAdd(&objects,2);s->iface.lpVtbl=&source_vtable;s->service.lpVtbl=&service_vtable;s->ks.lpVtbl=(const IKsControlVtbl*)&ks_vtable;s->allocator.lpVtbl=&allocator_vtable;s->refs=1;s->slot=slot;InitializeCriticalSection(&s->lock);stream->iface.lpVtbl=&stream_vtable.base;stream->refs=1;stream->pipe=INVALID_HANDLE_VALUE;stream->fps=30;InitializeCriticalSection(&stream->lock);s->stream=stream;
 HRESULT hr=MFCreateEventQueue(&s->events);if(SUCCEEDED(hr))hr=MFCreateEventQueue(&stream->events);if(SUCCEEDED(hr))hr=MFCreateAttributes(&s->attributes,4);if(SUCCEEDED(hr))hr=MFCreateAttributes(&s->stream_attributes,4);if(FAILED(hr))goto failed;
 IMFAttributes_SetUINT32(s->attributes,&mc_frame_shared,TRUE);IMFAttributes_SetUINT32(s->stream_attributes,&mc_stream_id,0);IMFAttributes_SetGUID(s->stream_attributes,&mc_stream_category,&PINNAME_VIDEO_CAPTURE);IMFAttributes_SetUINT32(s->stream_attributes,&mc_frame_shared,TRUE);
 IMFMediaType *types[2]={0};for(int i=0;i<2;i++){hr=MFCreateMediaType(&types[i]);if(FAILED(hr))break;IMFMediaType_SetGUID(types[i],&MF_MT_MAJOR_TYPE,&MFMediaType_Video);IMFMediaType_SetGUID(types[i],&MF_MT_SUBTYPE,&MFVideoFormat_NV12);IMFMediaType_SetUINT64(types[i],&MF_MT_FRAME_SIZE,((UINT64)WIDTH<<32)|HEIGHT);IMFMediaType_SetUINT64(types[i],&MF_MT_FRAME_RATE,((UINT64)(i?60:30)<<32)|1);IMFMediaType_SetUINT64(types[i],&MF_MT_PIXEL_ASPECT_RATIO,((UINT64)1<<32)|1);IMFMediaType_SetUINT32(types[i],&MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive);IMFMediaType_SetUINT32(types[i],&MF_MT_DEFAULT_STRIDE,WIDTH);IMFMediaType_SetUINT32(types[i],&MF_MT_SAMPLE_SIZE,FRAME_BYTES);IMFMediaType_SetUINT32(types[i],&MF_MT_FIXED_SIZE_SAMPLES,TRUE);IMFMediaType_SetUINT32(types[i],&MF_MT_ALL_SAMPLES_INDEPENDENT,TRUE);IMFMediaType_SetUINT32(types[i],&MF_MT_YUV_MATRIX,MFVideoTransferMatrix_BT709);IMFMediaType_SetUINT32(types[i],&MF_MT_VIDEO_NOMINAL_RANGE,MFNominalRange_16_235);}
 if(SUCCEEDED(hr))hr=MFCreateStreamDescriptor(0,2,types,&stream->descriptor);if(SUCCEEDED(hr)){IMFMediaTypeHandler *handler=NULL;hr=IMFStreamDescriptor_GetMediaTypeHandler(stream->descriptor,&handler);if(SUCCEEDED(hr)){hr=IMFMediaTypeHandler_SetCurrentMediaType(handler,types[0]);IMFMediaTypeHandler_Release(handler);}}for(int i=0;i<2;i++)if(types[i])IMFMediaType_Release(types[i]);if(FAILED(hr))goto failed;
 hr=MFCreatePresentationDescriptor(1,&stream->descriptor,&s->presentation);if(FAILED(hr))goto failed;IMFPresentationDescriptor_SelectStream(s->presentation,0);IMFAttributes_CopyAllItems(s->stream_attributes,(IMFAttributes*)stream->descriptor);
 wchar_t base[192]={0};DWORD bytes=sizeof(base);if(RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\MultiCameraTE",L"PipeBase",RRF_RT_REG_SZ,NULL,base,&bytes)!=ERROR_SUCCESS){if(!mc_user_pipe(base,192)){hr=E_FAIL;goto failed;}}swprintf(stream->pipe_name,256,L"%ls.%d",base,slot+1);
 stream->wake=CreateEventW(NULL,FALSE,FALSE,NULL);if(!stream->wake){hr=E_OUTOFMEMORY;goto failed;}stream->parent=s;source_addref(&s->iface);stream->thread=CreateThread(NULL,0,produce_samples,stream,0,NULL);if(!stream->thread){stream->parent=NULL;source_release(&s->iface);hr=E_FAIL;goto failed;}*out=&s->iface;return S_OK;
failed:source_shutdown(&s->iface);source_release(&s->iface);return hr;
}
struct activation {IMFActivate iface;LONG refs;IMFAttributes *attributes;IMFMediaSourceEx *source;int slot;};
static HRESULT WINAPI activate_query(IMFActivate *iface,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;if(IsEqualIID(iid,&IID_IUnknown)||IsEqualIID(iid,&IID_IMFAttributes)||IsEqualIID(iid,&IID_IMFActivate)){*out=iface;InterlockedIncrement(&((struct activation*)iface)->refs);return S_OK;}return E_NOINTERFACE;}
static ULONG WINAPI activate_addref(IMFActivate *iface){return InterlockedIncrement(&((struct activation*)iface)->refs);}
static ULONG WINAPI activate_release(IMFActivate *iface){struct activation *a=(struct activation*)iface;LONG refs=InterlockedDecrement(&a->refs);if(!refs){if(a->source)IMFMediaSourceEx_Release(a->source);if(a->attributes)IMFAttributes_Release(a->attributes);free(a);InterlockedDecrement(&objects);}return refs;}
/* Attribute forwarding functions and activation vtable are generated from the SDK ABI below. */
#include "activate-forward.h"
static HRESULT WINAPI activate_object(IMFActivate *iface,REFIID iid,void **out){struct activation *a=(struct activation*)iface;if(!a->source){HRESULT hr=make_source(a->slot,&a->source);if(FAILED(hr))return hr;IMFAttributes_CopyAllItems(a->attributes,((struct source*)a->source)->attributes);}return IMFMediaSourceEx_QueryInterface(a->source,iid,out);}
static HRESULT WINAPI activate_shutdown(IMFActivate *iface){struct activation *a=(struct activation*)iface;if(a->source){IMFMediaSourceEx_Shutdown(a->source);IMFMediaSourceEx_Release(a->source);a->source=NULL;}return S_OK;}
static HRESULT WINAPI activate_detach(IMFActivate *iface){struct activation *a=(struct activation*)iface;if(a->source){IMFMediaSourceEx_Release(a->source);a->source=NULL;}return S_OK;}
#include "activate-vtable.h"
struct factory {IClassFactory iface;LONG refs;int slot;};
static HRESULT WINAPI factory_query(IClassFactory *iface,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;if(IsEqualIID(iid,&IID_IUnknown)||IsEqualIID(iid,&IID_IClassFactory)){*out=iface;InterlockedIncrement(&((struct factory*)iface)->refs);return S_OK;}return E_NOINTERFACE;}
static ULONG WINAPI factory_addref(IClassFactory *iface){return InterlockedIncrement(&((struct factory*)iface)->refs);}
static ULONG WINAPI factory_release(IClassFactory *iface){struct factory *f=(struct factory*)iface;LONG refs=InterlockedDecrement(&f->refs);if(!refs){free(f);InterlockedDecrement(&objects);}return refs;}
static HRESULT WINAPI factory_create(IClassFactory *iface,IUnknown *outer,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;if(outer)return CLASS_E_NOAGGREGATION;struct activation *a=calloc(1,sizeof(*a));if(!a)return E_OUTOFMEMORY;a->iface.lpVtbl=&activate_vtable;a->refs=1;a->slot=((struct factory*)iface)->slot;InterlockedIncrement(&objects);HRESULT hr=MFCreateAttributes(&a->attributes,8);if(SUCCEEDED(hr))hr=activate_query(&a->iface,iid,out);activate_release(&a->iface);return hr;}
static HRESULT WINAPI factory_lock(IClassFactory *iface,BOOL lock){(void)iface;if(lock)InterlockedIncrement(&locks);else InterlockedDecrement(&locks);return S_OK;}
static const IClassFactoryVtbl factory_vtable={factory_query,factory_addref,factory_release,factory_create,factory_lock};
HRESULT WINAPI DllGetClassObject(REFCLSID clsid,REFIID iid,void **out){if(!out)return E_POINTER;*out=NULL;int slot=-1;for(int i=0;i<5;i++)if(IsEqualCLSID(clsid,&mc_camera_ids[i]))slot=i;if(slot<0)return CLASS_E_CLASSNOTAVAILABLE;struct factory *f=calloc(1,sizeof(*f));if(!f)return E_OUTOFMEMORY;f->iface.lpVtbl=&factory_vtable;f->refs=1;f->slot=slot;InterlockedIncrement(&objects);HRESULT hr=factory_query(&f->iface,iid,out);factory_release(&f->iface);return hr;}
HRESULT WINAPI DllCanUnloadNow(void){return objects==0&&locks==0?S_OK:S_FALSE;}
