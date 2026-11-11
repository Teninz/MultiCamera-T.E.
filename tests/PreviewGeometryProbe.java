package ru.codex.obslancamera;
/** Checks the visible result in pixel coordinates, including TextureView's initial stretch. */
public class PreviewGeometryProbe {
 static void near(double a,double b){if(Math.abs(a-b)>.01)throw new AssertionError(a+" != "+b);}
 public static void main(String[] args){int count=0;
  for(int[] source:new int[][]{{1920,1080},{640,480},{3840,2160}})
  for(int[] view:new int[][]{{1080,2400},{2400,1080},{720,1280},{1280,720}})
  for(int angle:new int[]{0,90,180,270})for(boolean mirror:new boolean[]{false,true})for(boolean fill:new boolean[]{false,true}){
   int sw=source[0],sh=source[1],vw=view[0],vh=view[1];float[] m=PreviewGeometry.matrix(sw,sh,vw,vh,angle,mirror,fill);
   // One source pixel along each axis must have the same scale after the initial stretch.
   double scaleX=Math.hypot(m[0]*(double)vw/sw,m[3]*(double)vw/sw),scaleY=Math.hypot(m[1]*(double)vh/sh,m[4]*(double)vh/sh);near(scaleX,scaleY);
   double minX=Double.POSITIVE_INFINITY,minY=minX,maxX=-minX,maxY=-minX;
   for(int x:new int[]{0,vw})for(int y:new int[]{0,vh}){double px=m[0]*x+m[1]*y+m[2],py=m[3]*x+m[4]*y+m[5];minX=Math.min(minX,px);maxX=Math.max(maxX,px);minY=Math.min(minY,py);maxY=Math.max(maxY,py);}
   near((minX+maxX)/2,vw/2.0);near((minY+maxY)/2,vh/2.0);
   if(fill?(minX>.01||minY>.01||maxX<vw-.01||maxY<vh-.01):(minX<-.01||minY<-.01||maxX>vw+.01||maxY>vh+.01))throw new AssertionError("Wrong crop/fit");count++;
  }System.out.println("PREVIEW_GEOMETRY_PASS "+count+" combinations; uniform proportions, centered crop/fit");
 }
}
