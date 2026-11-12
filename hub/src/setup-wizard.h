/* Native light installer: embedded PNGs, no WebView or browser dependency. */
#include <gdiplus.h>
#include <shlwapi.h>
static HWND wizard,choice_buttons[2],install_button;
static HFONT wizard_font,wizard_title,wizard_card_title,wizard_small;
static GpBitmap *wizard_images[3];static IStream *wizard_streams[3];
static HBITMAP wizard_backdrop;
static unsigned wizard_choice=1,wizard_result;static UINT wizard_dpi=96;
static int wd(int value){return MulDiv(value,wizard_dpi,96);}
static void wizard_round(HDC dc,RECT r,ARGB fill,ARGB stroke,int radius){
 GpGraphics *g=NULL;GpPath *p=NULL;GpSolidFill *brush=NULL;GpPen *pen=NULL;GdipCreateFromHDC(dc,&g);if(!g)return;
 GdipSetSmoothingMode(g,SmoothingModeAntiAlias);GdipCreatePath(FillModeWinding,&p);float x=r.left+1,y=r.top+1,w=r.right-r.left-2,h=r.bottom-r.top-2,d=wd(radius*2);
 GdipAddPathArc(p,x,y,d,d,180,90);GdipAddPathArc(p,x+w-d,y,d,d,270,90);GdipAddPathArc(p,x+w-d,y+h-d,d,d,0,90);GdipAddPathArc(p,x,y+h-d,d,d,90,90);GdipClosePathFigure(p);
 GdipCreateSolidFill(fill,&brush);GdipFillPath(g,(GpBrush*)brush,p);if(stroke){GdipCreatePen1(stroke,(REAL)wd(1),UnitPixel,&pen);GdipDrawPath(g,pen,p);GdipDeletePen(pen);}GdipDeleteBrush((GpBrush*)brush);GdipDeletePath(p);GdipDeleteGraphics(g);
}
static void wizard_text(HDC dc,const wchar_t *value,RECT r,HFONT font,COLORREF color,UINT flags){HGDIOBJ old=SelectObject(dc,font);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,color);DrawTextW(dc,value,-1,&r,flags|DT_NOPREFIX);SelectObject(dc,old);}
static void wizard_image(HDC dc,int id,RECT r){if(!wizard_images[id])return;GpGraphics *g=NULL;GdipCreateFromHDC(dc,&g);if(!g)return;GdipSetInterpolationMode(g,InterpolationModeHighQualityBicubic);GdipDrawImageRectI(g,(GpImage*)wizard_images[id],r.left,r.top,r.right-r.left,r.bottom-r.top);GdipDeleteGraphics(g);}
static void wizard_labels(void){
 wchar_t label[512];for(int i=0;i<2;i++){swprintf(label,512,L"%ls%ls",mc_text(i?MC_TEXT_COMPONENT_OBS:MC_TEXT_COMPONENT_APP),wizard_choice&(1u<<i)?mc_text(MC_TEXT_COMPONENT_SELECTED):L"");SetWindowTextW(choice_buttons[i],label);InvalidateRect(choice_buttons[i],NULL,FALSE);}EnableWindow(install_button,wizard_choice!=0);InvalidateRect(install_button,NULL,FALSE);
}
static LRESULT CALLBACK wizard_proc(HWND window,UINT message,WPARAM wp,LPARAM lp){
 switch(message){
 case WM_CREATE:{
  DWORD style=WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW;HINSTANCE module=GetModuleHandleW(NULL);
  choice_buttons[0]=CreateWindowW(L"BUTTON",L"",style,wd(32),wd(132),wd(396),wd(370),window,(HMENU)701,module,NULL);
  choice_buttons[1]=CreateWindowW(L"BUTTON",L"",style,wd(448),wd(132),wd(396),wd(370),window,(HMENU)702,module,NULL);
  CreateWindowW(L"BUTTON",mc_text(MC_TEXT_CANCEL),style,wd(32),wd(590),wd(138),wd(42),window,(HMENU)IDCANCEL,module,NULL);
  install_button=CreateWindowW(L"BUTTON",mc_text(MC_TEXT_INSTALL_BUTTON),style,wd(580),wd(590),wd(264),wd(42),window,(HMENU)IDOK,module,NULL);
  wizard_labels();return 0;
 }
 case WM_ERASEBKGND:return 1;
 case WM_PAINT:{PAINTSTRUCT ps;HDC target=BeginPaint(window,&ps);RECT r;GetClientRect(window,&r);HDC dc=CreateCompatibleDC(target);HBITMAP buffer=CreateCompatibleBitmap(target,r.right,r.bottom);HGDIOBJ old=SelectObject(dc,buffer);HBRUSH background=CreateSolidBrush(RGB(239,245,242));FillRect(dc,&r,background);DeleteObject(background);wizard_image(dc,0,r);
  wizard_text(dc,L"MULTICAMERA T.E.  /  SETUP",(RECT){wd(34),wd(24),wd(840),wd(44)},wizard_small,RGB(80,111,99),DT_LEFT);
  wizard_text(dc,mc_text(MC_TEXT_COMPONENT_TITLE),(RECT){wd(32),wd(52),wd(844),wd(94)},wizard_title,RGB(28,57,47),DT_LEFT);
  wizard_text(dc,mc_text(MC_TEXT_COMPONENT_SUBTITLE),(RECT){wd(34),wd(99),wd(842),wd(127)},wizard_font,RGB(73,99,88),DT_LEFT);
  wizard_text(dc,mc_text(MC_TEXT_COMPONENT_DEPENDENCY),(RECT){wd(36),wd(517),wd(840),wd(551)},wizard_font,RGB(42,76,61),DT_WORDBREAK);
  wizard_text(dc,mc_text(MC_TEXT_COMPONENT_NETWORK),(RECT){wd(36),wd(554),wd(840),wd(581)},wizard_small,RGB(79,102,93),DT_WORDBREAK);
  BitBlt(target,0,0,r.right,r.bottom,dc,0,0,SRCCOPY);SelectObject(dc,old);if(wizard_backdrop)DeleteObject(wizard_backdrop);wizard_backdrop=buffer;DeleteDC(dc);EndPaint(window,&ps);return 0;
 }
 case WM_DRAWITEM:{const DRAWITEMSTRUCT *item=(const DRAWITEMSTRUCT*)lp;RECT r=item->rcItem;HDC dc=item->hDC;
  /* Copy the exact parent pixels behind each rounded control, including the
     patterned glass. A solid child brush leaves rectangular corner patches. */
  if(wizard_backdrop){RECT position;GetWindowRect(item->hwndItem,&position);MapWindowPoints(NULL,window,(POINT*)&position,2);HDC background=CreateCompatibleDC(dc);HGDIOBJ old=SelectObject(background,wizard_backdrop);BitBlt(dc,0,0,r.right,r.bottom,background,position.left,position.top,SRCCOPY);SelectObject(background,old);DeleteDC(background);}
  else {HBRUSH bg=CreateSolidBrush(RGB(239,245,242));FillRect(dc,&r,bg);DeleteObject(bg);}
  if(item->CtlID==701||item->CtlID==702){int index=item->CtlID-701;bool selected=(wizard_choice&(1u<<index))!=0;
   wizard_round(dc,r,selected?0xffedf8f1:0xfffafcfc,selected?0xff76ac8b:0xffc8d8d0,22);
   wizard_image(dc,index+1,(RECT){wd(90),wd(19),wd(306),wd(235)});
   RECT box={r.right-wd(43),wd(20),r.right-wd(21),wd(42)};wizard_round(dc,box,selected?0xff276747:0xfff4f8f5,selected?0xff276747:0xffb0c6ba,6);
   if(selected)wizard_text(dc,L"✓",box,wizard_font,RGB(255,255,255),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
   wizard_text(dc,mc_text(index?MC_TEXT_CARD_OBS_TITLE:MC_TEXT_CARD_APP_TITLE),(RECT){wd(24),wd(239),r.right-wd(24),wd(273)},wizard_card_title,RGB(28,57,47),DT_CENTER);
   wizard_text(dc,mc_text(index?MC_TEXT_CARD_OBS_DESC:MC_TEXT_CARD_APP_DESC),(RECT){wd(28),wd(283),r.right-wd(28),wd(355)},wizard_font,RGB(62,91,77),DT_CENTER|DT_WORDBREAK);
  }else{bool primary=item->CtlID==IDOK,disabled=item->itemState&ODS_DISABLED;wizard_round(dc,r,primary?(disabled?0xffa9bcb1:0xff286747):0xfff8fbf9,primary?0:0xffbecfc5,12);wchar_t label[128];GetWindowTextW(item->hwndItem,label,128);wizard_text(dc,label,r,wizard_font,primary?RGB(255,255,255):RGB(47,78,62),DT_CENTER|DT_VCENTER|DT_SINGLELINE);}
  if(item->itemState&ODS_FOCUS){RECT focus=r;InflateRect(&focus,-wd(6),-wd(6));wizard_round(dc,focus,0,0xff3b8060,item->CtlID==701||item->CtlID==702?17:7);}return TRUE;
 }
 case WM_COMMAND:if(LOWORD(wp)==701||LOWORD(wp)==702){wizard_choice^=1u<<(LOWORD(wp)-701);wizard_labels();return 0;}if(LOWORD(wp)==IDOK&&wizard_choice){wizard_result=wizard_choice;DestroyWindow(window);return 0;}if(LOWORD(wp)==IDCANCEL){DestroyWindow(window);return 0;}break;
 case WM_CLOSE:DestroyWindow(window);return 0;
 }return DefWindowProcW(window,message,wp,lp);
}
static unsigned choose_components(void){
 SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);wizard_dpi=GetDpiForSystem();HINSTANCE module=GetModuleHandleW(NULL);ULONG_PTR token=0;GdiplusStartupInput startup={1,NULL,FALSE,FALSE};if(GdiplusStartup(&token,&startup,NULL)!=Ok)return 0;
 for(int i=0;i<3;i++){HRSRC resource=FindResourceW(module,MAKEINTRESOURCEW(201+i),RT_RCDATA);if(resource){wizard_streams[i]=SHCreateMemStream(LockResource(LoadResource(module,resource)),SizeofResource(module,resource));if(wizard_streams[i])GdipCreateBitmapFromStream(wizard_streams[i],&wizard_images[i]);}}
 wizard_font=CreateFontW(-wd(15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");wizard_title=CreateFontW(-wd(28),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");wizard_card_title=CreateFontW(-wd(22),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");wizard_small=CreateFontW(-wd(12),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 WNDCLASSW cls={0};cls.hInstance=module;cls.hIcon=LoadIconW(module,MAKEINTRESOURCEW(1));cls.hCursor=LoadCursor(NULL,IDC_ARROW);cls.lpfnWndProc=wizard_proc;cls.lpszClassName=L"MultiCameraTE.Setup.Choice";RegisterClassW(&cls);RECT area;SystemParametersInfoW(SPI_GETWORKAREA,0,&area,0);RECT bounds={0,0,wd(876),wd(656)};AdjustWindowRectExForDpi(&bounds,WS_CAPTION|WS_SYSMENU,FALSE,0,wizard_dpi);
 wizard=CreateWindowW(cls.lpszClassName,MC_TITLE,WS_CAPTION|WS_SYSMENU|WS_VISIBLE|WS_CLIPCHILDREN,area.left+(area.right-area.left-(bounds.right-bounds.left))/2,area.top+(area.bottom-area.top-(bounds.bottom-bounds.top))/2,bounds.right-bounds.left,bounds.bottom-bounds.top,NULL,NULL,module,NULL);
 if(wizard){SetFocus(choice_buttons[0]);MSG message;while(IsWindow(wizard)&&GetMessageW(&message,NULL,0,0)>0){if(!IsDialogMessageW(wizard,&message)){TranslateMessage(&message);DispatchMessageW(&message);}}}
 for(int i=0;i<3;i++){if(wizard_images[i])GdipDisposeImage((GpImage*)wizard_images[i]);if(wizard_streams[i])IStream_Release(wizard_streams[i]);}if(wizard_backdrop){DeleteObject(wizard_backdrop);wizard_backdrop=NULL;}DeleteObject(wizard_font);DeleteObject(wizard_title);DeleteObject(wizard_card_title);DeleteObject(wizard_small);GdiplusShutdown(token);return wizard_result;
}
