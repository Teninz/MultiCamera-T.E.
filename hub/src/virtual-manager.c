#include "app.h"
#include <stdio.h>
bool mc_register_source(const wchar_t *dll,const wchar_t *pipe_base){
 if(GetFileAttributesW(dll)==INVALID_FILE_ATTRIBUTES)return false;
 for(int i=0;i<5;i++){wchar_t id[64],path[256];StringFromGUID2(&mc_camera_ids[i],id,64);swprintf(path,256,L"SOFTWARE\\Classes\\CLSID\\%ls\\InprocServer32",id);HKEY key;LONG result=RegCreateKeyExW(HKEY_LOCAL_MACHINE,path,0,NULL,0,KEY_SET_VALUE|KEY_WOW64_64KEY,NULL,&key,NULL);if(result!=ERROR_SUCCESS)return false;result=RegSetValueExW(key,NULL,0,REG_SZ,(const BYTE*)dll,(DWORD)((wcslen(dll)+1)*2));if(result==ERROR_SUCCESS)result=RegSetValueExW(key,L"ThreadingModel",0,REG_SZ,(const BYTE*)L"Both",10);RegCloseKey(key);if(result!=ERROR_SUCCESS)return false;}
 HKEY key;if(RegCreateKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\MultiCameraTE",0,NULL,0,KEY_SET_VALUE|KEY_WOW64_64KEY,NULL,&key,NULL)!=ERROR_SUCCESS)return false;LONG result=RegSetValueExW(key,L"PipeBase",0,REG_SZ,(const BYTE*)pipe_base,(DWORD)((wcslen(pipe_base)+1)*2));RegCloseKey(key);return result==ERROR_SUCCESS;
}
static DWORD WINAPI create_cameras(void *data){
 struct mc_app *app=data;CoInitializeEx(NULL,COINIT_MULTITHREADED);HMODULE module=LoadLibraryExW(L"mfsensorgroup.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);union {FARPROC raw;mc_create_virtual_fn create;} api;api.raw=module?GetProcAddress(module,"MFCreateVirtualCamera"):NULL;mc_create_virtual_fn create=api.create;HRESULT hr=create?S_OK:HRESULT_FROM_WIN32(ERROR_OLD_WIN_VERSION);
 for(int i=0;i<5&&SUCCEEDED(hr)&&!app->closing;i++){wchar_t name[96],id[64];swprintf(name,96,L"MultiCamera T.E. %d",i+1);StringFromGUID2(&mc_camera_ids[i],id,64);hr=create(0,0,0,name,id,NULL,0,&app->cameras[i]);if(SUCCEEDED(hr))hr=app->cameras[i]->lpVtbl->Start(app->cameras[i],NULL);}
 app->camera_error=hr;InterlockedExchange(&app->camera_status,SUCCEEDED(hr)?1:-1);
 /* Keep apartment and objects alive together. The UI may be an STA. */
 WaitForSingleObject(app->camera_stop,INFINITE);
 for(int i=0;i<5;i++)if(app->cameras[i]){app->cameras[i]->lpVtbl->Stop(app->cameras[i]);app->cameras[i]->lpVtbl->Shutdown(app->cameras[i]);app->cameras[i]->lpVtbl->attributes.Release((IMFAttributes*)app->cameras[i]);app->cameras[i]=NULL;}
 if(module)FreeLibrary(module);CoUninitialize();return 0;
}
void mc_cameras_start(struct mc_app *app){InterlockedExchange(&app->camera_status,0);app->camera_stop=CreateEventW(NULL,TRUE,FALSE,NULL);if(app->camera_stop)app->camera_thread=CreateThread(NULL,0,create_cameras,app,0,NULL);if(!app->camera_thread){if(app->camera_stop){CloseHandle(app->camera_stop);app->camera_stop=NULL;}app->camera_error=E_OUTOFMEMORY;InterlockedExchange(&app->camera_status,-1);}}
void mc_cameras_stop(struct mc_app *app){if(app->camera_stop)SetEvent(app->camera_stop);if(app->camera_thread){WaitForSingleObject(app->camera_thread,INFINITE);CloseHandle(app->camera_thread);app->camera_thread=NULL;}if(app->camera_stop){CloseHandle(app->camera_stop);app->camera_stop=NULL;}}
