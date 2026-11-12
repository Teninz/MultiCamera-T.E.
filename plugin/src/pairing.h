#pragma once
#include <obs.h>
#include <stdbool.h>
void olc_pairing_init(void);
void olc_pairing_frontend_ready(void);
void olc_pairing_shutdown(void);
bool olc_pairing_available(void);
void olc_pairing_show(obs_source_t *source,const char *address,int port,const char *token,bool (*connected)(void *),void *context);

void *olc_pairing_owner(void);
