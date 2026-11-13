#!/usr/bin/env python3
# Side by side: Version 53 results against a later version's. usage: compare.py OLD_DIR NEW_DIR [file ...]
import re, sys, os
old, new = sys.argv[1], sys.argv[2]
files = sys.argv[3:] or sorted(f for f in os.listdir(new) if f.endswith('.txt') and (f.startswith('play-') or f.startswith('late-') or f.startswith('level-') or f.endswith('sweep.txt')))
def parse(path):
    out = {}
    if not os.path.exists(path): return out
    cur = None
    for line in open(path, errors='replace'):
        m = re.match(r'^(\w+) from (\w+ \d+) seed (\d+) level (\w+): played (\d+) months to ([^;]*); (.*)$', line)
        if m:
            cur = dict(me=m[1], frm=m[2], seed=m[3], level=m[4], months=int(m[5]), end=m[7].strip(), quiet='', long='', falls={}, problems='', goals='', sieges='')
            out[(m[1], m[2], m[3], m[4], len([k for k in out if k[:4] == (m[1], m[2], m[3], m[4])]))] = cur
            continue
        if cur is None: continue
        m = re.search(r'quiet months.*?: (\d+) of (\d+); longest quiet run (\d+) months', line)
        if m: cur['quiet'] = f'{m[1]}/{m[2]}'; cur['long'] = m[3]
        m = re.match(r'\s+goals: (.*)$', line)
        if m: cur['goals'] = m[1].replace('short ', 's:').replace('long ', 'l:')
        if line.strip().startswith('fallen:'):
            for nat, when, to, back in re.findall(r'(\w+) (\w+ \d+)→(\w+)( \(back\))?', line): cur['falls'][nat] = f"{when[:3]} {when[-2:]}>{to}{'*' if back else ''}"
        m = re.match(r'\s+problems: (\d+), errors: (\d+)', line)
        if m: cur['problems'] = f'{m[1]}p/{m[2]}e'
        m = re.match(r'\s+sieges: (\d+) ended \((\d+) towns fell, (\d+) raised\), mean ([\d.-]+) months; my assaults (\d+) \((\d+) took', line)
        if m: cur['sieges'] = f'{m[1]} sieges, mean {m[4]}mo, my assaults {m[6]}/{m[5]}'
    return out
def falls(d): return ' '.join(f'{k} {d["falls"][k]}' for k in ['FRA', 'SOV', 'GER', 'ITA', 'UK'] if k in d['falls'])
for f in files:
    a, b = parse(os.path.join(old, f)), parse(os.path.join(new, f))
    print(f'== {f}')
    for k in b:
        x, y = a.get(k), b[k]
        tag = f'{k[0]} {k[1][-4:]} s{k[2]} {k[3][:3]}'
        print(f'  {tag:22} v53: {(x["quiet"] if x else "-"):>6} quiet, longest {(x["long"] if x else "-"):>2}, {(x["end"][:14] if x else "-"):14} {x["goals"] if x else "":16} | {falls(x) if x else ""}')
        print(f'  {"":22} new: {y["quiet"]:>6} quiet, longest {y["long"]:>2}, {y["end"][:14]:14} {y["goals"]:16} | {falls(y)} | {y["sieges"]} | {y["problems"]}')
