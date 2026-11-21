package ru.codex.obslancamera;
import android.content.Context;
import android.hardware.display.DisplayManager;
import android.media.*;
import android.os.Build;
import android.view.Display;
import java.util.*;
final class ScreenCaptureModes {
    static float displayMaximum(Context context){Display display=((DisplayManager)context.getSystemService(Context.DISPLAY_SERVICE)).getDisplay(Display.DEFAULT_DISPLAY);float max=30;if(display!=null)for(Display.Mode mode:display.getSupportedModes())max=Math.max(max,mode.getRefreshRate());return max;}
    static String encoder(int width,int height,int fps){return encoder(width,height,fps,java.util.Collections.emptySet());}
    static String encoder(int width,int height,int fps,java.util.Set<String> rejected){
        String fallback=null;
        for(MediaCodecInfo info:new MediaCodecList(MediaCodecList.REGULAR_CODECS).getCodecInfos()){
            if(!info.isEncoder()||Build.VERSION.SDK_INT>=29&&info.isAlias()||rejected.contains(info.getName()+"@"+width+"x"+height+"/"+fps))continue;
            try{MediaCodecInfo.CodecCapabilities caps=info.getCapabilitiesForType("video/avc");boolean surface=false;for(int color:caps.colorFormats)if(color==MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface)surface=true;
                if(!surface||!caps.getVideoCapabilities().areSizeAndRateSupported(width,height,fps))continue;
                if(Build.VERSION.SDK_INT>=29&&!info.isSoftwareOnly())return info.getName();if(fallback==null)fallback=info.getName();
            }catch(Exception ignored){}
        }return fallback;
    }
    static List<Integer> rates(Context context,int width,int height){
        List<Integer> result=new ArrayList<>();float max=displayMaximum(context);
        for(int rate:new int[]{15,24,30,48,60,90,120,144,165,240})if(ScreenCapturePolicy.rateAllowed(rate,max)&&encoder(width,height,rate)!=null&&encoder(height,width,rate)!=null)result.add(rate);
        return result;
    }
}
