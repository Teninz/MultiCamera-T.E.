package ru.codex.obslancamera;
/** Pure affine geometry: TextureView's initial stretched coordinates -> uniform crop/fit. */
final class PreviewGeometry {
    static float[] matrix(int sourceW,int sourceH,int viewW,int viewH,int rotation,boolean mirror,boolean fill){
        if(sourceW<=0||sourceH<=0||viewW<=0||viewH<=0)return new float[]{1,0,0,0,1,0,0,0,1};
        boolean swap=rotation==90||rotation==270;float rw=swap?sourceH:sourceW,rh=swap?sourceW:sourceH;
        float scale=fill?Math.max(viewW/rw,viewH/rh):Math.min(viewW/rw,viewH/rh);
        float sx=(float)sourceW/viewW,sy=(float)sourceH/viewH;
        float a=1,b=0,d=0,e=1,ox=0,oy=0;
        if(rotation==90){a=0;b=-1;d=1;e=0;ox=sourceH;}
        else if(rotation==180){a=-1;e=-1;ox=sourceW;oy=sourceH;}
        else if(rotation==270){a=0;b=1;d=-1;e=0;oy=sourceW;}
        float[] m={a*sx*scale,b*sy*scale,ox*scale+(viewW-rw*scale)/2,d*sx*scale,e*sy*scale,oy*scale+(viewH-rh*scale)/2,0,0,1};
        if(mirror){m[0]=-m[0];m[1]=-m[1];m[2]=viewW-m[2];}return m;
    }
}
