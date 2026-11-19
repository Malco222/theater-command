"""
STINGER: "The Field Is Ours" (battle won)

Plays once when a battle is won, over the battle sounds fading (battle-mix.md section 6: the end). F major,
100 to the minute slowing to 80, five bars, about 13 seconds with its ring.

A drum roll gathers on the dominant. Then the horns, with trumpets an octave above, play the main theme's
low call (Brian's "The Final Salute": C, F, E, D flat, C) turned to the major, on dotted beats: C, up to F,
down to E and D, then up instead of back down. The tune's long D flat, which sighs to C in the main theme,
here climbs: D flat to E flat to F, over the chords of D flat and E flat into F major, everyone at full
strength, the timpani rolling under the last chord. It is a stinger, not a loop.

  1  drums gather on C
  2  the call, in the major: C, F
  3  E, D
  4  the long D flat climbs to E flat, slowing
  5  F major, everyone
"""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece

P = Piece('battle_won', 100, seed=41, tempo=[(4, 0, 100, False), (5, 0, 80, True)])
T = P.T
T['tbn'].spread_ms = 6; T['tpt'].spread_ms = 5; T['hn'].spread_ms = 8   # the brass strike together
for b, l in ((1, 'Drums gather'), (2, 'The call, in the major'), (4, 'The long D flat climbs'), (5, 'F major')):
    P.section(b, l)

P.harmony(1, 'C | F | C/E . Bb . | Db . Eb . | F')

# ---------------------------------------------------------------- bar 1: the drums gather on C
P.T['timp'].add(P.at(1), 4, 'C3', 92, 'roll', curve=[(0, -20), (0.85, 0), (1, 0)])
P.T['snare'].add(P.at(1, 1), 3, 60, 84, 'roll', curve=[(0, -22), (1, 0)])
P.hit('cym', 2, 0, 60, 100, art='swell', dur=3.0 * 60 / 100, nudge=-2.9, release=0.2)
P.chords('vc', 1, [(4, 'C3')], vel='mf', art='trem', curve=[(0, -18), (1, 0)])
P.chords('vla', 1, [(4, 'G3')], vel='mf', art='trem', curve=[(0, -18), (1, 0)])
P.chords('vln2', 1, [(4, 'E4')], vel='mf', art='trem', curve=[(0, -18), (1, 0)])
P.chords('vln1', 1, [(4, 'G4')], vel='mf', art='trem', curve=[(0, -18), (1, 0)])
P.chords('tbn', 1, [(4, 'C3')], vel='mp', curve=[(0, -20), (1, 0)])
P.chords('cb', 1, [(4, 'C2')], vel='mf', art='trem', curve=[(0, -18), (1, 0)])

# ---------------------------------------------------------------- bars 2 to 4: the call in the major
CALL = 'C4:1.5 C4:0.5 F4:2 | E4:1.5 E4:0.5 D4:2 | <f> F4:2 G4:2 | <ff> A4:4'
P.line('hn', 2, CALL, vel='ff', legato=0.06)
P.line('tpt', 2, 'C5:1.5 C5:0.5 F5:2 | E5:1.5 E5:0.5 D5:2 | <ff> Db5:2 Eb5:2 | F5:4', vel='f', legato=0.04)
P.line('vln1', 2, 'A5:3 C6:1 | G5:2 F5:2 | Ab5:2 Bb5:2 | <ff> C6:4', vel='f', legato=0.12)
P.line('vln2', 2, 'F5:4 | E5:2 D5:2 | F5:2 G5:2 | <ff> A5:4', vel='f', legato=0.12)
P.chords('vla', 2, [(4, 'C4'), (2, 'C4'), (2, 'D4'), (2, 'Ab4'), (2, 'Bb4'), (4, 'C5')], vel='f', overlap=0.08)
LOW = [(4, 'F2'), (2, 'E2'), (2, 'Bb1'), (2, 'Db2'), (2, 'Eb2'), (4, 'F1')]
P.chords('cb', 2, LOW, vel='f', overlap=0.06)
P.chords('vc', 2, [(4, 'F3'), (2, 'C3'), (2, 'D3'), (2, 'Ab3'), (2, 'Bb3'), (4, 'F3')], vel='f', overlap=0.06)
P.chords('tba', 2, [(4, 'F1'), (2, 'E2'), (2, 'Bb1'), (2, 'Db2'), (2, 'Eb2'), (4, 'F1')], vel='f', overlap=0.05)
P.chords('tbn', 2, [(4, 'C3'), (2, 'C3'), (2, 'F3'), (2, 'F3'), (2, 'G3'), (4, 'C3', 'f')], vel='mf', overlap=0.05)
P.chords('tbn', 2, [(4, 'A3'), (2, 'G3'), (2, 'D3'), (2, 'Ab3'), (2, 'Bb3'), (4, 'A3', 'f')], vel='mf', overlap=0.05)
P.chords('bsn', 2, [(4, 'F2'), (2, 'E2'), (2, 'Bb2'), (2, 'Db3'), (2, 'Eb3'), (4, 'F2')], vel='mf', overlap=0.05)

# the drums: each chord struck, a roll into the last
P.hit('cym', 2, 0, 60, 112, art='crash')
P.hit('bd', 2, 0, 60, 110)
P.hit('timp', 2, 0, 'F2', 96)
P.hit('timp', 2, 3, 'C3', 80)
P.hit('timp', 2, 3.5, 'C3', 84)
P.hit('timp', 3, 0, 'C3', 88)
P.hit('timp', 3, 2, 'F2', 80)
P.hit('bd', 3, 0, 60, 84)
P.hit('timp', 4, 0, 'C3', 96)
P.hit('timp', 4, 2, 'Eb3', 100)
P.hit('bd', 4, 0, 60, 90)
P.hit('bd', 4, 2, 60, 96)
P.drums('snare', 2, 'X..x X.xx X... x.xx', accent=104, normal=80, ghost=50, bars=2)
P.T['snare'].add(P.at(4), 4, 60, 96, 'roll', curve=[(0, -10), (1, 0)])

# ---------------------------------------------------------------- bar 5: F major, everyone
P.hit('cym', 5, 0, 60, 124, art='crash')
P.hit('cym', 5, 0, 60, 96, art='gong')
P.hit('bd', 5, 0, 60, 124)
P.hit('timp', 5, 0, 'F2', 118)
P.T['timp'].add(P.at(5, 0.25), 3.5, 'F2', 96, 'roll', curve=[(0, -4), (0.7, 0), (1, -12)])

BARS = 5

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '/home/user/score/out'
    if P.warnings:
        print('\n'.join(P.warnings))
    path, info = P.render(BARS, out, loop=False, tail=4.5, lufs=-14.0)
    from engine import save_loudness; save_loudness()
    print('\n'.join(info['balance'])); print('limiter', info['limiter_db'], 'seconds', info['seconds'])
