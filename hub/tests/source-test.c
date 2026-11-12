#define COBJMACROS
#define CONST_VTABLE
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <stdio.h>
#include "../src/vcam.h"
typedef HRESULT(WINAPI *get_class_fn)(REFCLSID,REFIID,void**);
typedef HRESULT(WINAPI *unload_fn)(void);
int main(int argc,char **argv){if(argc<2||argc>3)return 2;CoInitializeEx(NULL,COINIT_MULTITHREADED);MFStartup(MF_VERSION,MFSTARTUP_FULL);HMODULE dll=LoadLibraryA(argv[1]);if(!dll){printf("LOAD %lu\n",GetLastError());return 3;}get_class_fn get=(get_class_fn)GetProcAddress(dll,"DllGetClassObject");unload_fn unload=(unload_fn)GetProcAddress(dll,"DllCanUnloadNow");
 for(int i=0;i<5;i++){IClassFactory *factory=NULL;IMFActivate *activation=NULL;IMFMediaSource *source=NULL;IMFSourceReader *reader=NULL;HRESULT hr=get(&mc_camera_ids[i],&IID_IClassFactory,(void**)&factory);if(SUCCEEDED(hr))hr=IClassFactory_CreateInstance(factory,NULL,&IID_IMFActivate,(void**)&activation);if(factory)IClassFactory_Release(factory);if(SUCCEEDED(hr))hr=IMFActivate_ActivateObject(activation,&IID_IMFMediaSource,(void**)&source);if(activation){IMFActivate_Release(activation);activation=NULL;}if(SUCCEEDED(hr))hr=MFCreateSourceReaderFromMediaSource(source,NULL,&reader);if(FAILED(hr)){printf("SOURCE_CREATE %d %08lx\n",i+1,(unsigned long)hr);return 4;}
  for(int frame=0;frame<10;frame++){IMFSample *sample=NULL;DWORD flags=0;LONGLONG timestamp=0;hr=IMFSourceReader_ReadSample(reader,MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,NULL,&flags,&timestamp,&sample);if(FAILED(hr)||!sample){printf("READ_SAMPLE %d %08lx flags=%lu\n",i+1,(unsigned long)hr,flags);return 5;}DWORD size=0;IMFSample_GetTotalLength(sample,&size);if(argc==3){IMFMediaBuffer *buffer=NULL;BYTE *pixels=NULL;DWORD length=0;HRESULT check=IMFSample_ConvertToContiguousBuffer(sample,&buffer);if(SUCCEEDED(check))check=IMFMediaBuffer_Lock(buffer,&pixels,NULL,&length);unsigned long long total=0;if(SUCCEEDED(check)&&length>=1280*720){for(unsigned n=0;n<1280*720;n+=16)total+=pixels[n];IMFMediaBuffer_Unlock(buffer);}if(buffer)IMFMediaBuffer_Release(buffer);if(FAILED(check)||total/(1280*720/16)<=20){printf("LIVE_PIXELS_MISSING %d\n",i+1);return 8;}}IMFSample_Release(sample);if(size!=1280*720*3/2){printf("SIZE %lu\n",size);return 6;}}
  IMFSourceReader_Release(reader);IMFMediaSource_Shutdown(source);IMFMediaSource_Release(source);if(activation){IMFActivate_ShutdownObject(activation);IMFActivate_Release(activation);}printf("VIRTUAL_SOURCE_%d_PASS\n",i+1);
 }
 HRESULT can=unload();printf("SOURCE_UNLOAD=%08lx\n",(unsigned long)can);FreeLibrary(dll);MFShutdown();CoUninitialize();return can==S_OK?0:7;
}
