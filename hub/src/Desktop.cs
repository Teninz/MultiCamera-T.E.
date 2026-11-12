using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

internal static class Native {
 const string Dll = "MultiCameraTE.Core.dll";
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl)] internal static extern int mc_open();
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl)] internal static extern void mc_close();
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl)] internal static extern int mc_value(int field,int slot);
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] internal static extern void mc_text_value(int field,int index,StringBuilder output,int capacity);
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl)] internal static extern int mc_refresh_usb();
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl)] internal static extern int mc_preferences(int language,int animation);
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Unicode)] internal static extern int mc_settings(string server,string key,int enabled);
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Ansi)] internal static extern int mc_pair_uri(int slot,int mode,int adapter,StringBuilder output,int capacity);
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl,CharSet=CharSet.Ansi)] internal static extern int mc_qr(string value,byte[] modules,int capacity);
 [DllImport(Dll, CallingConvention=CallingConvention.Cdecl)] internal static extern int mc_thumbnail(int slot,ref ulong sequence,byte[] output,int capacity);
 [DllImport("user32.dll")] internal static extern bool SetProcessDpiAwarenessContext(IntPtr context);
 [DllImport("dwmapi.dll")] internal static extern int DwmSetWindowAttribute(IntPtr window,int attribute,ref int value,int length);
}

internal sealed class Desktop : Form {
 const string Origin="https://multicamera.internal";
 const string Page=Origin+"/index.html";
 const string Release="https://github.com/Teninz/obs-lan-camera/releases/download/v0.4.2/OBS-LAN-Camera-Android.apk";
 readonly JavaScriptSerializer json=new JavaScriptSerializer();
 readonly WebView2 web=new WebView2();
 readonly System.Windows.Forms.Timer timer=new System.Windows.Forms.Timer();
 readonly ulong[] sequences=new ulong[5];
 readonly byte[] pixels=new byte[320*180*4];
 bool ready,started,closing;int ticks;
 string lastState="";
 internal Desktop(){
  Text="MultiCamera T.E.";ClientSize=new Size(1280,840);MinimumSize=new Size(860,640);StartPosition=FormStartPosition.CenterScreen;
  BackColor=Color.FromArgb(16,18,23);Font=new Font("Segoe UI",10);web.Dock=DockStyle.Fill;web.DefaultBackgroundColor=BackColor;Controls.Add(web);
  timer.Interval=100;timer.Tick+=Tick;Shown+=Initialize;FormClosing+=Shutdown;
 }
 string TextValue(int field,int slot){var text=new StringBuilder(2048);Native.mc_text_value(field,slot,text,text.Capacity);return text.ToString();}
 void Send(object value){if(ready&&!closing)web.CoreWebView2.PostWebMessageAsJson(json.Serialize(value));}
 void Theme(bool light){int dark=light?0:1;Native.DwmSetWindowAttribute(Handle,20,ref dark,4);BackColor=light?Color.FromArgb(237,240,240):Color.FromArgb(16,18,23);web.DefaultBackgroundColor=BackColor;}
 async void Initialize(object sender,EventArgs args){
  try {
   int result=Native.mc_open();if(result!=0)throw new InvalidOperationException(result==3?"Cannot read protected settings. / Не удалось прочитать защищённые настройки.":"Cannot start receiver. Close another MultiCamera T.E. instance. / Не удалось запустить приёмник. Закройте другую копию приложения.");started=true;
   var folder=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"MultiCameraTE","WebView2");
   var environment=await CoreWebView2Environment.CreateAsync(null,folder,null);await web.EnsureCoreWebView2Async(environment);
   var core=web.CoreWebView2;core.Settings.AreDevToolsEnabled=false;core.Settings.AreDefaultContextMenusEnabled=false;core.Settings.AreBrowserAcceleratorKeysEnabled=false;core.Settings.IsStatusBarEnabled=false;core.Settings.IsZoomControlEnabled=false;core.Settings.IsPasswordAutosaveEnabled=false;core.Settings.IsGeneralAutofillEnabled=false;
   core.SetVirtualHostNameToFolderMapping("multicamera.internal",Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"ui"),CoreWebView2HostResourceAccessKind.DenyCors);
   core.NavigationStarting+=(s,e)=>{if(e.Uri!=Page&&!e.Uri.StartsWith(Page+"#",StringComparison.Ordinal))e.Cancel=true;};
   core.NewWindowRequested+=(s,e)=>{e.Handled=true;};core.DownloadStarting+=(s,e)=>{e.Cancel=true;};core.PermissionRequested+=(s,e)=>{e.State=CoreWebView2PermissionState.Deny;};
   core.WebMessageReceived+=Message;core.ProcessFailed+=(s,e)=>{timer.Stop();MessageBox.Show(this,"The interface stopped. Restart MultiCamera T.E. / Перезапустите MultiCamera T.E.",Text,MessageBoxButtons.OK,MessageBoxIcon.Warning);};
   core.Navigate(Page);Theme(false);timer.Start();
  }catch(Exception error){MessageBox.Show(this,error.Message+"\n\nMicrosoft Edge WebView2 Runtime is required.",Text,MessageBoxButtons.OK,MessageBoxIcon.Error);Close();}
 }
 void Message(object sender,CoreWebView2WebMessageReceivedEventArgs args){
  if(args.Source!=Page&&!args.Source.StartsWith(Page+"#",StringComparison.Ordinal))return;
  try {
   var data=json.Deserialize<Dictionary<string,object>>(args.WebMessageAsJson);string action=Convert.ToString(data["action"]);
   switch(action){
    case "ready":ready=true;Send(new{type="init",language=Native.mc_value(2,0),animation=Native.mc_value(3,0)!=0,version="0.4.2"});State();break;
    case "theme":Theme(Convert.ToString(data["value"])=="light");break;
    case "preferences":if(Native.mc_preferences(Convert.ToInt32(data["language"]),Convert.ToBoolean(data["animation"])?1:0)==0)Send(new{type="error",key="save_error"});break;
    case "settings":Send(new{type="settings",server=TextValue(1,0),key=TextValue(2,0),enabled=Native.mc_value(4,0)!=0,animation=Native.mc_value(3,0)!=0});break;
    case "save":{
     string server=Convert.ToString(data["server"]).Trim(),key=Convert.ToString(data["key"]);Uri uri;
     bool enabled=Convert.ToBoolean(data["enabled"]);if((server.Length>0&&(!Uri.TryCreate(server,UriKind.Absolute,out uri)||uri.Scheme!="wss"||!String.IsNullOrEmpty(uri.UserInfo)||!String.IsNullOrEmpty(uri.Query)||!String.IsNullOrEmpty(uri.Fragment)))||(enabled&&server.Length==0)||Native.mc_settings(server,key,enabled?1:0)==0){Send(new{type="error",key="server_invalid"});break;}
     Native.mc_preferences(Native.mc_value(2,0),Convert.ToBoolean(data["animation"])?1:0);Send(new{type="saved"});State();break;
    }
    case "pair":Pair(Convert.ToInt32(data["slot"]),Convert.ToInt32(data["mode"]),Convert.ToInt32(data["adapter"]));break;
    case "copy":{
     var uri=PairUri(Convert.ToInt32(data["slot"]),Convert.ToInt32(data["mode"]),Convert.ToInt32(data["adapter"]));if(uri.Length>0){Clipboard.SetText(uri);Send(new{type="copied"});}break;
    }
    case "usb":{
     int adapter=Convert.ToInt32(data["adapter"]);if(adapter<0||adapter>=Native.mc_value(5,0))break;string argument=TextValue(4,adapter);if(argument.Length==0)break;LaunchSetup("--usb-firewall "+argument+" "+Native.mc_value(2,0));break;
    }
    case "install":LaunchSetup(null);break;
    case "release":Process.Start(new ProcessStartInfo("https://github.com/Teninz/obs-lan-camera/releases/latest"){UseShellExecute=true});break;
   }
  }catch(Exception error){if(!(error is ArgumentException||error is InvalidCastException||error is KeyNotFoundException||error is FormatException))Send(new{type="error",key="action_error"});}
 }
 void LaunchSetup(string arguments){
  string path=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"MultiCameraTE-Setup.exe");
  if(!File.Exists(path)){var directory=new StringBuilder(2048); // Development builds use the already installed helper.
   using(var registry=Microsoft.Win32.Registry.LocalMachine.OpenSubKey("SOFTWARE\\MultiCameraTE")){if(registry!=null)path=Path.Combine(Convert.ToString(registry.GetValue("InstallDir","")),"MultiCameraTE-Setup.exe");}
  }
  if(File.Exists(path))Process.Start(new ProcessStartInfo(path,arguments??""){UseShellExecute=true});else Send(new{type="error",key="camera_error"});
 }
 string PairUri(int slot,int mode,int adapter){var uri=new StringBuilder(8192);return Native.mc_pair_uri(slot,mode,adapter,uri,uri.Capacity)!=0?uri.ToString():"";}
 object QR(string value){if(value.Length==0)return null;var modules=new byte[177*177];int n=Native.mc_qr(value,modules,modules.Length);if(n==0)return null;var bits=new StringBuilder(n*n);for(int i=0;i<n*n;i++)bits.Append(modules[i]==0?'0':'1');return new{size=n,bits=bits.ToString()};}
 void Pair(int slot,int mode,int adapter){
  if(slot<0||slot>4||mode<0||mode>2)return;
  int count=Native.mc_refresh_usb();if(adapter<0||adapter>=count)adapter=0;
  var adapters=new List<object>();for(int i=0;i<count;i++)adapters.Add(new{index=i,label=TextValue(3,i)});
  string uri=PairUri(slot,mode,adapter);
  Send(new{type="pair",slot=slot,mode=mode,adapter=adapter,adapters=adapters,apk=QR(Release),connection=QR(uri),internet=Native.mc_value(4,0)!=0,server=TextValue(1,0).Length>0,address=mode==2&&count>0?TextValue(3,adapter):TextValue(0,0)+":34872"});
 }
 void State(){
  var slots=new List<object>();for(int i=0;i<5;i++)slots.Add(new{connected=Native.mc_value(10,i)!=0,width=Native.mc_value(11,i),height=Native.mc_value(12,i),audio=Native.mc_value(13,i)!=0});
  string value=json.Serialize(new{type="state",camera=Native.mc_value(0,0),relay=Native.mc_value(1,0),address=TextValue(0,0),slots=slots});if(value!=lastState){web.CoreWebView2.PostWebMessageAsJson(value);lastState=value;}
 }
 void Tick(object sender,EventArgs args){
  if(!ready||closing)return;if(WindowState==FormWindowState.Minimized){if(ticks++%20==0)Send(new{type="visibility",active=false});return;}
  if(ticks++%5==0){State();Send(new{type="visibility",active=true});}
  for(int i=0;i<5;i++)if(Native.mc_thumbnail(i,ref sequences[i],pixels,pixels.Length)!=0){
   using(var bitmap=new Bitmap(320,180,PixelFormat.Format32bppArgb)){
    var bits=bitmap.LockBits(new Rectangle(0,0,320,180),ImageLockMode.WriteOnly,PixelFormat.Format32bppArgb);Marshal.Copy(pixels,0,bits.Scan0,pixels.Length);bitmap.UnlockBits(bits);
    using(var memory=new MemoryStream()){bitmap.Save(memory,ImageFormat.Jpeg);Send(new{type="preview",slot=i,image="data:image/jpeg;base64,"+Convert.ToBase64String(memory.ToArray())});}
   }
  }
 }
 void Shutdown(object sender,FormClosingEventArgs args){closing=true;timer.Stop();web.Dispose();if(started){Native.mc_close();started=false;}}
 [STAThread] internal static void Main(){
  Native.SetProcessDpiAwarenessContext(new IntPtr(-4));bool created;
  using(var mutex=new Mutex(true,"Local\\MultiCameraTE."+WindowsIdentity.GetCurrent().User.Value,out created)){
   if(!created){MessageBox.Show("MultiCamera T.E. is already running. / Приложение уже запущено.","MultiCamera T.E.");return;}
   Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);Application.Run(new Desktop());
  }
 }
}
