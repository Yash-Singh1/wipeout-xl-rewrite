#!/usr/bin/env python3
"""Compare the port's drawing with a console recording, frame by frame.

Both traces are WXLTRC01 files: the console one from the recorder, the port's
from WIPEOUT_TRACE. The GPU command streams are decoded into packets and cut
into frames at each drawing-environment change (PutDrawEnv's area packet).
Frames are compared packet by packet, so the report names the first frame
and the first packet in it where the port draws something else.

GPU traffic from the console's movie player (code loaded at 0x80119000) is
left out: the port plays the movies itself. Game drawing that
the console's DMA delivers after the player has been started still counts.
"""
import argparse
from copy import deepcopy
import json
from pathlib import Path

from reader import events
from gp0 import Decoder

MOVIE_LOW, MOVIE_HIGH = 0x80119000, 0x80140000
ROM_LOW = 0xBFC00000  # the boot ROM's own writes (around Exec), not the game's
META = {'iteration', 'frame', 'tick', 'address', 'order', 'words'}


def foreign(pc):
    return MOVIE_LOW <= pc < MOVIE_HIGH


class RawDecoder(Decoder):
    """A decoder that keeps each packet's words."""
    def decode(self, b, context):
        result = Decoder.decode(self, b, context)
        result['raw_words'] = list(b)
        return result


def normalise(packet):
    """The packet's values the GPU uses. Raw-textured primitives ignore their
    colour, which the game often leaves uninitialised."""
    p = {k: v for k, v in packet.items() if k not in META and k != 'raw_words'}
    if p.get('raw_texture'):
        p.pop('colour', None)
        for v in p.get('vertices', ()):
            v.pop('colour', None)
    return p


class Frame:
    """One frame: its packets and, for port traces, its ordering tables (the
    packets of each node, in drawing order) with each node's packet words and
    link as they were when the game next waited for the GPU."""
    def __init__(self, iteration):
        self.iteration = iteration
        self.packets = []
        self.raw = []
        self.table_start = None
        self.table_end = None
        self.state = None          # the decoder at the start of the tables
        self.nodes = []            # [(link number, [packet index, ...]), ...] over all tables
        self.table_of = []         # table number of each node
        self.late_heads = []
        self.late = {}             # link number -> (next link number, words)


def frames(path):
    """Yield a Frame per drawing environment."""
    decoder = RawDecoder()
    frame, started = None, False
    dma = False
    kinds = {'gp0', 'gp1', 'dma', 'table', 'late_table', 'late_packet'}
    tables = 0
    for e in events(path, kinds=kinds):
        kind = e['kind']
        if kind == 'dma':
            dma = True
            continue
        delivered, dma = dma, False
        if foreign(e['pc']) or (e['pc'] >= ROM_LOW and not delivered):
            continue
        if kind == 'gp1':
            if e['words'][0] >> 24 == 0:
                decoder.buffer, decoder.context = [], []
                decoder.mode, decoder.offset, decoder.window = 0, [0, 0], 0
            continue
        if frame is None and kind != 'gp0':
            continue
        if kind == 'table':
            pairs = list(zip(e['words'][0::2], e['words'][1::2]))
            total = sum(n for _, n in pairs)
            start, words = len(frame.packets), 0
            while words < total and start > 0:
                start -= 1
                words += len(frame.raw[start])
            index = start
            for node, n in pairs:
                members, used = [], 0
                while used < n and index < len(frame.packets):
                    members.append(index)
                    used += len(frame.raw[index])
                    index += 1
                frame.nodes.append((node, members))
                frame.table_of.append(tables)
            tables += 1
            frame.table_end = len(frame.packets)
            continue
        if kind == 'late_table':
            frame.late_heads.append(e['words'][0])
            continue
        if kind == 'late_packet':
            w = e['words']
            frame.late[w[0]] = (w[1], w[2:])
            continue
        for p in decoder.feed(e['words'], e):
            if p['type'] == 'draw_area_min':
                if frame is not None:
                    yield frame
                frame = Frame(p['iteration'])
                tables = 0
                started = True
            elif not started:
                continue  # the console's boot ROM draws before the game starts
            frame.packets.append(normalise(p))
            frame.raw.append(p['raw_words'])
            if p['type'] == 'mask' and len(frame.packets) == 6:
                frame.table_start = 6
                frame.state = deepcopy(decoder)
            elif frame.table_start == 6 and len(frame.packets) == 7 and p['type'] in ('fill', 'TILE'):
                frame.table_start = 7
                frame.state = deepcopy(decoder)
    if frame is not None:
        yield frame


END = 0xFFFFFF
WINDOW = 96   # nodes before the first difference where the GPU may have gone over


def decode_all(decoder, words):
    return [normalise(p) for p in decoder.feed(words, dict(iteration=0, frame=0, tick=0))]


def decode_one(decoder, words):
    out = decode_all(decoder, words)
    return out[0] if out else None


def late_path(port, table, node, limit):
    """Node words from node on, following the late links, then the later tables."""
    out = []
    while len(out) <= limit:
        if node == END:
            table += 1
            if table >= len(port.late_heads):
                break
            node = port.late_heads[table]
            continue
        entry = port.late.get(node)
        if entry is None:
            break
        out.append(entry[1])
        node = entry[0]
    return out


def table_split(console, port):
    """Where the port frame's tables are, and the console frame's matching part."""
    head = port.nodes[0][1][0] if port.nodes and port.nodes[0][1] else (port.table_start or 0)
    end = port.table_end if port.table_end is not None else head
    tail = len(port.packets) - end
    return head, len(console.packets) - tail, end


def explained(a, b):
    """None when the console frame is the port's as the console's GPU could
    have read it; else the index of the first packet nothing explains."""
    if a.packets == b.packets:
        return None
    if not b.nodes or b.state is None:
        return first_plain(a, b)
    head, end, port_end = table_split(a, b)
    if a.packets[:head] != b.packets[:head]:
        return first_plain(a, b)
    if a.packets[end:] != b.packets[port_end:]:
        return end + first_plain_list(a.packets[end:], b.packets[port_end:])
    console, early = a.packets[head:end], b.packets[head:port_end]
    k = first_plain_list(console, early)
    # the node holding packet k
    first_node = 0
    for n, (_, members) in enumerate(b.nodes):
        if members and members[-1] - head >= k:
            first_node = n
            break
    best = k
    decoder = deepcopy(b.state)
    start = max(0, first_node - WINDOW)
    for n in range(start):
        for i in b.nodes[n][1]:
            decode_all(decoder, b.raw[i])
    for switch in range(start, first_node + 1):
        trial = deepcopy(decoder)
        done = (b.nodes[switch][1][0] - head) if b.nodes[switch][1] else 0
        words = late_path(b, b.table_of[switch], b.nodes[switch][0], len(console) - done + 2)
        n = done
        ok = True
        for w in words:
            for packet in decode_all(trial, w):
                if n >= len(console) or packet != console[n]:
                    ok = False
                    break
                n += 1
            if not ok:
                break
        if ok and n == len(console):
            return None
        best = max(best, n)
        for i in b.nodes[switch][1]:
            decode_all(decoder, b.raw[i])
    if bounded_race(console, b, head):
        return None
    return head + best


def race_key(packet):
    """A packet's drawn values without the texture page an untextured
    primitive inherits from the packets before it."""
    p = dict(packet)
    if not any('uv' in v for v in p.get('vertices', ())) and 'uv' not in p:
        p.pop('tpage', None)
    return json.dumps(p, sort_keys=True)


def bounded_race(console, port, head):
    """The console's GPU read the table while the game was rebuilding some of
    its packets (changing them or linking them into the next table), and no
    single point of going over explains it. Accept the frame when every packet
    the game did not touch is there in order and every other packet is one
    of the touched packets as drawn or as left, or at least a packet of the
    same kind (torn: read while the game was writing it)."""
    nodes = port.nodes
    touched, relinked = set(), set()
    for n, (node, members) in enumerate(nodes):
        late = port.late.get(node)
        words = [w for i in members for w in port.raw[i]]
        nxt = nodes[n + 1][0] if n + 1 < len(nodes) and port.table_of[n + 1] == port.table_of[n] else END
        if late is None or list(late[1]) != words:
            touched.add(n)
        if late is not None and late[0] != nxt:
            touched.add(n)
            relinked.update(port.packets[i]['opcode'] for i in members)
    if not touched:
        return False
    allowed, kinds = set(), set()
    for n in touched:
        for i in nodes[n][1]:
            allowed.add(race_key(port.packets[i]))
            kinds.add(port.packets[i]['opcode'])
    for _, words in port.late.values():
        for packet in decode_all(deepcopy(port.state), words):
            allowed.add(race_key(packet))
            kinds.add(packet.get('opcode'))
    steady = [race_key(port.packets[i]) for n, (_, members) in enumerate(nodes)
              if n not in touched for i in members if port.packets[i]['opcode'] not in relinked]
    j = 0
    for packet in console:
        key = race_key(packet)
        if j < len(steady) and key == steady[j]:
            j += 1
        elif key not in allowed and packet.get('opcode') not in kinds:
            return False
    return j == len(steady)


def first_plain_list(x, y):
    for i in range(max(len(x), len(y))):
        if i >= len(x) or i >= len(y) or x[i] != y[i]:
            return i
    return len(x)


def first_plain(a, b):
    return first_plain_list(a.packets, b.packets)


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('console', type=Path, help='recording directory or its trace.bin(.gz)')
    p.add_argument('port', type=Path, help="the port's trace")
    p.add_argument('--frames', type=int, help='compare at most this many frames')
    p.add_argument('--show', type=int, default=8, help='mismatched frames to list')
    p.add_argument('--context', type=int, default=3, help='packets shown before the first difference')
    args = p.parse_args()

    console = args.console / 'trace.bin.gz' if args.console.is_dir() else args.console
    left, right = frames(console), frames(args.port)
    count = same = timing = 0
    mismatched = []
    first = None
    while args.frames is None or count < args.frames:
        a, b = next(left, None), next(right, None)
        if a is None:
            if b is not None:
                print('(the port ran on past the end of the console recording)')
            break
        count += 1
        index = explained(a, b) if b is not None else 0
        if index is None:
            same += 1
            timing += a.packets != b.packets
            continue
        mismatched.append((count - 1, a.iteration, b.iteration if b else None,
                           len(a.packets), len(b.packets) if b else 0))
        if first is None:
            first = count - 1, a, b, index
    print(f'frames compared: {count}, the same: {same} ({same - timing} packet for packet, '
          f'{timing} as the console GPU read the tables while the game went on), different: {count - same}')
    for index, ci, pi, cn, pn in mismatched[:args.show]:
        print(f'  frame {index}: console iteration {ci} ({cn} packets), port iteration {pi} ({pn} packets)')
    if first:
        index, a, b, i = first
        print(f'\nfirst different frame: {index}')
        if b is None:
            print('  only the console has this frame')
        else:
            print(f'  first packet no GPU timing explains: {i} of {len(a.packets)} (console) / {len(b.packets)} (port)')
            for j in range(max(0, i - args.context), i):
                print(f'  same    {json.dumps(a.packets[j], sort_keys=True)}')
            x = a.packets[i] if i < len(a.packets) else None
            y = b.packets[i] if i < len(b.packets) else None
            print(f'  console {json.dumps(x, sort_keys=True)}')
            print(f'  port    {json.dumps(y, sort_keys=True)}  (as drawn)')
    return 0 if first is None else 1


if __name__ == '__main__':
    raise SystemExit(main())
