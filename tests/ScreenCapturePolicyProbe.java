package ru.codex.obslancamera;
public final class ScreenCapturePolicyProbe {
 public static void main(String[] args){
  for(int w:new int[]{2,480,720,1080,1440,2160})for(int h:new int[]{2,480,720,1920,2400,3840}){
   int[] size=ScreenCapturePolicy.size(w,h);if(size[0]%2!=0||size[1]%2!=0||Math.max(size[0],size[1])>1280||Math.min(size[0],size[1])>720)throw new AssertionError();
   if(w>=480&&h>=480&&Math.abs((double)size[0]/size[1]-(double)w/h)>.02)throw new AssertionError();
   int[] rotated=ScreenCapturePolicy.size(h,w);if(rotated[0]!=size[1]||rotated[1]!=size[0])throw new AssertionError();
  }
  if(!ScreenCapturePolicy.rateAllowed(240,240)||ScreenCapturePolicy.rateAllowed(240,120)||ScreenCapturePolicy.rateAllowed(120,60)||ScreenCapturePolicy.rateAllowed(300,360)||!ScreenCapturePolicy.rateAllowed(60,59.94f))throw new AssertionError();
  if(ScreenCapturePolicy.mbps(1000000,1000000000L)!=8||ScreenCapturePolicy.sendMs(5000000,10)!=.5||ScreenCapturePolicy.sendMs(1,0)!=0)throw new AssertionError();
  try{ScreenCapturePolicy.size(0,1080);throw new AssertionError();}catch(IllegalArgumentException expected){}
  System.out.println("SCREEN_CAPTURE_SIZE_ROTATION_PASS");
 }
}
