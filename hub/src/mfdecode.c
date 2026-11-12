/* Native Windows H.264 decoder; no OBS or FFmpeg runtime dependency. GPL-2.0-or-later. */
#include "hub.h"
#include <mfapi.h>
#include <mfidl.h>
#include <mftransform.h>
#include <mferror.h>
#include <wmcodecdsp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct mc_decoder {IMFTransform *mft;unsigned width,height;LONG stride;bool output;};
static bool output_type(struct mc_decoder *d);
void mc_black(uint8_t *pixels){memset(pixels,16,MC_WIDTH*MC_HEIGHT);memset(pixels+MC_WIDTH*MC_HEIGHT,128,MC_WIDTH*MC_HEIGHT/2);}
struct bits {const uint8_t *p;size_t bytes,pos;bool bad;};
static unsigned bit(struct bits *b){if(b->pos>=b->bytes*8){b->bad=true;return 0;}unsigned n=(b->p[b->pos/8]>>(7-b->pos%8))&1;b->pos++;return n;}
static unsigned bits(struct bits *b,int n){unsigned v=0;while(n-->0)v=(v<<1)|bit(b);return v;}
static unsigned ue(struct bits *b){int zeros=0;while(!b->bad&&!bit(b)){if(++zeros>30){b->bad=true;return 0;}}return ((1u<<zeros)-1)+bits(b,zeros);}
static int se(struct bits *b){unsigned n=ue(b);return n&1?(int)(n+1)/2:-(int)n/2;}
static bool sps_size(const uint8_t *data,size_t count,unsigned *width,unsigned *height){
 for(size_t i=0;i+5<count;i++){size_t n=0;if(data[i]==0&&data[i+1]==0&&data[i+2]==1)n=i+3;else if(i+5<count&&data[i]==0&&data[i+1]==0&&data[i+2]==0&&data[i+3]==1)n=i+4;if(!n||(data[n]&31)!=7)continue;
  uint8_t rbsp[1024];size_t end=n+1;while(end<count&&end<n+sizeof(rbsp)&&!(end+3<count&&data[end]==0&&data[end+1]==0&&(data[end+2]==1||(data[end+2]==0&&data[end+3]==1))))end++;
  size_t used=0;int zeros=0;for(size_t p=n+1;p<end&&used<sizeof(rbsp);p++){if(zeros>=2&&data[p]==3){zeros=0;continue;}rbsp[used++]=data[p];zeros=data[p]==0?zeros+1:0;}
  struct bits b={rbsp,used,0,false};unsigned profile=bits(&b,8);bits(&b,16);ue(&b);unsigned chroma=1;
  if(profile==100||profile==110||profile==122||profile==244||profile==44||profile==83||profile==86||profile==118||profile==128||profile==138||profile==139||profile==134||profile==135){chroma=ue(&b);if(chroma==3)bit(&b);ue(&b);ue(&b);bit(&b);if(bit(&b)){int lists=chroma==3?12:8;for(int l=0;l<lists;l++)if(bit(&b)){int last=8,next=8;for(int k=0;k<(l<6?16:64);k++){if(next)next=(last+se(&b)+256)%256;last=next?next:last;}}}}
  ue(&b);unsigned order=ue(&b);if(order==0)ue(&b);else if(order==1){bit(&b);se(&b);se(&b);unsigned cycle=ue(&b);if(cycle>256)return false;for(unsigned k=0;k<cycle;k++)se(&b);}ue(&b);bit(&b);
  unsigned w=ue(&b)+1,h=ue(&b)+1,frame=bit(&b);if(!frame)bit(&b);bit(&b);unsigned left=0,right=0,top=0,bottom=0;if(bit(&b)){left=ue(&b);right=ue(&b);top=ue(&b);bottom=ue(&b);}
  unsigned cropx=chroma==0||chroma==3?1:2,cropy=(chroma==1?2:1)*(2-frame);if(b.bad||w>512||h>512)return false;
  unsigned rw=w*16,rh=h*16*(2-frame);if((uint64_t)(left+right)*cropx>=rw||(uint64_t)(top+bottom)*cropy>=rh)return false;rw-=(left+right)*cropx;rh-=(top+bottom)*cropy;if(rw<16||rh<16||(rw&1)||(rh&1)||(uint64_t)rw*rh>33177600)return false;*width=rw;*height=rh;return true;
 }return false;
}
static bool configure(struct mc_decoder *d,unsigned width,unsigned height){
 IMFMediaType *type=NULL;HRESULT hr=MFCreateMediaType(&type);if(FAILED(hr))return false;
 IMFMediaType_SetGUID(type,&MF_MT_MAJOR_TYPE,&MFMediaType_Video);IMFMediaType_SetGUID(type,&MF_MT_SUBTYPE,&MFVideoFormat_H264);IMFMediaType_SetUINT64(type,&MF_MT_FRAME_SIZE,((uint64_t)width<<32)|height);IMFMediaType_SetUINT64(type,&MF_MT_FRAME_RATE,((uint64_t)30<<32)|1);IMFMediaType_SetUINT32(type,&MF_MT_INTERLACE_MODE,MFVideoInterlace_Progressive);
 hr=IMFTransform_SetInputType(d->mft,0,type,0);IMFMediaType_Release(type);if(FAILED(hr))return false;d->width=width;d->height=height;d->output=false;
 if(!output_type(d))return false;IMFTransform_ProcessMessage(d->mft,MFT_MESSAGE_NOTIFY_BEGIN_STREAMING,0);IMFTransform_ProcessMessage(d->mft,MFT_MESSAGE_NOTIFY_START_OF_STREAM,0);return true;
}
static bool output_type(struct mc_decoder *d){
 for(DWORD index=0;index<64;index++){IMFMediaType *type=NULL;HRESULT hr=IMFTransform_GetOutputAvailableType(d->mft,0,index,&type);if(FAILED(hr))break;GUID subtype;hr=IMFMediaType_GetGUID(type,&MF_MT_SUBTYPE,&subtype);
  if(SUCCEEDED(hr)&&IsEqualGUID(&subtype,&MFVideoFormat_NV12)&&SUCCEEDED(IMFTransform_SetOutputType(d->mft,0,type,0))){UINT64 size=0;UINT32 stride=0;IMFMediaType_GetUINT64(type,&MF_MT_FRAME_SIZE,&size);IMFMediaType_GetUINT32(type,&MF_MT_DEFAULT_STRIDE,&stride);d->width=(unsigned)(size>>32);d->height=(unsigned)size;d->stride=stride?(LONG)stride:(LONG)d->width;d->output=true;IMFMediaType_Release(type);return true;}IMFMediaType_Release(type);
 }return false;
}
static void normalize(struct mc_decoder *d,const uint8_t *src,DWORD length,int rotation,struct mc_slot *slot,uint64_t pts){
 unsigned w=d->width,h=d->height,stride=d->stride>0?(unsigned)d->stride:w;if(!w||!h||stride<w||length<(uint64_t)stride*h*3/2)return;
 unsigned rw=rotation==90||rotation==270?h:w,rh=rotation==90||rotation==270?w:h;
 double scale=(double)MC_WIDTH/rw;if((double)MC_HEIGHT/rh<scale)scale=(double)MC_HEIGHT/rh;
 unsigned dw=((unsigned)(rw*scale))&~1u,dh=((unsigned)(rh*scale))&~1u;if(!dw||!dh)return;unsigned ox=((MC_WIDTH-dw)/2)&~1u,oy=((MC_HEIGHT-dh)/2)&~1u;
 uint8_t *out=malloc(MC_BYTES);if(!out)return;mc_black(out);
 for(unsigned y=0;y<dh;y++)for(unsigned x=0;x<dw;x++){unsigned rx=(uint64_t)x*rw/dw,ry=(uint64_t)y*rh/dh,sx,sy;
  if(rotation==90){sx=ry;sy=h-1-rx;}else if(rotation==180){sx=w-1-rx;sy=h-1-ry;}else if(rotation==270){sx=w-1-ry;sy=rx;}else{sx=rx;sy=ry;}
  out[(oy+y)*MC_WIDTH+ox+x]=src[sy*stride+sx];
  if(!(x&1)&&!(y&1)){size_t uv=(size_t)stride*h+(sy/2)*stride+(sx&~1u),duv=MC_WIDTH*MC_HEIGHT+((oy+y)/2)*MC_WIDTH+ox+x;if(uv+1<length){out[duv]=src[uv];out[duv+1]=src[uv+1];}}
 }
 EnterCriticalSection(&slot->lock);memcpy(slot->pixels,out,MC_BYTES);slot->input_width=rw;slot->input_height=rh;slot->timestamp_us=pts;slot->sequence++;slot->frames++;LeaveCriticalSection(&slot->lock);SetEvent(slot->changed);free(out);
}
struct mc_decoder *mc_decoder_create(void){struct mc_decoder *d=calloc(1,sizeof(*d));if(!d)return NULL;HRESULT hr=CoCreateInstance(&CLSID_CMSH264DecoderMFT,NULL,CLSCTX_INPROC_SERVER,&IID_IMFTransform,(void**)&d->mft);if(FAILED(hr)){free(d);return NULL;}IMFAttributes *a=NULL;if(SUCCEEDED(IMFTransform_GetAttributes(d->mft,&a))){IMFAttributes_SetUINT32(a,&MF_LOW_LATENCY,TRUE);IMFAttributes_Release(a);}return d;}
bool mc_decoder_packet(struct mc_decoder *d,const uint8_t *data,size_t bytes,uint64_t pts,int rotation,struct mc_slot *slot){
 unsigned width=0,height=0;bool sps=sps_size(data,bytes,&width,&height);if(!d->width){if(!sps)return true;if(!configure(d,width,height))return false;}
 IMFSample *input=NULL;IMFMediaBuffer *buffer=NULL;BYTE *p=NULL;HRESULT hr=MFCreateSample(&input);if(SUCCEEDED(hr))hr=MFCreateMemoryBuffer((DWORD)bytes,&buffer);if(SUCCEEDED(hr))hr=IMFMediaBuffer_Lock(buffer,&p,NULL,NULL);if(SUCCEEDED(hr)){memcpy(p,data,bytes);IMFMediaBuffer_Unlock(buffer);IMFMediaBuffer_SetCurrentLength(buffer,(DWORD)bytes);IMFSample_AddBuffer(input,buffer);IMFSample_SetSampleTime(input,(LONGLONG)(pts*10));IMFSample_SetSampleDuration(input,333333);hr=IMFTransform_ProcessInput(d->mft,0,input,0);}if(buffer)IMFMediaBuffer_Release(buffer);if(input)IMFSample_Release(input);if(FAILED(hr)&&hr!=MF_E_NOTACCEPTING)return false;
 for(int repeat=0;repeat<32;repeat++){
  if(!d->output&&!output_type(d))return false;MFT_OUTPUT_STREAM_INFO info={0};if(FAILED(IMFTransform_GetOutputStreamInfo(d->mft,0,&info)))return false;
  IMFSample *sample=NULL;IMFMediaBuffer *output=NULL;if(!(info.dwFlags&MFT_OUTPUT_STREAM_PROVIDES_SAMPLES)){if(FAILED(MFCreateSample(&sample)))return false;DWORD capacity=info.cbSize;uint64_t minimum=(uint64_t)(d->stride>0?d->stride:(LONG)d->width)*d->height*3/2;if(minimum>64*1024*1024){IMFSample_Release(sample);return false;}if(capacity<minimum)capacity=(DWORD)minimum;if(FAILED(MFCreateMemoryBuffer(capacity,&output))){IMFSample_Release(sample);return false;}IMFSample_AddBuffer(sample,output);IMFMediaBuffer_Release(output);}
  MFT_OUTPUT_DATA_BUFFER result={0};result.pSample=sample;DWORD status=0;hr=IMFTransform_ProcessOutput(d->mft,0,1,&result,&status);if(result.pEvents)IMFCollection_Release(result.pEvents);
  if(hr==MF_E_TRANSFORM_STREAM_CHANGE){if(sample)IMFSample_Release(sample);d->output=false;continue;}if(hr==MF_E_TRANSFORM_NEED_MORE_INPUT){if(sample)IMFSample_Release(sample);return true;}if(FAILED(hr)){if(sample)IMFSample_Release(sample);return false;}
  if(result.pSample){LONGLONG time=pts*10;IMFSample_GetSampleTime(result.pSample,&time);if(SUCCEEDED(IMFSample_ConvertToContiguousBuffer(result.pSample,&output))){DWORD length=0;if(SUCCEEDED(IMFMediaBuffer_Lock(output,&p,NULL,&length))){normalize(d,p,length,rotation,slot,time>=0?(uint64_t)time/10:pts);IMFMediaBuffer_Unlock(output);}IMFMediaBuffer_Release(output);}IMFSample_Release(result.pSample);}else if(sample)IMFSample_Release(sample);
 }return true;
}
void mc_decoder_destroy(struct mc_decoder *d){if(d){if(d->mft)IMFTransform_Release(d->mft);free(d);}}
