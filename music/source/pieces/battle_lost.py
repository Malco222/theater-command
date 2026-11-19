"""
STINGER: "The Line Gives Way" (battle lost)

Plays once when a battle is lost, over the battle sounds fading (battle-mix.md section 6: the end). F minor,
72 to the minute slowing to 54, three bars and a last plucked note, about 15 seconds with its ring.

A soft stroke of the tam-tam over a low F. One horn plays the main theme's low call (Brian's "The Final
Salute": C, F, E, D flat) slowly, the trombones an octave under it, and stops on C without going home.
The violins come in on the main theme's long D flat and let it sigh down to C, as in Brian's song, over a
chord that does not rest. The basses close it with one plucked F. No drums but the timpani, softly.

  1  the call, slowly: C, F, E, D flat
  2  the horn holds C; the violins enter on D flat
  3  the D flat sighs to C
"""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece

P = Piece('battle_lost', 72, seed=43, tempo=[(2, 0, 72, False), (3, 2, 54, True)])
T = P.T
T['hn'].players = 1        # one horn: a voice, not a section
for b, l in ((1, 'The call, slowly'), (2, 'The long D flat'), (3, 'It sighs to C')):
    P.section(b, l)

P.harmony(1, 'Fm . C/E Db')
P.harmony(2, 'Fm/C . Db .')
P.harmony(3, 'Fm/C')
P.harmony(4, 'Fm')

# ---------------------------------------------------------------- bar 1: the call, slowly, over a low F
P.hit('cym', 1, 0, 60, 70, art='gong')
P.T['timp'].add(P.at(1), 4, 'F2', 46, 'roll', curve=[(0, -4), (0.4, -10), (1, -16)])
P.chords('cb', 1, [(4, 'F1'), (2, 'C2'), (2, 'Db2'), (4, 'C2')], vel='p', curve=[(0, -3), (0.6, 0), (1, -4)], overlap=0.1)
P.chords('vc', 1, [(4, 'F2'), (2, 'C3'), (2, 'Db3'), (4, 'C3')], vel='p', curve=[(0, -6), (0.6, 0), (1, -4)], overlap=0.1)
P.line('hn', 1, 'C4:1.5 F4:0.5 E4:1 Db4:1 | C4:4\\', vel='mf', legato=0.12)
P.line('tbn', 1, 'C3:1.5 F3:0.5 E3:1 Db3:1 | C3:4\\', vel='p', legato=0.12)
P.chords('vla', 1, [(2, 'Ab3'), (1, 'G3'), (1, 'F3'), (2, 'Ab3'), (2, 'Ab3'), (4, 'Ab3')], vel='mp',
         curve=[(0, -6), (0.5, 0), (1, -3)], overlap=0.1)
P.chords('vln2', 1, [(2, 'C4'), (1, 'C4'), (1, 'Ab3'), (2, 'F4'), (2, 'F4'), (4, 'F4')], vel='mp',
         curve=[(0, -6), (0.5, 0), (1, -3)], overlap=0.1)

# ---------------------------------------------------------------- bars 2 and 3: the D flat sighs to C
P.line('vln1', 2, 'r:2 Db5:2> | C5:4\\', vel='mf', legato=0.2)
P.line('cl', 2, 'r:2 F4:2 | Eb4:2 C4:2\\', vel='p', legato=0.1, check=False)
P.T['timp'].add(P.at(3), 3, 'C3', 34, 'roll', curve=[(0, -6), (1, -18)])

# ---------------------------------------------------------------- the end: one plucked F
P.hit('cb', 4, 0, 'F1', 60, art='pizz')
P.hit('vc', 4, 0, 'F2', 48, art='pizz')
P.hit('timp', 4, 0, 'F2', 40)

BARS = 3

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '/home/user/score/out'
    if P.warnings:
        print('\n'.join(P.warnings))
    path, info = P.render(BARS, out, loop=False, tail=4.5, lufs=-17.0)
    from engine import save_loudness; save_loudness()
    print('\n'.join(info['balance'])); print('limiter', info['limiter_db'], 'seconds', info['seconds'])
