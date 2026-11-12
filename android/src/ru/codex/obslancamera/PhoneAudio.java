package ru.codex.obslancamera;
import android.media.*;
import android.os.SystemClock;
import java.nio.*;
import java.util.function.Consumer;
/** PCM mono, 20 ms records. No microphone resource is held while this object is stopped. */
final class PhoneAudio implements AutoCloseable {
    private volatile boolean stop;private volatile AudioRecord record;private Thread thread;
    PhoneAudio(Wire wire,Consumer<String> log,Runnable captureFailure){
        thread=new Thread(()->{
            AudioRecord audio=null;
            try{
                int rate=48000;int minimum=AudioRecord.getMinBufferSize(rate,AudioFormat.CHANNEL_IN_MONO,AudioFormat.ENCODING_PCM_16BIT);
                if(minimum<=0){rate=44100;minimum=AudioRecord.getMinBufferSize(rate,AudioFormat.CHANNEL_IN_MONO,AudioFormat.ENCODING_PCM_16BIT);}
                if(minimum<=0)throw new IllegalStateException(Language.text("Не найден поддерживаемый формат микрофона"));
                audio=new AudioRecord(MediaRecorder.AudioSource.MIC,rate,AudioFormat.CHANNEL_IN_MONO,AudioFormat.ENCODING_PCM_16BIT,Math.max(minimum,rate*2/5));record=audio;
                if(audio.getState()!=AudioRecord.STATE_INITIALIZED)throw new IllegalStateException(Language.text("Не удалось открыть микрофон"));
                if(stop)return;audio.startRecording();log.accept(Language.text("Микрофон: PCM16 mono ")+rate+" Hz");short[] samples=new short[rate/50];long position=0;AudioTimestamp timestamp=new AudioTimestamp();
                while(!stop){int n=audio.read(samples,0,samples.length,AudioRecord.READ_BLOCKING);if(stop)break;if(n<=0)throw new IllegalStateException("AudioRecord.read: "+n);
                    long ns=SystemClock.elapsedRealtimeNanos()-(long)n*1000000000L/rate;
                    if(audio.getTimestamp(timestamp,AudioTimestamp.TIMEBASE_BOOTTIME)==AudioRecord.SUCCESS&&timestamp.nanoTime>0)ns=timestamp.nanoTime+(position-timestamp.framePosition)*1000000000L/rate;
                    position+=n;ByteBuffer data=ByteBuffer.allocate(5+n*2);data.putInt(rate).put((byte)1);data.order(ByteOrder.LITTLE_ENDIAN);for(int i=0;i<n;i++)data.putShort(samples[i]);
                    wire.send(4,0,Math.max(0,ns/1000),data.array());
                }
            }catch(Exception e){if(!stop){log.accept(Language.text("Микрофон: ")+EncoderCapture.diagnostic(e));if(!(e instanceof java.io.IOException))captureFailure.run();}}
            finally{record=null;if(audio!=null){try{audio.stop();}catch(Exception ignored){}audio.release();}log.accept(Language.text("Микрофон остановлен"));}
        },"obs-phone-audio");thread.start();
    }
    public void close(){stop=true;AudioRecord audio=record;if(audio!=null)try{audio.stop();}catch(Exception ignored){} }
}
