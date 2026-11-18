"""
BATTLE: "The Advance"

For a battle. D minor, 132 to the minute, fifty-two bars, made to loop.

Cellos and basses drive a figure grouped three, three and two from the first bar; a snare drum and the
timpani keep it moving. The horns sound the five-note call in D minor (A  D  E  C#  F), hammered into a
battle theme. In the middle the trumpets break into the main theme's call in its own key, B flat major,
as if the line were holding; it is turned back into D minor, and the whole orchestra takes the battle
theme at full strength before the figure winds down to begin again.

  1-4    drums gather, the strings start their figure
  5-12   the horns: the battle theme
  13-20  trumpets and violins take it; the horns answer
  21-28  woodwinds and horns toss pieces of it about; the strings tremble
  29-36  the trumpets: the main theme's call in B flat, the line holding
  37-44  everyone: the battle theme at full strength
  45-52  the figure winds down, the brass still striking
"""
import sys
sys.path.insert(0, '/home/user/score')
from score import Piece, chord_tones

P = Piece('advance', 132, seed=31)
T = P.T
T['tbn'].spread_ms = 5; T['tpt'].spread_ms = 4; T['hn'].spread_ms = 8   # quick music: the brass strike together
for b, l in ((1, 'Drums gather'), (5, 'The horns: the battle theme'), (13, 'Trumpets and violins take it'),
             (21, 'Pieces of it, tossed about'), (29, 'The line holds: the call in B flat'), (37, 'Everyone'),
             (45, 'Winding down')):
    P.section(b, l)

THEME_H = 'Dm . A7 Dm | A | Dm . A7 . | Dm . A7 . | F . . C | Dm | Gm . . A7 | A'
P.harmony(1, 'Dm | Dm | Bb | A')
P.harmony(5, THEME_H)
P.harmony(13, THEME_H.replace('| A', '| A7') if False else THEME_H)
P.harmony(21, 'Bb | Gm | Eb | A | Bb | Gm | Em7b5 | A7')
P.harmony(29, 'Bb | F Gm | Cm7 | F7 | Bb | D/F# Gm7 | Cm7 F7 | A')
P.harmony(37, THEME_H)
P.harmony(45, 'Dm | Bb | Gm | A | Dm | Bb | Gm A | A7')

THEME = 'D4:1.5 E4:0.5 C#4:1 F4:1 | E4:3 A3:1 | D4:1.5 E4:0.5 C#4:1 G4:1 | F4:2 E4:1 D4:1 | A4:1.5 G4:0.5 F4:1 E4:1 | F4:2 D4:2 | Bb4:1.5 A4:0.5 G4:1 E4:1 | A4:4>'
PICK = 'r:3.5 A3:0.5 | '

# ---------------------------------------------------------------- the figure
OST = ('R', 'R', '8', 'R', 'R', '8', '5', '3')
for bar, n, v in ((1, 4, 'p'), (5, 8, 'mp'), (13, 8, 'mp'), (21, 8, 'p'), (29, 8, 'mp'), (37, 8, 'mf'), (45, 8, 'mp')):
    P.ostinato('vc', bar, n, pattern=OST, vel=v, low=38, accent=10)
    P.ostinato('cb', bar, n, pattern=OST, vel=v, low=38, octave=-1, accent=10)
for bar, n, v in ((13, 8, 'p'), (21, 8, 'pp'), (29, 16, 'p'), (45, 8, 'p')):
    P.ostinato('vla', bar, n, pattern=OST, vel=v, low=50, accent=10)
for bar, n, v in ((13, 8, 'mp'), (29, 16, 'mp')):
    P.ostinato('vln2', bar, n, pattern=('R', '5', '8', 'R', '5', '8', '5', '3'), vel=v, low=62, accent=8)
P.ostinato('cl', 21, 8, pattern=('8', '5', '3', '8', '5', '3', '5', '8'), vel='mp', low=62, art='stac', accent=8)
P.ostinato('fl', 25, 4, pattern=('8', '5', '3', '8', '5', '3', '5', '8'), vel='mp', low=74, art='stac', accent=8)
P.ostinato('bsn', 37, 8, pattern=OST, vel='mf', low=38, art='stac', accent=10)


def stabs(track, bar, bars, vel, low, n=2, beats=(0, 1.5, 3), art='stac', top=None):
    """Brass striking the chord on the figure's accents."""
    for b in range(bar, bar + bars):
        for beat in beats:
            sym = P.chord_at(P.at(b, beat))
            ts = chord_tones(sym, low, 4)
            picks = ts[:n] if top is None else ts[1:1 + n]
            for k in picks:
                T[track].add(P.at(b, beat), 0.45, k, vel + (10 if beat == 0 else 0), art)


# ---------------------------------------------------------------- the intro
P.T['timp'].add(P.at(1), 8, 38, 70, 'roll', curve=[(0, -16), (0.9, 0), (1, 0)])
P.T['snare'].add(P.at(3), 8, 60, 80, 'roll', curve=[(0, -20), (1, 0)])
P.hit('cym', 5, 0, 60, 80, art='swell', dur=3.9 * 132 / 60, nudge=-3.7, release=0.25)
stabs('tbn', 1, 4, 62, 38, beats=(0,))
stabs('tba', 1, 4, 62, 29, n=1, beats=(0,))
P.chords('hn', 3, [(4, 'F3 A3 D4'.split()[0]), (4, 'E3')], vel='mf', curve=[(0, -10), (1, 0)])
P.chords('hn', 3, [(4, 'Bb3'), (4, 'A3')], vel='mf', curve=[(0, -10), (1, 0)])
P.chords('hn', 3, [(4, 'D4'), (4, 'C#4')], vel='mf', curve=[(0, -10), (1, 0)])

# ---------------------------------------------------------------- 5-12: the horns
P.line('hn', 4, PICK + THEME, vel='f', legato=0.06)
stabs('tbn', 5, 8, 60, 38)
stabs('tba', 5, 8, 62, 29, n=1)
P.chords('vln1', 5, [(8, 'A4'), (8, 'A4'), (4, 'A4'), (4, 'A4'), (4, 'Bb4'), (4, 'C#5')], vel='mp', art='trem', curve=[(0, -6), (0.6, 0), (1, -2)])
P.chords('vln2', 5, [(8, 'F4'), (8, 'F4'), (4, 'F4'), (4, 'F4'), (4, 'G4'), (4, 'E4')], vel='mp', art='trem', curve=[(0, -6), (0.6, 0), (1, -2)])

# ---------------------------------------------------------------- 13-20: trumpets and violins
P.line('tpt', 12, PICK + THEME, vel='f', legato=0.05, octave=1)
P.line('vln1', 12, PICK + THEME, vel='f', legato=0.1, octave=1, check=False)
ANSWER = [(4, 'A3'), (4, 'A3'), (4, 'A3'), (4, 'A3'), (2, 'C4'), (2, 'C4'), (4, 'A3'), (3, 'Bb3'), (1, 'A3'), (4, 'C#4')]
P.chords('hn', 13, ANSWER, vel='mf', curve=[(0, -4), (0.5, 0), (1, -2)], overlap=0.05)
P.chords('hn', 13, [(2, 'F3'), (1, 'E3'), (1, 'F3'), (4, 'E3'), (2, 'F3'), (2, 'E3'), (2, 'F3'), (2, 'E3'), (3, 'F3'), (1, 'E3'), (4, 'F3'), (3, 'G3'), (1, 'E3'), (4, 'E3')],
         vel='mf', curve=[(0, -4), (0.5, 0), (1, -2)], overlap=0.05)
stabs('tbn', 13, 8, 66, 38)
stabs('tba', 13, 8, 66, 29, n=1)

# ---------------------------------------------------------------- 21-28: pieces of it
P.line('hn', 21, 'D4:1.5 F4:0.5 Bb4:2 | Bb4:1.5 A4:0.5 G4:2 | G4:1.5 F4:0.5 Eb4:2 | E4:1.5 C#4:0.5 A3:2 | '
       'D4:1.5 F4:0.5 Bb4:2 | Bb4:1.5 C5:0.5 D5:2 | E4:1 G4:1 Bb4:1 D5:1 | C#5:4>', vel='mf', legato=0.06)
P.line('ob', 21, 'r:2 D5:1.5 F5:0.5 | r:2 Bb4:1.5 A4:0.5 | r:2 Eb5:1.5 F5:0.5 | r:2 E5:1.5 C#5:0.5 | '
       'r:2 F5:1.5 D5:0.5 | r:2 G5:1.5 A5:0.5 | r:4 | r:4', vel='mp', check=False)
P.chords('vln1', 21, [(4, 'F5'), (4, 'D5'), (4, 'Eb5'), (4, 'E5'), (4, 'F5'), (4, 'G5'), (4, 'G5'), (4, 'G5')], vel='p', art='trem',
         curve=[(0, -4), (1, 0)])
P.chords('vln2', 21, [(4, 'D5'), (4, 'Bb4'), (4, 'G4'), (4, 'C#5'), (4, 'D5'), (4, 'D5'), (4, 'E5'), (4, 'E5')], vel='p', art='trem',
         curve=[(0, -4), (1, 0)])
stabs('tbn', 21, 8, 52, 38, beats=(0, 3))
stabs('tba', 21, 8, 54, 29, n=1, beats=(0,))
P.chords('tbn', 27, [(4, 'Bb2'), (4, 'A2')], vel='mf', curve=[(0, -12), (1, 0)])
P.chords('tbn', 27, [(4, 'E3'), (4, 'G3')], vel='mf', curve=[(0, -12), (1, 0)])
P.T['timp'].add(P.at(27), 8, 45, 80, 'roll', curve=[(0, -18), (1, 0)])
P.hit('cym', 29, 0, 60, 80, art='swell', dur=3.9 * 132 / 60, nudge=-3.7, release=0.25)

# ---------------------------------------------------------------- 29-36: the line holds
CALL_BB = 'F4:1 Bb4:2 C5:1 | A4:2 D5:2 | C5:1.5 Bb4:0.5 G4:1 Bb4:1 | F4:4 | F4:1 Bb4:2 C5:1 | A4:2 F5:2 | Eb5:1.5 D5:0.5 C5:1 A4:1 | A4:2 C#5:1 E5:1'
P.line('tpt', 29, CALL_BB, vel='f', legato=0.05)
P.line('vln1', 29, CALL_BB, vel='f', legato=0.1, check=False)
P.line('fl', 29, CALL_BB, vel='mf', legato=0.06, octave=1, check=False)
HYMN = [(4, 'D4 Bb3 F3 Bb1'), (2, 'C4 A3 F3 F1'), (2, 'D4 Bb3 G3 G1'), (4, 'Eb4 Bb3 G3 C2'), (4, 'Eb4 A3 F3 F1'),
        (4, 'D4 Bb3 F3 Bb1'), (2, 'D4 A3 F#3 F#1'), (2, 'D4 Bb3 F3 G1'), (2, 'Eb4 Bb3 G3 C2'), (2, 'Eb4 A3 F3 F1'), (4, 'E4 C#4 A3 A1')]
for i in range(3):
    P.chords('hn', 29, [(b, n.split()[i]) for b, n in HYMN], vel='mf', overlap=0.05, players=2)
P.chords('tbn', 29, [(b, n.split()[2]) for b, n in HYMN], vel='mf', overlap=0.05)
P.chords('tba', 29, [(b, n.split()[3]) for b, n in HYMN], vel='mf', overlap=0.05)

# ---------------------------------------------------------------- 37-44: everyone
P.line('hn', 36, PICK + THEME, vel='ff', legato=0.06)
P.line('tpt', 36, PICK + THEME, vel='ff', legato=0.05, octave=1)
P.line('vln1', 36, PICK + THEME, vel='ff', legato=0.1, octave=1, check=False)
P.line('vln2', 36, PICK + THEME, vel='f', legato=0.1, check=False)
P.line('ob', 36, PICK + THEME, vel='f', legato=0.06, octave=1, check=False)
P.line('fl', 36, PICK + THEME, vel='f', legato=0.06, octave=2, check=False)
stabs('tbn', 37, 8, 76, 38)
stabs('tba', 37, 8, 78, 29, n=1)
P.ostinato('vla', 37, 8, pattern=OST, vel='mp', low=50, accent=10)

# ---------------------------------------------------------------- 45-52: winding down
stabs('tbn', 45, 8, 62, 38)
stabs('tba', 45, 8, 62, 29, n=1)
stabs('hn', 45, 8, 56, 50, n=2, beats=(0, 3), top=True)
stabs('tpt', 49, 4, 50, 60, n=2, beats=(0,), top=True)
P.chords('vln1', 45, [(4, 'A4'), (4, 'Bb4'), (4, 'Bb4'), (4, 'A4'), (4, 'A4'), (4, 'Bb4'), (2, 'Bb4'), (2, 'A4'), (4, 'G4')],
         vel='mp', art='trem', curve=[(0, -2), (1, -4)])
P.chords('vln2', 45, [(4, 'F4'), (4, 'F4'), (4, 'G4'), (4, 'E4'), (4, 'F4'), (4, 'F4'), (2, 'G4'), (2, 'E4'), (4, 'E4')],
         vel='mp', art='trem', curve=[(0, -2), (1, -4)])

# ---------------------------------------------------------------- drums
P.drums('snare', 5, 'X.xx x.xx X.xx R--x', accent=82, normal=62, ghost=40, roll_vel=66, bars=8)
P.drums('snare', 13, 'X.xx x.xx X.xx R--x', accent=86, normal=66, ghost=42, roll_vel=70, bars=8)
P.drums('snare', 21, 'x.g. g.x. g.x. g.gg', accent=64, normal=54, ghost=34, bars=8)
P.drums('snare', 29, 'X... x.x. X... R--x', accent=84, normal=64, ghost=40, roll_vel=66, bars=8)
P.drums('snare', 37, 'X.xx X.xx X.xx R--X', accent=96, normal=74, ghost=46, roll_vel=80, bars=8)
P.drums('snare', 45, 'X.xx x.xx X.xx R--x', accent=74, normal=56, ghost=36, roll_vel=60, bars=7)
P.T['snare'].add(P.at(52), 4, 60, 62, 'roll', curve=[(0, -10), (1, 0)])
for bar in list(range(5, 21)) + list(range(29, 53)):
    v = 70 if bar < 13 else 76 if bar < 21 else 72 if bar < 37 else 88 if bar < 45 else 66
    for beat, d in ((0, 0), (1.5, -10), (3, -6)):
        sym = P.chord_at(P.at(bar, beat))
        k = chord_tones(sym, 38, 1)[0]
        P.hit('timp', bar, beat, k, v + d)
for bar in (5, 13, 29, 37, 41):
    P.hit('cym', bar, 0, 60, 96 if bar in (37,) else 84, art='crash')
for bar in range(37, 45):
    P.hit('bd', bar, 0, 60, 92)
for bar in (5, 9, 13, 17, 29, 33, 45, 49):
    P.hit('bd', bar, 0, 60, 78)
P.hit('cym', 37, 0, 60, 84, art='gong')

BARS = 52

if __name__ == '__main__':
    out = sys.argv[1] if len(sys.argv) > 1 else '/home/user/score/out'
    if P.warnings:
        print('\n'.join(P.warnings))
    path, info = P.render(BARS, out, loop=True, tail=5.0, lufs=-15.0)
    from engine import save_loudness; save_loudness()
    print('\n'.join(info['balance'])); print('limiter', info['limiter_db'])
