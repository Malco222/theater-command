"""The five-note call alone, for the listening page: a trumpet over a held B flat chord."""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece
from engine import Track
P = Piece('the_call', 72, seed=5)
T = P.T
T['tpt_solo'] = Track('tpt_solo', T['tpt'].arts, pan=0.1, width=0.4, send=0.45, depth=0.6, players=1)
P.harmony(1, 'Bb | Bb . F/A . | Gm')
P.line('tpt_solo', 1, 'r:3 F4:1 | Bb4:2 C5:1 A4:1 | D5:4\\', vel='mf', legato=0.05)
P.chords('vc', 1, [(12, 'Bb2')], vel='p', curve=[(0, -12), (0.25, 0), (1, -10)])
P.chords('cb', 1, [(12, 'Bb1')], vel='p', curve=[(0, -12), (0.25, 0), (1, -10)])
P.chords('vla', 1, [(12, 'F3')], vel='pp', curve=[(0, -12), (0.25, 0), (1, -10)])
P.section(1, 'The call')
if __name__ == '__main__':
    out = sys.argv[1]
    path, info = P.render(3, out, loop=False, tail=3.0, lufs=-18.0)
    print(info['seconds'], info['lufs'], info['warnings'])
