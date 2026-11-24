#!/usr/bin/env python3
"""
New units ready (release 2; Brian, 24 Nov 2026 18:30Z: "let me know when a unit has been delivered"): a flat preview of
the READY FOR ORDERS plate as his month opens, with SHOW ME pressed, and the brass NEW tag on the new army's counter.

The month is a real one from the 3D bridge (TC.endMonth's 'ready' list and its view); the map is the campaign map's
provinces, flat, as picture.py draws them. It is not the 3D table: the Mac takes those pictures (60g, 60h).

Usage: ready_picture.py dump.json out.png
  dump.json: [{date, ready: [{what, text, region, corps, name}], ctrl: [...], corps: [{id, reg, nat, ready, name}]}]
"""
import json
import os
import sys
import textwrap

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from picture import BRASS, CREAM, LINE, MAP, NATIONS, PAPER, SEA, SS, H, W, font, hexrgb, mix  # noqa: E402


def main():
    month = json.load(open(sys.argv[1]))[0]
    out = sys.argv[2]
    whole = json.load(open(MAP))
    regions = whole['regions']
    nations = {k: v for k, v in json.load(open(NATIONS)).items() if isinstance(v, dict)}
    ready = month['ready']
    first = ready[0]
    at = regions[first['region']]
    span = 420
    scale = W / span
    cx, cy = at['x'], at['y'] - 14  # (room for the plate at the top)

    def px(x, y):
        return ((x - cx) * scale + W / 2) * SS, ((y - cy) * scale + H / 2) * SS

    img = Image.new('RGB', (W * SS, H * SS), SEA)
    d = ImageDraw.Draw(img)
    ctrl = month['ctrl']
    for r in regions:
        holder = ctrl[r['id']] if r['id'] < len(ctrl) else r['owner']
        fill = mix(PAPER, hexrgb(nations.get(holder, {}).get('color', '#888888')), 0.42)
        for poly in r['polys']:
            for ring in poly[:1]:
                d.polygon([px(x, y) for x, y in ring], fill=fill)
    for r in regions:
        for poly in r['polys']:
            for ring in poly[:1]:
                d.line([px(x, y) for x, y in ring] + [px(*ring[0])], fill=LINE, width=int(1.4 * SS))
    img = img.filter(ImageFilter.GaussianBlur(0.6 * SS))
    d = ImageDraw.Draw(img)

    # The armies: a counter per nation and province, how many corps; the one in hand ringed in brass.
    stacks, fresh = {}, set()
    for c in month['corps']:
        stacks[(c['reg'], c['nat'])] = stacks.get((c['reg'], c['nat']), 0) + 1
        if c.get('ready'):
            fresh.add((c['reg'], c['nat']))
    small = font(11 * SS, True)
    slot, spot = {}, {}
    for (reg, nat), n in sorted(stacks.items()):
        r = regions[reg]
        k = slot.get(reg, 0)
        slot[reg] = k + 1
        x, y = px(r['x'] + 9 + 13 * k, r['y'] - 10)
        spot[(reg, nat)] = (x, y)
        colour = hexrgb(nations.get(nat, {}).get('color', '#888888'))
        s = 8 * SS
        if (reg, nat) in fresh:
            d.rounded_rectangle([x - s - 4 * SS, y - s * 0.75 - 4 * SS, x + s + 4 * SS, y + s * 0.75 + 4 * SS], radius=3 * SS, outline=BRASS, width=2 * SS)
        d.rectangle([x - s, y - s * 0.75, x + s, y + s * 0.75], fill=mix(colour, (0, 0, 0), 0.15), outline=(20, 16, 12), width=SS)
        d.text((x, y), str(n), font=small, fill=CREAM, anchor='mm')
    # The brass NEW tag, swallow-tailed, hung where an enemy's red mark would hang, lettered in near-black.
    tagfont = font(10 * SS, True)
    for key in fresh:
        x, y = spot[key]
        word = 'NEW'
        tw = d.textlength(word, font=tagfont) + 10 * SS
        th = 14 * SS
        right, top = x - 10 * SS, y - 26 * SS
        left, tail = right - tw, th * 0.45
        d.polygon([(right, top), (left, top), (left - tail, top), (left, top + th / 2), (left - tail, top + th), (right, top + th)], fill=(201, 162, 39), outline=(21, 19, 15))
        d.line([(right, top + th / 2), (x - 8 * SS, y - 6 * SS)], fill=(21, 19, 15), width=SS)
        d.text(((left + right) / 2, top + th / 2), word, font=tagfont, fill=(21, 19, 15), anchor='mm')
    label = font(15 * SS)
    x, y = px(at['x'], at['y'] + 15)
    d.text((x, y), at['name'], font=label, fill=CREAM, anchor='mt', stroke_width=3 * SS // 2, stroke_fill=(24, 18, 12))

    img = img.resize((W, H), Image.LANCZOS)
    d = ImageDraw.Draw(img, 'RGBA')
    # The plate, top middle where the month's notices stand: his flag, READY FOR ORDERS, the rules' own lines, SHOW ME.
    body = font(21)
    lines = []
    for unit in ready[:3]:
        lines += textwrap.wrap(unit['text'], 80)
    if len(ready) > 3:
        lines.append(f"And {len(ready) - 3} more: press R for the month's report.")
    head = 'READY FOR ORDERS' if len(ready) == 1 else f'READY FOR ORDERS  ·  1 OF {len(ready)}'
    hf = font(14, True)
    pw = min(W - 120, max([d.textlength(line, font=body) for line in lines] + [d.textlength(head, font=hf) + 30]) + 70)
    ph = 54 + 30 * len(lines) + 40
    x0, y0 = (W - pw) / 2, 74
    d.rounded_rectangle([x0, y0, x0 + pw, y0 + ph], radius=6, fill=(16, 13, 10, 222), outline=(224, 184, 114, 120), width=1)
    hw = d.textlength(head, font=hf) + 30
    hx = (W - hw) / 2
    human = month['corps'][0]['nat'] if not ready else next(c['nat'] for c in month['corps'] if c['id'] == first['corps'])
    d.rectangle([hx, y0 + 15, hx + 21, y0 + 29], fill=hexrgb(nations.get(human, {}).get('color', '#888888')), outline=(20, 16, 12))
    d.text((hx + 30, y0 + 22), head, font=hf, fill=BRASS, anchor='lm')
    for i, line in enumerate(lines):
        d.text((W / 2, y0 + 46 + 30 * i), line, font=body, fill=CREAM, anchor='mt')
    bf = font(13, True)
    button = 'SHOW ME  ·  G'
    bw = d.textlength(button, font=bf) + 34
    by = y0 + 52 + 30 * len(lines)
    d.rounded_rectangle([(W - bw) / 2, by, (W + bw) / 2, by + 26], radius=4, fill=(40, 32, 22, 240), outline=(224, 184, 114, 200), width=1)
    d.text((W / 2, by + 13), button, font=bf, fill=BRASS, anchor='mm')
    d.text((W - 14, 12), f"{month.get('date', '')}  ·  flat preview of the table", font=font(13), fill=(238, 230, 208, 170), anchor='ra')
    img.save(out)


if __name__ == '__main__':
    main()
