#!/usr/bin/env python3
# The red band before the player's own country falls (probe91's DANGER lines), as one table.
#   python3 dangertab.py 'out/v831d/*.txt' ['out/v831e/*.txt' ...]   (a later glob replaces an earlier file's seat and seed)
import glob, re, sys, collections
seen = {}
for pat in sys.argv[1:]:
    for f in sorted(glob.glob(pat)):
        name = f.split('/')[-1][:-4].replace('fixed-', '')
        lines = open(f).read().splitlines()
        d = [l for l in lines if l.startswith('DANGER')]
        if d: seen[name] = (d[0], [l for l in lines if 'WORDS' in l], [l for l in lines if 'ERR' in l])
kinds = collections.OrderedDict()
for name, (l, words, errs) in sorted(seen.items()):
    kinds.setdefault('-'.join(name.split('-')[:2]), []).append((name, l, words, errs))
tot = collections.Counter(); notice = collections.Counter(); ends = collections.Counter()
print('seat                wars  fell  last card: FALL  LAST  REACH  NOTHING | months of unbroken notice (1, 2, 3, 4+) | stands  false alarms | broken runs')
for k, v in kinds.items():
    fell = [x for x in v if ' FELL ' in x[1]]; stands = [x for x in v if 'STANDS' in x[1]]
    c = collections.Counter(re.search(r'opened: (\w+)', x[1]).group(1) for x in fell)
    ns = [int(re.search(r'unbroken notice (\d+)', x[1]).group(1)) for x in fell]
    false = [x for x in stands if 'fall bands in 0 months' not in x[1]]
    broken = [x for x in fell if int(re.search(r'not in the run up to the end (-?\d+)', x[1]).group(1)) > 0]
    print(f"{k:18} {len(v):5} {len(fell):5} {c['FALL']:16} {c['LAST']:5} {c['REACH']:6} {c['NOTHING']:8} | {sum(n==1 for n in ns):2} {sum(n==2 for n in ns):2} {sum(n==3 for n in ns):2} {sum(n>=4 for n in ns):2} {'':24}| {len(stands):6} {len(false):13} | {len(broken)}")
    tot['wars'] += len(v); tot['fell'] += len(fell); tot['ok'] += c['FALL'] + c['LAST']; tot['stands'] += len(stands); tot['false'] += len(false); tot['broken'] += len(broken); tot['err'] += sum(1 for x in v if x[3])
    for n in ns: notice[min(n, 4)] += 1
    for x in fell:
        for w in x[2]:
            m = re.search(r'(If \w[\w ]* is taken[^.]*\.|A government shut[^.]*\.|That is more than[^.]*\.|With a little more lost[^.]*\.)', w)
            ends[re.sub(r'(Berlin|Rome|Paris|Moscow|Germany|Italy|France|the Soviet Union)', 'X', m.group(1)) if m else 'other'] += 1
print(f"\nall: {tot['wars']} wars; the country fell in {tot['fell']}; a red band or the LAST MONTH lead on the last card before the fall in {tot['ok']} of {tot['fell']}; "
      f"notice of 1 month {notice[1]}, 2 months {notice[2]}, 3 months {notice[3]}, 4 or more {notice[4]}; seats that stand {tot['stands']}, with a fall band in any month {tot['false']}; "
      f"wars where a fall band came and went before the end {tot['broken']}; wars with an error line {tot['err']}")
print('what the last band said ends it:'); [print(f'  {n:3}  {e}') for e, n in ends.most_common()]
