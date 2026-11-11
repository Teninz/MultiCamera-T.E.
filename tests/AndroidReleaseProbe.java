package ru.codex.obslancamera;
import java.io.*;
import java.nio.file.*;
public class AndroidReleaseProbe {
 static void check(boolean value){if(!value)throw new AssertionError();}
 public static void main(String[] args)throws Exception{
  check(ReleasePolicy.newer("v0.3.0","0.2.0"));check(ReleasePolicy.newer("v0.10.0","0.3.99"));check(!ReleasePolicy.newer("v0.3.0","0.3.0"));check(!ReleasePolicy.newer("v0.3.0-beta","0.2.0"));check(!ReleasePolicy.newer("v99999999.0.0","0.2.0"));
  String hash="sha256:"+new String(new char[64]).replace('\0','a'),url=ReleasePolicy.REPO+"/releases/download/v0.3.0/"+ReleasePolicy.APK;
  check(ReleasePolicy.asset("v0.3.0",url,hash,123));check(!ReleasePolicy.asset("v0.3.0",url.replace("https:","http:"),hash,123));check(!ReleasePolicy.asset("v0.3.0",url.replace("Teninz","Other"),hash,123));check(!ReleasePolicy.asset("v0.3.0",url,"sha256:abc",123));check(!ReleasePolicy.asset("v0.3.0",url,hash,ReleasePolicy.MAX_APK+1));
  try{ReleaseDownload.open("https://evil.example/update.apk");throw new AssertionError("Untrusted redirect accepted");}catch(IOException expected){}
  System.out.println("ANDROID_UPDATE_POLICY_PASS versions, exact repository/asset URL, HTTPS/CDN allowlist, SHA256 format, size cap");
  if(args.length==5){File target=new File(args[4]);ReleaseDownload.apk(args[0],args[1],args[2],Long.parseLong(args[3]),target,()->false);System.out.println("ANDROID_HTTPS_DOWNLOAD_PASS "+target.length());
   File bad=new File(target.getParentFile(),"must-not-install.apk");try{ReleaseDownload.apk(args[0],args[1],"sha256:"+new String(new char[64]).replace('\0','0'),Long.parseLong(args[3]),bad,()->false);throw new AssertionError("Bad hash accepted");}catch(IOException expected){check(!bad.exists());}System.out.println("ANDROID_BAD_HASH_REJECTED");}
 }
}
