#!/usr/bin/env python3
"""
A raid on troops, made from a recorded campaign raid (Brian, 24 November 2026: "i want every air battle playable" and
"if a bomber group is bombing troops instead of a factory or town and they get intercepted than i want that playable").

Until the rules thread records an intercepted raid on troops (the every-kind contract, CONTRACT.md section 8, item 2),
the 3D side builds against this: the release 2 live recording of the German day raid on Hampshire in May 1942
(day-whole.json, one view a battle second, recorded by tools/air-record/live-steps.ts) made over into a raid on a British
column on the march, as the contract's sections 2 to 4 have it:

  place.kind and place.mission 'ground'; place.marks: a march column on a road through the aim (6 tanks, 18 lorries,
  6 towed guns, 4 light flak guns and 10 blocks of 60 men, 44 pieces, every one inside the target ring), the defender's;
  the two boxes' air:bombs (from the recording made a step at a time, day-steps.json, where the whole one has none);
  air:mark-hit events at and after the bombs (mostly near misses and men scattered, a wreck and a burn); an escort group
  of Bf 109s that comes down and makes one strafing pass down the road (air:strafe) with two hits; view.marks on the
  views after the bombs (fire, cut, the pieces changed), and one stale word on a wrecked gun at 420 s, which the scene
  must refuse (a piece that is out never changes again: the scene's guard, AirShow's MarkChangesRefused).

Everything here is the test's own, not the rules': which pieces are hit and how is chosen by hand to be plausible (near
where each box's bombs come down), and none of it is a count the rules made.

Usage: make_marks_recording.py [day-whole.json] [day-steps.json] [out.json]
       (defaults: /home/user/air-rec/day-whole.json, day-steps.json beside it, troops-test.json beside it)
"""

import copy
import glob
import json
import math
import os
import sys

AIM = (50000.0, 70000.0)
ROAD_WAY = math.radians(60.0)          # the road's heading through the aim, clockwise from north
SPACING = 55.0                          # a march column's gap, metres, piece to piece
ESCORT_ID = 7
STRAFE_T = 408.0
LANE_HALF = 600.0


def along(s, off=0.0):
    """A point s metres along the road from the aim (plus: north-east), off metres right of it."""
    fx, fy = math.sin(ROAD_WAY), math.cos(ROAD_WAY)
    return (AIM[0] + fx * s + fy * off, AIM[1] + fy * s - fx * off)


def road():
    # (straight through the aim for 1.5 km either side, then easing off a little, as a country road does)
    pts = []
    for s, off in ((-3000.0, 60.0), (-2200.0, 20.0), (-1500.0, 0.0), (0.0, 0.0), (1500.0, 0.0), (2200.0, -25.0), (3000.0, -80.0)):
        x, y = along(s, off)
        pts.append([round(x, 1), round(y, 1)])
    return pts


def column():
    """44 pieces, front (north-east) to back: a vanguard of tanks, lorried infantry, the guns and their tractors, the
    light flak spread through it, a rearguard of tanks."""
    order = (['tank'] * 3 + ['men', 'men', 'lorry', 'lorry', 'lorry', 'aa'] + ['gun', 'lorry', 'gun', 'lorry', 'men', 'men']
             + ['lorry', 'lorry', 'lorry', 'aa', 'gun', 'lorry', 'gun'] + ['men', 'men', 'lorry', 'lorry', 'lorry', 'lorry']
             + ['aa', 'gun', 'lorry', 'gun', 'lorry', 'men', 'men'] + ['lorry', 'lorry', 'aa', 'men', 'men', 'lorry']
             + ['tank'] * 3)
    want = {'tank': 6, 'lorry': 18, 'gun': 6, 'aa': 4, 'men': 10}
    got = {k: order.count(k) for k in want}
    assert got == want, got
    models = {'tank': 'Matilda', 'lorry': '', 'gun': '25pdr', 'aa': '', 'men': ''}
    pieces = []
    n = len(order)
    face = ROAD_WAY
    for k, what in enumerate(order):
        s = (n - 1) * SPACING / 2.0 - k * SPACING   # the head of the column north-east of the aim
        x, y = along(s, 0.0)
        p = {'id': k + 1, 'what': what, 'model': models[what], 'nat': 'UK', 'x': round(x, 1), 'y': round(y, 1),
             'face': round(face, 4), 'n': 60 if what == 'men' else 1}
        pieces.append(p)
    return pieces


def lerp(a, b, t):
    return a + (b - a) * t


def escort_at(t, bombers):
    """The escort's place, heading and height at battle time t: over the bombers until 355 s, then down and round to
    the gate of its pass (back along the road, south-west of the aim), along the road, and up again."""
    bx, by = bombers
    gate = along(-LANE_HALF - 3216.0, 0.0)
    if t <= 355.0:
        return bx + 400.0, by - 500.0, 0.0, 5200.0
    if t <= 385.0:
        u = (t - 355.0) / 30.0
        a = (50400.0, 64500.0)
        b = (45000.0, 64000.0)
        return lerp(a[0], b[0], u), lerp(a[1], b[1], u), math.radians(265.0), lerp(5200.0, 2000.0, u)
    if t <= 404.0:
        u = (t - 385.0) / 19.0
        a = (45000.0, 64000.0)
        return lerp(a[0], gate[0], u), lerp(a[1], gate[1], u), math.atan2(gate[0] - a[0], gate[1] - a[1]) % (2 * math.pi), lerp(2000.0, 900.0, u)
    s = -LANE_HALF - 3216.0 + (t - 404.0) * 240.0
    x, y = along(s, 0.0)
    alt = 900.0 if t <= 430.0 else min(2500.0, 900.0 + (t - 430.0) * 60.0)
    return x, y, ROAD_WAY, alt


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else '/home/user/air-rec/day-whole.json'
    steps = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(src), 'day-steps.json')
    out = sys.argv[3] if len(sys.argv) > 3 else os.path.join(os.path.dirname(src), 'troops-test.json')
    shared = '/mnt/project-files/air-rules/every-air-battle/'
    real = sorted(glob.glob(shared + '*.json')) if os.path.isdir(shared) else []
    print('real ground recordings in %s: %s' % (shared, ', '.join(real) if real else 'none'))

    with open(src) as f:
        rec = json.load(f)
    d = copy.deepcopy(rec)
    d['key'] = 'troops-test'
    d['kind'] = 'ground'
    d['what'] = ('A RAID ON TROOPS, MADE OVER BY HAND (Tools/air_show/make_marks_recording.py) from ' + rec.get('key', '?')
                 + ': place.marks, view.marks, air:mark-hit and air:strafe are the test\'s own, not the rules\'.')
    place = d['sky']['place']
    place['kind'] = 'ground'
    place['mission'] = 'ground'
    pieces = column()
    r = d['sky']['target']['r']
    for p in pieces:
        assert math.hypot(p['x'] - AIM[0], p['y'] - AIM[1]) < r, p
    assert len(pieces) <= 64
    place['marks'] = {
        'nat': 'UK', 'posture': 'march', 'road': road(), 'divs': 3, 'men': 45000, 'per': 10, 'pieces': pieces,
        'corps': [{'id': 1, 'nat': 'UK', 'name': 'I Corps', 'kind': 'inf', 'divs': 3, 'men': 45000, 'str': 0.92,
                   'org': 0.88, 'posture': 'march'}],
        'law': {'note': 'the test\'s own; the 3D game ignores it'},
    }

    # The two boxes' bombs, as the step-by-step recording has them (the whole one, a view a second, has none).
    bombs = []
    if os.path.exists(steps):
        with open(steps) as f:
            st = json.load(f)
        for v in st['views']:
            for e in v.get('events', []):
                if e.get('what') == 'air:bombs':
                    bombs.append(e)
    if not bombs:
        for unit, x, n in ((1, 49250.0, 90), (2, 50750.0, 80)):
            bombs.append({'t': 393.3, 'what': 'air:bombs', 'text': 'Bombs away.', 'unit': unit, 'side': 'att', 'x': x,
                          'y': 67861.0, 'cue': 'bomb_stick_01', 'extra': {'n': n}})
    print('air:bombs: %d (%s)' % (len(bombs), ', '.join('unit %d at %.1f' % (e['unit'], e['t']) for e in bombs)))

    # The hits the test counts: on pieces where the show's drawn bombs of each box come down on this recording (box 1's
    # on the column from about 350 to 650 m south-west of the aim; box 2's no nearer than 220 m to any piece, east of
    # it, so its two are a near miss and men scattered there, to show a bomb laid on a piece from farther off); the
    # strafe's inside its lane.
    byid = {p['id']: p for p in pieces}

    def hit(t, unit, pid, how, men=0, strike='bomb', text=None):
        p = byid[pid]
        e = {'t': t, 'what': 'air:mark-hit', 'unit': unit, 'side': 'att', 'x': p['x'], 'y': p['y'],
             'extra': {'piece': p['id'], 'strike': strike, 'how': how, 'men': men, 'big': how in ('wreck', 'burn')}}
        if text:
            e['text'] = text
        return e, p
    hits = [hit(394.2, 1, 29, 'near'), hit(394.6, 1, 35, 'scatter', 7),
            hit(395.1, 1, 30, 'wreck', 0, 'bomb', 'A gun of the column is wrecked on the road.'),
            hit(395.8, 1, 28, 'burn', 0, 'bomb', 'A lorry burns on the road.'),
            hit(396.4, 1, 34, 'scatter', 4), hit(397.0, 1, 27, 'near'),
            hit(397.6, 2, 15, 'scatter', 5), hit(398.0, 2, 18, 'near')]
    strafe_hits = [hit(410.0, ESCORT_ID, 25, 'burn', 0, 'strafe', 'The strafing sets a lorry on fire.'),
                   hit(412.0, ESCORT_ID, 14, 'scatter', 6, 'strafe')]
    for e, p in hits + strafe_hits:
        assert p['what'] in ('lorry', 'men', 'gun', 'aa', 'tank'), p
    lane_from, lane_to = along(-LANE_HALF), along(LANE_HALF)
    strafe = {'t': STRAFE_T, 'what': 'air:strafe', 'unit': ESCORT_ID, 'side': 'att',
              'text': 'II./JG 2 dives on the column and strafes the road.',
              'x': AIM[0], 'y': AIM[1],
              'extra': {'from': [round(lane_from[0], 1), round(lane_from[1], 1)], 'to': [round(lane_to[0], 1), round(lane_to[1], 1)],
                        'width': 60, 'dur': 10, 'strike': 'strafe'}}

    # What each piece is after (view.marks): from each hit, in the first view at or after it; the stale word at 420.
    state = {p['id']: ('ok', p['n']) for p in pieces}
    words = []  # (t, id, state, men)
    for e, p in hits + strafe_hits:
        how = e['extra']['how']
        st_, men = state[p['id']]
        rank = ['ok', 'hit', 'burn', 'out']
        to = {'wreck': 'out', 'blast': 'out', 'burn': 'burn'}.get(how, 'hit' if how == 'near' and p['what'] != 'men' else st_)
        if rank.index(to) < rank.index(st_):
            to = st_
        if p['what'] == 'men':
            men = max(0, men - e['extra']['men'])
        state[p['id']] = (to, men)
        words.append((e['t'], p['id'], to, men if p['what'] == 'men' else None))
    wrecked = [p for e, p in hits if e['extra']['how'] == 'wreck'][0]
    words.append((420.0, wrecked['id'], 'hit', None))   # stale: the scene must refuse it

    views = d['views']
    by_t = [v['t'] for v in views]
    for v in views:
        t = v['t']
        # The escort, in every view.
        b1 = next((u for u in v['units'] if u['id'] == 1), None)
        bx, by = (b1['x'], b1['y']) if b1 else (50000.0, AIM[1])
        x, y, face, alt = escort_at(t, (bx, by))
        v['units'].append({
            'id': ESCORT_ID, 'side': 'att', 'kind': 'ftr', 'name': 'II./JG 2', 'x': round(x, 1), 'y': round(y, 1),
            'face': round(face, 4), 'str': 1, 'mor': 1, 'state': 'steady', 'figures0': 4, 'figures': 4, 'w': 300,
            'd': 200, 'act': 'march', 'speed': 240, 'engaged': -1, 'cover': '', 'loop': 'fighter_engine_loop',
            'order': None, 'extra': {'model': 'Bf109', 'cool': 'liquid', 'form': 'fours', 'n': 12, 'n0': 12, 'band': 1,
                                     'alt': round(alt, 1), 'job': 'escort', 'per': 3}})
        evs = v.setdefault('events', [])
        prev = by_t[by_t.index(t) - 1] if by_t.index(t) > 0 else -1.0
        for e in bombs + [h[0] for h in hits] + [strafe] + [h[0] for h in strafe_hits]:
            if prev < e['t'] <= t:
                evs.append(copy.deepcopy(e))
        evs.sort(key=lambda e: e.get('t', 0.0))
        # view.marks: from the bombs on.
        if t >= 393.3:
            changed = []
            for wt, pid, to, men in words:
                if prev < wt <= t:
                    row = {'id': pid, 'state': to}
                    if men is not None:
                        row['men'] = men
                    changed.append(row)
            cut = 0.0
            for e, p in hits + strafe_hits:
                if e['t'] <= t:
                    cut += {'wreck': 0.004, 'burn': 0.003, 'near': 0.0012, 'scatter': 0.0018}.get(e['extra']['how'], 0.001)
            fire = 1.0 if t < 394.0 else 0.75
            v['marks'] = {'fire': fire, 'cut': round(min(0.04, cut), 4), 'pieces': changed}

    # The after card speaks of the column, not a town's output.
    end = d.get('end') or {}
    end['note'] = 'The raid cut the organisation of the British I Corps by %.1f%%.' % (100.0 * min(0.04, sum(
        {'wreck': 0.004, 'burn': 0.003, 'near': 0.0012, 'scatter': 0.0018}.get(e['extra']['how'], 0.001) for e, _ in hits + strafe_hits)))
    after = end.get('after') or {}
    lines = list(after.get('lines') or [])
    if lines:
        lines[0] = lines[0].replace('dropped their bombs on Hampshire', 'bombed the British column in Hampshire')
    lines.append(end['note'])
    after['lines'] = lines
    end['after'] = after
    d['end'] = end

    with open(out, 'w') as f:
        json.dump(d, f, separators=(',', ':'))
    size = os.path.getsize(out)
    print('wrote %s: %d bytes, %d views, %d pieces, %d bomb hits, %d strafe hits, 1 strafe' % (
        out, size, len(views), len(pieces), len(hits), len(strafe_hits)))
    for e, p in hits + strafe_hits:
        print('  %.1f unit %d %s on piece %d (%s) men %d' % (e['t'], e['unit'], e['extra']['how'], p['id'], p['what'], e['extra']['men']))


if __name__ == '__main__':
    main()
