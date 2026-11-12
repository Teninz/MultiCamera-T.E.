import ru.codex.obslancamera.Wire;
import ru.codex.obslancamera.WssSocket;
import java.io.*;
import java.nio.file.*;
import java.util.concurrent.*;
public class MultiWireProbe {
 static Wire connect(String[] args,int id)throws Exception{if(args.length>2&&args[1].equals("--local-profile")){java.util.List<String> values=Files.readAllLines(Paths.get(args[2]));String[] address=values.get(0).split(":");return new Wire(address[0],Integer.parseInt(address[1]),values.get(id+1));}return args.length>1?new Wire(WssSocket.open(args[1]+"/v1/phone/11111111111111111111111111111111/"+(id+1)),String.format("%032x",id+1)):new Wire("127.0.0.1",34880,String.format("%032x",id+1));}

 public static void main(String[] args)throws Exception{
  final CyclicBarrier ready=new CyclicBarrier(5);ExecutorService pool=Executors.newFixedThreadPool(5);java.util.List<Future<?>> jobs=new java.util.ArrayList<>();
  for(int slot=0;slot<5;slot++){final int id=slot;jobs.add(pool.submit(()->{try(Wire wire=connect(args,id);DataInputStream input=new DataInputStream(new FileInputStream(args[0]))){ready.await(5,TimeUnit.SECONDS);wire.send(1,0,0,Files.readAllBytes(Paths.get(args[0]+".sps")));wire.send(1,0,0,Files.readAllBytes(Paths.get(args[0]+".pps")));
    if(id==0){boolean rejected=false;try(Wire duplicate=connect(args,0)){}catch(Exception expected){rejected=true;}if(!rejected)throw new AssertionError("Duplicate slot accepted");}
    int frame=0;while(input.available()>0){int length=input.readInt();byte[] packet=new byte[length];input.readFully(packet);wire.send(2,(id%4)*90,id*10000000L+frame*33333L,packet);if(frame%3==0){byte[] pcm=new byte[5+960*2];pcm[0]=0;pcm[1]=0;pcm[2]=(byte)0xbb;pcm[3]=(byte)0x80;pcm[4]=1;for(int j=5;j<pcm.length;j+=2){int sample=(id+1)*4000;pcm[j]=(byte)sample;pcm[j+1]=(byte)(sample>>>8);}wire.send(4,0,id*10000000L+frame*33333L,pcm);}frame++;Thread.sleep(32);}if(args.length>3&&args[1].equals("--local-profile")){int repetitions=Integer.parseInt(args[3]);for(int round=1;round<repetitions;round++){try(DataInputStream repeat=new DataInputStream(new FileInputStream(args[0]))){while(repeat.available()>0){int length=repeat.readInt();byte[] packet=new byte[length];repeat.readFully(packet);wire.send(2,(id%4)*90,id*10000000L+frame*33333L,packet);frame++;Thread.sleep(32);}}}}Thread.sleep(2000);System.out.println("MULTI_WIRE slot="+(id+1)+" sent="+frame);
   }catch(Exception e){throw new RuntimeException(e);}}));}for(Future<?> job:jobs)job.get(args.length>3?90:15,TimeUnit.SECONDS);pool.shutdown();System.out.println("FIVE_CLIENTS_FINISHED");
 }
}
