"""Build MultiCamera T.E. with Zig and the Windows system multimedia APIs."""
from pathlib import Path
import argparse,json,os,subprocess,hashlib,shutil
ROOT=Path(__file__).resolve().parent
def main():
 p=argparse.ArgumentParser();p.add_argument('--zig',type=Path,required=True);p.add_argument('--work',type=Path,required=True);p.add_argument('--installer',action='store_true');p.add_argument('--out-dir',type=Path);p.add_argument('--webview2',type=Path,required=True,help='Extracted Microsoft.Web.WebView2 1.0.4314.35 NuGet');a=p.parse_args();work=a.work.resolve();work.mkdir(parents=True,exist_ok=True);out=(a.out_dir or ROOT/'bin').resolve();out.mkdir(parents=True,exist_ok=True)
 locale=json.loads((ROOT/'translations.json').read_text(encoding='utf-8'));languages=locale['languages'];keys=list(locale['strings'])
 for key,values in locale['strings'].items():
  if len(values)!=5 or any(not value.strip() for value in values):raise ValueError('Missing translation: '+key)
 header=['#pragma once','enum mc_text_key {'+','.join('MC_TEXT_'+key.upper() for key in keys)+'};','extern unsigned mc_language;','static const wchar_t *mc_messages[][5]={']
 for key in keys:header.append('{'+','.join('L'+json.dumps(value,ensure_ascii=False) for value in locale['strings'][key])+'},')
 header+=['};','static const wchar_t *mc_text(enum mc_text_key key){return mc_messages[key][mc_language<5?mc_language:0];}'];(work/'i18n.h').write_text('\n'.join(header),encoding='utf-8')
 env=os.environ.copy();env['ZIG_GLOBAL_CACHE_DIR']=str(work/'zig-global');env['ZIG_LOCAL_CACHE_DIR']=str(work/'zig-local')
 def run(args):subprocess.run([str(v) for v in args],env=env,check=True)
 common=[a.zig,'cc','-target','x86_64-windows-gnu','-std=gnu11','-O2','-g','-Wall','-Wextra','-Wno-unused-parameter','-I'+str(work)]
 run([*common,'-shared',ROOT/'src/virtual-source.c',ROOT/'src/virtual-source.def','-lmfplat','-lmfuuid','-lole32','-luuid','-ladvapi32','-lksuser','-o',out/'MultiCameraTE.Source.dll'])
 run([*common,'-shared',*[ROOT/'src'/name for name in ['core-api.c','config.c','receiver.c','mfdecode.c','ipc.c','virtual-manager.c','relay.c','usb.c']],ROOT.parent/'plugin/src/qrcodegen.c','-lmfplat','-lmfuuid','-lwmcodecdspuuid','-lole32','-luuid','-ladvapi32','-lcrypt32','-lbcrypt','-lws2_32','-liphlpapi','-lshell32','-luser32','-lwinhttp','-o',out/'MultiCameraTE.Core.dll'])
 sdk=a.webview2.resolve();managed=sdk/'lib/net462';native=sdk/'build/native/x64'
 for filename in ['Microsoft.Web.WebView2.Core.dll','Microsoft.Web.WebView2.WinForms.dll']:shutil.copyfile(managed/filename,out/filename)
 shutil.copyfile(native/'WebView2Loader.dll',out/'WebView2Loader.dll');shutil.copyfile(sdk/'LICENSE.txt',out/'WebView2-LICENSE.txt')
 csc=Path(os.environ.get('WINDIR','C:/Windows'))/'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
 run([csc,'/nologo','/target:winexe','/platform:x64','/optimize+','/win32manifest:'+str(ROOT/'src/setup.manifest'),'/win32icon:'+str(ROOT/'assets/multicamera.ico'),'/reference:System.Windows.Forms.dll','/reference:System.Drawing.dll','/reference:System.Web.Extensions.dll','/reference:'+str(out/'Microsoft.Web.WebView2.Core.dll'),'/reference:'+str(out/'Microsoft.Web.WebView2.WinForms.dll'),'/out:'+str(out/'MultiCameraTE.exe'),ROOT/'src/Desktop.cs'])
 ui=out/'ui';ui.mkdir(exist_ok=True);(ui/'assets').mkdir(exist_ok=True)
 strings=dict(locale['strings']);strings.update(json.loads((ROOT/'ui/strings.json').read_text(encoding='utf-8')))
 for key,values in strings.items():
  if len(values)!=5 or any(not value.strip() for value in values):raise ValueError('Missing UI translation: '+key)
 (ui/'translations.js').write_text('window.MC_TEXT='+json.dumps(strings,ensure_ascii=False)+';\n',encoding='utf-8')
 ui_names=['index.html','style.css','app.js']
 asset_names=['background-dark.png','background-light.png','brand-emblem.png','connect-phone.png','preview-lens.png']
 for name in ui_names:shutil.copyfile(ROOT/'ui'/name,ui/name)
 for name in asset_names:shutil.copyfile(ROOT/'ui/assets'/name,ui/'assets'/name)
 print(out/'MultiCameraTE.exe')
 if a.installer:
  files=[(0,out/name,name) for name in ['MultiCameraTE.exe','MultiCameraTE.Source.dll','MultiCameraTE.Core.dll','Microsoft.Web.WebView2.Core.dll','Microsoft.Web.WebView2.WinForms.dll','WebView2Loader.dll','WebView2-LICENSE.txt']]+[(0,ROOT.parent/'COPYING','COPYING')]
  files.extend((0,ui/name,'ui/'+name) for name in ui_names+['translations.js'])
  files.extend((0,ui/'assets'/name,'ui/assets/'+name) for name in asset_names)
  if (ROOT.parent/'OBS-LAN-Camera-Android.apk').exists():files.append((0,ROOT.parent/'OBS-LAN-Camera-Android.apk','MultiCameraTE-Android.apk'))
  plugin=ROOT.parent/'plugin';files.append((1,plugin/'bin/64bit/obs-lan-camera.dll','bin/64bit/obs-lan-camera.dll'))
  files.extend((1,f,f.relative_to(plugin).as_posix()) for f in sorted((plugin/'data').rglob('*')) if f.is_file())
  payload=['#include <stdint.h>\nstruct suite_file { int group; const wchar_t *name; const uint8_t *bytes; size_t size; uint8_t hash[32]; };'];inventory=[]
  for i,(group,path,name) in enumerate(files):
   data=path.read_bytes();payload.append('static const uint8_t file_'+str(i)+'[]={'+','.join(str(b) for b in data)+'};');inventory.append({'group':group,'name':name,'size':len(data),'sha256':hashlib.sha256(data).hexdigest()})
  payload.append('static const struct suite_file suite_files[]={')
  for i,item in enumerate(inventory):payload.append('{'+str(item['group'])+',L'+json.dumps(item['name'].replace('/','\\'))+',file_'+str(i)+',sizeof(file_'+str(i)+'),{'+','.join(str(v) for v in bytes.fromhex(item['sha256']))+'}},')
  payload.append('};\n#define SUITE_COUNT (sizeof(suite_files)/sizeof(suite_files[0]))');(work/'suite-payload.h').write_text('\n'.join(payload),encoding='utf-8');(work/'suite-inventory.json').write_text(json.dumps(inventory,indent=2))
  manifest=str(ROOT/'src/setup.manifest').replace('\\','/');(work/'suite.rc').write_text('1 24 "'+manifest+'"\n1 ICON '+json.dumps(str(ROOT/'assets/multicamera.ico').replace('\\','/'))+'\n'+''.join(str(i)+' RCDATA '+json.dumps(str(path).replace('\\','/'))+'\n' for i,path in [(201,ROOT/'ui/assets/background-light.png'),(202,ROOT/'assets/setup-app.png'),(203,ROOT/'assets/setup-obs.png')]),encoding='utf-8');run([a.zig,'rc','/fo'+str(work/'suite.res'),work/'suite.rc'])
  setup=ROOT.parent/'MultiCameraTE-Setup.exe';run([*common,'-municode','-Wl,--subsystem,windows',ROOT/'src/setup.c',ROOT/'src/virtual-manager.c',ROOT/'src/usb.c',work/'suite.res','-lmfplat','-lmfuuid','-lole32','-loleaut32','-luuid','-ladvapi32','-lbcrypt','-lws2_32','-liphlpapi','-lshell32','-luser32','-lcomctl32','-lgdi32','-lgdiplus','-lshlwapi','-o',setup]);shutil.copyfile(setup,ROOT.parent/'OBS-LAN-Camera-Setup.exe');print(setup)
if __name__=='__main__':main()
