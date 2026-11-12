#pragma once
#include <winsock2.h>
#include <windows.h>
#include <stdbool.h>
struct mc_usb_adapter {ULONG index;char address[64];wchar_t subnet[80],name[256];};
bool mc_usb_description(const wchar_t *description);
int mc_usb_adapters(struct mc_usb_adapter *adapters,int capacity);
