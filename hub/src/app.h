#pragma once
#include "hub.h"
#include <mfapi.h>
#include <mfidl.h>
#include "vcam.h"
struct mc_config {uint32_t magic,version,language,internet;char tokens[5][33],names[5][96],room[33];wchar_t relay[512],host_key[256];uint32_t animation;};
struct mc_app {struct mc_hub hub;struct mc_config config;struct mc_virtual_camera *cameras[5];volatile LONG camera_status,closing;HRESULT camera_error;HANDLE camera_thread,camera_stop;void *relay;};
bool mc_config_load(struct mc_config *config);
bool mc_config_save(const struct mc_config *config);
bool mc_register_source(const wchar_t *dll,const wchar_t *pipe_base);
void mc_cameras_start(struct mc_app *app);
void mc_cameras_stop(struct mc_app *app);
void mc_relay_start(struct mc_app *app);
void mc_relay_stop(struct mc_app *app);
int mc_relay_status(struct mc_app *app);
