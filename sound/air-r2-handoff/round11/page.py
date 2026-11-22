import json, sys
src, dst = sys.argv[1], sys.argv[2]
s = open(src, encoding='utf-8').read()
r = json.load(open('r11.json')); M = json.load(open('marks.json'))
M['sheet'][0][1] = 'Steel sheet'; M['sheet'][1][1] = 'Shed door 1'; M['sheet'][2][1] = 'Shed door 2'
def rep(old, new, count=1):
    global s
    assert s.count(old) == count, (old[:80], s.count(old)); s = s.replace(old, new)

# 1. headline and lead
rep('<h1>New: the squad under fire, and real B-17s</h1>', '<h1>New: rounds striking the plane</h1>')
rep('<p class="lead">The newest cards are at the bottom: twelve for the squad around you on the ground, and above them four real B-17 recordings that could replace the old studio-library takes. None of them is in the game yet.',
    '<p class="lead">The newest cards are at the bottom: two for rounds striking the plane you ride. Above them wait twelve for the squad around you on the ground and the far air battle rebuilt with the real B-17s. None of them is in the game yet.')

# 2. the section, after the squad's
rep('  <div id="squad"></div>\n',
    '  <div id="squad"></div>\n\n'
    '  <h2>Rounds striking the plane</h2>\n'
    '  <p class="section-note">Round eleven, new this morning, for your “better sound of bullets riddling a metal plane”. '
    'Nobody has found a real recording of rounds hitting an aircraft’s skin yet, so these two cards are the nearest real sounds: sheet metal struck by other things (hail, a steel tool, knocks on a shed door). '
    'Each card plays its strikes one at a time, then three bursts landing as the game lands them, then those bursts riding a B-17 and riding a fighter, at the game’s own levels. '
    'Stamp a card Good and its strikes go in; name any strike you don’t want in the note. '
    'The three closer leads fetched on your Mac this morning were measured and set aside: a BB striking a melon rind (not metal); an airsoft gun fired at a can, where the recording can’t tell the pellet striking the can from the gun’s own muzzle; and an effects company’s “bullets hitting metal”, real strikes on metal but with no sign of a gun in it, from an effects library.</p>\n'
    '  <div id="hits"></div>\n')

# 3. the cards, after round ten's last
last = " measured:[['6','Pings'],['2.4 / 4.1 / 5.7 / 7.3 kHz','Where their ring sits (the game’s ping: 2.4 / 4.0 / 5.6 / 7.3 kHz)'],['−24.9 LUFS','As loud as the game’s ping']]},\n];"
J = lambda m: json.dumps(m, ensure_ascii=False).replace('"', "'")
cards = (
 " measured:[['6','Pings'],['2.4 / 4.1 / 5.7 / 7.3 kHz','Where their ring sits (the game’s ping: 2.4 / 4.0 / 5.6 / 7.3 kHz)'],['−24.9 LUFS','As loud as the game’s ping']]},\n"
 " {id:'r11_hail', group:'hits', tag:'New recordings \u00b7 9 strikes, then three bursts \u00b7 21 s', title:'Rounds striking the plane: hail on tin',\n"
 "  where:'When rounds or flak splinters strike the plane you ride (heard right where you are) or any aircraft within 60 m of the camera (heard where it is struck). Silent in the game today: you marked round seven\u2019s steel targets Fix, \u201cdoesn\u2019t sound like bullets riddling against metal\u201d.',\n"
 "  analogy:'The flashing round a window, in a hailstorm: thin metal, struck again and again.',\n"
 "  what:'Not bullets: hail striking the metal flashing round a loft window, recorded with a pair of small Rode lavalier microphones (corkyburger, CC0); each strike is one microphone\u2019s channel. The recording\u2019s background sits 30 to 39 dB under each strike. Then three bursts as the game lands them: a fighter\u2019s rounds strike in a run, one every 0.05 to 0.1 s, until the damage begins, and the game plays one no sooner than 0.08 s after the last. Two short bursts hit the bomber, then a longer one brings it down: 14 strikes heard in 2.6 s, each picked at random, never the same twice in a row. The engines under them are the game\u2019s own: Wings Over Whiteman, one of the two B-17 interiors you stamped Good (another bomber you ride plays the Sioux City one), and the fighter engine heard behind a fighter.',\n"
 "  clip:'r11_hail_run',\n"
 "  marks:" + J(M['hail']) + ",\n"
 "  extra:{clip:'plane_hits_four', label:'For comparison, round seven\u2019s steel targets, which you marked Fix'},\n"
 "  measured:[['9','Strikes, each heard in the bursts too'],['6 to 22 ms','How long each takes to fall 20 dB (round seven\u2019s, measured the same way: 0.17 to 0.22 s)'],['3.3 to 8.2 kHz','Where each strike\u2019s energy centres, from its first instant (round seven\u2019s: 2.4 to 3.3 kHz)'],['16 dB','How far the quietest strike stands over the B-17\u2019s engines above 2 kHz; across all pitches, about 1 dB (behind a fighter: 23 dB, and 5 dB)']]},\n"
 " {id:'r11_sheet', group:'hits', tag:'New recordings \u00b7 3 strikes, then three bursts \u00b7 17 s', title:'Rounds striking the plane: a steel sheet and a shed door',\n"
 "  where:'The same place as the hail. If you like both cards, both sets go in and the game picks among all twelve.',\n"
 "  analogy:'A steel tool on a steel sheet, and knocks on a metal shed door.',\n"
 "  what:'Not bullets either: one strike on a sheet of steel with a steel tool (Stereo Surgeon, CC0) and two on a metal shed door (EpicWizard, CC0). Each takes several times as long as the hail to fall away, and the shed door\u2019s two centre higher than most of the hail. The three bursts land at the same moments as on the hail card, so the two compare moment for moment; which strike plays at each is drawn at random, never the same twice in a row.',\n"
 "  clip:'r11_sheet_run',\n"
 "  marks:" + J(M['sheet']) + ",\n"
 "  extra:{clip:'plane_hits_four', label:'For comparison, round seven\u2019s steel targets, which you marked Fix'},\n"
 "  measured:[['3','Strikes'],['About 70 ms','How long each takes to fall 20 dB (the hail: 6 to 22 ms; round seven\u2019s: 0.17 to 0.22 s)'],['4.9 to 9.3 kHz','Where each strike\u2019s energy centres: the steel sheet 4.9, the shed door 7.7 and 9.3'],['19 dB','How far the quietest strike stands over the B-17\u2019s engines above 2 kHz; across all pitches, 5 dB (behind a fighter: 27 dB, and 8 dB)']]},\n" "];")
rep(last, cards)

# 4. the level strips
envs = {k: v['env'] for k, v in r['clips'].items()}
rep('const ENV = {', 'const ENV = {' + json.dumps(envs, separators=(',', ':'))[1:-1] + ',')

# 5. the credits
rep('      <li>Nothing comes from films, series or games.</li>',
    '      <li>Round eleven’s recordings were fetched on your Mac on 22 November from Freesound: corkyburger (hail on a window’s metal flashing), Stereo Surgeon (a steel sheet) and EpicWizard (a shed door), all CC0. Each strike is cut, faded and levelled (the steel and shed door to one level, the hail to one peak, which leaves the hail within about 5 dB of each other); the bursts are those strikes placed in time as the game’s gunnery lands them, over the game’s own engine recordings, through a model of the game’s limiter. Nothing is synthesized.</li>\n'
    '      <li>Nothing comes from films, series or games.</li>')
n = s.count(" {id:'"); rep('<b id="t-all">40</b>', '<b id="t-all">%d</b>' % n)
open(dst, 'w', encoding='utf-8').write(s)
print('ok', len(s))
