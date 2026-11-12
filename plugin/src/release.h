#pragma once
#define OLC_VERSION "0.4.2"
#define OLC_REPOSITORY "Teninz/obs-lan-camera"
#define OLC_RELEASE_PAGE "https://github.com/" OLC_REPOSITORY "/releases/latest"
#define OLC_APK_URL "https://github.com/" OLC_REPOSITORY "/releases/download/v" OLC_VERSION "/OBS-LAN-Camera-Android.apk"
#define OLC_INSTALLER_NAME "OBS-LAN-Camera-Setup.exe"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
static inline bool olc_version_parse(const char *s,uint32_t parts[3])
{
    if(!s)return false;if(*s=='v')s++;
    for(int i=0;i<3;i++){
        if(!isdigit((unsigned char)*s))return false;
        uint32_t n=0;int digits=0;
        do{if(++digits>6)return false;n=n*10+(*s++-'0');}while(isdigit((unsigned char)*s));
        parts[i]=n;if(i<2){if(*s++!='.')return false;}else if(*s)return false;
    }return true;
}
static inline bool olc_newer(const char *candidate,const char *current)
{
    uint32_t a[3],b[3];if(!olc_version_parse(candidate,a)||!olc_version_parse(current,b))return false;
    for(int i=0;i<3;i++){if(a[i]!=b[i])return a[i]>b[i];}return false;
}
static inline bool olc_asset_url(const char *url,const char *tag)
{
    char expected[300];uint32_t v[3];if(!olc_version_parse(tag,v))return false;
    int n=snprintf(expected,sizeof(expected),"https://github.com/" OLC_REPOSITORY "/releases/download/%s/" OLC_INSTALLER_NAME,tag);
    return n>0&&(size_t)n<sizeof(expected)&&!strcmp(url,expected);
}
static inline bool olc_sha256(const char *s)
{
    if(!s||strlen(s)!=71||strncmp(s,"sha256:",7))return false;
    for(int i=7;i<71;i++)if(!isxdigit((unsigned char)s[i]))return false;return true;
}
