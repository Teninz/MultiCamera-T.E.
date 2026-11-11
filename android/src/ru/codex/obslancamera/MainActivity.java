package ru.codex.obslancamera;

import android.Manifest;
import android.app.*;
import android.os.*;
import android.content.*;
import android.content.pm.PackageManager;
import android.graphics.*;
import android.hardware.camera2.*;
import android.media.*;
import android.net.Uri;
import android.util.*;
import android.view.*;
import android.widget.*;
import java.nio.ByteBuffer;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.AtomicReference;

public class MainActivity extends Activity implements TextureView.SurfaceTextureListener {
    private EditText address,token;
    private TextView status;
    private Spinner cameraChoice,quality;
    private TextureView preview;
    private Button connect,stop,scan;
    private volatile boolean running;
    private volatile Thread worker;
    private volatile Wire wire;
    private volatile int sensorOrientation=90;
    private volatile boolean front;
    private final Handler ui=new Handler(Looper.getMainLooper());
    private final Runnable watchdog=new Runnable(){public void run(){Wire w=wire;if(w!=null&&w.writeStarted!=0&&System.nanoTime()-w.writeStarted>1500000000L)w.close();if(running)ui.postDelayed(this,300);}};
    private static final int PERMISSION=10,SCAN=11;
    private int dp(int n){return (int)(n*getResources().getDisplayMetrics().density+.5f);}
    private TextView text(String s,int sp,int color){TextView v=new TextView(this);v.setText(s);v.setTextSize(sp);v.setTextColor(color);return v;}
    private void message(String s){ui.post(()->status.setText(s));}
    @Override public void onCreate(Bundle state){super.onCreate(state);getWindow().setStatusBarColor(0xfff3f6f8);getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR);
        LinearLayout root=new LinearLayout(this);root.setOrientation(1);root.setPadding(dp(20),dp(12),dp(20),dp(12));root.setBackgroundColor(0xfff3f6f8);
        root.addView(text("OBS LAN CAMERA",12,0xff147b6c));root.addView(text("Камера для OBS",28,0xff203246));
        status=text("Подключите смартфон к Wi-Fi той же сети, что и ПК.",14,0xff637486);root.addView(status,new LinearLayout.LayoutParams(-1,dp(48)));
        preview=new TextureView(this);preview.setSurfaceTextureListener(this);root.addView(preview,new LinearLayout.LayoutParams(-1,0,1));
        LinearLayout choices=new LinearLayout(this);cameraChoice=new Spinner(this);cameraChoice.setAdapter(new ArrayAdapter<String>(this,android.R.layout.simple_spinner_dropdown_item,new String[]{"Задняя камера","Передняя камера"}));
        quality=new Spinner(this);quality.setAdapter(new ArrayAdapter<String>(this,android.R.layout.simple_spinner_dropdown_item,new String[]{"720p · 30 кадров/с","1080p · 30 кадров/с","480p · 30 кадров/с"}));choices.addView(cameraChoice,new LinearLayout.LayoutParams(0,dp(50),1));choices.addView(quality,new LinearLayout.LayoutParams(0,dp(50),1));root.addView(choices);
        address=new EditText(this);address.setSingleLine();address.setTextSize(16);address.setHint("Адрес ПК, например 192.168.3.3:34871");address.setInputType(1);root.addView(address);
        token=new EditText(this);token.setSingleLine();token.setTextSize(14);token.setHint("Ключ подключения из OBS");token.setInputType(android.text.InputType.TYPE_CLASS_TEXT|android.text.InputType.TYPE_TEXT_VARIATION_VISIBLE_PASSWORD);root.addView(token);
        scan=new Button(this);scan.setText("Сканировать QR с компьютера");root.addView(scan);scan.setOnClickListener(v->{if(permission())startActivityForResult(new Intent(this,ScanActivity.class),SCAN);});
        LinearLayout actions=new LinearLayout(this);connect=new Button(this);connect.setText("Подключить");stop=new Button(this);stop.setText("Остановить");stop.setEnabled(false);actions.addView(connect,new LinearLayout.LayoutParams(0,dp(52),1));actions.addView(stop,new LinearLayout.LayoutParams(0,dp(52),1));root.addView(actions);
        TextView note=text("Видео идёт напрямую в OBS и шифруется. Микрофон не используется. Во время передачи оставьте приложение открытым.",12,0xff68788a);root.addView(note);setContentView(root);
        SharedPreferences saved=getPreferences(0);address.setText(saved.getString("address",""));token.setText(saved.getString("token",""));
        connect.setOnClickListener(v->start());stop.setOnClickListener(v->halt());if(getIntent().getData()!=null)pair(getIntent().getData().toString());
    }
    private boolean permission(){if(checkSelfPermission(Manifest.permission.CAMERA)==PackageManager.PERMISSION_GRANTED)return true;requestPermissions(new String[]{Manifest.permission.CAMERA},PERMISSION);return false;}
    @Override public void onRequestPermissionsResult(int request,String[] permissions,int[] grants){super.onRequestPermissionsResult(request,permissions,grants);if(request==PERMISSION){if(grants.length>0&&grants[0]==PackageManager.PERMISSION_GRANTED)message("Камера разрешена. Нажмите подключение или сканирование QR.");else message("Для передачи видео нужен доступ к камере.");}}
    private void pair(String value){try{Uri u=Uri.parse(value);if(!"obslan".equals(u.getScheme())||u.getHost()==null||u.getFragment()==null||!u.getFragment().matches("[0-9a-f]{32}"))throw new IllegalArgumentException();address.setText(u.getHost()+":"+(u.getPort()>0?u.getPort():34871));token.setText(u.getFragment());message("Сопряжение заполнено. Нажмите «Подключить».");}catch(Exception e){message("Это не QR подключения OBS LAN Camera.");}}
    @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);if(request==SCAN&&result==RESULT_OK&&data!=null)pair(data.getStringExtra("pair"));}
    private void controls(boolean active){connect.setEnabled(!active);stop.setEnabled(active);scan.setEnabled(!active);address.setEnabled(!active);token.setEnabled(!active);cameraChoice.setEnabled(!active);quality.setEnabled(!active);}
    private void start(){if(running||worker!=null)return;if(!permission())return;if(!preview.isAvailable()){message("Ожидаем поверхность предпросмотра. Попробуйте ещё раз.");return;}
        String endpoint=address.getText().toString().trim(),secret=token.getText().toString().trim().toLowerCase(Locale.ROOT);int port=34871;String host=endpoint;
        try{int colon=endpoint.indexOf(':');if(colon>=0){host=endpoint.substring(0,colon);port=Integer.parseInt(endpoint.substring(colon+1));}if(port<1024||port>65535||!secret.matches("[0-9a-f]{32}"))throw new IllegalArgumentException();}catch(Exception e){message("Проверьте адрес ПК, порт и 32-символьный ключ.");return;}
        getPreferences(0).edit().putString("address",endpoint).putString("token",secret).apply();running=true;controls(true);getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);ui.post(watchdog);
        final String target=host;final int targetPort=port,selection=quality.getSelectedItemPosition();final boolean useFront=cameraChoice.getSelectedItemPosition()==1;
        worker=new Thread(()->{try{while(running){try{message("Подключаемся к OBS…");Wire channel=new Wire(target,targetPort,secret);wire=channel;if(!running){channel.close();break;}stream(channel,useFront,selection);}catch(Exception e){if(running)message("Связь прервана: "+friendly(e)+". Повтор через 2 секунды…");}finally{Wire w=wire;wire=null;if(w!=null)w.close();}if(running)try{Thread.sleep(2000);}catch(InterruptedException ignored){}}}finally{worker=null;ui.post(()->{running=false;controls(false);getWindow().clearFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);status.setText("Передача остановлена.");});}},"obs-camera-stream");worker.start();
    }
    private String friendly(Exception e){if(e instanceof java.net.ConnectException)return "приёмник недоступен — проверьте IP, OBS и брандмауэр";if(e instanceof java.net.SocketTimeoutException)return "OBS не ответил — проверьте ключ и сеть";if(e instanceof java.io.EOFException)return "OBS отклонил ключ или уже подключён другой телефон";String s=e.getMessage();return s==null?e.getClass().getSimpleName():s;}
    private void halt(){running=false;ui.removeCallbacks(watchdog);Wire w=wire;if(w!=null)w.close();Thread t=worker;if(t!=null)t.interrupt();message("Останавливаем камеру…");}
    @Override protected void onPause(){super.onPause();halt();}
    private int rotation(){int display=getWindowManager().getDefaultDisplay().getRotation()*90;return front?(sensorOrientation+display)%360:(sensorOrientation-display+360)%360;}
    private void transform(int w,int h){ui.post(()->{if(!preview.isAvailable())return;Matrix matrix=new Matrix();RectF view=new RectF(0,0,preview.getWidth(),preview.getHeight());boolean swap=rotation()==90||rotation()==270;RectF video=new RectF(0,0,swap?h:w,swap?w:h);matrix.setRectToRect(video,view,Matrix.ScaleToFit.CENTER);/* TextureView starts with a stretched buffer; compensate below. */
        Matrix m=new Matrix();float cx=view.centerX(),cy=view.centerY();int rot=rotation();m.postRotate(rot,cx,cy);if(rot==90||rot==270){float factor=Math.max(view.width()/h,view.height()/w);m.postScale((h*factor)/view.width(),(w*factor)/view.height(),cx,cy);}if(front)m.postScale(-1,1,cx,cy);preview.setTransform(m);});}
    private void stream(Wire channel,boolean useFront,int selection) throws Exception {
        CameraManager manager=(CameraManager)getSystemService(CAMERA_SERVICE);String id=null;CameraCharacteristics chars=null;
        for(String candidate:manager.getCameraIdList()){CameraCharacteristics c=manager.getCameraCharacteristics(candidate);Integer facing=c.get(CameraCharacteristics.LENS_FACING);if(facing!=null&&facing==(useFront?CameraCharacteristics.LENS_FACING_FRONT:CameraCharacteristics.LENS_FACING_BACK)){id=candidate;chars=c;break;}}
        if(id==null||chars==null)throw new Exception("Выбранная камера не найдена");front=useFront;Integer orientation=chars.get(CameraCharacteristics.SENSOR_ORIENTATION);sensorOrientation=orientation==null?90:orientation;
        int targetW=selection==1?1920:selection==2?640:1280,targetH=selection==1?1080:selection==2?480:720;
        Size[] sizes=chars.get(CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP).getOutputSizes(MediaCodec.class);if(sizes==null||sizes.length==0)throw new Exception("Камера не поддерживает видеокодирование");
        Size best=null;double score=Double.MAX_VALUE;for(Size size:sizes){if(size.getWidth()>1920||size.getHeight()>1080||size.getWidth()<320)continue;double s=Math.abs(Math.log((double)size.getWidth()*size.getHeight()/(targetW*targetH)))+Math.abs((double)size.getWidth()/size.getHeight()-(double)targetW/targetH)*4;if(s<score){score=s;best=size;}}
        if(best==null)throw new Exception("Не найден подходящий размер камеры");final int w=best.getWidth(),h=best.getHeight();
        MediaCodec encoder=null;Surface input=null,view=null;HandlerThread cameraThread=new HandlerThread("obs-camera-control");cameraThread.start();Handler cameraHandler=new Handler(cameraThread.getLooper());
        AtomicReference<CameraDevice> device=new AtomicReference<>();AtomicReference<CameraCaptureSession> session=new AtomicReference<>();AtomicReference<Exception> failure=new AtomicReference<>();CountDownLatch opened=new CountDownLatch(1),configured=new CountDownLatch(1);
        try {
            encoder=MediaCodec.createEncoderByType("video/avc");MediaFormat format=MediaFormat.createVideoFormat("video/avc",w,h);format.setInteger(MediaFormat.KEY_COLOR_FORMAT,MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);format.setInteger(MediaFormat.KEY_BIT_RATE,selection==1?8000000:selection==2?2000000:4500000);format.setInteger(MediaFormat.KEY_FRAME_RATE,30);format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL,1);format.setInteger(MediaFormat.KEY_COLOR_STANDARD,MediaFormat.COLOR_STANDARD_BT709);format.setInteger(MediaFormat.KEY_COLOR_RANGE,MediaFormat.COLOR_RANGE_LIMITED);format.setInteger(MediaFormat.KEY_PRIORITY,0);format.setInteger(MediaFormat.KEY_PROFILE,MediaCodecInfo.CodecProfileLevel.AVCProfileBaseline);if(Build.VERSION.SDK_INT>=29)format.setInteger(MediaFormat.KEY_MAX_B_FRAMES,0);
            encoder.configure(format,null,null,MediaCodec.CONFIGURE_FLAG_ENCODE);input=encoder.createInputSurface();encoder.start();
            if(!preview.isAvailable())throw new Exception("Предпросмотр закрыт");SurfaceTexture texture=preview.getSurfaceTexture();texture.setDefaultBufferSize(w,h);view=new Surface(texture);transform(w,h);
            manager.openCamera(id,new CameraDevice.StateCallback(){public void onOpened(CameraDevice d){device.set(d);opened.countDown();}public void onDisconnected(CameraDevice d){failure.set(new Exception("Камера отключена"));d.close();opened.countDown();}public void onError(CameraDevice d,int error){failure.set(new Exception("Камера занята или недоступна: "+error));d.close();opened.countDown();}},cameraHandler);
            if(!opened.await(5,TimeUnit.SECONDS)||device.get()==null||failure.get()!=null)throw failure.get()!=null?failure.get():new Exception("Камера не ответила");
            final Surface encSurface=input,previewSurface=view;final CameraCharacteristics caps=chars;
            device.get().createCaptureSession(Arrays.asList(input,view),new CameraCaptureSession.StateCallback(){public void onConfigured(CameraCaptureSession s){session.set(s);try{CaptureRequest.Builder b=device.get().createCaptureRequest(CameraDevice.TEMPLATE_RECORD);b.addTarget(encSurface);b.addTarget(previewSurface);b.set(CaptureRequest.CONTROL_AF_MODE,CaptureRequest.CONTROL_AF_MODE_CONTINUOUS_VIDEO);
                Range<Integer>[] ranges=caps.get(CameraCharacteristics.CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES);Range<Integer> fps=null;if(ranges!=null)for(Range<Integer> r:ranges)if(r.getUpper()==30&&(fps==null||r.getLower()>fps.getLower()))fps=r;if(fps!=null)b.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE,fps);s.setRepeatingRequest(b.build(),null,cameraHandler);}catch(Exception e){failure.set(e);}configured.countDown();}public void onConfigureFailed(CameraCaptureSession s){failure.set(new Exception("Камера не поддерживает этот режим. Попробуйте 480p."));configured.countDown();}},cameraHandler);
            if(!configured.await(5,TimeUnit.SECONDS)||failure.get()!=null)throw failure.get()!=null?failure.get():new Exception("Не удалось запустить видеопоток");
            message("В эфире камеры: "+w+" × "+h+" · H.264 · без микрофона");MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();long frames=0,lastUpdate=System.nanoTime();
            while(running&&failure.get()==null){int index=encoder.dequeueOutputBuffer(info,10000);if(index==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED){MediaFormat actual=encoder.getOutputFormat();for(String csd:new String[]{"csd-0","csd-1"}){ByteBuffer b=actual.getByteBuffer(csd);if(b!=null){ByteBuffer copy=b.duplicate();byte[] bytes=new byte[copy.remaining()];copy.get(bytes);channel.send(1,rotation(),0,bytes);}}}
                else if(index>=0){try{ByteBuffer b=encoder.getOutputBuffer(index);if(b!=null&&info.size>0){b.position(info.offset);b.limit(info.offset+info.size);byte[] bytes=new byte[info.size];b.get(bytes);int type=(info.flags&MediaCodec.BUFFER_FLAG_CODEC_CONFIG)!=0?1:2;channel.send(type,rotation(),info.presentationTimeUs,bytes);if(type==2)frames++;}}finally{encoder.releaseOutputBuffer(index,false);}}
                long now=System.nanoTime();if(now-lastUpdate>2000000000L){double fps=frames*1e9/(now-lastUpdate);message(String.format(Locale.ROOT,"Подключено к OBS · %d × %d · %.0f кадров/с",w,h,fps));frames=0;lastUpdate=now;}}
            if(failure.get()!=null)throw failure.get();
        } finally {
            CameraCaptureSession s=session.get();if(s!=null)try{s.stopRepeating();s.close();}catch(Exception ignored){}CameraDevice d=device.get();if(d!=null)d.close();
            if(encoder!=null){try{encoder.stop();}catch(Exception ignored){}encoder.release();}if(input!=null)input.release();if(view!=null)view.release();cameraThread.quitSafely();cameraThread.join(1200);
        }
    }
    public void onSurfaceTextureAvailable(SurfaceTexture texture,int w,int h){}
    public void onSurfaceTextureSizeChanged(SurfaceTexture texture,int w,int h){}
    public boolean onSurfaceTextureDestroyed(SurfaceTexture texture){halt();return true;}
    public void onSurfaceTextureUpdated(SurfaceTexture texture){}
}
