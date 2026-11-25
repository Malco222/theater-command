#!/usr/bin/env python3
"""
A fleet under way on the campaign map (release 2): a flat drawing of how a fleet sails today beside how she sails with
the ship's course, made in the cloud from the scenes Tools/ship_way/run.sh checked (its scenes.json).

The right-hand column is the game's own course and times (Campaign/ShipWay.h, as the test ran them). The left-hand
column is ships that sail as the map moves them today (AArmyPiece::SailAlong and Tick, re-told here: the way's own
corners, a piece's ease along it, her head turned toward each leg at five a second and back to the table over the last
sixth). Each ghost is her place a third of a second on from the last; the white line is her wake at one moment.

It is a drawing, not the 3D table: the Mac takes those pictures.

Usage: picture.py scenes.json out.jpg
"""
import json
import math
import sys

from PIL import Image, ImageDraw, ImageFont

SS = 2  # drawn at twice the size, then made smaller: smooth edges
SEA = (44, 66, 82)
SEA_LINE = (60, 86, 104)
LAND = (196, 182, 150)
LAND_EDGE = (120, 108, 84)
INK = (238, 232, 214)
SOFT = (170, 184, 192)
BRASS = (226, 186, 112)
RED = (214, 104, 84)
PANEL_W, PANEL_H = 760, 400
MARGIN, TOP, ROW_GAP, CAPTION = 40, 182, 96, 62
ROWS = [
    ('open sea, abeam', 'A short move to the next sea', 0.30),
    ('round a cape', 'Round a cape', 0.50),
    ('a long passage', 'A long voyage with three turns', 0.42),
]


def font(size, bold=False):
    name = 'DejaVuSans-Bold.ttf' if bold else 'DejaVuSans.ttf'
    try:
        return ImageFont.truetype('/usr/share/fonts/truetype/dejavu/' + name, size * SS)
    except OSError:
        return ImageFont.load_default()


def today(scene):
    """Ships that sail, as the map moves a fleet today: (x, y, hx, hy) sixty times a second, and her time."""
    way = scene['way']
    marks = [0.0]
    for a, b in zip(way, way[1:]):
        marks.append(marks[-1] + math.hypot(b[0] - a[0], b[1] - a[1]))
    long = marks[-1]
    time = min(max(1.1 + long / 380.0, 1.4), 3.6)
    table = math.atan2(scene['rest'][1], scene['rest'][0])
    yaw = math.atan2(scene['bows'][1], scene['bows'][0])
    out = []
    frames = int(math.ceil(time * 60.0))
    for frame in range(frames + 1):
        progress = min(frame / 60.0 / time, 1.0)
        eased = progress * progress * (3.0 - 2.0 * progress)
        target = eased * long
        k = 1
        while k < len(marks) - 1 and marks[k] < target:
            k += 1
        span = max(marks[k] - marks[k - 1], 1.0e-6)
        f = (target - marks[k - 1]) / span
        x = way[k - 1][0] + (way[k][0] - way[k - 1][0]) * f
        y = way[k - 1][1] + (way[k][1] - way[k - 1][1]) * f
        want = math.atan2(way[k][1] - way[k - 1][1], way[k][0] - way[k - 1][0]) if progress < 0.84 else table
        rate = 5.0 if progress < 0.84 else max(5.0, 3.0 / max(0.05, time * (1.0 - progress)))
        turn = (want - yaw + math.pi) % (2.0 * math.pi) - math.pi
        yaw += turn * min(1.0, rate / 60.0)
        out.append((x, y, math.cos(yaw), math.sin(yaw)))
    return out, time


def with_course(scene):
    """The ship's course as the game times it: (x, y, hx, hy) at each tick, and her time."""
    return [(p[0], p[1], p[2], p[3]) for p in scene['at']], scene['seconds']


def at_share(track, share):
    return track[min(int(share * (len(track) - 1)), len(track) - 1)]


def wake_today(track, share, reach):
    """A stiff tail: straight astern of her keel, however she has turned."""
    x, y, hx, hy = at_share(track, share)
    return [(x, y), (x - hx * reach, y - hy * reach)]


def wake_course(track, share, reach):
    """The water she has crossed: back along her own track, and before her start straight astern of how she lay."""
    upto = min(int(share * (len(track) - 1)), len(track) - 1)
    out = [(track[upto][0], track[upto][1])]
    left = reach
    for k in range(upto, 0, -1):
        step = math.hypot(track[k][0] - track[k - 1][0], track[k][1] - track[k - 1][1])
        if step >= left:
            f = left / step if step > 0 else 0.0
            out.append((track[k][0] + (track[k - 1][0] - track[k][0]) * f, track[k][1] + (track[k - 1][1] - track[k][1]) * f))
            return out
        left -= step
        out.append((track[k - 1][0], track[k - 1][1]))
    out.append((track[0][0] - track[0][2] * left, track[0][1] - track[0][3] * left))
    return out


def hull(draw, to_px, x, y, hx, hy, long, fill, outline):
    """One ship, seen from above: a pointed bow, a square stern."""
    beam = long * 0.16
    px, py = -hy, hx
    points = [
        (x + hx * long * 0.5, y + hy * long * 0.5),
        (x + hx * long * 0.2 + px * beam, y + hy * long * 0.2 + py * beam),
        (x - hx * long * 0.5 + px * beam * 0.8, y - hy * long * 0.5 + py * beam * 0.8),
        (x - hx * long * 0.5 - px * beam * 0.8, y - hy * long * 0.5 - py * beam * 0.8),
        (x + hx * long * 0.2 - px * beam, y + hy * long * 0.2 - py * beam),
    ]
    draw.polygon([to_px(p[0], p[1]) for p in points], fill=fill, outline=outline)


def panel(image, draw, left, top, scene, track, time, share, new):
    box = (left * SS, top * SS, (left + PANEL_W) * SS, (top + PANEL_H) * SS)
    draw.rectangle(box, fill=SEA, outline=SEA_LINE, width=SS)
    xs = [p[0] for p in scene['course']] + [p[0] for p in scene['way']]
    ys = [p[1] for p in scene['course']] + [p[1] for p in scene['way']]
    for isle in scene['isles']:
        xs += [isle[0] - isle[2] * 0.6, isle[0] + isle[2] * 0.6]
        ys += [isle[1] - isle[2] * 0.6, isle[1] + isle[2] * 0.6]
    pad = 0.16 * max(max(xs) - min(xs), max(ys) - min(ys), 60.0)
    x0, x1, y0, y1 = min(xs) - pad, max(xs) + pad, min(ys) - pad, max(ys) + pad
    scale = min(PANEL_W / (x1 - x0), PANEL_H / (y1 - y0))
    mid_x, mid_y = 0.5 * (x0 + x1), 0.5 * (y0 + y1)

    def to_px(x, y):
        # (the test's north is up the page)
        return ((left + PANEL_W * 0.5 + (x - mid_x) * scale) * SS, (top + PANEL_H * 0.5 - (y - mid_y) * scale) * SS)

    clip = Image.new('RGB', image.size, SEA)
    layer = ImageDraw.Draw(clip)
    layer.rectangle(box, fill=SEA)
    for isle in scene['isles']:
        cx, cy = to_px(isle[0], isle[1])
        r = isle[2] * scale * SS
        layer.ellipse((cx - r, cy - r, cx + r, cy + r), fill=LAND, outline=LAND_EDGE, width=SS)
    # her track, faint; then her ghosts a third of a second apart; then one moment of her, with her wake
    layer.line([to_px(p[0], p[1]) for p in track], fill=SEA_LINE, width=2 * SS, joint='curve')
    long = max(9.0, 26.0 / scale)  # (a fleet's piece is about this long on the map; never too small to read)
    every = 20 if not new else max(1, int(round((1.0 / 3.0) / 0.02)))
    for k in range(0, len(track), every):
        x, y, hx, hy = track[k]
        hull(layer, to_px, x, y, hx, hy, long, (96, 116, 130), (132, 150, 162))
    wake = (wake_course if new else wake_today)(track, share, 70.0)
    layer.line([to_px(p[0], p[1]) for p in wake], fill=INK, width=3 * SS, joint='curve')
    x, y, hx, hy = at_share(track, share)
    hull(layer, to_px, x, y, hx, hy, long, BRASS if new else RED, INK)
    for point in (scene['way'][0], scene['way'][-1]):
        cx, cy = to_px(point[0], point[1])
        layer.ellipse((cx - 4 * SS, cy - 4 * SS, cx + 4 * SS, cy + 4 * SS), outline=INK, width=SS)
    image.paste(clip.crop(box), box[:2])
    draw.rectangle(box, outline=SEA_LINE, width=SS)
    draw.text(((left + 12) * SS, (top + 10) * SS), 'WITH THE UPGRADE' if new else 'TODAY', font=font(15, True), fill=BRASS if new else RED)
    draw.text(((left + PANEL_W - 12) * SS, (top + 10) * SS), '%.1f seconds' % time, font=font(15), fill=INK, anchor='ra')


def main():
    scenes = {scene['name']: scene for scene in json.load(open(sys.argv[1]))['scenes']}
    width = MARGIN * 3 + PANEL_W * 2
    height = TOP + len(ROWS) * (PANEL_H + CAPTION + ROW_GAP) - ROW_GAP + 120
    image = Image.new('RGB', (width * SS, height * SS), (24, 32, 40))
    draw = ImageDraw.Draw(image)
    draw.text((MARGIN * SS, 28 * SS), 'A fleet ordered to another sea: today, and with upgrade 1', font=font(30, True), fill=INK)
    draw.text((MARGIN * SS, 74 * SS), 'A drawing made from the course\'s own numbers, seen from straight above. It is not a picture from the game: the Mac films that next.', font=font(15), fill=SOFT)
    draw.text((MARGIN * SS, 100 * SS), 'Each grey ship is her place a third of a second after the last: bunched together she is slow, spread out she is fast. The white line is her wake at one moment.', font=font(15), fill=SOFT)
    words = [
        ('She spins on the spot, slides corner to corner like a counter pushed by hand, and spins back. Her wake swings behind her like a stiff tail.',
         'She swings out on her own turning circle, gathers way, and comes round on a circle to rest. Her wake lies on the water she has crossed.'),
        ('She turns at a point off the cape, at full speed, and her head snaps round with her.',
         'She sweeps wide round the cape on one long curve, a little to seaward, slowing only as much as her helm needs.'),
        ('Three sharp corners, each taken in an instant, at one even glide from start to finish.',
         'Slow off the mark, fast on the open legs, easing through each wide turn, and carrying her way in before she comes round to rest.'),
    ]
    for row, (name, title, share) in enumerate(ROWS):
        scene = scenes[name]
        top = TOP + row * (PANEL_H + CAPTION + ROW_GAP)
        draw.text((MARGIN * SS, (top - 34) * SS), title, font=font(20, True), fill=INK)
        for column, new in enumerate((False, True)):
            track, time = with_course(scene) if new else today(scene)
            left = MARGIN + column * (PANEL_W + MARGIN)
            panel(image, draw, left, top, scene, track, time, share, new)
            # (the words under the panel, wrapped to its width)
            line, lines = '', []
            for word in words[row][column].split():
                trial = (line + ' ' + word).strip()
                if draw.textlength(trial, font=font(15)) > PANEL_W * SS:
                    lines.append(line)
                    line = word
                else:
                    line = trial
            lines.append(line)
            for k, text in enumerate(lines[:3]):
                draw.text((left * SS, (top + PANEL_H + 10 + k * 21) * SS), text, font=font(15), fill=SOFT)
    draw.text((MARGIN * SS, (height - 44) * SS), 'Still to come on this upgrade: each ship leaning into her turn and riding the swell. Off in the game until you have seen the Mac\'s film.', font=font(15), fill=SOFT)
    image = image.resize((width, height), Image.LANCZOS)
    image.save(sys.argv[2], quality=90)
    print('wrote', sys.argv[2], image.size)


if __name__ == '__main__':
    main()
