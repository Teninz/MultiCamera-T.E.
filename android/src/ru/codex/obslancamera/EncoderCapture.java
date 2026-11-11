package ru.codex.obslancamera;

import android.graphics.ImageFormat;
import android.graphics.SurfaceTexture;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.*;
import android.os.*;
import android.util.Size;
import android.view.Surface;
import java.nio.ByteBuffer;
import java.util.*;
import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Consumer;

/** Negotiates real camera/codec capabilities. Optional vendor-sensitive keys are not forced. */
final class EncoderCapture implements AutoCloseable {
    final MediaCodec codec;
    final Surface surface;
    final int width,height,fps;
    final String name,key;
    final boolean bufferInput;
    private final ImageReader reader;
    private volatile boolean closed;

    static final class Failure extends Exception {
        final String key;
        Failure(String key,String message,Throwable cause){super(message,cause);this.key=key;}
    }
    static final class Unavailable extends Exception { Unavailable(String text){super(text);} }
    private EncoderCapture(MediaCodec c,Surface s,ImageReader r,int w,int h,int rate,String n,String k){codec=c;surface=s;reader=r;width=w;height=h;fps=rate;name=n;key=k;bufferInput=r!=null;}
    static String diagnostic(Throwable error){
        if(error instanceof MediaCodec.CodecException){MediaCodec.CodecException e=(MediaCodec.CodecException)error;return String.format(Locale.ROOT,"MediaCodec 0x%08x (%s)",e.getErrorCode(),e.getDiagnosticInfo());}
        return error.getClass().getSimpleName()+": "+String.valueOf(error.getMessage());
    }
    private static boolean software(MediaCodecInfo info){return Build.VERSION.SDK_INT>=29?info.isSoftwareOnly():info.getName().startsWith("OMX.google.")||info.getName().startsWith("c2.android.");}
    private static boolean has(int[] values,int target){for(int v:values)if(v==target)return true;return false;}

    static EncoderCapture open(CameraCharacteristics camera,CameraModes.Mode requested,Set<String> rejected,Consumer<String> log) throws Exception {
        StreamConfigurationMap map=camera.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        if(map==null)throw new Unavailable("Камера не сообщила поддерживаемые режимы");
        if(requested==null)requested=CameraModes.recommended(CameraModes.discover(camera));
        int maxW=requested.width,maxH=requested.height,targetFps=requested.fps;
        List<MediaCodecInfo> encoders=new ArrayList<>();
        for(MediaCodecInfo info:new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos()){
            if(!info.isEncoder()||(Build.VERSION.SDK_INT>=29&&info.isAlias()))continue;
            for(String type:info.getSupportedTypes())if(type.equalsIgnoreCase("video/avc")){encoders.add(info);break;}
        }
        encoders.sort(Comparator.comparingInt(i->software(i)?1:0));
        String last="";
        // Prefer the fast Surface path, then a small YUV-buffer/software fallback.
        for(boolean buffered:new boolean[]{false,true}){
            Size[] supported=buffered?map.getOutputSizes(ImageFormat.YUV_420_888):map.getOutputSizes(MediaCodec.class);
            Size[] previews=map.getOutputSizes(SurfaceTexture.class);
            if(supported==null||previews==null)continue;
            List<Size> sizes=new ArrayList<>();
            for(Size size:supported){
                if(size.getWidth()>maxW||size.getHeight()>maxH||size.getWidth()<320||(buffered&&(size.getWidth()>640||size.getHeight()>480)))continue;
                boolean preview=false;for(Size p:previews)if(p.equals(size))preview=true;
                if(preview&&!sizes.contains(size))sizes.add(size);
            }
            sizes.sort(Comparator.comparingDouble(s->Math.abs(Math.log((double)s.getWidth()*s.getHeight()/(maxW*maxH)))+Math.abs((double)s.getWidth()/s.getHeight()-(double)maxW/maxH)*3));
            for(Size size:sizes)for(MediaCodecInfo info:encoders){
                if(Thread.currentThread().isInterrupted())throw new InterruptedException();
                try{
                    MediaCodecInfo.CodecCapabilities caps=info.getCapabilitiesForType("video/avc");
                    int color=buffered?MediaCodecInfo.CodecCapabilities.COLOR_FormatYUV420Flexible:MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface;
                    if(!has(caps.colorFormats,color))continue;
                    MediaCodecInfo.VideoCapabilities video=caps.getVideoCapabilities();int w=size.getWidth(),h=size.getHeight();
                    int rate=0;for(int test:new int[]{buffered?15:targetFps,30,24,15})if(test<=targetFps&&(!buffered||test<=15)&&CameraModes.supportsFps(camera,test)&&(buffered||CameraModes.sizeSupportsFps(map,size,test))&&video.areSizeAndRateSupported(w,h,test)){rate=test;break;}
                    if(rate==0)continue;
                    String key=info.getName()+"@"+w+"x"+h+"/"+rate+(buffered?"/buffer":"/surface");
                    if(rejected.contains(key))continue;
                    MediaCodec codec=null;Surface input=null;ImageReader reader=null;
                    try{
                        log.accept("Пробуем "+key);
                        codec=MediaCodec.createByCodecName(info.getName());
                        MediaFormat format=MediaFormat.createVideoFormat("video/avc",w,h);
                        format.setInteger(MediaFormat.KEY_COLOR_FORMAT,color);
                        int desired=Math.max(500000,(int)Math.min(40000000L,(long)w*h*rate/10));
                        format.setInteger(MediaFormat.KEY_BIT_RATE,video.getBitrateRange().clamp(desired));
                        format.setInteger(MediaFormat.KEY_FRAME_RATE,rate);
                        format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL,1);
                        codec.configure(format,null,null,MediaCodec.CONFIGURE_FLAG_ENCODE);
                        if(!buffered)input=codec.createInputSurface();
                        codec.start();
                        if(buffered){reader=ImageReader.newInstance(w,h,ImageFormat.YUV_420_888,2);input=reader.getSurface();}
                        log.accept("Запущен "+key);
                        return new EncoderCapture(codec,input,reader,w,h,rate,info.getName(),key);
                    }catch(Exception error){
                        rejected.add(key);last=key+": "+diagnostic(error);log.accept(last);
                        if(reader!=null)reader.close();else if(input!=null)input.release();
                        if(codec!=null)try{codec.release();}catch(Exception ignored){}
                    }
                }catch(Exception error){last=diagnostic(error);}
            }
        }
        throw new Unavailable("Не удалось запустить кодировщик камеры. "+last+". Откройте «Диагностика».");
    }
    void receiveImages(Handler handler,AtomicReference<Exception> failure){
        if(reader==null)return;
        reader.setOnImageAvailableListener(r->{
            Image source=null;
            try{
                source=r.acquireLatestImage();if(source==null||closed)return;
                int index=codec.dequeueInputBuffer(0);if(index<0)return;
                Image target=codec.getInputImage(index);
                if(target==null)throw new IllegalStateException("Кодировщик не предоставил YUV input Image");
                Image.Plane[] src=source.getPlanes(),dst=target.getPlanes();
                for(int p=0;p<3;p++){
                    int w=p==0?width:width/2,h=p==0?height:height/2;
                    ByteBuffer from=src[p].getBuffer().duplicate(),to=dst[p].getBuffer().duplicate();int fromBase=from.position(),toBase=to.position();
                    for(int y=0;y<h;y++)for(int x=0;x<w;x++)to.put(toBase+y*dst[p].getRowStride()+x*dst[p].getPixelStride(),from.get(fromBase+y*src[p].getRowStride()+x*src[p].getPixelStride()));
                }
                codec.queueInputBuffer(index,0,width*height*3/2,source.getTimestamp()/1000,0);
            }catch(Exception error){if(!closed)failure.compareAndSet(null,new Failure(key,"Ошибка YUV-кодирования: "+diagnostic(error),error));}
            finally{if(source!=null)source.close();}
        },handler);
    }
    public void close(){
        closed=true;if(reader!=null){reader.setOnImageAvailableListener(null,null);reader.close();}
        try{codec.stop();}catch(Exception ignored){}try{codec.release();}catch(Exception ignored){}
        if(reader==null)surface.release();
    }
}
