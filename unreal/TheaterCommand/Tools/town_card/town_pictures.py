"""
The town card's pictures (release 2, the UI director; Brian, 24 November 2026: "lets improve these graphics and UI.
its very underwhelming"). Painted offline with Tools/land_kit/card_picture.cpp from the ground battles' own houses
(LandHouse, the Norman set), as the art director passed them on 24 November (round 2), with the booth notes: no flag
cloth is baked (the game lays the province owner's own flag on each pole), and the aid post's board is on its wall.

What it makes, in Content/TownCard/:
  street-<set>.jpg               the town's street, 1824 x 568, every plot at its resting look
  spot-<set>-<i>-<state>.png     one plot as it stands: only the pixels that differ from the street, on a clear ground
  big-<state>.jpg, row-<state>.jpg   each building alone: 16:9 (992 x 558) and 4:3 (160 x 120), made for their shape
  town-card.json                 where each plot is in the street (its ground, its tag), each piece's place, each pole

Sets: 'town' (the Caen street, two plots: a village has the first, a town both) and 'city' (the same street seen
wider, four plots: a city has three, the capital four). States: lot, going, market, farm, school, camp, hospital,
hall (and house, a plot that is not the town's). Every level of a line shows the line's building for now.

    python3 -I town_pictures.py <painter dir with card_picture and Content> <out dir>
"""
import json, math, os, subprocess, sys
from concurrent.futures import ThreadPoolExecutor

import numpy as np
from PIL import Image, ImageFilter

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import cards2  # the round 2 scenes (backdrops, the street's houses and trees)

PAINTER = sys.argv[1] if len(sys.argv) > 1 else cards2.UE
OUT = sys.argv[2] if len(sys.argv) > 2 else os.path.join(HERE, 'out', 'towncard')
RAW = os.path.join(OUT, 'raw')
os.makedirs(RAW, exist_ok=True)
j = cards2.j

STATES = ['lot', 'going', 'market', 'farm', 'school', 'camp', 'hospital', 'hall']

# Screen order, left to right (x = -xs). A and B are the round 2 street's two plots; C and D replace two houses.
SPOT_XS = [-26.0, 35.0, -52.0, 62.0]
HOUSE_AT = {2: ('h', -52, 0, 6, 13.6, 7.0, 2, 0, 5, 0), 3: ('h', 62, 0, 6, 12.4, 6.8, 2, 0, 3, 0)}


def spot_items(i, state):
    """One plot's things, at the spot's place in the street (world x)."""
    x = -SPOT_XS[i]
    if state == 'house':
        it = list(HOUSE_AT.get(i, ('h', SPOT_XS[i], 0.3, 6, 11.4, 6.6, 2, 1, 11, 0)))
        return [j(it[0], -it[1], *it[2:])]
    if state == 'lot':
        return [j('p', x, 0, 12, 8)]
    if state == 'going':
        return [j('P', x, 0, 12, 8)]
    if state == 'market':
        return [j('m', x, 0, 13.0, 8.2)]
    if state == 'farm':
        return [j('h', x, -1, 0, 12.6, 7.2, 2, 0, 7, 0), j('h', x + 4, -12, 1, 12, 8.0, 1, 0, 3, 0), j('c', x - 4.5, 6.5)]
    if state == 'school':
        return [j('h', x, 0, 6, 13.0, 7.2, 1, 0, 5, 0, 2)]
    if state == 'camp':
        return [j('g', x - 6.5, -3.5, x + 6.5, 5.0, 1), j('F', x + 4.5, 3.2, 8.5), j('h', x, -8, 6, 12.0, 6.0, 1, 0, 4, 0)]
    if state == 'hospital':
        return [j('h', x, 0, 6, 9.4, 6.4, 1, 1, 12, 0, 3)]
    if state == 'hall':
        return [j('h', x, 0, 0, 12.8, 7.2, 2, 1, 2, 0, 1)]
    raise ValueError(state)


def street_items(spots, wide=False):
    """The round 2 street without its plots and the two houses that become plots C and D, then each spot's things.
    Seen wide, the near side's walls, hedges and trees would stand in front of plots C and D: they go, and two trees
    frame the far ends instead."""
    base = cards2.street().split(';')
    drop = {j('p', 26, 0, 12, 8), j('p', -35, 0, 12, 8)}
    for i, it in HOUSE_AT.items():
        drop.add(j(it[0], -it[1], *it[2:]))
    keep = [s for s in base if s not in drop]
    assert len(base) - len(keep) == 4, (len(base), len(keep))
    if wide:
        near = lambda q: (q[0] in 'te' and float(q[2]) >= 15.0) or (q[0] == 'w' and abs(float(q[2]) - 13.2) < 0.01)
        n0 = len(keep)
        keep = [s for s in keep if not near(s.split(','))]
        assert n0 - len(keep) == 8, n0 - len(keep)
        keep += [j('t', 96, 19, 12, 3.8, 95), j('t', -98, 19, 12, 3.8, 96)]
        # (the near side, low enough to leave every plot's front in sight from the wide eye: the street's packed
        # road before the houses, garden walls of 0.8 m with gaps, two carts on the road)
        keep += [j('g', -110, 5.4, 110, 10.2, 1)]
        for x0, x1 in [(108, 70), (64, 40), (-20, -48), (-54, -84), (-90, -112)]:
            keep.append(j('w', x0, 14.0, x1, 14.0, 0.8))
        keep += [j('c', 40, 8.2), j('c', -48, 7.8)]
        # (and nearer the eye, filling the foot of the picture without hiding a plot: a lane, its hedges, a cart,
        # two trees at the very edges)
        keep += [j('g', -110, 27.0, 110, 31.5, 1), j('e', 110, 35.5, 24, 35.5, 1.1), j('e', -14, 35.5, -110, 35.5, 1.1),
                 j('c', 14, 29.5), j('t', 34, 44, 12, 4.0, 97), j('t', -47, 45, 13, 4.2, 98)]
    for i, state in enumerate(spots):
        keep += spot_items(i, state)
    return ';'.join(keep)


SETS = {
    # the street the art director passed (two plots), and the same street seen wider (four plots)
    'town': dict(aim=(3, 0, 12.0, -4, -2.6, 64), plots=2, rest=['lot', 'lot', 'house', 'house']),
    'city': dict(aim=(-2, 0, 13.0, -4, -2.6, 86), plots=4, rest=['lot', 'lot', 'lot', 'lot']),
}
STREET_ENV = {'CARD_FOV': '30', 'CARD_LANE_Y': '8.5', 'CARD_HAZE': '380', 'CARD_SUN_UP': '12', 'CARD_SKY': '0.16', 'CARD_FLAG_CLOTH': '0'}
SW, SH = 1824, 568


def render(name, items, aim, sun, w, h, extra=None):
    env = dict(os.environ, CARD_W=str(w), CARD_H=str(h), CARD_SUN_UP='13', CARD_SKY='0.14', CARD_LANE_Y='7.5', CARD_HAZE='240',
               CARD_STREET=items, CARD_AIM=','.join(str(v) for v in aim), CARD_SUN_BEARING=str(sun), CARD_OUT=name,
               LAND_KIT_CAMERA='1', CARD_FLAG_CLOTH='0')
    if extra:
        env.update(extra)
    r = subprocess.run(['./card_picture', 'Content', RAW], cwd=PAINTER, env=env, capture_output=True, text=True)
    ok = os.path.exists(os.path.join(RAW, name + '.ppm'))
    return name, ok, (r.stdout.strip().splitlines() or [r.stderr[-200:]])[-1]


def camera(name):
    lines = open(os.path.join(RAW, name + '.ppm.camera')).read().split('\n')
    c = [float(v) for v in lines[0].split()]
    poles = [[float(v) for v in l.split()[1:]] for l in lines[1:] if l.startswith('flag')]
    return np.array(c[:3]), np.array(c[3:6]), poles


def projector(name, w, h, fov_deg):
    cam, tgt, poles = camera(name)
    fwd = (tgt - cam) / np.linalg.norm(tgt - cam)
    right = np.cross(fwd, [0, 0, 1.0]); right /= np.linalg.norm(right)
    up = np.cross(right, fwd)
    F = (h / 2) / math.tan(math.radians(fov_deg) / 2)

    def p(x, y, z):
        d = np.array([x, y, z]) - cam
        zf = d.dot(fwd)
        return (w / 2 + F * d.dot(right) / zf, h / 2 - F * d.dot(up) / zf, F / zf)
    return p, poles


def pole_marks(p, poles):
    out = []
    for x, y, hgt, sg in poles:
        px, py, ppm = p(x, y, hgt + 0.18)
        out.append([round(px, 1), round(py, 1), round(ppm, 2), int(sg)])
    return out


def load(name):
    return np.asarray(Image.open(os.path.join(RAW, name + '.ppm')).convert('RGB')).astype(np.int16)


def piece(base, var):
    """The pixels of var that differ from base, softened at their edge, cut to their box."""
    d = np.abs(var - base).max(axis=2)
    # (only the plot's own run of columns: a far shadow cast on another plot's ground would be laid over that plot's
    # building when both stand, so what lies apart from the main run, past a gap of a dozen clear columns, is left out)
    cols = (d > 5).sum(axis=0)
    runs, start, gap = [], None, 0
    for x, c in enumerate(cols):
        if c > 0:
            if start is None:
                start = x
            gap, end = 0, x + 1
        elif start is not None:
            gap += 1
            if gap > 12:
                runs.append((start, end)); start = None
    if start is not None:
        runs.append((start, end))
    if runs:
        a0, a1 = max(runs, key=lambda r: cols[r[0]:r[1]].sum())
        d = d.copy(); d[:, :a0] = 0; d[:, a1:] = 0
    m = Image.fromarray((d > 5).astype(np.uint8) * 255)
    m = m.filter(ImageFilter.MaxFilter(5)).filter(ImageFilter.GaussianBlur(1.2))
    a = np.asarray(m)
    ys, xs = np.nonzero(a > 2)
    if len(xs) == 0:
        return None
    x0, x1, y0, y1 = xs.min(), xs.max() + 1, ys.min(), ys.max() + 1
    rgba = np.dstack([var[y0:y1, x0:x1].astype(np.uint8), a[y0:y1, x0:x1].astype(np.uint8)])
    return Image.fromarray(rgba, 'RGBA'), [int(x0), int(y0), int(x1 - x0), int(y1 - y0)], float((d > 5).mean())


def main():
    jobs = []
    for s, cfg in SETS.items():
        rest = cfg['rest']
        wide = s == 'city'
        jobs.append((f'street-{s}', street_items(rest, wide), cfg['aim'], -58, SW, SH, STREET_ENV))
        for i in range(cfg['plots']):
            for state in STATES + ['house']:
                # (a house where a smaller place has no plot: the town street's second, the city street's fourth)
                if state == rest[i] or (state == 'house' and i != (3 if wide else 1)):
                    continue
                spots = list(rest); spots[i] = state
                jobs.append((f'spot-{s}-{i}-{state}', street_items(spots, wide), cfg['aim'], -58, SW, SH, STREET_ENV))
    card_of = {'lot': 'plot', 'going': 'going', 'market': 'market', 'farm': 'farms', 'school': 'school', 'camp': 'drill', 'hospital': 'aid', 'hall': 'townhall'}
    for state, card in card_of.items():
        c = cards2.CARDS[card]
        a = list(c['aim'])
        jobs.append((f'big-{state}', ';'.join(c['items']), tuple(a), c['sun'], 992, 558, None))
        a4 = list(a); a4[5] = a[5] * 1.22
        jobs.append((f'row-{state}', ';'.join(c['items']), tuple(a4), c['sun'], 400, 300, None))
    only = os.environ.get('ONLY')
    if only:
        jobs = [q for q in jobs if any(q[0].startswith(o) for o in only.split(','))]
    with ThreadPoolExecutor(4) as ex:
        for name, ok, line in ex.map(lambda q: render(*q), jobs):
            print(('ok  ' if ok else 'FAIL') + ' ' + name + ' ' + line, flush=True)


def pack():
    """From the raw renders: the pictures and the map of where each piece goes."""
    meta = {'note': 'Made by Tools/town_card/town_pictures.py; pixels of the street picture, 1824 x 568.', 'streets': {}, 'pictures': {}}
    for s, cfg in SETS.items():
        base = load(f'street-{s}')
        Image.fromarray(base.astype(np.uint8)).save(os.path.join(OUT, f'street-{s}.jpg'), quality=90, optimize=True)
        p, _ = projector(f'street-{s}', SW, SH, 30.0)
        spots = []
        for i in range(cfg['plots']):
            x = -SPOT_XS[i]
            ground = [p(x + dx, dy, 0.0)[:2] for dx, dy in [(-6, -4), (6, -4), (6, 4), (-6, 4)]]
            foot = p(x, 4.0, 0.0)
            spots.append({'rest': cfg['rest'][i], 'ground': [[round(a, 1), round(b, 1)] for a, b in ground],
                          'tag': [round(foot[0], 1), round(foot[1], 1)], 'pieces': {}})
        for i in range(len(spots)):
            for state in STATES + ['house']:
                name = f'spot-{s}-{i}-{state}'
                if not os.path.exists(os.path.join(RAW, name + '.ppm')):
                    continue
                got = piece(base, load(name))
                if not got:
                    print('no change', name)
                    continue
                im, box, share = got
                im.save(os.path.join(OUT, name + '.png'), optimize=True)
                pp, poles = projector(name, SW, SH, 30.0)
                spots[i]['pieces'][state] = {'box': box, 'flags': pole_marks(pp, poles)}
                print(name, box, f'{share * 100:.1f}% changed')
        meta['streets'][s] = {'size': [SW, SH], 'spots': spots}
    for state in STATES:
        for kind, (w, h), out in [('big', (992, 558), (992, 558)), ('row', (400, 300), (160, 120))]:
            name = f'{kind}-{state}'
            im = Image.fromarray(load(name).astype(np.uint8))
            pp, poles = projector(name, w, h, 28.0)
            k = out[0] / w
            if out != (w, h):
                im = im.resize(out, Image.LANCZOS)
            im.save(os.path.join(OUT, name + '.jpg'), quality=90, optimize=True)
            meta['pictures'][name] = {'size': list(out), 'flags': [[round(a * k, 1), round(b * k, 1), round(c * k, 2), sg] for a, b, c, sg in pole_marks(pp, poles)]}
    with open(os.path.join(OUT, 'town-card.json'), 'w') as f:
        json.dump(meta, f, indent=1)
    print('packed', OUT)


if __name__ == '__main__':
    if os.environ.get('PACK'):
        pack()
    else:
        main()
