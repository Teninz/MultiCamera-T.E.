package ru.codex.obslancamera;
/** A corner swipe must start in a corner and travel inward far enough to be deliberate. */
final class ScreenWakePolicy {
 static boolean cornerSwipe(float x0,float y0,float x,float y,int width,int height,float density){
  if(width<=0||height<=0||density<=0)return false;
  float edge=56*density;boolean left=x0<=edge,right=x0>=width-edge,top=y0<=edge,bottom=y0>=height-edge;
  if(!(left||right)||!(top||bottom))return false;
  float dx=x-x0,dy=y-y0,inwardX=left?dx:-dx,inwardY=top?dy:-dy;
  return Math.hypot(dx,dy)>=96*density&&inwardX>=-12*density&&inwardY>=-12*density&&Math.max(inwardX,inwardY)>=64*density;
 }
}
