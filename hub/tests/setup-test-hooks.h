/* Compile-time hooks only. Production exe has no path/registry redirection. */
static wchar_t setup_test_root[1024];
static int setup_test_registers,setup_test_shortcuts,setup_test_app_rules,setup_test_obs_rules;
static bool setup_test_fail_register;
static HRESULT setup_test_folder(REFKNOWNFOLDERID id,DWORD flags,HANDLE token,PWSTR *out){
 const wchar_t *name=IsEqualGUID(id,&FOLDERID_ProgramFiles)?L"pf":L"pd";
 *out=CoTaskMemAlloc(2048*sizeof(wchar_t));if(!*out)return E_OUTOFMEMORY;swprintf(*out,2048,L"%ls/%ls",setup_test_root,name);SHCreateDirectoryExW(NULL,*out,NULL);return S_OK;
}
static bool setup_test_register(const wchar_t *dll,const wchar_t *base){setup_test_registers++;return !setup_test_fail_register;}
static bool setup_test_firewall(const wchar_t *exe,const wchar_t *rule,const wchar_t *port){if(!wcscmp(port,L"34872"))setup_test_app_rules++;else if(!wcscmp(port,L"34871"))setup_test_obs_rules++;else abort();return true;}
static LSTATUS WINAPI setup_test_open(HKEY key,LPCWSTR path,DWORD options,REGSAM access,PHKEY out){*out=(HKEY)(ULONG_PTR)1;return ERROR_SUCCESS;}
static LSTATUS WINAPI setup_test_set(HKEY key,LPCWSTR name,DWORD reserved,DWORD type,const BYTE *bytes,DWORD count){return ERROR_SUCCESS;}
static LSTATUS WINAPI setup_test_close(HKEY key){return ERROR_SUCCESS;}
static int WINAPI setup_test_message(HWND owner,LPCWSTR text,LPCWSTR title,UINT flags){return IDOK;}
#define SHGetKnownFolderPath setup_test_folder
#define mc_register_source setup_test_register
#define RegOpenKeyExW setup_test_open
#define RegSetValueExW setup_test_set
#define RegCloseKey setup_test_close
#define MessageBoxW setup_test_message
