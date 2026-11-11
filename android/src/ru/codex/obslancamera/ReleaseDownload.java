package ru.codex.obslancamera;
import java.io.*;
import java.net.*;
import java.security.*;
import java.util.*;
import java.util.function.BooleanSupplier;
/** Public HTTPS only. No credential, pairing key, camera or microphone data is sent. */
final class ReleaseDownload {
    static HttpURLConnection open(String address)throws Exception {
        for(int redirects=0;redirects<5;redirects++){
            URL url=new URL(address);String host=url.getHost();if(!"https".equals(url.getProtocol())||url.getUserInfo()!=null||(url.getPort()!=-1&&url.getPort()!=443)||!(host.equals("api.github.com")||host.equals("github.com")||host.equals("release-assets.githubusercontent.com")||host.equals("objects.githubusercontent.com")))throw new IOException("Недопустимый адрес обновления");
            HttpURLConnection c=(HttpURLConnection)url.openConnection();c.setInstanceFollowRedirects(false);c.setConnectTimeout(10000);c.setReadTimeout(15000);c.setRequestProperty("User-Agent","OBS-LAN-Camera-Android");c.setRequestProperty("Accept","application/vnd.github+json");int status=c.getResponseCode();
            if(status==301||status==302||status==303||status==307||status==308){String next=c.getHeaderField("Location");c.disconnect();if(next==null)throw new IOException("Пустой адрес перенаправления");address=new URL(url,next).toString();continue;}
            if(status!=200){c.disconnect();throw new IOException("GitHub: HTTP "+status);}return c;
        }throw new IOException("Слишком много перенаправлений");
    }
    static byte[] metadata(String address)throws Exception{HttpURLConnection c=open(address);try(InputStream input=c.getInputStream();ByteArrayOutputStream output=new ByteArrayOutputStream()){byte[] b=new byte[8192];int n;while((n=input.read(b))!=-1){if(output.size()+n>1024*1024)throw new IOException("Слишком большое описание релиза");output.write(b,0,n);}return output.toByteArray();}finally{c.disconnect();}}
    static void apk(String tag,String address,String digest,long size,File target,BooleanSupplier cancelled)throws Exception{
        if(!ReleasePolicy.asset(tag,address,digest,size))throw new IOException("Неверные данные APK");File parent=target.getParentFile();if(!parent.isDirectory()&&!parent.mkdirs())throw new IOException("Не удалось создать папку обновления");File part=new File(parent,"update.part");HttpURLConnection c=null;
        try{c=open(address);MessageDigest hash=MessageDigest.getInstance("SHA-256");long total=0;
            try(InputStream input=c.getInputStream();OutputStream output=new FileOutputStream(part)){byte[] b=new byte[16384];int n;while((n=input.read(b))!=-1){if(cancelled.getAsBoolean())throw new IOException("Скачивание отменено");total+=n;if(total>size||total>ReleasePolicy.MAX_APK)throw new IOException("Неверный размер APK");hash.update(b,0,n);output.write(b,0,n);}}
            StringBuilder hex=new StringBuilder();for(byte b:hash.digest())hex.append(String.format(Locale.ROOT,"%02x",b&255));if(total!=size||!hex.toString().equalsIgnoreCase(digest.substring(7)))throw new IOException("Проверка SHA-256 APK не пройдена");
            if(cancelled.getAsBoolean())throw new IOException("Скачивание отменено");if(target.exists()&&!target.delete())throw new IOException("Не удалось заменить файл обновления");if(!part.renameTo(target))throw new IOException("Не удалось сохранить APK");
        }finally{if(c!=null)c.disconnect();if(part.exists())part.delete();}
    }
}
