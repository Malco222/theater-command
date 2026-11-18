"""A crew's brush hand: the capitals a tank's name is painted in, as strokes.

Brian's pick b477 (2026-11-18): a tank that has come through three battles wears a name her crew painted. The art
director's look: brush-painted capitals in the field, each letter a little different in size and lean, a baseline that
wanders, the brush's ends showing; never a stencil, a printed face, an outline or a shadow. So there is no font here:
every letter is a handful of strokes (as a man draws them), and `word()` lays them out with a hand's unevenness and
gives each stroke back as a ribbon of the brush's width.

    strokes, long = brushhand.word("BUCKEYE", seed="Sherman")     # ribbons in units of the letters' height, and the word's length

Latin capitals (with the marks French, Polish, Italian and German names carry: a crew paints the accent as one more
dab of the brush) and the Russian ones (the Soviet slogans were in their own letters; `SLOGANS` gives the historian's
twenty in them). Japanese is not here.
The same table is written out for the games as `brush-alphabet.json` (export_land.py), so the field paints the hand
the pictures show.
"""
import math
import random


def _arc(cx, cy, rx, ry, a0, a1, n=None):
    n = n or max(4, int(abs(a1 - a0) / 18.0))
    return [(cx + rx * math.cos(math.radians(a0 + (a1 - a0) * k / n)), cy + ry * math.sin(math.radians(a0 + (a1 - a0) * k / n))) for k in range(n + 1)]


def _bowl(xa, xb, y0, y1):
    """A bowl on a stem: from the stem at y1 out to xb and back to the stem at y0."""
    ym, ry = (y0 + y1) / 2, (y1 - y0) / 2
    cx = xa + (xb - xa) * 0.42
    return [(xa, y1)] + _arc(cx, ym, xb - cx, ry, 90, -90) + [(xa, y0)]


# Each letter: (its width, its strokes). A stroke is a run of points, x from 0, y from 0 (the baseline) to 1.
LATIN = {
    "A": (0.64, [[(0, 0), (0.32, 1), (0.64, 0)], [(0.13, 0.36), (0.51, 0.36)]]),
    "B": (0.56, [[(0, 0), (0, 1)], _bowl(0, 0.50, 0.53, 1), _bowl(0, 0.56, 0, 0.53)]),
    "C": (0.60, [_arc(0.34, 0.5, 0.34, 0.5, 48, 312)]),
    "D": (0.60, [[(0, 0), (0, 1)], _bowl(0, 0.60, 0, 1)]),
    "E": (0.52, [[(0.52, 1), (0, 1), (0, 0), (0.52, 0)], [(0, 0.52), (0.40, 0.52)]]),
    "F": (0.52, [[(0.52, 1), (0, 1), (0, 0)], [(0, 0.52), (0.40, 0.52)]]),
    "G": (0.64, [_arc(0.34, 0.5, 0.34, 0.5, 48, 345) + [(0.66, 0.46), (0.38, 0.46)]]),
    "H": (0.60, [[(0, 0), (0, 1)], [(0.60, 0), (0.60, 1)], [(0, 0.52), (0.60, 0.52)]]),
    "I": (0.10, [[(0.05, 0), (0.05, 1)]]),
    "J": (0.46, [[(0.46, 1), (0.46, 0.26)] + _arc(0.23, 0.26, 0.23, 0.26, 0, -170)]),
    "K": (0.58, [[(0, 0), (0, 1)], [(0.55, 1), (0, 0.42)], [(0.17, 0.58), (0.58, 0)]]),
    "L": (0.50, [[(0, 1), (0, 0), (0.50, 0)]]),
    "M": (0.78, [[(0, 0), (0, 1), (0.39, 0.34), (0.78, 1), (0.78, 0)]]),
    "N": (0.60, [[(0, 0), (0, 1), (0.60, 0), (0.60, 1)]]),
    "O": (0.68, [_arc(0.34, 0.5, 0.34, 0.5, 100, 460)]),
    "P": (0.54, [[(0, 0), (0, 1)], _bowl(0, 0.54, 0.47, 1)]),
    "Q": (0.70, [_arc(0.34, 0.5, 0.34, 0.5, 100, 460), [(0.42, 0.22), (0.70, -0.06)]]),
    "R": (0.58, [[(0, 0), (0, 1)], _bowl(0, 0.54, 0.47, 1), [(0.22, 0.47), (0.58, 0)]]),
    "S": (0.56, [_arc(0.29, 0.75, 0.27, 0.25, 25, 270) + _arc(0.29, 0.25, 0.29, 0.25, 90, -150)[1:]]),
    "T": (0.62, [[(0, 1), (0.62, 1)], [(0.31, 1), (0.31, 0)]]),
    "U": (0.60, [[(0, 1), (0, 0.30)] + _arc(0.30, 0.30, 0.30, 0.30, 180, 360) + [(0.60, 1)]]),
    "V": (0.62, [[(0, 1), (0.31, 0), (0.62, 1)]]),
    "W": (0.86, [[(0, 1), (0.21, 0), (0.43, 0.70), (0.65, 0), (0.86, 1)]]),
    "X": (0.60, [[(0, 0), (0.60, 1)], [(0, 1), (0.60, 0)]]),
    "Y": (0.62, [[(0, 1), (0.31, 0.47), (0.62, 1)], [(0.31, 0.47), (0.31, 0)]]),
    "Z": (0.58, [[(0, 1), (0.58, 1), (0, 0), (0.58, 0)]]),
    "-": (0.32, [[(0.02, 0.46), (0.30, 0.46)]]),
    "'": (0.10, [[(0.07, 1.0), (0.03, 0.78)]]),
    "!": (0.10, [[(0.05, 1.0), (0.05, 0.30)], [(0.05, 0.07), (0.05, 0.0)]]),
}


def _marked(base, mark):
    """A capital with its mark: the mark is one more dab of the brush, over the letter or under it."""
    w, strokes = LATIN[base]
    c = 0.34 if base in "CG" else w / 2
    dab = {"acute": [[(c - 0.05, 1.13), (c + 0.09, 1.30)]], "grave": [[(c + 0.05, 1.13), (c - 0.09, 1.30)]],
           "hat": [[(c - 0.14, 1.12), (c, 1.28), (c + 0.14, 1.12)]], "dots": [[(c - 0.12, 1.15), (c - 0.12, 1.25)], [(c + 0.12, 1.15), (c + 0.12, 1.25)]],
           "bar": [[(c - 0.16, 1.20), (c + 0.16, 1.20)]], "dot": [[(c, 1.14), (c, 1.25)]],
           "tail": [[(w - 0.04, 0.0), (w - 0.13, -0.12), (w - 0.03, -0.20)]], "hook": [[(c + 0.02, 0.0), (c - 0.04, -0.12), (c + 0.07, -0.20)]],
           "stroke": [[(-0.10, 0.36), (0.22, 0.64)]]}[mark]
    return (w, list(strokes) + dab)


_MARKS = {"À": "A grave", "Â": "A hat", "Ä": "A dots", "Ā": "A bar", "Ą": "A tail", "Ç": "C hook", "Ć": "C acute", "É": "E acute", "È": "E grave", "Ê": "E hat",
          "Ë": "E dots", "Ē": "E bar", "Ę": "E tail", "Ì": "I grave", "Î": "I hat", "Ï": "I dots", "Ī": "I bar", "Ł": "L stroke", "Ń": "N acute", "Ò": "O grave",
          "Ó": "O acute", "Ô": "O hat", "Ö": "O dots", "Ō": "O bar", "Ś": "S acute", "Ù": "U grave", "Û": "U hat", "Ü": "U dots", "Ū": "U bar", "Ź": "Z acute", "Ż": "Z dot"}
LATIN.update({ch: _marked(*how.split()) for ch, how in _MARKS.items()})
_L = LATIN
RUSSIAN = {
    "А": _L["A"], "В": _L["B"], "Е": _L["E"], "К": _L["K"], "М": _L["M"], "Н": _L["H"], "О": _L["O"], "Р": _L["P"], "С": _L["C"], "Т": _L["T"], "Х": _L["X"],
    "Б": (0.56, [[(0.50, 1), (0, 1), (0, 0)], _bowl(0, 0.56, 0, 0.53)]),
    "Г": (0.50, [[(0.50, 1), (0, 1), (0, 0)]]),
    "Д": (0.70, [[(0.0, -0.12), (0.0, 0.0), (0.70, 0.0), (0.70, -0.12)], [(0.10, 0.0), (0.24, 1), (0.60, 1), (0.60, 0.0)]]),
    "Ж": (0.86, [[(0.43, 0), (0.43, 1)], [(0, 1), (0.43, 0.5), (0, 0)], [(0.86, 1), (0.43, 0.5), (0.86, 0)]]),
    "З": (0.54, [_arc(0.25, 0.75, 0.27, 0.25, 150, -90) + _arc(0.25, 0.25, 0.29, 0.25, 90, -150)[1:]]),
    "И": (0.60, [[(0, 1), (0, 0), (0.60, 1), (0.60, 0)]]),
    "Й": (0.60, [[(0, 1), (0, 0), (0.60, 1), (0.60, 0)], [(0.16, 1.22), (0.30, 1.13), (0.44, 1.22)]]),
    "Л": (0.62, [[(0, 0), (0.10, 0.10), (0.24, 1), (0.62, 1), (0.62, 0)]]),
    "П": (0.60, [[(0, 0), (0, 1), (0.60, 1), (0.60, 0)]]),
    "У": (0.60, [[(0, 1), (0.32, 0.36)], [(0.60, 1), (0.22, 0.06), (0.06, 0.0)]]),
    "Ф": (0.76, [[(0.38, 0), (0.38, 1)], _arc(0.38, 0.55, 0.38, 0.30, 100, 460)]),
    "Ц": (0.68, [[(0, 1), (0, 0), (0.60, 0), (0.60, 1)], [(0.60, 0), (0.68, 0), (0.68, -0.14)]]),
    "Ч": (0.56, [[(0, 1), (0, 0.62)] + _arc(0.16, 0.62, 0.16, 0.16, 180, 270)[1:] + [(0.56, 0.46)], [(0.56, 1), (0.56, 0)]]),
    "Ш": (0.84, [[(0, 1), (0, 0), (0.84, 0), (0.84, 1)], [(0.42, 1), (0.42, 0)]]),
    "Щ": (0.92, [[(0, 1), (0, 0), (0.84, 0), (0.84, 1)], [(0.42, 1), (0.42, 0)], [(0.84, 0), (0.92, 0), (0.92, -0.14)]]),
    "Ъ": (0.66, [[(0, 1), (0.14, 1), (0.14, 0)], _bowl(0.14, 0.66, 0, 0.53)]),
    "Ы": (0.80, [[(0, 1), (0, 0)], _bowl(0, 0.50, 0, 0.53), [(0.80, 1), (0.80, 0)]]),
    "Ь": (0.54, [[(0, 1), (0, 0)], _bowl(0, 0.54, 0, 0.53)]),
    "Э": (0.60, [_arc(0.26, 0.5, 0.34, 0.5, 132, -132), [(0.24, 0.5), (0.60, 0.5)]]),
    "Ю": (0.88, [[(0, 0), (0, 1)], [(0, 0.5), (0.24, 0.5)], _arc(0.56, 0.5, 0.32, 0.5, 100, 460)]),
    "Я": (0.58, [[(0.58, 0), (0.58, 1)], [(0.58, 1)] + _arc(0.24, 0.735, 0.24, 0.265, 90, 270) + [(0.58, 0.47)], [(0.36, 0.47), (0.0, 0)]]),
}
LETTERS = dict(LATIN)
LETTERS.update(RUSSIAN)
SPACE = 0.36

# Russian letters for a slogan the historian gives in Latin letters (his own spelling of them).
_RU = [("shch", "Щ"), ("zh", "Ж"), ("kh", "Х"), ("ts", "Ц"), ("ch", "Ч"), ("sh", "Ш"), ("yu", "Ю"), ("ya", "Я"), ("yy", "ЫЙ"), ("iy", "ИЙ"), ("a", "А"), ("b", "Б"), ("v", "В"), ("g", "Г"),
       ("d", "Д"), ("e", "Е"), ("z", "З"), ("i", "И"), ("y", "Ы"), ("k", "К"), ("l", "Л"), ("m", "М"), ("n", "Н"), ("o", "О"), ("p", "П"), ("r", "Р"), ("s", "С"), ("t", "Т"),
       ("u", "У"), ("f", "Ф"), ("'", "Ь")]


# The historian's twenty Soviet lines (tank-names-b477.json), in their own letters: his Latin spelling drops the soft
# sign, so a letter-for-letter reading cannot give it back. Period paint wrote Е for Ё.
SLOGANS = {"Za Rodinu": "ЗА РОДИНУ", "Mstitel": "МСТИТЕЛЬ", "Smelyy": "СМЕЛЫЙ", "Vpered na zapad": "ВПЕРЕД НА ЗАПАД", "Za nashu Moskvu": "ЗА НАШУ МОСКВУ",
           "Za Odessu": "ЗА ОДЕССУ", "Bogatyr": "БОГАТЫРЬ", "Sokol": "СОКОЛ", "Burya": "БУРЯ", "Molniya": "МОЛНИЯ", "Podarok frontu": "ПОДАРОК ФРОНТУ",
           "Ot kolkhoznikov": "ОТ КОЛХОЗНИКОВ", "Ne otstupim": "НЕ ОТСТУПИМ", "Pobeda": "ПОБЕДА", "Sibiryak": "СИБИРЯК", "Volga": "ВОЛГА", "Groza": "ГРОЗА",
           "Druzhba": "ДРУЖБА", "Za Sevastopol": "ЗА СЕВАСТОПОЛЬ", "Ural": "УРАЛ"}
_SLOGANS = {k.lower(): v for k, v in SLOGANS.items()}


def russian(latin):
    """A slogan written in Latin letters, in Russian ones: the historian's own twenty from the table, any other by a
    plain letter-for-letter reading (good for a picture; give the game the Russian itself)."""
    key = " ".join(latin.replace("_", " ").replace("'", "").lower().split())
    if key in _SLOGANS:
        return _SLOGANS[key]
    out, s, i = "", latin.lower(), 0
    while i < len(s):
        for a, b in _RU:
            if s.startswith(a, i):
                out += b
                i += len(a)
                break
        else:
            out += s[i].upper()
            i += 1
    return out


def _ribbon(pts, width, rng):
    """A stroke as the brush leaves it: a ribbon along the points, a touch fuller where the brush is pressed down,
    thinning as it is lifted off at the end."""
    # more points along it, so that it lies down on a plate that is not flat
    fine = [pts[0]]
    for a, b in zip(pts, pts[1:]):
        d = math.hypot(b[0] - a[0], b[1] - a[1])
        n = max(1, int(d / 0.16))
        fine += [(a[0] + (b[0] - a[0]) * k / n, a[1] + (b[1] - a[1]) * k / n) for k in range(1, n + 1)]
    pts = fine
    # the ends run on a little past the last point, as a loaded brush does
    def run_on(p, q, by):
        d = math.hypot(p[0] - q[0], p[1] - q[1]) or 1.0
        return (p[0] + (p[0] - q[0]) / d * by, p[1] + (p[1] - q[1]) / d * by)
    pts = [run_on(pts[0], pts[1], width * 0.30)] + pts + [run_on(pts[-1], pts[-2], width * 0.22)]
    n = len(pts)
    press = rng.uniform(0.92, 1.10)
    left, right = [], []
    for k, p in enumerate(pts):
        a, b = pts[max(0, k - 1)], pts[min(n - 1, k + 1)]
        tx, ty = b[0] - a[0], b[1] - a[1]
        d = math.hypot(tx, ty) or 1.0
        nx, ny = -ty / d, tx / d
        t = k / (n - 1)
        w = width * press * (0.80 if k == 0 else 1.0) * (1.0 - 0.38 * max(0.0, (t - 0.78) / 0.22)) * rng.uniform(0.93, 1.07)
        left.append((p[0] + nx * w / 2, p[1] + ny * w / 2))
        right.append((p[0] - nx * w / 2, p[1] - ny * w / 2))
    return [[left[k], left[k + 1], right[k + 1], right[k]] for k in range(n - 1)]


def word(text, seed="", brush=0.135, down=False):
    """The word as painted: (quads, long). Each quad is four (x, y) points in units of the letters' height, x along
    the writing from 0, y up from the baseline; `long` is how far the word runs. brush: the brush's width.
    down: the letters stand one under another (x across from 0, y going down from 0), `long` then how far down."""
    rng = random.Random("brush:%s:%s" % (seed, text))
    quads, x = [], 0.0
    wander = rng.uniform(0.0, 6.28)
    k = 0
    for ch in text.upper():
        if ch == " ":
            x += SPACE * rng.uniform(0.9, 1.15)
            continue
        if ch not in LETTERS:
            continue
        wide, strokes = LETTERS[ch]
        size = rng.uniform(0.93, 1.07)
        lean = rng.uniform(-0.055, 0.075)
        dy = 0.035 * math.sin(wander + 0.9 * k) + rng.uniform(-0.025, 0.025)
        for st in strokes:
            pts = [(px * size + py * size * lean, py * size + dy) for (px, py) in st]
            for q in _ribbon(pts, brush, rng):
                if down:
                    quads.append([(a - wide * size / 2, b - x - 1.0) for (a, b) in q])
                else:
                    quads.append([(a + x, b) for (a, b) in q])
        x += (1.0 if down else wide * size) + rng.uniform(0.25, 0.35)
        k += 1
    return quads, max(0.0, x - 0.30)


def table():
    """The letters as the games can read them: for each, its width and strokes, and how a hand lays them out."""
    return {"about": "A tank crew's brush hand (Brian's pick b477): capitals as strokes, in units of the letters' height; x from 0, y from 0 (the baseline) to 1. "
                     "Paint each stroke as a ribbon about `brush` wide that runs on a little past both ends and thins as the brush lifts off. "
                     "A hand is never even: `hand` gives how far each letter's size, lean and baseline may stray, and the gap between letters. Never a printed face.",
            "brush": 0.135, "space": SPACE,
            "hand": {"size": [0.93, 1.07], "lean": [-0.055, 0.075], "baseline": 0.06, "gap": [0.25, 0.35], "run_on": [0.30, 0.22], "lift_off": 0.38},
            "slogans_russian": dict(SLOGANS),
            "letters": {ch: {"wide": round(w, 3), "strokes": [[[round(a, 3), round(b, 3)] for (a, b) in st] for st in strokes]} for ch, (w, strokes) in LETTERS.items()}}
