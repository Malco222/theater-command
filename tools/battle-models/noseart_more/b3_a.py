"""Batch 3, numbers 81 to 87 (painter b3_a): a gander and her goslings, a kitchen sink with wings, a four o'clock
alarm clock, a woman under her umbrella in the rain, a skier with her skis, a pair of winged dice and a singing barber
pole. Each picture is drawn from flat shapes with noseart's Drawing API and its kit; the helpers below are built the
same way, for these paintings."""
from noseart import *  # noqa: F401,F403

import math  # noqa: E402

from noseart_more._kit import *  # noqa: F401,F403,E402


def _P(x, y, k, s=1):
    """A local frame: (u, v) in units of k from (x, y), u mirrored by s."""
    return lambda u, v: (x + s * u * k, y + v * k)


# ---- 81: the gander and her goslings ----------------------------------------------------------------------------------
def goose(d, x, y, size, s=-1, body="art_white", wing="art_lgrey", bill="art_orange", scarf=None):
    """A farmyard goose walking, her feet at (x, y), `size` tall to the crown, facing s (-1: to the left, forward on
    her side): a boat of a body, a long neck held proud, an orange bill and orange webbed feet, her folded wing laid
    along her side in three scalloped rows; `scarf` ties an airman's scarf round the foot of her neck."""
    k = size / 20.0
    P = _P(x, y, k, s)                                   # (u forward, toward the way she faces)
    Q = lambda pts: [P(u, v) for u, v in pts]            # noqa: E731
    # her legs and feet, behind the body: the near leg striding forward, the far one back
    for (hu, hv), (fu, fv), col in (((0.6, 4.6), (-0.2, 0.5), bill), ((-1.6, 4.6), (-3.0, 0.9), bill)):
        d.limb(col, Q([(hu, hv), ((hu + fu) / 2 + 0.3, (hv + fv) / 2), (fu, fv)]), [0.55 * k, 0.45 * k, 0.4 * k], ink=d.ink * 0.8)
        d.fill(col, Q([(fu - 0.4, fv + 0.3), (fu + 2.4, fv + 0.1), (fu + 2.0, fv - 0.5), (fu + 1.2, fv - 0.2), (fu + 0.6, fv - 0.6),
                       (fu - 0.6, fv - 0.5)]), ink=d.ink * 0.8, steps=3)
    # the body: breast forward and low, the back long and flat, the tail lifted
    d.fill(body, Q([(4.4, 10.2), (5.8, 8.2), (5.4, 5.6), (3.0, 3.8), (-1.0, 3.4), (-5.0, 4.2), (-8.0, 6.4), (-9.8, 9.4),
                    (-10.6, 11.2), (-9.0, 11.0), (-6.0, 10.8), (-2.0, 11.2), (1.4, 11.2)]), steps=5)
    # the neck, rising from the breast in a gentle S, and the head
    d.limb(body, Q([(3.2, 9.4), (4.4, 12.6), (3.8, 15.6), (4.2, 18.0)]), [2.1 * k, 1.35 * k, 1.1 * k, 1.15 * k], ink=d.ink)
    d.oval(body, *P(4.6, 18.4), 1.75 * k, 1.45 * k, turn=-s * 8)
    # the bill: a long orange wedge, its gape line, its nostril
    d.fill(bill, Q([(5.8, 19.2), (8.8, 18.0), (9.0, 17.5), (6.0, 17.1), (5.6, 17.9)]), ink=d.ink * 0.85, steps=2)
    d.line("art_ink", Q([(6.2, 17.95), (8.6, 17.65)]), 0.22 * k)
    d.oval("art_ink", *P(6.9, 18.65), 0.18 * k, 0.12 * k, ink=0.0)
    # her eye, bright and proud
    d.oval("art_ink", *P(4.9, 18.75), 0.38 * k, 0.42 * k, ink=0.0)
    d.oval("art_white", *P(5.05, 18.95), 0.12 * k, 0.12 * k, ink=0.0)
    # the folded wing: three rows of scallops, laid back along her side
    d.fill(wing, Q([(3.0, 9.8), (2.0, 7.2), (-1.0, 6.2), (-5.2, 6.8), (-9.0, 8.8), (-10.0, 10.6), (-6.0, 10.4), (-1.0, 10.6)]), ink=d.ink * 0.8, steps=4)
    for row, (v0, us) in enumerate(((8.6, (1.2, -1.2, -3.6, -6.0)), (7.4, (-0.4, -2.8, -5.2, -7.6)))):
        for u in us:
            d.line("art_ink", Q([(u + 1.1, v0 + 0.4), (u, v0 - 0.4), (u - 1.1, v0 + 0.2)]), 0.24 * k)
    if scarf:
        d.fill(scarf, Q([(2.6, 10.6), (4.2, 11.4), (5.8, 11.0), (5.4, 10.0), (3.6, 9.6)]), ink=d.ink * 0.8, steps=3)
        d.fill(scarf, Q([(2.8, 10.6), (0.4, 11.6), (-2.6, 12.8), (-5.0, 12.4), (-4.0, 13.6), (-6.6, 13.8), (-3.4, 14.4), (-0.6, 13.0),
                         (2.4, 11.6)]), ink=d.ink * 0.8, steps=4)
    return P


def gosling(d, x, y, size, s=-1, down="art_yellow", bill="art_orange", step=0):
    """A gosling toddling after her, feet at (x, y), `size` tall: a round ball of yellow down, a round head, a stub of
    an orange bill, two orange feet; `step` 0 or 1 picks which foot is forward."""
    k = size / 10.0
    P = _P(x, y, k, s)
    Q = lambda pts: [P(u, v) for u, v in pts]            # noqa: E731
    fwd, back = ((0.9, -0.9), (-0.9, 0.9))[step]
    for fu in (back, fwd):
        d.limb(bill, Q([(fu * 0.4, 3.0), (fu, 0.4)]), [0.42 * k, 0.36 * k], ink=d.ink * 0.7)
        d.fill(bill, Q([(fu - 0.4, 0.5), (fu + 1.6, 0.4), (fu + 1.4, -0.2), (fu - 0.5, -0.2)]), ink=d.ink * 0.7, steps=2)
    d.fill(down, Q([(2.6, 5.4), (2.8, 3.4), (1.2, 1.9), (-1.8, 2.0), (-3.4, 3.4), (-4.4, 5.2), (-3.2, 5.0), (-3.6, 6.2), (-1.0, 6.6), (1.4, 6.4)]), steps=4)
    d.oval(down, *P(2.0, 7.8), 2.2 * k, 2.0 * k)
    d.fill(bill, Q([(3.8, 8.3), (5.3, 7.7), (3.9, 7.1)]), ink=d.ink * 0.8, steps=2)
    d.oval("art_ink", *P(2.6, 8.4), 0.36 * k, 0.4 * k, ink=0.0)
    d.line("art_ink", Q([(-0.4, 4.8), (-1.6, 4.0), (-2.6, 4.4)]), 0.28 * k)   # (its stub of a wing)


@painting(81)
def p81(d, a):   # a goose leading three goslings
    goose(d, 30, 4, 62)
    for i, (gx, sz) in enumerate(((60, 17.0), (78, 16.0), (95, 15.0))):
        gosling(d, gx, 4 + i * 0.6, sz, step=i % 2)
    names(d, a, (92, 61), 15, lines=["Gander's", "Gang"], gap=1.38, indent=8)


def _T(x, y, turn=0.0, k=1.0, s=1):
    """A local frame at (x, y): (u, v) in units of k, u mirrored by s, the whole turned `turn` degrees."""
    c, sn = math.cos(math.radians(turn)), math.sin(math.radians(turn))
    return lambda u, v: (x + (s * u * k) * c - (v * k) * sn, y + (s * u * k) * sn + (v * k) * c)


def _clip_box(poly, x0, y0, x1, y1):
    """A polygon clipped to the box x0..x1, y0..y1 (in its own frame), edge by edge."""
    def clip(pts, inside, cross):
        out = []
        for i in range(len(pts)):
            p, q = pts[i - 1], pts[i]
            if inside(q):
                if not inside(p):
                    out.append(cross(p, q))
                out.append(q)
            elif inside(p):
                out.append(cross(p, q))
        return out

    def at_x(xc):
        return lambda p, q: (xc, p[1] + (q[1] - p[1]) * (xc - p[0]) / ((q[0] - p[0]) or 1e-9))

    def at_y(yc):
        return lambda p, q: (p[0] + (q[0] - p[0]) * (yc - p[1]) / ((q[1] - p[1]) or 1e-9), yc)
    pts = list(poly)
    for inside, cross in ((lambda p: p[0] >= x0, at_x(x0)), (lambda p: p[0] <= x1, at_x(x1)),
                          (lambda p: p[1] >= y0, at_y(y0)), (lambda p: p[1] <= y1, at_y(y1))):
        if not pts:
            break
        pts = clip(pts, inside, cross)
    return pts


# ---- 82: the kitchen sink with wings ------------------------------------------------------------------------------------
def kitchen_sink(d, x, y, w, turn=0.0, enamel="art_white", rim="art_lgrey", wall="art_grey", hole="art_dark", steel="art_lsteel"):
    """A farmhouse kitchen sink in white enamel seen from the front and a little above, its apron's top middle at (x, y),
    `w` wide, turned `turn` degrees: the apron front, the broad rim round the deep basin (its far wall lit, its floor in
    shadow, the drain in it), the backsplash at the back with a gooseneck faucet arching over the basin and two
    cross-handled taps, and the drain's trap hanging under it. Returns its frame (u across, v up, grid units)."""
    G = _T(x, y, turn)
    Q = lambda pts: [G(u, v) for u, v in pts]            # noqa: E731
    hw, ha, D, sk = w / 2.0, w * 0.2, w * 0.36, w * 0.1   # (half its width, the apron's depth, the rim's depth, the slant of its sides)
    # (the trap under it: down from the drain, round the U, and back up and away)
    d.limb(steel, Q([(sk * 0.5, -ha + 0.5), (sk * 0.5, -ha - 4.5), (sk * 0.5 + 1.2, -ha - 7.6), (sk * 0.5 + 4.0, -ha - 8.0), (sk * 0.5 + 5.6, -ha - 5.8),
                     (sk * 0.5 + 6.6, -ha - 4.2), (sk * 0.5 + 11.0, -ha - 4.0)]), [1.3, 1.3, 1.3, 1.3, 1.25, 1.2, 1.2], ink=d.ink * 0.8)
    # (the backsplash at the back, its top rolled)
    d.fill(enamel, Q([(-hw + sk, D - 0.5), (-hw + sk, D + 7.0), (-hw + sk + 1.4, D + 8.2), (hw + sk - 1.4, D + 8.2), (hw + sk, D + 7.0),
                      (hw + sk, D - 0.5)]), steps=2)
    d.line(rim, Q([(-hw + sk + 1.0, D + 6.6), (hw + sk - 1.0, D + 6.6)]), 0.8, taper=False)
    # (the apron front, square, a shine down its face)
    d.fill(enamel, Q([(-hw, -ha), (-hw, 0.0), (hw, 0.0), (hw, -ha)]), steps=1)
    d.line(rim, Q([(-hw * 0.6, -1.4), (-hw * 0.6, -ha + 1.6)]), 1.1)
    # (the rim, seen from above, and the basin in it: the far wall lit, the floor in shadow, the drain)
    d.shape(enamel, Q([(-hw, 0.0), (hw, 0.0), (hw + sk, D), (-hw + sk, D)]))
    i0, i1 = 2.4, 2.6
    d.shape(wall, Q([(-hw + i0 + 0.3, 1.8), (hw - i0 + 0.3, 1.8), (hw + sk - i1, D - 2.0), (-hw + sk + i1, D - 2.0)]), ink=d.ink * 0.8)
    d.shape(hole, Q([(-hw + i0 + 1.2, 1.8), (hw - i0 - 0.4, 1.8), (hw + sk * 0.6 - i1 - 1.0, D * 0.55), (-hw + sk * 0.6 + i1 + 1.0, D * 0.55)]), ink=0.0)
    d.oval(steel, *G(sk * 0.25, D * 0.3), 1.9, 1.2, turn=turn, ink=d.ink * 0.6)
    d.oval("art_black", *G(sk * 0.25, D * 0.3), 0.9, 0.55, turn=turn, ink=0.0)
    # (the taps on the back rim, one each side, their cross handles)
    for tu in (-hw * 0.52, hw * 0.52):
        cu = tu + sk * 0.95
        d.limb(steel, Q([(cu, D - 1.0), (cu, D + 2.6)]), [1.3, 1.2], ink=d.ink * 0.7)
        d.line(steel, Q([(cu - 3.4, D + 3.0), (cu + 3.4, D + 3.0)]), 1.3, taper=False)
        for ku in (-3.4, 3.4):
            d.oval(steel, *G(cu + ku, D + 3.0), 1.2, 1.2, ink=d.ink * 0.6)
        d.oval("art_white", *G(cu, D + 3.6), 1.1, 1.1, ink=d.ink * 0.6)
    # (the gooseneck faucet, up from the back rim's middle and arching forward over the basin)
    cu = sk * 0.95
    d.oval(steel, *G(cu, D - 1.0), 2.2, 1.2, turn=turn, ink=d.ink * 0.7)
    d.limb(steel, Q([(cu, D - 1.0), (cu, D + 7.0), (cu - 0.6, D + 11.0), (cu - 3.0, D + 13.2), (cu - 6.4, D + 13.0), (cu - 8.4, D + 10.6),
                     (cu - 8.6, D + 8.4)]), [2.0, 1.9, 1.8, 1.7, 1.65, 1.6, 1.6], ink=d.ink * 0.85)
    d.limb(steel, Q([(cu - 8.6, D + 8.8), (cu - 8.6, D + 6.8)]), [2.0, 2.0], ink=d.ink * 0.85)        # (its spout's nozzle)
    for dv, dr in ((4.6, 0.95), (2.0, 0.75)):           # (a drip or two, falling to the basin)
        d.oval("art_sky", *G(cu - 8.6, D + dv), dr * 0.8, dr, turn=turn, ink=d.ink * 0.5)
    return G


@painting(82)
def p82(d, a):   # a kitchen sink with wings falling among the bombs
    x, y, w, turn = 30.0, 30.0, 40.0, -12.0
    G = _T(x, y, turn)
    D = w * 0.36
    # (its wings, spread up from the backsplash's two ends, the far one behind)
    wing(d, G(-w * 0.36, D + 3.0), 136 + turn, 30, 11, "art_lgrey", shade="art_grey", spread=-30)
    wing(d, G(w * 0.5, D + 3.0), 44 + turn, 32, 11.5, "art_white", spread=30)
    kitchen_sink(d, x, y, w, turn)
    # (the bombs it falls among: two, nose down, with the rush of their fall above them)
    for bx, by, bl in ((-6.0, 16.0, 24.0), (57.0, -2.0, 21.0)):
        bomb(d, bx, by, bl, turn=-90)
        speed(d, bx, by + bl + 2.5, 8.0, n=2, gap=3.2, paint="art_white", turn=90, w=0.9)
    names(d, a, (131, 36), 16.5, lines=["The", "Kitchen", "Sink"], gap=1.34)


# ---- 83: the four o'clock alarm ------------------------------------------------------------------------------------------
def alarm_clock(d, x, y, r, body="art_red", dial="art_white", bell="art_lsteel", turn=0.0):
    """A twin-bell alarm clock, its dial's middle at (x, y), `r` the dial's radius, rocking `turn` degrees as it rings:
    the round case, its two bells on top and the hammer between them, two stubby feet; its dial a bleary face, eyes
    half shut over heavy bags and a yawn, its hands at four o'clock (the hour hand at the four, the minute at twelve),
    and tick marks for the hours, no figures. Returns its frame."""
    G = _T(x, y, turn)
    Q = lambda pts: [G(u, v) for u, v in pts]            # noqa: E731
    # (its feet and the bells' stalks, behind the case)
    for su in (-1, 1):
        d.limb(bell, Q([(su * r * 0.62, -r * 0.9), (su * r * 0.9, -r * 1.32)]), [r * 0.11, r * 0.13], ink=d.ink * 0.8)
        d.oval(bell, *G(su * r * 0.93, -r * 1.36), r * 0.2, r * 0.1, turn=turn, ink=d.ink * 0.8)
        d.limb(bell, Q([(su * r * 0.5, r * 0.95), (su * r * 0.78, r * 1.22)]), [r * 0.07, r * 0.07], ink=d.ink * 0.7)
    # (the hammer's post, up from the case's top, the hammer between the bells)
    d.limb(bell, Q([(0.0, r * 1.05), (0.0, r * 1.5)]), [r * 0.06, r * 0.06], ink=d.ink * 0.7)
    d.oval(bell, *G(0.0, r * 1.56), r * 0.16, r * 0.1, turn=turn, ink=d.ink * 0.7)
    # (the bells: two domes leaning out)
    for su in (-1, 1):
        cx, cy = su * r * 0.86, r * 1.3
        tilt = -su * 32.0
        Bq = _T(*G(cx, cy), turn + tilt)
        d.fill(bell, [Bq(u * r, v * r) for u, v in ((-0.46, -0.1), (-0.44, 0.14), (-0.34, 0.36), (-0.16, 0.5), (0.0, 0.53), (0.16, 0.5), (0.34, 0.36),
                                                    (0.44, 0.14), (0.46, -0.1), (0.0, -0.04))], steps=4)
        d.oval(bell, *Bq(0.0, r * 0.57), r * 0.08, r * 0.08, ink=d.ink * 0.6)
        d.line("art_white", [Bq(-r * 0.26, r * 0.2), Bq(-r * 0.08, r * 0.33)], r * 0.06)
    # (the case and the dial)
    d.oval(body, x, y, r * 1.12, r * 1.12)
    d.oval(dial, x, y, r * 0.9, r * 0.9, ink=d.ink * 0.8)
    for hh_ in range(12):                                # (the hours: ticks only)
        t = math.radians(90 - 30 * hh_ + turn)
        a0, a1 = (0.74, 0.86) if hh_ % 3 else (0.68, 0.86)
        d.line("art_ink", [(x + r * a0 * math.cos(t), y + r * a0 * math.sin(t)), (x + r * a1 * math.cos(t), y + r * a1 * math.sin(t))],
               r * (0.05 if hh_ % 3 else 0.075), taper=False)
    # (its bleary eyes: heavy lids half down over the pupils, bags under them)
    for su in (-1, 1):
        ex, ey = G(su * r * 0.3, r * 0.2)
        d.oval("art_white", ex, ey, r * 0.17, r * 0.15, turn=turn, ink=d.ink * 0.6)
        d.oval("art_ink", *G(su * r * 0.3 + r * 0.02, r * 0.14), r * 0.07, r * 0.07, ink=0.0)
        Lq = _T(ex, ey, turn)
        d.fill("art_pink", [Lq(u * r, v * r) for u, v in ((-0.19, 0.0), (-0.16, 0.1), (0.0, 0.16), (0.16, 0.1), (0.19, 0.0), (0.0, 0.02))],
               ink=d.ink * 0.7, steps=3)
        d.line("art_ink", [Lq(-r * 0.16, -r * 0.2), Lq(0.0, -r * 0.25), Lq(r * 0.16, -r * 0.2)], r * 0.035)
    # (its yawn)
    d.oval("art_black", *G(0.0, -r * 0.45), r * 0.15, r * 0.19, turn=turn, ink=d.ink * 0.7)
    d.oval("art_pink", *G(0.0, -r * 0.53), r * 0.09, r * 0.07, turn=turn, ink=0.0)
    # (its hands: the minute hand up to twelve, the hour hand to four)
    d.limb("art_black", Q([(0.0, 0.0), (0.0, r * 0.72)]), [r * 0.06, r * 0.025], ink=0.0)
    t4 = math.radians(90 - 120)
    d.limb("art_black", Q([(0.0, 0.0), (r * 0.5 * math.cos(t4), r * 0.5 * math.sin(t4))]), [r * 0.075, r * 0.03], ink=0.0)
    d.oval("art_black", x, y, r * 0.07, r * 0.07, ink=0.0)
    return G


@painting(83)
def p83(d, a):   # a bleary-eyed alarm clock ringing at four in the morning
    x, y, r = 32.0, 34.0, 22.0
    alarm_clock(d, x, y, r, turn=-6.0)
    for su in (-1, 1):                                   # (its ringing: three short arcs out past each bell)
        cx, cy = x + su * r * 1.25, y + r * 1.38
        for i, rr in enumerate((r * 0.42, r * 0.62, r * 0.82)):
            a0 = 20 if su > 0 else 120
            d.line("art_white", [(cx + rr * math.cos(math.radians(t)), cy + rr * math.sin(math.radians(t))) for t in range(a0, a0 + 41, 10)], 1.2)
    names(d, a, (118, 40), 16.5, lines=["Briefing", "at Four"], gap=1.4)


def moon_shape(x, y, r):
    """A crescent moon, its outer edge round (x, y), horns to the right."""
    outer = [(x + r * math.cos(math.radians(t)), y + r * math.sin(math.radians(t))) for t in range(60, 301, 12)]
    inner = [(x + r * 0.45 + r * 0.82 * math.cos(math.radians(t)), y + r * 0.82 * math.sin(math.radians(t))) for t in range(250, 109, -14)]
    return outer + inner


# ---- 84: Ruth under her umbrella --------------------------------------------------------------------------------------------
def umbrella(d, cx, cy, R, tip_from, cloth="art_red", rib="art_ink", shaft="art_lsteel", ribs=7):
    """An open umbrella, its canopy's rim middle at (cx, cy), `R` its half width: a shallow dome, its rim scalloped
    between the ribs, the ribs running up to the ferrule on top; its shaft down to `tip_from` (where her hand holds it)
    and on a little to the crook of its handle."""
    top = (cx, cy + 0.6 * R)
    ts = [180.0 - 180.0 * i / (ribs - 1) for i in range(ribs)]
    rim = [(cx + R * math.cos(math.radians(t)), cy - 0.1 * R * math.sin(math.radians(t))) for t in ts]
    # (the shaft, behind the canopy's rim; the handle's crook below her hand)
    hx, hy = tip_from
    dx, dy = hx - cx, hy - cy
    ln = math.hypot(dx, dy) or 1.0
    ux, uy = dx / ln, dy / ln
    end = (hx + ux * R * 0.18, hy + uy * R * 0.18)
    d.limb(shaft, [(cx, cy + 0.1 * R), (hx, hy), end], [R * 0.035, R * 0.035, R * 0.035], ink=d.ink * 0.7)
    d.limb("art_brown", [end, (end[0] + ux * R * 0.08, end[1] + uy * R * 0.08), (end[0] + ux * R * 0.1 + R * 0.08, end[1] + uy * R * 0.1 - R * 0.02),
                         (end[0] + R * 0.14, end[1] + R * 0.06)], [R * 0.05] * 4, ink=d.ink * 0.8)
    # (the canopy: the dome over, scallops along its rim)
    dome = [(cx + R * math.cos(math.radians(t)), cy + 0.6 * R * math.sin(math.radians(t))) for t in range(0, 181, 10)]
    scal = []
    for i in range(ribs - 1, 0, -1):
        (x0, y0), (x1, y1) = rim[i], rim[i - 1]
        scal += [(x0, y0), ((x0 + x1) / 2, (y0 + y1) / 2 + 0.11 * R)]
    scal.append(rim[0])
    canopy = dome[::-1][::-1]
    poly = [(cx + R, cy)] + dome[1:-1] + [(cx - R, cy)] + scal[::-1][1:][::-1]
    d.fill(cloth, dome + scal[1:], steps=2)
    for (rx, ry) in rim[1:-1]:
        d.line(rib, [top, ((top[0] + rx) / 2 + (rx - cx) * 0.12, (top[1] + ry) / 2 + 0.04 * R), (rx, ry)], R * 0.03)
    d.oval(shaft, top[0], top[1] + R * 0.06, R * 0.04, R * 0.08, ink=d.ink * 0.6)


@painting(84)
def p84(d, a):   # a woman in galoshes and a raincoat under a big umbrella
    x, y, h = 30.0, 6.0, 54.0
    # (the puddle she stands in, and its rings)
    d.oval("art_lblue", x + 1.0, y + 0.4, 15.0, 2.8, ink=d.ink * 0.7)
    d.line("art_sky", ellipse(x + 1.0, y + 0.6, 10.0, 1.6, 0, 24, 200, 340), 0.6)
    F = lady(d, x, y, h, 1, pose="stand", arms=("wave", "down"),
             wear={"top": "art_khaki", "coat": "art_khaki", "belt": "art_khaki", "buckle": "art_brown", "buttons": "art_brown",
                   "sleeves": "long", "boots": "art_black", "lapel": "art_khaki", "collar": "art_khaki"},
             hair="rolls", hair_paint="art_auburn")
    (hx, hy), _ = F.hand["fore"]
    c = F.P(0.3, 9.5)                                           # (the canopy's rim middle, over her head)
    umbrella(d, c[0], c[1], 22.0, (hx, hy))
    d.oval("art_skin", hx, hy + 0.3, 0.17 * F.k, 0.15 * F.k, ink=d.ink * 0.7)
    for (u, v) in ((3.0, 50.0), (8.0, 36.0), (12.0, 62.0), (2.0, 22.0), (9.0, 12.0)):   # (the rain beside her, none under it)
        d.line("art_sky", [(u + 1.6, v + 4.6), (u, v)], 1.0)
    names(d, a, (113, 25), 16.5, lines=["Rainy Day", "Ruth"], gap=1.4)


# ---- 85: the Green Mountain girl ---------------------------------------------------------------------------------------------
def ski(d, F, back, front, wide=0.12, wood="art_tan", edge="art_brown", binding="art_black", side=1, curl=None):
    """One wooden ski in her frame (heads), from its tail `back` to its tip `front`, the tip curled over to `side` (+1
    left of the way it points, -1 right) by `curl`; a dark groove down it and its binding."""
    bx, by = back
    fx, fy = front
    ln = math.hypot(fx - bx, fy - by)
    ux, uy = (fx - bx) / ln, (fy - by) / ln
    nx, ny = -uy * side, ux * side
    c = wide * 1.3 if curl is None else curl
    hw = wide * 0.5
    pts = [(bx - nx * hw + ux * hw * 0.6, by - ny * hw + uy * hw * 0.6), (bx - ux * 0.02, by - uy * 0.02),
           (bx + nx * hw + ux * hw * 0.6, by + ny * hw + uy * hw * 0.6),
           (fx - ux * c * 1.6 + nx * hw, fy - uy * c * 1.6 + ny * hw),
           (fx - ux * c * 0.7 + nx * (hw + c * 0.55), fy - uy * c * 0.7 + ny * (hw + c * 0.55)),
           (fx + nx * (hw + c), fy + ny * (hw + c)),
           (fx + nx * (hw + c * 0.7) + ux * c * 0.25, fy + ny * (hw + c * 0.7) + uy * c * 0.25),
           (fx - ux * c * 0.2 - nx * hw * 0.2, fy - uy * c * 0.2 - ny * hw * 0.2),
           (fx - ux * c * 1.4 - nx * hw, fy - uy * c * 1.4 - ny * hw)]
    d.fill(wood, F.Ps(pts), ink=d.ink * 0.8, steps=2)
    d.line(edge, F.Ps([lerp(back, front, 0.06), lerp(back, front, 0.88)]), F.r(wide * 0.16), taper=False)
    m = lerp(back, front, 0.42)
    d.shape(binding, F.Ps([(m[0] - ux * wide * 0.5 - nx * hw * 1.15, m[1] - uy * wide * 0.5 - ny * hw * 1.15),
                           (m[0] + ux * wide * 0.5 - nx * hw * 1.15, m[1] + uy * wide * 0.5 - ny * hw * 1.15),
                           (m[0] + ux * wide * 0.5 + nx * hw * 1.15, m[1] + uy * wide * 0.5 + ny * hw * 1.15),
                           (m[0] - ux * wide * 0.5 + nx * hw * 1.15, m[1] - uy * wide * 0.5 + ny * hw * 1.15)]), ink=d.ink * 0.6)


def _lady(d, x, y, h, s=1, extra_arms=None, **kw):
    """lady(), with arms of our own (name: (elbow, hand), as noseart's ARMS gives them) lent to her for this one call."""
    added = [k for k in (extra_arms or {}) if k not in ARMS]
    for k in added:
        ARMS[k] = extra_arms[k]
    try:
        return lady(d, x, y, h, s, **kw)
    finally:
        for k in added:
            ARMS.pop(k, None)


@painting(85)
def p85(d, a):   # a woman in ski clothes carrying her skis over one shoulder
    x, y, h = 34.0, 7.0, 56.0
    # (one green mountain behind her, snow on its top)
    d.fill("art_green", [(-2.0, 8.0), (4.0, 30.0), (12.0, 40.0), (18.0, 44.0), (24.0, 40.0), (34.0, 28.0), (44.0, 18.0), (56.0, 8.0)], steps=4)
    d.fill("art_white", [(9.4, 36.6), (12.0, 40.0), (18.0, 44.0), (24.0, 40.0), (26.4, 37.0), (22.0, 38.6), (19.0, 36.0), (15.6, 38.8), (12.4, 35.6)],
           ink=d.ink * 0.7, steps=3)
    # (the snow she stands on)
    d.fill("art_white", [(4.0, 4.0), (12.0, 9.6), (30.0, 10.6), (48.0, 9.2), (60.0, 4.0), (60.0, 2.0), (4.0, 2.0)], steps=4)
    Fk = Fig(d, x, y, h, 1)
    # (her skis, stood on their tails in the snow beside her, splayed a little, their tips curled out at the top)
    ski(d, Fk, (-2.62, 0.2), (-3.15, 9.35), wide=0.42, side=1, curl=0.5)
    ski(d, Fk, (-2.18, 0.15), (-1.6, 9.5), wide=0.42, side=-1, curl=0.5)
    F = _lady(d, x, y, h, 1, extra_arms={"_b3a_skis": ((0.9, -0.62), (1.22, 0.38))}, pose="stand", arms=("down", "_b3a_skis"),
              wear={"top": "art_red", "trousers": "art_navy", "sleeves": "long", "boots": "art_brown", "belt": None, "collar": "art_white"},
              hair="rolls", hair_paint="art_hair")
    R = F.r
    # her knit cap: red, its cuff turned up white, a white pompom
    d.fill("art_red", F.Ps([(0.54, 7.12), (0.5, 7.45), (0.3, 7.76), (0.0, 7.9), (-0.32, 7.84), (-0.58, 7.6), (-0.7, 7.25), (-0.72, 6.98)]), steps=4)
    d.fill("art_white", F.Ps([(0.58, 7.0), (0.58, 7.24), (-0.74, 7.08), (-0.76, 6.86)]), ink=d.ink * 0.8, steps=2)
    d.oval("art_white", *F.P(-0.12, 8.0), R(0.2), R(0.2), ink=d.ink * 0.7)
    (hx, hy), _ = F.hand["aft"]                          # (her hand round them)
    d.oval("art_skin", hx, hy, R(0.17), R(0.15), ink=d.ink * 0.7)
    names(d, a, (113, 40), 17.5, lines=["Green", "Mountain", "Girl"], gap=1.32)


# ---- 86: the winged dice --------------------------------------------------------------------------------------------------------
def die(d, x, y, size, turn=0.0, pips=4, face="art_red", top="art_lred", side="art_auburn", pip="art_white"):
    """A die in three-quarter view, its front face's middle at (x, y), `size` its edge, turned `turn` degrees: the front
    face showing `pips`, the top face lighter, the side face darker; the corners a little rounded."""
    G = _T(x, y, turn)
    e = size / 2.0
    dx, dy = size * 0.32, size * 0.26                   # (the depth: back and up to the right)
    d.fill(side, [G(e, -e), G(e + dx, -e + dy), G(e + dx, e + dy), G(e, e)], steps=1)
    d.fill(top, [G(-e, e), G(e, e), G(e + dx, e + dy), G(-e + dx, e + dy)], steps=1)
    d.fill(face, [G(-e, -e), G(-e, e), G(e, e), G(e, -e)], steps=1)
    spots = {1: [(0, 0)], 2: [(-1, 1), (1, -1)], 3: [(-1, 1), (0, 0), (1, -1)], 4: [(-1, 1), (1, 1), (-1, -1), (1, -1)],
             5: [(-1, 1), (1, 1), (0, 0), (-1, -1), (1, -1)], 6: [(-1, 1), (1, 1), (-1, 0), (1, 0), (-1, -1), (1, -1)]}[pips]
    for (su, sv) in spots:
        d.oval(pip, *G(su * e * 0.52, sv * e * 0.52), size * 0.1, size * 0.1, turn=turn, ink=d.ink * 0.5)
    d.oval(pip, *G(dx * 0.5, e + dy * 0.5), size * 0.08, size * 0.05, turn=turn, ink=d.ink * 0.4)    # (the one on its top)
    return G


@painting(86)
def p86(d, a):   # a pair of dice showing a four and a three, with wings
    for (x, y, sz, turn, pips) in ((36.0, 14.0, 19.0, -16.0, 3), (12.0, 46.0, 20.0, 14.0, 4)):   # (the low one first, behind)
        G = _T(x, y, turn)
        e = sz / 2.0
        wing(d, G(-e + sz * 0.2, e + sz * 0.2), 128 + turn, sz * 0.95, sz * 0.36, "art_lgrey", shade="art_grey", spread=-30)
        die(d, x, y, sz, turn, pips)
        wing(d, G(e * 0.4, e + sz * 0.18), 62 + turn, sz * 1.05, sz * 0.38, "art_white", spread=30)
    speed(d, -4.0, 22.0, 8.0, n=3, gap=4.0, paint="art_white", w=1.0)
    names(d, a, (100, 40), 18, lines=["Seven", "Come", "Home"], gap=1.32)


# ---- 87: the singing barber pole ------------------------------------------------------------------------------------------
def note(d, x, y, h, paint="art_white", turn=0.0):
    """A music note, brushed as a shape: a tilted round head at (x, y), its stem up `h`, one flag."""
    d.oval(paint, x, y, h * 0.2, h * 0.14, turn=20 + turn, ink=d.ink * 0.7)
    d.line(paint, [(x + h * 0.17, y + h * 0.04), (x + h * 0.17, y + h)], h * 0.08, taper=False)
    d.line(paint, [(x + h * 0.17, y + h), (x + h * 0.42, y + h * 0.78), (x + h * 0.46, y + h * 0.54)], h * 0.08)


@painting(87)
def p87(d, a):   # one singing barber pole, its mouth open on a high note
    x, y0, y1, hw = 28.0, 8.0, 64.0, 7.4
    # (its foot: a flared steel cup)
    d.fill("art_lsteel", [(x - hw - 1.4, y0 + 1.0), (x - hw - 3.0, y0 - 4.0), (x - hw - 1.0, y0 - 5.6), (x + hw + 1.0, y0 - 5.6), (x + hw + 3.0, y0 - 4.0),
                          (x + hw + 1.4, y0 + 1.0)], steps=2)
    # (the glass: white, its red and blue stripes winding up it)
    d.shape("art_white", [(x - hw, y0), (x - hw, y1), (x + hw, y1), (x + hw, y0)])
    pitch, bw, slope = 14.0, 4.6, 0.55
    for i in range(-2, 7):
        b0 = y0 + i * pitch
        for paint, off in (("art_red", 0.0), ("art_blue", pitch * 0.5)):
            band = [(x - hw, b0 + off), (x + hw, b0 + off + 2 * hw * slope), (x + hw, b0 + off + 2 * hw * slope + bw), (x - hw, b0 + off + bw)]
            cl = _clip_box(band, x - hw, y0, x + hw, y1)
            if len(cl) >= 3:
                d.shape(paint, cl, ink=0.0)
    d.shape(None, [(x - hw, y0), (x - hw, y1), (x + hw, y1), (x + hw, y0)])
    d.line("art_white", [(x - hw * 0.55, y1 - 3.0), (x - hw * 0.55, y0 + 4.0)], 1.0)      # (the glass's shine)
    # (its cap: a steel dome with a knob, and a straw boater tipped on it)
    d.fill("art_lsteel", [(x - hw - 1.4, y1 - 0.6), (x - hw - 1.4, y1 + 1.6), (x - hw * 0.8, y1 + 5.0), (x, y1 + 6.6), (x + hw * 0.8, y1 + 5.0),
                          (x + hw + 1.4, y1 + 1.6), (x + hw + 1.4, y1 - 0.6)], steps=3)
    B = _T(x + 1.0, y1 + 6.0, -10.0)
    d.fill("art_cream", [B(-13.0, 0.0), B(-12.0, 1.2), B(12.0, 1.2), B(13.0, 0.0), B(12.0, -1.0), B(-12.0, -1.0)], steps=2)
    d.shape("art_cream", [B(-7.6, 0.6), B(-7.4, 6.4), B(7.4, 6.4), B(7.6, 0.6)])
    d.shape("art_black", [B(-7.55, 1.2), B(-7.5, 3.0), B(7.5, 3.0), B(7.55, 1.2)], ink=d.ink * 0.5)
    # (its face: a white patch on the glass, eyes shut in joy, brows up, the mouth wide open on the high note)
    fy = 40.0
    d.oval("art_white", x, fy, hw * 0.92, 11.0, ink=d.ink * 0.8)
    for su in (-1, 1):
        ex = x + su * 2.7
        d.line("art_ink", [(ex - 1.8, fy + 4.0), (ex, fy + 5.2), (ex + 1.8, fy + 4.0)], 0.75)
        d.line("art_ink", [(ex - 1.6, fy + 8.0), (ex + su * 0.2, fy + 8.8), (ex + 1.8, fy + 8.2)], 0.6)
        d.oval("art_pink", x + su * 4.6, fy + 0.8, 1.4, 0.9, ink=0.0)
    d.oval("art_black", x, fy - 4.6, 3.0, 4.2, ink=d.ink * 0.8)
    d.oval("art_red", x, fy - 6.6, 1.9, 1.4, ink=0.0)
    # (the high note, rising from it)
    note(d, x + 14.0, 56.0, 9.0)
    note(d, x + 21.0, 66.0, 7.0, turn=-8)
    names(d, a, (96, 40), 15, lines=["Barbershop", "Four"], gap=1.42)
