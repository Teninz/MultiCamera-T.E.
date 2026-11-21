package ru.codex.obslancamera;

import android.app.*;
import android.content.*;
import android.content.pm.ServiceInfo;
import android.hardware.display.*;
import android.media.*;
import android.media.projection.*;
import android.os.*;
import android.util.DisplayMetrics;
import android.view.Surface;
import java.nio.ByteBuffer;

/** User-consented MediaProjection; never uses accessibility or reuses consent. */
public final class ScreenCaptureService extends Service {
    static volatile boolean active;
    static volatile String status="";
    private volatile boolean stopping;
    private volatile Wire wire;
    private volatile long contentSize;
    private volatile int density=160;
    private void contentSize(int width,int height){contentSize=((long)width<<32)|(height&0xffffffffL);}
    private MediaProjection projection;
    private VirtualDisplay display;
    private Thread worker;private ScreenPerformanceOverlay performance;
    private DisplayManager displays;
    private final Handler main=new Handler(Looper.getMainLooper());
    private final Runnable watchdog=new Runnable(){public void run(){Wire channel=wire;if(channel!=null&&channel.writeStarted!=0&&System.nanoTime()-channel.writeStarted>1500000000L)channel.close();if(!stopping)main.postDelayed(this,300);}};
    private final DisplayManager.DisplayListener changes=new DisplayManager.DisplayListener(){public void onDisplayAdded(int id){}public void onDisplayRemoved(int id){}public void onDisplayChanged(int id){if(Build.VERSION.SDK_INT<34)physicalSize();}};
    private final MediaProjection.Callback projectionCallback=new MediaProjection.Callback(){
        public void onStop(){shutdown();}
        public void onCapturedContentResize(int width,int height){if(width>1&&height>1){contentSize(width,height);}}
    };
    public IBinder onBind(Intent intent){return null;}
    private void log(String value){android.util.Log.i("OBSLANCamera",value);}
    private void physicalSize(){DisplayMetrics metrics=new DisplayMetrics();android.view.Display d=displays.getDisplay(android.view.Display.DEFAULT_DISPLAY);if(d!=null){d.getRealMetrics(metrics);if(metrics.widthPixels>1&&metrics.heightPixels>1){contentSize(metrics.widthPixels,metrics.heightPixels);density=metrics.densityDpi;}}}
    private Notification notification(){
        PendingIntent open=PendingIntent.getActivity(this,30,new Intent(this,MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_SINGLE_TOP),PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);
        PendingIntent stop=PendingIntent.getService(this,31,new Intent(this,ScreenCaptureService.class).setAction("stop"),PendingIntent.FLAG_IMMUTABLE);
        return new Notification.Builder(this,"screen-capture").setSmallIcon(android.R.drawable.ic_menu_view).setContentTitle("MultiCamera T.E.")
            .setContentText(Language.text("Трансляция экрана включена")).setContentIntent(open).setOngoing(true).setCategory(Notification.CATEGORY_SERVICE)
            .addAction(new Notification.Action.Builder(null,Language.text("Остановить передачу"),stop).build()).addAction(new Notification.Action.Builder(null,Language.text("Панель"),PendingIntent.getService(this,32,new Intent(this,ScreenCaptureService.class).setAction("panel"),PendingIntent.FLAG_IMMUTABLE)).build()).build();
    }
    public int onStartCommand(Intent intent,int flags,int startId){
        if(intent==null||"stop".equals(intent.getAction())){shutdown();return START_NOT_STICKY;}
        if(worker!=null){if("panel".equals(intent.getAction())&&performance!=null)performance.toggle();if("overlay-refresh".equals(intent.getAction())&&performance!=null)performance.enabled(intent.getBooleanExtra("overlay",false));return START_NOT_STICKY;}
        if(intent.getAction()!=null){stopSelf();return START_NOT_STICKY;}
        try{
            boolean mic=intent.getBooleanExtra("mic",false);NotificationManager notifications=(NotificationManager)getSystemService(NOTIFICATION_SERVICE);
            notifications.createNotificationChannel(new NotificationChannel("screen-capture","MultiCamera T.E. Screen",NotificationManager.IMPORTANCE_LOW));
            if(Build.VERSION.SDK_INT>=29)startForeground(42,notification(),ServiceInfo.FOREGROUND_SERVICE_TYPE_MEDIA_PROJECTION|(Build.VERSION.SDK_INT>=30&&mic?ServiceInfo.FOREGROUND_SERVICE_TYPE_MICROPHONE:0));else startForeground(42,notification());
            Intent consent=intent.getParcelableExtra("consent");if(intent.getIntExtra("result",Activity.RESULT_CANCELED)!=Activity.RESULT_OK||consent==null)throw new SecurityException("Missing screen capture consent");
            projection=((MediaProjectionManager)getSystemService(MEDIA_PROJECTION_SERVICE)).getMediaProjection(Activity.RESULT_OK,consent);
            projection.registerCallback(projectionCallback,main);displays=(DisplayManager)getSystemService(DISPLAY_SERVICE);physicalSize();displays.registerDisplayListener(changes,main);
            performance=new ScreenPerformanceOverlay(this,this::shutdown,()->{Intent open=new Intent(this,MainActivity.class).setFlags(Intent.FLAG_ACTIVITY_NEW_TASK|Intent.FLAG_ACTIVITY_SINGLE_TOP);PendingIntent pending=PendingIntent.getActivity(this,30,open,PendingIntent.FLAG_IMMUTABLE|PendingIntent.FLAG_UPDATE_CURRENT);try{pending.send();}catch(PendingIntent.CanceledException ignored){}});performance.enabled(intent.getBooleanExtra("overlay",false));
            active=true;status=Language.text("Подключение…");main.post(watchdog);
            worker=new Thread(()->runStream(intent),"multicamera-screen");worker.start();return START_NOT_STICKY;
        }catch(Exception error){log("Screen start: "+EncoderCapture.diagnostic(error));status=Language.text("Не удалось запустить трансляцию экрана");shutdown();return START_NOT_STICKY;}
    }
    private Wire connect(Intent options)throws Exception{
        String address=options.getStringExtra("address"),key=options.getStringExtra("key");
        if(address==null||key==null||!key.matches("[0-9a-f]{32}"))throw new IllegalArgumentException("Invalid pairing");
        if(address.startsWith("wss://"))return new Wire(WssSocket.open(address),key);
        int colon=address.lastIndexOf(':');return new Wire(colon<0?address:address.substring(0,colon),colon<0?34872:Integer.parseInt(address.substring(colon+1)),key);
    }
    private void runStream(Intent options){
        java.util.Set<String> rejected=new java.util.HashSet<>();
        try{while(!stopping){
            MediaCodec codec=null;Surface surface=null;PhoneAudio audio=null;
            try{
                long capturedSize=contentSize;int sourceWidth=(int)(capturedSize>>>32),sourceHeight=(int)capturedSize;int[] size=ScreenCapturePolicy.size(sourceWidth,sourceHeight);
                wire=connect(options);if(stopping)break;
                int fps=options.getIntExtra("fps",30);if(!ScreenCapturePolicy.rateAllowed(fps,ScreenCaptureModes.displayMaximum(this)))fps=30;String encoder=ScreenCaptureModes.encoder(size[0],size[1],fps,rejected);if(encoder==null)throw new IllegalStateException("No screen encoder for selected FPS");
                MediaFormat format=MediaFormat.createVideoFormat("video/avc",size[0],size[1]);format.setInteger(MediaFormat.KEY_COLOR_FORMAT,MediaCodecInfo.CodecCapabilities.COLOR_FormatSurface);
                format.setInteger(MediaFormat.KEY_FRAME_RATE,fps);format.setInteger(MediaFormat.KEY_BIT_RATE,Math.max(1000000,(int)Math.min(40000000L,(long)size[0]*size[1]*fps/8)));format.setInteger(MediaFormat.KEY_I_FRAME_INTERVAL,1);
                if(Build.VERSION.SDK_INT>=29)format.setFloat(MediaFormat.KEY_MAX_FPS_TO_ENCODER,(float)fps);
                try{codec=MediaCodec.createByCodecName(encoder);codec.configure(format,null,null,MediaCodec.CONFIGURE_FLAG_ENCODE);surface=codec.createInputSurface();codec.start();}catch(Exception error){rejected.add(encoder+"@"+size[0]+"x"+size[1]+"/"+fps);throw error;}
                // Android 14 permits only one createVirtualDisplay per consent token.
                // Rotation/reconnect replace its Surface and resize the existing display.
                if(display==null)display=projection.createVirtualDisplay("MultiCamera T.E. Screen",size[0],size[1],density,DisplayManager.VIRTUAL_DISPLAY_FLAG_AUTO_MIRROR,surface,null,main);
                else{display.resize(size[0],size[1],density);display.setSurface(surface);}
                if(options.getBooleanExtra("mic",false))audio=new PhoneAudio(wire,this::log,()->log("Screen microphone unavailable"));
                status=Language.text("Трансляция экрана включена");log("SCREEN_STREAM "+size[0]+"x"+size[1]+" / "+fps+" FPS");MediaCodec.BufferInfo info=new MediaCodec.BufferInfo();long offset=Long.MIN_VALUE,last=System.nanoTime(),bytesSent=0,sendNs=0;int frames=0;double encoderMs=0;boolean encoderKnown=false;
                while(!stopping&&capturedSize==contentSize){
                    int index=codec.dequeueOutputBuffer(info,10000);
                    if(index==MediaCodec.INFO_OUTPUT_FORMAT_CHANGED){MediaFormat output=codec.getOutputFormat();for(String name:new String[]{"csd-0","csd-1"}){ByteBuffer bytes=output.getByteBuffer(name);if(bytes!=null){ByteBuffer copy=bytes.duplicate();byte[] config=new byte[copy.remaining()];copy.get(config);wire.send(1,0,0,config);}}}
                    else if(index>=0)try{ByteBuffer output=codec.getOutputBuffer(index);if(output!=null&&info.size>0){output.position(info.offset);output.limit(info.offset+info.size);byte[] data=new byte[info.size];output.get(data);int type=(info.flags&MediaCodec.BUFFER_FLAG_CODEC_CONFIG)!=0?1:2;long pts=info.presentationTimeUs;if(type==2){long now=SystemClock.elapsedRealtimeNanos()/1000;if(offset==Long.MIN_VALUE)offset=Math.abs(now-pts)<10000000L?0:now-pts-1000000L/fps;pts+=offset;}long beforeSend=System.nanoTime();wire.send(type,0,pts,data);if(type==2){frames++;bytesSent+=data.length;sendNs+=System.nanoTime()-beforeSend;double age=(beforeSend/1000-info.presentationTimeUs)/1000.0;if(age>=0&&age<5000){encoderMs=age;encoderKnown=true;}}}}finally{codec.releaseOutputBuffer(index,false);}
                    long now=System.nanoTime();if(now-last>2000000000L){double actual=frames*1e9/(now-last);log("SCREEN_ENCODED_FPS "+actual+" target="+fps);status=Language.text("Экран")+String.format(java.util.Locale.ROOT," · %.1f / %d FPS",actual,fps);double mbps=ScreenCapturePolicy.mbps(bytesSent,now-last),sendMs=ScreenCapturePolicy.sendMs(sendNs,frames);final int chosen=fps;final Double age=encoderKnown?encoderMs:null;main.post(()->{if(performance!=null)performance.update(actual,chosen,mbps,sendMs,age);});frames=0;bytesSent=0;sendNs=0;last=now;}
                }
            }catch(Exception error){if(!stopping){log("Screen stream: "+EncoderCapture.diagnostic(error));status=Language.text("Подключение…");try{Thread.sleep(1500);}catch(InterruptedException ignored){}}}
            finally{
                if(audio!=null)audio.close();if(display!=null)try{display.setSurface(null);}catch(Exception ignored){}
                Wire channel=wire;wire=null;if(channel!=null)channel.close();
                if(codec!=null){try{codec.stop();}catch(Exception ignored){}try{codec.release();}catch(Exception ignored){}}if(surface!=null)surface.release();
            }
        }}finally{
            if(display!=null){display.release();display=null;}active=false;main.post(()->stopSelf());
        }
    }
    private void shutdown(){stopping=true;active=false;if(performance!=null)performance.remove();main.removeCallbacks(watchdog);Wire channel=wire;if(channel!=null)channel.close();Thread thread=worker;if(thread!=null)thread.interrupt();stopSelf();}
    public void onDestroy(){shutdown();if(displays!=null)displays.unregisterDisplayListener(changes);if(projection!=null){projection.unregisterCallback(projectionCallback);projection.stop();projection=null;}stopForeground(true);super.onDestroy();}
}
