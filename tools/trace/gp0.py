#!/usr/bin/env python3
"""Decode submitted PSX GP0 words in execution order, across DMA and loop boundaries."""
import argparse
import hashlib
import struct
import json
from pathlib import Path
from reader import events


def signed(n,bits):
    return (n&((1<<bits)-1))-(1<<bits) if n&(1<<(bits-1)) else n&((1<<bits)-1)


def xy(w):return [signed(w,16),signed(w>>16,16)]
def rgb(w):return [w&255,w>>8&255,w>>16&255]
def uv(w):return [w&255,w>>8&255]

def page(w):
    return dict(raw=w,x=(w&15)*64,y=(w>>4&1)*256,blend=w>>5&3,
                blend_equation=['B/2+F/2','B+F','B-F','B+F/4'][w>>5&3],depth=[4,8,15,'reserved'][w>>7&3])

def clut(w):return dict(raw=w,x=(w&63)*16,y=w>>6&511)


class Decoder:
    def __init__(self):
        self.buffer=[];self.context=[];self.mode=0;self.offset=[0,0];self.window=0;self.order=0

    def length(self):
        b=self.buffer;op=b[0]>>24
        if 0x20<=op<0x40:
            n=4 if op&8 else 3
            return n*(1+bool(op&4)+bool(op&16))+(not bool(op&16))
        if 0x40<=op<0x60:
            if op&8:
                # The terminator is a coordinate position, never a colour word.
                first=4 if op&16 else 3;stride=2 if op&16 else 1
                for i in range(first,len(b),stride):
                    if b[i]&0xf000f000==0x50005000:return i+1
                return None
            return 4 if op&16 else 3
        if 0x60<=op<0x80:return 2+bool(op&4)+((op>>3&3)==0)
        if 0x80<=op<0xa0:return 4
        if 0xa0<=op<0xc0:
            if len(b)<3:return None
            width=((b[2]&65535)-1&1023)+1;height=((b[2]>>16)-1&511)+1
            return 3+(width*height+1)//2
        if 0xc0<=op<0xe0:return 3
        return 3 if op==2 else 1

    def feed(self, words, event, address=None, increment=4):
        for i,w in enumerate(words):
            self.buffer.append(w)
            self.context.append(dict(iteration=event['iteration'],frame=event['frame'],tick=event['tick'],
                                     address=None if address is None else (address+i*increment)&0x1fffff))
            length=self.length()
            if length is not None and len(self.buffer)>=length:
                packet=self.buffer[:length];context=self.context[0]
                self.buffer=self.buffer[length:];self.context=self.context[length:]
                yield self.decode(packet,context)

    def decode(self,b,context):
        op=b[0]>>24;result=dict(context,order=self.order,opcode=op,words=len(b))
        self.order+=1
        if 0x20<=op<0x40:
            textured=bool(op&4);gouraud=bool(op&16);n=4 if op&8 else 3
            result.update(type=('GT' if gouraud and textured else 'G' if gouraud else 'FT' if textured else 'F')+str(n),
                          semi_transparent=bool(op&2),raw_texture=bool(op&1),vertices=[])
            i=0 if gouraud else 1;col=rgb(b[0]);tp=self.mode;cl=None
            for vertex in range(n):
                if gouraud:col=rgb(b[i]);i+=1
                v=dict(xy=xy(b[i]),colour=col);i+=1
                if textured:
                    v['uv']=uv(b[i])
                    if vertex==0:cl=b[i]>>16
                    if vertex==1:tp=b[i]>>16;self.mode=(self.mode&~0x1ff)|(tp&0x1ff)
                    i+=1
                result['vertices'].append(v)
            result.update(tpage=page(tp),clut=clut(cl) if cl is not None else None,draw_offset=self.offset[:])
        elif 0x40<=op<0x60:
            gouraud=bool(op&16);polyline=bool(op&8)
            result.update(type='LINE_G2' if gouraud else 'LINE_F2',polyline=polyline,
                          semi_transparent=bool(op&2),vertices=[],tpage=page(self.mode),draw_offset=self.offset[:])
            i=0 if gouraud else 1;colour=rgb(b[0]);end=len(b)-bool(polyline)
            while i<end:
                if gouraud:colour=rgb(b[i]);i+=1
                result['vertices'].append(dict(xy=xy(b[i]),colour=colour));i+=1
        elif 0x60<=op<0x80:
            textured=bool(op&4);size=(op>>3)&3;i=2
            result.update(type='SPRT' if textured else 'TILE',xy=xy(b[1]),colour=rgb(b[0]),
                          semi_transparent=bool(op&2),raw_texture=bool(op&1),tpage=page(self.mode),draw_offset=self.offset[:])
            if textured:result.update(uv=uv(b[i]),clut=clut(b[i]>>16));i+=1
            result['size']=[b[i]&65535,b[i]>>16] if size==0 else [[1,1],[8,8],[16,16]][size-1]
        elif op==0xe1:
            self.mode=b[0]&0xffffff;result.update(type='DR_MODE',tpage=page(self.mode),dither=bool(self.mode&512),draw_to_display=bool(self.mode&1024))
        elif op==0xe2:
            self.window=b[0]&0xfffff;result.update(type='texture_window',raw=self.window)
        elif op in (0xe3,0xe4):result.update(type='draw_area_min' if op==0xe3 else 'draw_area_max',xy=[b[0]&1023,b[0]>>10&511])
        elif op==0xe5:
            self.offset=[signed(b[0],11),signed(b[0]>>11,11)];result.update(type='draw_offset',xy=self.offset[:])
        elif op==0xe6:result.update(type='mask',set_mask=bool(b[0]&1),check_mask=bool(b[0]&2))
        elif 0xa0<=op<0xc0:
            width=((b[2]&65535)-1&1023)+1;height=((b[2]>>16)-1&511)+1
            pixels=struct.pack('<'+'I'*len(b[3:]),*b[3:])[:width*height*2]
            result.update(type='upload',xy=xy(b[1]),size=[width,height],data_sha256=hashlib.sha256(pixels).hexdigest())
        elif 0xc0<=op<0xe0:result.update(type='readback',xy=xy(b[1]),size=[b[2]&65535,b[2]>>16])
        elif 0x80<=op<0xa0:result.update(type='copy',source=xy(b[1]),destination=xy(b[2]),size=[b[3]&65535,b[3]>>16])
        elif op==2:result.update(type='fill',colour=rgb(b[0]),xy=xy(b[1]),size=[b[2]&65535,b[2]>>16])
        else:result.update(type={0:'nop',1:'clear_cache',0x1f:'irq'}.get(op,'unknown'),raw=b)
        return result


def packets(path):
    decoder=Decoder();dma=None
    for e in events(path,kinds={'gp0','gp1','dma'}):
        if e['kind']=='dma':dma=e['words']
        elif e['kind']=='gp0':
            yield from decoder.feed(e['words'],e,address=dma[0] if dma else None,increment=dma[1] if dma else 4)
            dma=None
        elif e['kind']=='gp1':
            w=e['words'][0];op=w>>24
            if op in (0,1):
                # GP1 reset/clear FIFO can discard an incomplete GP0 transfer.
                decoder.buffer=[];decoder.context=[]
            if op==0:decoder.mode=0;decoder.offset=[0,0];decoder.window=0
    if decoder.buffer:raise ValueError('Trace ends inside a GP0 packet')


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('trace',type=Path)
    p.add_argument('--start',type=int,default=0);p.add_argument('--end',type=int)
    a=p.parse_args()
    for packet in packets(a.trace):
        if packet['iteration']>=a.start and (a.end is None or packet['iteration']<=a.end):
            print(json.dumps(packet,separators=(',',':')))

if __name__=='__main__':main()
