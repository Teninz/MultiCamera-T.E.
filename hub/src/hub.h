#pragma once
#define WIN32_LEAN_AND_MEAN
#define COBJMACROS
#include <winsock2.h>
#include <windows.h>
#include <stdint.h>
#include <stdbool.h>
#define MC_SLOTS 5
#define MC_PORT 34872
#define MC_WIDTH 1280
#define MC_HEIGHT 720
#define MC_BYTES (MC_WIDTH*MC_HEIGHT*3/2)
#define MC_VERSION "0.4.3"
#define MC_TITLE L"MultiCamera T.E."
struct mc_frame {uint32_t magic,width,height,bytes;uint64_t sequence,timestamp_us;};
struct mc_audio {uint64_t sequence,timestamp_us;uint32_t rate,frames,bytes;uint8_t pcm[19200];};
struct mc_slot {CRITICAL_SECTION lock;char token[33],name[96],error[512];volatile LONG connected;uint64_t sequence,timestamp_us,frames,audio_frames,audio_sequence,last_audio_ms;unsigned input_width,input_height;uint8_t *pixels;HANDLE changed;struct mc_audio audio[16];};
struct mc_hub {struct mc_slot slot[MC_SLOTS];volatile LONG stopping,clients;CRITICAL_SECTION connections_lock;SOCKET connections[10],listener;HANDLE network_thread,clients_done;char address[64];wchar_t pipe_base[192];void *ipc;};
struct mc_decoder;
struct mc_decoder *mc_decoder_create(void);
bool mc_decoder_packet(struct mc_decoder *,const uint8_t *,size_t,uint64_t,int,struct mc_slot *);
void mc_decoder_destroy(struct mc_decoder *);
void mc_black(uint8_t *pixels);
bool mc_hub_start(struct mc_hub *,int port);
void mc_hub_stop(struct mc_hub *);
bool mc_hub_init(struct mc_hub *);
void mc_hub_free(struct mc_hub *);
bool mc_ipc_start(struct mc_hub *);
void mc_ipc_stop(struct mc_hub *);
