package ru.codex.obslancamera;
/** Checks raw affine geometry AND Camera2's implicit producer transform. */
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
  }
  int cameraCases=0;
  for(int[] source:new int[][]{{1920,1080},{640,480},{3840,2160}})
  for(int[] view:new int[][]{{1080,2400},{2400,1080},{720,1280},{1280,720}})
  for(int sensor:new int[]{0,90,180,270})for(int display:new int[]{0,90,180,270})for(boolean fill:new boolean[]{false,true}){
   int sw=source[0],sh=source[1],vw=view[0],vh=view[1];float[] m=PreviewGeometry.textureMatrix(sw,sh,vw,vh,sensor,display,fill);
   // Android first rotates sensor image into producer space, then stretches it into the TextureView.
   int bw=sensor%180==0?sw:sh,bh=sensor%180==0?sh:sw;
   double xScale=Math.hypot(m[0]*(double)vw/bw,m[3]*(double)vw/bw),yScale=Math.hypot(m[1]*(double)vh/bh,m[4]*(double)vh/bh);near(xScale,yScale);
   // An upright arrow in Android's producer space must receive only -DISPLAY rotation.
   double dx=m[1]*vh/bh,dy=m[4]*vh/bh;int rotation=(360-display)%360;
   if(rotation==0){near(dx,0);check(dy>0);}else if(rotation==90){check(dx<0);near(dy,0);}else if(rotation==180){near(dx,0);check(dy<0);}else{check(dx>0);near(dy,0);}
   // The center remains in the middle even after a 180-degree turn without resizing.
   near(m[0]*vw/2.0+m[1]*vh/2.0+m[2],vw/2.0);near(m[3]*vw/2.0+m[4]*vh/2.0+m[5],vh/2.0);cameraCases++;
  }
  // Regression from user's photo: a landscape view must NOT receive another sensor turn.
  float[] landscape=PreviewGeometry.textureMatrix(1280,720,2400,1080,270,90,true);near(landscape[0],0);check(landscape[1]>0);check(landscape[3]<0);near(landscape[4],0);
  System.out.println("PREVIEW_GEOMETRY_PASS affine="+count+" Camera2="+cameraCases+"; implicit sensor transform, display rotation, uniform proportions");
 }
 static void check(boolean value){if(!value)throw new AssertionError();}
}
