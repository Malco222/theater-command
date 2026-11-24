#!/usr/bin/env python3
"""
The enemy's month near you (release 2): a flat preview of one stop of the watch, drawn in the cloud.

The arrows and battle marks are the game's own (Campaign/EnemyMoves, laid by Tools/enemy_moves/run.sh); the map under
them is the campaign map's provinces, flat, tinted by who holds each when the step ends; the plate at the bottom is the
watch's heading and sentence as the table shows them. It is not the 3D table: the Mac takes those pictures.

Usage: picture.py near-steps.json out-dir HUMAN:MONTH:NAT [...]
  near-steps.json: the steps the watch stops at, each {human, month, nat, date, arrows, nearNote, note, ctrl, corps}.
"""
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


def picture(step, count, shown, regions, nations, path):
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
    ph = 54 + 30 * len(lines)
    x0, y0 = (W - pw) / 2, H - ph - 34
    d.rounded_rectangle([x0, y0, x0 + pw, y0 + ph], radius=6, fill=(16, 13, 10, 222), outline=(224, 184, 114, 120), width=1)
    hw = d.textlength(head, font=hf) + 30
    hx = (W - hw) / 2
    colour = hexrgb(nations.get(step['nat'], {}).get('color', '#888888'))
    d.rectangle([hx, y0 + 15, hx + 21, y0 + 29], fill=colour, outline=(20, 16, 12))
    d.text((hx + 30, y0 + 22), head, font=hf, fill=BRASS, anchor='lm')
    for i, l in enumerate(lines):
        d.text((W / 2, y0 + 46 + 30 * i), l, font=body, fill=CREAM, anchor='mt')
    d.text((W - 14, 12), f"{step.get('date', '')}  ·  flat preview of the table", font=font(13), fill=(238, 230, 208, 170), anchor='ra')
    img.save(path)


def main():
    steps = json.load(open(sys.argv[1]))
    out = sys.argv[2]
    os.makedirs(out, exist_ok=True)
    regions = json.load(open(MAP))['regions']
    nations = {k: v for k, v in json.load(open(NATIONS)).items() if isinstance(v, dict)}
    for want in sys.argv[3:]:
        human, month, nat = want.split(':')
        month = int(month)
        same = [s for s in steps if s['human'] == human and s['month'] == month]
        step = next(s for s in same if s['nat'] == nat)
        name = f"enemy-moves-{human.lower()}-month{month}-{nat.lower()}.png"
        picture(step, len(same), same.index(step) + 1, regions, nations, os.path.join(out, name))
        print(name)


if __name__ == '__main__':
    main()
