#!/usr/bin/env python3
"""The "Once ashore" forecast against what happened, every seat together (HELD lines of probe56).
holdall.py FILE_GLOB... ; prints by seat and how played, then totals for the navies weaker than their enemy and for Britain."""
import sys,re,glob,collections
rows=collections.defaultdict(lambda: collections.Counter())
for pat in sys.argv[1:]:
    for f in glob.glob(pat):
        how='Field command' if '/domfield-' in f else 'by hand'
        for l in open(f):
            m=re.match(r'HELD (\w+) seed (\d+): (.*?) with (\d+) corps: the card said the (.*?) (stays open|would be closed) \((.*?)\); a month later it was (CLOSED|open), the force (gone|fed|unfed).*?closed in any of the three months after: (yes|no)',l)
            if not m: continue
            me,seed,where,k,sea,said,why,later,force,any3=m.groups()
            c=rows[(me,how)]
            key='open' if said=='stays open' else 'warn'
            c[key]+=1
            if later=='CLOSED': c[key+'_closed1']+=1
            if any3=='yes' or later=='CLOSED': c[key+'_closed3']+=1
names={'GER':'Germany','ITA':'Italy','UK':'Britain','FRA':'France','SOV':'Soviet Union'}
print('   seat          how played     "stays open": landings, closed a month later, closed within three months   "would be closed": landings, closed a month later, closed within three months')
tot=collections.defaultdict(lambda: collections.Counter())
for (me,how) in sorted(rows):
    c=rows[(me,how)]
    print(f"   {names.get(me,me):13s} {how:14s} {c['open']:5d} {c['open_closed1']:5d} {c['open_closed3']:5d}      {c['warn']:5d} {c['warn_closed1']:5d} {c['warn_closed3']:5d}")
    g='Britain (the stronger navy)' if me=='UK' else 'Germany, Italy, France, the Soviet Union (the weaker navy)'
    tot[g].update(c); tot['every seat'].update(c)
print()
for g,c in tot.items():
    def pc(a,b): return f"{a} of {b} ({round(100*a/b) if b else 0}%)"
    print(f"   {g}:")
    print(f"      the card said \"stays open\" before {c['open']} landings: closed a month later in {pc(c['open_closed1'],c['open'])}, within three months in {pc(c['open_closed3'],c['open'])}")
    print(f"      the card said \"would be closed\" before {c['warn']} landings: closed a month later in {pc(c['warn_closed1'],c['warn'])}, within three months in {pc(c['warn_closed3'],c['warn'])}")
