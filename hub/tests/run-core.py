"""Headless native/Java interoperability checks. No GUI or system certificate changes.

Requires an existing H.264 fixture.bin (+.sps/.pps), Zig, JDK, OpenSSL,
and a separate directory containing the pinned relay Python dependency.
The positive WinHTTP tunnel fixture changes TLS to loopback WS only in a
scratch copy; the shipped source is unchanged. Android TLS is tested with
a JVM-only test trust store, then checked against untrusted/wrong-host certs.
"""
from pathlib import Path
import argparse,os,subprocess,sys,time,json,hashlib
ROOT=Path(__file__).resolve().parents[2]
HUB=ROOT/'hub';SRC=HUB/'src'
p=argparse.ArgumentParser()
for name in ('zig','jdk','work','fixture','relay-deps','openssl'):p.add_argument('--'+name,type=Path,required=True)
a=p.parse_args();work=a.work.resolve();work.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env['ZIG_GLOBAL_CACHE_DIR']=str(work/'zig-global');env['ZIG_LOCAL_CACHE_DIR']=str(work/'zig-local')
def run(args,**kw):return subprocess.run([str(x) for x in args],env=env,check=True,**kw)
base=[a.zig,'cc','-target','x86_64-windows-gnu','-std=gnu11','-DMC_BIND_ADDRESS=INADDR_LOOPBACK','-O2','-g','-I'+str(SRC)]
libs=['-lmfplat','-lmfuuid','-lwmcodecdspuuid','-lole32','-luuid','-lws2_32','-lbcrypt','-liphlpapi','-ladvapi32','-luser32']
native=[HUB/'tests/receiver-test.c',SRC/'receiver.c',SRC/'mfdecode.c',SRC/'ipc.c',SRC/'usb.c']
run([*base,*native,*libs,'-o',work/'receiver-test.exe'])
run([*base,HUB/'tests/relay-tls-test.c','-lwinhttp','-lws2_32','-o',work/'relay-tls-test.exe'])
run([*base,HUB/'tests/source-test.c','-lmfplat','-lmfuuid','-lmfreadwrite','-lole32','-luuid','-o',work/'source-test.exe'])
java=ROOT/'android/src/ru/codex/obslancamera';classes=work/'classes';classes.mkdir(exist_ok=True)
run([a.jdk/'bin/javac.exe','-encoding','UTF-8','--release','8','-d',classes,java/'Language.java',java/'Wire.java',java/'WssSocket.java',HUB/'tests/MultiWireProbe.java',HUB/'tests/TlsProbe.java'])
def pipeline(exe,remote=False):
 receiver=subprocess.Popen([str(exe)],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env)
 try:
  start=[]
  while True:
   line=receiver.stdout.readline();start.append(line)
   if 'HUB_READY' in line:break
   if not line:raise AssertionError('Receiver startup failed: '+''.join(start))
  command=[a.jdk/'bin/java.exe']
  if remote:command+=['-Djavax.net.ssl.trustStore='+str(work/'trust.p12'),'-Djavax.net.ssl.trustStorePassword=test-only']
  command+=['-cp',classes,'MultiWireProbe',a.fixture]
  if remote:command+=['wss://localhost:8766']
  sender=subprocess.Popen([str(v) for v in command],stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env)
  try:
   time.sleep(.8)
   run([work/'source-test.exe',HUB/'bin/MultiCameraTE.Source.dll','--live'],timeout=10)
   sender_text,_=sender.communicate(timeout=15);print(sender_text);assert sender.returncode==0
   remaining,_=receiver.communicate(timeout=20);content=''.join(start)+remaining;print(content)
   (work/('relay.log' if remote else 'lan.log')).write_text(content,encoding='utf-8');assert receiver.returncode==0
  finally:
   if sender.poll() is None:sender.kill();sender.wait()
 finally:
  if receiver.poll() is None:receiver.kill();receiver.wait()
pipeline(work/'receiver-test.exe')
run([a.openssl,'req','-x509','-newkey','rsa:2048','-nodes','-keyout',work/'key.pem','-out',work/'cert.pem','-days','2','-subj','/CN=localhost','-addext','subjectAltName=DNS:localhost'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
if not (work/'trust.p12').exists():run([a.jdk/'bin/keytool.exe','-importcert','-noprompt','-alias','local-test','-file',work/'cert.pem','-keystore',work/'trust.p12','-storepass','test-only'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
else:
 # Regenerate the test trust store without ever touching system roots.
 (work/'trust.p12').unlink();run([a.jdk/'bin/keytool.exe','-importcert','-noprompt','-alias','local-test','-file',work/'cert.pem','-keystore',work/'trust.p12','-storepass','test-only'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
product=(SRC/'relay.c').read_text(encoding='utf-8')
fixture=product.replace('L"https%ls"','L"http%ls"').replace('INTERNET_SCHEME_HTTPS','INTERNET_SCHEME_HTTP').replace('WINHTTP_FLAG_SECURE','0').replace('htons(MC_PORT)','htons(34880)')
assert fixture!=product and 'WinHttpSetOption(request,WINHTTP_OPTION_SECURITY_FLAGS' not in product
(work/'relay-loopback.c').write_text(fixture,encoding='utf-8')
run([*base,'-DMC_RELAY_TEST',*native,work/'relay-loopback.c',*libs,'-lwinhttp','-o',work/'receiver-relay-test.exe'])
relay=subprocess.Popen([sys.executable,str(HUB/'tests/relay-fixture.py'),str(a.relay_deps.resolve()),str(ROOT/'relay'),str(work/'cert.pem'),str(work/'key.pem')],stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,env=env)
try:
 assert 'RELAY_READY' in relay.stdout.readline()
 run([work/'relay-tls-test.exe'],timeout=15)
 run([a.jdk/'bin/java.exe','-cp',classes,'TlsProbe','wss://localhost:8766'])
 run([a.jdk/'bin/java.exe','-Djavax.net.ssl.trustStore='+str(work/'trust.p12'),'-Djavax.net.ssl.trustStorePassword=test-only','-cp',classes,'TlsProbe','wss://127.0.0.1:8766'])
 pipeline(work/'receiver-relay-test.exe',True)
finally:
 relay.terminate();relay.wait(timeout=5)
print('PASS: five simultaneous encrypted cameras, live pixels through five native media sources, independent PCM/mute, duplicate-slot rejection, WSS Android transport, relay tunnel, WebSocket keepalive, certificate and hostname rejection')
