package ru.codex.obslancamera;
import android.app.Activity;
import android.content.Context;
import android.graphics.*;
import android.graphics.drawable.GradientDrawable;
import android.view.*;
import android.widget.*;
import java.util.*;
/** Familiar camera controls, kept separate from transport and preview geometry. */
final class CameraOverlay extends FrameLayout {
    interface Actions {void shutter();void scan();void flip();void modes();void fps();void stabilization();void settings();void dim();void mic();void torch();void fit();void zoom(float value);}
    private final Actions actions;private final int accent=0xffb5f3cc;
    private TextView state,detail,quality,mic,torch,fit,centerHint,fpsButton,stabilization;private Shutter shutter;
    private String stabilizationText=Language.text("Стаб. определение…");private int targetFps=30;
    private LinearLayout zoomRow;private boolean active,audio,light,fill=true,landscape;private String status=Language.text("Не подключено"),mode=Language.text("АВТО · 30"),info="",lens=Language.text("Основная");
    private float minZoom=1,maxZoom=1,currentZoom=1;private boolean flash;
    CameraOverlay(Context c,Actions a){super(c);actions=a;setClipChildren(false);build();}
    private int dp(float n){return (int)(n*getResources().getDisplayMetrics().density+.5f);}
    private TextView label(String text,int sp,int color){TextView v=new TextView(getContext());v.setText(text);v.setTextSize(sp);v.setTextColor(color);v.setGravity(Gravity.CENTER_VERTICAL);v.setFontFeatureSettings("tnum");return v;}
    static GradientDrawable rounded(int color,float radius){GradientDrawable d=new GradientDrawable();d.setColor(color);d.setCornerRadius(radius);return d;}
    private TextView pill(String text,OnClickListener click){TextView v=label(text,12,Color.WHITE);v.setGravity(Gravity.CENTER);v.setPadding(dp(13),0,dp(13),0);v.setBackground(rounded(0xb025292f,dp(22)));v.setOnClickListener(click);return v;}
    private void build(){
        removeAllViews();int padding=dp(16);
        LinearLayout top=new LinearLayout(getContext());top.setOrientation(1);top.setPadding(padding,dp(14),padding,0);
        LinearLayout first=new LinearLayout(getContext());first.setGravity(Gravity.CENTER_VERTICAL);state=label("●  "+status,12,accent);first.addView(state,new LinearLayout.LayoutParams(0,dp(42),1));
        quality=pill(mode,v->actions.modes());first.addView(quality,new LinearLayout.LayoutParams(dp(116),dp(40)));
        TextView dark=pill("☾",v->actions.dim());dark.setTextSize(23);dark.setContentDescription(Language.text("Затемнить экран"));LinearLayout.LayoutParams darkLp=new LinearLayout.LayoutParams(dp(42),dp(40));darkLp.leftMargin=dp(7);first.addView(dark,darkLp);TextView settings=pill("⚙",v->actions.settings());settings.setTextSize(24);settings.setContentDescription(Language.text("Подключение и настройки"));LinearLayout.LayoutParams gear=new LinearLayout.LayoutParams(dp(44),dp(40));gear.leftMargin=dp(8);first.addView(settings,gear);top.addView(first);
        LinearLayout quick=new LinearLayout(getContext());quick.setGravity(Gravity.CENTER_VERTICAL);torch=pill(light?Language.text("⚡ Вкл"):"⚡",v->actions.torch());torch.setVisibility(flash?VISIBLE:GONE);quick.addView(torch,new LinearLayout.LayoutParams(dp(56),dp(36)));
        mic=pill(audio?Language.text("● Микр. вкл"):Language.text("Микр. выкл"),v->actions.mic());LinearLayout.LayoutParams mp=new LinearLayout.LayoutParams(dp(110),dp(36));mp.leftMargin=flash?dp(8):0;quick.addView(mic,mp);Space space=new Space(getContext());quick.addView(space,new LinearLayout.LayoutParams(0,1,1));
        fit=pill(fill?Language.text("Заполнить"):Language.text("Вписать"),v->actions.fit());quick.addView(fit,new LinearLayout.LayoutParams(dp(100),dp(36)));top.addView(quick);
        LinearLayout cameraControls=new LinearLayout(getContext());cameraControls.setGravity(Gravity.CENTER_VERTICAL);fpsButton=pill(targetFps+" FPS",v->actions.fps());cameraControls.addView(fpsButton,new LinearLayout.LayoutParams(dp(86),dp(36)));stabilization=pill(stabilizationText,v->actions.stabilization());LinearLayout.LayoutParams stabilizationLp=new LinearLayout.LayoutParams(0,dp(36),1);stabilizationLp.leftMargin=dp(8);cameraControls.addView(stabilization,stabilizationLp);LinearLayout.LayoutParams controlsLp=new LinearLayout.LayoutParams(-1,dp(36));controlsLp.topMargin=dp(7);top.addView(cameraControls,controlsLp);
        detail=label(info,11,0xffd2d8dd);detail.setMaxLines(2);LinearLayout.LayoutParams detailLp=new LinearLayout.LayoutParams(-1,dp(36));detailLp.topMargin=dp(7);top.addView(detail,detailLp);
        FrameLayout.LayoutParams topLp=new FrameLayout.LayoutParams(-1,-2,Gravity.TOP);if(landscape)topLp.rightMargin=dp(120);addView(top,topLp);
        centerHint=label(Language.text("Камера для OBS\n\nНажмите круглую кнопку,\nчтобы начать передачу"),20,0xffe0e5e9);centerHint.setGravity(Gravity.CENTER);centerHint.setVisibility(active?GONE:VISIBLE);FrameLayout.LayoutParams hint=new FrameLayout.LayoutParams(-1,dp(180),Gravity.CENTER);hint.bottomMargin=landscape?0:dp(100);if(landscape)hint.rightMargin=dp(120);addView(centerHint,hint);
        LinearLayout bottom=new LinearLayout(getContext());bottom.setOrientation(1);bottom.setGravity(Gravity.CENTER);bottom.setPadding(padding,dp(10),padding,dp(20));
        TextView modeLabel=label(Language.text("В OBS"),12,accent);modeLabel.setGravity(Gravity.CENTER);modeLabel.setLetterSpacing(.15f);bottom.addView(modeLabel,new LinearLayout.LayoutParams(-1,dp(26)));
        LinearLayout controls=new LinearLayout(getContext());controls.setGravity(Gravity.CENTER);controls.setOrientation(landscape?1:0);
        TextView qr=pill("QR",v->actions.scan());qr.setTextSize(16);qr.setContentDescription(Language.text("Сканировать QR подключения"));controls.addView(qr,new LinearLayout.LayoutParams(dp(54),dp(54)));
        shutter=new Shutter(getContext());shutter.active=active;shutter.setOnClickListener(v->actions.shutter());LinearLayout.LayoutParams shutterLp=new LinearLayout.LayoutParams(dp(88),dp(88));if(landscape){shutterLp.topMargin=dp(20);shutterLp.bottomMargin=dp(20);}else{shutterLp.leftMargin=dp(32);shutterLp.rightMargin=dp(32);}controls.addView(shutter,shutterLp);
        TextView flip=pill("⇄",v->actions.flip());flip.setTextSize(29);flip.setContentDescription(Language.text("Переключить основную и переднюю камеру"));controls.addView(flip,new LinearLayout.LayoutParams(dp(54),dp(54)));bottom.addView(controls);
        TextView camera=label(lens,11,0xffaab4bc);camera.setGravity(Gravity.CENTER);LinearLayout.LayoutParams lensLp=new LinearLayout.LayoutParams(-1,dp(24));lensLp.topMargin=dp(6);bottom.addView(camera,lensLp);
        FrameLayout.LayoutParams bottomLp=new FrameLayout.LayoutParams(landscape?dp(120):-1,landscape?-1:dp(176),landscape?Gravity.RIGHT:Gravity.BOTTOM);bottom.setBackground(new GradientDrawable(landscape?GradientDrawable.Orientation.RIGHT_LEFT:GradientDrawable.Orientation.BOTTOM_TOP,new int[]{0xf2080b0f,0x00080b0f}));addView(bottom,bottomLp);
        zoomRow=new LinearLayout(getContext());zoomRow.setGravity(Gravity.CENTER);FrameLayout.LayoutParams zl=new FrameLayout.LayoutParams(-1,dp(42),Gravity.BOTTOM);zl.bottomMargin=landscape?dp(22):dp(190);if(landscape)zl.rightMargin=dp(120);addView(zoomRow,zl);refreshZoom();refreshColors();
    }
    void landscape(boolean value){if(landscape!=value){landscape=value;build();}}
    void status(String text){status=text;if(state!=null)state.setText("●  "+text);}
    void detail(String text){info=text;if(detail!=null)detail.setText(text);}
    void mode(String text){mode=text;if(quality!=null)quality.setText(text);}
    void fps(int value){targetFps=value;if(fpsButton!=null)fpsButton.setText(value+" FPS");}
    void stabilization(String text){stabilizationText=Language.text(text);if(stabilization!=null)stabilization.setText(stabilizationText);}
    void active(boolean value){active=value;if(shutter!=null){shutter.active=value;shutter.setContentDescription(value?Language.text("Остановить передачу в OBS"):Language.text("Начать передачу в OBS"));shutter.invalidate();}if(centerHint!=null)centerHint.setVisibility(value?GONE:VISIBLE);}
    void microphone(boolean value){audio=value;refreshColors();}
    void torch(boolean value){light=value;if(torch!=null)torch.setText(value?Language.text("⚡ Вкл"):"⚡");}
    void fill(boolean value){fill=value;if(fit!=null)fit.setText(value?Language.text("Заполнить"):Language.text("Вписать"));}
    void capabilities(boolean front,boolean flash,float min,float max,float zoom){lens=front?Language.text("Передняя камера"):Language.text("Основная камера");this.flash=flash;minZoom=min;maxZoom=max;currentZoom=zoom;build();}
    void zoom(float value){currentZoom=value;refreshZoom();}
    private void refreshColors(){if(mic!=null){mic.setText(audio?Language.text("● Микр. вкл"):Language.text("Микр. выкл"));mic.setTextColor(audio?0xff10291b:Color.WHITE);mic.setBackground(rounded(audio?accent:0xb025292f,dp(22)));}}
    private void refreshZoom(){if(zoomRow==null)return;zoomRow.removeAllViews();for(float z:new float[]{.5f,1,2,5}){if(z<minZoom-.01f||z>maxZoom+.01f)continue;String text=z<1?"0.5×":((int)z)+"×";TextView p=pill(text,v->actions.zoom(z));if(Math.abs(z-currentZoom)<.05){p.setBackground(rounded(accent,dp(22)));p.setTextColor(0xff10291b);}LinearLayout.LayoutParams lp=new LinearLayout.LayoutParams(dp(46),dp(38));lp.leftMargin=dp(4);lp.rightMargin=dp(4);zoomRow.addView(p,lp);}}
    private static final class Shutter extends View {
        boolean active;final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);Shutter(Context c){super(c);setFocusable(true);}
        protected void onDraw(Canvas c){super.onDraw(c);float x=getWidth()/2f,y=getHeight()/2f,r=Math.min(x,y)-5;paint.setColor(Color.WHITE);paint.setStyle(Paint.Style.STROKE);paint.setStrokeWidth(3*getResources().getDisplayMetrics().density);c.drawCircle(x,y,r,paint);paint.setStyle(Paint.Style.FILL);paint.setColor(active?0xffff675f:0xffb5f3cc);if(active){float a=r*.44f;c.drawRoundRect(x-a,y-a,x+a,y+a,8,8,paint);}else c.drawCircle(x,y,r*.78f,paint);}
    }
}
