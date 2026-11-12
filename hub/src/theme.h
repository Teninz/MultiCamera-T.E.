#pragma once
#include <windows.h>
#include <commctrl.h>
#include <stdbool.h>
void mc_theme_init(HINSTANCE instance);
void mc_theme_free(void);
void mc_theme_motion(bool enabled);
bool mc_theme_is_animated(void);
void mc_theme_field(HDC dc, RECT rect);
void mc_theme_window(HWND window);
void mc_theme_control(HWND control, const wchar_t *kind);
void mc_theme_background(HDC dc, RECT client);
void mc_theme_glass(HDC dc, RECT rect, COLORREF tint, int radius);
void mc_theme_outline(HDC dc, RECT rect, COLORREF color, int radius);
void mc_theme_placeholder(HDC dc, RECT rect, unsigned number);
bool mc_theme_draw(const DRAWITEMSTRUCT *draw, HFONT font);
HBRUSH mc_theme_color(HDC dc);
