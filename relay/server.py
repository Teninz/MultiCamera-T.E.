"""MultiCamera T.E. opaque WebSocket relay. Put behind a trusted HTTPS reverse proxy."""
import asyncio,hmac,os,re,signal
from dataclasses import dataclass,field
from websockets.asyncio.server import serve

@dataclass
class Slot:
    host: object=None
    phone: object=None
    ready: asyncio.Event=field(default_factory=asyncio.Event)

slots={}
peers=0
HOST_KEY=os.environ.get('MULTICAMERA_HOST_KEY','')
MAX_PEERS=320

async def forward(source,target):
    async for packet in source:
        if not isinstance(packet,bytes):
            await source.close(1003,'Binary transport required')
            return
        await target.send(packet)

async def handler(connection):
    global peers
    match=re.fullmatch(r'/v1/(host|phone)/([0-9a-f]{32})/([1-5])',connection.request.path)
    if match is None or peers>=MAX_PEERS:
        await connection.close(1008,'Invalid route or capacity reached')
        return
    role,room,number=match.groups()
    if role=='host' and HOST_KEY and not hmac.compare_digest(connection.request.headers.get('Authorization',''),'Bearer '+HOST_KEY):
        await connection.close(1008,'Host authorization required')
        return
    key=(room,number)
    slot=slots.setdefault(key,Slot())
    if getattr(slot,role) is not None:
        await connection.close(1008,'Slot occupied')
        return
    setattr(slot,role,connection)
    peers+=1
    if slot.host and slot.phone:slot.ready.set()
    try:
        if role=='host':
            ready=asyncio.create_task(slot.ready.wait())
            closed=asyncio.create_task(connection.wait_closed())
            done,pending=await asyncio.wait([ready,closed],return_when=asyncio.FIRST_COMPLETED)
            for task in pending:task.cancel()
            await asyncio.gather(*pending,return_exceptions=True)
            if closed in done or slot.phone is None:return
            to_phone=asyncio.create_task(forward(connection,slot.phone))
            to_host=asyncio.create_task(forward(slot.phone,connection))
            done,pending=await asyncio.wait([to_phone,to_host],return_when=asyncio.FIRST_COMPLETED)
            for task in pending:task.cancel()
            await asyncio.gather(*done,*pending,return_exceptions=True)
        else:
            await connection.wait_closed()
    finally:
        peers-=1
        if getattr(slot,role) is connection:setattr(slot,role,None)
        counterpart=slot.phone if role=='host' else slot.host
        if counterpart is not None:await counterpart.close(1000,'Peer disconnected')
        slot.ready.clear()
        if slot.host is None and slot.phone is None:slots.pop(key,None)

async def main():
    host=os.environ.get('MULTICAMERA_BIND','127.0.0.1')
    port=int(os.environ.get('MULTICAMERA_PORT','8080'))
    async with serve(handler,host,port,origins=[None],compression=None,max_size=65536,max_queue=4,ping_interval=20,ping_timeout=20,close_timeout=3):
        print('MultiCamera T.E. relay ready',flush=True)
        await asyncio.Future()

if __name__=='__main__':
    try:asyncio.run(main())
    except KeyboardInterrupt:pass
