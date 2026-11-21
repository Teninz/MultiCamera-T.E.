package ru.codex.obslancamera;
/** Bounded, even H.264 dimensions with the captured content's aspect ratio. */
final class ScreenCapturePolicy {
    static double mbps(long bytes,long nanos){return nanos>0?bytes*8000.0/nanos:0;}
    static double sendMs(long nanos,int frames){return frames>0?nanos/1000000.0/frames:0;}
    static boolean rateAllowed(int fps,float displayMaximum){return fps>=15&&fps<=240&&fps<=displayMaximum+.5f;}
    static int[] size(int width,int height){
        if(width<2||height<2)throw new IllegalArgumentException("Invalid capture size");
        double scale=Math.min(1.0,Math.min(1280.0/Math.max(width,height),720.0/Math.min(width,height)));
        return new int[]{Math.max(2,((int)(width*scale))&~1),Math.max(2,((int)(height*scale))&~1)};
    }
}
