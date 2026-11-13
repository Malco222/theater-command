#!/usr/bin/env python3
"""Version 59 on: towns the cabinet asked for: its own odds when it asked against the attack card's odds before the player moved.
usage: cardtab.py FILE... [-v]"""
import sys, re, statistics as st
rows = []; V = '-v' in sys.argv
for f in [a for a in sys.argv[1:] if a != '-v']:
    cur = None
    for ln in open(f, errors='replace'):
        m = re.match(r'(\w+ \d{4})\s+DIRECTIVE (take|relief) .*?: (.+?); (\d+) months;.*the cabinet\'s odds (over 99|[\d.]+)', ln)
        if m:
            dv = re.search(r'when asked: held by (\w+), ([\d.]+) divs', ln)
            cur = dict(f=f.split('/')[-1][:-4], when=m.group(1), title=m.group(3), cab=99.0 if m.group(5) == 'over 99' else float(m.group(5)), divs=float(dv.group(2)) if dv else None, hard='strongly held' in ln, end='open', first=None, best=None, none=None, months=None); rows.append(cur); continue
        m = re.match(r'\s+(\w+ \d{4})\s+=> ([a-z ]+?)(?: \[the card\'s odds before the player moved: the month asked (no attack|[\d.]+), best in (\d+) months (no attack|[\d.]+), months with no attack on the card (\d+)\])?\s*$', ln)
        if m and cur and cur['end'] == 'open':
            cur['end'] = m.group(2).strip()
            if m.group(3): cur['first'] = None if m.group(3) == 'no attack' else float(m.group(3)); cur['best'] = None if m.group(5) == 'no attack' else float(m.group(5)); cur['months'] = int(m.group(4)); cur['none'] = int(m.group(6))
            cur = None
have = [r for r in rows if r['months'] is not None]
print(f"towns asked for: {len(rows)}; ended with the card's odds on record: {len(have)}")
def pr(label, xs):
    if not xs: return
    done = sum(1 for r in xs if r['end'] == 'carried out')
    print(f"  {label:<58}{len(xs):>4} asked, {done:>3} taken ({100 * done / len(xs):.0f}%)")
card = [r for r in have if r['first'] is not None]
print(f"an attack was on the card in the month asked for {len(card)}; none for {len(have) - len(card)}")
if card:
    ratio = [r['cab'] / max(0.05, r['first']) for r in card]
    print(f"  the cabinet's odds over the card's, the month asked: median {st.median(ratio):.1f} times; twice or more {sum(1 for x in ratio if x >= 2)}, four times or more {sum(1 for x in ratio if x >= 4)} of {len(ratio)}")
    fair = [r for r in card if r['cab'] >= 1.5]
    print(f"  the cabinet read 1.5 to 1 or better for {len(fair)}: the card read under 1.2 to 1 for {sum(1 for r in fair if r['first'] < 1.2)}, under 1 to 1 for {sum(1 for r in fair if r['first'] < 1.0)}")
    print("by the card's odds in the month asked:")
    for lo, hi in ((0, 0.6), (0.6, 1.0), (1.0, 1.5), (1.5, 3), (3, 999)): pr(f"  card {lo} to {hi if hi < 900 else 'up'}", [r for r in card if lo <= r['first'] < hi])
    pr('  no attack on the card that month', [r for r in have if r['first'] is None])
    print("by the best the card ever read while the directive ran:")
    for lo, hi in ((0, 1.0), (1.0, 1.5), (1.5, 999)): pr(f"  best {lo} to {hi if hi < 900 else 'up'}", [r for r in have if r['best'] is not None and lo <= r['best'] < hi])
    pr('  never an attack on the card', [r for r in have if r['best'] is None])
    print("by the cabinet's own odds:")
    for lo, hi in ((0, 1.0), (1.0, 1.5), (1.5, 3), (3, 6), (6, 999)): pr(f"  cabinet {lo} to {hi if hi < 900 else 'up'}", [r for r in have if lo <= r['cab'] < hi])
if V:
    for r in have: print(f"   {r['f']:<22}{r['when']:<16}{r['title'][:30]:<32}cabinet {r['cab']:>6.2f}  card {('%.2f' % r['first']) if r['first'] is not None else '  - '}  best {('%.2f' % r['best']) if r['best'] is not None else '  - '} in {r['months']}  {'HARD ' if r['hard'] else ''}-> {r['end']}")
