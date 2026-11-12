/* Run after setup has registered the COM source. Creates only our five session cameras. */
#include "../src/app.h"
#include <mfreadwrite.h>
#include <stdio.h>
int main(int argc,char **argv){
 CoInitializeEx(NULL,COINIT_MULTITHREADED);MFStartup(MF_VERSION,MFSTARTUP_FULL);
 bool existing=argc>1&&(!strcmp(argv[1],"--existing")||!strcmp(argv[1],"--existing-live"));struct mc_app *app=calloc(1,sizeof(*app));if(!existing)mc_cameras_start(app);else app->camera_status=1;
 if(!existing){ULONGLONG deadline=GetTickCount64()+15000;while(!app->camera_status&&GetTickCount64()<deadline)Sleep(20);if(!app->camera_status){puts("WINDOWS_CAMERA_START_TIMEOUT");mc_cameras_stop(app);return 2;}}
 printf("WINDOWS_CAMERA_STATUS=%ld HRESULT=%08lx\n",app->camera_status,(unsigned long)app->camera_error);fflush(stdout);
 IMFAttributes *attributes=NULL;IMFActivate **devices=NULL;UINT32 count=0;
 HRESULT hr=MFCreateAttributes(&attributes,1);if(SUCCEEDED(hr))hr=IMFAttributes_SetGUID(attributes,&MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,&MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);if(SUCCEEDED(hr))hr=MFEnumDeviceSources(attributes,&devices,&count);if(attributes)IMFAttributes_Release(attributes);
 unsigned found=0;bool ok=app->camera_status==1;
 for(UINT32 i=0;i<count;i++){
  wchar_t *name=NULL;UINT32 length=0;IMFActivate_GetAllocatedString(devices[i],&MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,&name,&length);
  if(name&&!wcsncmp(name,L"MultiCamera T.E.",wcslen(L"MultiCamera T.E."))){
   found++;char utf8[512];WideCharToMultiByte(CP_UTF8,0,name,-1,utf8,sizeof(utf8),NULL,NULL);printf("CAMERA_ENUM %s\n",utf8);fflush(stdout);
   if(argc<2||existing){IMFMediaSource *source=NULL;IMFSourceReader *reader=NULL;hr=IMFActivate_ActivateObject(devices[i],&IID_IMFMediaSource,(void**)&source);printf("ACTIVATE=%08lx\n",(unsigned long)hr);fflush(stdout);if(SUCCEEDED(hr))hr=MFCreateSourceReaderFromMediaSource(source,NULL,&reader);
    unsigned samples=0,events=0;if(SUCCEEDED(hr))for(;samples<10&&events<100;events++){IMFSample *sample=NULL;DWORD flags=0;LONGLONG timestamp=0;hr=IMFSourceReader_ReadSample(reader,MF_SOURCE_READER_FIRST_VIDEO_STREAM,0,NULL,&flags,&timestamp,&sample);if(SUCCEEDED(hr)&&!sample&&!(flags&(MF_SOURCE_READERF_ERROR|MF_SOURCE_READERF_ENDOFSTREAM)))continue;if(FAILED(hr)||!sample){ok=false;printf("CAPTURE_FAIL=%08lx flags=%lu\n",(unsigned long)hr,flags);break;}if(argc>1&&!strcmp(argv[1],"--existing-live")){IMFMediaBuffer *pixels_buffer=NULL;BYTE *pixels=NULL;DWORD pixel_bytes=0;HRESULT image=IMFSample_ConvertToContiguousBuffer(sample,&pixels_buffer);if(SUCCEEDED(image))image=IMFMediaBuffer_Lock(pixels_buffer,&pixels,NULL,&pixel_bytes);unsigned long long luma=0;if(SUCCEEDED(image)){for(unsigned x=0;x<1280*720&&x<pixel_bytes;x+=16)luma+=pixels[x];IMFMediaBuffer_Unlock(pixels_buffer);}if(pixels_buffer)IMFMediaBuffer_Release(pixels_buffer);if(FAILED(image)||luma/(1280*720/16)<=20){puts("WINDOWS_LIVE_FRAME_MISSING");ok=false;}}samples++;DWORD bytes=0;IMFSample_GetTotalLength(sample,&bytes);IMFSample_Release(sample);if(bytes!=1280*720*3/2){ok=false;printf("CAPTURE_SIZE=%lu\n",bytes);}}
    else{ok=false;printf("CAMERA_OPEN_FAIL=%08lx\n",(unsigned long)hr);}
    if(samples<10)ok=false;printf("CAPTURE_SAMPLES=%u\n",samples);if(reader)IMFSourceReader_Release(reader);if(source){IMFMediaSource_Shutdown(source);IMFMediaSource_Release(source);}IMFActivate_ShutdownObject(devices[i]);printf("CAMERA_CAPTURE_DONE\n");fflush(stdout);
   }
  }
  CoTaskMemFree(name);IMFActivate_Release(devices[i]);
 }
 CoTaskMemFree(devices);printf("WINDOWS_CAMERA_COUNT=%u\n",found);InterlockedExchange(&app->closing,1);mc_cameras_stop(app);free(app);MFShutdown();CoUninitialize();return ok&&found==5?0:3;
}
