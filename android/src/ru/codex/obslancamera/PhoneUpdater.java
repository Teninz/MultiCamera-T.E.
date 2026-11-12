package ru.codex.obslancamera;
import android.app.*;
import android.content.*;
import android.content.pm.*;
import android.net.Uri;
import android.os.*;
import android.provider.Settings;
import android.widget.*;
import org.json.*;
import java.io.*;
import java.util.*;
import java.util.function.Consumer;
/** A startup check, explicit download consent, then Android's own installation confirmation. */
final class PhoneUpdater {
    private final Activity activity;private final Consumer<String> log;private final Handler ui=new Handler(Looper.getMainLooper());
    private volatile boolean busy,closed,cancelled;private boolean resumed,awaitingInstallPermission,installReady,pendingInstall;private Release pending;private String pendingMessage;
    private static final class Release {String tag,body,url,digest;long size;}
    PhoneUpdater(Activity activity,Consumer<String> log){this.activity=activity;this.log=log;}
    String current(){try{return activity.getPackageManager().getPackageInfo(activity.getPackageName(),0).versionName;}catch(Exception ignored){return "0.0.0";}}
    void resume(){resumed=true;if(awaitingInstallPermission){awaitingInstallPermission=false;if(activity.getPackageManager().canRequestPackageInstalls())install();else message(Language.text("Установка не разрешена. APK не установлен."));}else if(pendingInstall){pendingInstall=false;requestInstall();}showPending();}
    void pause(){resumed=false;}
    void close(){closed=true;cancelled=true;}
    void check(boolean manual){if(busy||closed)return;if(manual&&installReady){requestInstall();return;}busy=true;new Thread(()->{try{
        JSONObject json=new JSONObject(new String(ReleaseDownload.metadata("https://api.github.com/repos/Teninz/obs-lan-camera/releases/latest"),"UTF-8"));if(json.optBoolean("draft")||json.optBoolean("prerelease"))throw new IOException(Language.text("Нет обычного релиза"));String tag=json.getString("tag_name");
        if(!ReleasePolicy.newer(tag,current())){if(manual)message(Language.text("Установлена актуальная версия ")+current());return;}
        JSONArray assets=json.getJSONArray("assets");Release release=null;for(int i=0;i<assets.length();i++){JSONObject asset=assets.getJSONObject(i);if(!ReleasePolicy.APK.equals(asset.optString("name")))continue;String url=asset.getString("browser_download_url"),digest=asset.optString("digest");long size=asset.getLong("size");if(!ReleasePolicy.asset(tag,url,digest,size))continue;release=new Release();release.tag=tag;release.url=url;release.digest=digest;release.size=size;release.body=json.optString("body","");break;}
        if(release==null)throw new IOException(Language.text("В релизе нет проверяемого APK"));final Release found=release;ui.post(()->{if(!closed){pending=found;showPending();}});
    }catch(Exception e){log.accept(Language.text("Обновления: ")+e.getMessage());if(manual)message(Language.text("Не удалось проверить обновления: ")+e.getMessage());}finally{busy=false;}},"obs-update-check").start();}
    private void showPending(){if(closed||!resumed||activity.isFinishing())return;if(pendingMessage!=null){String text=pendingMessage;pendingMessage=null;message(text);}Release release=pending;if(release==null)return;pending=null;
        String body=release.body.length()>10000?release.body.substring(0,10000)+"…":release.body;TextView text=new TextView(activity);text.setPadding(28,18,28,18);text.setText(String.format(Locale.ROOT,Language.text("Установлено: %s\nДоступно: %s · %.1f МБ\n\n%s\n\nСкачать обновление с GitHub? После проверки файла Android отдельно запросит установку."),current(),release.tag,release.size/1048576.0,body));ScrollView scroll=new ScrollView(activity);scroll.addView(text);
        new AlertDialog.Builder(activity,AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle(Language.text("Обновление OBS LAN Camera")).setView(scroll).setNegativeButton(Language.text("Позже"),null).setPositiveButton(Language.text("Скачать"),(d,w)->download(release)).show();
    }
    private void message(String text){ui.post(()->{if(closed)return;if(!resumed){pendingMessage=text;return;}new AlertDialog.Builder(activity,AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle("MultiCamera T.E.").setMessage(text).setPositiveButton("OK",null).show();});}
    private void download(Release release){if(busy||closed)return;busy=true;cancelled=false;ProgressDialog progress=new ProgressDialog(activity,AlertDialog.THEME_DEVICE_DEFAULT_DARK);progress.setMessage(Language.text("Скачиваем ")+release.tag+Language.text(" и проверяем APK…"));progress.setIndeterminate(true);progress.setCancelable(false);progress.setButton(DialogInterface.BUTTON_NEGATIVE,Language.text("Отмена"),(d,w)->cancelled=true);progress.show();
        new Thread(()->{try{File file=UpdateProvider.file(activity);ReleaseDownload.apk(release.tag,release.url,release.digest,release.size,file,()->cancelled||closed);verify(file,release.tag);ui.post(()->{if(!closed){progress.dismiss();installReady=true;requestInstall();}});}catch(Exception e){log.accept(Language.text("Обновление APK: ")+e.getMessage());ui.post(()->{if(!closed)progress.dismiss();});if(!cancelled)message(Language.text("Не удалось обновить приложение: ")+e.getMessage());}finally{busy=false;}},"obs-update-download").start();
    }
    private void verify(File file,String tag)throws Exception{
        PackageManager pm=activity.getPackageManager();int flags=Build.VERSION.SDK_INT>=28?PackageManager.GET_SIGNING_CERTIFICATES:PackageManager.GET_SIGNATURES;PackageInfo installed=pm.getPackageInfo(activity.getPackageName(),flags),downloaded=pm.getPackageArchiveInfo(file.getAbsolutePath(),flags);
        if(downloaded==null||!activity.getPackageName().equals(downloaded.packageName)||!tag.replaceFirst("^v","").equals(downloaded.versionName))throw new IOException(Language.text("APK не соответствует приложению/версии"));long oldCode=Build.VERSION.SDK_INT>=28?installed.getLongVersionCode():installed.versionCode,newCode=Build.VERSION.SDK_INT>=28?downloaded.getLongVersionCode():downloaded.versionCode;if(newCode<=oldCode)throw new IOException(Language.text("APK не новее установленного"));
        Signature[] oldSign=Build.VERSION.SDK_INT>=28?installed.signingInfo.getApkContentsSigners():installed.signatures,newSign=Build.VERSION.SDK_INT>=28?downloaded.signingInfo.getApkContentsSigners():downloaded.signatures;if(oldSign==null||newSign==null||oldSign.length==0||!new HashSet<>(Arrays.asList(oldSign)).equals(new HashSet<>(Arrays.asList(newSign))))throw new IOException(Language.text("Подпись APK отличается от установленного приложения"));
    }
    private void requestInstall(){if(closed)return;if(!resumed){pendingInstall=true;return;}
        if(activity.getPackageManager().canRequestPackageInstalls()){install();return;}
        new AlertDialog.Builder(activity,AlertDialog.THEME_DEVICE_DEFAULT_DARK).setTitle(Language.text("Разрешение Android")).setMessage(Language.text("APK скачан и проверен. Android требует один раз разрешить этому приложению предлагать установку обновлений. Открыть системные настройки? Установка всё равно потребует вашего подтверждения.")).setNegativeButton(Language.text("Позже"),null).setPositiveButton(Language.text("Открыть настройки"),(d,w)->{try{awaitingInstallPermission=true;activity.startActivity(new Intent(Settings.ACTION_MANAGE_UNKNOWN_APP_SOURCES,Uri.parse("package:"+activity.getPackageName())));}catch(Exception e){awaitingInstallPermission=false;message(Language.text("Не удалось открыть настройки установки"));}}).show();
    }
    private void install(){try{Intent intent=new Intent(Intent.ACTION_VIEW).setDataAndType(UpdateProvider.uri(),"application/vnd.android.package-archive").addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);intent.setClipData(ClipData.newRawUri("OBS LAN Camera APK",UpdateProvider.uri()));activity.startActivity(intent);}catch(Exception e){message(Language.text("Не удалось открыть установщик Android: ")+e.getMessage());}}
}
