#define MC_SETUP_TEST 1
#define wWinMain unused_installer_entry
#include "../src/setup.c"
#include <assert.h>
static void marker(const wchar_t *path){FILE *file=_wfopen(path,L"wb");assert(file);fputc(42,file);fclose(file);}
int wmain(int argc,wchar_t **argv){
 assert(argc==2&&wcslen(argv[1])<1000);wcscpy(setup_test_root,argv[1]);assert(!exists(setup_test_root));assert(directory(setup_test_root));
 wchar_t app[2048],plugin[2048],app_marker[2048],plugin_marker[2048],exe[2048],obs[2048],parent[2048];
 swprintf(app,2048,L"%ls/pf/MultiCamera T.E",setup_test_root);swprintf(plugin,2048,L"%ls/pd/obs-studio/plugins/obs-lan-camera",setup_test_root);
 swprintf(app_marker,2048,L"%ls/marker",app);swprintf(plugin_marker,2048,L"%ls/marker",plugin);swprintf(exe,2048,L"%ls/MultiCameraTE.exe",app);
 swprintf(parent,2048,L"%ls/pf/obs-studio/bin/64bit",setup_test_root);assert(directory(parent));swprintf(obs,2048,L"%ls/obs64.exe",parent);marker(obs);
 components=1;assert(install(L"test-pipe"));assert(exists(exe)&&!exists(plugin));assert(setup_test_registers==1&&setup_test_shortcuts==1&&setup_test_app_rules==1&&setup_test_obs_rules==0);marker(app_marker);puts("APP_ONLY_PASS");
 components=2;assert(install(L"test-pipe"));assert(exists(plugin)&&exists(app_marker));assert(setup_test_registers==1&&setup_test_shortcuts==1&&setup_test_app_rules==1&&setup_test_obs_rules==1);marker(plugin_marker);puts("PLUGIN_ONLY_PRESERVES_APP_PASS");
 components=1;assert(install(L"test-pipe"));assert(exists(plugin_marker)&&!exists(app_marker));assert(setup_test_registers==2&&setup_test_obs_rules==1);puts("APP_UPGRADE_PRESERVES_PLUGIN_PASS");
 components=3;assert(install(L"test-pipe"));assert(!exists(plugin_marker)&&exists(exe));assert(setup_test_registers==3&&setup_test_shortcuts==3&&setup_test_app_rules==3&&setup_test_obs_rules==2);puts("BOTH_PASS");
 marker(app_marker);marker(plugin_marker);setup_test_fail_register=true;assert(!install(L"test-pipe"));assert(exists(app_marker)&&exists(plugin_marker));assert(setup_test_app_rules==3&&setup_test_obs_rules==2);puts("BOTH_ROLLBACK_PASS");
 assert(parse_components(L"1")==1&&parse_components(L"2")==2&&parse_components(L"3")==3&&!parse_components(L"0")&&!parse_components(L"12")&&!parse_components(L"3junk"));puts("COMPONENT_MASK_VALIDATION_PASS");return 0;
}
