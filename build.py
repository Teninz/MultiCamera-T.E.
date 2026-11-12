"""Build native OBS receiver and Android sender without Gradle. See README."""
from pathlib import Path
import argparse,os,subprocess,zipfile,secrets,hashlib,json
from pe import exported_names
ROOT=Path(__file__).resolve().parent
def main():
 p=argparse.ArgumentParser();p.add_argument('--zig',type=Path);p.add_argument('--obs-source',type=Path);p.add_argument('--ffmpeg',type=Path);p.add_argument('--sdk',type=Path);p.add_argument('--jdk',type=Path);p.add_argument('--zxing',type=Path);p.add_argument('--installer',action='store_true');p.add_argument('--work',type=Path,required=True);p.add_argument('--obs-bin',type=Path,default=Path(r'C:\Program Files\obs-studio\bin\64bit'));a=p.parse_args()
 work=a.work.resolve();work.mkdir(parents=True,exist_ok=True);env=os.environ.copy();env['ZIG_GLOBAL_CACHE_DIR']=str(work/'zig-global');env['ZIG_LOCAL_CACHE_DIR']=str(work/'zig-local')
 def run(args):subprocess.run([str(x) for x in args],check=True,env=env)
 if a.zig:
  (work/'obsconfig.h').write_text('#pragma once\n#define OBS_RELEASE_CANDIDATE 0\n#define OBS_BETA 0\n#define OBS_DATA_PATH "data"\n#define OBS_INSTALL_PREFIX ""\n')
  libs=[]
  for name in ['obs','avcodec-62','avutil-60']:
   definition=work/(name+'.def');definition.write_text('LIBRARY '+name+'.dll\nEXPORTS\n'+'\n'.join(exported_names(a.obs_bin/(name+'.dll')))+'\n');lib=work/('lib'+name+'.a');run([a.zig,'dlltool','-m','i386:x86-64','-d',definition,'-l',lib]);libs.append(lib)
  definition=work/'module.def';definition.write_text('EXPORTS\nobs_module_ver\nobs_module_set_pointer\nobs_module_load\nobs_module_unload\nobs_module_post_load\nobs_module_set_locale\nobs_module_free_locale\nobs_module_description\nobs_module_get_string\n')
  binary=ROOT/'plugin/bin/64bit/obs-lan-camera.dll';binary.parent.mkdir(parents=True,exist_ok=True)
  run([a.zig,'cc','-target','x86_64-windows-gnu','-std=gnu11','-O2','-g','-shared','-fms-extensions','-D_M_X64','-DWIN32_LEAN_AND_MEAN','-Wall','-Wextra','-Wno-macro-redefined','-I'+str(work),'-I'+str(a.obs_source/'libobs'),'-I'+str(a.obs_source/'deps/w32-pthreads'),'-I'+str(a.ffmpeg),ROOT/'plugin/src/lan-camera.c',ROOT/'plugin/src/updater.c',ROOT/'plugin/src/qrcodegen.c',ROOT/'plugin/src/pairing.c',ROOT/'plugin/src/hub-source.c',definition,*libs,'-lws2_32','-lwinhttp','-lbcrypt','-liphlpapi','-lshell32','-luser32','-lgdi32','-lole32','-ladvapi32','-o',binary]);print(binary)
 if a.sdk:
  run([os.sys.executable,ROOT/'android/generate_language.py'])
  java=a.jdk/'bin/java.exe';javac=a.jdk/'bin/javac.exe';jar=a.jdk/'bin/jar.exe';keytool=a.jdk/'bin/keytool.exe';android=a.sdk/'platforms/android-35/android.jar';bt=a.sdk/'build-tools/35.0.1'
  classes=work/'classes';classes.mkdir(exist_ok=True);dex=work/'dex';dex.mkdir(exist_ok=True)
  run([javac,'-encoding','UTF-8','--release','8','-classpath',str(android)+os.pathsep+str(a.zxing),'-d',classes,*sorted((ROOT/'android/src').rglob('*.java'))])
  run([jar,'cf',work/'classes.jar','-C',classes,'.'])
  run([java,'-cp',bt/'lib/d8.jar','com.android.tools.r8.D8','--release','--lib',android,'--min-api','26','--output',dex,work/'classes.jar',a.zxing])
  run([bt/'aapt2.exe','link','-I',android,'--manifest',ROOT/'android/AndroidManifest.xml','-o',work/'unsigned.apk'])
  with zipfile.ZipFile(work/'unsigned.apk','a',zipfile.ZIP_DEFLATED) as z:
   for f in dex.glob('*.dex'):z.write(f,f.name)
  run([bt/'zipalign.exe','-f','4',work/'unsigned.apk',work/'aligned.apk'])
  key=work/'signing-key.jks';password=work/'signing-password.txt'
  if not key.exists():
   password.write_text(secrets.token_urlsafe(24))
   run([keytool,'-genkeypair','-keystore',key,'-storepass:file',password,'-keypass:file',password,'-alias','obslancamera','-keyalg','RSA','-keysize','3072','-validity','10000','-dname','CN=OBS LAN Camera local build','-storetype','JKS'])
  apk=ROOT/'OBS-LAN-Camera-Android.apk'
  env['OBS_LAN_SIGNING_PASS']=password.read_text().strip()
  run([java,'-jar',bt/'lib/apksigner.jar','sign','--ks',key,'--ks-key-alias','obslancamera','--ks-pass','env:OBS_LAN_SIGNING_PASS','--key-pass','env:OBS_LAN_SIGNING_PASS','--out',apk,work/'aligned.apk'])
  run([java,'-jar',bt/'lib/apksigner.jar','verify','--verbose',apk]);(ROOT/'plugin/data/app.apk').write_bytes(apk.read_bytes());print(apk)
 if a.installer:
  if not a.zig:p.error('--installer needs --zig and native build arguments')
  files=[ROOT/'plugin/bin/64bit/obs-lan-camera.dll',*sorted((ROOT/'plugin/data').rglob('*'))]
  files=[f for f in files if f.is_file()]
  payload=['#include <stdint.h>\n#include <stdbool.h>\nstruct payload_file { const wchar_t *name; const uint8_t *bytes; size_t size; uint8_t sha256[32]; };']
  inventory=[]
  for i,f in enumerate(files):
   data=f.read_bytes();payload.append('static const uint8_t payload_'+str(i)+'[]={'+','.join(str(b) for b in data)+'};')
   inventory.append({'path':f.relative_to(ROOT/'plugin').as_posix(),'sha256':hashlib.sha256(data).hexdigest(),'size':len(data)})
  payload.append('static const struct payload_file payload_files[]={')
  for i,item in enumerate(inventory):
   path=item['path'].replace('/','\\');payload.append('{L'+json.dumps(path)+',payload_'+str(i)+',sizeof(payload_'+str(i)+'),{'+','.join(str(b) for b in bytes.fromhex(item['sha256']))+'}},')
  payload.append('};\n#define PAYLOAD_COUNT (sizeof(payload_files)/sizeof(payload_files[0]))')
  (work/'payload.h').write_text('\n'.join(payload),encoding='utf-8');(work/'payload-inventory.json').write_text(json.dumps(inventory,indent=2))
  manifest=str(ROOT/'installer/setup.manifest').replace('\\','/');(work/'setup.rc').write_text('1 24 "'+manifest+'"\n',encoding='utf-8')
  run([a.zig,'rc','/fo'+str(work/'setup.res'),work/'setup.rc'])
  setup=ROOT/'OBS-LAN-Camera-Setup.exe'
  run([a.zig,'cc','-target','x86_64-windows-gnu','-std=gnu11','-Os','-municode','-Wl,--subsystem,windows','-I'+str(work),ROOT/'installer/setup.c',work/'setup.res','-lbcrypt','-lshell32','-lole32','-luser32','-o',setup]);print(setup)
if __name__=='__main__':main()
