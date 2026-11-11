/* GitHub Releases updater. Metadata checks never download an executable. GPL-2.0-or-later. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#include <initguid.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <shellapi.h>
#include <obs-module.h>
#include <util/platform.h>
#include <stdio.h>
#include "release.h"
#include "updater.h"

static CRITICAL_SECTION lock;
static HANDLE worker;
static volatile LONG stopping;
struct request { HINTERNET session,connection,request; };
static HWND obs_window(void)
{
    HMODULE dll=GetModuleHandleW(L"obs-frontend-api.dll");if(!dll)return NULL;
    union { FARPROC raw; void *(*get)(void); } symbol;
    symbol.raw=GetProcAddress(dll,"obs_frontend_get_main_window_handle");return symbol.get?(HWND)symbol.get():NULL;
}
static wchar_t *wide(const char *s)
{
    int n=MultiByteToWideChar(CP_UTF8,0,s,-1,NULL,0);wchar_t *w=bzalloc((size_t)n*sizeof(wchar_t));MultiByteToWideChar(CP_UTF8,0,s,-1,w,n);return w;
}
static int prompt(const char *text,UINT flags)
{
    if(stopping)return IDNO;wchar_t *w=wide(text);int answer=MessageBoxW(obs_window(),w,L"OBS LAN Camera — обновления",flags|MB_SETFOREGROUND);bfree(w);return answer;
}
static void close_request(struct request *r)
{
    if(r->request)WinHttpCloseHandle(r->request);if(r->connection)WinHttpCloseHandle(r->connection);if(r->session)WinHttpCloseHandle(r->session);memset(r,0,sizeof(*r));
}
static bool open_request(struct request *r,const wchar_t *url)
{
    memset(r,0,sizeof(*r));URL_COMPONENTS u={.dwStructSize=sizeof(u)};
    wchar_t host[256],path[2000];u.lpszHostName=host;u.dwHostNameLength=255;u.lpszUrlPath=path;u.dwUrlPathLength=1999;
    if(!WinHttpCrackUrl(url,0,0,&u)||u.nScheme!=INTERNET_SCHEME_HTTPS)return false;
    host[u.dwHostNameLength]=0;path[u.dwUrlPathLength]=0;
    r->session=WinHttpOpen(L"OBS-LAN-Camera/" L"0.3.0",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!r->session)return false;
    WinHttpSetTimeouts(r->session,4000,4000,4000,4000);
    r->connection=WinHttpConnect(r->session,host,u.nPort,0);if(!r->connection)goto fail;
    r->request=WinHttpOpenRequest(r->connection,L"GET",path,NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE);if(!r->request)goto fail;
    DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP;WinHttpSetOption(r->request,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy));
    if(!WinHttpSendRequest(r->request,L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n",(DWORD)-1,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(r->request,NULL))goto fail;
    DWORD status=0,size=sizeof(status);if(!WinHttpQueryHeaders(r->request,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)goto fail;
    return true;
fail:close_request(r);return false;
}
static char *get_metadata(void)
{
    struct request r;wchar_t *url=wide("https://api.github.com/repos/" OLC_REPOSITORY "/releases/latest");bool ok=open_request(&r,url);bfree(url);if(!ok)return NULL;
    size_t used=0,capacity=65536;char *data=bmalloc(capacity);DWORD got;
    while(!stopping){if(capacity-used<16385){if(capacity>=1024*1024)goto fail;capacity*=2;data=brealloc(data,capacity);}
        if(!WinHttpReadData(r.request,data+used,16384,&got))goto fail;if(!got)break;used+=got;}
    data[used]=0;close_request(&r);if(stopping){bfree(data);return NULL;}return data;
fail:close_request(&r);bfree(data);return NULL;
}
static bool download(const char *url,const char *digest,uint64_t expected,wchar_t result[1024])
{
    struct request r={0};BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;HANDLE file=INVALID_HANDLE_VALUE;PWSTR downloads=NULL;
    wchar_t part[1024]={0},final[1024]={0};bool success=false,created=false;
    if(!olc_sha256(digest)||!expected||expected>200*1024*1024)return false;
    if(FAILED(SHGetKnownFolderPath(&FOLDERID_Downloads,0,NULL,&downloads)))return false;
    wchar_t directory[1024];if(swprintf(directory,1024,L"%ls\\OBS LAN Camera",downloads)<0)goto done;
    if(!CreateDirectoryW(directory,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)goto done;
    uint8_t random[4];if(BCryptGenRandom(NULL,random,4,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)goto done;
    unsigned suffix;memcpy(&suffix,random,4);
    swprintf(final,1024,L"%ls\\OBS-LAN-Camera-Setup-%08x.exe",directory,suffix);swprintf(part,1024,L"%ls.part",final);
    file=CreateFileW(part,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(file==INVALID_HANDLE_VALUE)goto done;
    created=true;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)<0||BCryptCreateHash(alg,&hash,NULL,0,NULL,0,0)<0)goto done;
    wchar_t *wurl=wide(url);bool opened=open_request(&r,wurl);bfree(wurl);if(!opened)goto done;
    uint8_t chunk[65536],actual[32];uint64_t total=0;DWORD got,written;
    while(!stopping){if(!WinHttpReadData(r.request,chunk,sizeof(chunk),&got))goto done;if(!got)break;
        total+=got;if(total>expected||!WriteFile(file,chunk,got,&written,NULL)||written!=got||BCryptHashData(hash,chunk,got,0)<0)goto done;}
    if(stopping||total!=expected||BCryptFinishHash(hash,actual,32,0)<0)goto done;
    for(int i=0;i<32;i++){unsigned v;if(sscanf(digest+7+2*i,"%2x",&v)!=1||actual[i]!=(uint8_t)v)goto done;}
    FlushFileBuffers(file);CloseHandle(file);file=INVALID_HANDLE_VALUE;
    if(!MoveFileW(part,final))goto done;wcscpy(result,final);success=true;
done:
    close_request(&r);if(file!=INVALID_HANDLE_VALUE)CloseHandle(file);if(!success&&created)DeleteFileW(part);
    if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);if(downloads)CoTaskMemFree(downloads);return success;
}
static DWORD WINAPI check_thread(void *value)
{
    bool automatic=(bool)(uintptr_t)value;char *json=get_metadata();if(!json){if(!automatic)prompt("Не удалось проверить обновления на GitHub. Проверьте подключение к интернету и попробуйте позже.",MB_OK|MB_ICONINFORMATION);return 0;}
    obs_data_t *release=obs_data_create_from_json(json);bfree(json);if(!release)return 0;
    const char *tag=obs_data_get_string(release,"tag_name");
    if(obs_data_get_bool(release,"draft")||obs_data_get_bool(release,"prerelease")||!olc_newer(tag,OLC_VERSION)){
        if(!automatic)prompt("Установлена актуальная версия OBS LAN Camera " OLC_VERSION ".",MB_OK|MB_ICONINFORMATION);goto done;}
    char url[600]={0},digest[80]={0};uint64_t bytes=0;
    obs_data_array_t *assets=obs_data_get_array(release,"assets");
    for(size_t i=0;assets && i<obs_data_array_count(assets);i++){
        obs_data_t *asset=obs_data_array_item(assets,i);const char *name=obs_data_get_string(asset,"name"),*u=obs_data_get_string(asset,"browser_download_url"),*d=obs_data_get_string(asset,"digest");
        int64_t size=obs_data_get_int(asset,"size");
        if(!strcmp(name,OLC_INSTALLER_NAME)&&olc_asset_url(u,tag)&&olc_sha256(d)&&size>0&&size<=200*1024*1024){snprintf(url,sizeof(url),"%s",u);snprintf(digest,sizeof(digest),"%s",d);bytes=(uint64_t)size;}
        obs_data_release(asset);
    }
    obs_data_array_release(assets);
    if(!*url){if(!automatic)prompt("На GitHub есть новая версия, но проверенный установщик пока недоступен. Посмотрите страницу Releases.",MB_OK|MB_ICONINFORMATION);goto done;}
    const char *notes=obs_data_get_string(release,"body");char text[6500];
    snprintf(text,sizeof(text),"Доступна OBS LAN Camera %s\nУстановлена: " OLC_VERSION "\nРазмер установщика: %.1f МБ\n\nЧто нового:\n%.4800s\n\nСкачать установщик с GitHub?\nОн будет сохранён в «Загрузки / OBS LAN Camera». Установка запускается вами отдельно.",tag,(double)bytes/1048576.0,*notes?notes:"Описание изменений отсутствует.");
    if(prompt(text,MB_YESNO|MB_ICONINFORMATION|MB_DEFBUTTON2)==IDYES&&!stopping){
        wchar_t path[1024];
        if(download(url,digest,bytes,path)){
            if(!stopping){MessageBoxW(obs_window(),L"Установщик скачан, SHA-256 совпадает с GitHub.\n\nЗакройте OBS и запустите установщик из открывшейся папки.",L"OBS LAN Camera",MB_OK|MB_ICONINFORMATION);wchar_t command[1200];swprintf(command,1200,L"/select,\"%ls\"",path);ShellExecuteW(NULL,L"open",L"explorer.exe",command,NULL,SW_SHOWNORMAL);}
        }else prompt("Не удалось скачать или проверить установщик. Файл не будет запущен. Повторите проверку позже.",MB_OK|MB_ICONWARNING);
    }
done:obs_data_release(release);return 0;
}
void olc_updater_init(void) { InitializeCriticalSection(&lock);stopping=0; }
void olc_updater_check(bool automatic)
{
    EnterCriticalSection(&lock);
    if(!stopping){if(worker&&WaitForSingleObject(worker,0)==WAIT_OBJECT_0){CloseHandle(worker);worker=NULL;}if(!worker)worker=CreateThread(NULL,0,check_thread,(void*)(uintptr_t)automatic,0,NULL);}
    LeaveCriticalSection(&lock);
}
void olc_updater_shutdown(void)
{
    InterlockedExchange(&stopping,1);EnterCriticalSection(&lock);HANDLE thread=worker;worker=NULL;LeaveCriticalSection(&lock);
    if(thread){WaitForSingleObject(thread,INFINITE);CloseHandle(thread);}DeleteCriticalSection(&lock);
}
