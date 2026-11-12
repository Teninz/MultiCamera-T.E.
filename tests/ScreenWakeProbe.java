package ru.codex.obslancamera;
public class ScreenWakeProbe {
 static void check(boolean value){if(!value)throw new AssertionError();}
 public static void main(String[] args){
  check(ScreenWakePolicy.cornerSwipe(8,8,260,200,1080,1920,1));
  check(ScreenWakePolicy.cornerSwipe(1072,8,800,200,1080,1920,1));
  check(ScreenWakePolicy.cornerSwipe(8,1912,260,1700,1080,1920,1));
  check(ScreenWakePolicy.cornerSwipe(1072,1912,800,1700,1080,1920,1));
  check(ScreenWakePolicy.cornerSwipe(8,8,220,9,1080,1920,1));
  check(ScreenWakePolicy.cornerSwipe(630,350,400,100,640,360,2));
  check(!ScreenWakePolicy.cornerSwipe(540,960,500,100,1080,1920,1));
  check(!ScreenWakePolicy.cornerSwipe(8,8,35,40,1080,1920,1));
  check(!ScreenWakePolicy.cornerSwipe(8,8,-300,100,1080,1920,1));
  check(!ScreenWakePolicy.cornerSwipe(8,8,500,500,0,0,1));
  System.out.println("SCREEN_CORNER_GESTURES_PASS four corners, portrait/landscape, density, incidental touch rejection");
 }
}
