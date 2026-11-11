"""Build using Zig's Windows C compiler, OBS 33.0.1 headers, and the installed obs.dll."""
import argparse
from pathlib import Path
import os
import struct
import subprocess

ROOT=Path(__file__).resolve().parent

def exported_names(path):
    data=path.read_bytes()
    pe=struct.unpack_from('<I',data,0x3c)[0]
    assert data[pe:pe+4]==b'PE\0\0'
    sections=struct.unpack_from('<H',data,pe+6)[0]
    optional_size=struct.unpack_from('<H',data,pe+20)[0]
    optional=pe+24
    assert struct.unpack_from('<H',data,optional)[0]==0x20b
    export_rva=struct.unpack_from('<I',data,optional+112)[0]
    table=optional+optional_size
    def offset(rva):
        for n in range(sections):
            header=table+n*40
            vsize,va,rsize,raw=struct.unpack_from('<IIII',data,header+8)
            if va<=rva<va+max(vsize,rsize):return raw+rva-va
        raise ValueError(f'RVA {rva:x}')
    exp=offset(export_rva)
    count=struct.unpack_from('<I',data,exp+24)[0]
    names=offset(struct.unpack_from('<I',data,exp+32)[0])
    result=[]
    for n in range(count):
        p=offset(struct.unpack_from('<I',data,names+n*4)[0])
        result.append(data[p:data.index(b'\0',p)].decode('ascii'))
    return result

