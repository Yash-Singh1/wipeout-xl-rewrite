#!/usr/bin/env python3
"""Reads a frame recording saved by the frame recorder (Tab in the game, platform/psx/framecap.h).

  framecap.py DIR                      the frames: game frame, primitives, shown or not
  framecap.py DIR pixel FRAME X Y      every primitive at pixel X, Y of FRAME (the console's columns: with
                                       widescreen the sides are left of 0 and beyond 319), topmost last, and
                                       what each writes there
  framecap.py DIR flicker              pixels that turn dark for a frame or two and back, with the
                                       primitives on top there before, during and after
  framecap.py DIR prim FRAME SERIAL    one primitive in full
"""
import math
import os
import struct
import sys

PRIM_HEAD = struct.Struct('<iBBBBHH4h4h')
VERTEX = struct.Struct('<fffBBBBffB')
DRAW = struct.Struct('<15i')
PRIM_TAIL = struct.Struct('<Q48s')
FRAME = struct.Struct('<qiiiiiiiii')
# the last field (a depth test the port once had) is always 0 now, as the vertices' sky mark (bg)
DRAW_FIELDS = ('textured', 'raw', 'semi', 'depth', 'pageX', 'pageY', 'clutX', 'clutY', 'windowMaskX', 'windowMaskY',
               'windowOffsetX', 'windowOffsetY', 'dither', 'fade', 'depthTest')
KINDS = ('polygon', 'line', 'rectangle', 'fill')
SKIP0, SKIP1, FAR, PRECISE, DROPPED = 1, 2, 4, 8, 16
BLENDS = ('B/2+F/2', 'B+F', 'B-F', 'B+F/4')


class Prim:
    pass


def load(folder):
    data = open(os.path.join(folder, 'frames.bin'), 'rb').read()
    assert data[:4] == b'XLFC', 'not a frame recording'
    version, count = struct.unpack_from('<ii', data, 4)
    pos = 12
    frames = []
    for index in range(count):
        game, shown, ox, oy, ax, ay, aw, ah, vram, n = FRAME.unpack_from(data, pos)
        pos += FRAME.size
        picture_x = 0  # the column of the picture's left edge (version 2: widescreen's pictures start left of 0)
        if version >= 2:
            picture_x, = struct.unpack_from('<i', data, pos)
            pos += 4
        between = 0.0  # version 3: a picture drawn between game frames, how far along (0: a game frame)
        if version >= 3:
            between, = struct.unpack_from('<f', data, pos)
            pos += 4
        prims = []
        for _ in range(n):
            p = Prim()
            p.serial, p.kind, p.code, p.flags, p.count, p.tpage, p.clut, *xy = PRIM_HEAD.unpack_from(data, pos)
            p.ix, p.iy = xy[:4], xy[4:]
            pos += PRIM_HEAD.size
            p.v = []
            for _ in range(4):
                x, y, w, r, g, b, a, u, v, bg = VERTEX.unpack_from(data, pos)
                pos += VERTEX.size
                p.v.append(dict(x=x, y=y, w=w, r=r, g=g, b=b, a=a, u=u, v=v, bg=bg))
            p.draw = dict(zip(DRAW_FIELDS, DRAW.unpack_from(data, pos)))
            pos += DRAW.size
            p.address, tag = PRIM_TAIL.unpack_from(data, pos)
            pos += PRIM_TAIL.size
            p.tag = tag.split(b'\0')[0].decode('latin-1')
            prims.append(p)
        frames.append(dict(index=index, game=game, shown=shown, offset=(ox, oy), area=(ax, ay, aw, ah), vram=vram,
                           prims=prims, folder=folder, picture_x=picture_x, between=between))
    return frames


_vram = {}


def vram(frame):
    key = (frame['folder'], frame['vram'])
    if key not in _vram:
        path = os.path.join(frame['folder'], 'vram_%d.bin' % frame['vram'])
        raw = open(path, 'rb').read() if os.path.exists(path) else b''
        _vram[key] = struct.unpack('<%dH' % (len(raw) // 2), raw) if raw else None
    return _vram[key]


_pictures = {}


def picture(frame):
    """RGB rows of the picture shown, or None (picture_NNN.jpg; older recordings: .ppm)."""
    base = os.path.join(frame['folder'], 'picture_%03d' % frame['index'])
    if base not in _pictures:
        _pictures[base] = None
        if os.path.exists(base + '.jpg'):
            from PIL import Image  # (the pictures are JPEGs at the framebuffer's scale)
            image = Image.open(base + '.jpg').convert('RGB')
            _pictures[base] = (image.width, image.height, image.tobytes())
        elif os.path.exists(base + '.ppm'):
            raw = open(base + '.ppm', 'rb').read()
            parts = raw.split(b'\n', 3)
            w, h = map(int, parts[1].split())
            _pictures[base] = (w, h, parts[3])
    return _pictures[base]


def pixel_rgb(frame, x, y):
    """The picture's colour at the console's column x, row y."""
    pic = picture(frame)
    if not pic:
        return None
    w, h, data = pic
    scale = max(1, h // 240)  # the picture's pixels per console pixel (240 rows of the console's)
    x -= frame['picture_x']
    if not (0 <= x < w // scale and 0 <= y < 240):
        return None
    i = ((y * scale + scale // 2) * w + x * scale + scale // 2) * 3
    return tuple(data[i:i + 3])


def name(p):
    if p.kind == 0:
        s = ('G' if p.code & 0x10 else 'F') + ('T' if p.code & 4 else '') + str(p.count)
    else:
        s = KINDS[p.kind]
    if p.code & 2 and p.kind != 3:
        s += ' semi'
    if p.code & 1 and p.kind == 0:
        s += ' raw'
    return s


def inside(a, b, c, x, y):
    d = (b['y'] - c['y']) * (a['x'] - c['x']) + (c['x'] - b['x']) * (a['y'] - c['y'])
    if abs(d) < 1e-6:
        return None
    l0 = ((b['y'] - c['y']) * (x - c['x']) + (c['x'] - b['x']) * (y - c['y'])) / d
    l1 = ((c['y'] - a['y']) * (x - c['x']) + (a['x'] - c['x']) * (y - c['y'])) / d
    l2 = 1 - l0 - l1
    if min(l0, l1, l2) < -1e-4:
        return None
    return (l0, l1, l2)


def shade(frame, p, t, l):
    """What gpu_gl.c's fragment shader writes at barycentric l of triangle t."""
    ws = [v['w'] if v['w'] > 0 else 1.0 for v in t]
    k = [l[i] / ws[i] for i in range(3)]
    s = sum(k)
    k = [x / s for x in k]
    u = sum(k[i] * t[i]['u'] for i in range(3))
    v = sum(k[i] * t[i]['v'] for i in range(3))
    c = [sum(k[i] * t[i][ch] for i in range(3)) for ch in 'rgba']
    out = dict(u=u, v=v, texel=None, alpha=c[3], transparent=False)
    d = p.draw
    mem = vram(frame)
    if not d['textured'] or d['depth'] == 3 or mem is None:
        out['rgb'] = c[:3]
        return out

    def fetch(x, y):
        return mem[(y & 511) * 1024 + (x & 1023)]
    iu, iv = int(math.floor(u)) & 255, int(math.floor(v)) & 255
    iu = (iu & ~(d['windowMaskX'] * 8)) | ((d['windowOffsetX'] & d['windowMaskX']) * 8)
    iv = (iv & ~(d['windowMaskY'] * 8)) | ((d['windowOffsetY'] & d['windowMaskY']) * 8)
    if d['depth'] == 0:
        index = (fetch(d['pageX'] + (iu >> 2), d['pageY'] + iv) >> ((iu & 3) * 4)) & 15
        texel = fetch(d['clutX'] + index, d['clutY'])
    elif d['depth'] == 1:
        index = (fetch(d['pageX'] + (iu >> 1), d['pageY'] + iv) >> ((iu & 1) * 8)) & 255
        texel = fetch(d['clutX'] + index, d['clutY'])
    else:
        texel = fetch(d['pageX'] + iu, d['pageY'] + iv)
    out['texel'] = texel
    out['transparent'] = texel == 0
    t8 = [(texel & 31) * 8, ((texel >> 5) & 31) * 8, ((texel >> 10) & 31) * 8]
    if not d['raw']:
        t8 = [min(t8[i] * c[i] / 128.0, 255.0) for i in range(3)]
    out['rgb'] = t8
    return out


def hits(frame, x, y):
    """The triangles covering video-memory pixel x, y, in drawing order."""
    px, py = x + 0.5, y + 0.5
    found = []
    for i, p in enumerate(frame['prims']):
        if p.kind == 3:
            a, b = p.v[0], p.v[1]
            if a['x'] <= px < b['x'] and a['y'] <= py < b['y']:
                found.append(dict(prim=i, triangle=0, skipped=False, rgb=[a['r'], a['g'], a['b']], texel=None,
                                  transparent=False, alpha=255, u=0, v=0))
            continue
        if p.kind == 1:
            continue
        triangles = 1 if p.kind == 0 and p.count == 3 else 2
        for k in range(triangles):
            t = p.v[k:k + 3]
            l = inside(t[0], t[1], t[2], px, py)
            if l is None:
                continue
            h = shade(frame, p, t, l)
            h.update(prim=i, triangle=k,
                     skipped=bool(p.flags & DROPPED or p.flags & (SKIP1 if k else SKIP0)))
            found.append(h)
    return found


def notes(p, h):
    n = []
    if h['skipped']:
        n.append('NOT DRAWN (%s)' % ('dropped' if p.flags & DROPPED else 'GPU size limit'))
    if h['transparent']:
        n.append('transparent texel')
    if p.draw['semi'] >= 0 and p.kind != 3:
        n.append('blend ' + BLENDS[p.draw['semi'] & 3])
    if h['alpha'] < 254.5 and p.kind != 3:
        n.append('fade %.0f%%' % (h['alpha'] / 2.55))
    if p.flags & FAR:
        n.append('far view')
    if p.kind == 0 and not p.flags & PRECISE:
        n.append('affine')
    if not h['skipped'] and not h['transparent'] and sum(h['rgb']) < 24:
        n.append('BLACK')
    return ', '.join(n)


def describe(frame, x, y, out=sys.stdout):
    ax, ay = frame['area'][:2]
    found = hits(frame, ax + x, ay + y)
    print('frame %d (game %d) pixel %d,%d shows %s; %d primitives there, drawing order:' % (
        frame['index'], frame['game'], x, y, pixel_rgb(frame, x, y), len(found)), file=out)
    for h in found:
        p = frame['prims'][h['prim']]
        texel = '' if h['texel'] is None else ' texel %04x' % h['texel']
        print('  #%-5d %-28s %-12s writes %3.0f,%3.0f,%3.0f%s  %s' % (
            p.serial, p.tag or '(untagged)', name(p), *h['rgb'], texel, notes(p, h)), file=out)
    return found


def dump_prim(frame, serial):
    p = frame['prims'][serial]
    print('#%d %s %s packet %02x at %x flags %x tpage %04x clut %04x' % (
        p.serial, p.tag or '(untagged)', name(p), p.code, p.address, p.flags, p.tpage, p.clut))
    ox, oy = frame['offset']
    for i in range(4 if p.kind != 3 else 2):
        if i >= p.count:
            break
        v = p.v[i]
        print('  corner %d: packet %d,%d drawn %.2f,%.2f w %.1f rgb %d,%d,%d a %d uv %.1f,%.1f bg %d' % (
            i, p.ix[i], p.iy[i], v['x'] - ox, v['y'] - oy, v['w'], v['r'], v['g'], v['b'], v['a'], v['u'], v['v'],
            v['bg']))
    print('  draw', p.draw)


def flicker(frames, threshold=40):
    """Pixels dark in a shown frame but not in the shown frames before and after it."""
    shown = [f for f in frames if f['shown'] and picture(f)]
    for i in range(1, len(shown) - 1):
        a, b, c = shown[i - 1], shown[i], shown[i + 1]
        w, h, _ = picture(b)
        w, h = w // max(1, h // 240), 240  # in console pixels
        spots = []
        for y in range(0, h, 2):
            for x in range(b['picture_x'], b['picture_x'] + w, 2):
                pb = pixel_rgb(b, x, y)
                if sum(pb) > threshold:
                    continue
                pa, pc = pixel_rgb(a, x, y), pixel_rgb(c, x, y)
                if pa and pc and sum(pa) > threshold * 3 and sum(pc) > threshold * 3:
                    spots.append((x, y))
        if len(spots) < 8:
            continue
        xs = sorted(s[0] for s in spots)
        ys = sorted(s[1] for s in spots)
        x, y = xs[len(xs) // 2], ys[len(ys) // 2]
        if (x, y) not in spots:
            x, y = spots[len(spots) // 2]
        print('\n== frame %d: %d dark pixels that are bright just before and after, around %d,%d' % (
            b['index'], len(spots) * 4, x, y))
        for f in (a, b, c):
            found = hits(f, f['area'][0] + x, f['area'][1] + y)
            top = [h for h in found if not h['skipped'] and not h['transparent']]
            last = top[-1] if top else None
            if last:
                p = f['prims'][last['prim']]
                print('  frame %d pixel %s: on top #%d %s %s writes %.0f,%.0f,%.0f  %s' % (
                    f['index'], pixel_rgb(f, x, y), p.serial, p.tag or '(untagged)', name(p), *last['rgb'],
                    notes(p, last)))
            else:
                print('  frame %d pixel %s: nothing drawn there' % (f['index'], pixel_rgb(f, x, y)))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return
    frames = load(sys.argv[1])
    command = sys.argv[2] if len(sys.argv) > 2 else 'list'
    if command == 'list':
        for f in frames:
            print('%3d game %6d shown %6d offset %s prims %5d vram %d' % (
                f['index'], f['game'], f['shown'], f['offset'], len(f['prims']), f['vram']))
    elif command == 'pixel':
        describe(frames[int(sys.argv[3])], int(sys.argv[4]), int(sys.argv[5]))
    elif command == 'prim':
        dump_prim(frames[int(sys.argv[3])], int(sys.argv[4]))
    elif command == 'flicker':
        flicker(frames)
    else:
        print(__doc__)


if __name__ == '__main__':
    main()
