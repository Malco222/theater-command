#!/usr/bin/env python3
# Share of months that are quiet for the player, by country played, old results against new. usage: quiet.py OLD_DIR NEW_DIR
import re, sys, os
groups = {'Germany': ['play-GER.txt', 'play-GER_WEST.txt', 'play-GER_SEALION.txt'], 'Britain': ['play-UK.txt'], 'Soviet Union': ['play-SOV.txt', 'play-SOV_FIRST.txt'],
          'Italy': ['play-ITA.txt', 'play-ITA_BALKAN.txt'], 'France': ['play-FRA.txt', 'play-FRA_ATTACK.txt'], 'Small countries': ['play-ESP.txt', 'play-FIN.txt', 'play-ROM.txt', 'play-SWE.txt', 'play-TUR.txt']}
def tot(d, files):
    q = m = n = 0
    for f in files:
        p = os.path.join(d, f)
        if not os.path.exists(p): continue
        for a, b in re.findall(r'quiet months.*?: (\d+) of (\d+);', open(p, errors='replace').read()):
            q += int(a); m += int(b); n += 1
    return q, m, n
for k, files in groups.items():
    a, b = tot(sys.argv[1], files), tot(sys.argv[2], files)
    print(f'{k:16} old {a[0]:4}/{a[1]:4} = {100*a[0]/max(a[1],1):5.1f}% ({a[2]} wars)   new {b[0]:4}/{b[1]:4} = {100*b[0]/max(b[1],1):5.1f}% ({b[2]} wars)')
