# Round twelve on the listening page: real recordings for four old studio-library takes.
# Usage: python3 -I page.py <clips dir> <page in> <page out> <r12.json out>
import json, subprocess, sys
import numpy as np

clips_dir, src, dst, rj = sys.argv[1:5]
s = open(src, encoding='utf-8').read()

def rep(old, new, count=1):
    global s
    assert s.count(old) == count, (old[:80], s.count(old))
    s = s.replace(old, new)

def env(path):  # the page's level strip: mono 8 kHz, 50/100/250 ms steps
    raw = subprocess.run(['ffmpeg', '-v', 'error', '-i', path, '-ac', '1', '-ar', '8000', '-f', 'f32le', '-'],
                         check=True, capture_output=True).stdout
    x = np.frombuffer(raw, dtype=np.float32).astype(np.float64); sr = 8000
    dd = len(x) / sr
    step = 0.05 if dd < 12 else (0.1 if dd < 30 else 0.25)
    h = int(sr * step); k = len(x) // h
    r = np.sqrt(np.mean(x[:k * h].reshape(k, h) ** 2, axis=1) + 1e-12)
    return {'step': step, 'dur': round(dd, 2), 'db': [round(float(v), 1) for v in np.maximum(20 * np.log10(r), -60)]}

WHERE_LAUNCH = ('Would replace the old studio-library torpedo launch (2.5 s), for “a submarine or destroyer fires a torpedo”. '
                'The game lists that take but isn’t placing it in a battle yet in the code I checked; whichever you stamp Good is the one the sea battle will use.')
WHERE_DC = ('Would replace the old studio-library depth-charge thrower (0.9 s). The game lists that take but isn’t placing it in a battle yet '
            'in the code I checked; whichever you stamp Good is the one the sea battle will use.')
WHERE_RUN = ('Would replace the old studio-library torpedo run, a 9 s loop. Like the launch, the game lists it but isn’t placing it in a battle yet '
             'in the code I checked; whichever you stamp Good is the one the sea battle will use.')
RUN_MARKS = lambda j, twice=True: [[0, 'Old take, twice'], [9, 'Old take comes round'], [18, '1 s silence'],
                                   [19, 'New take, twice' if twice else 'New take'], [j, 'New take comes round']]

CARDS = [
 dict(id='r12_stuka', tag='Freesound recording · loops every 12 s', title='The Stuka’s engines? A Bf 109’s Daimler-Benz on the ground',
  take='stuka_drone_loop_r12_fs242179_1.wav', sound='stuka_drone_loop',
  where='Would replace the Stuka formation’s engines, an old studio-library loop heard wherever Ju 87s fly; when you ride a Ju 87 the same loop plays around you with the sound above 200 Hz turned down. A stand-in: no recording of a Ju 87’s own Jumo engine has turned up yet.',
  analogy='Like a cousin’s voice on the phone: the same family of engine (a German upside-down V12), not the same one.',
  what='A Bf 109 taxiing (an E-3 with a DB 601, according to a second-hand description), recorded by Fight2FlyPhoto. Twelve seconds at one steady medium setting (2:19.5 to 2:32 in the recording), looped. Its speed, estimated from its lowest tones, is about 1,300 rpm; a Jumo cruising would turn roughly 2,200 to 2,400, so this is slower than a Stuka in flight. Two knocks, at 5.4 and 10.3 s, stand about 5 dB over the engine and come round every 12 s with the loop, and there is one probable clipped peak at 10.8 s. Only the left microphone is used, because adding the right one cancels some of the top. Licence: CC BY-NC 3.0, which needs the credit and allows no sale.',
  clip='r12_stuka_db02_1300',
  marks=[[0, 'Old take (studio library), twice'], [12, 'Old take comes round'], [24, '1 s silence'], [25, 'New take, twice'], [37.05, 'New take comes round']],
  extra=('r12_stuka_db02_1300_source', 'The recording around it, 35 s from 2:05 at its own balance: a burst of power, then it settles; the loop is 14.5 to 27 s in'),
  measured=[['39%', 'Energy under 200 Hz (the old take: 17%): the part that still plays when you ride a Ju 87'],
            ['About 1,300 rpm', 'Engine speed, estimated (the old take: about 2,360, or 1,180)'],
            ['1.6 LU', 'How much the level wanders (the old take: 0.9)'],
            ['0.34 dB', 'Level jump where the loop comes round (the old take: 0.54)']]),
 dict(id='r12_torp_shot', tag='Navy tape · 2.5 s', title='A torpedo leaving its tube: inside a submarine’s torpedo room',
  take='torpedo_launch_r12_awmS04697_1.wav', sound='torpedo_launch',
  where=WHERE_LAUNCH,
  analogy='Like a door slammed in a steel room: one hard thump, then the room rumbling after it.',
  what='From an Australian War Memorial tape of HMAS Oxley, an Oberon-class submarine, “operational sounds”. The tape’s track is titled “Torpedo Room – Firing torpedo”; this is the largest sudden event in it, so that it is the firing is an inference. Above 2 kHz the sound rises about 20 dB in a tenth of a second, then a low rumble follows. The take opens on the room’s hum, about −24 dBFS, and the shot stands 9.5 dB over it. Its end is a made fade of 0.25 s, since the rumble hasn’t died away. The tape stops at 8.4 kHz, so there is no very top. Licence: the Memorial allows personal, non-commercial use only.',
  clip='r12_torp_oxley_shot',
  marks=[[0, 'Old take'], [3.5, 'New take'], [3.55, 'The tube fires'], [4.45, 'Low rumble']],
  extra=('r12_torp_oxley_shot_source', 'The tape around it, 31 s: the tube fires at 12 s; an air rush starts at 17.8 s (the next card)'),
  measured=[['−17.4 LUFS', 'Loudness (the old take: −21.6), with both set to the same loudest 50 ms'],
            ['About 20 dB', 'Rise above 2 kHz in 0.1 s as it fires'],
            ['9.5 dB', 'How far the shot stands over the room hum it opens on'],
            ['8.4 kHz', 'Where the tape stops (the old take reaches 18 kHz)']]),
 dict(id='r12_torp_vent', tag='Navy tape · 2.6 s', title='A torpedo leaving its tube: the air rush after it',
  take='torpedo_launch_r12_awmS04697_2.wav', sound='torpedo_launch',
  where='The same place as the card above. If you like both, say in a note whether this rush should play straight after the shot.',
  analogy='Like letting a lorry tyre down indoors.',
  what='From the same tape, 5.9 s after the shot: a rush of air that rises in 12 ms. Two-thirds of its energy lies above 2 kHz. That it is the tube venting is an inference. Its end is a made fade of 0.3 s, because the rush carries on (a second step comes 12 s later on the tape). Licence as above: personal, non-commercial use only.',
  clip='r12_torp_oxley_vent',
  marks=[[0, 'Old take'], [3.5, 'New take'], [3.55, 'Air rush starts']],
  extra=('r12_torp_oxley_vent_source', 'The tape around it, 31 s: the tube fires at 12 s; this rush starts at 17.8 s'),
  measured=[['−14 LUFS', 'Loudness (the old take: −21.6), with both set to the same loudest 50 ms'],
            ['12 ms', 'How fast the rush rises (the old take: 135 ms)'],
            ['67%', 'Energy above 2 kHz (the old take: 14%)']]),
 dict(id='r12_torp_deck', tag='Navy film · 1.3 s', title='A torpedo leaving a cruiser’s deck tube',
  take='torpedo_launch_r12_dvids665898_1.wav', sound='torpedo_launch',
  where='The same place: it fits the destroyer half of “a submarine or destroyer fires a torpedo”.',
  analogy='Like a cork popped out on a ship’s deck: a sharp pop and a puff of air.',
  what='From a U.S. Navy film of USS Chancellorsville firing exercise torpedoes (MC2 Jeremy Graham, public domain). It opens on the ship’s background at about −25 dBFS, then a pop at 0.05 s and a burst of air. After 0.6 s it sits 21 dB below the background it opened on, while the camera’s automatic level seems to recover over the next 3 s (inference), so part of that drop is the camera, not the sea. Left channel only.',
  clip='r12_torp_cv_deck',
  marks=[[0, 'Old take'], [3.5, 'New take'], [3.55, 'Pop and air burst']],
  extra=('r12_torp_cv_deck_source', 'The film around it, 5.5 s: the pop at 1.0 s'),
  measured=[['57%', 'Energy above 2 kHz (the old take: 14%)'],
            ['15 kHz', 'How high the film’s sound reaches'],
            ['0.2 s', 'To fall 20 dB (the old take: 2.1 s)'],
            ['1.3 s', 'Length (the old take: 2.5 s)']]),
 dict(id='r12_torp_far', tag='Navy film · 2.8 s', title='A torpedo launched, heard from further off',
  take='torpedo_launch_r12_dvids665900_1.wav', sound='torpedo_launch',
  where='The same place.',
  analogy='Like hearing it down a telephone line: everything above 3.3 kHz is gone.',
  what='From a U.S. Navy film of USS Curtis Wilbur’s torpedo exercise and 5-inch gun shoot (MC1 Alexandra Seeley, public domain). A pop, then a long rush. It is probably not the gun (inference: nothing overloads, and it takes about 150 ms to rise, where gun blasts rise in a few ms). The film’s sound stops at 3.3 kHz, the same upper limit a telephone line has. Below 200 Hz the level is about 14 dB lower after the event than before, so the camera may have turned itself down. Raised 8.5 dB to the game’s level; its end is a made fade.',
  clip='r12_torp_cw_far',
  marks=[[0, 'Old take'], [3.5, 'New take'], [3.55, 'Pop, then a long rush']],
  extra=('r12_torp_cw_far_source', 'The film’s first 4.3 s: the take runs from 0.7 to 3.5 s'),
  measured=[['3.3 kHz', 'Where the film’s sound stops (the old take: 18 kHz)'],
            ['About 150 ms', 'To rise (the old take: 135 ms)'],
            ['+8.5 dB', 'Gain to reach the game’s level'],
            ['2.8 s', 'Length (the old take: 2.5 s)']]),
 dict(id='r12_dc_mortar_a', tag='Marine Corps film · 1.1 s', title='The depth-charge thrower? An 81 mm mortar firing',
  take='depth_charge_thrower_r12_dvids729278_1.wav', sound='depth_charge_thrower',
  where=WHERE_DC,
  analogy='Both a depth-charge thrower and a mortar lob a heavy charge with one bang from a short tube.',
  what='From a Marine Corps film of students firing M252 81 mm mortars (Lance Cpl. Drake Nickels, public domain). Of the 27 firings in the film, this one has the fewest overloaded samples, but it still overloads: the original is flattened near −2.5 to −3.4 dBFS for its first 35 to 50 ms, and again 80 to 95 ms after it starts. It rises in 4 ms and falls 20 dB in 0.2 s, with tones at 760 and 1,010 Hz in it. The talk that follows in the film is left out. The film’s background sits 60 dB under the shot.',
  clip='r12_dc_m252_a',
  marks=[[0, 'Old take'], [1.92, 'New take'], [1.96, 'The mortar fires']],
  extra=None,
  measured=[['4 ms', 'To rise (the old take: 190 ms)'],
            ['0.2 s', 'To fall 20 dB (the old take: 0.5 s)'],
            ['60 dB', 'How far the shot stands over the film’s background'],
            ['40%', 'Energy above 2 kHz (the old take: 19%)']]),
 dict(id='r12_dc_mortar_b', tag='Marine Corps film · 0.7 s', title='The depth-charge thrower? Another 81 mm firing',
  take='depth_charge_thrower_r12_dvids729278_2.wav', sound='depth_charge_thrower',
  where='The same place. If you like more than one of these three, say in a note which goes in.',
  analogy='The same kind of gun again, a shorter shot.',
  what='Another firing from the same film. It rises in 6 ms and stands 55 dB over the film’s background. It overloads too, for 45 to 70 ms at the start, and a thin gliding tone from 6.3 to 8 kHz runs from 0.37 to 0.53 s. The take stops before a chirp that follows in the film.',
  clip='r12_dc_m252_b',
  marks=[[0, 'Old take'], [1.92, 'New take'], [1.96, 'The mortar fires']],
  extra=None,
  measured=[['6 ms', 'To rise (the old take: 190 ms)'],
            ['0.24 s', 'To fall 20 dB (the old take: 0.5 s)'],
            ['55 dB', 'How far the shot stands over the film’s background'],
            ['0.7 s', 'Length (the old take: 0.9 s)']]),
 dict(id='r12_dc_close', tag='War Memorial tape · 1.9 s', title='The depth-charge thrower? A mortar fired close to the microphone',
  take='depth_charge_thrower_r12_awmS04685_3.wav', sound='depth_charge_thrower',
  where='The same place.',
  analogy='Like standing beside the gun instead of across the field: mostly a deep thud.',
  what='From an Australian War Memorial tape, “Mortar – operational sounds”, with a clink before each firing (a round dropped in, inference). Close to the microphone: 114 samples touch full scale between 0.13 and 0.79 s into the take, none for longer than 1 ms at a time. 86% of its energy is below 200 Hz. The rumble is still 8 dB above the background when the take ends, so its end is a made fade. The tape stops at 8.4 kHz. Licence: personal, non-commercial use only.',
  clip='r12_dc_awm_close',
  marks=[[0, 'Old take'], [1.92, 'New take'], [1.96, 'The mortar fires']],
  extra=('r12_dc_awm_close_source', 'The tape around it, 17 s: three clinks and firings; this take is the third firing, at 12.7 s'),
  measured=[['86%', 'Energy under 200 Hz (the old take: 20%)'],
            ['114', 'Samples at full scale, none held over 1 ms'],
            ['1.9 s', 'Length (the old take: 0.9 s)'],
            ['8.4 kHz', 'Where the tape stops (the old take: 18 kHz)']]),
 dict(id='r12_run_tank', tag='Hydrophone recording · loops every 9 s', title='A torpedo running? An outboard motor heard underwater',
  take='torpedo_run_loop_r12_wolfsetT16_1.wav', sound='torpedo_run_loop',
  where=WHERE_RUN,
  analogy='Like a small boat motor heard with your head under the water.',
  what='From Wolfset, a set of underwater recordings: an outboard the index calls “Motor 1” (the lead notes, unconfirmed, say a 4.5 hp Mercury, heard with a hydrophone 2.5 m down in the anechoic tank at the Lisbon naval base). It pulses about 11.8 times a second, with a tone at 23.5 Hz and mains hum at 50 Hz. Cut between air-rifle shots that are elsewhere in the recording. Licence still to confirm on the dataset’s page. A cleaner file of the same motor idling (5 minutes, no shots) is on the next list to fetch.',
  clip='r12_torp_wolfset_t16',
  marks=RUN_MARKS(28.07),
  extra=('r12_torp_wolfset_t16_source', 'The recording around it, 20 s from 36 s: the air-rifle shots at 3.6, 13.5 and 18.3 s are left out of the take'),
  measured=[['1.6 dB', 'How much the level wanders (the old take: 1.4)'],
            ['0.12 dB', 'Level jump where the loop comes round (the old take: 1.7)'],
            ['11.8 a second', 'Pulse rate (the old take’s strongest rhythm: 31 a second)'],
            ['11%', 'Energy under 40 Hz']]),
 dict(id='r12_run_whine', tag='Park Service hydrophone · loops every 12 s', title='A torpedo running? A small boat’s whine underwater',
  take='torpedo_run_loop_r12_nps06_1.wav', sound='torpedo_run_loop',
  where='The same place. If you like more than one of these four, say in a note which goes in.',
  analogy='Like a microphone fixed to the sea floor near a harbour mouth while a small motorboat works past.',
  what='From the National Park Service’s underwater recordings at Glacier Bay, Alaska (a hydrophone anchored near the mouth of the bay, according to the lead notes). A whine with tones at 961 to 984 Hz and a beat about 5 times a second (a shaft turning about 300 rpm, inference). It wanders more than the tank recording (5.4 dB against 1.6). Raised 10.6 dB to the game’s level. Licence still to confirm on the Park Service’s page.',
  clip='r12_torp_nps_whine',
  marks=RUN_MARKS(31.1, twice=False),
  extra=('r12_torp_nps_whine_source', 'The whole Park Service clip, 31 s: the take runs from 5.9 to 18 s'),
  measured=[['5.4 dB', 'How much the level wanders (the old take: 1.4)'],
            ['961–984 Hz', 'Where its whine sits'],
            ['0.41 dB', 'Level jump where the loop comes round (the old take: 1.7)'],
            ['+10.6 dB', 'Gain to reach the game’s level']]),
 dict(id='r12_run_prop', tag='Navy tape · loops every 9 s', title='A torpedo running? A big propeller through listening gear',
  take='torpedo_run_loop_r12_oxley10_1.wav', sound='torpedo_run_loop',
  where='The same place.',
  analogy='Like a big ship’s propeller churning the water, picked up through navy listening gear (whose ship, and from where, the Memorial’s page does not say).',
  what='From the HMAS Oxley tape again: a propeller heard on the tape’s listening segment, turning about 270 rpm (inference). Almost nothing above 2 kHz. It is the least steady of the four (10.7 dB), so its loop join needed a 1.5 s crossfade. Licence: personal, non-commercial use only.',
  clip='r12_torp_oxley_prop',
  marks=RUN_MARKS(28.01),
  extra=('r12_torp_oxley_prop_source', 'The tape around it, 17 s: the propeller segment starts at 4.8 s; the take runs from 6.5 s'),
  measured=[['10.7 dB', 'How much the level wanders (the old take: 1.4)'],
            ['0.1%', 'Energy above 2 kHz (the old take: none)'],
            ['About 270 rpm', 'Propeller speed, estimated'],
            ['1.5 s', 'Crossfade at the loop join']]),
 dict(id='r12_run_outboard', tag='Park Service hydrophone · loops every 9 s', title='A torpedo running? An outboard passing underwater',
  take='torpedo_run_loop_r12_nps07_1.wav', sound='torpedo_run_loop',
  where='The same place.',
  analogy='Like a speedboat heard from under the water: a high, fast buzz.',
  what='From the same Park Service recordings: an outboard engine whose tones at 73.4 Hz suggest about 4,400 rpm (inference). Nearer a torpedo’s size and speed than the other sources (inference: wartime torpedoes ran at roughly 30 to 45 knots, this boat about 20). Three-quarters of its energy is above 2 kHz. Clicks in it come round every 9 s with the loop. A low cut at 20 Hz takes out deep rumble with no tone in it (water flowing over the hydrophone, inference). Steadier than the whine (2.7 dB against 5.4). Licence still to confirm.',
  clip='r12_torp_nps_outboard',
  marks=RUN_MARKS(28.03),
  extra=('r12_torp_nps_outboard_source', 'The whole Park Service clip, 9.7 s: high clicks at 1.2 and 1.7 s, low thumps at 1.3 and 5.9 s'),
  measured=[['76%', 'Energy above 2 kHz (the old take: none)'],
            ['2.7 dB', 'How much the level wanders (the old take: 1.4)'],
            ['About 4,400 rpm', 'Engine speed, estimated'],
            ['0.34 dB', 'Level jump where the loop comes round (the old take: 1.7)']]),
]

def q(t):
    t = str(t); assert "'" not in t and '\\' not in t and '\n' not in t, t; return "'" + t + "'"
def num(v): return ('%g' % v)
def card_js(c):
    out = (" {id:%s, group:'r12', tag:%s, title:%s,\n  where:%s,\n  analogy:%s,\n  what:%s,\n  clip:%s,\n  marks:[%s],\n"
           % (q(c['id']), q(c['tag']), q(c['title']), q(c['where']), q(c['analogy']), q(c['what']), q(c['clip']),
              ', '.join('[%s, %s]' % (num(t), q(l)) for t, l in c['marks'])))
    if c['extra']: out += '  extra:{clip:%s, label:%s},\n' % (q(c['extra'][0]), q(c['extra'][1]))
    out += '  measured:[%s]},\n' % ','.join('[%s,%s]' % (q(v), q(k)) for v, k in c['measured'])
    return out

# 1. headline and lead
rep('<h1>New: rounds striking the plane</h1>', '<h1>New: real recordings for four old studio-library sounds</h1>')
rep('<p class="lead">The newest cards are at the bottom: two for rounds striking the plane you ride. Above them wait twelve for the squad around you on the ground and the far air battle rebuilt with the real B-17s. None of them is in the game yet.',
    '<p class="lead">The newest cards are at the bottom: twelve real recordings that could replace old studio-library takes for the Stuka’s engines, a torpedo’s launch and run, and the depth-charge thrower. Above them wait two for rounds striking the plane you ride, twelve for the squad around you on the ground, and the far air battle rebuilt with the real B-17s. None of them is in the game yet.')

# 2. the section, after round eleven's
rep('  <div id="hits"></div>\n',
    '  <div id="hits"></div>\n\n'
    '  <h2>Real recordings for four more old studio-library sounds</h2>\n'
    '  <p class="section-note">Round twelve, new tonight, from the leads your Mac fetched on 22 November. Each card plays the old studio-library take, 1 s of silence, then the new one, both at the game’s level; where there is a second player, it plays the recording the new take was cut from. '
    'Stamp Good the one you want for each sound; the old take stays until you do. '
    'Three sounds found nothing usable: the bomb’s fall (the B-52 film has clanks in the bomb bay but no fall or landing), the ship going down (the three films’ sound is aircraft or cabin noise, and the third overloads after 17 s), and the plane breaking up (NASA’s crash films have no sound). '
    'A second Bf 109 run-up was set aside because another engine runs through it. Some of these carry non-commercial licences, which suit your game as long as it is never sold.</p>\n'
    '  <div id="r12"></div>\n')

# 3. the cards, after round eleven's last
rep("(behind a fighter: 27 dB, and 8 dB)']]},\n];",
    "(behind a fighter: 27 dB, and 8 dB)']]},\n" + ''.join(card_js(c) for c in CARDS) + "];")

# 4. the level strips
names = [c['clip'] for c in CARDS] + [c['extra'][0] for c in CARDS if c['extra']]
envs = {n: env('%s/%s.mp3' % (clips_dir, n)) for n in names}
for n, e in envs.items():
    assert ('"%s":' % n) not in s, n
rep('const ENV = {', 'const ENV = {' + json.dumps(envs, separators=(',', ':'))[1:-1] + ',')

# 5. the credits
rep('      <li>Nothing comes from films, series or games.</li>',
    '      <li>Round twelve’s recordings were fetched on your Mac on 22 November: Fight2FlyPhoto’s “Messerschmitt Bf 109 Startup and Taxiing” on Freesound (CC BY-NC 3.0); the Australian War Memorial’s tapes S04697 (HMAS Oxley) and S04685 (a mortar), for personal, non-commercial use only; U.S. Navy films by MC2 Jeremy Graham and MC1 Alexandra Seeley and a Marine Corps film by Lance Cpl. Drake Nickels, on DVIDS (public domain); and the National Park Service’s Glacier Bay underwater recordings and the Wolfset dataset (licences still to confirm). Each take is cut, faded and levelled, and the loops are joined with a crossfade; one has a low cut at 20 Hz and one at 5 Hz. Nothing is synthesized.</li>\n'
    '      <li>Nothing comes from films, series or games.</li>')
n = s.count(" {id:'"); rep('<b id="t-all">42</b>', '<b id="t-all">%d</b>' % n)
open(dst, 'w', encoding='utf-8').write(s)
json.dump({'cards': CARDS, 'env_clips': names}, open(rj, 'w'), ensure_ascii=False, indent=1)
print('ok', len(s), 'cards', n, 'clips', len(names))
