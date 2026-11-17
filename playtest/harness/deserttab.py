"""Version 84: the desert from an Axis seat, from the BLOW lines of probe95 (files DIR/PREFIX-SEED.txt).
usage: python3 deserttab.py DIR PREFIX..."""
import re, sys, glob, collections
for pre in sys.argv[2:]:
    n = 0; when = collections.Counter(); c = collections.Counter(); lost = collections.Counter(); thr = []
    for f in sorted(glob.glob(f'{sys.argv[1]}/{pre}-*.txt')):
        for l in open(f):
            if not l.startswith('BLOW'): continue
            n += 1
            m = re.search(r'first British blow with artillery: ([A-Z][a-z]+ \d{4}|never in the window)', l); w = m.group(1) if m else '?'
            when[w] += 1
            if w.startswith('never'): 
                pass
            else:
                seg = l.split('first British blow with artillery:')[1].split('| first ground lost')[0]
                g = re.search(r'their guns ([^,|]*(?:\([^)]*\))?[^,|]*)', seg); guns = seg.split('their guns ')[1].split(', card')[0] if 'their guns ' in seg else '?'
                c['guns: none in the desert as the month opened'] += guns.startswith('none')
                c['guns: there, not identified'] += ('not identified' in guns)
                c['guns: there, identified'] += (not guns.startswith('none') and 'not identified' not in guns)
                c['card: nothing of the desert, or an honour only'] += ('card nothing' in seg or 'Honour' in seg)
                t = re.search(r'highest threat on the line ([\d.]+)', seg); tb = re.search(r'the month before: threat ([\d.]+)', seg)
                if t: thr.append((float(tb.group(1)) if tb else -1, float(t.group(1))))
                c['threat under 1.0 the month before'] += bool(tb and float(tb.group(1)) < 1.0)
            m = re.search(r'first ground lost in the desert: ([A-Z][a-z]+ \d{4}|never)', l)
            if m: lost[m.group(1)[-4:] if m.group(1) != 'never' else 'never'] += 1
            c['guns ever identified before they fired: yes'] += ('before they fired: yes' in l)
            c['winter 1940-41 ground lost (to February 1941)'] += bool(m and re.search(r'(October|November|December) 1940|(January|February) 1941', m.group(1) or ''))
    blows = n - when['never in the window']
    print(f'== {pre}: {n} wars; a British blow with artillery in {blows}: ' + ', '.join(f'{k} {v}' for k, v in sorted(when.items(), key=lambda kv: (kv[0][-4:], kv[0])) if not k.startswith('never')))
    print('   of those ' + str(blows) + ': ' + '; '.join(f'{k} {v}' for k, v in c.items() if not k.startswith('guns ever') and not k.startswith('winter')))
    print(f"   all wars: guns ever identified before they fired {c['guns ever identified before they fired: yes']} of {n}; first desert ground lost by February 1941 in {c['winter 1940-41 ground lost (to February 1941)']}; by year {dict(lost)}")
