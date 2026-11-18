"""A check, not a piece: the opening of Brian's "The Final Salute" as read from his recording (a transcriber's
note picture, read bar by bar), played back by the sampler so the two can be compared bar by bar.
Bars 1-8: the low figure in octaves over an F drone. Bars 9-16: the tune above, with its chords."""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece

# 5 1 7 b6 5 in F minor, then its answer, which turns up through a diminished chord to F.
FIG_A = 'C4:2 F4:2 | E4:2 Db4:2 | C4:4 | C4:4'
FIG_B = 'C4:2 F4:2 | E4:2 Db4:2 | C4:4 | C4:4'
# The tune (bar 8 beat 4 pickup is left out: bar 9 starts on the long C).
TUNE = 'C5:3 r:1 | Ab4:1.5 F4:1.5 Db5:1 | Db5:4 | Db5:1 A4:0.5 Bb4:2.5 | Bb4:1 r:2 Ab4:1 | Ab4:0.5 G4:1.5 Eb4:1 C5:1 | C5:4 | C5:1 E4:1 F4:2'
HARM_B = 'Fm | Fm | Db | Db . Eb . | Eb . Bb7 . | Cm/Eb | C | C7 . F .'


def build():
    P = Piece('salute_check', 120, seed=5)
    P.harmony(9, HARM_B)
    fig = FIG_A + ' | ' + FIG_B
    for desk in ('vc', 'vla', 'hn'):
        P.line(desk, 1, fig, vel='mf', legato=0.1)
    P.line('bsn', 5, 'r:4 | r:2 G3:2 | F3:4 | F3:4', vel='mf', legato=0.1)
    P.line('cb', 1, 'F2:4 | ' * 8 + 'F2:4 | F2:4 | Db2:4 | Db2:1 Eb2:3 | Eb2:2 Bb1:2 | Eb2:4 | C2:4 | C2:2 F2:2', vel='mf', legato=0.1)
    P.line('vln1', 9, TUNE, vel='mf', legato=0.1)
    pads = [(4, 'C4 Ab3'), (4, 'C4 Ab3'), (4, 'Ab3 F3'), (1, 'Ab3 F3'), (3, 'G3 Eb3'), (2, 'G3 Eb3'), (2, 'Ab3 D3'),
            (4, 'G3 C3'), (4, 'G3 E3'), (2, 'Bb3 E3'), (2, 'A3 F3')]
    P.chords('vln2', 9, [(b, n.split()[0]) for b, n in pads], vel='p', overlap=0.1)
    P.chords('vla', 9, [(b, n.split()[1]) for b, n in pads], vel='p', overlap=0.1)
    return P


if __name__ == '__main__':
    P = build()
    print('\n'.join(P.warnings) or 'no warnings')
    path, info = P.render(16, sys.argv[1], loop=False, tail=2.0, lufs=-18.0)
    print(path)
