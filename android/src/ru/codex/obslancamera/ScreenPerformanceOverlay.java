package ru.codex.obslancamera;
import android.content.*;
import android.graphics.PixelFormat;
import android.graphics.Color;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.drawable.GradientDrawable;
import android.provider.Settings;
import android.view.*;
import android.widget.*;
/** Optional, non-focusable controls. No accessibility service or touch interception outside the bar. */
final class ScreenPerformanceOverlay {
    private final Context context;private final Runnable stop,open;private final WindowManager windows;
    private View view;private TextView metrics;private boolean enabled,hidden;private String last="";
    ScreenPerformanceOverlay(Context context,Runnable stop,Runnable open){this.context=context;this.stop=stop;this.open=open;windows=(WindowManager)context.getSystemService(Context.WINDOW_SERVICE);}
    private int dp(int value){return (int)(value*context.getResources().getDisplayMetrics().density+.5f);}
    void enabled(boolean value){enabled=value;hidden=false;if(!value)remove();else show();}
    void toggle(){if(!enabled)return;hidden=!hidden;show();}
    private TextView button(String label,Runnable action){TextView text=new TextView(context);text.setText(label);text.setTextColor(Color.WHITE);text.setTextSize(12);text.setGravity(Gravity.CENTER);text.setPadding(dp(10),dp(10),dp(10),dp(10));text.setOnClickListener(v->action.run());return text;}
    private View icon(int kind,String description,Runnable action){
        View control=new View(context){private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
            public CharSequence getAccessibilityClassName(){return "android.widget.Button";}
            protected void onDraw(Canvas canvas){super.onDraw(canvas);float cx=getWidth()/2f,cy=getHeight()/2f;
                paint.setStyle(Paint.Style.FILL);paint.setColor(kind==0?0xffc44a55:kind==1?0xff397b62:0xff3c524a);canvas.drawRoundRect(cx-dp(21),cy-dp(21),cx+dp(21),cy+dp(21),dp(12),dp(12),paint);
                paint.setColor(Color.WHITE);paint.setStrokeWidth(dp(2));paint.setStrokeCap(Paint.Cap.ROUND);
                if(kind==0)canvas.drawRoundRect(cx-dp(8),cy-dp(8),cx+dp(8),cy+dp(8),dp(3),dp(3),paint);
                else if(kind==1){paint.setStyle(Paint.Style.STROKE);canvas.drawRoundRect(cx-dp(10),cy-dp(8),cx+dp(2),cy+dp(10),dp(2),dp(2),paint);canvas.drawLine(cx-dp(2),cy+dp(2),cx+dp(11),cy-dp(10),paint);canvas.drawLine(cx+dp(4),cy-dp(10),cx+dp(11),cy-dp(10),paint);canvas.drawLine(cx+dp(11),cy-dp(10),cx+dp(11),cy-dp(3),paint);}
                else {canvas.drawLine(cx-dp(8),cy-dp(3),cx,cy+dp(5),paint);canvas.drawLine(cx,cy+dp(5),cx+dp(8),cy-dp(3),paint);}
            }
        };control.setContentDescription(description);control.setTooltipText(description);control.setFocusable(true);control.setOnClickListener(v->action.run());return control;
    }
    private void show(){
        remove();if(!enabled||hidden||!Settings.canDrawOverlays(context))return;
        LinearLayout row=new LinearLayout(context);row.setGravity(Gravity.CENTER_VERTICAL);row.setOrientation(LinearLayout.HORIZONTAL);GradientDrawable background=new GradientDrawable();background.setColor(0xd51a2925);background.setCornerRadius(dp(10));row.setBackground(background);
        {
            metrics=button(last,()->{});metrics.setTextSize(11);metrics.setGravity(Gravity.CENTER_VERTICAL);row.addView(metrics,new LinearLayout.LayoutParams(0,-2,1));
            row.addView(icon(0,Language.text("Остановить передачу"),stop),new LinearLayout.LayoutParams(dp(52),dp(52)));
            row.addView(icon(1,Language.text("В приложение"),open),new LinearLayout.LayoutParams(dp(52),dp(52)));
            row.addView(icon(2,Language.text("Скрыть панель"),()->{hidden=true;show();}),new LinearLayout.LayoutParams(dp(52),dp(52)));
        }
        // FLAG_SECURE redacts this rectangle to black on Realme; it does not reveal the app below.
        // Use full removal when hidden, and restore through the foreground notification.
        WindowManager.LayoutParams params=new WindowManager.LayoutParams(-1,-2,WindowManager.LayoutParams.TYPE_APPLICATION_OVERLAY,WindowManager.LayoutParams.FLAG_NOT_FOCUSABLE|WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL,PixelFormat.TRANSLUCENT);params.gravity=Gravity.TOP|Gravity.RIGHT;params.y=dp(28);
        try{windows.addView(row,params);view=row;}catch(SecurityException error){enabled=false;}
    }
    void update(double fps,int target,double mbps,double sendMs,Double encoderMs){
        last=String.format(java.util.Locale.ROOT,"%.1f/%d FPS · %.1f Mbps\n%s %.1f ms · %s %s",fps,target,mbps,Language.text("Отправка"),sendMs,Language.text("Кодирование"),encoderMs==null?"—":String.format(java.util.Locale.ROOT,"%.1f ms",encoderMs));if(metrics!=null)metrics.setText(last);
    }
    void remove(){if(view!=null){try{windows.removeView(view);}catch(IllegalArgumentException ignored){}view=null;metrics=null;}}
}
