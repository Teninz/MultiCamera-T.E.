import ru.codex.obslancamera.Wire;
import java.io.*;
public class WireProbe {
 public static void main(String[] args)throws Exception {
  boolean rejected=false;
  try(Wire invalid=new Wire("127.0.0.1",34879,"11111111111111111111111111111111")){}catch(Exception e){rejected=true;}
  if(!rejected)throw new AssertionError("Wrong key accepted");Thread.sleep(150);
  for(int round=0;round<2;round++){
   try(Wire wire=new Wire("127.0.0.1",34879,"0123456789abcdef0123456789abcdef");DataInputStream file=new DataInputStream(new FileInputStream(args[0]))){
    byte[] sps=java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(args[0]+".sps"));byte[] pps=java.nio.file.Files.readAllBytes(java.nio.file.Paths.get(args[0]+".pps"));wire.send(1,90,0,sps);wire.send(1,90,0,pps);
    int sent=0;while(file.available()>0){int len=file.readInt();byte[] packet=new byte[len];file.readFully(packet);wire.send(2,90,sent*33333L,packet);sent++;Thread.sleep(32);}
    System.out.println("WIRE_PROBE round="+round+" sent="+sent);
   }Thread.sleep(250);
  }
  System.out.println("WIRE_PROBE wrong_key_rejected=true reconnect=true");
 }
}
