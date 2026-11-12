#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include "theme.h"
#include <wincodec.h>
#include <shlwapi.h>
#include <dwmapi.h>
#include <gdiplus.h>
#include <math.h>
#include <stdio.h>

static ULONG_PTR gdiplus_token;
static HBRUSH control_brush;
static bool motion=true;
static HBITMAP mips[6];
static int mip_width[6],mip_height[6],mip_count;
struct image_cache {HBITMAP bitmap;int width,height;};
static struct image_cache caches[3];
static unsigned next_cache;

static ARGB argb(unsigned alpha,COLORREF color){return ((ARGB)alpha<<24)|((ARGB)GetRValue(color)<<16)|((ARGB)GetGValue(color)<<8)|GetBValue(color);}
static GpGraphics *graphics(HDC dc){GpGraphics *g=NULL;if(GdipCreateFromHDC(dc,&g)!=Ok)return NULL;GdipSetSmoothingMode(g,SmoothingModeAntiAlias);GdipSetPixelOffsetMode(g,PixelOffsetModeHalf);return g;}
static GpPath *round_path(RECT r,float radius){
    GpPath *path=NULL;GdipCreatePath(FillModeWinding,&path);if(!path)return NULL;
    float x=r.left+.5f,y=r.top+.5f,w=r.right-r.left-1.0f,h=r.bottom-r.top-1.0f,d=radius*2;
    if(d>w)d=w;if(d>h)d=h;
    GdipAddPathArc(path,x,y,d,d,180,90);GdipAddPathArc(path,x+w-d,y,d,d,270,90);
    GdipAddPathArc(path,x+w-d,y+h-d,d,d,0,90);GdipAddPathArc(path,x,y+h-d,d,d,90,90);GdipClosePathFigure(path);return path;
}
static void fill_round(HDC dc,RECT r,COLORREF color,int radius){
    GpGraphics *g=graphics(dc);GpPath *path=round_path(r,(float)radius);GpSolidFill *brush=NULL;GdipCreateSolidFill(argb(255,color),&brush);
    if(g&&path&&brush)GdipFillPath(g,(GpBrush*)brush,path);
    if(brush)GdipDeleteBrush((GpBrush*)brush);if(path)GdipDeletePath(path);if(g)GdipDeleteGraphics(g);
}
static HBITMAP load_background(HINSTANCE module,int *image_width,int *image_height){
    HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(101),RT_RCDATA);if(!resource)return NULL;
    DWORD length=SizeofResource(module,resource);const BYTE *bytes=LockResource(LoadResource(module,resource));
    IStream *stream=SHCreateMemStream(bytes,length);IWICImagingFactory *factory=NULL;IWICBitmapDecoder *decoder=NULL;IWICBitmapFrameDecode *frame=NULL;IWICFormatConverter *convert=NULL;HBITMAP result=NULL;
    HRESULT hr=CoCreateInstance(&CLSID_WICImagingFactory,NULL,CLSCTX_INPROC_SERVER,&IID_IWICImagingFactory,(void**)&factory);
    if(SUCCEEDED(hr)&&stream)hr=IWICImagingFactory_CreateDecoderFromStream(factory,stream,NULL,WICDecodeMetadataCacheOnLoad,&decoder);else hr=E_FAIL;
    if(SUCCEEDED(hr))hr=IWICBitmapDecoder_GetFrame(decoder,0,&frame);
    if(SUCCEEDED(hr))hr=IWICImagingFactory_CreateFormatConverter(factory,&convert);
    if(SUCCEEDED(hr))hr=IWICFormatConverter_Initialize(convert,(IWICBitmapSource*)frame,&GUID_WICPixelFormat32bppBGR,WICBitmapDitherTypeNone,NULL,0,WICBitmapPaletteTypeCustom);
    UINT width=0,height=0;if(SUCCEEDED(hr))hr=IWICFormatConverter_GetSize(convert,&width,&height);
    if(SUCCEEDED(hr)&&width&&height&&width<=4096&&height<=4096){
        BITMAPINFO info={0};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-(LONG)height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        BYTE *pixels=NULL;result=CreateDIBSection(NULL,&info,DIB_RGB_COLORS,(void**)&pixels,NULL,0);
        if(!result||FAILED(IWICFormatConverter_CopyPixels(convert,NULL,width*4,width*height*4,pixels))){if(result)DeleteObject(result);result=NULL;}else{*image_width=width;*image_height=height;}
    }
    if(convert)IWICFormatConverter_Release(convert);if(frame)IWICBitmapFrameDecode_Release(frame);if(decoder)IWICBitmapDecoder_Release(decoder);if(factory)IWICImagingFactory_Release(factory);if(stream)IStream_Release(stream);return result;
}
void mc_theme_init(HINSTANCE instance){
    GdiplusStartupInput startup={1,NULL,FALSE,FALSE};GdiplusStartup(&gdiplus_token,&startup,NULL);control_brush=CreateSolidBrush(RGB(21,24,28));
    BOOL allowed=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&allowed,0);motion=allowed!=FALSE;
    mips[0]=load_background(instance,&mip_width[0],&mip_height[0]);if(!mips[0])return;mip_count=1;
    HDC screen=GetDC(NULL);
    for(int i=1;i<6;i++){
        int w=mip_width[i-1]/2,h=mip_height[i-1]/2;if(w<64||h<64)break;
        HDC src=CreateCompatibleDC(screen),dst=CreateCompatibleDC(screen);HBITMAP bitmap=CreateCompatibleBitmap(screen,w,h);HGDIOBJ a=SelectObject(src,mips[i-1]),b=SelectObject(dst,bitmap);
        SetStretchBltMode(dst,HALFTONE);StretchBlt(dst,0,0,w,h,src,0,0,mip_width[i-1],mip_height[i-1],SRCCOPY);SelectObject(src,a);SelectObject(dst,b);DeleteDC(src);DeleteDC(dst);
        mips[i]=bitmap;mip_width[i]=w;mip_height[i]=h;mip_count++;
    }
    ReleaseDC(NULL,screen);
}
void mc_theme_motion(bool enabled){BOOL allowed=TRUE;SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&allowed,0);motion=enabled&&allowed;}
bool mc_theme_is_animated(void){return motion;}
void mc_theme_free(void){for(int i=0;i<mip_count;i++)DeleteObject(mips[i]);for(int i=0;i<3;i++)if(caches[i].bitmap)DeleteObject(caches[i].bitmap);if(control_brush)DeleteObject(control_brush);if(gdiplus_token)GdiplusShutdown(gdiplus_token);}
void mc_theme_window(HWND window){BOOL dark=TRUE;DwmSetWindowAttribute(window,20,&dark,sizeof(dark));}
void mc_theme_outline(HDC dc,RECT r,COLORREF color,int radius){
    GpGraphics *g=graphics(dc);GpPath *path=round_path(r,(float)radius/2);GpPen *pen=NULL;GdipCreatePen1(argb(165,color),1.0f,UnitPixel,&pen);
    if(g&&path&&pen)GdipDrawPath(g,pen,path);if(pen)GdipDeletePen(pen);if(path)GdipDeletePath(path);if(g)GdipDeleteGraphics(g);
}
static void sheen(GpGraphics *g,RECT r,double phase,COLORREF color,unsigned alpha,float width){
    float cx=(float)(phase*(r.right+2*width)-width);GpPointF a={cx-width,0},b={cx+width,(REAL)r.bottom};GpLineGradient *brush=NULL;
    if(GdipCreateLineBrush(&a,&b,argb(0,color),argb(0,color),WrapModeTileFlipXY,&brush)!=Ok)return;
    ARGB colors[5]={argb(0,color),argb(alpha/4,color),argb(alpha,color),argb(alpha/4,color),argb(0,color)};REAL positions[5]={0,.35f,.5f,.65f,1};
    GdipSetLinePresetBlend(brush,colors,positions,5);GdipFillRectangleI(g,(GpBrush*)brush,0,0,r.right,r.bottom);GdipDeleteBrush((GpBrush*)brush);
}
void mc_theme_background(HDC dc,RECT client){
    if(mips[0]&&client.right>0&&client.bottom>0){
        struct image_cache *cache=NULL;for(int i=0;i<3;i++)if(caches[i].width==client.right&&caches[i].height==client.bottom&&caches[i].bitmap)cache=&caches[i];
        if(!cache){
            cache=&caches[next_cache++%3];if(cache->bitmap)DeleteObject(cache->bitmap);cache->width=client.right;cache->height=client.bottom;cache->bitmap=CreateCompatibleBitmap(dc,client.right,client.bottom);
            int mip=0;while(mip+1<mip_count&&mip_width[mip+1]>=client.right&&mip_height[mip+1]>=client.bottom)mip++;
            HDC src=CreateCompatibleDC(dc),dst=CreateCompatibleDC(dc);HGDIOBJ a=SelectObject(src,mips[mip]),b=SelectObject(dst,cache->bitmap);SetStretchBltMode(dst,HALFTONE);StretchBlt(dst,0,0,client.right,client.bottom,src,0,0,mip_width[mip],mip_height[mip],SRCCOPY);SelectObject(src,a);SelectObject(dst,b);DeleteDC(src);DeleteDC(dst);
        }
        HDC memory=CreateCompatibleDC(dc);HGDIOBJ old=SelectObject(memory,cache->bitmap);BitBlt(dc,0,0,client.right,client.bottom,memory,0,0,SRCCOPY);SelectObject(memory,old);DeleteDC(memory);
    }else{HBRUSH brush=CreateSolidBrush(RGB(10,12,15));FillRect(dc,&client,brush);DeleteObject(brush);}
    if(motion){
        GpGraphics *g=graphics(dc);if(g){double seconds=GetTickCount64()/1000.0;
            sheen(g,client,fmod(seconds/24.0,1.0),RGB(191,208,203),22,client.right*.30f);
            sheen(g,client,fmod(seconds/37.0+.36,1.0),RGB(53,174,112),17,client.right*.55f);
            sheen(g,client,fmod(seconds/43.0+.70,1.0),RGB(134,58,170),19,client.right*.45f);GdipDeleteGraphics(g);
        }
    }
}
void mc_theme_glass(HDC dc,RECT rect,COLORREF tint,int radius){
    GpGraphics *g=graphics(dc);GpPath *path=round_path(rect,(float)radius/2);GpLineGradient *brush=NULL;GpRect bounds={rect.left,rect.top,rect.right-rect.left,rect.bottom-rect.top};COLORREF top=RGB(GetRValue(tint)+14,GetGValue(tint)+14,GetBValue(tint)+14);
    GdipCreateLineBrushFromRectI(&bounds,argb(232,top),argb(241,tint),LinearGradientModeVertical,WrapModeTileFlipXY,&brush);
    if(g&&path&&brush)GdipFillPath(g,(GpBrush*)brush,path);if(brush)GdipDeleteBrush((GpBrush*)brush);if(path)GdipDeletePath(path);if(g)GdipDeleteGraphics(g);mc_theme_outline(dc,rect,RGB(87,106,101),radius);
}
static void parent_background(HWND child,HDC dc){
    HWND parent=GetParent(child);RECT r;GetClientRect(parent,&r);POINT offset={0,0};MapWindowPoints(child,parent,&offset,1);POINT old;SetViewportOrgEx(dc,-offset.x,-offset.y,&old);mc_theme_background(dc,r);SetViewportOrgEx(dc,old.x,old.y,NULL);
}
static LRESULT CALLBACK button_proc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    if(message==WM_MOUSEMOVE){if(!GetPropW(window,L"MC.Hover")){SetPropW(window,L"MC.Hover",(HANDLE)1);TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,window,0};TrackMouseEvent(&track);InvalidateRect(window,NULL,FALSE);}}
    else if(message==WM_MOUSELEAVE){RemovePropW(window,L"MC.Hover");InvalidateRect(window,NULL,FALSE);}
    else if(message==WM_NCDESTROY){RemovePropW(window,L"MC.Hover");RemoveWindowSubclass(window,button_proc,id);}
    return DefSubclassProc(window,message,wp,lp);
}
static LRESULT CALLBACK toggle_proc(HWND window,UINT message,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data){
    if(message==WM_ERASEBKGND)return 1;
    if(message==WM_PAINT){
        PAINTSTRUCT paint;HDC original=BeginPaint(window,&paint);RECT r;GetClientRect(window,&r);HDC dc=CreateCompatibleDC(original);HBITMAP bitmap=CreateCompatibleBitmap(original,r.right,r.bottom);HGDIOBJ old=SelectObject(dc,bitmap);parent_background(window,dc);fill_round(dc,r,RGB(18,22,25),10);
        bool checked=SendMessageW(window,BM_GETCHECK,0,0)==BST_CHECKED;RECT track={r.right-48,7,r.right-2,r.bottom-7};fill_round(dc,track,checked?RGB(42,106,75):RGB(49,49,59),11);int knob_x=checked?track.right-22:track.left+3;fill_round(dc,(RECT){knob_x,track.top+3,knob_x+19,track.bottom-3},checked?RGB(182,242,205):RGB(162,166,177),9);
        wchar_t label[256];GetWindowTextW(window,label,256);HGDIOBJ oldfont=SelectObject(dc,(HFONT)SendMessageW(window,WM_GETFONT,0,0));SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(222,232,228));RECT label_r={8,0,r.right-62,r.bottom};DrawTextW(dc,label,-1,&label_r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);SelectObject(dc,oldfont);
        BitBlt(original,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);EndPaint(window,&paint);return 0;
    }
    if(message==WM_NCDESTROY)RemoveWindowSubclass(window,toggle_proc,id);
    LRESULT result=DefSubclassProc(window,message,wp,lp);if(message==BM_SETCHECK||message==WM_LBUTTONUP||message==WM_KEYUP)InvalidateRect(window,NULL,FALSE);return result;
}
void mc_theme_field(HDC dc,RECT rect){fill_round(dc,rect,RGB(21,24,28),10);mc_theme_outline(dc,rect,RGB(72,86,82),20);}
void mc_theme_control(HWND control,const wchar_t *kind){
    if(!_wcsicmp(kind,L"STATIC")){LONG_PTR style=GetWindowLongPtrW(control,GWL_STYLE);SetWindowLongPtrW(control,GWL_STYLE,(style&~SS_TYPEMASK)|SS_OWNERDRAW);}
    if(!_wcsicmp(kind,L"BUTTON")){LONG_PTR style=GetWindowLongPtrW(control,GWL_STYLE);if((style&BS_TYPEMASK)!=BS_AUTOCHECKBOX){SetWindowLongPtrW(control,GWL_STYLE,(style&~BS_TYPEMASK)|BS_OWNERDRAW);SetWindowSubclass(control,button_proc,1,0);}else SetWindowSubclass(control,toggle_proc,2,0);}
}
bool mc_theme_draw(const DRAWITEMSTRUCT *draw,HFONT font){
    if(draw->CtlType!=ODT_BUTTON&&draw->CtlType!=ODT_STATIC)return false;RECT r=draw->rcItem;int width=r.right-r.left,height=r.bottom-r.top;
    HDC dc=CreateCompatibleDC(draw->hDC);HBITMAP bitmap=CreateCompatibleBitmap(draw->hDC,width,height);HGDIOBJ old=SelectObject(dc,bitmap);r=(RECT){0,0,width,height};
    if(draw->CtlType==ODT_STATIC){parent_background(draw->hwndItem,dc);wchar_t label[512];GetWindowTextW(draw->hwndItem,label,512);HGDIOBJ oldfont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(213,225,219));DrawTextW(dc,label,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE);SelectObject(dc,oldfont);BitBlt(draw->hDC,0,0,width,height,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return true;}
    bool primary=draw->CtlID>=200&&draw->CtlID<205;
    if(primary){HBRUSH base=CreateSolidBrush(RGB(15,26,24));FillRect(dc,&r,base);DeleteObject(base);}else parent_background(draw->hwndItem,dc);
    bool hover=GetPropW(draw->hwndItem,L"MC.Hover")!=NULL,pressed=(draw->itemState&ODS_SELECTED)!=0;
    COLORREF tint=primary?(pressed?RGB(14,54,40):hover?RGB(28,82,60):RGB(21,65,48)):(pressed?RGB(35,27,44):hover?RGB(54,45,65):RGB(31,32,39));
    mc_theme_glass(dc,(RECT){1,1,width-1,height-1},tint,20);if(primary&&hover)mc_theme_outline(dc,(RECT){1,1,width-1,height-1},RGB(100,174,136),20);
    wchar_t label[256];GetWindowTextW(draw->hwndItem,label,256);HGDIOBJ oldfont=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,draw->itemState&ODS_DISABLED?RGB(120,127,130):primary?RGB(217,255,231):RGB(228,231,237));
    if(pressed)OffsetRect(&r,0,1);
    if(draw->CtlID==101||draw->CtlID==501||draw->CtlID==503){r.right-=18;DrawTextW(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);RECT arrow={width-24,height/2-6,width-8,height/2+7};DrawTextW(dc,L"⌄",-1,&arrow,DT_CENTER|DT_VCENTER|DT_SINGLELINE);}else DrawTextW(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
    if(draw->itemState&ODS_FOCUS)mc_theme_outline(dc,(RECT){4,4,width-4,height-4},RGB(115,157,136),14);
    SelectObject(dc,oldfont);BitBlt(draw->hDC,0,0,width,height,dc,0,0,SRCCOPY);SelectObject(dc,old);DeleteObject(bitmap);DeleteDC(dc);return true;
}
HBRUSH mc_theme_color(HDC dc){SetTextColor(dc,RGB(227,234,233));SetBkColor(dc,RGB(21,24,28));return control_brush;}
void mc_theme_placeholder(HDC dc,RECT r,unsigned number){
    fill_round(dc,r,RGB(8,12,13),12);mc_theme_outline(dc,r,RGB(43,58,52),24);int x=(r.left+r.right)/2,y=(r.top+r.bottom)/2;
    mc_theme_outline(dc,(RECT){x-24,y-17,x+24,y+17},RGB(76,107,92),14);GpGraphics *g=graphics(dc);GpPen *pen=NULL;GdipCreatePen1(argb(200,RGB(100,151,123)),1.2f,UnitPixel,&pen);if(g&&pen)GdipDrawEllipse(g,pen,x-8,y-8,16,16);if(pen)GdipDeletePen(pen);if(g)GdipDeleteGraphics(g);
}
