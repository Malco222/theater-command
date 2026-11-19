"""
TENSE: "Standing To"

For a country at war, on the campaign map between battles: the line is manned and waiting. G minor,
92 to the minute, forty-eight bars, made to loop.

Low strings drive a restless figure grouped three, three and two; a timpani beats under it like a pulse.
The horns carry the five-note call turned minor (D  G  A  F#  B flat): the same figure as the main theme,
darkened. It gathers to a climax and falls back to the pulse alone, ending on the chord that leads into
its own beginning.

  1-8    the pulse: cellos and basses, timpani, a high tremolo
  9-16   the horns: the call, in minor
  17-24  violins climb over it; a snare starts to tap
  25-32  muted trumpets, the call again, gathering
  33-40  the climax: everyone, the bass drum, the violins at the top
  41-48  back to the pulse, waiting
"""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece, chord_tones

P = Piece('standing_to', 92, seed=21)
T = P.T
for b, l in ((1, 'The pulse'), (9, 'The horns: the call in minor'), (17, 'Violins climb'), (25, 'Muted trumpets, gathering'),
             (33, 'The climax'), (41, 'Back to the pulse')):
    P.section(b, l)

CALL_H = 'Gm | D7 . . Gm | Eb | Cm | Gm | D7 . . Am7b5 | Gm/Bb . . D | D7'
P.harmony(1, 'Gm | Gm | Gm | Gm | Gm | Eb/G | Cm/G | D/G')
P.harmony(9, CALL_H)
P.harmony(17, 'Eb | Cm6 | Am7b5 | D7 | Gm | Eb | Cm | D7sus4 D7')
P.harmony(25, CALL_H)
P.harmony(33, CALL_H)
P.harmony(41, 'Gm | Eb | Cm | D | Gm | Eb/G | Cm/G | D')

# ---------------------------------------------------------------- the pulse
dyn = [(1, 8, 'p'), (9, 8, 'p'), (17, 8, 'p'), (25, 8, 'mp'), (33, 8, 'mp'), (41, 8, 'p')]
for bar, n, v in dyn:
    P.ostinato('vc', bar, n, vel=v, low=38, accent=10)
    P.ostinato('cb', bar, n, vel=v, low=38, octave=-1, accent=10)
P.ostinato('vla', 25, 16, vel='p', low=50, accent=10)
P.ostinato('bsn', 33, 8, vel='mp', low=38, art='stac', accent=10)

# timpani: a pulse on the first and last of the three groups
for bar in range(1, 49):
    v = 40 if bar < 9 else 52 if bar < 25 else 66 if bar < 33 else 84 if bar < 41 else 40
    for beat in (0, 3):
        sym = P.chord_at(P.at(bar, beat))
        from score import chord_tones
        k = chord_tones(sym, 38, 1)[0]
        P.hit('timp', bar, beat, k, v - (8 if beat else 0))

# a high tremolo, moving by step with the harmony
P.chords('vln2', 1, [(16, 'D5'), (8, 'D5'), (4, 'Eb5'), (4, 'D5')], vel='pp', art='trem', curve=[(0, -12), (0.5, 0), (1, -4)])
P.chords('vln2', 41, [(16, 'D5'), (4, 'D5'), (4, 'Eb5'), (4, 'Eb5'), (4, 'D5')], vel='pp', art='trem', curve=[(0, -6), (0.6, 0), (1, -6)])
P.chords('vla', 5, [(4, 'Bb3'), (4, 'Bb3'), (4, 'C4'), (4, 'A3')], vel='p', curve=[(0, -8), (0.6, 0), (1, -3)], overlap=0.1)

# ---------------------------------------------------------------- the call, in minor
CALL = 'r:2 D4:1 G4:1 | A4:2 F#4:1 Bb4:1 | Bb4:3 A4:1 | G4:4 | r:2 D4:1 G4:1 | A4:2 F#4:1 C5:1 | Bb4:3 A4:1 | D4:4>'
P.line('hn', 9, CALL, vel='mf', legato=0.1)
P.chords('vln2', 9, [(4, 'D5'), (2, 'C5'), (2, 'D5'), (4, 'Eb5'), (4, 'Eb5'), (4, 'D5'), (2, 'C5'), (2, 'C5'), (2, 'D5'), (2, 'D5'), (4, 'C5')],
         vel='p', art='trem', curve=[(0, -6), (0.5, 0), (1, -4)], overlap=0.05)
P.chords('tbn', 13, [(4, 'Bb2'), (4, 'A2'), (4, 'Bb2'), (4, 'A2')], vel='p', curve=[(0, -12), (0.7, 0), (1, -4)])

# ---------------------------------------------------------------- the climb
P.line('vln1', 17, 'G4:4 | A4:4 | C5:4 | C5:2 A4:2 | Bb4:4 | G4:2 Bb4:2 | Eb5:4 | D5:4>', vel='mf', legato=0.15)
P.line('cl', 17, 'Bb3:4 | C4:4 | Eb4:4 | F#4:4 | G4:4 | Eb4:4 | G4:4 | G4:2 F#4:2', vel='mp', legato=0.1)
P.chords('tbn', 21, [(4, 'Bb2'), (4, 'G2'), (4, 'G2'), (2, 'A2'), (2, 'A2')], vel='mp', curve=[(0, -10), (0.7, 0), (1, -3)])
P.chords('tba', 21, [(4, 'G1'), (4, 'Eb2'), (4, 'C2'), (4, 'D2')], vel='mp', curve=[(0, -10), (0.7, 0), (1, -3)])
P.drums('snare', 17, 'g.g. g.g. g.g. g.gg', accent=40, normal=34, ghost=26, bars=8)

# ---------------------------------------------------------------- gathering: muted trumpets and the call
P.line('hn', 25, CALL, vel='f', legato=0.1)
P.line('vln1', 25, CALL, vel='mf', legato=0.12, octave=1)
for bar in range(25, 33):
    for beat in (0, 1.5, 3):
        sym = P.chord_at(P.at(bar, beat))
        ts = chord_tones(sym, 55, 3)
        for k in ts[1:3]:
            T['tpt'].add(P.at(bar, beat), 0.4, k, 70 if beat == 0 else 58, 'mute', release=0.15)
P.chords('tbn', 29, [(4, 'Bb2'), (4, 'A2'), (4, 'Bb2'), (4, 'A2')], vel='mf', curve=[(0, -10), (0.7, 0), (1, -3)])
P.chords('tba', 29, [(4, 'G1'), (4, 'D2'), (4, 'G1'), (4, 'D2')], vel='mf', curve=[(0, -10), (0.7, 0), (1, -3)])
P.drums('snare', 25, 'g.gg g.gg g.gg x.gg', accent=60, normal=50, ghost=36, bars=6)
P.drums('snare', 31, 'x.xx x.xx x.xx x.xx', accent=72, normal=62, ghost=40, bars=1)
P.T['snare'].add(P.at(32), 4, 60, 76, 'roll', curve=[(0, -18), (1, 0)])
P.T['timp'].add(P.at(32), 4, 38, 70, 'roll', curve=[(0, -16), (1, 0)])
P.hit('cym', 33, 0, 60, 80, art='swell', dur=3.9 * 92 / 60, nudge=-3.7, release=0.25)

# ---------------------------------------------------------------- the climax
DESCANT = 'Bb5:4 | A5:3 G5:1 | G5:2 Bb5:2 | C6:4 | Bb5:4 | A5:2 F#5:1 Eb5:1 | D5:3 F#5:1 | A5:4>'
P.line('vln1', 33, DESCANT, vel='f', legato=0.15)
P.line('vln2', 33, DESCANT, vel='f', legato=0.15, octave=-1, check=False)
P.line('hn', 33, CALL.replace('D4:4>', 'D4:4'), vel='f', legato=0.1)
P.line('tpt', 33, 'r:2 D5:1 G5:1 | A5:2 F#5:1 Bb5:1 | Bb5:3 A5:1 | G5:4 | r:2 D5:1 G5:1 | A5:2 F#5:1 C6:1 | Bb5:3 A5:1 | D5:4>',
       vel='mf', legato=0.06)
CLIMAX_LOW = [(4, 'D3 Bb2 G1'), (3, 'C3 F#2 D2'), (1, 'D3 Bb2 G1'), (4, 'Eb3 Bb2 Eb2'), (4, 'Eb3 G2 C2'), (4, 'D3 Bb2 G1'),
              (3, 'C3 F#2 D2'), (1, 'Eb3 C3 A1'), (3, 'D3 G2 Bb1'), (1, 'D3 F#2 D2'), (4, 'C3 A2 D2')]
P.chords('tbn', 33, [(b, n.split()[0]) for b, n in CLIMAX_LOW], vel='mf', overlap=0.05)
P.chords('tbn', 33, [(b, n.split()[1]) for b, n in CLIMAX_LOW], vel='mf', overlap=0.05)
P.chords('tba', 33, [(b, n.split()[2]) for b, n in CLIMAX_LOW], vel='mf', overlap=0.05)
P.drums('snare', 33, 'X.xx x.xx X.xx R--x', accent=88, normal=66, ghost=44, roll_vel=70, bars=8)
for bar in (33, 35, 37, 39):
    P.hit('bd', bar, 0, 60, 90)
P.hit('cym', 33, 0, 60, 96, art='crash')
P.hit('cym', 37, 0, 60, 84, art='crash')
P.hit('cym', 33, 0, 60, 70, art='gong')

# ---------------------------------------------------------------- waiting
P.line('hn', 43, 'r:2 C4:1 Eb4:1 | D4:4\\ | r:2 D4:1 G4:1 | G4:4\\ | r:4 | r:2 D4:1 F#4:1>', vel='p', legato=0.1)
P.chords('vla', 45, [(4, 'Bb3'), (4, 'Bb3'), (4, 'C4'), (4, 'A3')], vel='pp', curve=[(0, -8), (0.6, 0), (1, -3)], overlap=0.1)

BARS = 48

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '/home/user/score/out'
    if P.warnings:
        print('\n'.join(P.warnings))
    path, info = P.render(BARS, out, loop=True, tail=6.0, lufs=-16.0)
    from engine import save_loudness; save_loudness()
    print('\n'.join(info['balance'])); print('limiter', info['limiter_db'])
