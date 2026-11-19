"""
BATTLE TENSION: "Zero Hour"

For the minutes before a battle: the armies deployed, waiting for the first shot (the battle's music
"before contact", battle-mix.md section 6). A minor, 84 to the minute, thirty-two bars, about a minute and a
half, made to loop. When contact comes the battle pieces take over (The Advance is in D minor, and this
waits on A, its dominant).

A slow heartbeat on the bass drum and timpani; cellos and basses tick a quiet figure grouped three, three
and two; violins hold a thin high tremolo. Muted horns, trombones an octave below, play the main theme's
low call (Brian's "The Final Salute": in his key C, F, E, D flat; here E, A, G sharp, F). Then the cellos
take the head of the main theme's tune, the open horns answer with its close, and the whole orchestra
strikes four chords climbing to E, the drums hammering, before everything drops away to the heartbeat
and a clarinet plays the call once more, low, waiting.

  1-8    the heartbeat; strings tick, a high tremolo
  9-16   muted horns: the main theme's call
  17-20  cellos: the head of the main theme's tune
  21-24  open horns: its close, violins climbing
  25-28  four chords, the drums hammering
  29-32  back to the heartbeat; the call, low, in the clarinet
"""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece
from engine import Articulation

P = Piece('zero_hour', 84, seed=51)
T = P.T
mute = Articulation('horn mute', 'FHornMute.sfz', release=0.6, attack=0.04, lead=0.04)
mute.target = -24.0
T['hn'].arts['mute'] = mute
for b, l in ((1, 'The heartbeat'), (9, 'Muted horns: the main theme\'s call'), (17, 'Cellos: the head of the tune'),
             (21, 'Open horns: its close'), (25, 'Four chords, the drums hammering'), (29, 'The heartbeat again; the call, low')):
    P.section(b, l)

P.harmony(1, 'Am | Am | F/A | Am | Am | F/A | Dm/A | E/A')
P.harmony(9, 'Am | E7b9/G# | Am | Am | Am | E7b9/G# | Am | E')
P.harmony(17, 'Am | F | F . G . | C . Em/G .')
P.harmony(21, 'Am/E | F | Am . Bm7b5 . | E . E7 .')
P.harmony(25, 'F | Dm | Bb | E')
P.harmony(29, 'Am | E7b9/G# | Am | E')

# ---------------------------------------------------------------- the ticking figure
OST = ('R', 'R', '8', 'R', 'R', '8', '5', 'R')
for bar, n, v in ((1, 8, 'pp'), (9, 8, 'p'), (17, 8, 'p'), (25, 4, 'mf'), (29, 4, 'pp')):
    P.ostinato('vc', bar, n, pattern=OST, vel=v, low=45, accent=10)
    P.ostinato('cb', bar, n, pattern=OST, vel=v, low=33, accent=10)
P.ostinato('vla', 17, 8, pattern=('8', '5', '3', '8', '5', '3', '5', '3'), vel='pp', low=57, accent=8)
P.ostinato('vla', 25, 4, pattern=('8', '5', '3', '8', '5', '3', '5', '3'), vel='mp', low=57, accent=10)
P.ostinato('bsn', 25, 4, pattern=OST, vel='mf', low=40, art='stac', accent=10)

# ---------------------------------------------------------------- the heartbeat
for bar in list(range(1, 25)) + list(range(29, 33)):
    lub = 46 if bar < 9 else 50 if bar < 17 else 54 if bar < 25 else 44
    for beat in (0, 2):
        P.hit('bd', bar, beat, 60, lub)
        P.hit('timp', bar, beat + 0.5, 'A2', lub - 8)

# ---------------------------------------------------------------- the high tremolo
P.chords('vln1', 1, [(16, 'E5'), (8, 'E5'), (4, 'F5'), (4, 'E5')], vel='p', art='trem', curve=[(0, -14), (0.6, 0), (1, -6)])
P.chords('vln2', 9, [(4, 'A4'), (4, 'G#4'), (8, 'A4'), (4, 'A4'), (4, 'G#4'), (4, 'A4'), (4, 'G#4')],
         vel='pp', art='trem', curve=[(0, -8), (0.5, 0), (1, -4)], overlap=0.05)
P.chords('vln1', 29, [(16, 'E5')], vel='pp', art='trem', curve=[(0, -4), (0.5, -8), (1, -14)])
for bar in (8, 16, 24):
    P.hit('cym', bar + 1, 0, 60, 54, art='swell', dur=3.9 * 60 / 84, nudge=-3.7, release=0.25)

# ---------------------------------------------------------------- the call, on muted horns
CALL = 'E4:2 A4:2 | G#4:2 F4:2 | E4:4 | E4:4 | E4:2 A4:2 | G#4:2 F4:2 | E4:4 | E4:2 G#4:2>'
P.line('hn', 9, CALL, vel='f', art='mute', legato=0.1)
P.line('tbn', 9, CALL, vel='pp', legato=0.1, octave=-1)
P.line('hn', 14, 'r:2 B3:2 | A3:4', vel='mp', art='mute', legato=0.1)

# ---------------------------------------------------------------- the tune: its head in the cellos, its close in the horns
P.line('vc', 17, 'E3:3 C3:0.5 A2:0.5 | F3:4 | F3:1 C#3:0.5 D3:2.5 | C3:1 B2:1 G2:1 E3:1', vel='mf', legato=0.15)
P.chords('cl', 17, [(4, 'C4'), (4, 'C4'), (2, 'A3'), (2, 'B3'), (2, 'G3'), (2, 'G3')], vel='pp', overlap=0.1)
P.line('hn', 21, 'E4:3 C4:0.5 A3:0.5 | F4:4 | E4:2 D4:1 B3:1 | E4:2 G#3:2>', vel='f', legato=0.1)
P.chords('vln2', 21, [(4, 'C5'), (4, 'C5'), (2, 'C5'), (2, 'D5'), (2, 'E5'), (2, 'D5')], vel='p', art='trem',
         curve=[(0, -6), (0.6, 0), (1, -2)], overlap=0.05)
P.chords('vln1', 21, [(4, 'A5'), (4, 'A5'), (2, 'A5'), (2, 'B5'), (2, 'B5'), (2, 'B5')], vel='pp', art='trem',
         curve=[(0, -10), (0.7, 0), (1, 0)], overlap=0.05)
P.chords('tbn', 21, [(4, 'A2'), (4, 'A2'), (2, 'A2'), (2, 'B2'), (4, 'B2')], vel='p', curve=[(0, -10), (0.7, 0), (1, -3)])
P.chords('tba', 21, [(4, 'E2'), (4, 'F2'), (2, 'A1'), (2, 'B1'), (4, 'E2')], vel='p', curve=[(0, -10), (0.7, 0), (1, -3)])
P.drums('snare', 21, 'g.g. g.g. g.g. g.gg', accent=44, normal=36, ghost=28, bars=4)
P.T['timp'].add(P.at(24), 4, 'E2', 70, 'roll', curve=[(0, -16), (1, 0)])

# ---------------------------------------------------------------- four chords, the drums hammering
STABS = [('F', 'F2 C3 A3', 'A4 C5', 'A5', 'F2'), ('Dm', 'D2 A2 F3', 'A4 D5', 'A5', 'D2'),
         ('Bb', 'Bb1 F2 D3', 'Bb4 D5', 'Bb5', 'Bb2'), ('E', 'E2 B2 G#3', 'B4 E5', 'B5', 'E2')]
for i, (sym, low, horns, top, timp) in enumerate(STABS):
    bar = 25 + i
    for beat, v in ((0, 'mf'), (1.5, 'mp'), (3, 'mf')):
        P.chords('tbn', bar, [(1.2, ' '.join(low.split()[1:]))], vel=v, beat=beat)
        P.chords('tba', bar, [(1.2, low.split()[0])], vel=v, beat=beat)
        P.chords('hn', bar, [(1.2, horns)], vel=v, beat=beat)
        P.hit('timp', bar, beat, timp, 90 if v == 'mf' else 76)
    P.chords('vln1', bar, [(4, top)], vel='f', art='trem', overlap=0.05)
    P.hit('bd', bar, 0, 60, 86)
P.chords('vln2', 25, [(4, 'C5'), (4, 'D5'), (4, 'D5'), (4, 'E5')], vel='mf', art='trem', overlap=0.05)
P.drums('snare', 25, 'X.xX .xX. X.xX .xxx', accent=96, normal=72, ghost=46, bars=3)
P.T['snare'].add(P.at(28), 4, 60, 92, 'roll', curve=[(0, -12), (1, 0)])
P.hit('cym', 25, 0, 60, 100, art='crash')
P.hit('cym', 25, 0, 60, 76, art='gong')
P.hit('cym', 27, 0, 60, 88, art='crash')

# ---------------------------------------------------------------- back to the heartbeat; the call, low
P.line('cl', 29, 'E3:2 A3:2 | G#3:2 F3:2 | E3:4 | r:2 E3:1 G#3:1>', vel='mf', legato=0.1)

BARS = 32

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '/home/user/score/out'
    if P.warnings:
        print('\n'.join(P.warnings))
    path, info = P.render(BARS, out, loop=True, tail=6.0, lufs=-17.0)
    from engine import save_loudness; save_loudness()
    print('\n'.join(info['balance'])); print('limiter', info['limiter_db'], 'seconds', info['seconds'])
