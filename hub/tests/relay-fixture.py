"""Local test only: identical relay handler on WS host and TLS phone endpoints."""
import asyncio,os,ssl,sys
from pathlib import Path
sys.path.insert(0,sys.argv[1]);sys.path.insert(0,sys.argv[2])
os.environ['MULTICAMERA_HOST_KEY']='integration-test-only'
from websockets.asyncio.server import serve
from server import handler
async def main():
 tls=ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER);tls.load_cert_chain(sys.argv[3],sys.argv[4])
 options=dict(origins=[None],compression=None,max_size=65536,max_queue=4,ping_interval=.5,ping_timeout=1,close_timeout=1)
 async with serve(handler,'127.0.0.1',8765,**options),serve(handler,'127.0.0.1',8766,ssl=tls,**options):
  print('RELAY_READY',flush=True);await asyncio.Future()
asyncio.run(main())
