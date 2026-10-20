#!/usr/bin/env python3
"""Streaming reader for WXLTRC01, including gzip recordings."""
import argparse
import gzip
import json
from pathlib import Path
import struct

HEADER = struct.Struct('<7I')
KINDS = {1:'boundary', 2:'return', 3:'gp0', 4:'gp1', 5:'spu', 6:'cd_command',
         7:'cd_write', 8:'cd_read', 9:'timer_read', 10:'ram', 11:'pad',
         12:'sample_load', 13:'debug_write', 14:'state', 15:'game_call',
         16:'dma', 17:'call', 18:'end',19:'serial_pad_read',20:'serial_pad_write',21:'gpu_read',22:'spu_read',42:'late_packet',43:'table',44:'late_table'}
STATE_FIELDS = 'input menu race_context game_type owner ship lap section weapon attr damage perfect_lap perfect_count countdown autopilot update race_end race_start_vcount myseed gameplay_seed lateral_offset ship_damage cheat_damage rear_fire target_ship race_type plasma_busy combat_attr ship_target x y z'.split()
FUNCTIONS = {0x800684c4:'Swap', 0x800686e8:'CallbackSwap', 0x800820d8:'VSync',
             0x80082090:'GsGetVcount',0x80081fbc:'GetRCnt',0x8008a65c:'CdSync',
             0x8008a67c:'CdReady',0x8006726c:'anim',0x80077898:'SfxTransEffect',
             0x8002f53c:'DamageCollide',0x8006648c:'UpdateRescueDroid',0x8002fb3c:'CheckPitShips',0x80026dfc:'FireWeapon',0x80065c60:'SetFire',0x8003ce84:'CheckShipDestroyed'}
CD_COMMANDS = {1:'Nop',2:'Setloc',3:'Play',4:'Forward',5:'Backward',6:'ReadN',7:'MotorOn',
               8:'Stop',9:'Pause',10:'Init',11:'Mute',12:'Demute',13:'Setfilter',14:'Setmode',
               15:'Getparam',16:'GetlocL',17:'GetlocP',19:'GetTN',20:'GetTD',21:'SeekL',
               22:'SeekP',25:'Test',26:'GetID',27:'ReadS',30:'ReadTOC'}


def events(path, kinds=None):
    opener = gzip.open if str(path).endswith('.gz') else open
    with opener(path, 'rb') as f:
        if f.read(12) != b'WXLTRC01\x01\x00\x00\x00':
            raise ValueError('Unsupported retail trace header')
        ended = False
        while raw := f.read(HEADER.size):
            if len(raw) != HEADER.size:
                raise ValueError('Truncated event header')
            kind, size, iteration, frame, lo, hi, pc = HEADER.unpack(raw)
            if size % 4 or size > 16*1024*1024:
                raise ValueError(f'Invalid payload size {size}')
            payload = f.read(size)
            if len(payload) != size:
                raise ValueError('Truncated event payload')
            if ended:
                raise ValueError('Events after trace footer')
            ended = kind == 18
            if kinds is not None and KINDS.get(kind) not in kinds:continue
            yield dict(kind=KINDS.get(kind, f'unknown_{kind}'), iteration=iteration,
                       frame=frame, tick=lo | hi << 32, pc=pc,
                       words=list(struct.unpack('<'+'I'*(size//4),payload)))
        if not ended:
            raise ValueError('Missing trace footer; capture did not complete')


def describe(event):
    e = dict(event)
    w = e['words']
    if e['kind'] == 'state':
        e['state'] = dict(zip(STATE_FIELDS,w))
        for field in ('x','y','z','myseed','gameplay_seed','ship_target'):
            if e['state'].get(field,0)>=0x80000000:e['state'][field]-=0x100000000
    elif e['kind'] == 'pad':
        e.update(reader=['fixed','game','intro'][w[0]],input=w[1],address=w[2],
                 header=w[3]&65535,buttons_active_high=(w[3]>>16)^65535,
                 packet=struct.pack('<II',*w[3:5]).hex())
    elif e['kind'] in ('call','return','game_call'):
        e['function'] = FUNCTIONS.get(w[0],f'{w[0]:08x}')
        if e['kind']=='return':
            e.update(caller=w[1],args=w[2:6],v0=w[6],v1=w[7])
            if w[0] in (0x8008a65c,0x8008a67c):
                e['result_buffer']=struct.pack('<II',*w[8:10]).hex() if w[3] else None
    elif e['kind'] == 'sample_load':
        e['name'] = struct.pack('<16I',*w[2:]).split(b'\0',1)[0].decode('ascii')
        e['sample_id'] = w[0]
    elif e['kind'] == 'cd_command':
        e['command'] = CD_COMMANDS.get(w[0],f'{w[0]:02x}')
        e['params'] = w[2:]
    elif e['kind'] == 'gp1':
        value=w[0];op=value>>24;param=value&0xffffff
        e['command']={0:'reset',1:'clear_fifo',2:'ack_irq',3:'display_disable',4:'dma_direction',
                      5:'display_start',6:'horizontal_range',7:'vertical_range',8:'display_mode',9:'texture_disable'}.get(op,'unknown')
        e['param']=param
        if op==5:e.update(x=param&1023,y=param>>10&511)
        elif op==6:e.update(start=param&4095,end=param>>12&4095)
        elif op==7:e.update(start=param&1023,end=param>>10&1023)
        elif op==8:e.update(width=368 if param&64 else [256,320,512,640][param&3],
                           height=480 if param&4 else 240,pal=bool(param&8),rgb24=bool(param&16),interlace=bool(param&32))
    elif e['kind'] == 'spu':
        offset,value = w
        if offset < 0x180:
            e.update(voice=offset//16,register=['volume_left','volume_right','pitch','start',
                     'adsr_low','adsr_high','adsr_volume','loop'][offset%16//2],value=value)
            if offset%16 in (6,14):e['byte_address']=value*8
        else:
            names={0x180:'main_left',0x182:'main_right',0x184:'reverb_left',0x186:'reverb_right',
                   0x188:'key_on_low',0x18a:'key_on_high',0x18c:'key_off_low',0x18e:'key_off_high',
                   0x190:'pitch_mod_low',0x192:'pitch_mod_high',0x194:'noise_low',0x196:'noise_high',
                   0x198:'reverb_on_low',0x19a:'reverb_on_high',0x1a2:'reverb_base',
                   0x1a4:'irq_address',0x1a6:'transfer_address',0x1a8:'transfer_data',
                   0x1aa:'control',0x1ac:'transfer_control',0x1b0:'cd_left',0x1b2:'cd_right'}
            e.update(register=names.get(offset,'reverb_parameter' if 0x1c0<=offset<0x200 else f'{offset:03x}'),value=value)
            if offset in (0x188,0x18a,0x18c,0x18e):
                e['voices'] = [i+(16 if offset&2 else 0) for i in range(16) if value>>i&1]
    return e


def iterations(path):
    """Yield complete iteration event lists, then the final partial segment."""
    batch = []
    for e in events(path):
        batch.append(e)
        if e['kind'] in ('boundary','end'):
            yield dict(iteration=e['iteration'],complete=e['kind']=='boundary',events=batch)
            batch=[]


def ram_regions(path):
    """Yield sparse snapshots with the source pointer and its resolved address."""
    for e in events(path,kinds={'ram'}):
        source,address,size,depth,*words=e['words']
        data=struct.pack('<'+'I'*len(words),*words)[:size]
        yield dict(iteration=e['iteration'],frame=e['frame'],source=source,address=address,
                   depth=depth,size=size,data=data,valid=address!=0xffffffff)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('trace',type=Path);p.add_argument('--kind',action='append')
    p.add_argument('--start',type=int,default=0);p.add_argument('--end',type=int)
    a=p.parse_args()
    for e in events(a.trace):
        if e['iteration']>=a.start and (a.end is None or e['iteration']<=a.end) and (not a.kind or e['kind'] in a.kind):
            print(json.dumps(describe(e),separators=(',',':')))

if __name__=='__main__':main()
