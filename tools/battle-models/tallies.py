"""A bomber's record, painted on her: a yellow bomb for each mission and a symbol for each aircraft her gunners shot down.

Brian, 2026-11-20: "i want the same mechanics I talked about bombing to be applied for all nations not just the US",
"nose art only for US bombers, other countries didnt do that", "but i still want yellow painted bombs for each bombing
mission they did and then the symbols showing how many aircraft they shot down".

The look is the art director's ruling of that day (art-director/battles/bomber-tallies-ruling.md). The aim is that a
player who knows nothing reads two things from the close view in a second: how long this aircraft has lived, and whom
it has fought. So every nation's bomber wears the same panel in the same place:

  * under the pilot's window on her port side (on a glass nose, the first clear metal aft of the glazing at the same
    height), never across her national marking, her codes or her nose art, never on the fin;
  * a fixed line: the kills stand above it, the bombs hang below it, so the panel never jumps about between aircraft;
  * bombs in rows of ten with a small gap after the fifth, so the eye counts in fives; five rows to a block; past fifty
    a second block aft of the first; none shown past a hundred. Rows fill from the front aft and new rows go below.
    A bomb is about 12 cm tall and 4.5 wide, 2.5 apart, rows 4 apart;
  * kills in rows of ten above the line, new rows upward toward the window, none shown past thirty. They are grouped by
    the victim's nation, the commonest first; each nation starts a row of its own while rows remain, so a nation is
    never split between rows (only when the nations need more rows than there are do the smallest share one, an empty
    place between them). A symbol is about 9 cm, and never touches the next: a third of its width or more between them;
  * the whole panel four fifths the size where the side under the cockpit is under about a metre tall;
  * yellow on a dark finish plain; on a light finish each bomb edged in black. The symbols the same way: on dark paint
    the dark ones carry a thin white edge, on light paint the white ones a thin dark edge;
  * by hand: each bomb a little different in size and lean, the spacing uneven, the rows not quite level, each row
    perhaps from another tin; the oldest bombs faded to two thirds of the newest's contrast, the oldest kills only to
    0.85 (a paler disc or star would read as another nation's); the battle soots and holes them as it does the skin
    around them.

This module lays the panel out on a bomber (`panel`, called by planes.py while it converts her: the slots go into her
file's `tally`), and says which mark goes in which slot for a record (`fill`), for the pictures (marks.py) and, by the
same rules written into the file, for the game. Nothing here is copied from any real machine.
"""
import math
import random

from mathutils import Vector
from mathutils.bvhtree import BVHTree

# ---- the panel's measures (the ruling, section 1), in metres --------------------------------------------------------
BOMB_R = 0.06                 # (half a bomb's height: about 12 cm tall)
BOMB_W = 0.72 * BOMB_R        # (marks.bomb's own shape: 0.72 of its half-height wide, about 4.3 cm)
GAP = 0.025                   # (2 to 3 cm apart)
FIVE = 0.03                   # (the small gap after the fifth bomb of a row)
ROW_GAP = 0.04                # (rows 4 cm apart)
ROWS = 5                      # (rows to a block: 50 bombs)
BLOCK_GAP = 0.15              # (the second block starts this far aft of the first)
BLOCKS = 2                    # (none shown past a hundred)
PER_ROW = 10
KILL_R = 0.045                # (half a kill symbol: 8 to 10 cm)
# Symbols never touch (the art director, 19:48Z): at least a third of a symbol's width between them, its thin edge and
# the painter's hand included (a symbol with its edge is 2.22 radii wide; a third of that is 3.3 cm).
KILL_GAP = 0.05
KILL_ROW_GAP = 0.05
KILL_ROWS = 3                 # (room for two rows; a third, rarely, up to thirty)
LINE = 0.03                   # (the gap from the line to the bombs below it and to the kills above it)
SMALL = 0.8                   # (the whole panel on a small bomber)
SMALL_SIDE = 1.0              # (a bomber is small when the side under her cockpit is under this tall)

# Where her panel's front edge stands, where planes.py's rule (under the pilot's window, from the front of her canopy)
# does not serve: aft of a B-17's nose art, which sits below and behind her navigator's windows; and the first clear
# metal aft of a glass nose.
FRONT = {"B17": 5.05, "He111": 3.85, "Amerikabomber": 6.8, "B29": 8.25}
# A whole panel a little smaller where two blocks would not fit between what must stay clear: the B-17's painting
# ahead of it and her wing root behind (at nine tenths a bomb is still 11 cm tall and a kill symbol 8).
SCALE = {"B17": 0.9}
ART_MARGIN = 0.035            # (noseart.raster's margin: the painting stands this share of its patch in from each edge)
DEBUG = False
SEE_THROUGH = {"blur", "blur_tip", "glint", "glare"}
PROUD = 0.08                  # (anything standing further off her side than this is not her side: a wing root, a nacelle, a blister)
DETAIL = {"line", "rivet", "rivet_under", "seam", "seam_under", "stain", "patch"}     # (her skin's own details: a mark may lie over them)

# ---- what is painted (the ruling, section 4) ------------------------------------------------------------------------
# A victim's plain marking, in small: never a swastika, a rayed flag, the fasces or a party badge.
VICTIM = {"GER": "KILL-GER", "JAP": "KILL-JAP", "ITA": "KILL-ITA", "SOV": "KILL-SOV", "USA": "KILL-USA",
          "UK": "KILL-UK", "CAN": "KILL-UK", "AUS": "KILL-UK", "NZL": "KILL-UK", "SAF": "KILL-UK", "IND": "KILL-UK",
          "FRA": "KILL-FRA", "VIC": "KILL-FRA", "FFR": "KILL-FRA"}
ANYONE = "KILL-PLANE"         # (anyone else: a plain aircraft seen from above, white on a dark finish, black on a light)
# A nation that paints its own symbol for every kill, whoever the victim: the Soviet Union a red star, Japan a cherry
# blossom (a star would be taken for an American or Soviet victim in the same system).
OWN = {"SOV": "KILL-SOV", "JAP": "KILL-BLOSSOM"}
MOST_BOMBS, MOST_KILLS = PER_ROW * ROWS * BLOCKS, PER_ROW * KILL_ROWS

# Each row of bombs from a tin of its own (the newest row's tin is the first): a slightly different yellow.
TINS = ["D4AA30", "DCB443", "C99F2A", "D8B03A"]
FADE = 0.67                   # (the oldest bomb's contrast, as a share of the newest's)
KILL_FADE = 0.85              # (the oldest kill's: no further, or a red disc reads pink and a blue disc loses its blue)
LIGHT_L = 50.0                # (a finish is light when its lightness L* is at least this: the yellow, at about 72, sinks into it)


def lightness(hexcol):
    """The CIE lightness L* (0 black, 100 white) of a colour."""
    def lin(c):
        c /= 255.0
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    r, g, b = (lin(int(hexcol[i:i + 2], 16)) for i in (0, 2, 4))
    y = 0.2126 * r + 0.7152 * g + 0.0722 * b
    return 116.0 * (y ** (1.0 / 3.0) if y > 216.0 / 24389.0 else (24389.0 / 27.0 * y + 16.0) / 116.0) - 16.0


def finish_of(hexcol):
    return "light" if lightness(hexcol) >= LIGHT_L else "dark"


def mix(a, b, t):
    """Two colours mixed, t of the way from a to b."""
    return "".join("%02X" % int(round(int(a[i:i + 2], 16) * (1.0 - t) + int(b[i:i + 2], 16) * t)) for i in (0, 2, 4))


def worn_at(panel_, paints):
    """The paint she wears under the middle of her panel, for her paints as she flies (a plain machine's are her
    owner's): `under`, and where that is her `upper`, mixed toward her `upper2` by her coat's share there (a mottle, a
    patch, the sun's fading)."""
    under = panel_.get("under", "upper")
    colour = paints.get(under) or panel_.get("colour")
    if under == "upper" and panel_.get("blend", 0.0) > 0.0 and paints.get("upper2"):
        colour = mix(colour, paints["upper2"], panel_["blend"])
    return colour


def hand(seed, n_bombs=MOST_BOMBS, n_kills=MOST_KILLS):
    """The painter's hand, slot by slot: [lean in degrees, size, along, up] (along and up in metres, on the skin; see
    RULES["hand"] for which way each turns and moves a mark). Each bomb differs a little in size (within 5%) and lean
    (within 3 degrees), the spacing is uneven and a row is not quite level. The kills are painted with a steadier hand
    (size within 1.5%, a millimetre or so off their places, rows level) so that they never come within a third of a
    symbol's width of each other. One hand for the airframe; the game may start it at another slot for each aircraft."""
    rng = random.Random("tally:" + seed)

    def g(sd, most):
        return max(-most, min(most, rng.gauss(0.0, sd)))

    bombs, kills = [], []
    row_dv, row_tilt = 0.0, 0.0
    for k in range(n_bombs):
        if k % PER_ROW == 0:
            row_dv, row_tilt = g(0.003, 0.006), g(0.004, 0.01)                   # (this row a little high or low, and not quite level)
        lean = g(1.3, 3.0)
        size = 1.0 + g(0.022, 0.05)
        du = g(0.0028, 0.006)
        dv = row_dv + row_tilt * (k % PER_ROW) * 0.07 + g(0.0012, 0.003)
        bombs.append([round(lean, 2), round(size, 3), round(du, 4), round(dv, 4)])
    for k in range(n_kills):
        if k % PER_ROW == 0:
            row_dv = g(0.0006, 0.001)                                            # (a row of kills a millimetre high or low, but level)
        lean = g(1.3, 3.0)
        size = 1.0 + g(0.007, 0.015)
        du = g(0.0006, 0.0012)
        dv = row_dv + g(0.0004, 0.0008)
        kills.append([round(lean, 2), round(size, 3), round(du, 4), round(dv, 4)])
    return bombs, kills


def kill_rows(groups):
    """The rows of kill symbols, the lowest first, for groups [(tag, count)] in order (the commonest first): each a list
    of tags, None for an empty place. Each group starts a row of its own, and one of more than ten fills whole rows and
    runs on into its next, so a nation is never split between rows (the art director, 19:48Z). While that takes more
    than three rows, the smallest share: from the end, the last two neighbouring rows that fit together in one row (an
    empty place between groups) are joined; when no neighbours fit, every row after the third is taken away and its
    groups put back singly, the largest first, each whole into the row it leaves least room in (ties: the later row),
    an empty place before it. A group that fits nowhere is not shown (never more than thirty)."""
    rows = [[(tag, min(PER_ROW, n - k))] for tag, n in groups for k in range(0, n, PER_ROW)]

    def width(row):
        return sum(n for _, n in row) + len(row) - 1

    while len(rows) > KILL_ROWS:
        for i in range(len(rows) - 2, -1, -1):                                   # (neighbours from the end: the smallest share first)
            if width(rows[i] + rows[i + 1]) <= PER_ROW:
                rows[i:i + 2] = [rows[i] + rows[i + 1]]
                break
        else:                                                                    # (no neighbours fit: the rows past the third go back singly)
            rest = [one for row in rows[KILL_ROWS:] for one in row]
            rows = rows[:KILL_ROWS]
            for one in sorted(rest, key=lambda s: -s[1]):                        # (the largest first; ties keep their order)
                fits = [j for j in range(len(rows)) if width(rows[j]) + 1 + one[1] <= PER_ROW]
                if fits:
                    j = min(fits, key=lambda j: (PER_ROW - width(rows[j]) - 1 - one[1], -j))
                    rows[j] = rows[j] + [one]
    out = []
    for row in rows:
        line = []
        for tag, n in row:
            line += ([None] if line else []) + [tag] * n
        out.append(line)
    return out


def kills_in_order(nation, kills, by=None):
    """The symbols of a bomber's kills slot by slot (ten to a row, the lowest row first, each from the front), None for
    an empty place: grouped by symbol (the victim's nation as `VICTIM` paints it), the commonest first and ties by the
    symbol's name, each group on a row of its own (`kill_rows`). Kills with no nation recorded join the first group,
    or are plain aircraft when none is recorded. At most thirty are shown."""
    kills = int(kills or 0)
    if kills <= 0:
        return []
    by = {k: int(v) for k, v in (by or {}).items() if int(v) > 0}
    if nation in OWN:                                                            # (one symbol for every kill: no groups)
        groups = [(OWN[nation], kills)]
    else:
        counts = {}
        for victim, n in by.items():
            tag = VICTIM.get(victim, ANYONE)
            counts[tag] = counts.get(tag, 0) + n
        rest = kills - sum(counts.values())
        if rest > 0:                                                             # (the unrecorded join the first group: the commonest, ties by name)
            top = min(counts, key=lambda t: (-counts[t], t)) if counts else ANYONE
            counts[top] = counts.get(top, 0) + rest
        groups = [(t, counts[t]) for t in sorted(counts, key=lambda t: (-counts[t], t))]
    out = []
    for row in kill_rows(groups):
        out += [None] * (-len(out) % PER_ROW) + row                              # (each row from its own first slot)
    return out


def fill(tally, rec, seat=0):
    """Which mark goes in which slot for a record ({"missions": 25, "kills": 3, "by": {"GER": 2, "ITA": 1}}): a list of
    {tag, slot, kind, row, fade, tin} (tin the yellow of a bomb's row; fade its contrast, newest 1.0, the oldest bomb
    0.67 and the oldest kill 0.85).
    seat: which aircraft of her group this is (any whole number), to start the painter's hand at another slot."""
    out = []
    flown = max(0, min(int(rec.get("missions", 0) or 0), len(tally["missions"]["slots"])))
    for k in range(flown):
        row = k // PER_ROW
        rows = (flown - 1) // PER_ROW + 1
        out.append({"kind": "missions", "slot": k, "tag": "BOMB", "row": row,
                    "fade": round(FADE + (1.0 - FADE) * (k / (flown - 1) if flown > 1 else 1.0), 3),
                    "tin": TINS[(rows - 1 - row + seat) % len(TINS)]})
    kills = rec.get("kills", 0)
    by = rec.get("by")
    if isinstance(kills, list):                                                  # (the older record: [["GER", 14], ["ITA", 3]])
        by = {}
        for nat, n in kills:
            by[nat] = by.get(nat, 0) + int(n)
        kills = sum(by.values())
    tags = kills_in_order(rec.get("nation") or tally.get("nation", "*"), kills, by)
    slots = len(tally["kills"]["slots"])
    for row0 in range(0, len(tags), PER_ROW):
        row = [(k, t) for k, t in enumerate(tags[row0:row0 + PER_ROW], row0) if t]
        for j, (k, t) in enumerate(row):                                         # (the kills fade by the same rule within their rows)
            if k < slots:
                out.append({"kind": "kills", "slot": k, "tag": t, "row": k // PER_ROW,
                            "fade": round(KILL_FADE + (1.0 - KILL_FADE) * (j / (len(row) - 1) if len(row) > 1 else 1.0), 3)})
    return out


# ---- the rules as the game reads them from her file ------------------------------------------------------------------
RULES = {
    "missions": "One bomb for each mission flown: mission k in slot k-1, from the front of the top row aft, ten to a row, "
                "new rows below, fifty to a block and the second block aft of the first. None past 100.",
    "kills": "One symbol for each aircraft her gunners shot down, in the slots above the line, ten to a row from the "
             "front, new rows upward. Group them by symbol (`victims` gives each victim nation's), the commonest first, "
             "ties by the symbol's name (A to Z). Kills with no nation recorded join the first group (KILL-PLANE when "
             "none is recorded). A nation in `own` paints its own symbol for every kill, as one group. Lay out: each "
             "group starts a row of its own, from that row's first slot; a group of more than ten fills whole rows and "
             "runs on into its next row, so a nation is never split between rows. While that takes more than three "
             "rows: from the end, join the last two neighbouring rows that fit together in one row of ten (one slot "
             "empty between groups); when no two neighbours fit, take away every row after the third and put their "
             "groups back singly, the largest first, each whole into the row it leaves the least room in (ties: the "
             "later row), one slot empty before it. A group that fits nowhere is not shown. tallies.kill_rows is the "
             "reference. The slots keep a third of a symbol's width between symbols: never draw them closer.",
    "victims": dict(VICTIM, **{"*": ANYONE}),
    "own": OWN,
    "hand": "`hand` is the painter's hand for this airframe, one entry a slot: [lean in degrees, size, along, up]. "
            "Draw the mark upright on her side (the decal's up along her skin toward the sky, its right toward her "
            "tail), turned by lean (positive turns it anticlockwise as seen from outside her), scaled by size, and "
            "moved along metres toward her tail and up metres up her skin. Missions first, then kills. For another "
            "aircraft of the same type start at another slot: entry (k + 37 * seat) mod the list's length.",
    "tins": {"colours": TINS, "rule": "each row of bombs takes one tin; the newest row the first tin, the row before it "
                                      "the second, and so on round (start one later for each seat)"},
    "fade": {"oldest": FADE, "oldest_kill": KILL_FADE,
             "rule": "the newest mark at full strength, each older one a step paler: the first bomb at 0.67 of the "
                     "newest's contrast (draw it at that opacity over the skin). Kills fade within each row, the front "
                     "one oldest, but only to 0.85, so a red disc never reads pink nor a blue disc lose its blue. "
                     "Never so far that a mark drops out of the count"},
    "finish": {"light_from_L": LIGHT_L, "rule": "the finish under the panel, not the aircraft's name: test the paint "
                                               "she wears at `panel.middle` (the middle of the panel's first block), "
                                               "camouflage, mottle and her owner's colours included: her paint "
                                               "`panel.under` there, and when that is `upper`, mixed `panel.blend` of "
                                               "the way to her `upper2` (with a plain machine's owner's colours for "
                                               "each where the owner gives them). Light when its lightness L* is at "
                                               "least this. `panel.colour` and `panel.finish` are the answer for her "
                                               "own paint. Use the decals' dark or light set to match, for the whole "
                                               "panel"},
    "damage": "The battle marks the tally as it marks the skin: draw soot, oil and bullet holes over it.",
    "decals": "tally/tally-sheet.png and tally-sheet.json: each tag's picture for a dark and a light finish. A bomb's "
              "is white with its black edge (on a light finish): multiply it by its row's tin.",
}


# ---- laying the panel out on her ------------------------------------------------------------------------------------
def panel(name, s, hull, hood, nation, model, marks=(), art=None, x_front=None):
    """Her panel's slots: {"missions": {...100 slots}, "kills": {...30 slots}, "panel": {...}, "hand": ..., "rules": ...}.

    hull: her Body; hood: the Body of her canopy (or None); model: the Model as converted so far (her faces, to check
    each slot lands on her own painted skin: not on glass, a frame, a gun or her wing); marks: her markings (each slot
    stays clear of them); art: her nose art's patch ((x, z) middle, width, height), which the panel stays clear of."""

    def side(x, z):
        """The place on her port side at this height, and the way her skin faces there (in her own axes, y to port:
        her Body gives them in the game's, y to the right)."""
        t = max(-0.97, min(0.97, (z - hull.z(-x)) / hull.h(-x)))
        a = math.pi - math.asin(math.copysign(abs(t) ** (1.0 / hull.power), t))
        p, n = hull.at(x, a), hull.out(x, a)
        return Vector((p.x, -p.y, p.z)), Vector((n.x, -n.y, n.z))

    def sill(x):
        """How high her skin first comes clear of her canopy on that side."""
        if hood is None or not hood.back < x < hood.front:
            return None
        for k in range(0, 90):
            p = hull.at(x, math.radians(90.0 + k))
            dy, dz = p.y / max(hood.w(-x), 1.0e-3), (p.z - hood.z(-x)) / max(hood.h(-x), 1.0e-3)
            if dy * dy + dz * dz > 1.0:
                return p.z
        return None

    # ---- her faces, to look at what lies under each slot ----
    names = list(model.paints)
    skin_paints = {"upper", "upper2", "under", "nose", "boss"}
    tree = BVHTree.FromPolygons([tuple(v) for v in model.verts], [tuple(f) for f in model.faces])

    def under(p, n):
        """What is first met coming in to this place along its facing: (paint name, how far short of it, its face).
        A turning airscrew's blur and a glint are seen through."""
        start, reach = p + n * 0.8, 1.6
        for _ in range(6):
            hit = tree.ray_cast(start, -n, reach)
            if hit[0] is None:
                return None, 1.0, None
            paint = names[model.fpaint[hit[2]]]
            if paint not in SEE_THROUGH:
                return paint, (p - hit[0]).dot(n), hit[2]
            reach -= (start - hit[0]).length + 0.002
            start = hit[0] - n * 0.002
        return None, 1.0, None

    # ---- the front of the panel ----
    if x_front is None:
        x_front = FRONT.get(name)
    if x_front is None:
        tall = max(r[3] for r in s["hoods"][0][0])
        x_front = max(r[0] for r in s["hoods"][0][0] if r[3] >= 0.6 * tall) - 0.05       # (under the pilot's window, from the foot of her windscreen)

    # ---- the clear side: under her window (or where her side stops facing a man beside her), down to the first
    #      thing that is not her bare side (her wing root, a window), or to where her side turns under ----
    def ok(paint):
        return paint in skin_paints or paint in DETAIL or (paint or "").endswith(".on")

    def span(x):
        """(top, bottom) of her clear port side at this station."""
        zc, h = hull.z(-x), hull.h(-x)
        top = sill(x)
        top = (top - 0.05) if top is not None else zc + 0.97 * h
        while top > zc - h and side(x, top)[1].z > 0.62:                         # (no higher than where her side still faces a man beside her)
            top -= 0.005
        z = top
        while z > zc - 0.97 * h:
            p_, n_ = side(x, z)
            if n_.z < -0.5:
                break
            paint, short, _ = under(p_, n_)
            if not ok(paint) or short < -PROUD:                                  # (her model's skin stands a few centimetres off her drawn hull here and there)
                break
            z -= 0.01
        return top, z + 0.01

    def skin_from(x, z0, d):
        """The height reached going d metres along her skin from height z0 (up for d > 0) at this station."""
        k = max(1, int(math.ceil(abs(d) / 0.002 - 1.0e-6)))  # (all of d, in equal steps of up to 2 mm: int(0.284 / 0.002) is 141, not 142)
        z, step = z0, d / k
        for _ in range(k):
            n_ = side(x, z)[1]
            z += step * math.sqrt(max(0.05, 1.0 - n_.z * n_.z))
        return z

    def skin_between(x, lo, hi):
        z, d = lo, 0.0
        while z < hi:
            n_ = side(x, z)[1]
            z += 0.002
            d += 0.002 / math.sqrt(max(0.05, 1.0 - n_.z * n_.z))
        return d

    def scaled(k):
        return k * scale

    scale = SCALE.get(name, 1.0)
    for _ in range(2):
        kill_h3 = KILL_ROWS * scaled(2.0 * KILL_R) + (KILL_ROWS - 1) * scaled(KILL_ROW_GAP) + scaled(LINE)
        kill_h2 = 2 * scaled(2.0 * KILL_R) + scaled(KILL_ROW_GAP) + scaled(LINE)
        bomb_h = ROWS * scaled(2.0 * BOMB_R) + (ROWS - 1) * scaled(ROW_GAP) + scaled(LINE)
        row_w = PER_ROW * scaled(BOMB_W) + (PER_ROW - 1) * scaled(GAP) + scaled(FIVE)
        kill_w = PER_ROW * scaled(2.0 * KILL_R) + (PER_ROW - 1) * scaled(KILL_GAP)
        wide = max(BLOCKS * row_w + (BLOCKS - 1) * scaled(BLOCK_GAP), kill_w)
        x_mid = x_front - 0.5 * row_w
        spans = [span(x_front - wide * k / 12.0) for k in range(13)]
        if DEBUG:
            for k, (t_, b_) in enumerate(spans):
                x_ = x_front - wide * k / 12.0
                p_, n_ = side(x_, b_ - 0.01)
                print(f"  span {name} x {x_:.2f}: {b_:.2f} to {t_:.2f}; below it {under(p_, n_)[:2]}", flush=True)
        top = min(t for t, _ in spans)
        bottom = max(b for _, b in spans) + 0.03
        room = skin_between(x_mid, bottom, top)
        if scale > SMALL and room < SMALL_SIDE:                                  # (a small bomber's side, or a side her wing root leaves short)
            scale = SMALL
            continue
        break
    if room >= kill_h3 + bomb_h:                                                 # (all three rows of kills under her window)
        line = skin_from(x_mid, top, -kill_h3)
        fit = "under her window"
    elif room >= kill_h2 + bomb_h:                                               # (two rows; a third, rarely, a little higher)
        line = skin_from(x_mid, top, -kill_h2)
        fit = "two rows of kills under her window, a third above"
    else:                                                                        # (the bombs from just above what is below; the kills above them as high as they go)
        line = skin_from(x_mid, bottom, bomb_h)
        fit = "from her wing root up; the kills above %.2f m (side faces up %.2f there)" % (top, side(x_mid, skin_from(x_mid, line, kill_h2))[1].z)
    low = skin_from(x_mid, line, -bomb_h)

    def along(z0, d):
        return skin_from(x_mid, z0, d)

    # ---- the slots ----
    def slot(x, z):
        p, n = side(x, z)
        return p, n

    bombs, kills = [], []
    block_w = row_w + scaled(BLOCK_GAP)
    # (each slot measured up or down her skin from the line at its own station, so the rows keep their spacing on
    #  the skin where her side curves differently fore and aft)
    for k in range(MOST_BOMBS):
        block, j = divmod(k, PER_ROW * ROWS)
        row, col = divmod(j, PER_ROW)
        x = x_front - block * block_w - (col * (scaled(BOMB_W) + scaled(GAP)) + (scaled(FIVE) if col >= 5 else 0.0) + 0.5 * scaled(BOMB_W))
        z = skin_from(x, line, -(scaled(LINE) + row * (scaled(2.0 * BOMB_R) + scaled(ROW_GAP)) + scaled(BOMB_R)))
        bombs.append(slot(x, z))
    for k in range(MOST_KILLS):
        row, col = divmod(k, PER_ROW)
        x = x_front - (col * (scaled(2.0 * KILL_R) + scaled(KILL_GAP)) + scaled(KILL_R))
        z = skin_from(x, line, scaled(LINE) + row * (scaled(2.0 * KILL_R) + scaled(KILL_ROW_GAP)) + scaled(KILL_R))
        kills.append(slot(x, z))

    # ---- what lies under them: her own painted skin, clear of her markings and her nose art ----
    trouble = []

    def look(kind, k, p, n, r, w):
        for du, dv in ((0, 0), (-w, 0), (w, 0), (0, -r), (0, r)):
            up = Vector((0, 0, 1)) if abs(n.z) < 0.7 else Vector((1, 0, 0))
            v = (up - n * up.dot(n)).normalized()
            u = v.cross(n)
            q = p + u * du + v * dv
            paint, short, _ = under(q, n)
            if paint not in skin_paints and paint not in DETAIL and not (paint or "").endswith(".on"):
                trouble.append((kind, k, "lands on " + str(paint)))
                return
            if short < -PROUD:
                trouble.append((kind, k, "under something %.2f m proud of her skin (%s)" % (-short, paint)))
                return
        for mk in marks:
            at = Vector((mk["at"][0], -mk["at"][1], mk["at"][2]))
            if mk.get("facing") and mk["facing"][1] > 0.5:                     # (a mark on her starboard side)
                continue
            reach_mk = mk["radius"] * (2.1 if str(mk.get("tag", "")).endswith("BAR") else 1.0) + 0.1
            if (at - p).length < reach_mk + r:
                trouble.append((kind, k, "too near her marking %s" % mk.get("tag")))
                return
        if art:                                                                  # (the painting itself: its patch less the margin it is painted within)
            (ax, az), aw, ah = art
            if abs(p.x - ax) < 0.5 * aw * (1.0 - 2.0 * ART_MARGIN) + w + 0.02 and abs(p.z - az) < 0.5 * ah * (1.0 - 2.0 * ART_MARGIN) + r + 0.02:
                trouble.append((kind, k, "on her nose art"))

    for k, (p, n) in enumerate(bombs):
        look("missions", k, p, n, scaled(BOMB_R), 0.5 * scaled(BOMB_W))
    for k, (p, n) in enumerate(kills):
        look("kills", k, p, n, scaled(KILL_R), scaled(KILL_R))

    # ---- the finish under the panel: her paint under the middle of its first block (the art director, 19:48Z) ----
    mid_p, mid_n = side(x_front - 0.5 * row_w, along(line, -0.5 * (bomb_h + scaled(LINE))))
    paint, _, face = under(mid_p, mid_n)
    paint = paint if paint in model.paints else "upper"
    paint = paint[:-3] if paint.endswith(".on") else paint
    blend = 0.0                                                                  # (how far her coat is into its second colour there: a mottle, a patch)
    if paint == "upper" and face is not None and len(getattr(model, "vblend", ())) == len(model.verts):
        corners = model.faces[face]
        blend = sum(model.vblend[v] for v in corners) / len(corners)
    colour = worn_at({"under": paint, "blend": blend}, model.paints)

    def game(p, n):
        return [round(v, 4) for v in (p.x, -p.y, p.z, n.x, -n.y, n.z)]          # (the game's axes: y to the right; to a tenth of a millimetre)

    hb, hk = hand(name)
    return {
        "missions": {"tag": "BOMB", "radius": round(scaled(BOMB_R), 4), "per_row": PER_ROW, "rows": ROWS, "blocks": BLOCKS,
                     "slots": [game(p, n) for p, n in bombs]},
        "kills": {"tag": "KILL-<victim>, or her nation's own (rules.own)", "radius": round(scaled(KILL_R), 4), "per_row": PER_ROW,
                  "rows": KILL_ROWS, "slots": [game(p, n) for p, n in kills]},
        "panel": {"front": round(x_front, 3), "line": round(line, 3), "low": round(low, 3), "scale": scale, "clear": [round(bottom, 3), round(top, 3)], "fit": fit,
                  "middle": game(mid_p, mid_n), "under": paint, "blend": round(blend, 3), "colour": colour, "finish": finish_of(colour),
                  "note": "the fixed line: kills above it, bombs below it, the panel's front edge at x `front` (her "
                          "own metres); `middle` is the middle of its first block, where the light test reads the "
                          "paint she wears (rules.finish): `under` is her paint there and `blend` how far her coat is "
                          "into `upper2` there when that is `upper` (a mottle or a patch); `colour` and `finish` are "
                          "that paint as she is built. A plain machine wears her owner's colours: mix those the same "
                          "way and test that"},
        "hand": {"missions": hb, "kills": hk},
        "rules": RULES,
        "_trouble": trouble,
    }
