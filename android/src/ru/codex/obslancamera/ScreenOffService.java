package ru.codex.obslancamera;

import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.hardware.camera2.*;
import android.media.*;
import android.os.*;
import java.nio.ByteBuffer;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

/** Explicitly started while the activity is visible. Owns camera without an Activity or TextureView. */
public final class ScreenOffService extends Service {
    static volatile boolean active;
    private volatile boolean stopping;
    private volatile Wire wire;
    private Thread worker;
    private PowerManager.WakeLock wake;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final Runnable watchdog=new Runnable(){public void run(){Wire w=wire;if(w!=null&&w.writeStarted!=0&&System.nanoTime()-w.writeStarted>1500000000L)w.close();if(!stopping)main.postDelayed(this,300);}};
    public IBinder onBind(Intent intent){return null;}
    private void log(String value){android.util.Log.i("OBSLANCamera",value);}
    private Notification notification(String text){
        Intent open=new Intent(this,MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_SINGLE_TOP);
        PendingIntent content=PendingIntent.getActivity(this,0,open,PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        Intent stop=new Intent(this,ScreenOffService.class).setAction("stop");
        PendingIntent cancel=PendingIntent.getService(this,1,stop,PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Builder(this,"camera-stream").setSmallIcon(android.R.drawable.presence_video_online).setContentTitle("MultiCamera T.E.")
            .setContentText(text).setContentIntent(content).setOngoing(true).setCategory(Notification.CATEGORY_SERVICE)
            .addAction(new Notification.Action.Builder(null,Language.text("Остановить передачу"),cancel).build()).build();
    }
    public int onStartCommand(Intent intent,int flags,int startId){
        if(intent==null||"stop".equals(intent.getAction())){stopSelf();return START_NOT_STICKY;}
        if(worker!=null)return START_NOT_STICKY;
        try {
            NotificationManager manager=(NotificationManager)getSystemService(NOTIFICATION_SERVICE);
            manager.createNotificationChannel(new NotificationChannel("camera-stream","MultiCamera T.E.",NotificationManager.IMPORTANCE_LOW));
            boolean mic=intent.getBooleanExtra("mic",false);
            Notification notification=notification(Language.text("Передача камеры. Экран можно выключить кнопкой питания."));
            if(Build.VERSION.SDK_INT>=30)startForeground(41,notification,ServiceInfo.FOREGROUND_SERVICE_TYPE_CAMERA|(mic?ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE:0));else startForeground(41,notification);
            wake=((PowerManager)getSystemService(POWER_SERVICE)).newWakeLock(PowerManager.PARTIAL_WAKE_LOCK,"MultiCameraTE:stream");wake.setReferenceCounted(false);wake.acquire(600000);
            active=true;main.post(watchdog);
            worker=new Thread(()->{
                try{while(!stopping){
                    try{String address=intent.getStringExtra("address"),key=intent.getStringExtra("key");
                        Wire channel;if(address.startsWith("wss://"))channel=new Wire(WssSocket.open(address),key);
                        else{int colon=address.lastIndexOf(':');channel=new Wire(colon<0?address:address.substring(0,colon),colon<0?34872:Integer.parseInt(address.substring(colon+1)),key);}
                        wire=channel;if(stopping)break;record(channel,intent);
                    }catch(Exception error){if(!stopping){log("Screen-off: "+EncoderCapture.diagnostic(error));manager.notify(41,notification(Language.text("Повтор подключения камеры…")));}}
                    finally{Wire channel=wire;wire=null;if(channel!=null)channel.close();}
                    if(!stopping)try{Thread.sleep(1500);}catch(InterruptedException ignored){}
                }}finally{active=false;main.post(()->stopSelf());}
            },"multicamera-screen-off");worker.start();return START_NOT_STICKY;
        }catch(Exception error){log("Screen-off start: "+EncoderCapture.diagnostic(error));stopSelf();return START_NOT_STICKY;}
    }
    private void record(Wire channel,Intent options) throws Exception {
        CameraManager manager=(CameraManager)getSystemService(CAMERA_SERVICE);String id=options.getStringExtra("camera");CameraCharacteristics caps=manager.getCameraCharacteristics(id);
        int choice=options.getIntExtra("stabilization",0);CameraModes.Mode mode=new CameraModes.Mode(options.getIntExtra("width",1280),options.getIntExtra("height",720),options.getIntExtra("fps",30));
        // Headless high-speed capture requires a preview target on some HALs. Use the normal 30fps path explicitly.
        if(choice==1||CameraModes.highSpeed(caps,mode.width,mode.height,mode.fps)){CameraModes.Mode compatible=CameraModes.stabilized(CameraModes.discover(caps));if(compatible!=null)mode=compatible;}
        EncoderCapture capture=EncoderCapture.open(caps,mode,new HashSet<>(),this::log);
        HandlerThread control=new HandlerThread("screen-off-camera");control.start();Handler handler=new Handler(control.getLooper());
        AtomicReference<CameraDevice> device=new AtomicReference<>();AtomicReference<CameraCaptureSession> session=new AtomicReference<>();AtomicReference<Exception> failure=new AtomicReference<>();
        CountDownLatch opened=new CountDownLatch(1),configured=new CountDownLatch(1),closed=new CountDownLatch(1);AtomicBoolean ending=new AtomicBoolean();PhoneAudio audio=null;
        try {
            capture.receiveImages(handler,failure);
            manager.openCamera(id,new CameraDevice.StateCallback(){public void onOpened(CameraDevice d){if(ending.get()||stopping)d.close();else device.set(d);opened.countDown();}public void onDisconnected(CameraDevice d){failure.set(new Exception("Camera disconnected"));d.close();opened.countDown();}public void onError(CameraDevice d,int error){failure.set(new Exception("Camera error "+error));d.close();opened.countDown();}public void onClosed(CameraDevice d){closed.countDown();}},handler);
            if(!opened.await(5,TimeUnit.SECONDS)||device.get()==null)throw new Exception("Camera start timeout");
            CameraStabilization stabilization=new CameraStabilization(caps,capture.width,capture.height,capture.fps,choice==2,capture.bufferInput);
            CaptureRequest.Builder request=device.get().createCaptureRequest(CameraDevice.TEMPLATE_RECORD);request.addTarget(capture.surface);
            request.set(CaptureRequest.CONTROL_MODE,CaptureRequest.CONTROL_MODE_AUTO);request.set(CaptureRequest.CONTROL_CAPTURE_INTENT,CaptureRequest.CONTROL_CAPTURE_INTENT_VIDEO_RECORD);
            android.util.Range<Integer> range=CameraModes.fpsRange(caps,capture.fps);if(range!=null)request.set(CaptureRequest.CONTROL_AE_TARGET_FPS_RANGE,range);stabilization.apply(request);
            CameraCaptureSession.CaptureCallback callback=new CameraCaptureSession.CaptureCallback(){String last="";public void onCaptureCompleted(CameraCaptureSession s,CaptureRequest r,TotalCaptureResult result){if(ending.get()||stopping)return;boolean retry=stabilization.result(result,capture.fps);String observed=stabilization.actual();if(!last.equals(observed)){last=observed;log("Screen-off "+observed);}if(retry)try{stabilization.apply(request);s.setRepeatingRequest(request.build(),this,handler);}catch(Exception error){failure.compareAndSet(null,error);}}};
            CameraCaptureSession.StateCallback ready=new CameraCaptureSession.StateCallback(){public void onConfigured(CameraCaptureSession s){if(ending.get()||stopping){s.close();configured.countDown();return;}session.set(s);try{s.setRepeatingRequest(request.build(),callback,handler);}catch(Exception error){failure.set(error);}configured.countDown();}public void onConfigureFailed(CameraCaptureSession s){failure.set(new Exception("Headless session rejected"));configured.countDown();}};
            if(Build.VERSION.SDK_INT>=28){android.hardware.camera2.params.SessionConfiguration config=new android.hardware.camera2.params.SessionConfiguration(0,Collections.singletonList(new android.hardware.camera2.params.OutputConfiguration(capture.surface)),r->handler.post(r),ready);config.setSessionParameters(request.build());device.get().createCaptureSession(config);}else device.get().createCaptureSession(Collections.singletonList(capture.surface),ready,handler);
            if(!configured.await(5,TimeUnit.SECONDS)||failure.get()!=null)throw failure.get()!=null?failure.get():new Exception("Session timeout");
            if(options.getBooleanExtra("mic",false))audio=new PhoneAudio(channel,this::log,()->log("Screen-off microphone unavailable"));
            ((NotificationManager)getSystemService(NOTIFICATION_SERVICE)).notify(41,notification(Language.text("Передача камеры. Экран можно выключить кнопкой питания.")+" "+capture.width+"×"+capture.height+" / "+capture.fps+" FPS"));
            MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();long renewal=SystemClock.elapsedRealtime(),ptsOffset=Long.MIN_VALUE;
            while(!stopping&&failure.get()==null){int index=capture.codec.dequeueOutputBuffer(info,10000);if(index==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED){MediaFormat format=capture.codec.getOutputFormat();for(String name:new String[]{"csd-0","csd-1"}){ByteBuffer buffer=format.getByteBuffer(name);if(buffer!=null){byte[] bytes=new byte[buffer.remaining()];buffer.get(bytes);channel.send(1,options.getIntExtra("rotation",0),0,bytes);}}}else if(index>=0)try{ByteBuffer buffer=capture.codec.getOutputBuffer(index);if(buffer!=null&&info.size>0){buffer.position(info.offset);buffer.limit(info.offset+info.size);byte[] bytes=new byte[info.size];buffer.get(bytes);int type=(info.flags&MediaCodec.BUFFER_FLAG_CODEC_CONFIG)!=0?1:2;long pts=info.presentationTimeUs;if(type==2){long now=SystemClock.elapsedRealtimeNanos()/1000;if(ptsOffset==Long.MIN_VALUE)ptsOffset=Math.abs(now-pts)<10000000L?0:now-pts-1000000L/capture.fps;pts+=ptsOffset;}channel.send(type,options.getIntExtra("rotation",0),pts,bytes);}}finally{capture.codec.releaseOutputBuffer(index,false);}
                if(!stopping&&SystemClock.elapsedRealtime()-renewal>60000){wake.acquire(600000);renewal=SystemClock.elapsedRealtime();}}
            if(failure.get()!=null)throw failure.get();
        }finally{ending.set(true);if(audio!=null)audio.close();CameraCaptureSession s=session.get();if(s!=null)s.close();CameraDevice d=device.get();if(d!=null){d.close();try{closed.await(1500,TimeUnit.MILLISECONDS);}catch(InterruptedException ignored){}}capture.close();control.quitSafely();try{control.join(1500);}catch(InterruptedException ignored){}}
    }
    public void onTaskRemoved(Intent intent){stopSelf();}
    public void onDestroy(){stopping=true;active=false;main.removeCallbacks(watchdog);Wire channel=wire;if(channel!=null)channel.close();if(worker!=null)worker.interrupt();if(wake!=null&&wake.isHeld())wake.release();stopForeground(true);super.onDestroy();}
}
