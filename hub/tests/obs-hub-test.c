/* Real libobs consumer: five independent sources, decoded quadrants and PCM. */
#include <obs.h>
#include <util/platform.h>
#include <windows.h>
#include <stdio.h>
#include <stdatomic.h>
struct probe {unsigned slot;atomic_uint frames,audio,signal;};
static struct probe probes[5];
static void probe_destroy(void *data){(void)data;}
static const char *probe_name(void *unused){return "MultiCamera test probe";}
static void *probe_create(obs_data_t *data,obs_source_t *source){return &probes[obs_data_get_int(data,"slot")];}
static struct obs_source_frame *probe_frame(void *data,struct obs_source_frame *frame){
 struct probe *p=data;if(frame->format!=VIDEO_FORMAT_NV12||frame->width!=1280||frame->height!=720)return frame;
 unsigned id=p->slot,x1=(id==1||id==3)?505:400,x2=(id==1||id==3)?775:880;
 unsigned got[4]={frame->data[0][180*frame->linesize[0]+x1],frame->data[0][180*frame->linesize[0]+x2],frame->data[0][540*frame->linesize[0]+x1],frame->data[0][540*frame->linesize[0]+x2]};
 const unsigned expected[4][4]={{81,145,41,235},{41,81,235,145},{235,41,145,81},{145,235,81,41}};bool ok=true;
 for(int i=0;i<4;i++)if(abs((int)got[i]-(int)expected[id%4][i])>12)ok=false;
 if(ok)atomic_fetch_add(&p->frames,1);return frame;
}
static void probe_audio(void *data,obs_source_t *source,const struct audio_data *audio,bool muted){
 struct probe *p=data;atomic_fetch_add(&p->audio,audio->frames);const float *samples=(const float*)audio->data[0];
 for(unsigned i=0;i<audio->frames;i++)if(samples[i]>.05f||samples[i]<-.05f){atomic_fetch_add(&p->signal,1);break;}
}
int main(int argc,char **argv){
 if(argc!=6)return 2;if(!obs_startup("en-US",argv[5],NULL))return 3;obs_add_data_path(argv[3]);
 struct obs_audio_info audio={.samples_per_sec=48000,.speakers=SPEAKERS_STEREO};if(!obs_reset_audio(&audio))return 4;
 struct obs_video_info video={.graphics_module=argv[4],.fps_num=30,.fps_den=1,.base_width=1920,.base_height=1080,.output_width=1920,.output_height=1080,.output_format=VIDEO_FORMAT_NV12,.adapter=0,.gpu_conversion=true,.colorspace=VIDEO_CS_709,.range=VIDEO_RANGE_PARTIAL,.scale_type=OBS_SCALE_BICUBIC};if(obs_reset_video(&video)!=OBS_VIDEO_SUCCESS)return 5;
 obs_module_t *module=NULL;if(obs_open_module(&module,argv[1],argv[2])!=MODULE_SUCCESS||!obs_init_module(module))return 6;
 struct obs_source_info filter={.id="multicamera_test_probe",.type=OBS_SOURCE_TYPE_FILTER,.output_flags=OBS_SOURCE_ASYNC_VIDEO,.get_name=probe_name,.create=probe_create,.destroy=probe_destroy,.filter_video=probe_frame};obs_register_source(&filter);
 obs_scene_t *scene=obs_scene_create("MultiCamera test scene");obs_source_t *sources[5]={0};
 for(unsigned i=0;i<5;i++){
  probes[i].slot=i;obs_data_t *settings=obs_data_create();obs_data_set_int(settings,"device",i+1);char name[64];snprintf(name,sizeof(name),"Test camera %u",i+1);sources[i]=obs_source_create("multicamera_te",name,settings,NULL);if(!sources[i])return 7;
  obs_data_set_int(settings,"slot",i);obs_source_t *probe=obs_source_create("multicamera_test_probe",name,settings,NULL);obs_source_filter_add(sources[i],probe);obs_source_release(probe);obs_data_release(settings);obs_source_add_audio_capture_callback(sources[i],probe_audio,&probes[i]);
  obs_sceneitem_t *item=obs_scene_add(scene,sources[i]);struct vec2 scale={.x=.48f,.y=.48f},position={.x=(float)(i%3)*640,.y=(float)(i/3)*540};obs_sceneitem_set_scale(item,&scale);obs_sceneitem_set_pos(item,&position);
 }
 obs_set_output_source(0,obs_scene_get_source(scene));puts("OBS_HUB_READY");fflush(stdout);Sleep(6000);bool ok=true;
 for(int i=0;i<5;i++){unsigned frames=atomic_load(&probes[i].frames),pcm=atomic_load(&probes[i].audio),signal=atomic_load(&probes[i].signal);printf("OBS_SLOT_%d video=%u pcm=%u signal=%u\n",i+1,frames,pcm,signal);if(frames<20||pcm<16000||signal<8)ok=false;obs_source_remove_audio_capture_callback(sources[i],probe_audio,&probes[i]);}
 obs_set_output_source(0,NULL);obs_scene_release(scene);for(int i=0;i<5;i++)obs_source_release(sources[i]);obs_wait_for_destroy_queue();obs_shutdown();return ok?0:8;
}
