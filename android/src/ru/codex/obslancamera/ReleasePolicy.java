package ru.codex.obslancamera;
/** Shared, platform-independent validation of public release metadata. */
final class ReleasePolicy {
    static final String REPO="https://github.com/Teninz/obs-lan-camera";
    static final String APK="OBS-LAN-Camera-Android.apk";
    static final long MAX_APK=50L*1024*1024;
    static long[] version(String text){if(text==null)return null;if(text.startsWith("v"))text=text.substring(1);if(!text.matches("[0-9]{1,6}\\.[0-9]{1,6}\\.[0-9]{1,6}"))return null;String[] bits=text.split("\\.");return new long[]{Long.parseLong(bits[0]),Long.parseLong(bits[1]),Long.parseLong(bits[2])};}
    static boolean newer(String candidate,String current){long[] a=version(candidate),b=version(current);if(a==null||b==null)return false;for(int i=0;i<3;i++)if(a[i]!=b[i])return a[i]>b[i];return false;}
    static boolean asset(String tag,String url,String digest,long size){return version(tag)!=null&&(REPO+"/releases/download/"+tag+"/"+APK).equals(url)&&digest!=null&&digest.matches("sha256:[0-9a-fA-F]{64}")&&size>0&&size<=MAX_APK;}
}
