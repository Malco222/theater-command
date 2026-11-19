"""
CAMPAIGN MAP: "The Long Front"

For the campaign map at war, taking turns with Brian's "The Silent Front" and Standing To: the player
looking over a front that runs across a continent. C minor, 66 to the minute, forty bars, about two and a
half minutes, made to loop. Slower and wider than Standing To, with no drum pulse.

A harp ripples over low strings. The cellos play the main theme's tune (Brian's "The Final Salute": up to
the fifth, down through the third to home, up to a long flat sixth that sighs back to the fifth) in C
minor, low and warm; then the horns take it an octave higher, the violins answering. The music opens out
into E flat major and the whole orchestra plays the tune broad, turned to the major as in The Map Room,
with its long note now on a bright C flat chord. Then it settles back into C minor: a clarinet and an oboe
play the main theme's low call, quietly, and the harp brings it round to the start.

  1-4    first light: harp and low strings
  5-12   the cellos: the main theme's tune, in C minor
  13-20  the horns take it, the violins answering
  21-28  the whole front: the tune broad, in E flat major
  29-36  back to the line: the call in the clarinet, then the oboe
  37-40  last light: harp and low strings
"""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece
from engine import note_number

P = Piece('long_front', 66, seed=61)
T = P.T
for b, l in ((1, 'First light'), (5, 'The cellos: the main theme\'s tune'), (13, 'The horns take it'),
             (21, 'The whole front: the tune in E flat major'), (29, 'Back to the line: the call'), (37, 'Last light')):
    P.section(b, l)

TUNE_H = 'Cm | Ab | Ab . Bb . | Eb . Gm/Bb . | Cm/G | Ab | Cm . Dm7b5 . | G . G7 .'
MAJOR_H = 'Eb | Cb | Bb7sus4 . Bb7 . | Eb | Eb | Ab | Eb . Bb7 . | Bb7sus4 . Bb7 .'
CALL_H = 'Cm | G7b9/B | Cm | Cm | Cm | G7b9/B | Cm | G'
P.harmony(1, 'Cm | Ab | Fm | G')
P.harmony(5, TUNE_H)
P.harmony(13, TUNE_H)
P.harmony(21, MAJOR_H)
P.harmony(29, CALL_H)
P.harmony(37, 'Cm | Ab | Fm | G')

# the main theme's tune in C minor (Standing To's, a fifth lower), and turned to E flat major (The Map Room's,
# a tone lower)
TUNE = ('G3:3 Eb3:0.5 C3:0.5 | Ab3:4 | Ab3:1 E3:0.5 F3:2.5 | Eb3:1 D3:1 Bb2:1 G3:1 | '
        'G3:3 Eb3:0.5 C3:0.5 | Ab3:4 | G3:2 F3:1 D3:1 | G3:2 B2:2>')
MAJOR = ('Bb4:3 G4:0.5 Eb4:0.5 | Cb5:4 | Bb4:2 Ab4:1 F4:1 | G4:4 | '
         'Bb4:3 G4:0.5 Eb4:0.5 | C5:4 | Bb4:1 G4:1 Ab4:1 F4:1 | Eb4:2 D4:2\\')


def pad(bar, seq, vel='p', vel_bass=None, upper=('vln2', 'vla'), curve=None, cello=True, basses=True):
    """seq: (beats, 'upper upper bass'); the cellos take the bass an octave up, the basses as written."""
    names = ['C', 'Db', 'D', 'Eb', 'E', 'F', 'F#', 'G', 'Ab', 'A', 'Bb', 'B']
    up = lambda n, s: (lambda k: f'{names[k % 12]}{k // 12 - 1}')(note_number(n) + s)
    cv = curve or [(0, -5), (0.5, 0), (1, -3)]
    for i, desk in enumerate(upper):
        P.chords(desk, bar, [(b, n.split()[i]) for b, n in seq], vel=vel, curve=cv, overlap=0.12)
    if cello:
        P.chords('vc', bar, [(b, up(n.split()[2], 12)) for b, n in seq], vel=vel_bass or vel, curve=cv, overlap=0.12)
    if basses:
        P.chords('cb', bar, [(b, n.split()[2]) for b, n in seq], vel=vel_bass or vel, curve=cv, overlap=0.12)


INTRO = [(4, 'G4 Eb4 C2'), (4, 'Ab4 C4 Ab1'), (4, 'Ab4 C4 F1'), (4, 'G4 B3 G1')]
PAD_T = [(4, 'Eb4 G3 C2'), (4, 'Eb4 C4 Ab1'), (2, 'Eb4 C4 Ab1'), (2, 'D4 F3 Bb1'), (2, 'Eb4 G3 Eb2'), (2, 'D4 G3 Bb1'),
         (4, 'Eb4 C4 G1'), (4, 'Eb4 C4 Ab1'), (2, 'Eb4 G3 C2'), (2, 'D4 Ab3 D2'), (2, 'D4 B3 G1'), (2, 'D4 F3 G1')]

# ---------------------------------------------------------------- first light
pad(1, INTRO, vel='p', curve=[(0, -10), (0.6, 0), (1, -3)])
P.arp('harp', 1, 4, step=1.0, pattern=(0, 2, 3, 4), vel='pp', low=36)

# ---------------------------------------------------------------- the cellos: the tune
P.line('vc', 5, TUNE, vel='mf', legato=0.15)
pad(5, PAD_T, vel='pp', vel_bass='p', cello=False)
P.arp('harp', 5, 8, step=1.0, pattern=(0, 2, 3, 4, 3, 2), vel=26, low=36)
P.chords('cl', 9, [(4, 'C4'), (4, 'C4'), (2, 'C4'), (2, 'D4'), (4, 'D4')], vel='pp', curve=[(0, -8), (0.6, 0), (1, -3)], overlap=0.1)

# ---------------------------------------------------------------- the horns take it
P.line('hn', 13, TUNE, vel='mf', legato=0.1, octave=1)
P.line('vln1', 14, 'r:2 C5:2 | C5:4 | r:2 D5:2 | Eb5:4 | r:2 Eb5:2 | C5:2 F5:2 | D5:4>', vel='p', legato=0.15)
pad(13, PAD_T, vel='pp', vel_bass='p', cello=True, basses=False)
for bar, (n1, n2) in zip(range(13, 21), [('C2', 'G2'), ('Ab1', 'Eb2'), ('Ab1', 'Bb1'), ('Eb2', 'Bb1'),
                                         ('G1', 'C2'), ('Ab1', 'C2'), ('C2', 'D2'), ('G1', 'G1')]):
    P.hit('cb', bar, 0, n1, 52, art='pizz')
    P.hit('cb', bar, 2, n2, 44, art='pizz')
P.T['timp'].add(P.at(20), 4, 'G2', 40, 'roll', curve=[(0, -18), (1, -2)])
P.hit('cym', 21, 0, 60, 70, art='swell', dur=3.9 * 60 / 66, nudge=-3.7, release=0.3)

# ---------------------------------------------------------------- the whole front: E flat major
P.line('vln1', 21, MAJOR, vel='f', legato=0.15, octave=1)
P.line('hn', 21, MAJOR, vel='f', legato=0.1)
P.line('vln2', 21, MAJOR, vel='mf', legato=0.15, check=False)
PAD_M = [(4, 'G4 Bb3 Eb2'), (4, 'Gb4 Eb4 Cb2'), (2, 'F4 Eb4 Bb1'), (2, 'F4 D4 Bb1'), (4, 'G4 Bb3 Eb2'),
         (4, 'G4 Bb3 Eb2'), (4, 'Ab4 C4 Ab1'), (2, 'G4 Bb3 Eb2'), (2, 'F4 D4 Bb1'), (2, 'F4 Eb4 Bb1'), (2, 'F4 D4 Bb1')]
pad(21, PAD_M, vel='mf', upper=('vla', 'cl'))
P.chords('tbn', 21, [(4, 'Bb2'), (4, 'Gb3'), (2, 'F3'), (2, 'F3'), (4, 'G3'), (4, 'G3'), (4, 'Ab3'), (2, 'G3'), (2, 'F3'),
                     (2, 'F3'), (2, 'F3')], vel='mp', curve=[(0, -6), (0.5, 0), (1, -3)], overlap=0.05)
P.chords('tba', 21, [(4, 'Eb2'), (4, 'Cb2'), (4, 'Bb1'), (4, 'Eb2'), (4, 'Eb2'), (4, 'Ab1'), (2, 'Eb2'), (2, 'Bb1'), (4, 'Bb1')],
         vel='mp', curve=[(0, -6), (0.5, 0), (1, -3)], overlap=0.05)
P.arp('harp', 21, 8, step=0.5, vel=34, low=43)
for bar in (21, 25):
    P.hit('timp', bar, 0, 'Eb2', 70)
P.hit('timp', 22, 0, 'Cb3', 66)
P.hit('timp', 27, 2, 'Bb2', 60)
P.T['timp'].add(P.at(28), 4, 'Bb2', 46, 'roll', curve=[(0, -4), (1, -16)])

# ---------------------------------------------------------------- back to the line: the call
P.line('cl', 29, 'G3:2 C4:2 | B3:2 Ab3:2 | G3:4 | G3:4\\', vel='mp', legato=0.1)
P.line('ob', 33, 'G4:2 C5:2 | B4:2 Ab4:2 | G4:4 | G4:2 B4:2\\', vel='mp', legato=0.1)
pad(29, [(4, 'Eb4 G3 C2'), (4, 'D4 F3 B1'), (8, 'Eb4 G3 C2'), (4, 'Eb4 G3 C2'), (4, 'D4 F3 B1'), (4, 'Eb4 G3 C2'),
         (4, 'D4 B3 G1')], vel='pp', curve=[(0, -6), (0.5, 0), (1, -3)])
P.arp('harp', 29, 8, step=1.0, pattern=(0, 2, 3, 4), vel=24, low=36)

# ---------------------------------------------------------------- last light
pad(37, INTRO, vel='p', curve=[(0, -4), (0.6, 0), (1, -6)])
P.arp('harp', 37, 4, step=1.0, pattern=(0, 2, 3, 4), vel='pp', low=36)
P.line('fl', 39, 'C5:2 Ab4:2 | G4:2 B4:2\\', vel='pp', legato=0.1)

BARS = 40

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '/home/user/score/out'
    if P.warnings:
        print('\n'.join(P.warnings))
    path, info = P.render(BARS, out, loop=True, tail=7.0, lufs=-17.0)
    from engine import save_loudness; save_loudness()
    print('\n'.join(info['balance'])); print('limiter', info['limiter_db'], 'seconds', info['seconds'])
