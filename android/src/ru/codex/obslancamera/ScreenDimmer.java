package ru.codex.obslancamera;
import android.app.Activity;
import android.graphics.Color;
import android.os.*;
import android.view.*;
import android.widget.*;
import java.util.function.BooleanSupplier;
/** Covers the preview and dims only this window. Camera, encoder and microphone stay alive. */
final class ScreenDimmer {
 private final Activity activity;private final FrameLayout cover;private final TextView hint;
 private final BooleanSupplier streaming;private final Handler ui=new Handler(Looper.getMainLooper());
 private final GestureDetector gestures;private boolean dimmed,automatic,focused;private float brightness,startX,startY;private int wakingKey=-1;
 private final Runnable idle;
 private final Runnable hideHint;
 ScreenDimmer(Activity activity,FrameLayout root,BooleanSupplier streaming){
  this.activity=activity;this.streaming=streaming;idle=()->{if(focused&&streaming.getAsBoolean())show();};
  cover=new FrameLayout(activity);cover.setBackgroundColor(Color.BLACK);cover.setVisibility(View.GONE);cover.setClickable(true);cover.setFocusable(true);cover.setContentDescription(Language.text("Экран затемнён. Двойное касание, свайп от угла или кнопка громкости — вернуть экран."));
  hint=new TextView(activity);hint.setText(Language.text("Передача продолжается\n\nДвойное касание · свайп от угла · громкость"));hint.setTextColor(0xff929d96);hint.setTextSize(14);hint.setGravity(Gravity.CENTER);hint.setPadding(24,24,24,24);cover.addView(hint,new FrameLayout.LayoutParams(-1,-1));root.addView(cover,new FrameLayout.LayoutParams(-1,-1));hideHint=()->hint.setVisibility(View.INVISIBLE);
  gestures=new GestureDetector(activity,new GestureDetector.SimpleOnGestureListener(){public boolean onDown(android.view.MotionEvent e){return true;}public boolean onDoubleTap(android.view.MotionEvent e){restore();return true;}});
  cover.setOnTouchListener((view,event)->{if(event.getActionMasked()==MotionEvent.ACTION_DOWN){startX=event.getX();startY=event.getY();}gestures.onTouchEvent(event);if(event.getActionMasked()==MotionEvent.ACTION_UP&&dimmed&&ScreenWakePolicy.cornerSwipe(startX,startY,event.getX(),event.getY(),cover.getWidth(),cover.getHeight(),activity.getResources().getDisplayMetrics().density))restore();return true;});
  // Accessibility ACTION_CLICK also restores the screen; ordinary single touches are consumed above.
  cover.setOnClickListener(view->restore());
 }
 boolean active(){return dimmed;}
 void focus(boolean value){focused=value;if(!value){restore();ui.removeCallbacks(idle);}else touch();}
 void automatic(boolean enabled){automatic=enabled;touch();}
 void touch(){ui.removeCallbacks(idle);if(automatic&&focused&&!dimmed&&streaming.getAsBoolean())ui.postDelayed(idle,30000);}
 void show(){
  if(dimmed||!streaming.getAsBoolean())return;ui.removeCallbacks(idle);dimmed=true;
  WindowManager.LayoutParams params=activity.getWindow().getAttributes();brightness=params.screenBrightness;params.screenBrightness=.01f;activity.getWindow().setAttributes(params);
  hint.setVisibility(View.VISIBLE);cover.setVisibility(View.VISIBLE);cover.bringToFront();cover.requestFocus();ui.removeCallbacks(hideHint);ui.postDelayed(hideHint,3500);
 }
 void restore(){
  if(!dimmed)return;dimmed=false;ui.removeCallbacks(hideHint);cover.setVisibility(View.GONE);WindowManager.LayoutParams params=activity.getWindow().getAttributes();params.screenBrightness=brightness;activity.getWindow().setAttributes(params);touch();
 }
 void stop(){wakingKey=-1;ui.removeCallbacks(idle);restore();ui.removeCallbacks(idle);}
 boolean keyDown(int key){if(wakingKey==key)return true;if(dimmed&&(key==KeyEvent.KEYCODE_VOLUME_UP||key==KeyEvent.KEYCODE_VOLUME_DOWN||key==KeyEvent.KEYCODE_VOLUME_MUTE)){wakingKey=key;restore();return true;}return false;}
 boolean keyUp(int key){if(wakingKey!=key)return false;wakingKey=-1;return true;}
 void close(){stop();ui.removeCallbacksAndMessages(null);}
}
