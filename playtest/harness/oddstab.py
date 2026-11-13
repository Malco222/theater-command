#!/usr/bin/env python3
"""Version 59 on: the cabinet's odds for a town it asks for against the attack card's odds the same month.
usage: oddstab.py FILE..."""
import sys, re, statistics as st
rows = []
for f in sys.argv[1:]:
    cur = None
    for ln in open(f, errors='replace'):
        m = re.match(r'(\w+ \d{4})\s+DIRECTIVE (take|relief) .*?: (.+?); (\d+) months;.*odds now ([\d.]+|none)\].*the cabinet\'s odds (over 99|[\d.]+)', ln)
        if m:
            cur = dict(f=f.split('/')[-1][:-4], when=m.group(1), title=m.group(3), card=None if m.group(5) == 'none' else float(m.group(5)), cab=99.0 if m.group(6) == 'over 99' else float(m.group(6)), hard='strongly held' in ln, end='open'); rows.append(cur); continue
        m = re.match(r'\s+\w+ \d{4}\s+=> (.+)', ln)
        if m and cur and cur['end'] == 'open': cur['end'] = m.group(1).strip()
both = [r for r in rows if r['card'] is not None]
print(f"towns asked for: {len(rows)}; still the enemy's at the end of the month asked, with an attack on the card: {len(both)}")
if both:
    ratio = [r['cab'] / max(0.05, r['card']) for r in both]
    print(f"  the cabinet's odds over the card's: median {st.median(ratio):.1f} times; twice or more in {sum(1 for x in ratio if x >= 2)}, four times or more in {sum(1 for x in ratio if x >= 4)}")
    fair = [r for r in both if r['cab'] >= 1.5]
    print(f"  the cabinet read 1.5 to 1 or better in {len(fair)}; the card read under 1.2 to 1 in {sum(1 for r in fair if r['card'] < 1.2)} of those, under 1 to 1 in {sum(1 for r in fair if r['card'] < 1.0)}")
    for lo, hi in ((0, 1.0), (1.0, 1.5), (1.5, 99)):
        xs = [r for r in both if lo <= r['card'] < hi]; 
        if xs: print(f"  card's odds {lo} to {hi}: {len(xs)} asked, {sum(1 for r in xs if r['end'] == 'carried out')} done, {sum(1 for r in xs if r['end'] == 'not carried out')} missed")
if '-v' in sys.argv[0:0]: pass
for r in both[:0]: print(r)
