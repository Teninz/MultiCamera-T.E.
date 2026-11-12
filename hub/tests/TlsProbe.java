import ru.codex.obslancamera.WssSocket;
import java.net.Socket;
public class TlsProbe {
 public static void main(String[] args)throws Exception {
  boolean connected=false;
  try(Socket socket=WssSocket.open(args[0])){connected=true;}
  catch(Exception expected){System.out.println("TLS_REJECTED "+expected.getClass().getSimpleName());}
  if(connected)throw new AssertionError("Untrusted certificate or wrong hostname accepted");
 }
}
