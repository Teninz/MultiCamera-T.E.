#include "app.h"
#include <bcrypt.h>
#include <wincrypt.h>
#include <shlobj.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
static bool config_path(wchar_t *path){PWSTR base=NULL;if(FAILED(SHGetKnownFolderPath(&FOLDERID_LocalAppData,0,NULL,&base)))return false;swprintf(path,2048,L"%ls\\MultiCameraTE",base);CoTaskMemFree(base);if(SHCreateDirectoryExW(NULL,path,NULL)!=ERROR_SUCCESS&&GetLastError()!=ERROR_ALREADY_EXISTS&&GetFileAttributesW(path)==INVALID_FILE_ATTRIBUTES)return false;wcscat(path,L"\\state.bin");return true;}
static bool random_hex(char *out){uint8_t bytes[16];if(BCryptGenRandom(NULL,bytes,16,BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)return false;for(int i=0;i<16;i++)sprintf(out+2*i,"%02x",bytes[i]);SecureZeroMemory(bytes,16);return true;}
bool mc_config_load(struct mc_config *config){
 wchar_t path[2048];if(!config_path(path))return false;FILE *file=_wfopen(path,L"rb");if(!file){memset(config,0,sizeof(*config));config->magic=0x3143544d;config->version=2;config->animation=1;WORD language=PRIMARYLANGID(GetUserDefaultUILanguage());config->language=language==LANG_RUSSIAN?1:language==LANG_JAPANESE?2:language==LANG_SPANISH?3:language==LANG_CHINESE?4:0;for(int i=0;i<5;i++){if(!random_hex(config->tokens[i]))return false;snprintf(config->names[i],96,"Smartphone %d",i+1);}if(!random_hex(config->room))return false;return mc_config_save(config);}
 fseek(file,0,SEEK_END);long length=ftell(file);rewind(file);if(length<1||length>32768){fclose(file);return false;}BYTE *bytes=malloc(length);if(!bytes){fclose(file);return false;}bool ok=fread(bytes,1,length,file)==(size_t)length;fclose(file);DATA_BLOB input={(DWORD)length,bytes},output={0};if(ok)ok=CryptUnprotectData(&input,NULL,NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&output)!=0;if(ok){ok=output.cbData==sizeof(*config)||output.cbData==offsetof(struct mc_config,animation);if(ok){memset(config,0,sizeof(*config));config->animation=1;memcpy(config,output.pbData,output.cbData);ok=config->magic==0x3143544d&&(config->version==1||config->version==2)&&config->language<5;config->version=2;for(int i=0;i<5;i++)if(strnlen(config->tokens[i],33)!=32||strspn(config->tokens[i],"0123456789abcdef")!=32)ok=false;config->relay[511]=0;config->host_key[255]=0;config->room[32]=0;}SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);}SecureZeroMemory(bytes,length);free(bytes);return ok;
}
bool mc_config_save(const struct mc_config *config){wchar_t path[2048],temporary[2080];if(!config_path(path))return false;swprintf(temporary,2080,L"%ls.tmp",path);DATA_BLOB input={sizeof(*config),(BYTE*)config},output={0};if(!CryptProtectData(&input,L"MultiCamera T.E. pairing",NULL,NULL,NULL,CRYPTPROTECT_UI_FORBIDDEN,&output))return false;FILE *file=_wfopen(temporary,L"wb");bool ok=false;if(file){ok=fwrite(output.pbData,1,output.cbData,file)==output.cbData;fclose(file);if(ok)ok=MoveFileExW(temporary,path,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;}LocalFree(output.pbData);return ok;}
