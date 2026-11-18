"""
MAIN THEME: "Theater Command"

A hymn for brass and strings in B flat, 72 to the minute, forty bars, made to loop.

It is built on the game's own five-note call, F  B flat  C  A  D (the figure the sound team's calls are built
on): a rising fourth, a step, a dip to the leading note, and a leap of a fourth to the third of the key.
The theme is that figure, answered twice, the second time reaching higher.

  1-4    a far trumpet calls the figure over a low B flat and a soft drum roll
  5-12   the horns sing the theme; strings hold the harmony; a muffled snare keeps step
  13-20  the strings take the theme, cellos an octave under; the horns hold the chords
  21-28  the bridge, in G minor: the cost of it. Oboe and violins; the drums gather
  29-36  everyone: trumpets and violins on the theme, horns, trombones and tuba on the hymn, drums and cymbals
  37-40  the far trumpet again, over a darkened chord (E flat minor), and back to the start
"""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece
from engine import Track

P = Piece('theme', 72, seed=3, tempo=[(36, 0, 72, False), (37, 0, 66, True), (40, 4, 66, False)])
T = P.T
# a single trumpet, far away at the back of the hall
T['tpt_solo'] = Track('tpt_solo', T['tpt'].arts, pan=0.3, width=0.4, send=0.6, depth=1.0, players=1)

P.section(1, 'A far trumpet calls')
P.section(5, 'The horns sing the theme')
P.section(13, 'The strings take it')
P.section(21, 'The bridge, in G minor')
P.section(29, 'Everyone')
P.section(37, 'The far trumpet again')

# ---------------------------------------------------------------- harmony (checked against every note)
P.harmony(1, 'Bb5 | Bb5 | Bb . F/A . | Gm')
A = 'Bb | F Gm | Cm7 | F7sus4 F7 | Bb . . F/A | D/F# Gm7 | Cm7 F7 | Bb'
P.harmony(5, A)
P.harmony(13, A)
P.harmony(21, 'Gm | Ebmaj7 | Cm | D | Gm/Bb | Eb Cm | Cm7 . Bb/D F7 | F7sus4 F7')
P.harmony(29, A)
P.harmony(37, 'Bb | Eb/Bb . Ebm/Bb . | F/A Bb | Bb')

# ---------------------------------------------------------------- the tune
THEME = ('F4:1 | Bb4:3 C5:1 | A4:2 D5:2 | C5:1.5 Bb4:0.5 G4:1 Bb4:1 | F4:3 F4:1 | '
         'Bb4:3 C5:1 | A4:2 F5:2> | Eb5:1.5 D5:0.5 C5:1 A4:1 | Bb4:4')

# intro: the far trumpet
P.line('tpt_solo', 2, 'r:3 F4:1 | Bb4:2 C5:1 A4:1 | D5:4\\', vel='mp', legato=0.05)

# A: horns
P.line('hn', 4, 'r:3 ' + THEME, vel='mf', legato=0.12)
# A': strings, cellos an octave under
P.line('vln1', 12, 'r:3 ' + THEME, vel='f', legato=0.15)
P.line('vc', 12, 'r:3 ' + THEME, vel='mp', legato=0.15, octave=-1)
# A'': trumpets and violins, flute and oboe an octave over
P.line('tpt', 28, 'r:3 ' + THEME, vel='f', legato=0.08)
P.line('vln1', 28, 'r:3 ' + THEME, vel='f', legato=0.15, check=False)
P.line('vln2', 28, 'r:3 ' + THEME, vel='f', legato=0.15, check=False)
P.line('fl', 28, 'r:3 ' + THEME, vel='f', legato=0.1, octave=1)
P.line('ob', 28, 'r:3 ' + THEME, vel='mf', legato=0.1, octave=1)

# B: the bridge tune, violins and oboe
BRIDGE = ('D5:3 Eb5:1 | D5:2 Bb4:2 | C5:1.5 D5:0.5 Eb5:1 G5:1 | F#5:2 D5:2 | '
          'G5:3 F5:1 | Eb5:2 C5:2 | Bb4:1.5 C5:0.5 D5:1 Eb5:1 | F5:3>')
P.line('vln1', 21, BRIDGE, vel='mf', legato=0.15)
P.line('ob', 21, BRIDGE, vel='mp', legato=0.1)
P.line('cl', 25, 'G4:3 F4:1 | Eb4:2 C4:2 | Bb3:1.5 C4:0.5 D4:1 Eb4:1 | F4:3', vel='mp', legato=0.1)

# coda: the far trumpet
P.line('tpt_solo', 37, 'r:3 F4:1 | Bb4:3 C5:1 | A4:2 D5:2 | D5:4\\', vel='mp', legato=0.05)

# ---------------------------------------------------------------- the hymn under the tune
# five voices: three upper, tenor, bass, one chord per entry (beats, notes)
CHORALE = [
    (4, 'D4 Bb3 F3 D3 Bb1'),
    (2, 'C4 A3 F3 C3 A1'), (2, 'D4 Bb3 G3 D3 G1'),
    (4, 'Eb4 Bb3 G3 C3 C2'),
    (2, 'Eb4 Bb3 F3 C3 F1'), (2, 'Eb4 A3 F3 C3 F1'),
    (3, 'D4 Bb3 F3 D3 Bb1'), (1, 'C4 A3 F3 C3 A1'),
    (2, 'D4 A3 F#3 D3 F#1'), (2, 'D4 Bb3 F3 D3 G1'),
    (2, 'Eb4 Bb3 G3 C3 C2'), (2, 'Eb4 A3 F3 C3 F1'),
    (4, 'D4 Bb3 F3 D3 Bb1'),
]


def voice(i, shift=0):
    """One voice of the hymn as (beats, note) pairs, moved by octaves if asked."""
    from engine import note_number
    out = []
    names = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
    for beats, notes in CHORALE:
        k = note_number(notes.split()[i]) + 12 * shift
        out.append((beats, f'{names[k % 12]}{k // 12 - 1}'))
    return out


swell = [(0, -4), (0.5, 0), (1, -3)]
# A: strings hold the chords, softly
P.chords('vln2', 5, voice(0), vel='mp', curve=swell, overlap=0.1)
P.chords('vla', 5, voice(1), vel='mp', curve=swell, overlap=0.1)
P.chords('vc', 5, voice(4, 1), vel='mp', curve=swell, overlap=0.1)
P.chords('cb', 5, voice(4), vel='mp', curve=swell, overlap=0.1)
# A': horns hold the chords, two to a note; violas the tenor; basses and bassoon the bass
for i in range(3):
    P.chords('hn', 13, voice(i), vel='pp', curve=swell, overlap=0.1, players=2)
P.chords('vla', 13, voice(3, 1), vel='p', curve=swell, overlap=0.1)
P.chords('cb', 13, voice(4), vel='mp', curve=swell, overlap=0.1)
P.chords('bsn', 13, voice(4, 1), vel='p', curve=swell, overlap=0.1)
# A'': the brass hymn, full
for i in range(3):
    P.chords('hn', 29, voice(i), vel='mp', curve=[(0, -2), (0.4, 0), (1, -2)], overlap=0.08, players=2)
P.chords('tbn', 29, voice(2), vel='mp', overlap=0.08)
P.chords('tbn', 29, voice(3), vel='mp', overlap=0.08)
P.chords('tba', 29, voice(4), vel='mf', overlap=0.08)
P.chords('vla', 29, voice(0), vel='mf', overlap=0.1)
P.chords('vc', 29, voice(4, 1), vel='mf', overlap=0.1)
P.chords('cb', 29, voice(4), vel='mf', overlap=0.1)
P.chords('bsn', 29, voice(4, 1), vel='mf', overlap=0.1)

# B: the bridge's chords
BRIDGE_CH = [
    (4, 'G4 Bb3 G2 G1'), (4, 'G4 Bb3 Eb2 Eb1'), (4, 'G4 Eb4 C3 C2'), (4, 'A4 F#4 D3 D2'),
    (4, 'G4 D4 Bb2 Bb1'), (2, 'G4 Eb4 Eb3 Eb2'), (2, 'G4 Eb4 C3 C2'),
    (2, 'G4 Eb4 C3 C2'), (1, 'F4 D4 D3 D2'), (1, 'F4 C4 F2 F1'),
    (2, 'Bb4 Eb4 F2 F1'), (2, 'A4 Eb4 F2 F1'),
]
for desk, i in (('vln2', 0), ('vla', 1), ('vc', 2), ('cb', 3)):
    seq = [(b, n.split()[i]) for b, n in BRIDGE_CH]
    P.chords(desk, 21, seq, vel='mp', curve=[(0, -3), (0.5, 0), (1, -2)], overlap=0.1)
# horns come in under the second half of the bridge, and swell into the tutti
P.chords('hn', 25, [(4, 'D4'), (2, 'Eb4'), (2, 'Eb4'), (2, 'Eb4'), (1, 'D4'), (1, 'Eb4'), (2, 'Eb4'), (2, 'Eb4')], vel='p', overlap=0.1)
P.chords('hn', 25, [(4, 'Bb3'), (2, 'Bb3'), (2, 'G3'), (2, 'G3'), (1, 'F3'), (1, 'A3'), (2, 'Bb3'), (2, 'A3')], vel='p', overlap=0.1)
P.chords('hn', 28, [(4, 'F3')], vel='f', curve=[(0, -10), (1, 0)])
P.chords('tbn', 27, [(4, 'C3'), (4, 'C3')], vel='mf', curve=[(0, -12), (1, 0)])
P.chords('tbn', 28, [(4, 'F2')], vel='mf', curve=[(0, -12), (1, 0)])
P.chords('tba', 28, [(4, 'F1')], vel='mf', curve=[(0, -12), (1, 0)])

# ---------------------------------------------------------------- the intro and the coda
P.chords('cb', 1, [(16, 'Bb1')], vel='p', curve=[(0, -10), (0.3, 0), (0.8, -2), (1, -4)])
P.chords('vc', 1, [(16, 'Bb2')], vel='p', curve=[(0, -10), (0.3, 0), (0.8, -2), (1, -4)])
P.chords('vc', 1, [(16, 'F3')], vel='pp', curve=[(0, -10), (0.3, 0), (1, -3)])
P.chords('vln2', 1, [(16, 'F5')], vel='pp', art='trem', curve=[(0, -14), (0.5, -4), (1, -8)])
P.chords('vla', 3, [(2, 'D4'), (2, 'C4'), (4, 'D4')], vel='pp', curve=[(0, -8), (0.6, 0), (1, -4)], overlap=0.1)

CODA = [(4, 'F4 D4 Bb2 Bb1'), (2, 'G4 Eb4 Bb2 Bb1'), (2, 'Gb4 Eb4 Bb2 Bb1'),
        (2, 'F4 C4 A2 A1'), (2, 'F4 D4 Bb2 Bb1'), (4, 'F4 D4 Bb2 Bb1')]
for desk, i in (('vln2', 0), ('vla', 1), ('vc', 2), ('cb', 3)):
    seq = [(b, n.split()[i]) for b, n in CODA]
    P.chords(desk, 37, seq[:-1], vel='mp', curve=[(0, -2), (1, -5)], overlap=0.1)
    P.chords(desk, 40, seq[-1:], vel='p', curve=[(0, 0), (1, -16)])
P.chords('hn', 38, [(2, 'Bb3'), (2, 'Bb3'), (2, 'A3'), (2, 'Bb3')], vel='p', overlap=0.1, players=2)

# ---------------------------------------------------------------- drums
P.T['timp'].add(P.at(1), 8, 46, 54, 'roll', curve=[(0, -16), (0.55, 0), (1, -14)])
P.T['timp'].add(P.at(3, 2), 6, 41, 30, 'roll', curve=[(0, -16), (0.6, -2), (1, -12)])
for bar, key, v in ((5, 46, 52), (9, 46, 48), (12, 46, 50), (13, 46, 56), (17, 46, 52), (20, 46, 50)):
    P.hit('timp', bar, 0, key, v)
P.T['timp'].add(P.at(27), 8, 41, 70, 'roll', curve=[(0, -18), (0.85, 0), (1, 0)])
for bar, key in ((29, 46), (30, 41), (31, 48), (32, 41), (33, 46), (34, 42), (35, 48), (36, 46)):
    P.hit('timp', bar, 0, key, 88)
P.hit('timp', 30, 2, 43, 76); P.hit('timp', 34, 2, 43, 76); P.hit('timp', 35, 2, 41, 82)
P.T['timp'].add(P.at(39), 8, 46, 26, 'roll', curve=[(0, -18), (0.5, -6), (1, -20)])

slow = 'X... x.x. X... R--x'
P.drums('snare', 3, slow, accent=46, normal=32, ghost=22, roll_vel=30, bars=2)
P.drums('snare', 5, slow, accent=56, normal=40, ghost=26, roll_vel=36, bars=8)
P.drums('snare', 13, slow, accent=48, normal=34, ghost=22, roll_vel=30, bars=8)
P.T['snare'].add(P.at(27), 8, 60, 70, 'roll', curve=[(0, -24), (0.9, 0), (1, 0)])
march = 'X.xx X.x. X.xx R--X'
P.drums('snare', 29, march, accent=92, normal=70, ghost=46, roll_vel=74, bars=7)
P.drums('snare', 36, 'X... ..x. X... ....', accent=96, normal=70)
P.drums('snare', 37, slow, accent=40, normal=28, ghost=20, roll_vel=26, bars=1)

# a bowed cymbal swells into the tutti: its loudest moment lands on the downbeat of bar 29
P.hit('cym', 29, 0, 60, 80, art='swell', dur=3.9 * 72 / 60, nudge=-3.7, release=0.25)
P.hit('cym', 29, 0, 60, 112, art='crash')
P.hit('cym', 33, 0, 60, 96, art='crash')
P.hit('cym', 36, 0, 60, 100, art='sus')
P.hit('bd', 29, 0, 60, 96); P.hit('bd', 33, 0, 60, 88); P.hit('bd', 36, 0, 60, 92)
P.hit('cym', 21, 0, 60, 40, art='gong')

BARS = 40

if __name__ == '__main__':
    import json
    out = sys.argv[1] if len(sys.argv) > 1 else '/home/user/score/out'
    if P.warnings:
        print('\n'.join(P.warnings))
    path, info = P.render(BARS, out, loop=True, tail=7.0)
    print(json.dumps({k: v for k, v in info.items() if k != 'warnings'}, indent=1))
from engine import save_loudness; save_loudness()
