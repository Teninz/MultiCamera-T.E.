package ru.codex.obslancamera;
import android.hardware.camera2.*;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.media.*;
import android.graphics.SurfaceTexture;
import android.os.Build;
import android.util.Size;
import java.util.*;
final class CameraModes {
    static final long MAX_PIXELS=33177600L;
    static final class Mode {
        final int width,height,fps;
        Mode(int w,int h,int f){width=w;height=h;fps=f;}
        String label(){return width+" × "+height+" · "+fps+Language.text(" кадров/с");}
        String shortLabel(){String prefix=width==3840&&height==2160?"4K":height==1080?"FHD":height==720?"HD":width+"×"+height;return prefix+" · "+fps;}
    }
    static boolean supportsFps(CameraCharacteristics camera,int fps){
        android.util.Range<Integer>[] ranges=camera.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);
        if(ranges==null)return fps<=30;int[][] values=new int[ranges.length][2];for(int i=0;i<ranges.length;i++){values[i][0]=ranges[i].getLower();values[i][1]=ranges[i].getUpper();}return CameraPolicy.fpsInRanges(fps,values);
    }
    static android.util.Range<Integer> fpsRange(CameraCharacteristics camera,int fps){
        android.util.Range<Integer>[] ranges=camera.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);android.util.Range<Integer> selected=null;
        if(ranges!=null)for(android.util.Range<Integer> r:ranges)if(r.contains(fps)&&(selected==null||Math.abs(r.getUpper()-fps)<Math.abs(selected.getUpper()-fps)||r.getUpper().equals(selected.getUpper())&&r.getLower()>selected.getLower()))selected=r;return selected;
    }
    static android.util.Range<Integer> highSpeedRange(CameraCharacteristics camera,Size size,int fps){
        StreamConfigurationMap map=camera.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        if(map==null||fps<=30)return null;
        try{android.util.Range<Integer> best=null;for(android.util.Range<Integer> range:map.getHighSpeedVideoFpsRangesFor(size))
            if(CameraPolicy.highSpeedRate(fps,range.getLower(),range.getUpper(),android.os.Build.VERSION.SDK_INT>=29)&&(best==null||range.getUpper()<best.getUpper()||range.getUpper().equals(best.getUpper())&&range.getLower()>best.getLower()))best=range;return best;
        }catch(IllegalArgumentException ignored){return null;}
    }
    static boolean highSpeed(CameraCharacteristics camera,int width,int height,int fps){return !supportsFps(camera,fps)&&highSpeedRange(camera,new Size(width,height),fps)!=null;}
    static String diagnostic(CameraCharacteristics camera){
        StreamConfigurationMap map=camera.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);StringBuilder text=new StringBuilder("AE="+Arrays.toString(camera.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES)));
        if(map!=null)try{for(Size size:map.getHighSpeedVideoSizes())text.append("; high-speed ").append(size).append("=").append(Arrays.toString(map.getHighSpeedVideoFpsRangesFor(size)));}catch(Exception ignored){}
        return text.toString();
    }
    static boolean sizeSupportsFps(StreamConfigurationMap map,Size size,int fps){
        try{long camera=map.getOutputMinFrameDuration(MediaCodec.class,size),preview=map.getOutputMinFrameDuration(SurfaceTexture.class,size);return Math.max(camera,preview)<=1000000000L/fps+1000000L;}catch(Exception ignored){return fps<=30;}
    }
    static List<Mode> discover(CameraCharacteristics camera){
        List<Mode> modes=new ArrayList<>();StreamConfigurationMap map=camera.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        if(map==null)return modes;Size[] sizes=map.getOutputSizes(MediaCodec.class),previews=map.getOutputSizes(SurfaceTexture.class);if(sizes==null||previews==null)return modes;
        MediaCodecInfo[] codecs=new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos();
        for(Size s:sizes){if(s.getWidth()<320||s.getWidth()>8192||s.getHeight()>8192||(long)s.getWidth()*s.getHeight()>MAX_PIXELS)continue;
            boolean preview=false;for(Size p:previews)if(p.equals(s))preview=true;if(!preview)continue;
            for(int fps:new int[]{60,30,24,15}){
                boolean fast=highSpeed(camera,s.getWidth(),s.getHeight(),fps);
                if(!supportsFps(camera,fps)&&!fast)continue;
                if(!fast&&!sizeSupportsFps(map,s,fps))continue;
                boolean valid=false;
                for(MediaCodecInfo info:codecs){if(!info.isEncoder()||(Build.VERSION.SDK_INT>=29&&info.isAlias()))continue;
                    try{MediaCodecInfo.CodecCapabilities cap=info.getCapabilitiesForType("video/avc");boolean surface=false;for(int color:cap.colorFormats)if(color==MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)surface=true;
                        if(surface&&cap.getVideoCapabilities().areSizeAndRateSupported(s.getWidth(),s.getHeight(),fps)){valid=true;break;}}
                    catch(Exception ignored){}
                }
                if(valid)modes.add(new Mode(s.getWidth(),s.getHeight(),fps));
            }
        }
        modes.sort((a,b)->{int pixels=Long.compare((long)b.width*b.height,(long)a.width*a.height);return pixels!=0?pixels:Integer.compare(b.fps,a.fps);});return modes;
    }
    static Mode recommended(List<Mode> modes){
        Mode best=null;double score=Double.MAX_VALUE;
        for(Mode mode:modes){double s=Math.abs(Math.log((double)mode.width*mode.height/(1280*720)))+Math.abs(mode.fps-30)*.08+Math.abs((double)mode.width/mode.height-16.0/9);
            if(s<score){score=s;best=mode;}}
        return best==null?new Mode(640,480,30):best;
    }
    static Mode stabilized(List<Mode> modes){
        Mode best=null;
        for(Mode mode:modes)if(mode.width<=1920&&mode.height<=1080&&mode.fps==30&&
            (best==null||(long)mode.width*mode.height>(long)best.width*best.height))best=mode;
        return best;
    }
}
