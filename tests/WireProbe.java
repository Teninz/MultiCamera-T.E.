import ru.codex.obslancamera.Wire;
import java.io.*;
import java.nio.*;
import java.net.*;
public class WireProbe {
 static String status()throws Exception {try(InputStream in=new URL("http://127.0.0.1:34879/api/status").openStream()){ByteArrayOutputStream b=new ByteArrayOutputStream();byte[] tmp=new byte[4096];int n;while((n=in.read(tmp))>0)b.write(tmp,0,n);return new String(b.toByteArray(),"UTF-8");}}
 static long frames(String s){java.util.regex.Matcher m=java.util.regex.Pattern.compile("\\\"audio_frames\\\"\\s*:\\s*(\\d+)").matcher(s);if(!m.find())throw new AssertionError(s);return Long.parseLong(m.group(1));}
 public static void main(String[] args)throws Exception {
  boolean rejected=false;
  try(Wire invalid=new Wire("127.0.0.1",34879,"11111111111111111111111111111111")){}catch(Exception e){rejected=true;}
  if(!rejected)throw new AssertionError("Wrong key accepted");Thread.sleep(600);
  for(int round=0;round<2;round++){
   try(Wire wire=new Wire("127.0.0.1",34879,"0123456789abcdef0123456789abcdef");DataInputStream file=new DataInputStream(new FileInputStream(args[0]))){
    byte[] sps=java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(args[0]+".sps"));byte[] pps=java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(args[0]+".pps"));wire.send(1,90,0,sps);wire.send(1,90,0,pps);
    wire.send(3,0,0,"probe diagnostics".getBytes("UTF-8"));
    java.util.concurrent.atomic.AtomicBoolean mic=new java.util.concurrent.atomic.AtomicBoolean();Thread audioThread=null;
    int sent=0;while(file.available()>0){int len=file.readInt();byte[] packet=new byte[len];file.readFully(packet);wire.send(2,90,System.nanoTime()/1000,packet);
     if(sent==10){mic.set(true);audioThread=new Thread(()->{try{int position=0;while(mic.get()){ByteBuffer audio=ByteBuffer.allocate(5+1920);audio.putInt(48000).put((byte)1).order(ByteOrder.LITTLE_ENDIAN);for(int i=0;i<960;i++)audio.putShort((short)(Math.sin((position+i)*2*Math.PI*440/48000)*12000));position+=960;wire.send(4,0,System.nanoTime()/1000,audio.array());Thread.sleep(20);}}catch(Exception e){throw new RuntimeException(e);}});audioThread.start();}
     if(sent==35){mic.set(false);audioThread.join();}
     if(sent==45){long a=frames(status());Thread.sleep(250);String muted=status();if(frames(muted)!=a||!muted.replaceAll("\\s+","").contains("\"audio_active\":false"))throw new AssertionError("Mute failed: "+muted);}
     sent++;Thread.sleep(32);}
    if(!status().contains("probe diagnostics"))throw new AssertionError("Diagnostics not received");
    System.out.println("WIRE_PROBE round="+round+" sent="+sent);
   }Thread.sleep(250);
  }
  System.out.println("WIRE_PROBE wrong_key_rejected=true reconnect=true");
 }
}
