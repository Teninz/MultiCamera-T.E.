package ru.codex.obslancamera;

import android.Manifest;
import android.app.*;
import android.os.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.graphics.*;
import android.hardware.camera2.*;
import android.hardware.camera2.params.*;
import android.hardware.display.DisplayManager;
import android.media.*;
import android.net.Uri;
import android.util.*;
import android.view.*;
import android.widget.*;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

public class MainActivity extends Activity implements TextureView.SurfaceTextureListener,CameraOverlay.Actions {
    private static final int CAMERA_PERMISSION=10,MIC_PERMISSION=11,SCAN=12;
    private TextureView preview;private CameraOverlay overlay;private CameraManager cameras;
    private ScreenDimmer dimmer;private PhoneUpdater updater;private boolean resumeAfterLanguage;
    private int connectionMode;private String endpoint="",secret="",cameraId="";private CameraCharacteristics characteristics;
    private List<CameraModes.Mode> availableModes=new ArrayList<>();private CameraModes.Mode selectedMode;
    private volatile boolean running,captureReady,microphone;private boolean front,fill=true,flashlight,resumed,micPermissionPending,restartAfterMicPermission;
    private volatile Thread worker;private volatile Wire wire;private PhoneAudio audio;
    private final Object audioLock=new Object();private final Handler ui=new Handler(Looper.getMainLooper());
    private final ArrayDeque<String> diagnostics=new ArrayDeque<>();
    private volatile int sensorOrientation=90,physicalOrientation=-1;private int previewW=1280,previewH=720;
    private OrientationEventListener orientationListener;private Runnable pendingRestart;private String finalMessage;
    private DisplayManager displays;private final DisplayManager.DisplayListener displayListener=new DisplayManager.DisplayListener(){public void onDisplayAdded(int id){}public void onDisplayRemoved(int id){}public void onDisplayChanged(int id){updatePreview();}};
    private volatile float zoomRatio=1;private float minZoom=1,maxZoom=1;private boolean hasFlash;
    private volatile Handler cameraHandler;private volatile CameraCaptureSession activeSession;private volatile CaptureRequest.Builder activeRequest;
    private volatile CameraCaptureSession.CaptureCallback activeCallback;private volatile String stabilizationState=Language.text("Стаб. определение…");
    private final Runnable watchdog=new Runnable(){public void run(){Wire w=wire;if(w!=null&&w.writeStarted!=0&&System.nanoTime()-w.writeStarted>1500000000L)w.close();if(running)ui.postDelayed(this,300);}};
    private int dp(int n){return (int)(n*getResources().getDisplayMetrics().density+.5f);}
    private void log(String text){synchronized(diagnostics){if(diagnostics.size()>=70)diagnostics.removeFirst();diagnostics.addLast(text);}Log.i("OBSLANCamera",text);}
    private void detail(String text){ui.post(()->overlay.detail(text));}
    @Override public void onCreate(Bundle state){
        super.onCreate(state);Language.set(getPreferences(0).getString("language",Locale.getDefault().getLanguage()));resumeAfterLanguage=getIntent().getBooleanExtra("resume_after_language",false);getIntent().removeExtra("resume_after_language");microphone=resumeAfterLanguage&&getIntent().getBooleanExtra("mic_after_language",false);getIntent().removeExtra("mic_after_language");getWindow().setStatusBarColor(Color.BLACK);getWindow().setNavigationBarColor(Color.BLACK);
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LAYOUT_STABLE|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        FrameLayout root=new FrameLayout(this);root.setBackgroundColor(0xff070b10);preview=new TextureView(this);preview.setSurfaceTextureListener(this);root.addView(preview,new FrameLayout.LayoutParams(-1,-1));overlay=new CameraOverlay(this,this);root.addView(overlay,new FrameLayout.LayoutParams(-1,-1));dimmer=new ScreenDimmer(this,root,()->running);dimmer.automatic(getPreferences(0).getBoolean("auto_dim",false));setContentView(root);
        root.setOnApplyWindowInsetsListener((v,insets)->{int l,t,r,b;if(Build.VERSION.SDK_INT>=30){android.graphics.Insets bars=insets.getInsets(WindowInsets.Type.displayCutout());l=bars.left;t=bars.top;r=bars.right;b=bars.bottom;}else{l=insets.getSystemWindowInsetLeft();t=insets.getSystemWindowInsetTop();r=insets.getSystemWindowInsetRight();b=insets.getSystemWindowInsetBottom();}overlay.setPadding(l,t,r,b);return insets;});
        cameras=(CameraManager)getSystemService(CAMERA_SERVICE);displays=(DisplayManager)getSystemService(DISPLAY_SERVICE);endpoint=getPreferences(0).getString("address","");secret=getPreferences(0).getString("token","");connectionMode=getPreferences(0).getInt("transport",endpoint.startsWith("wss://")?2:0);
        log("MultiCamera T.E. 0.4.1; "+Build.MANUFACTURER+" "+Build.MODEL+"; Android "+Build.VERSION.RELEASE+" / API "+Build.VERSION.SDK_INT);
        orientationListener=new OrientationEventListener(this){public void onOrientationChanged(int degrees){if(degrees==ORIENTATION_UNKNOWN)return;int value=((degrees+45)/90*90)%360;if(physicalOrientation!=value){physicalOrientation=value;updatePreview();}}};
        refreshCamera();layoutOrientation();if(getIntent().getData()!=null)pair(getIntent().getData().toString());updater=new PhoneUpdater(this,this::log);updater.check(false);
    }
    @Override protected void onResume(){super.onResume();resumed=true;displays.registerDisplayListener(displayListener,ui);if(orientationListener.canDetectOrientation())orientationListener.enable();updatePreview();resumeAfterMicPermission();if(updater!=null)updater.resume();resumeLanguageStream();}
    private void resumeLanguageStream(){if(resumeAfterLanguage&&resumed&&preview.isAvailable()){resumeAfterLanguage=false;start();}}
    private void resumeAfterMicPermission(){if(resumed&&!micPermissionPending&&restartAfterMicPermission){restartAfterMicPermission=false;afterStop(()->start());}}
    @Override protected void onPause(){resumed=false;if(dimmer!=null)dimmer.stop();displays.unregisterDisplayListener(displayListener);if(updater!=null)updater.pause();orientationListener.disable();halt();super.onPause();}
    @Override protected void onDestroy(){if(dimmer!=null)dimmer.close();if(updater!=null)updater.close();super.onDestroy();}
    @Override public void onConfigurationChanged(Configuration config){super.onConfigurationChanged(config);layoutOrientation();updatePreview();}
    private void layoutOrientation(){overlay.landscape(getResources().getConfiguration().orientation==Configuration.ORIENTATION_LANDSCAPE);}
    private void refreshCamera(){
        try{
            cameraId="";for(String id:cameras.getCameraIdList()){CameraCharacteristics c=cameras.getCameraCharacteristics(id);Integer facing=c.get(CameraCharacteristics.LENS_FACING);if(facing!=null&&facing==(front?CameraCharacteristics.LENS_FACING_FRONT:CameraCharacteristics.LENS_FACING_BACK)){cameraId=id;if("0".equals(id)&&!front)break;if(front)break;}}
            if(cameraId.isEmpty())throw new Exception(Language.text("Эта камера недоступна"));characteristics=cameras.getCameraCharacteristics(cameraId);Integer sensor=characteristics.get(CameraCharacteristics.SENSOR_ORIENTATION);sensorOrientation=sensor==null?90:sensor;
            availableModes=CameraModes.discover(characteristics);hasFlash=Boolean.TRUE.equals(characteristics.get(CameraCharacteristics.FLASH_INFO_AVAILABLE));
            Range<Float> range=Build.VERSION.SDK_INT>=30?characteristics.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE):null;
            Float maximum=characteristics.get(CameraCharacteristics.SCALER_AVAILABLE_MAX_DIGITAL_ZOOM);minZoom=range!=null?range.getLower():1;maxZoom=range!=null?range.getUpper():maximum==null?1:maximum;zoomRatio=Math.max(minZoom,Math.min(maxZoom,zoomRatio));
            overlay.capabilities(front,hasFlash,minZoom,maxZoom,zoomRatio);overlay.mode(selectedMode==null?Language.text("АВТО · 30"):selectedMode.shortLabel());overlay.microphone(microphone);overlay.fill(fill);overlay.torch(flashlight);
            overlay.fps(selectedMode==null?30:selectedMode.fps);stabilizationState=Language.text("Стаб. авто");overlay.stabilization(stabilizationState);
            log(Language.text("Камера ")+cameraId+": "+availableModes.size()+Language.text(" режимов; sensor=")+sensorOrientation+"; zoom="+minZoom+".."+maxZoom);
            log(Language.text("Стабилизация: ")+CameraStabilization.capabilities(characteristics)+"; AE FPS="+Arrays.toString(characteristics.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES)));
        }catch(Exception error){log(Language.text("Камера: ")+EncoderCapture.diagnostic(error));detail(Language.text("Не удалось определить камеру. Откройте диагностику."));}
    }
    private boolean cameraPermission(){if(checkSelfPermission(Manifest.permission.CAMERA)==PackageManager.PERMISSION_GRANTED)return true;requestPermissions(new String[]{Manifest.permission.CAMERA},CAMERA_PERMISSION);return false;}
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] grants){
        super.onRequestPermissionsResult(request,permissions,grants);boolean granted=grants.length>0&&grants[0]==PackageManager.PERMISSION_GRANTED;
        if(request==CAMERA_PERMISSION){if(granted){refreshCamera();detail(Language.text("Камера разрешена. Нажмите кнопку запуска."));}else detail(Language.text("Для передачи изображения нужен доступ к камере."));}
        if(request==MIC_PERMISSION){micPermissionPending=false;microphone=granted;overlay.microphone(granted);if(granted){startAudio();detail(Language.text("Микрофон включён"));}else{stopAudio();detail(Language.text("Доступ к микрофону не разрешён"));}resumeAfterMicPermission();}
    }
    public void dim(){if(running)dimmer.show();else detail(Language.text("Затемнение доступно во время передачи камеры."));}
    @Override public void onWindowFocusChanged(boolean focused){super.onWindowFocusChanged(focused);if(dimmer!=null)dimmer.focus(focused);}
    @Override public boolean dispatchTouchEvent(MotionEvent event){if(dimmer!=null&&!dimmer.active()&&event.getActionMasked()==MotionEvent.ACTION_DOWN)dimmer.touch();return super.dispatchTouchEvent(event);}
    @Override public boolean onKeyDown(int key,KeyEvent event){if(dimmer!=null&&dimmer.keyDown(key))return true;return super.onKeyDown(key,event);}
    @Override public boolean onKeyUp(int key,KeyEvent event){if(dimmer!=null&&dimmer.keyUp(key))return true;return super.onKeyUp(key,event);}
    @Override public void onBackPressed(){if(dimmer!=null&&dimmer.active()){dimmer.restore();return;}super.onBackPressed();}
    public void shutter(){if(running)halt();else if(endpoint.isEmpty()||secret.isEmpty())settings();else start();}
    public void scan(){if(!cameraPermission())return;afterStop(()->startActivityForResult(new Intent(this,ScanActivity.class),SCAN));}
    public void flip(){afterStop(()->{front=!front;flashlight=false;zoomRatio=1;selectedMode=null;refreshCamera();if(!endpoint.isEmpty()&&!secret.isEmpty())start();});}
    public void fit(){fill=!fill;overlay.fill(fill);updatePreview();}
    public void torch(){if(!hasFlash)return;flashlight=!flashlight;overlay.torch(flashlight);applyLiveControls();}
    public void zoom(float value){zoomRatio=Math.max(minZoom,Math.min(maxZoom,value));overlay.zoom(zoomRatio);applyLiveControls();}
    public void mic(){
        if(microphone){microphone=false;stopAudio();overlay.microphone(false);detail(Language.text("Микрофон выключен"));return;}
        if(checkSelfPermission(Manifest.permission.RECORD_AUDIO)!=PackageManager.PERMISSION_GRANTED){restartAfterMicPermission=running;micPermissionPending=true;requestPermissions(new String[]{Manifest.permission.RECORD_AUDIO},MIC_PERMISSION);return;}
        microphone=true;overlay.microphone(true);startAudio();
    }
    public void modes(){
        if(availableModes.isEmpty()){refreshCamera();if(availableModes.isEmpty()){diagnostics();return;}}
        String[] labels=new String[availableModes.size()+1];labels[0]=Language.text("Автоматически · рекомендуется");for(int i=0;i<availableModes.size();i++)labels[i+1]=availableModes.get(i).label();
        AlertDialog dialog=new AlertDialog.Builder(this,android.app.AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle(front?Language.text("Режим передней камеры"):Language.text("Режим основной камеры")).setItems(labels,(d,which)->afterStop(()->{selectedMode=which==0?null:availableModes.get(which-1);overlay.mode(selectedMode==null?Language.text("АВТО · 30"):selectedMode.shortLabel());overlay.fps(selectedMode==null?30:selectedMode.fps);if(!endpoint.isEmpty()&&!secret.isEmpty())start();})).setNegativeButton(Language.text("Отмена"),null).create();dialog.show();
    }
    public void fps(){
        CameraModes.Mode current=selectedMode==null?CameraModes.recommended(availableModes):selectedMode;List<CameraModes.Mode> choices=new ArrayList<>();
        for(int rate:new int[]{60,30,24,15}){CameraModes.Mode best=null;double score=Double.MAX_VALUE;for(CameraModes.Mode candidate:availableModes)if(candidate.fps==rate){double distance=Math.abs(Math.log((double)candidate.width*candidate.height/((long)current.width*current.height)))+Math.abs((double)candidate.width/candidate.height-(double)current.width/current.height)*3;if(distance<score){score=distance;best=candidate;}}if(best!=null)choices.add(best);}
        if(choices.isEmpty()){detail(Language.text("Камера не сообщила доступные частоты. Откройте диагностику."));return;}String[] labels=new String[choices.size()];for(int i=0;i<labels.length;i++)labels[i]=choices.get(i).fps+Language.text(" кадров/с · ")+choices.get(i).width+" × "+choices.get(i).height;
        new AlertDialog.Builder(this,AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle(Language.text("Частота кадров выбранной камеры")).setItems(labels,(d,which)->afterStop(()->{selectedMode=choices.get(which);overlay.mode(selectedMode.shortLabel());overlay.fps(selectedMode.fps);if(!endpoint.isEmpty()&&!secret.isEmpty())start();})).setNegativeButton(Language.text("Отмена"),null).show();
    }
    public void stabilization(){new AlertDialog.Builder(this,AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle(Language.text("Стабилизация камеры")).setMessage(Language.text(stabilizationState)+"\n\n"+CameraStabilization.capabilities(characteristics)+Language.text("\n\nВключается автоматически. EIS — электронная, OIS — оптическая стабилизация. При 60 FPS или высоком разрешении камера может разрешать только OIS либо отключать стабилизацию. Индикатор показывает фактический ответ камеры.")).setPositiveButton("OK",null).show();}
    private void afterStop(Runnable action){if(worker!=null){pendingRestart=action;halt();}else action.run();}
    public void settings(){
        LinearLayout body=new LinearLayout(this);body.setOrientation(1);body.setPadding(dp(22),dp(12),dp(22),dp(12));
        TextView hint=new TextView(this);hint.setText(Language.text("В OBS откройте окно «Подключение смартфона» и считайте QR подключения. Адрес и ключ также можно ввести вручную."));hint.setTextColor(0xffc3cbd2);hint.setTextSize(14);body.addView(hint);
        RadioGroup transport=new RadioGroup(this);RadioButton local=new RadioButton(this),internet=new RadioButton(this),usb=new RadioButton(this);local.setId(701);internet.setId(702);usb.setId(703);usb.setText(Language.text("USB-кабель"));local.setText(Language.text("Локальная сеть"));internet.setText(Language.text("Интернет / свой сервер"));transport.addView(local);transport.addView(usb);transport.addView(internet);transport.check(connectionMode==2?702:connectionMode==1?703:701);body.addView(transport);
        EditText address=new EditText(this),key=new EditText(this);address.setText(endpoint);address.setHint(Language.text("Адрес ПК:порт"));address.setSingleLine();address.setTextColor(Color.WHITE);address.setHintTextColor(0xff95a4af);address.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);key.setText(secret);key.setHint(Language.text("Ключ из OBS"));key.setSingleLine();key.setTextColor(Color.WHITE);key.setHintTextColor(0xff95a4af);key.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD);body.addView(address);body.addView(key);
        Button usbGuide=new Button(this);usbGuide.setText(Language.text("Как подключить по USB"));usbGuide.setOnClickListener(v->usbHelp());body.addView(usbGuide);
        Button qr=new Button(this);qr.setText(Language.text("Сканировать QR"));body.addView(qr);Button diagnostic=new Button(this);diagnostic.setText(Language.text("Диагностика"));body.addView(diagnostic);Button updates=new Button(this);updates.setText(Language.text("Обновления · ")+updater.current());updates.setOnClickListener(v->updater.check(true));body.addView(updates);Button language=new Button(this);language.setText(Language.text("Язык")+" · "+Language.names()[java.util.Arrays.asList("en","ru","ja","es","zh").indexOf(Language.code())]);body.addView(language);
        Switch automaticDim=new Switch(this);automaticDim.setText(Language.text("Затемнять экран через 30 секунд без касаний"));automaticDim.setTextColor(Color.WHITE);automaticDim.setChecked(getPreferences(0).getBoolean("auto_dim",false));automaticDim.setOnCheckedChangeListener((button,enabled)->{getPreferences(0).edit().putBoolean("auto_dim",enabled).apply();dimmer.automatic(enabled);});body.addView(automaticDim);Button darken=new Button(this);darken.setText(Language.text("Затемнить экран"));body.addView(darken);
        ScrollView settingsScroll=new ScrollView(this);settingsScroll.addView(body);
        AlertDialog dialog=new AlertDialog.Builder(this,AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle(Language.text("Подключение к OBS")).setView(settingsScroll).setPositiveButton(Language.text("Подключить"),null).setNegativeButton(Language.text("Закрыть"),null).create();
        darken.setOnClickListener(v->{dialog.dismiss();dim();});
        language.setOnClickListener(v->new AlertDialog.Builder(this,AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle(Language.text("Язык")).setItems(Language.names(),(d,which)->{boolean restore=running,restoreMic=microphone;getPreferences(0).edit().putString("language",new String[]{"en","ru","ja","es","zh"}[which]).apply();dialog.dismiss();afterStop(()->{getIntent().putExtra("resume_after_language",restore);getIntent().putExtra("mic_after_language",restoreMic);recreate();});}).setNegativeButton(Language.text("Отмена"),null).show());
        qr.setOnClickListener(v->{dialog.dismiss();scan();});diagnostic.setOnClickListener(v->diagnostics());dialog.setOnShowListener(v->dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener(button->{String chosen=address.getText().toString().trim();if((transport.getCheckedRadioButtonId()==702)!=chosen.startsWith("wss://")){address.setError(Language.text("Адрес ПК:порт"));return;}connectionMode=transport.getCheckedRadioButtonId()==702?2:transport.getCheckedRadioButtonId()==703?1:0;endpoint=chosen;secret=key.getText().toString().trim().toLowerCase(Locale.ROOT);getPreferences(0).edit().putString("address",endpoint).putString("token",secret).putInt("transport",connectionMode).apply();dialog.dismiss();afterStop(()->start());}));dialog.show();
    }
    private void usbHelp(){new AlertDialog.Builder(this,AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle(Language.text("Подключение по USB")).setMessage(Language.text("1. Подключите телефон к ПК кабелем с передачей данных.\n2. В настройках Android включите «USB-модем».\n3. На ПК откройте MultiCamera T.E. → Подключить смартфон → USB-кабель. Выберите USB-адаптер и разрешите USB-подключение.\n4. Считайте QR подключения этим приложением и нажмите запуск.\n\nВидео передаётся по кабелю. Возможность USB-модема зависит от прошивки телефона.")).setPositiveButton(Language.text("Настройки USB"),(dialog,which)->{Intent settings=new Intent("android.settings.TETHER_SETTINGS");if(settings.resolveActivity(getPackageManager())==null)settings=new Intent(android.provider.Settings.ACTION_WIRELESS_SETTINGS);try{startActivity(settings);}catch(ActivityNotFoundException error){detail(Language.text("Откройте настройки Android → Подключения → USB-модем."));}}).setNegativeButton(Language.text("Закрыть"),null).show();}
    private void diagnostics(){
        StringBuilder info=new StringBuilder();synchronized(diagnostics){for(String line:diagnostics)info.append(line).append('\n');}
        TextView text=new TextView(this);text.setText(info.toString());text.setTextColor(Color.WHITE);text.setTextSize(12);text.setTextIsSelectable(true);text.setPadding(dp(18),dp(12),dp(18),dp(12));ScrollView scroll=new ScrollView(this);scroll.addView(text);
        new AlertDialog.Builder(this,AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle(Language.text("Диагностика камеры")).setView(scroll).setPositiveButton(Language.text("Закрыть"),null).setNeutralButton(Language.text("Копировать"),(d,w)->((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(ClipData.newPlainText("OBS LAN Camera diagnostics",info.toString()))).show();
    }
    private void pair(String value){try{Uri uri=Uri.parse(value);if(uri.getFragment()==null||!uri.getFragment().matches("[0-9a-fA-F]{32}"))throw new IllegalArgumentException();
        if("obslan".equals(uri.getScheme())&&uri.getHost()!=null){endpoint=uri.getHost()+":"+(uri.getPort()>0?uri.getPort():34872);connectionMode="usb".equals(uri.getQueryParameter("transport"))?1:0;}
        else if("multicamera".equals(uri.getScheme())&&"connect".equals(uri.getHost())){String relay=uri.getQueryParameter("relay");java.net.URI server=new java.net.URI(relay);if(!"wss".equalsIgnoreCase(server.getScheme())||server.getHost()==null||server.getUserInfo()!=null||server.getFragment()!=null)throw new IllegalArgumentException();endpoint=relay;connectionMode=2;}
        else throw new IllegalArgumentException();secret=uri.getFragment().toLowerCase(Locale.ROOT);getPreferences(0).edit().putString("address",endpoint).putString("token",secret).putInt("transport",connectionMode).apply();detail(Language.text("Сопряжение сохранено. Нажмите круглую кнопку для запуска."));
    }catch(Exception e){detail(Language.text("Это не QR подключения OBS LAN Camera."));}}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(request==SCAN&&result==RESULT_OK&&data!=null)pair(data.getStringExtra("pair"));}
    private void start(){
        if(running||worker!=null||!resumed)return;if(!cameraPermission())return;if(!preview.isAvailable()){detail(Language.text("Ожидаем предпросмотр. Нажмите запуск ещё раз."));return;}
        String host=endpoint;int port=34872;final boolean remote=endpoint.startsWith("wss://");try{if(remote){java.net.URI uri=new java.net.URI(endpoint);if(uri.getHost()==null||uri.getUserInfo()!=null||uri.getFragment()!=null)throw new Exception();}else{int colon=endpoint.indexOf(':');if(colon>=0){host=endpoint.substring(0,colon);port=Integer.parseInt(endpoint.substring(colon+1));}if(port<1024||port>65535)throw new Exception();}if(!secret.matches("[0-9a-f]{32}"))throw new Exception();}catch(Exception e){detail(Language.text("Проверьте адрес, порт и ключ подключения."));settings();return;}
        final String target=host,key=secret,id=cameraId;final int targetPort=port;final CameraCharacteristics caps=characteristics;final CameraModes.Mode requested=selectedMode;
        if(id.isEmpty()||caps==null){detail(Language.text("Камера недоступна"));return;}
        running=true;dimmer.touch();finalMessage=null;overlay.active(true);overlay.status(Language.text("Подключение…"));getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);ui.post(watchdog);
        worker=new Thread(()->{
            Set<String> rejected=new LinkedHashSet<>();
            try{while(running){
                try{ui.post(()->overlay.status(Language.text("Подключение…")));Wire channel=remote?new Wire(WssSocket.open(target),key):new Wire(target,targetPort,key);wire=channel;if(!running){channel.close();break;}stream(channel,id,caps,requested,rejected);}
                catch(EncoderCapture.Failure error){rejected.add(error.key);log(error.getMessage());detail(Language.text("Камера: пробуем другой совместимый режим…"));report(error.getMessage());}
                catch(EncoderCapture.Unavailable error){finalMessage=error.getMessage();log(finalMessage);report(finalMessage);running=false;}
                catch(SecurityException error){finalMessage=Language.text("Нет разрешения на камеру или микрофон");log(EncoderCapture.diagnostic(error));running=false;}
                catch(Exception error){if(running){String reason=networkReason(error);log(reason);detail(reason+Language.text(". Повтор через 2 секунды…"));}}
                finally{captureReady=false;stopAudio();Wire w=wire;wire=null;if(w!=null)w.close();}
                if(running)try{Thread.sleep(2000);}catch(InterruptedException ignored){}
            }}finally{ui.post(()->{worker=null;running=false;dimmer.stop();overlay.active(false);overlay.status(Language.text("Не подключено"));if(finalMessage!=null)overlay.detail(finalMessage);else overlay.detail(Language.text("Передача остановлена"));getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);Runnable next=pendingRestart;pendingRestart=null;if(next!=null&&resumed)next.run();});}
        },"obs-camera-stream");worker.start();
    }
    private String networkReason(Exception error){if(connectionMode==1&&(error instanceof java.net.ConnectException||error instanceof java.net.SocketTimeoutException))return Language.text("USB: проверьте кабель, USB-модем и выбранный USB-адаптер на ПК");if(error instanceof java.net.ConnectException)return Language.text("OBS недоступен: проверьте адрес и домашнюю сеть");if(error instanceof java.net.SocketTimeoutException)return Language.text("OBS не ответил: проверьте ключ и сеть");if(error instanceof java.io.EOFException)return Language.text("OBS закрыл подключение: проверьте ключ и версию плагина");return (error instanceof java.io.IOException?Language.text("Сеть: "):Language.text("Камера: "))+EncoderCapture.diagnostic(error);}
    private void report(String text){Wire w=wire;if(w!=null)try{w.send(3,0,0,text.substring(0,Math.min(500,text.length())).getBytes(StandardCharsets.UTF_8));}catch(Exception ignored){} }
    private void halt(){running=false;if(dimmer!=null)dimmer.stop();captureReady=false;ui.removeCallbacks(watchdog);stopAudio();Wire w=wire;if(w!=null)w.close();Thread t=worker;if(t!=null)t.interrupt();}
    private void startAudio(){synchronized(audioLock){Wire w=wire;if(microphone&&captureReady&&w!=null&&audio==null)audio=new PhoneAudio(w,this::log,()->ui.post(()->{microphone=false;overlay.microphone(false);stopAudio();detail(Language.text("Микрофон недоступен. Видео продолжает работать."));}));}}
    private void stopAudio(){synchronized(audioLock){if(audio!=null){audio.close();audio=null;}}}
    private int rotation(){if(physicalOrientation>=0)return front?(sensorOrientation-physicalOrientation+360)%360:(sensorOrientation+physicalOrientation)%360;int display=getWindowManager().getDefaultDisplay().getRotation()*90;return front?(sensorOrientation+display)%360:(sensorOrientation-display+360)%360;}
    private void updatePreview(){ui.post(()->{if(!preview.isAvailable())return;int display=getWindowManager().getDefaultDisplay().getRotation()*90;Matrix matrix=new Matrix();matrix.setValues(PreviewGeometry.textureMatrix(previewW,previewH,preview.getWidth(),preview.getHeight(),sensorOrientation,display,fill));preview.setTransform(matrix);});}
    private void applyControls(CaptureRequest.Builder b,CameraCharacteristics caps,int fps){
        b.set(CaptureRequest.CONTROL_MODE,CaptureRequest.CONTROL_MODE_AUTO);
        int[] af=caps.get(CameraCharacteristics.CONTROL_AF_AVAILABLE_MODES);if(af!=null)for(int mode:af)if(mode==CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO){b.set(CaptureRequest.CONTROL_AF_MODE,mode);break;}
        Range<Integer> selected=CameraModes.fpsRange(caps,fps);if(selected!=null)b.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE,selected);
        setZoomTorch(b,caps);
    }
    private void setZoomTorch(CaptureRequest.Builder b,CameraCharacteristics caps){
        Range<Float> range=Build.VERSION.SDK_INT>=30?caps.get(CameraCharacteristics.CONTROL_ZOOM_RATIO_RANGE):null;
        if(range!=null)b.set(CaptureRequest.CONTROL_ZOOM_RATIO,range.clamp(zoomRatio));else{Rect active=caps.get(CameraCharacteristics.SENSOR_INFO_ACTIVE_ARRAY_SIZE);if(active!=null){int halfW=(int)(active.width()/Math.max(1,zoomRatio)/2),halfH=(int)(active.height()/Math.max(1,zoomRatio)/2);b.set(CaptureRequest.SCALER_CROP_REGION,new Rect(active.centerX()-halfW,active.centerY()-halfH,active.centerX()+halfW,active.centerY()+halfH));}}
        if(Boolean.TRUE.equals(caps.get(CameraCharacteristics.FLASH_INFO_AVAILABLE)))b.set(CaptureRequest.FLASH_MODE,flashlight?CaptureRequest.FLASH_MODE_TORCH:CaptureRequest.FLASH_MODE_OFF);
    }
    private void applyLiveControls(){Handler handler=cameraHandler;if(handler!=null)handler.post(()->{try{CameraCaptureSession s=activeSession;CaptureRequest.Builder b=activeRequest;if(s!=null&&b!=null){setZoomTorch(b,characteristics);s.setRepeatingRequest(b.build(),activeCallback,handler);}}catch(Exception e){log(Language.text("Управление камерой: ")+EncoderCapture.diagnostic(e));}});}
    private void stream(Wire channel,String id,CameraCharacteristics caps,CameraModes.Mode requested,Set<String> rejected) throws Exception {
        EncoderCapture capture=EncoderCapture.open(caps,requested,rejected,this::log);previewW=capture.width;previewH=capture.height;updatePreview();
        HandlerThread thread=new HandlerThread("obs-camera-control");thread.start();Handler handler=new Handler(thread.getLooper());cameraHandler=handler;
        AtomicReference<CameraDevice> device=new AtomicReference<>();AtomicReference<CameraCaptureSession> session=new AtomicReference<>();AtomicReference<Exception> failure=new AtomicReference<>();AtomicBoolean closing=new AtomicBoolean();CountDownLatch opened=new CountDownLatch(1),configured=new CountDownLatch(1),closed=new CountDownLatch(1);Surface view=null;
        try{
            if(!running)return;if(!preview.isAvailable())throw new Exception(Language.text("Предпросмотр закрыт"));SurfaceTexture texture=preview.getSurfaceTexture();texture.setDefaultBufferSize(capture.width,capture.height);view=new Surface(texture);capture.receiveImages(handler,failure);
            cameras.openCamera(id,new CameraDevice.StateCallback(){public void onOpened(CameraDevice d){if(closing.get()||!running)d.close();else device.set(d);opened.countDown();}public void onDisconnected(CameraDevice d){failure.set(new Exception(Language.text("Камера отключена")));d.close();opened.countDown();}public void onError(CameraDevice d,int error){failure.set(new Exception(Language.text("Камера занята или недоступна: ")+error));d.close();opened.countDown();}public void onClosed(CameraDevice d){closed.countDown();}},handler);
            if(!opened.await(5,TimeUnit.SECONDS)||device.get()==null||failure.get()!=null)throw failure.get()!=null?failure.get():new Exception(Language.text("Камера не ответила"));
            final Surface previewSurface=view;
            final CameraStabilization stabilization=new CameraStabilization(caps,capture.width,capture.height,capture.fps);
            final CaptureRequest.Builder request=device.get().createCaptureRequest(CameraDevice.TEMPLATE_RECORD);request.addTarget(capture.surface);request.addTarget(previewSurface);applyControls(request,caps,capture.fps);stabilization.apply(request);
            log(Language.text("Запрос камеры: ")+capture.width+"×"+capture.height+" / "+capture.fps+" FPS; AE="+CameraModes.fpsRange(caps,capture.fps)+"; "+stabilization.requested());
            final CameraCaptureSession.CaptureCallback callback=new CameraCaptureSession.CaptureCallback(){
                private String last="";
                public void onCaptureCompleted(CameraCaptureSession s,CaptureRequest r,TotalCaptureResult result){if(closing.get()||!running)return;
                    boolean retry=stabilization.result(result,capture.fps);String observed=stabilization.actual();
                    if(!last.equals(observed)){last=observed;stabilizationState=observed;log(observed+"; AE="+result.get(CaptureResult.CONTROL_AE_TARGET_FPS_RANGE));ui.post(()->overlay.stabilization(observed));}
                    if(retry)try{stabilization.apply(request);s.setRepeatingRequest(request.build(),this,handler);log(Language.text("Повтор стабилизации: ")+stabilization.requested());}catch(Exception e){log(Language.text("Стабилизация: ")+EncoderCapture.diagnostic(e));}
                }
            };
            final CameraCaptureSession.StateCallback sessionCallback=new CameraCaptureSession.StateCallback(){
                public void onConfigured(CameraCaptureSession s){if(closing.get()){s.close();configured.countDown();return;}session.set(s);try{activeSession=s;activeRequest=request;activeCallback=callback;s.setRepeatingRequest(request.build(),callback,handler);}catch(Exception e){failure.set(new EncoderCapture.Failure(capture.key,Language.text("Не удалось настроить режим камеры: ")+EncoderCapture.diagnostic(e),e));}configured.countDown();}
                public void onConfigureFailed(CameraCaptureSession s){failure.set(new EncoderCapture.Failure(capture.key,Language.text("Камера отклонила ")+capture.width+"×"+capture.height+" / "+capture.fps,null));configured.countDown();}
            };
            if(Build.VERSION.SDK_INT>=28){SessionConfiguration configuration=new SessionConfiguration(SessionConfiguration.SESSION_REGULAR,Arrays.asList(new OutputConfiguration(capture.surface),new OutputConfiguration(view)),command->handler.post(command),sessionCallback);configuration.setSessionParameters(request.build());device.get().createCaptureSession(configuration);}
            else device.get().createCaptureSession(Arrays.asList(capture.surface,view),sessionCallback,handler);
            if(!configured.await(5,TimeUnit.SECONDS)||failure.get()!=null)throw failure.get()!=null?failure.get():new EncoderCapture.Failure(capture.key,Language.text("Истекло время настройки камеры"),null);
            captureReady=true;startAudio();ui.post(()->{overlay.status(Language.text("Передача в OBS"));overlay.mode(new CameraModes.Mode(capture.width,capture.height,capture.fps).shortLabel());overlay.fps(capture.fps);});
            MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();long frames=0,last=System.nanoTime(),ptsOffset=Long.MIN_VALUE;
            while(running&&failure.get()==null){int index=capture.codec.dequeueOutputBuffer(info,10000);
                if(index==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED){MediaFormat format=capture.codec.getOutputFormat();for(String key:new String[]{"csd-0","csd-1"}){ByteBuffer b=format.getByteBuffer(key);if(b!=null){ByteBuffer copy=b.duplicate();byte[] bytes=new byte[copy.remaining()];copy.get(bytes);channel.send(1,rotation(),0,bytes);}}}
                else if(index>=0){try{ByteBuffer b=capture.codec.getOutputBuffer(index);if(b!=null&&info.size>0){b.position(info.offset);b.limit(info.offset+info.size);byte[] bytes=new byte[info.size];b.get(bytes);int type=(info.flags&MediaCodec.BUFFER_FLAG_CODEC_CONFIG)!=0?1:2;long timestamp=info.presentationTimeUs;if(type==2){long now=SystemClock.elapsedRealtimeNanos()/1000;if(ptsOffset==Long.MIN_VALUE)ptsOffset=Math.abs(now-timestamp)<10000000L?0:now-timestamp-1000000L/capture.fps;timestamp+=ptsOffset;}channel.send(type,rotation(),timestamp,bytes);if(type==2)frames++;}}finally{capture.codec.releaseOutputBuffer(index,false);}}
                long now=System.nanoTime();if(now-last>2000000000L){double fps=frames*1e9/(now-last);detail(String.format(Locale.ROOT,Language.text("%d × %d · %.0f кадров/с · микрофон %s"),capture.width,capture.height,fps,microphone?Language.text("вкл"):Language.text("выкл")));frames=0;last=now;}
            }
            if(failure.get()!=null)throw failure.get();
        }catch(MediaCodec.CodecException error){throw new EncoderCapture.Failure(capture.key,Language.text("Кодировщик: ")+EncoderCapture.diagnostic(error),error);}
        catch(IllegalStateException error){throw new EncoderCapture.Failure(capture.key,Language.text("Состояние кодировщика: ")+EncoderCapture.diagnostic(error),error);}
        finally{
            closing.set(true);captureReady=false;stopAudio();activeSession=null;activeRequest=null;activeCallback=null;cameraHandler=null;
            CameraCaptureSession s=session.get();if(s!=null)try{s.stopRepeating();s.close();}catch(Exception ignored){}CameraDevice d=device.get();if(d!=null){d.close();try{closed.await(1200,TimeUnit.MILLISECONDS);}catch(InterruptedException ignored){}}
            capture.close();if(view!=null)view.release();thread.quitSafely();try{thread.join(1500);}catch(InterruptedException ignored){}
        }
    }
    public void onSurfaceTextureAvailable(SurfaceTexture texture,int w,int h){updatePreview();resumeLanguageStream();}
    public void onSurfaceTextureSizeChanged(SurfaceTexture texture,int w,int h){layoutOrientation();updatePreview();}
    public boolean onSurfaceTextureDestroyed(SurfaceTexture texture){halt();return true;}
    public void onSurfaceTextureUpdated(SurfaceTexture texture){}
}
