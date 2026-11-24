#!/usr/bin/env python3
"""
The enemy's month near you (release 2): a flat preview of one stop of the watch, drawn in the cloud.

The arrows and battle marks are the game's own (Campaign/EnemyMoves, laid by Tools/enemy_moves/run.sh); the map under
them is the campaign map's provinces, flat, tinted by who holds each when the step ends; the plate at the bottom is the
watch's heading and sentence as the table shows them. It is not the 3D table: the Mac takes those pictures.

Round 2 (Brian, 24 Nov 18:01Z, "yes to all"): the TAKE ME THERE button under the sentence, the small map in the
corner with the watch's stops on it (Campaign/EnemyMoves Ping: the ones to come pulsing, caught here mid-swell, the one
on screen in a brass ring, the ones seen a faint dot), and a red tag lettered with the town beside an army marked for
taking one of his towns (src/campaign/marked.ts).

Usage: picture.py near-steps.json out-dir HUMAN:MONTH:NAT [...]
  near-steps.json: the steps the watch stops at, each {human, month, nat, date, arrows, nearNote, allyNote, note, ctrl,
  corps: [{reg, nat, mark}]}.
"""
import math
import json
import os
import subprocess
import sys
import tempfile
import textwrap

from PIL import Image, ImageDraw, ImageFilter, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, '..', '..', '..', '..'))
MAP = os.path.join(ROOT, 'src', 'generated', 'campaign-map.json')
NATIONS = os.path.join(ROOT, 'data', 'campaign', 'nations.json')

W, H = 1600, 1000
SS = 2  # drawn at twice the size, then made smaller: smooth edges
SEA = (38, 52, 62)
PAPER = (205, 190, 158)
LINE = (58, 48, 38)
CREAM = (238, 230, 208)
BRASS = (224, 184, 114)


def font(size, bold=False):
    for name in (['DejaVuSerif-Bold.ttf'] if bold else []) + ['DejaVuSerif.ttf', 'LiberationSerif-Regular.ttf', 'FreeSerif.ttf']:
        for folder in ('/usr/share/fonts/truetype/dejavu', '/usr/share/fonts/truetype/liberation', '/usr/share/fonts/truetype/freefont'):
            path = os.path.join(folder, name)
            if os.path.exists(path):
                return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def hexrgb(h):
    h = h.lstrip('#')
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def mix(a, b, t):
    return tuple(int(round(a[i] * (1 - t) + b[i] * t)) for i in range(3))


def lay(arrows, regions):
    """The game's own drawing of these arrows (run.sh), ends at the provinces' middles."""
    with tempfile.TemporaryDirectory() as tmp:
        src = os.path.join(tmp, 'arrows.txt')
        out = os.path.join(tmp, 'drawing.json')
        with open(src, 'w') as f:
            for a in arrows:
                kind = {'move': 0, 'attack': 1, 'fell': 2}[a['kind']]
                to = regions[a['to']]
                has_from = a['from'] >= 0 and a['from'] != a['to']
                fr = regions[a['from']] if has_from else to
                f.write(f"{kind} {fr['x']} {fr['y']} {to['x']} {to['y']} {int(a.get('war', True))} {int(bool(a.get('away')))} "
                        f"{int(bool(a.get('taken')))} {int(bool(a.get('siege')))} {max(1, a.get('corps', 1))} {int(has_from)} "
                        f"{int(bool(a.get('ally')))} {int(bool(a.get('ours')))}\n")
        subprocess.run(['bash', os.path.join(HERE, 'run.sh'), src, out], check=True, stdout=subprocess.DEVNULL,
                       env={**os.environ, 'MOVES_OUT': os.path.join(tmp, 'build')})
        with open(out) as f:
            return json.load(f)


# The corner pings (Campaign/EnemyMoves.cpp Worst, Ping, PingInk), as the small map draws them.
PING_SECONDS = 1.4
INKS = {'war': (176, 30, 26), 'peace': (226, 150, 38), 'attack': (214, 38, 24), 'own': (238, 230, 208), 'allyring': (175, 195, 225)}


def worst(arrows):
    for a in arrows:
        if a['kind'] == 'attack' and not a.get('ally'):
            return a
    for a in arrows:
        if a['kind'] == 'move' and not a.get('away'):
            return a
    return arrows[0] if arrows else None


def ping_ink(a):
    if a.get('ally'):
        return INKS['allyring']
    return {'attack': INKS['attack'], 'fell': INKS['own']}.get(a['kind'], INKS['war'] if a.get('war', True) else INKS['peace'])


def ping_look(state, seconds, order):
    if state == 0:
        return dict(dot=2.0, dot_a=0.45, ring=0.0, ring_a=0.0, thick=0.0, brass=False)
    if state == 1:
        return dict(dot=3.0, dot_a=1.0, ring=7.5, ring_a=1.0, thick=2.0, brass=True)
    phase = math.fmod(seconds - 0.45 * max(order, 0), PING_SECONDS)
    phase = (phase + PING_SECONDS if phase < 0 else phase) / PING_SECONDS
    rest = 1.0 - phase
    return dict(dot=2.6, dot_a=1.0, ring=3.0 + 9.0 * (1.0 - rest * rest), ring_a=0.95 * rest * math.sqrt(rest), thick=1.6, brass=False)


def corner_map(img, stops, now, ctrl, regions, nations, map_w, map_h):
    """The small map in the corner, 232 high as on the table, in its brass bezel, with the watch's stops pinged."""
    high = 232
    wide = int(round(high * map_w / map_h))
    k = 3  # drawn larger, then made smaller
    inset = Image.new('RGB', (wide * k, high * k), SEA)
    d = ImageDraw.Draw(inset)
    sx, sy = wide * k / map_w, high * k / map_h
    for r in regions:
        holder = ctrl[r['id']] if r['id'] < len(ctrl) else r['owner']
        colour = mix(PAPER, hexrgb(nations.get(holder, {}).get('color', '#888888')), 0.55)
        for poly in r['polys']:
            for ring in poly[:1]:
                d.polygon([(x * sx, y * sy) for x, y in ring], fill=colour)
    inset = inset.resize((wide, high), Image.LANCZOS).convert('RGBA')
    over = Image.new('RGBA', (wide * k, high * k), (0, 0, 0, 0))
    o = ImageDraw.Draw(over)
    for order, (place, ink) in enumerate(stops):
        if place is None:
            continue
        state = 0 if order < now else 1 if order == now else 2
        look = ping_look(state, 1.0, order)  # (caught mid-swell)
        cx, cy = regions[place]['x'] * sx, regions[place]['y'] * sy
        def circle(radius, width, colour):
            o.ellipse([cx - radius * k, cy - radius * k, cx + radius * k, cy + radius * k], outline=colour, width=max(1, int(round(width * k))))
        if look['ring_a'] > 0.01:
            circle(look['ring'], look['thick'] + 1.6, (8, 5, 4, int(255 * 0.7 * look['ring_a'])))
            circle(look['ring'], look['thick'], (*(BRASS if look['brass'] else ink), int(255 * look['ring_a'])))
        r0 = look['dot'] + 0.8
        o.ellipse([cx - r0 * k, cy - r0 * k, cx + r0 * k, cy + r0 * k], fill=(8, 5, 4, int(255 * 0.75 * look['dot_a'])))
        r1 = look['dot']
        o.ellipse([cx - r1 * k, cy - r1 * k, cx + r1 * k, cy + r1 * k], fill=(*ink, int(255 * look['dot_a'])))
    inset = Image.alpha_composite(inset, over.resize((wide, high), Image.LANCZOS))
    # the bezel: dark panel, brass rim
    panel = Image.new('RGBA', (wide + 22, high + 22), (16, 13, 10, 235))
    pd = ImageDraw.Draw(panel)
    pd.rectangle([6, 6, wide + 15, high + 15], outline=(176, 141, 87, 255), width=4)
    panel.paste(inset, (11, 11))
    x0, y0 = W - wide - 22 - 18, H - high - 22 - 18
    base = img.convert('RGBA')
    base.alpha_composite(panel, (x0, y0))
    return base.convert('RGB')


def picture(step, count, shown, regions, nations, path, stops=None, map_size=(1491, 1712)):
    arrows = step['arrows']
    pts = []
    for a in arrows:
        pts.append(regions[a['to']])
        if a['from'] >= 0:
            pts.append(regions[a['from']])
    xs = [p['x'] for p in pts]
    ys = [p['y'] for p in pts]
    cx, cy = (min(xs) + max(xs)) / 2, (min(ys) + max(ys)) / 2
    span = max(max(xs) - min(xs) + 150, (max(ys) - min(ys) + 190) * W / H, 330)
    scale = W / span
    cy += 22 / scale * 2  # (room for the plate at the bottom)

    def px(x, y):
        return ((x - cx) * scale + W / 2) * SS, ((y - cy) * scale + H / 2) * SS

    img = Image.new('RGB', (W * SS, H * SS), SEA)
    d = ImageDraw.Draw(img)
    ctrl = step['ctrl']
    for r in regions:
        holder = ctrl[r['id']] if r['id'] < len(ctrl) else r['owner']
        colour = hexrgb(nations.get(holder, {}).get('color', '#888888'))
        fill = mix(PAPER, colour, 0.42)
        for poly in r['polys']:
            for ring in poly[:1]:
                d.polygon([px(x, y) for x, y in ring], fill=fill)
    for r in regions:
        for poly in r['polys']:
            for ring in poly[:1]:
                d.line([px(x, y) for x, y in ring] + [px(*ring[0])], fill=LINE, width=int(1.4 * SS))
    # a soft dark edge round the land, as a table's light falls
    img = img.filter(ImageFilter.GaussianBlur(0.6 * SS))
    d = ImageDraw.Draw(img)

    # the arrows and marks: the game's own drawing, the dark edges first, then the inks
    drawing = lay(arrows, regions)
    for layer in ('under', 'over'):
        over = Image.new('RGBA', img.size, (0, 0, 0, 0))
        o = ImageDraw.Draw(over)
        for s in drawing[layer]['strokes']:
            line = [px(x, y) for x, y in s['points']]
            if s['closed']:
                line.append(line[0])
            if len(line) < 2:
                continue
            o.line(line, fill=tuple(s['ink']), width=max(1, int(round(2 * s['half'] * scale * SS))), joint='curve')
        for t in drawing[layer]['triangles']:
            o.polygon([px(x, y) for x, y in t['points']], fill=tuple(t['ink']))
        img = Image.alpha_composite(img.convert('RGBA'), over).convert('RGB')
    d = ImageDraw.Draw(img)

    # the armies as the step leaves them (over the arrows, as the pieces stand over them on the table): a small counter per nation and province, how many corps
    stacks = {}
    for c in step.get('corps', []):
        if c.get('reg') is None:
            continue
        stacks[(c['reg'], c['nat'])] = stacks.get((c['reg'], c['nat']), 0) + 1
    small = font(11 * SS, True)
    slot = {}
    for (reg, nat), n in sorted(stacks.items()):
        r = regions[reg]
        k = slot.get(reg, 0)
        slot[reg] = k + 1
        x, y = px(r['x'] + 9 + 13 * k, r['y'] - 10)
        colour = hexrgb(nations.get(nat, {}).get('color', '#888888'))
        s = 8 * SS
        d.rectangle([x - s, y - s * 0.75, x + s, y + s * 0.75], fill=mix(colour, (0, 0, 0), 0.15), outline=(20, 16, 12), width=SS)
        d.text((x, y), str(n), font=small, fill=CREAM, anchor='mm')
    # (round 2, the marked army) a red tag, swallow-tailed, lettered with the town it took, hung to the left of its counter
    tagged = set()
    tagfont = font(10 * SS, True)
    for c in step.get('corps', []):
        if not c.get('mark') or c.get('reg') is None or (c['reg'], c['nat']) in tagged:
            continue
        tagged.add((c['reg'], c['nat']))
        r = regions[c['reg']]
        k = sorted(n for (reg, n) in stacks if reg == c['reg']).index(c['nat']) if (c['reg'], c['nat']) in stacks else 0
        x, y = px(r['x'] + 9 + 13 * k, r['y'] - 10)
        word = c['mark'].upper()
        tw = d.textlength(word, font=tagfont) + 10 * SS
        th = 14 * SS
        right = x - 10 * SS
        left = right - tw
        top = y - 26 * SS
        tail = th * 0.45
        d.polygon([(right, top), (left, top), (left - tail, top), (left, top + th / 2), (left - tail, top + th), (right, top + th)], fill=(176, 30, 26), outline=(21, 19, 15))
        d.line([(right, top + th / 2), (x - 8 * SS, y - 6 * SS)], fill=(21, 19, 15), width=SS)
        d.text(((left + right) / 2, top + th / 2), word, font=tagfont, fill=(244, 240, 228), anchor='mm')

    # the names of the places the arrows reach
    label = font(15 * SS)
    named = []
    for a in arrows:
        if a['to'] not in named:
            named.append(a['to'])
    for reg in named:
        r = regions[reg]
        x, y = px(r['x'], r['y'] + 15)
        d.text((x, y), r['name'], font=label, fill=CREAM, anchor='mt', stroke_width=3 * SS // 2, stroke_fill=(24, 18, 12))

    img = img.resize((W, H), Image.LANCZOS)
    d = ImageDraw.Draw(img, 'RGBA')

    # the plate: whose month, how far through them, and what happened, in his side's words
    note = ' '.join(x for x in (step.get('nearNote', ''), step.get('allyNote', '')) if x) or step['note']
    body = font(21)
    lines = textwrap.wrap(note, 86)
    head = f"MOVES NEAR YOU  ·  {nations.get(step['nat'], {}).get('name', step['nat']).upper()}  ·  {shown} OF {count}"
    hf = font(14, True)
    pw = min(W - 120, max([d.textlength(l, font=body) for l in lines] + [d.textlength(head, font=hf) + 30]) + 70)
    ph = 54 + 30 * len(lines) + 40  # (round 2: TAKE ME THERE under the sentence)
    x0, y0 = (W - pw) / 2, H - ph - 34
    d.rounded_rectangle([x0, y0, x0 + pw, y0 + ph], radius=6, fill=(16, 13, 10, 222), outline=(224, 184, 114, 120), width=1)
    hw = d.textlength(head, font=hf) + 30
    hx = (W - hw) / 2
    colour = hexrgb(nations.get(step['nat'], {}).get('color', '#888888'))
    d.rectangle([hx, y0 + 15, hx + 21, y0 + 29], fill=colour, outline=(20, 16, 12))
    d.text((hx + 30, y0 + 22), head, font=hf, fill=BRASS, anchor='lm')
    for i, l in enumerate(lines):
        d.text((W / 2, y0 + 46 + 30 * i), l, font=body, fill=CREAM, anchor='mt')
    bf = font(13, True)
    button = 'TAKE ME THERE  \u00b7  G'
    bw = d.textlength(button, font=bf) + 34
    by = y0 + 52 + 30 * len(lines)
    d.rounded_rectangle([(W - bw) / 2, by, (W + bw) / 2, by + 26], radius=4, fill=(40, 32, 22, 240), outline=(224, 184, 114, 200), width=1)
    d.text((W / 2, by + 13), button, font=bf, fill=BRASS, anchor='mm')
    d.text((W - 14, 12), f"{step.get('date', '')}  ·  flat preview of the table", font=font(13), fill=(238, 230, 208, 170), anchor='ra')
    if stops:
        img = corner_map(img, stops, shown - 1, step['ctrl'], regions, nations, *map_size)
    img.save(path)


def main():
    steps = json.load(open(sys.argv[1]))
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    whole = json.load(open(MAP))
    regions = whole['regions']
    nations = {k: v for k, v in json.load(open(NATIONS)).items() if isinstance(v, dict)}
    for want in sys.argv[3:]:
        human, month, nat = want.split(':')
        month = int(month)
        same = [s for s in steps if s['human'] == human and s['month'] == month and (s.get('arrows') or not s.get('near') is None)]
        same = [s for s in same if s.get('arrows')]
        step = next(s for s in same if s['nat'] == nat)
        stops = []
        for s in same:
            a = worst(s['arrows'])
            stops.append((a['to'], ping_ink(a)) if a else (None, None))
        name = f"enemy-moves-{human.lower()}-month{month}-{nat.lower()}.png"
        picture(step, len(same), same.index(step) + 1, regions, nations, os.path.join(out, name), stops, (whole['width'], whole['height']))
        print(name)


if __name__ == '__main__':
    main()
