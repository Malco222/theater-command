#!/usr/bin/env python3
"""A stand-in for the drill's sections, made from a recording without the rules.

The drill's owner is giving every rules figure a section of followers (the new track field j, 1 to n-1; the key man
keeps k and has no j). Until that recording comes, the 3D scene is made ready on this one: the real recording expanded
into the new format by plain geometry, no rules.

  python3 Tools/land_record/sections_standin.py <recording.json> <out.json> [seed]

What it does (and only this; nothing here decides a battle):

* Sections. Every infantry key man gets followers: an American rifle squad of 12 (the key man, an assistant, the BAR
  team of three, the scout pair, riflemen), a German Gruppe of 10 (the key man, the MG 42 team of three, riflemen). A
  key man of role 'rifle' is the section's leader (his role becomes 'leader'); an officer, gunner or bazooka man keeps
  his role and his section gets a 'leader' among its followers (an officer leads his own). An hmg team is made up to 7,
  a mortar team to 8, a gun gets 7 crew ('crew', j 1 to 7, k the gun's). Spotters (fo), tanks and lorries get none.
* Where they stand. A loose formation round the key man, 3 to 8 m apart: a wedge on the move, a staggered file (a
  column of twos) when he runs, a shallow two- or three-rank line when he halts or fires; a gun's crew at the breech,
  the trails and the ammunition; a team round its gun or tube. Each follower walks to his place a little behind the key
  man (his own delay, 0.3 to 1.4 s, and a second to close up), faces where he goes, or where the section faces with his
  own small turn when still, and a crowd pass keeps every living man at least 3 m from every other (a gun's crew 1.8 m
  from each other at their gun).
* Poses. Each follower copies his key man's changes of pose, cover and flags, a little after him (his delay and up to
  half a second more), the gunner going down on his bipod where the key man fires, a third of the riflemen preferring
  to fire lying down where the key man kneels or stands. The scene gives loaders and crews their own stances.
* Falls. A block's followers fall in proportion to its key men's (followers x key deaths / key men), one at a time,
  spread over the battle by the fire coming in on the block (each round on it weighed: a kill 6, a hit 2, a shell 0.4,
  a bullet 0.15). Each fall gets its own kill round with oj (the man struck), fired by the shooter of the round it was
  drawn from (j: a follower of the firing section where small arms fired it), landing at him (a shell 1 to 4 m off).
  A gun's or mortar's kill round on a follower has no shell event: the scene draws its burst itself.
* When a key man falls, his living followers join the nearest live section of the block (their tracks go on).
* Followers' shots. Of the key men's rifle rounds, 6 in 10 are given to a living rifleman of the section (j), and for
  most rifle rounds a second follower fires within a second and a half at the same block (a miss, landing within some
  metres of the first); a rules figure's mg rounds are fired by his section's gunner.

Its output is checked: the men by role and side, the closest two living men at each second, and the follower falls.
"""

import bisect
import json
import math
import random
import sys
from collections import Counter, defaultdict

STEP = 0.5          # seconds between a follower's computed places
CLOSE_UP = 1.0      # seconds a follower takes to close up on his place
MIN_GAP = 3.0       # metres between living men (the crowd pass)
CREW_GAP = 1.8      # metres between a gun's crew
FOOT_ROLES = {'officer', 'rifle', 'mg', 'at', 'sapper', 'holder', 'hmg', 'mortar', 'leader'}
HE = {'mortar', 'gun', 'tank', 'at'}
FIRING = {'fire', 'fire_stand', 'fire_kneel', 'fire_prone', 'lmg_burst', 'throw_grenade'}


def wrap(a):
    while a > math.pi:
        a -= 2 * math.pi
    while a <= -math.pi:
        a += 2 * math.pi
    return a


class Path:
    """A track's kept points, lerped as LandFeed does."""

    def __init__(self, at):
        self.at = at
        self.ts = [p[0] for p in at]

    def spot(self, t):
        at = self.at
        i = bisect.bisect_right(self.ts, t)
        if i == 0:
            return at[0][1], at[0][2], at[0][3]
        if i >= len(at):
            return at[-1][1], at[-1][2], at[-1][3]
        a, b = at[i - 1], at[i]
        u = (t - a[0]) / max(1e-6, b[0] - a[0])
        return a[1] + (b[1] - a[1]) * u, a[2] + (b[2] - a[2]) * u, wrap(a[3] + wrap(b[3] - a[3]) * u)

    def start(self):
        return self.at[0][0]

    def end(self):
        return self.at[-1][0]


_CH_TIMES = {}


def change_at(ch, t):
    """The change in force at t (None before the first)."""
    ts = _CH_TIMES.get(id(ch))
    if ts is None:
        ts = _CH_TIMES[id(ch)] = [c[0] for c in ch]
    i = bisect.bisect_right(ts, t)
    return ch[i - 1] if i else None


# ---- The sections' make-up ------------------------------------------------------------------------------------------

def rifle_section(nat, key_role):
    """The followers' roles, in slot order, for an infantry key man of a nation."""
    if nat == 'USA':
        # Squad leader, assistant, BAR team (gunner, assistant, ammunition bearer), two scouts, riflemen: 12.
        roles = ['leader', 'rifle', 'mg', 'loader', 'loader', 'rifle', 'rifle', 'rifle', 'rifle', 'rifle', 'rifle', 'rifle']
    else:
        # Gruppenfuehrer, the MG 42 team (gunner, number two, number three), riflemen: 10.
        roles = ['leader', 'mg', 'loader', 'loader', 'rifle', 'rifle', 'rifle', 'rifle', 'rifle', 'rifle']
    size = len(roles)
    # The key man takes the place that is his: a rifleman's key man is the leader; the gunner the gun; an officer leads
    # (no leader among his followers); a bazooka or Panzerschreck man a rifleman's place, with a loader for his tube.
    if key_role in ('rifle', 'leader', 'officer'):
        roles.remove('leader')
    elif key_role == 'mg':
        roles.remove('mg')
    else:
        roles.remove('rifle')
        if key_role == 'at':
            roles[roles.index('rifle')] = 'loader'
    assert len(roles) == size - 1
    return roles


def team_followers(key_role):
    if key_role == 'hmg':
        return ['loader', 'leader', 'loader', 'loader', 'rifle', 'rifle']               # 7 with the gunner
    if key_role == 'mortar':
        return ['mortar', 'loader', 'loader', 'loader', 'loader', 'leader', 'rifle']    # 8 with the key man
    if key_role == 'gun':
        return ['crew'] * 7
    return []


# ---- Places in a section's frame: x to the right of its heading, y ahead (metres) -------------------------------

def wedge(s):
    r, side = (s + 1) // 2, (-1 if s % 2 else 1)
    return side * (1.6 + 1.4 * min(r, 6)), -3.0 * r - (0.8 if side > 0 else 0.0)


def file2(s):
    r, side = (s + 1) // 2, (-1 if s % 2 else 1)
    return side * 1.8, -3.4 * r - (1.7 if side > 0 else 0.0)


def _line_slots():
    out = []
    for row in range(6):
        y = -0.8 - 3.8 * row
        xs = [3.6, 7.2] if row % 3 == 0 else ([1.8, 5.4, 9.0] if row % 3 == 1 else [0.0, 3.6, 7.2])
        for x in xs:
            for sx in ((1, -1) if x else (1,)):
                out.append((sx * x, y - 0.2 * x / 3.6))
    out.sort(key=lambda p: (round(math.hypot(*p) / 3.0), p[1] < 0, abs(p[0])))
    return out


LINE = _line_slots()


def line(s):
    return LINE[(s - 1) % len(LINE)]


HMG_HALT = [(-1.7, -0.5), (3.5, -4.5), (-3.5, -4.5), (0.0, -7.5), (-5.5, -8.5), (5.5, -8.5)]
MORTAR_HALT = [(1.6, -0.4), (-2.2, -3.0), (2.6, -3.6), (-4.0, -7.0), (4.2, -7.2), (0.0, -6.0), (0.0, -10.0)]
GUN_CREW = [(-1.9, -0.8), (1.9, -0.8), (-1.6, -3.6), (1.6, -3.6), (0.0, -6.0), (-3.0, -7.5), (3.0, -7.5)]


def place(x, y, slot, heading):
    sx, sy = slot
    s, c = math.sin(heading), math.cos(heading)
    # ahead (sin f, cos f); right (cos f, -sin f)
    return x + sx * c + sy * s, y - sx * s + sy * c


# ---- The making -------------------------------------------------------------------------------------------------

def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    src, dst = sys.argv[1], sys.argv[2]
    seed = int(sys.argv[3]) if len(sys.argv) > 3 else 1944
    rng = random.Random(seed)
    R = json.load(open(src))
    blocks = {b['id']: b for b in R['blocks']}
    men = R['men']
    if any('j' in m for m in men):
        sys.exit('the recording already has sections')
    end = R.get('end', R.get('limit', 270.0))

    keys = {}
    for m in men:
        keys[(m['b'], m['k'])] = m
    paths = {key: Path(m['at']) for key, m in keys.items()}

    # Sections and their followers.
    sections = []  # (key, [roles])
    for m in men:
        b = blocks[m['b']]
        role = m['r']
        if b['kind'] == 'inf' and role in FOOT_ROLES:
            sections.append(((m['b'], m['k']), rifle_section(b['nat'], role)))
        elif role in ('hmg', 'mortar', 'gun'):
            sections.append(((m['b'], m['k']), team_followers(role)))

    class Man:
        pass

    followers = []
    by_section = defaultdict(list)
    for key, roles in sections:
        km = keys[key]
        for j, role in enumerate(roles, start=1):
            f = Man()
            f.key = key
            f.lead = key                         # the section he follows now
            f.j = j
            f.slot = j
            f.role = role
            f.kind = 'gun' if km['r'] == 'gun' else 'team' if km['r'] in ('hmg', 'mortar') else 'rifle'
            f.delay = 0.3 + 1.1 * rng.random()
            f.turn = (rng.random() - 0.5) * 0.7
            f.prone = rng.random() < 0.33
            f.pose_lag = rng.random() * 0.5
            f.dead = -1.0
            f.track = None
            f.pts = []
            f.x = f.y = None
            f.f = 0.0
            followers.append(f)
            by_section[key].append(f)

    # ---- Falls: how many, when, and from which incoming round.
    key_dead = {key: m.get('d', -1.0) for key, m in keys.items()}
    rounds = R['rounds']
    next_n = max(r['n'] for r in rounds) + 1
    plans = []  # (time, block, source round)
    for bid, b in blocks.items():
        ks = [key for key, _ in sections if key[0] == bid]
        if not ks:
            continue
        fs = [f for f in followers if f.key[0] == bid]
        kd = sum(1 for key in ks if key_dead[key] >= 0.0)
        want = int(round(len(fs) * kd / len(ks)))
        if want == 0:
            continue
        inc = []
        for r in rounds:
            if r.get('on') != bid:
                continue
            w = 6.0 if r['r'] == 'kill' else 2.0 if r['r'] != 'miss' else (0.4 if r['w'] in HE else 0.15)
            inc.append((r['l'], w, r))
        inc.sort(key=lambda e: e[0])
        total = sum(w for _, w, _ in inc)
        cum, acc = [], 0.0
        for l, w, r in inc:
            acc += w
            cum.append(acc)
        last = -10.0
        used = set()
        for i in range(want):
            q = (i + 0.3 + 0.4 * rng.random()) / want * total
            ix = min(len(inc) - 1, bisect.bisect_left(cum, q))
            # (each incoming round gives one fall at most: the next one after it, where it has been used)
            while ix in used and ix + 1 < len(inc):
                ix += 1
            used.add(ix)
            l, _, r = inc[ix]
            t = max(l, last + 0.6)   # one at a time
            last = t
            plans.append((t, bid, r))
    plans.sort(key=lambda p: p[0])

    # ---- Walk everyone through the battle, a step at a time: places, joins, the crowd pass, and the falls on time.
    t0 = min(p.start() for p in paths.values())
    steps = int(math.ceil((end - t0) / STEP)) + 1
    plan_i = 0
    new_rounds = []
    death_log = []
    joined = 0

    def key_alive(key, t):
        d = key_dead.get(key, -1.0)
        return d < 0.0 or t < d

    def mode_of(key, t):
        km = keys[key]
        p = paths[key]
        x1, y1, f1 = p.spot(t)
        x0, y0, _ = p.spot(t - 1.0)
        v = math.hypot(x1 - x0, y1 - y0)
        c = change_at(km['ch'], t)
        pose = c[1] if c else 'idle'
        if km['r'] == 'gun':
            return 'gun', f1
        if v > 0.4:
            heading = math.atan2(x1 - x0, y1 - y0)
            return ('file' if pose == 'run' or v > 3.2 else 'wedge'), heading
        return 'halt', f1

    for s in range(steps):
        t = t0 + s * STEP
        # Joins: the followers of a fallen key man go to the nearest live section of the block.
        for f in followers:
            if f.dead >= 0.0 or key_alive(f.lead, t - 1.5):
                continue
            lx, ly, _ = paths[f.lead].spot(t)
            best, bd = None, 1e9
            for key in by_section:
                if key[0] != f.key[0] or not key_alive(key, t + 5.0) or keys[key]['r'] != keys[f.lead]['r'] and f.kind != 'rifle':
                    continue
                if f.kind == 'rifle' and blocks[key[0]]['kind'] != 'inf':
                    continue
                kx, ky, _ = paths[key].spot(t)
                d = math.hypot(kx - lx, ky - ly)
                if d < bd:
                    best, bd = key, d
            if best is None:
                continue
            f.lead = best
            f.slot = len(by_section[best]) + 1 + sum(1 for g in followers if g.lead == best and g.key != best)
            joined += 1
        # Where each living follower wants to be.
        for f in followers:
            p = paths[f.lead]
            if t < p.start() - 1e-6:
                continue
            if f.dead >= 0.0:
                continue
            tl = max(p.start(), t - f.delay)
            kx, ky, _ = p.spot(tl)
            mode, heading = mode_of(f.lead, tl)
            if f.kind == 'gun':
                slot = GUN_CREW[(f.slot - 1) % len(GUN_CREW)]
            elif mode == 'halt' and f.kind == 'team':
                table = HMG_HALT if keys[f.lead]['r'] == 'hmg' else MORTAR_HALT
                slot = table[(f.slot - 1) % len(table)] if f.lead == f.key else line(f.slot)
            elif mode == 'halt':
                slot = line(f.slot)
            elif mode == 'file':
                slot = file2(f.slot)
            else:
                slot = wedge(f.slot)
            wx, wy = place(kx, ky, slot, heading)
            if f.x is None:
                f.x, f.y = wx, wy
                f.f = heading
            else:
                k = min(1.0, STEP / CLOSE_UP)
                nx, ny = f.x + (wx - f.x) * k, f.y + (wy - f.y) * k
                if f.kind == 'gun':
                    nx, ny = wx, wy
                f.x, f.y = nx, ny
            f.want_heading = heading
        # The crowd pass: living men at least MIN_GAP apart (key men and crews stand; followers give way).
        living = []
        for key, km in keys.items():
            if km['r'] in ('tank', 'lorry') or t < paths[key].start():
                continue
            x, y, _ = paths[key].spot(t)
            living.append([x, y, None, 1.5 if km['r'] == 'gun' else 0.0])
        for f in followers:
            if f.x is None or t < paths[f.lead].start():
                continue
            if f.dead >= 0.0:
                # (a body: walked round, not onto)
                living.append([f.pts[-1][1], f.pts[-1][2], None, 1.8 - MIN_GAP])
                continue
            living.append([f.x, f.y, f, 0.0])
        for _ in range(4):
            grid = defaultdict(list)
            for i, e in enumerate(living):
                grid[(int(e[0] // 4), int(e[1] // 4))].append(i)
            for i, e in enumerate(living):
                f = e[2]
                if f is None or f.kind == 'gun':
                    continue
                cx, cy = int(e[0] // 4), int(e[1] // 4)
                for gx in (cx - 1, cx, cx + 1):
                    for gy in (cy - 1, cy, cy + 1):
                        for o in grid.get((gx, gy), ()):
                            if o == i:
                                continue
                            q = living[o]
                            gap = MIN_GAP + q[3]
                            dx, dy = e[0] - q[0], e[1] - q[1]
                            d = math.hypot(dx, dy)
                            if d >= gap:
                                continue
                            if d < 1e-3:
                                a = rng.random() * 2 * math.pi
                                dx, dy, d = math.sin(a), math.cos(a), 1.0
                            push = (gap - d) * (0.5 if q[2] is not None and q[2].kind != 'gun' else 1.0)
                            e[0] += dx / d * push
                            e[1] += dy / d * push
        for e in living:
            f = e[2]
            if f is not None and f.kind != 'gun':
                f.x, f.y = e[0], e[1]
        # Facings and the kept points.
        for f in followers:
            if f.x is None or f.dead >= 0.0 or t < paths[f.lead].start():
                continue
            if f.pts:
                px, py = f.pts[-1][1], f.pts[-1][2]
                moved = math.hypot(f.x - px, f.y - py)
            else:
                moved = 0.0
            if moved > 0.2:
                want = math.atan2(f.x - px, f.y - py)
            else:
                want = wrap(f.want_heading + f.turn)
            dmax = 2.5 * STEP
            f.f = wrap(f.f + max(-dmax, min(dmax, wrap(want - f.f))))
            f.pts.append([round(t, 2), round(f.x, 2), round(f.y, 2), round(f.f, 3)])
        # The falls due by now.
        while plan_i < len(plans) and plans[plan_i][0] <= t + STEP:
            when, bid, r = plans[plan_i]
            plan_i += 1
            alive = [f for f in followers if f.key[0] == bid and f.dead < 0.0 and f.x is not None]
            if not alive:
                continue
            he = r['w'] in HE
            near = sorted(alive, key=lambda f: math.hypot(f.x - r['x'], f.y - r['y']))
            if he:
                victim = near[0] if math.hypot(near[0].x - r['x'], near[0].y - r['y']) < 60.0 else rng.choice(alive)
            else:
                victim = rng.choice(near[:max(1, len(near) // 3)])
            land = round(when, 2)
            flight = r['l'] - r['t']
            victim.dead = round(land + 0.1, 2)
            # (he goes on as he was going until he is hit)
            vx = vy = 0.0
            if len(victim.pts) >= 2:
                a, b = victim.pts[-2], victim.pts[-1]
                vx, vy = (b[1] - a[1]) / max(1e-6, b[0] - a[0]), (b[2] - a[2]) / max(1e-6, b[0] - a[0])
            go = max(0.0, victim.dead - victim.pts[-1][0])
            victim.x, victim.y = victim.pts[-1][1] + vx * go, victim.pts[-1][2] + vy * go
            victim.pts.append([victim.dead, round(victim.x, 2), round(victim.y, 2), round(victim.f, 3)])
            if he:
                a = rng.random() * 2 * math.pi
                off = 1.0 + 3.0 * rng.random()
                hx, hy = victim.x + math.sin(a) * off, victim.y + math.cos(a) * off
            else:
                hx, hy = victim.x, victim.y
            k = {'t': round(land - flight, 2), 'l': land, 'by': r['by'], 'k': r['k'], 'w': r['w'], 'on': bid,
                 'ok': victim.key[1], 'oj': victim.j, 'x': round(hx, 1), 'y': round(hy, 1), 'r': 'kill', 'n': next_n}
            next_n += 1
            # The firer: a living follower of the firing section where small arms fired it.
            if not he and r['by'] >= 0:
                fk = (r['by'], r['k'])
                pool = [g for g in by_section.get(fk, []) if g.dead < 0.0 and g.x is not None
                        and (g.role == 'mg' if r['w'] == 'mg' else g.role in ('rifle', 'leader'))]
                if r['w'] == 'mg' and keys.get(fk, {}).get('r') == 'mg':
                    pool = []
                if pool:
                    k['j'] = rng.choice(pool).j
            new_rounds.append(k)
            death_log.append((victim.dead, bid, victim.key[1], victim.j, victim.role, r['w']))

    # ---- Followers' changes of pose.
    def follower_pose(f, pose):
        if pose in ('drive', 'reverse', 'halt', 'parked', 'limbered'):
            return 'idle'
        if f.role == 'mg' and pose in ('fire_prone', 'fire_kneel', 'fire_stand', 'fire', 'hide'):
            return 'lmg_burst'
        if pose == 'lmg_burst' and f.role != 'mg':
            return 'fire_prone'
        if f.kind == 'rifle' and f.prone and pose in ('fire_kneel', 'fire_stand'):
            return 'fire_prone'
        if f.kind == 'team' and pose == 'fire' and f.role in ('rifle', 'leader'):
            return 'fire_prone' if f.role == 'rifle' else 'wait'
        return pose

    for f in followers:
        km = keys[f.key]
        ch = []
        for i, c in enumerate(km['ch']):
            t = round(c[0] + (f.delay + f.pose_lag if i else 0.0), 2)
            if f.dead >= 0.0 and t >= f.dead:
                break
            pose = follower_pose(f, c[1])
            aim_x = aim_y = None
            if pose in FIRING and c[3] is not None and c[4] is not None:
                aim_x = round(c[3] + rng.uniform(-5, 5), 1)
                aim_y = round(c[4] + rng.uniform(-5, 5), 1)
            row = [t, pose, c[2], aim_x, aim_y, c[5], c[6]]
            if ch and ch[-1][1:3] == row[1:3] and ch[-1][5:] == row[5:] and (aim_x is None) == (ch[-1][3] is None):
                continue
            if ch and t <= ch[-1][0]:
                row[0] = round(ch[-1][0] + 0.05, 2)
            ch.append(row)
        f.ch = ch

    # ---- Followers' shots: rifle rounds handed to the section, and a second rifle beside the first.
    def shooters(key, t, roles):
        out = []
        for f in by_section.get(key, []):
            if f.role in roles and (f.dead < 0.0 or t < f.dead - 0.2) and f.pts and f.pts[0][0] <= t:
                out.append(f)
        return out

    handed = added = mg_handed = 0
    extra = []
    for r in rounds:
        key = (r.get('by', -1), r.get('k', -1))
        if key not in by_section or r['w'] not in ('rifle', 'mg') or keys[key]['r'] in ('hmg',):
            continue
        if r['w'] == 'mg':
            if keys[key]['r'] != 'mg':
                g = shooters(key, r['t'], ('mg',))
                if g:
                    r['j'] = g[0].j
                    mg_handed += 1
            continue
        pool = shooters(key, r['t'], ('rifle', 'leader'))
        if pool and rng.random() < 0.6:
            r['j'] = rng.choice(pool).j
            handed += 1
        if pool and rng.random() < 0.8:
            g = rng.choice(pool)
            dt = rng.uniform(0.2, 1.5)
            t = round(r['t'] + dt, 2)
            if g.dead >= 0.0 and t >= g.dead - 0.2:
                continue
            e = {'t': t, 'l': round(r['l'] + dt, 2), 'by': r['by'], 'k': r['k'], 'j': g.j, 'w': 'rifle', 'on': r['on'],
                 'x': round(r['x'] + rng.gauss(0, 5), 1), 'y': round(r['y'] + rng.gauss(0, 5), 1), 'r': 'miss', 'n': next_n}
            next_n += 1
            extra.append(e)
            added += 1
    rounds.extend(new_rounds)
    rounds.extend(extra)
    rounds.sort(key=lambda r: (r['t'], r['n']))

    # ---- The tracks out: key men first as they were (a rifle key man is his section's leader), then each section.
    out_men = []
    for m in men:
        mm = dict(m)
        if (m['b'], m['k']) in by_section and blocks[m['b']]['kind'] == 'inf' and m['r'] == 'rifle':
            mm['r'] = 'leader'
        out_men.append(mm)
    for f in followers:
        if not f.pts:
            continue
        pts = keep(f.pts)
        tr = {'b': f.key[0], 'k': f.key[1], 'j': f.j, 'r': f.role, 'at': pts, 'ch': f.ch, 's': []}
        if f.dead >= 0.0:
            tr['d'] = f.dead
        km = keys[f.key]
        if 'out' in km and f.dead < 0.0:
            tr['out'] = round(km['out'] + f.delay, 2)
        out_men.append(tr)
    R['men'] = out_men
    R['standin'] = 'sections stand-in (Tools/land_record/sections_standin.py, seed %d): no rules' % seed
    with open(dst, 'w') as fh:
        json.dump(R, fh, separators=(',', ':'))

    # ---- The checks.
    roles = Counter()
    for m in out_men:
        if m['r'] in ('tank', 'gun', 'lorry'):
            continue
        roles[(blocks[m['b']]['side'], m['r'])] += 1
    total = sum(roles.values())
    print('men %d (followers %d), tracks %d, rounds %d (+%d kill rounds for followers, +%d followers\' rifle rounds; '
          '%d rifle rounds and %d mg rounds handed to followers); %d followers joined another section'
          % (total, len(followers), len(out_men), len(rounds), len(new_rounds), added, handed, mg_handed, joined))
    for side in ('att', 'def'):
        print('  %s: %s' % (side, ', '.join('%s %d' % (r, n) for (s, r), n in sorted(roles.items()) if s == side)))
    falls = Counter(d[1] for d in death_log)
    print('follower falls %d by block %s; by weapon %s' % (len(death_log), dict(sorted(falls.items())), dict(Counter(d[5] for d in death_log))))
    check_gaps(out_men, end)


def keep(pts):
    """The kept points: where a straight line from the last kept point misses by under 0.08 m and 0.04 rad, left out."""
    if len(pts) <= 2:
        return pts
    out = [pts[0]]
    for i in range(1, len(pts) - 1):
        a, b, c = out[-1], pts[i], pts[i + 1]
        u = (b[0] - a[0]) / max(1e-6, c[0] - a[0])
        lx, ly = a[1] + (c[1] - a[1]) * u, a[2] + (c[2] - a[2]) * u
        lf = wrap(a[3] + wrap(c[3] - a[3]) * u)
        if math.hypot(lx - b[1], ly - b[2]) > 0.08 or abs(wrap(lf - b[3])) > 0.04:
            out.append(b)
    out.append(pts[-1])
    return out


def check_gaps(men, end):
    """The closest two living men each second: with a follower among them (the stand-in's own), a crew at its gun, and
    two rules figures (the recording's own: its two batteries' spotters share one walk, and figures close up)."""
    paths = [(m, Path(m['at'])) for m in men if m['r'] not in ('tank', 'lorry', 'gun')]
    worst = {'follower': 99.0, 'crew': 99.0, 'figures': 99.0}
    under = Counter()
    t = 0.0
    while t <= end:
        pts = []
        for m, p in paths:
            if t < p.start() or ('d' in m and t >= m['d']):
                continue
            x, y, _ = p.spot(t)
            pts.append((x, y, 'crew' if m['r'] == 'crew' else 'follower' if 'j' in m else 'figure'))
        grid = defaultdict(list)
        for i, q in enumerate(pts):
            grid[(int(q[0] // 3), int(q[1] // 3))].append(i)
        for i, q in enumerate(pts):
            cx, cy = int(q[0] // 3), int(q[1] // 3)
            for gx in (cx - 1, cx, cx + 1):
                for gy in (cy - 1, cy, cy + 1):
                    for o in grid.get((gx, gy), ()):
                        if o <= i:
                            continue
                        d = math.hypot(q[0] - pts[o][0], q[1] - pts[o][1])
                        kinds = {q[2], pts[o][2]}
                        what = 'crew' if kinds == {'crew'} else 'figures' if kinds == {'figure'} else 'follower'
                        worst[what] = min(worst[what], d)
                        if d < 1.5:
                            under[what] += 1
        t += 1.0
    print("closest two living men, each second: with a follower %.2f m, a gun's crew %.2f m, two rules figures %.2f m; "
          "pairs under 1.5 m: with a follower %d, crew %d, figures %d"
          % (worst['follower'], worst['crew'], worst['figures'], under['follower'], under['crew'], under['figures']))


if __name__ == '__main__':
    main()
