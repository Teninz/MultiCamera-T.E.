/* OBS LAN Camera installer. GPL-2.0-or-later. Files are staged before replacing the plugin. */
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include <stdio.h>
#include <wchar.h>
#include "payload.h"

static const wchar_t *title=L"OBS LAN Camera 0.2.0 — установка";
static int elevate(void)
{
    wchar_t exe[2048];GetModuleFileNameW(NULL,exe,2048);
    HINSTANCE result=ShellExecuteW(NULL,L"runas",exe,L"--elevated",NULL,SW_SHOWNORMAL);return (INT_PTR)result>32?0:6;
}
static bool exists(const wchar_t *p) { return GetFileAttributesW(p)!=INVALID_FILE_ATTRIBUTES; }
static bool directory(const wchar_t *path)
{
    wchar_t p[2048];if(wcslen(path)>=2048)return false;wcscpy(p,path);
    for(wchar_t *s=p+3;*s;s++)if(*s==L'\\'||*s==L'/'){wchar_t keep=*s;*s=0;if(!CreateDirectoryW(p,NULL)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;*s=keep;}
    return CreateDirectoryW(p,NULL)||GetLastError()==ERROR_ALREADY_EXISTS;
}
static bool digest(const uint8_t *data,size_t len,const uint8_t expected[32])
{
    BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;uint8_t actual[32];bool ok=false;
    if(BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0&&BCryptCreateHash(alg,&hash,NULL,0,NULL,0,0)>=0&&BCryptHashData(hash,(PUCHAR)data,(ULONG)len,0)>=0&&BCryptFinishHash(hash,actual,32,0)>=0)ok=!memcmp(actual,expected,32);
    if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);return ok;
}
static bool extract(const wchar_t *root)
{
    for(size_t i=0;i<PAYLOAD_COUNT;i++){
        const struct payload_file *f=&payload_files[i];if(!digest(f->bytes,f->size,f->sha256))return false;
        wchar_t path[2048],parent[2048];if(swprintf(path,2048,L"%ls\\%ls",root,f->name)<0)return false;wcscpy(parent,path);wchar_t *last=wcsrchr(parent,L'\\');if(last)*last=0;if(!directory(parent))return false;
        HANDLE out=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(out==INVALID_HANDLE_VALUE)return false;DWORD written;bool ok=WriteFile(out,f->bytes,(DWORD)f->size,&written,NULL)&&written==f->size;FlushFileBuffers(out);CloseHandle(out);if(!ok)return false;
    }return true;
}
static bool obs_running(wchar_t root[1024])
{
    bool found=false;HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return false;
    PROCESSENTRY32W process={.dwSize=sizeof(process)};
    if(Process32FirstW(snapshot,&process))do{if(!_wcsicmp(process.szExeFile,L"obs64.exe")){
        found=true;HANDLE p=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,process.th32ProcessID);if(p){DWORD n=1024;wchar_t image[1024];if(QueryFullProcessImageNameW(p,0,image,&n)){for(int i=0;i<3;i++){wchar_t *slash=wcsrchr(image,L'\\');if(slash)*slash=0;}wcscpy(root,image);}CloseHandle(p);}
    }}while(Process32NextW(snapshot,&process));CloseHandle(snapshot);return found;
}
static bool compatible(const wchar_t *root)
{
    const wchar_t *files[]={L"obs64.exe",L"obs.dll",L"avcodec-62.dll",L"avutil-60.dll"};wchar_t path[2048];
    for(int i=0;i<4;i++){swprintf(path,2048,L"%ls\\bin\\64bit\\%ls",root,files[i]);if(!exists(path))return false;}return true;
}
static bool choose_obs(wchar_t root[1024])
{
    BROWSEINFOW browse={0};browse.lpszTitle=L"Выберите папку OBS Studio 33 (в ней должна быть папка bin\\64bit)";browse.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
    PIDLIST_ABSOLUTE id=SHBrowseForFolderW(&browse);if(!id)return false;bool ok=SHGetPathFromIDListW(id,root);CoTaskMemFree(id);return ok&&compatible(root);
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE previous,PWSTR command,int show)
{
    (void)instance;(void)previous;(void)command;(void)show;
    int argc=0;wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    /* Used by the release tests. This path only extracts into an explicitly selected new directory. */
    if(argc==3&&!wcscmp(argv[1],L"--extract-to")){bool ok=!exists(argv[2])&&extract(argv[2]);LocalFree(argv);return ok?0:2;}
    bool elevated=argc==2&&!wcscmp(argv[1],L"--elevated");LocalFree(argv);CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
    wchar_t obs[1024]={0};bool running=obs_running(obs);PWSTR program_files=NULL;
    if(!*obs&&SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_ProgramFiles,0,NULL,&program_files))){swprintf(obs,1024,L"%ls\\obs-studio",program_files);CoTaskMemFree(program_files);}
    if(!compatible(obs)&&!choose_obs(obs)){MessageBoxW(NULL,L"Не найдена совместимая установка OBS. Нужен OBS Studio 33 для Windows x64 с FFmpeg 8 (avcodec-62).",title,MB_OK|MB_ICONWARNING);return 3;}
    wchar_t intro[1800];swprintf(intro,1800,L"Установить OBS LAN Camera 0.2.0?\n\nНайден OBS:\n%ls\n\nПриёмник камеры Android появится в меню источников OBS. Настройки OBS и ключи подключения сохранятся.\n\nПрограмма распространяется по GPL-2.0-or-later. Исходники: github.com/Teninz/obs-lan-camera",obs);
    if(MessageBoxW(NULL,intro,title,MB_YESNO|MB_ICONINFORMATION|MB_DEFBUTTON1)!=IDYES)return 0;
    while(running){if(MessageBoxW(NULL,L"Закройте OBS перед установкой, затем нажмите «Повторить».\nУстановщик не завершает OBS принудительно.",title,MB_RETRYCANCEL|MB_ICONINFORMATION)!=IDRETRY)return 0;running=obs_running(obs);}
    PWSTR program_data=NULL;if(FAILED(SHGetKnownFolderPath(&FOLDERID_ProgramData,0,NULL,&program_data)))return 4;
    wchar_t parent[2048],target[2048],stage[2048],backup_parent[2048],backup[2048];
    swprintf(parent,2048,L"%ls\\obs-studio\\plugins",program_data);swprintf(target,2048,L"%ls\\obs-lan-camera",parent);
    unsigned nonce=GetCurrentProcessId()^(unsigned)GetTickCount();swprintf(stage,2048,L"%ls\\.obs-lan-camera-stage-%08x",parent,nonce);
    swprintf(backup_parent,2048,L"%ls\\obs-studio\\plugin-backups",program_data);swprintf(backup,2048,L"%ls\\obs-lan-camera-%08x",backup_parent,nonce);CoTaskMemFree(program_data);
    if(exists(target)&&(GetFileAttributesW(target)&FILE_ATTRIBUTE_REPARSE_POINT)){MessageBoxW(NULL,L"Папка плагина является ссылкой. Установка остановлена; проверьте путь вручную.",title,MB_OK|MB_ICONWARNING);return 5;}
    if(exists(target)&&!elevated){HANDLE access=CreateFileW(target,DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);if(access==INVALID_HANDLE_VALUE&&GetLastError()==ERROR_ACCESS_DENIED)return elevate();if(access!=INVALID_HANDLE_VALUE)CloseHandle(access);}
    if(!directory(parent)||!CreateDirectoryW(stage,NULL)){
        if(!elevated&&(GetLastError()==ERROR_ACCESS_DENIED||GetLastError()==ERROR_PRIVILEGE_NOT_HELD)){
            return elevate();
        }MessageBoxW(NULL,L"Не удалось создать папку установки. Проверьте права доступа и свободное место.",title,MB_OK|MB_ICONERROR);return 6;
    }
    if(!extract(stage)){MessageBoxW(NULL,L"Не удалось распаковать или проверить файлы. Исходная установка плагина не изменена.",title,MB_OK|MB_ICONERROR);return 7;}
    bool old=exists(target);
    if(old&&(!directory(backup_parent)||!MoveFileExW(target,backup,MOVEFILE_WRITE_THROUGH))){MessageBoxW(NULL,L"Не удалось создать резервную копию плагина. Убедитесь, что OBS закрыт. Исходные файлы не заменены.",title,MB_OK|MB_ICONERROR);return 8;}
    if(!MoveFileExW(stage,target,MOVEFILE_WRITE_THROUGH)){
        if(old)MoveFileExW(backup,target,MOVEFILE_WRITE_THROUGH);MessageBoxW(NULL,L"Не удалось завершить установку. Выполнена попытка вернуть предыдущую папку плагина.",title,MB_OK|MB_ICONERROR);return 9;
    }
    if(MessageBoxW(NULL,L"OBS LAN Camera установлен.\n\nВ OBS: Источники → + → «Камера смартфона по Wi-Fi». В свойствах источника будет QR скачивания Android-приложения.\n\nЗапустить OBS сейчас?",title,MB_YESNO|MB_ICONINFORMATION)==IDYES){wchar_t exe[2048],working[2048];swprintf(exe,2048,L"%ls\\bin\\64bit\\obs64.exe",obs);swprintf(working,2048,L"%ls\\bin\\64bit",obs);ShellExecuteW(NULL,L"open",exe,NULL,working,SW_SHOWNORMAL);}return 0;
}
