"""A check, not a piece: Brian's opening eight bars of "The Long Walk Home" as read from his recording, played
back by the sampler so the two can be compared bar by bar (the notes were read by a transcriber, not heard)."""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece

WALK_A = 'F4:3 Bb4:0.5 C5:0.5 | Db5:4 | Eb5:3 Bb4:0.5 Gb4:0.5 | F4:4'
WALK_B = 'F4:3 Bb4:0.5 C5:0.5 | Db5:1.5 C5:0.5 D5:2 | Bb4:0.5 G4:0.5 A4:0.5 Bb4:0.5 C5:0.5 Eb5:0.5 D5:0.5 Db5:0.5 | Bb4:2 A4:2'
HARM = 'Bbm | Bbm | Cb/Gb | Bb/D . F7 . | Bbm | Gb . Bb/F . | Eb | Bbm/F . F .'


def build(melody_a=WALK_A, melody_b=WALK_B, harm=HARM, name='walk_check'):
    P = Piece(name, 75.2, seed=3)
    P.harmony(1, harm)
    P.line('vln1', 1, melody_a + ' | ' + melody_b, vel='mf', legato=0.1)
    P.line('hn', 1, melody_a + ' | ' + melody_b, vel='mp', legato=0.1)
    pads = [(8, 'Db4 F3 Bb1'), (4, 'Cb4 Eb3 Gb1'), (2, 'Bb3 F3 D2'), (2, 'A3 Eb3 F1'),
            (4, 'Db4 F3 Bb1'), (2, 'Db4 Bb3 Gb1'), (2, 'D4 Bb3 F1'), (4, 'Bb3 G3 Eb2'), (2, 'Db4 Bb3 F1'), (2, 'C4 A3 F1')]
    from engine import note_number
    for i, desk in enumerate(('vln2', 'vla')):
        P.chords(desk, 1, [(b, n.split()[i]) for b, n in pads], vel='p', overlap=0.1)
    P.chords('vc', 1, [(b, n.split()[2][:-1] + str(int(n.split()[2][-1]) + 1)) for b, n in pads], vel='p', overlap=0.1)
    P.chords('cb', 1, [(b, n.split()[2]) for b, n in pads], vel='p', overlap=0.1)
    return P


if __name__ == '__main__':
    P = build()
    print('\n'.join(P.warnings) or 'no warnings')
    path, info = P.render(8, sys.argv[1], loop=False, tail=3.0, lufs=-18.0)
    print(path)
