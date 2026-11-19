"""
CALM CAMPAIGN MAP: "The Map Room"

For a country at peace, while the player plans. F major, 66 to the minute, forty bars, made to loop.
Quiet strings, a harp turning slowly, and the same five-note call heard gently by a solo voice each time:
the oboe, then the clarinet in D minor, then the horn, then the violins. There is no drum in it.

  1-4    strings and harp alone
  5-12   the oboe sings the call and lets it fall
  13-20  the clarinet answers in D minor, darker, over plucked basses
  21-28  the horn, warm and low
  29-36  the violins, the clarinet beneath them
  37-40  a flute, a borrowed minor chord, and home
"""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece

P = Piece('map_room', 66, seed=11, tempo=[(36, 0, 66, False), (37, 0, 60, True), (40, 4, 60, False)])
T = P.T
T['hn'].players = 1      # one horn: a voice, not a section
P.section(1, 'Strings and harp')
P.section(5, 'The oboe')
P.section(13, 'The clarinet, in D minor')
P.section(21, 'The horn')
P.section(29, 'The violins')
P.section(37, 'A flute, and home')

A = 'F | C/E Am | Dm7 | Bbmaj7 | Am7 | Gm7 . . C7 | Dm7 . . C | F'
P.harmony(1, 'F | Dm7 | Bbmaj7 | C7sus4 C7')
P.harmony(5, A)
P.harmony(13, 'Dm | A7 . . Dm | Bb . . C | Dm | Bbmaj7 | C | Am7 Dm | Gm7 C7sus4')
P.harmony(21, 'F | C/E | Dm7 | Bbmaj7 | Gm7 F/A | Bb . . F/C | C7sus4 C7 | F')
P.harmony(29, A)
P.harmony(37, 'F | Bbm/F | F | F')

TUNE = 'r:2 C5:1 F5:1 | G5:2 E5:1 A5:1 | A5:3 G5:1 | F5:2 D5:2 | E5:3 C5:1 | D5:2 Bb4:1 C5:1 | A4:3 G4:1 | F4:4\\'
P.line('ob', 5, TUNE, vel='mf', legato=0.1)
P.line('cl', 13, 'r:2 A3:1 D4:1 | E4:2 C#4:1 F4:1 | F4:3 E4:1 | D4:4 | F4:2 A4:1 Bb4:1 | C5:2 G4:2 | A4:2 F4:2 | G4:2 F4:2',
       vel='mf', legato=0.1)
P.line('hn', 21, 'C4:2 F4:2 | G4:3 E4:1 | A4:3 G4:1 | F4:2 D4:2 | Bb4:2 A4:1 G4:1 | F4:2 D4:1 C4:1 | F4:2 E4:2 | F4:4\\',
       vel='p', legato=0.12)
P.line('vln1', 29, TUNE, vel='mf', legato=0.15)
P.line('cl', 29, TUNE, vel='p', legato=0.1, octave=-1)
P.line('fl', 37, 'r:2 C5:1 F5:1 | F5:2 Db5:2 | C5:4\\', vel='p', legato=0.1)


def pad(bar, seq, vel='p', vel_bass=None, upper=('vln2', 'vla'), curve=None):
    """seq: (beats, 'upper upper bass'); the cellos take the bass an octave up, the basses as written."""
    from engine import note_number
    names = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
    up = lambda n, s: (lambda k: f'{names[k % 12]}{k // 12 - 1}')(note_number(n) + s)
    cv = curve or [(0, -5), (0.5, 0), (1, -3)]
    for i, desk in enumerate(upper):
        P.chords(desk, bar, [(b, n.split()[i]) for b, n in seq], vel=vel, curve=cv, overlap=0.12)
    P.chords('vc', bar, [(b, up(n.split()[2], 12)) for b, n in seq], vel=vel_bass or vel, curve=cv, overlap=0.12)
    P.chords('cb', bar, [(b, n.split()[2]) for b, n in seq], vel=vel_bass or vel, curve=cv, overlap=0.12)


pad(1, [(4, 'A4 C4 F2')], vel='p', curve=[(0, -16), (0.8, -1), (1, -2)])
pad(2, [(4, 'A4 C4 D2'), (4, 'A4 D4 Bb1'), (2, 'Bb4 F4 C2'), (2, 'Bb4 E4 C2')], vel='p')
PAD_A = [(4, 'A4 C4 F2'), (2, 'G4 C4 E2'), (2, 'A4 C4 A1'), (4, 'A4 C4 D2'), (4, 'A4 D4 Bb1'), (4, 'G4 C4 A1'),
         (3, 'F4 Bb3 G1'), (1, 'E4 Bb3 C2'), (3, 'F4 A3 D2'), (1, 'E4 G3 C2'), (4, 'F4 A3 F1')]
pad(5, PAD_A, vel='p')
pad(13, [(4, 'F4 A3 D2'), (3, 'E4 C#4 A1'), (1, 'F4 D4 D2'), (3, 'F4 D4 Bb1'), (1, 'E4 C4 C2'), (4, 'F4 A3 D2'),
         (4, 'F4 A3 Bb1'), (4, 'E4 G3 C2'), (2, 'E4 G3 A1'), (2, 'F4 A3 D2'), (2, 'F4 Bb3 G1'), (2, 'F4 Bb3 C2')],
    vel='pp', vel_bass='p')
pad(21, [(4, 'C5 A3 F1'), (4, 'C5 G3 E1'), (4, 'C5 A3 D2'), (4, 'D5 A3 Bb1'), (2, 'D5 Bb3 G1'), (2, 'C5 A3 A1'),
         (3, 'D5 Bb3 Bb1'), (1, 'C5 A3 C2'), (2, 'C5 Bb3 C2'), (2, 'C5 Bb3 C2'), (4, 'C5 A3 F1')], vel='pp', vel_bass='p')
pad(29, PAD_A, vel='p', upper=('vln2', 'vla'))
pad(37, [(4, 'A4 C4 F2'), (4, 'Bb4 Db4 F2'), (4, 'A4 C4 F2')], vel='p')
pad(40, [(4, 'A4 C4 F2')], vel='p', curve=[(0, 0), (1, -9)])

# the harp: a slow ripple, quavers under the oboe and the violins, crotchets elsewhere
P.arp('harp', 1, 4, step=1.0, pattern=(0, 2, 3, 4), vel='pp', low=36)
P.arp('harp', 5, 8, step=0.5, vel=28, low=36)
P.arp('harp', 21, 8, step=1.0, pattern=(0, 2, 3, 4), vel='pp', low=36)
P.arp('harp', 29, 8, step=0.5, vel=28, low=36)
P.arp('harp', 37, 3, step=1.0, pattern=(0, 2, 3, 4), vel=28, low=36)
P.hit('harp', 40, 0, 'F2', 40, art='pluck', dur=4)
P.hit('harp', 40, 0, 'C3', 34, art='pluck', dur=4)
P.hit('harp', 40, 0, 'A3', 34, art='pluck', dur=4)

# under the clarinet: plucked basses on the strong beats
for bar, notes in zip(range(13, 21), ['D2', 'A1', 'Bb1', 'D2', 'Bb1', 'C2', 'A1', 'G1']):
    P.hit('cb', bar, 0, notes, 56, art='pizz')
for bar, notes in zip(range(13, 21), ['A2', 'E2', 'F2', 'A2', 'F2', 'G2', 'E2', 'D2']):
    P.hit('vc', bar, 2, notes, 46, art='pizz')
P.chords('bsn', 17, [(4, 'F3'), (4, 'E3'), (2, 'E3'), (2, 'F3'), (2, 'F3'), (2, 'F3')], vel='p', overlap=0.1)
# a soft roll leads into the violins
P.T['timp'].add(P.at(28, 0), 4, 41, 30, 'roll', curve=[(0, -18), (0.8, -4), (1, -10)])

BARS = 40

if __name__ == '__main__':
    import json
    out = sys.argv[1] if len(sys.argv) > 1 else '/home/user/score/out'
    if P.warnings:
        print('\n'.join(P.warnings))
    path, info = P.render(BARS, out, loop=True, tail=7.0, lufs=-18.0)
    from engine import save_loudness; save_loudness()
    print('\n'.join(info['balance'])); print('limiter', info['limiter_db'])
