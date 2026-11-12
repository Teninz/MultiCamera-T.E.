/* Two-QR pairing window owned by OBS. Pairing secrets and QR bitmaps remain in memory. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <obs-module.h>
#include <stdio.h>
#include <stdint.h>
#include "release.h"
#include "qrcodegen.h"
#include "pairing.h"
#define SHOW_PAIR (WM_APP+103)
struct dialog_data {
    obs_weak_source_t *weak;
    char address[64],token[33];int port;
    bool (*connected)(void *);void *context;
    HBITMAP apk,pair;HFONT normal,title,strong;HBRUSH background,card;
    HWND state;float scale;
};
static HANDLE ui_thread,ready;static DWORD ui_id;static HWND active;static HINSTANCE module;
static PVOID volatile owner_window;
static HWND obs_window(void){return (HWND)InterlockedCompareExchangePointer(&owner_window,NULL,NULL);}
static void cache_obs_window(void *unused)
{
    UNUSED_PARAMETER(unused);HMODULE dll=GetModuleHandleW(L"obs-frontend-api.dll");if(!dll)return;
    union{FARPROC raw;void *(*get)(void);}symbol;symbol.raw=GetProcAddress(dll,"obs_frontend_get_main_window_handle");if(symbol.get)InterlockedExchangePointer(&owner_window,symbol.get());
}
void olc_pairing_frontend_ready(void){obs_queue_task(OBS_TASK_UI,cache_obs_window,NULL,false);}
void *olc_pairing_owner(void){return obs_window();}
static const wchar_t *label(const char *key){static __declspec(thread) wchar_t buffer[8][1200];static __declspec(thread) int index;wchar_t *out=buffer[(index++)%8];MultiByteToWideChar(CP_UTF8,0,obs_module_text(key),-1,out,1200);return out;}
bool olc_pairing_available(void) { return obs_window()!=NULL; }
static wchar_t *wide(const char *text){int n=MultiByteToWideChar(CP_UTF8,0,text,-1,NULL,0);wchar_t *out=bzalloc(n*sizeof(wchar_t));MultiByteToWideChar(CP_UTF8,0,text,-1,out,n);return out;}
static int px(struct dialog_data *d,int n){return (int)(n*d->scale+.5f);}
static void free_data(struct dialog_data *d)
{
    if(!d)return;if(d->weak)obs_weak_source_release(d->weak);if(d->apk)DeleteObject(d->apk);if(d->pair)DeleteObject(d->pair);
    if(d->normal)DeleteObject(d->normal);if(d->title)DeleteObject(d->title);if(d->strong)DeleteObject(d->strong);if(d->background)DeleteObject(d->background);if(d->card)DeleteObject(d->card);SecureZeroMemory(d->token,sizeof(d->token));bfree(d);
}
static HBITMAP qr_bitmap(const char *text,int block)
{
    uint8_t temp[qrcodegen_BUFFER_LEN_MAX],qr[qrcodegen_BUFFER_LEN_MAX];
    if(!qrcodegen_encodeText(text,temp,qr,qrcodegen_Ecc_MEDIUM,1,qrcodegen_VERSION_MAX,qrcodegen_Mask_AUTO,true))return NULL;
    int size=qrcodegen_getSize(qr),width=(size+8)*block;BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-width;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    uint32_t *pixels=NULL;HBITMAP bitmap=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);if(!bitmap)return NULL;
    for(int y=0;y<width;y++)for(int x=0;x<width;x++){int qx=x/block-4,qy=y/block-4;bool black=qx>=0&&qy>=0&&qx<size&&qy<size&&qrcodegen_getModule(qr,qx,qy);pixels[y*width+x]=black?0xff000000:0xffffffff;}return bitmap;
}
static HWND control(HWND parent,struct dialog_data *d,const wchar_t *kind,const wchar_t *text,DWORD style,int x,int y,int w,int h,int id,HFONT font)
{
    HWND child=CreateWindowExW(0,kind,text,WS_CHILD|WS_VISIBLE|style,px(d,x),px(d,y),px(d,w),px(d,h),parent,(HMENU)(INT_PTR)id,module,NULL);SendMessageW(child,WM_SETFONT,(WPARAM)font,TRUE);return child;
}
static void image(HWND parent,struct dialog_data *d,HBITMAP bitmap,int center_x,int top)
{
    if(!bitmap)return;BITMAP size;GetObjectW(bitmap,sizeof(size),&size);HWND view=CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|SS_BITMAP,px(d,center_x)-size.bmWidth/2,px(d,top),size.bmWidth,size.bmHeight,parent,NULL,module,NULL);SendMessageW(view,STM_SETIMAGE,IMAGE_BITMAP,(LPARAM)bitmap);
}
static LRESULT CALLBACK window_proc(HWND window,UINT message,WPARAM wp,LPARAM lp)
{
    struct dialog_data *d=(struct dialog_data*)GetWindowLongPtrW(window,GWLP_USERDATA);
    if(message==WM_CREATE){d=((CREATESTRUCTW*)lp)->lpCreateParams;SetWindowLongPtrW(window,GWLP_USERDATA,(LONG_PTR)d);
        d->background=CreateSolidBrush(RGB(16,22,30));d->card=CreateSolidBrush(RGB(28,36,46));
        d->normal=CreateFontW(-px(d,15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        d->strong=CreateFontW(-px(d,17),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        d->title=CreateFontW(-px(d,24),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        control(window,d,L"STATIC",label("PairTitle"),0,24,18,710,32,0,d->title);
        control(window,d,L"STATIC",label("PairSubtitle"),0,24,58,710,38,0,d->normal);
        control(window,d,L"STATIC",label("PairInstall"),0,42,120,310,28,0,d->strong);
        control(window,d,L"STATIC",label("PairConnect"),0,410,120,310,28,0,d->strong);
        int block=px(d,4);if(block<3)block=3;d->apk=qr_bitmap(OLC_APK_URL,block);
        char uri[180];snprintf(uri,sizeof(uri),"obslan://%s:%d#%s",d->address,d->port,d->token);d->pair=qr_bitmap(uri,block);
        image(window,d,d->apk,196,164);image(window,d,d->pair,564,164);
        control(window,d,L"STATIC",label("PairInstallHint"),0,42,398,300,44,0,d->normal);
        control(window,d,L"STATIC",label("PairConnectHint"),0,410,398,300,44,0,d->normal);
        d->state=control(window,d,L"STATIC",label("Waiting"),0,24,476,420,28,0,d->strong);
        char address[120];snprintf(address,sizeof(address),"%s: %s:%d",obs_module_text("Address"),d->address,d->port);wchar_t *wa=wide(address);control(window,d,L"STATIC",wa,0,24,507,420,24,0,d->normal);bfree(wa);
        control(window,d,L"BUTTON",label("PairCopy"),BS_PUSHBUTTON|WS_TABSTOP,468,478,266,40,100,d->normal);
        control(window,d,L"BUTTON",label("PairReleases"),BS_PUSHBUTTON|WS_TABSTOP,24,544,180,34,101,d->normal);
        control(window,d,L"BUTTON",label("PairDone"),BS_DEFPUSHBUTTON|WS_TABSTOP,628,544,106,34,IDOK,d->normal);SetTimer(window,1,500,NULL);return 0;
    }
    if(!d)return DefWindowProcW(window,message,wp,lp);
    switch(message){
    case WM_ERASEBKGND:{RECT rect;GetClientRect(window,&rect);FillRect((HDC)wp,&rect,d->background);return 1;}
    case WM_PAINT:{PAINTSTRUCT paint;HDC dc=BeginPaint(window,&paint);HGDIOBJ old=SelectObject(dc,d->card),pen=SelectObject(dc,GetStockObject(NULL_PEN));RoundRect(dc,px(d,24),px(d,106),px(d,368),px(d,452),px(d,18),px(d,18));RoundRect(dc,px(d,392),px(d,106),px(d,736),px(d,452),px(d,18),px(d,18));SelectObject(dc,old);SelectObject(dc,pen);EndPaint(window,&paint);return 0;}
    case WM_CTLCOLORSTATIC:{HDC dc=(HDC)wp;RECT rect;GetWindowRect((HWND)lp,&rect);MapWindowPoints(NULL,window,(POINT*)&rect,2);bool card=rect.top>=px(d,106)&&rect.bottom<=px(d,452);SetTextColor(dc,RGB(227,236,242));SetBkColor(dc,card?RGB(28,36,46):RGB(16,22,30));return (LRESULT)(card?d->card:d->background);}
    case WM_TIMER:{obs_source_t *source=obs_weak_source_get_source(d->weak);if(!source){DestroyWindow(window);return 0;}bool removed=obs_source_removed(source),connected=!removed&&d->connected&&d->connected(d->context);obs_source_release(source);if(removed){DestroyWindow(window);return 0;}SetWindowTextW(d->state,connected?label("Connected"):label("Waiting"));return 0;}
    case WM_COMMAND:
        if(LOWORD(wp)==IDOK||LOWORD(wp)==IDCANCEL){DestroyWindow(window);return 0;}
        if(LOWORD(wp)==101){ShellExecuteW(window,L"open",L"https://github.com/Teninz/obs-lan-camera/releases/latest",NULL,NULL,SW_SHOWNORMAL);return 0;}
        if(LOWORD(wp)==100){char uri[180];snprintf(uri,sizeof(uri),"obslan://%s:%d#%s",d->address,d->port,d->token);wchar_t *text=wide(uri);size_t size=(wcslen(text)+1)*sizeof(wchar_t);HGLOBAL memory=GlobalAlloc(GMEM_MOVEABLE,size);if(memory){void *p=GlobalLock(memory);if(p){memcpy(p,text,size);GlobalUnlock(memory);if(OpenClipboard(window)){EmptyClipboard();if(SetClipboardData(CF_UNICODETEXT,memory))memory=NULL;CloseClipboard();}}if(memory)GlobalFree(memory);}SecureZeroMemory(text,size);bfree(text);return 0;}break;
    case WM_CLOSE:DestroyWindow(window);return 0;
    case WM_NCDESTROY:KillTimer(window,1);SetWindowLongPtrW(window,GWLP_USERDATA,0);active=NULL;free_data(d);return DefWindowProcW(window,message,wp,lp);
    }return DefWindowProcW(window,message,wp,lp);
}
static DWORD WINAPI ui_main(void *unused)
{
    UNUSED_PARAMETER(unused);MSG message;PeekMessageW(&message,NULL,0,0,PM_NOREMOVE);
    union{FARPROC raw;DPI_AWARENESS_CONTEXT(WINAPI *set)(DPI_AWARENESS_CONTEXT);}dpi;dpi.raw=GetProcAddress(GetModuleHandleW(L"user32.dll"),"SetThreadDpiAwarenessContext");if(dpi.set)dpi.set(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,(LPCWSTR)(uintptr_t)ui_main,&module);
    WNDCLASSW cls={0};cls.lpfnWndProc=window_proc;cls.hInstance=module;cls.lpszClassName=L"OBSLANCamera.Pairing";cls.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClassW(&cls);SetEvent(ready);
    while(GetMessageW(&message,NULL,0,0)>0){
        if(message.message==SHOW_PAIR){struct dialog_data *data=(struct dialog_data*)message.lParam;if(active)DestroyWindow(active);
            HWND owner=obs_window();UINT scale=owner?GetDpiForWindow(owner):96;data->scale=(scale?scale:96)/96.0f;
            RECT rect={0,0,px(data,760),px(data,598)};AdjustWindowRectEx(&rect,WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,0,WS_EX_DLGMODALFRAME);
            MONITORINFO monitor={.cbSize=sizeof(monitor)};GetMonitorInfoW(MonitorFromWindow(owner,MONITOR_DEFAULTTOPRIMARY),&monitor);int width=rect.right-rect.left,height=rect.bottom-rect.top;
            active=CreateWindowExW(WS_EX_DLGMODALFRAME,L"OBSLANCamera.Pairing",label("PairWindowTitle"),WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,(monitor.rcWork.left+monitor.rcWork.right-width)/2,(monitor.rcWork.top+monitor.rcWork.bottom-height)/2,width,height,owner,NULL,module,data);
            if(active){ShowWindow(active,SW_SHOW);SetForegroundWindow(active);}else free_data(data);continue;
        }
        if(active&&IsDialogMessageW(active,&message))continue;TranslateMessage(&message);DispatchMessageW(&message);
    }
    if(active)DestroyWindow(active);while(PeekMessageW(&message,NULL,SHOW_PAIR,SHOW_PAIR,PM_REMOVE))free_data((struct dialog_data*)message.lParam);UnregisterClassW(cls.lpszClassName,module);return 0;
}
void olc_pairing_init(void){ready=CreateEventW(NULL,TRUE,FALSE,NULL);ui_thread=CreateThread(NULL,0,ui_main,NULL,0,&ui_id);if(ui_thread)WaitForSingleObject(ready,5000);}
void olc_pairing_show(obs_source_t *source,const char *address,int port,const char *token,bool(*connected)(void *),void *context)
{
    if(!ui_thread||WaitForSingleObject(ready,0)!=WAIT_OBJECT_0)return;struct dialog_data *d=bzalloc(sizeof(*d));d->weak=obs_source_get_weak_source(source);snprintf(d->address,sizeof(d->address),"%s",address);snprintf(d->token,sizeof(d->token),"%s",token);d->port=port;d->connected=connected;d->context=context;
    if(!PostThreadMessageW(ui_id,SHOW_PAIR,0,(LPARAM)d))free_data(d);
}
void olc_pairing_shutdown(void){if(ui_thread){PostThreadMessageW(ui_id,WM_QUIT,0,0);WaitForSingleObject(ui_thread,INFINITE);CloseHandle(ui_thread);ui_thread=NULL;}if(ready){CloseHandle(ready);ready=NULL;}}
