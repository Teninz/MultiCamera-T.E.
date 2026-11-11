#include <obs.h>
#include <util/platform.h>
#include <windows.h>
#include <stdio.h>
#include <stdatomic.h>
static atomic_uint received, quadrants,audio_frames,audio_nonzero;
static obs_source_t *source;
static void receive_audio(void *unused,obs_source_t *s,const struct audio_data *audio,bool muted)
{
 (void)unused;(void)s;(void)muted;atomic_fetch_add(&audio_frames,audio->frames);
 const float *samples=(const float*)audio->data[0];for(uint32_t i=0;i<audio->frames;i++)if(samples[i]>.05f||samples[i]<-.05f){atomic_fetch_add(&audio_nonzero,1);break;}
}
static void receive(void *unused,struct video_data *v)
{
 (void)unused;
 if(obs_source_get_width(source)!=480 || obs_source_get_height(source)!=640)return;
 unsigned tl=v->data[0][160*v->linesize[0]+120],tr=v->data[0][160*v->linesize[0]+360];
 unsigned bl=v->data[0][480*v->linesize[0]+120],br=v->data[0][480*v->linesize[0]+360];
 if(tl<60 && tr>60 && br>tr && bl>br){atomic_store(&quadrants,tl|(tr<<8)|(bl<<16)|(br<<24));atomic_fetch_add(&received,1);}
}
int main(int argc,char **argv)
{
 if(argc!=6)return 2;if(!obs_startup("en-US",argv[5],NULL))return 3;obs_add_data_path(argv[3]);
 struct obs_audio_info ai={.samples_per_sec=48000,.speakers=SPEAKERS_STEREO};if(!obs_reset_audio(&ai))return 8;
 struct obs_video_info info={.graphics_module=argv[4],.fps_num=30,.fps_den=1,.base_width=480,.base_height=640,.output_width=480,.output_height=640,.output_format=VIDEO_FORMAT_NV12,.adapter=0,.gpu_conversion=true,.colorspace=VIDEO_CS_709,.range=VIDEO_RANGE_PARTIAL,.scale_type=OBS_SCALE_BICUBIC};
 if(obs_reset_video(&info)!=OBS_VIDEO_SUCCESS)return 4;obs_module_t *module=NULL;
 if(obs_open_module(&module,argv[1],argv[2])!=MODULE_SUCCESS||!obs_init_module(module))return 5;
 obs_data_t *settings=obs_data_create();obs_data_set_int(settings,"port",34879);obs_data_set_string(settings,"token","0123456789abcdef0123456789abcdef");
 source=obs_source_create("obs_lan_camera","test phone",settings,NULL);if(!source)return 6;
 obs_source_add_audio_capture_callback(source,receive_audio,NULL);
 struct video_scale_info scale={.format=VIDEO_FORMAT_NV12,.width=480,.height=640,.range=VIDEO_RANGE_PARTIAL,.colorspace=VIDEO_CS_709};obs_add_raw_video_callback(&scale,receive,NULL);obs_set_output_source(0,source);
 puts("RECEIVER_READY");fflush(stdout);Sleep(10000);
 unsigned count=atomic_load(&received),q=atomic_load(&quadrants);
 printf("RECEIVER_TEST frames=%u quadrants=%u,%u,%u,%u audio_frames=%u audio_nonzero=%u\n",count,q&255,(q>>8)&255,(q>>16)&255,(q>>24)&255,atomic_load(&audio_frames),atomic_load(&audio_nonzero));
 obs_set_output_source(0,NULL);obs_remove_raw_video_callback(receive,NULL);obs_source_remove_audio_capture_callback(source,receive_audio,NULL);obs_source_release(source);obs_data_release(settings);obs_shutdown();return count>20&&atomic_load(&audio_frames)>20000&&atomic_load(&audio_nonzero)>20?0:7;
}
