/* MultiCamera T.E. suite setup. Staged files, explicit elevation, optional OBS. GPL-2.0-or-later. */
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <windows.h>
#include <sddl.h>
#include <initguid.h>
#include <netfw.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#include <shlobj.h>
#include <knownfolders.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <commctrl.h>
#include <bcrypt.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "app.h"
#include "usb.h"
#include "protocol.h"
#include "i18n.h"
#include "suite-payload.h"
#ifdef MC_SETUP_TEST
#include "../tests/setup-test-hooks.h"
#endif
unsigned mc_language;
static unsigned components=3; /* bit 0: app, bit 1: OBS plugin */
static bool exists(const wchar_t *path){return GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES;}
static bool directory(const wchar_t *path){int result=SHCreateDirectoryExW(NULL,path,NULL);return result==ERROR_SUCCESS||result==ERROR_ALREADY_EXISTS||result==ERROR_FILE_EXISTS;}
static bool reparse(const wchar_t *path){DWORD flags=GetFileAttributesW(path);return flags!=INVALID_FILE_ATTRIBUTES&&(flags&FILE_ATTRIBUTE_REPARSE_POINT);}
static bool sha256(const uint8_t *bytes,size_t count,const uint8_t expected[32]){BCRYPT_ALG_HANDLE alg=NULL;BCRYPT_HASH_HANDLE hash=NULL;uint8_t actual[32];bool ok=BCryptOpenAlgorithmProvider(&alg,BCRYPT_SHA256_ALGORITHM,NULL,0)>=0&&BCryptCreateHash(alg,&hash,NULL,0,NULL,0,0)>=0&&BCryptHashData(hash,(PUCHAR)bytes,(ULONG)count,0)>=0&&BCryptFinishHash(hash,actual,32,0)>=0&&!memcmp(actual,expected,32);if(hash)BCryptDestroyHash(hash);if(alg)BCryptCloseAlgorithmProvider(alg,0);return ok;}
static bool extract(const wchar_t *root,int group){for(size_t i=0;i<SUITE_COUNT;i++){const struct suite_file *file=&suite_files[i];if(group>=0&&file->group!=group)continue;if(group<0&&!(components&(1u<<file->group)))continue;if(!sha256(file->bytes,file->size,file->hash))return false;wchar_t path[2048],parent[2048];if(group<0)swprintf(path,2048,L"%ls\\%ls\\%ls",root,file->group?L"obs-plugin":L"app",file->name);else swprintf(path,2048,L"%ls\\%ls",root,file->name);wcscpy(parent,path);wchar_t *last=wcsrchr(parent,L'\\');if(last)*last=0;if(!directory(parent))return false;HANDLE output=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);if(output==INVALID_HANDLE_VALUE)return false;DWORD written=0;bool ok=WriteFile(output,file->bytes,(DWORD)file->size,&written,NULL)&&written==file->size;FlushFileBuffers(output);CloseHandle(output);if(!ok)return false;}return true;}
static bool running(void){bool found=false;HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);if(snapshot==INVALID_HANDLE_VALUE)return true;PROCESSENTRY32W process={.dwSize=sizeof(process)};if(Process32FirstW(snapshot,&process))do{if(((components&1)&&!_wcsicmp(process.szExeFile,L"MultiCameraTE.exe"))||((components&2)&&!_wcsicmp(process.szExeFile,L"obs64.exe")))found=true;}while(Process32NextW(snapshot,&process));CloseHandle(snapshot);return found;}
static bool administrator(void){BOOL yes=FALSE;PSID sid=NULL;SID_IDENTIFIER_AUTHORITY authority=SECURITY_NT_AUTHORITY;if(AllocateAndInitializeSid(&authority,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&sid)){CheckTokenMembership(NULL,sid,&yes);FreeSid(sid);}return yes!=FALSE;}
static bool firewall_rule(const wchar_t *exe,const wchar_t *rule_name,const wchar_t *port,const wchar_t *local,const wchar_t *remote,const wchar_t *kind,const wchar_t *alias){
 INetFwPolicy2 *policy=NULL;INetFwRules *rules=NULL;INetFwRule *rule=NULL;HRESULT hr=CoCreateInstance(&CLSID_NetFwPolicy2,NULL,CLSCTX_INPROC_SERVER,&IID_INetFwPolicy2,(void**)&policy);if(SUCCEEDED(hr))hr=INetFwPolicy2_get_Rules(policy,&rules);if(SUCCEEDED(hr))hr=CoCreateInstance(&CLSID_NetFwRule,NULL,CLSCTX_INPROC_SERVER,&IID_INetFwRule,(void**)&rule);
 BSTR name=SysAllocString(rule_name),program=SysAllocString(exe),ports=SysAllocString(port),addresses=SysAllocString(remote),local_address=SysAllocString(local),interface_type=SysAllocString(kind);
 if(SUCCEEDED(hr))hr=INetFwRule_put_Name(rule,name);if(SUCCEEDED(hr))hr=INetFwRule_put_ApplicationName(rule,program);if(SUCCEEDED(hr))hr=INetFwRule_put_Protocol(rule,NET_FW_IP_PROTOCOL_TCP);if(SUCCEEDED(hr))hr=INetFwRule_put_LocalPorts(rule,ports);if(SUCCEEDED(hr))hr=INetFwRule_put_LocalAddresses(rule,local_address);if(SUCCEEDED(hr))hr=INetFwRule_put_RemoteAddresses(rule,addresses);if(SUCCEEDED(hr))hr=INetFwRule_put_InterfaceTypes(rule,interface_type);if(SUCCEEDED(hr)&&alias){VARIANT interfaces,item;VariantInit(&interfaces);VariantInit(&item);interfaces.vt=VT_ARRAY|VT_VARIANT;interfaces.parray=SafeArrayCreateVector(VT_VARIANT,0,1);item.vt=VT_BSTR;item.bstrVal=SysAllocString(alias);LONG index=0;if(!interfaces.parray||!item.bstrVal)hr=E_OUTOFMEMORY;else hr=SafeArrayPutElement(interfaces.parray,&index,&item);if(SUCCEEDED(hr))hr=INetFwRule_put_Interfaces(rule,interfaces);VariantClear(&item);VariantClear(&interfaces);}if(SUCCEEDED(hr))hr=INetFwRule_put_Direction(rule,NET_FW_RULE_DIR_IN);if(SUCCEEDED(hr))hr=INetFwRule_put_Profiles(rule,NET_FW_PROFILE2_ALL);if(SUCCEEDED(hr))hr=INetFwRule_put_Action(rule,NET_FW_ACTION_ALLOW);if(SUCCEEDED(hr))hr=INetFwRule_put_EdgeTraversal(rule,VARIANT_FALSE);if(SUCCEEDED(hr))hr=INetFwRule_put_Enabled(rule,VARIANT_TRUE);if(SUCCEEDED(hr)){INetFwRules_Remove(rules,name);hr=INetFwRules_Add(rules,rule);}SysFreeString(name);SysFreeString(program);SysFreeString(ports);SysFreeString(addresses);SysFreeString(local_address);SysFreeString(interface_type);if(rule)INetFwRule_Release(rule);if(rules)INetFwRules_Release(rules);if(policy)INetFwPolicy2_Release(policy);return SUCCEEDED(hr);
}
static bool firewall(const wchar_t *exe,const wchar_t *rule,const wchar_t *port){
#ifdef MC_SETUP_TEST
 return setup_test_firewall(exe,rule,port);
#endif
 IP_ADAPTER_ADDRESSES *buffer=NULL;ULONG size=16384;buffer=malloc(size);ULONG result=GetAdaptersAddresses(AF_INET,GAA_FLAG_INCLUDE_GATEWAYS,NULL,buffer,&size);if(result==ERROR_BUFFER_OVERFLOW){free(buffer);buffer=malloc(size);result=GetAdaptersAddresses(AF_INET,GAA_FLAG_INCLUDE_GATEWAYS,NULL,buffer,&size);}wchar_t local[64]={0},remote[80]={0},kind[32]={0};
 if(result==NO_ERROR)for(IP_ADAPTER_ADDRESSES *adapter=buffer;adapter;adapter=adapter->Next){if(mc_usb_description(adapter->Description)||adapter->OperStatus!=IfOperStatusUp||(adapter->IfType!=IF_TYPE_ETHERNET_CSMACD&&adapter->IfType!=IF_TYPE_IEEE80211)||!adapter->FirstGatewayAddress)continue;for(IP_ADAPTER_UNICAST_ADDRESS *entry=adapter->FirstUnicastAddress;entry;entry=entry->Next){if(entry->Address.lpSockaddr->sa_family!=AF_INET)continue;uint32_t address=((struct sockaddr_in*)entry->Address.lpSockaddr)->sin_addr.s_addr,host=ntohl(address);if(!((host>>24)==10||(host>>16)==0xc0a8||(host>>20)==0xac1))continue;unsigned prefix=entry->OnLinkPrefixLength;if(prefix<8||prefix>32)continue;uint32_t mask=prefix?0xffffffffu<<(32-prefix):0;struct in_addr network={.s_addr=htonl(host&mask)};wchar_t subnet[64];InetNtopW(AF_INET,&address,local,64);InetNtopW(AF_INET,&network,subnet,64);swprintf(remote,80,L"%ls/%u",subnet,prefix);wcscpy(kind,adapter->IfType==IF_TYPE_IEEE80211?L"Wireless":L"LAN");goto selected;}}
selected:free(buffer);if(!*local||!*remote)return false;
 return firewall_rule(exe,rule,port,local,remote,kind,NULL);
}
static void shortcut(const wchar_t *exe,const wchar_t *working){
#ifdef MC_SETUP_TEST
 setup_test_shortcuts++;return;
#endif
PWSTR menu=NULL;if(FAILED(SHGetKnownFolderPath(&FOLDERID_CommonPrograms,0,NULL,&menu)))return;IShellLinkW *link=NULL;if(SUCCEEDED(CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link))){IShellLinkW_SetPath(link,exe);IShellLinkW_SetWorkingDirectory(link,working);IShellLinkW_SetDescription(link,L"MultiCamera T.E.");IPersistFile *file=NULL;if(SUCCEEDED(IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&file))){wchar_t path[2048];swprintf(path,2048,L"%ls\\MultiCamera T.E.lnk",menu);IPersistFile_Save(file,path,TRUE);IPersistFile_Release(file);}IShellLinkW_Release(link);}CoTaskMemFree(menu);}
static bool install(const wchar_t *pipe_base){
 PWSTR files=NULL,data=NULL;
 if(FAILED(SHGetKnownFolderPath(&FOLDERID_ProgramFiles,0,NULL,&files)))return false;
 if(FAILED(SHGetKnownFolderPath(&FOLDERID_ProgramData,0,NULL,&data))){CoTaskMemFree(files);return false;}
 uint32_t nonce=0;bool random=BCryptGenRandom(NULL,(PUCHAR)&nonce,4,BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0;
 wchar_t target[2048],stage[2048],backup[2048],plugin[2048],plugin_stage[2048],plugin_backup[2048],obs[2048],parent[2048];
 swprintf(target,2048,L"%ls\\MultiCamera T.E",files);swprintf(stage,2048,L"%ls\\.MultiCameraTE-stage-%08x",files,nonce);swprintf(backup,2048,L"%ls\\MultiCamera T.E. Backups\\%08x",files,nonce);
 swprintf(plugin,2048,L"%ls\\obs-studio\\plugins\\obs-lan-camera",data);swprintf(plugin_stage,2048,L"%ls\\obs-studio\\plugins\\.multicamera-stage-%08x",data,nonce);swprintf(plugin_backup,2048,L"%ls\\obs-studio\\plugin-backups\\multicamera-%08x",data,nonce);
 swprintf(obs,2048,L"%ls\\obs-studio\\bin\\64bit\\obs64.exe",files);CoTaskMemFree(files);CoTaskMemFree(data);if(!random)return false;
 bool app=(components&1)!=0,with_obs=(components&2)!=0,old=false,old_plugin=false,app_moved=false,plugin_moved=false;
 if((app&&reparse(target))||(with_obs&&reparse(plugin)))return false;
 if(app&&(!CreateDirectoryW(stage,NULL)||!extract(stage,0)))return false;
 if(with_obs){wcscpy(parent,plugin_stage);*wcsrchr(parent,L'\\')=0;if(!directory(parent)||!CreateDirectoryW(plugin_stage,NULL)||!extract(plugin_stage,1))return false;}
 if(app){old=exists(target);if(old){wcscpy(parent,backup);*wcsrchr(parent,L'\\')=0;if(!directory(parent)||!MoveFileExW(target,backup,MOVEFILE_WRITE_THROUGH))return false;}
  if(!MoveFileExW(stage,target,MOVEFILE_WRITE_THROUGH)){if(old)MoveFileExW(backup,target,MOVEFILE_WRITE_THROUGH);return false;}app_moved=true;
 }
 if(with_obs){old_plugin=exists(plugin);if(old_plugin){wcscpy(parent,plugin_backup);*wcsrchr(parent,L'\\')=0;if(!directory(parent)||!MoveFileExW(plugin,plugin_backup,MOVEFILE_WRITE_THROUGH))goto rollback;}
  if(!MoveFileExW(plugin_stage,plugin,MOVEFILE_WRITE_THROUGH)){if(old_plugin)MoveFileExW(plugin_backup,plugin,MOVEFILE_WRITE_THROUGH);goto rollback;}plugin_moved=true;
 }
 if(app){wchar_t dll[2048],exe[2048],self[2048],setup[2048];swprintf(dll,2048,L"%ls\\MultiCameraTE.Source.dll",target);swprintf(exe,2048,L"%ls\\MultiCameraTE.exe",target);
  if(!mc_register_source(dll,pipe_base))goto rollback;
  HKEY key;if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\MultiCameraTE",0,KEY_SET_VALUE|KEY_WOW64_64KEY,&key)==ERROR_SUCCESS){RegSetValueExW(key,L"InstallDir",0,REG_SZ,(const BYTE*)target,(DWORD)((wcslen(target)+1)*2));RegCloseKey(key);}
  if(!firewall(exe,L"MultiCameraTE-LAN-34872",L"34872"))MessageBoxW(NULL,mc_text(MC_TEXT_FIREWALL_WARNING),MC_TITLE,MB_OK|MB_ICONWARNING);
  shortcut(exe,target);GetModuleFileNameW(NULL,self,2048);swprintf(setup,2048,L"%ls\\MultiCameraTE-Setup.exe",target);if(_wcsicmp(self,setup))CopyFileW(self,setup,FALSE);
 }
 if(with_obs){if(exists(obs)){if(!firewall(obs,L"MultiCameraTE-OBS-34871",L"34871"))MessageBoxW(NULL,mc_text(MC_TEXT_OBS_FIREWALL_WARNING),MC_TITLE,MB_OK|MB_ICONWARNING);}else MessageBoxW(NULL,mc_text(MC_TEXT_OBS_NOT_FOUND),MC_TITLE,MB_OK|MB_ICONINFORMATION);}
 return true;
rollback:
 if(plugin_moved){MoveFileExW(plugin,plugin_stage,MOVEFILE_WRITE_THROUGH);if(old_plugin)MoveFileExW(plugin_backup,plugin,MOVEFILE_WRITE_THROUGH);}
 if(app_moved){MoveFileExW(target,stage,MOVEFILE_WRITE_THROUGH);if(old)MoveFileExW(backup,target,MOVEFILE_WRITE_THROUGH);}return false;
}
#include "setup-wizard.h"
static unsigned parse_components(const wchar_t *value){return value&&value[0]>=L'1'&&value[0]<=L'3'&&!value[1]?(unsigned)(value[0]-L'0'):0;}
static int usb_setup(int argc,wchar_t **argv){
 bool child=!wcscmp(argv[1],L"--usb-elevated");if(argc==5){unsigned selected=(unsigned)_wtoi(argv[4]);if(selected<5)mc_language=selected;}
 wchar_t *last=NULL;unsigned long selected=wcstoul(argv[2],&last,10);if(!selected||!last||*last)return 7;struct mc_usb_adapter choices[32],*adapter=NULL;int count=mc_usb_adapters(choices,32);for(int i=0;i<count;i++){wchar_t address[64];MultiByteToWideChar(CP_UTF8,0,choices[i].address,-1,address,64);if(choices[i].index==selected&&!wcscmp(address,argv[3])){adapter=&choices[i];break;}}
 if(!adapter){MessageBoxW(NULL,mc_text(MC_TEXT_USB_MISSING),MC_TITLE,MB_OK|MB_ICONWARNING);return 7;}
 wchar_t folder[1024],exe[1200];DWORD length=sizeof(folder);if(RegGetValueW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\MultiCameraTE",L"InstallDir",RRF_RT_REG_SZ,NULL,folder,&length)!=ERROR_SUCCESS)return 7;swprintf(exe,1200,L"%ls\\MultiCameraTE.exe",folder);if(!exists(exe))return 7;
 if(!child){wchar_t question[1800];swprintf(question,1800,L"%ls\n\n%ls\n%ls · %ls · TCP 34872",mc_text(MC_TEXT_USB_QUESTION),adapter->name,argv[3],adapter->subnet);if(MessageBoxW(NULL,question,MC_TITLE,MB_YESNO|MB_ICONINFORMATION)!=IDYES)return 0;}
 if(!administrator()){
  if(child)return 7;wchar_t self[2048],arguments[512];GetModuleFileNameW(NULL,self,2048);swprintf(arguments,512,L"--usb-elevated %lu %ls %u",selected,argv[3],mc_language);SHELLEXECUTEINFOW execution={.cbSize=sizeof(execution),.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC,.lpVerb=L"runas",.lpFile=self,.lpParameters=arguments,.nShow=SW_SHOWNORMAL};if(!ShellExecuteExW(&execution))return 4;WaitForSingleObject(execution.hProcess,INFINITE);DWORD code=0;GetExitCodeProcess(execution.hProcess,&code);CloseHandle(execution.hProcess);MessageBoxW(NULL,mc_text(code==42?MC_TEXT_USB_ALLOWED:MC_TEXT_USB_FAILED),MC_TITLE,MB_OK|(code==42?MB_ICONINFORMATION:MB_ICONWARNING));return code==42?0:7;
 }
 wchar_t rule_name[96];swprintf(rule_name,96,L"MultiCameraTE-USB-%lu",selected);bool ok=firewall_rule(exe,rule_name,L"34872",argv[3],adapter->subnet,L"LAN",adapter->name);if(!child)MessageBoxW(NULL,mc_text(ok?MC_TEXT_USB_ALLOWED:MC_TEXT_USB_FAILED),MC_TITLE,MB_OK|(ok?MB_ICONINFORMATION:MB_ICONWARNING));return ok?42:7;
}
int WINAPI wWinMain(HINSTANCE module,HINSTANCE previous,LPWSTR command,int show){
 int argc=0;wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);WORD language=PRIMARYLANGID(GetUserDefaultUILanguage());mc_language=language==LANG_RUSSIAN?1:language==LANG_JAPANESE?2:language==LANG_SPANISH?3:language==LANG_CHINESE?4:0;
 if((argc==3||argc==4)&&!wcscmp(argv[1],L"--extract-to")){components=argc==4?parse_components(argv[3]):3;bool ok=components&&!exists(argv[2])&&extract(argv[2],-1);LocalFree(argv);return ok?0:2;}
 if(((argc==4||argc==5)&&!wcscmp(argv[1],L"--usb-firewall"))||(argc==5&&!wcscmp(argv[1],L"--usb-elevated"))){CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);int code=usb_setup(argc,argv);CoUninitialize();LocalFree(argv);return code;}
 wchar_t base[192]={0};bool elevated_child=argc==5&&!wcscmp(argv[1],L"--elevated");
 if(elevated_child){wcsncpy(base,argv[2],191);unsigned selected=(unsigned)_wtoi(argv[3]);if(selected<5)mc_language=selected;components=parse_components(argv[4]);if(!components){LocalFree(argv);return 6;}}
 else {if(argc!=1){LocalFree(argv);return 6;}if(!mc_user_pipe(base,192)){LocalFree(argv);return 3;}}
 LocalFree(argv);CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);INITCOMMONCONTROLSEX controls={sizeof(controls),ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
 if(!elevated_child){components=choose_components();if(!components){CoUninitialize();return 0;}}
 while(running())if(MessageBoxW(NULL,mc_text(components==1?MC_TEXT_CLOSE_APP_ONLY:components==2?MC_TEXT_CLOSE_OBS_ONLY:MC_TEXT_CLOSE_APPS),MC_TITLE,MB_RETRYCANCEL|MB_ICONINFORMATION)!=IDRETRY){CoUninitialize();return 0;}
 if(!administrator()){
  wchar_t exe[2048],arguments[512];GetModuleFileNameW(NULL,exe,2048);swprintf(arguments,512,L"--elevated \"%ls\" %u %u",base,mc_language,components);
  SHELLEXECUTEINFOW execution={.cbSize=sizeof(execution),.fMask=SEE_MASK_NOCLOSEPROCESS|SEE_MASK_NOASYNC,.lpVerb=L"runas",.lpFile=exe,.lpParameters=arguments,.nShow=SW_SHOWNORMAL};if(!ShellExecuteExW(&execution)){CoUninitialize();return 4;}
  WaitForSingleObject(execution.hProcess,INFINITE);DWORD code=0;GetExitCodeProcess(execution.hProcess,&code);CloseHandle(execution.hProcess);
  if(code==42){if(components&1){if(MessageBoxW(NULL,mc_text(MC_TEXT_INSTALL_COMPLETE),MC_TITLE,MB_YESNO|MB_ICONINFORMATION)==IDYES){PWSTR files=NULL;if(SUCCEEDED(SHGetKnownFolderPath(&FOLDERID_ProgramFiles,0,NULL,&files))){wchar_t program[2048],working[2048];swprintf(working,2048,L"%ls\\MultiCamera T.E",files);swprintf(program,2048,L"%ls\\MultiCameraTE.exe",working);CoTaskMemFree(files);ShellExecuteW(NULL,L"open",program,NULL,working,SW_SHOWNORMAL);}}}else MessageBoxW(NULL,mc_text(MC_TEXT_OBS_COMPLETE),MC_TITLE,MB_OK|MB_ICONINFORMATION);}
  CoUninitialize();return code==42?0:5;
 }
 if(wcsncmp(base,L"\\\\.\\pipe\\MultiCameraTE.S-1-",wcslen(L"\\\\.\\pipe\\MultiCameraTE.S-1-"))){MessageBoxW(NULL,mc_text(MC_TEXT_INSTALL_ERROR),MC_TITLE,MB_OK|MB_ICONERROR);CoUninitialize();return 6;}
 bool ok=install(base);if(!ok)MessageBoxW(NULL,mc_text(MC_TEXT_INSTALL_ERROR),MC_TITLE,MB_OK|MB_ICONERROR);CoUninitialize();return ok?42:7;
}
