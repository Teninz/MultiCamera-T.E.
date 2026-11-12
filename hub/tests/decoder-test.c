#include "../src/hub.h"
#include <mfapi.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv){if(argc!=2)return 2;CoInitializeEx(NULL,COINIT_MULTITHREADED);if(FAILED(MFStartup(MF_VERSION,MFSTARTUP_FULL)))return 3;
 struct mc_slot slot={0};InitializeCriticalSection(&slot.lock);slot.pixels=malloc(MC_BYTES);slot.changed=CreateEventW(NULL,FALSE,FALSE,NULL);mc_black(slot.pixels);struct mc_decoder *decoder=mc_decoder_create();if(!decoder){puts("DECODER_CREATE_FAILED");return 4;}
 FILE *file=fopen(argv[1],"rb");if(!file)return 5;uint32_t size;int sent=0;while(fread(&size,4,1,file)==1){size=ntohl(size);if(size>4*1024*1024)return 6;uint8_t *p=malloc(size);if(fread(p,1,size,file)!=size)return 7;if(!mc_decoder_packet(decoder,p,size,(uint64_t)sent*33333,90,&slot)){printf("DECODE_FAILED packet=%d\n",sent);return 8;}free(p);sent++;}
 fclose(file);unsigned tl=slot.pixels[180*MC_WIDTH+520],tr=slot.pixels[180*MC_WIDTH+760],bl=slot.pixels[540*MC_WIDTH+520],br=slot.pixels[540*MC_WIDTH+760];printf("MF_DECODER frames=%llu input=%ux%u quadrants=%u,%u,%u,%u\n",(unsigned long long)slot.frames,slot.input_width,slot.input_height,tl,tr,bl,br);
 bool ok=slot.frames>20&&slot.input_width==480&&slot.input_height==640&&tl<60&&tr>60&&bl>br&&br>tr;mc_decoder_destroy(decoder);CloseHandle(slot.changed);DeleteCriticalSection(&slot.lock);free(slot.pixels);MFShutdown();CoUninitialize();return ok?0:9;
}
