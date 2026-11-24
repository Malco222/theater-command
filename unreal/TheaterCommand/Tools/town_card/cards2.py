# Round 2 pictures for the province and town screens (art director's mends, 24 Nov).
# Builds each scene as CARD_STREET items and renders it big (16:9) and as a row picture (4:3), plus the street.
import os, subprocess, sys, math, json, random
from concurrent.futures import ThreadPoolExecutor

KIT = os.path.dirname(os.path.abspath(__file__))
UE = os.path.join(KIT, 'unreal', 'TheaterCommand')
OUT = os.path.join(KIT, 'out', 'r2')
os.makedirs(OUT, exist_ok=True)

def j(*a):
    return ','.join(str(round(v, 3)) if isinstance(v, float) else str(v) for v in a)

def backdrop(cx, near=-16.0, seed=1, wide=70.0):
    """A soft back row: a hedge, a line of trees, a roofline in the haze."""
    r = random.Random(seed)
    it = []
    it.append(j('e', cx - wide * 0.5, near, cx + wide * 0.6, near - 1.5, 2.2))
    x = cx - wide * 0.5
    k = 0
    while x < cx + wide * 0.65:
        it.append(j('t', x, near - 4 - r.random() * 5, 9 + r.random() * 5, 3.0 + r.random() * 1.6, 100 + k + seed * 31))
        x += 8 + r.random() * 6
        k += 1
    x = cx - wide * 0.45
    k = 0
    while x < cx + wide * 0.7:
        kind = r.choice([0, 6, 6, 1])
        lon = 11 + r.random() * 4
        it.append(j('h', x, near - 22 - r.random() * 14, kind, lon, 6.8 + r.random(), 2, r.choice([0, 0, 1]), 40 + k + seed * 7, 0))
        x += lon + 4 + r.random() * 8
        k += 1
    for q in range(5):
        it.append(j('t', cx - wide * 0.4 + q * wide * 0.27 + r.random() * 6, near - 48 - r.random() * 10, 12 + r.random() * 4, 4.0, 300 + q + seed))
    return it

CARDS = {
    'market': dict(items=[j('m', 0, 0, 13.6, 8.2), j('h', -15.5, -1, 6, 11.4, 6.6, 2, 1, 11, 0), j('c', 9.5, 7.5),
                          j('t', 12, -6, 11, 3.4, 7)] + backdrop(4, -14, 1),
                   aim=(0.5, 0, 3.8, -30, 7, 31), sun=30),
    'farms': dict(items=[j('h', -6, -2, 0, 14, 7.2, 2, 0, 7, 0), j('h', 12, -12, 1, 15, 8.4, 1, 0, 3, 0), j('h', 14, 4, 2, 13, 6.8, 1, 0, 4, 0),
                         j('t', -16, -8, 12, 4, 5), j('c', 4, 7)] + backdrop(4, -22, 2),
                  aim=(4, -3, 4.0, -34, 7, 48), sun=28),
    'school': dict(items=[j('h', 0, 0, 6, 20, 7.5, 1, 0, 5, 0, 2), j('t', -7, 7, 8, 2.4, 21), j('t', 13.5, -2, 11, 3.2, 22)] + backdrop(5, -14, 3),
                   aim=(1, 2.5, 2.6, -28, 8, 36), sun=32),
    'drill': dict(items=[j('g', -13, -4, 13, 11, 1), j('F', -9, 7, 9), j('h', 0, -10, 6, 24, 7, 1, 0, 4, 0), j('t', 17, -4, 11, 3.4, 31),
                         j('e', -22, -3, -22, 9, 1.8)] + backdrop(4, -20, 4),
                  aim=(0, 0, 2.4, -30, 10, 40), sun=32),
    'aid': dict(items=[j('h', 0, 0, 6, 9.4, 6.4, 1, 1, 12, 0, 3), j('h', -12, -1, 6, 10.4, 6.6, 2, 0, 15, 0), j('t', 10, -5, 10, 3.0, 41),
                       j('w', 5, 3.6, 5, 9, 1.2)] + backdrop(3, -13, 5),
                aim=(1.5, 1, 2.8, -30, 6, 25), sun=32),
    'townhall': dict(items=[j('h', 0, 0, 0, 14, 7.2, 2, 1, 2, 0, 1), j('g', -12, 3.7, 12, 12, 1), j('t', -12.5, 5.5, 8, 2.3, 51), j('t', 12.5, 5.5, 8, 2.3, 52),
                            j('h', -17, -1, 6, 11.4, 6.6, 2, 0, 3, 0)] + backdrop(4, -14, 6),
                     aim=(0, 1, 5.0, -28, 6, 36), sun=32),
    'plot': dict(items=[j('p', 0, 0, 12, 8), j('h', -14, -2, 6, 11.4, 6.6, 2, 1, 11, 0), j('h', 14, -1, 6, 12.4, 6.8, 2, 0, 3, 0),
                        j('w', -6, -4.5, 6, -4.5, 1.2)] + backdrop(2, -12, 7),
                 aim=(0, 0, 2.6, -18, 9, 30), sun=30),
    'going': dict(items=[j('P', 0, 0, 12, 8), j('h', -14, -2, 6, 11.4, 6.6, 2, 1, 11, 0), j('h', 14, -1, 6, 12.4, 6.8, 2, 0, 3, 0),
                         j('w', -6, -4.5, 6, -4.5, 1.2)] + backdrop(2, -12, 7),
                  aim=(0, 0, 2.6, -18, 9, 30), sun=30),
}

def street():
    # screen order, left to right (x = -xs)
    S = [
        ('h', -52, 0, 6, 13.6, 7.0, 2, 0, 5, 0),
        ('h', -39.5, 0.5, 6, 9.4, 6.4, 1, 1, 12, 0),
        ('p', -26, 0, 12, 8),
        ('h', -3, -3, 3, 18, 8.6, 1, 0, 7, 0),
        ('h', 21.5, 0, 6, 12.4, 6.8, 2, 0, 10, 0),
        ('p', 35, 0, 12, 8),
        ('h', 48, 0.3, 6, 11.4, 6.6, 2, 1, 11, 0),
        ('h', 62, 0, 6, 12.4, 6.8, 2, 0, 3, 0),
        ('h', 76, -1, 6, 11.0, 6.6, 2, 1, 33, 0),
        ('h', -66, -1, 0, 14, 7.2, 2, 0, 44, 0),
        # the back roofline
        ('h', -34, -24, 0, 14, 7.2, 2, 0, 7, 0),
        ('h', -18, -27, 6, 12, 6.8, 2, 1, 61, 0),
        ('h', 14, -34, 1, 15, 8.4, 1, 0, 3, 0),
        ('h', 31, -26, 6, 12.4, 6.8, 2, 1, 21, 0),
        ('h', 46, -30, 6, 11.0, 6.6, 2, 0, 62, 0),
        ('h', 62, -24, 0, 14, 7.2, 2, 1, 2, 0),
        ('h', -50, -28, 6, 11.4, 6.6, 2, 1, 63, 0),
        ('h', -84, -22, 6, 11.0, 6.6, 2, 1, 33, 0),
        ('h', 92, -26, 0, 14, 7.2, 2, 0, 64, 0),
        ('h', 2, -46, 6, 12, 6.8, 2, 0, 65, 0),
        ('h', -66, -44, 1, 14, 8, 1, 0, 66, 0),
        ('h', 70, -48, 6, 12, 6.8, 2, 1, 67, 0),
    ]
    out = []
    for it in S:
        t, xs, rest = it[0], it[1], list(it[2:])
        out.append(j(t, -xs, *rest))
    # trees in the square and the gardens
    for xs, y, h, r, k in [(-14, 2.5, 11, 3.0, 71), (9.5, 2.5, 11, 3.0, 72), (-46, -9, 10, 3.2, 73), (28, -8, 12, 3.4, 74),
                           (55, -10, 11, 3.2, 75), (-60, -11, 12, 3.6, 76), (84, -10, 11, 3.2, 77), (-28, -14, 12, 3.6, 78),
                           (40, -16, 12, 3.6, 79), (-8, -22, 13, 4.0, 80), (18, -52, 14, 4.5, 81), (-40, -56, 14, 4.5, 82),
                           (52, -58, 14, 4.5, 83), (-90, -50, 13, 4.2, 84), (100, -50, 13, 4.2, 85)]:
        out.append(j('t', -xs, y, h, r, k))
    # garden walls and the churchyard wall
    for xs0, y0, xs1, y1, h in [(-13, 4.6, -6.5, 4.6, 1.0), (-0.5, 4.6, 6.5, 4.6, 1.0), (-46, -3.5, -46, -9, 1.6), (28, -3.4, 28, -10, 1.6),
                                (55, -3.3, 55, -10, 1.6), (-60, -3.5, -60, -10, 1.6), (69.5, -3.4, 69.5, -10, 1.6)]:
        out.append(j('w', -xs0, y0, -xs1, y1, h))
    out.append(j('c', -8, 12.5))
    out.append(j('c', -52, 10.0))
    # the square before the church, and the near side of the street: garden walls, bushes, trees at the frame's edges
    out.append(j('g', -14, 5.0, 16, 24, 1))
    for xs0, xs1 in [(-75, -36), (44, 80)]:
        out.append(j('w', -xs0, 13.2, -xs1, 13.2, 1.3))
    for xs, y, h, r, k in [(-46, 17, 10, 3.4, 91), (-64, 18, 12, 3.8, 92), (52, 17, 10, 3.4, 93), (70, 18, 12, 3.8, 94)]:
        out.append(j('t', -xs, y, h, r, k))
    out.append(j('e', 75, 15.5, 36, 15.5, 1.6))
    out.append(j('e', -44, 15.5, -80, 15.5, 1.6))
    out.append(j('e', 110, -64, -110, -66, 2.4))
    return ';'.join(out)

def render(name, items, aim, sun, w, h, extra=None):
    env = dict(os.environ, CARD_W=str(w), CARD_H=str(h), CARD_SUN_UP='13', CARD_SKY='0.14', CARD_LANE_Y='7.5', CARD_HAZE='240',
               CARD_STREET=items if isinstance(items, str) else ';'.join(items), CARD_AIM=','.join(str(v) for v in aim),
               CARD_SUN_BEARING=str(sun), CARD_OUT=name, LAND_KIT_CAMERA='1')
    if extra:
        env.update(extra)
    r = subprocess.run(['./card_picture', 'Content', OUT], cwd=UE, env=env, capture_output=True, text=True)
    return name + ': ' + (r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[-300:])

if __name__ == '__main__':
    which = sys.argv[1:] or list(CARDS) + ['street']
    jobs = []
    for n in which:
        if n == 'street':
            jobs.append(('street', street(), (3, 0, 12.0, -4, -2.6, 64), -58, 1824, 568, {'CARD_FOV': '30', 'CARD_LANE_Y': '8.5', 'CARD_HAZE': '380', 'CARD_SUN_UP': '12', 'CARD_SKY': '0.16'}))
            continue
        c = CARDS[n]
        a = list(c['aim'])
        jobs.append((n + '_big', c['items'], tuple(a), c['sun'], 1024, 576, None))
        a4 = list(a); a4[5] = a[5] * 1.22
        jobs.append((n + '_row', c['items'], tuple(a4), c['sun'], 400, 300, None))
    with ThreadPoolExecutor(4) as ex:
        for line in ex.map(lambda q: render(*q), jobs):
            print(line)
