/* The real Windows TLS path must reject the fixture's untrusted certificate. */
#include "../src/relay.c"
int main(void){
 struct mc_app app={0};wcscpy(app.config.relay,L"wss://localhost:8766");wcscpy(app.config.host_key,L"integration-test-only");strcpy(app.config.room,"11111111111111111111111111111111");struct relay_manager manager={.app=&app};HINTERNET session=NULL,connection=NULL;
 HINTERNET websocket=connect_websocket(&manager,0,&session,&connection);DWORD error=GetLastError();if(websocket)WinHttpCloseHandle(websocket);if(connection)WinHttpCloseHandle(connection);if(session)WinHttpCloseHandle(session);
 printf("WINDOWS_TLS_REJECTION=%lu\n",error);return !websocket&&error==ERROR_WINHTTP_SECURE_FAILURE?0:2;
}
