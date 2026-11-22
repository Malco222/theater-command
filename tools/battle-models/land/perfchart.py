"""The picture of performance-land.json for Brian: where each named tank is thick and thin, and from how far each
gun gets through each face.

    python perfchart.py [performance-land.json] [out.png]
"""
import json
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.patches import Circle, FancyBboxPatch, Rectangle  # noqa: E402

SRC = sys.argv[1] if len(sys.argv) > 1 else "/mnt/project-files/battle-models/land/performance-land.json"
OUT = sys.argv[2] if len(sys.argv) > 2 else "/mnt/project-files/battle-models/land/armour-by-facing.png"

SURFACE, INK, INK2, MUTED, GRID = "#fcfcfb", "#0b0b0b", "#52514e", "#898781", "#e1e0d9"
# one hue, light to dark: more is darker
RAMP = ["#cde2fb", "#b7d3f6", "#9ec5f4", "#86b6ef", "#6da7ec", "#5598e7", "#3987e5", "#2a78d6", "#256abf", "#1c5cab", "#184f95", "#104281", "#0d366b"]
NEVER = "#f0efec"


def shade(x, lo, hi):
    """A value's step on the ramp and the ink that reads on it."""
    u = max(0.0, min(1.0, (x - lo) / (hi - lo)))
    i = int(round(u * (len(RAMP) - 1)))
    return RAMP[i], ("#ffffff" if i >= 6 else INK)


def plan(ax, name, title, v, size):
    """A tank seen from above, nose up, with a slab against each face as thick as that face is worth."""
    L, W = size
    w = v["worth_mm"]
    k = 1.0 / 95.0                                             # metres of slab for each mm of worth
    ax.set_xlim(-4.6, 4.6)
    ax.set_ylim(-6.5, 7.0)
    ax.set_aspect("equal")
    ax.axis("off")
    # the tank: tracks, hull, turret, gun
    for s in (-1, 1):
        ax.add_patch(FancyBboxPatch((s * W / 2 - (0.42 if s > 0 else 0.0), -L / 2), 0.42, L, boxstyle="round,pad=0,rounding_size=0.12", fc="#c3c2b7", ec="none", zorder=2))
    ax.add_patch(FancyBboxPatch((-W / 2 + 0.36, -L / 2 + 0.12), W - 0.72, L - 0.24, boxstyle="round,pad=0,rounding_size=0.18", fc="#e1e0d9", ec="#898781", lw=1.0, zorder=3))
    ax.add_patch(Circle((0, 0.25), W * 0.30, fc="#f0efec", ec="#898781", lw=1.0, zorder=4))
    ax.add_patch(Rectangle((-0.07, 0.25 + W * 0.28), 0.14, L * 0.44, fc="#898781", ec="none", zorder=4))
    gap = 0.16
    # the slabs
    for face, mm in (("front", w["front"]), ("rear", w["rear"]), ("side", w["side"])):
        fc, _ = shade(mm, 0, 140)
        t = mm * k
        if face == "front":
            ax.add_patch(FancyBboxPatch((-W / 2, L / 2 + gap), W, t, boxstyle="round,pad=0,rounding_size=0.06", fc=fc, ec="none", zorder=5))
            ax.text(W / 2 + 0.25, L / 2 + gap + t / 2, f"front  {mm} mm", ha="left", va="center", fontsize=13, color=INK, fontweight="bold")
        elif face == "rear":
            ax.add_patch(FancyBboxPatch((-W / 2, -L / 2 - gap - t), W, t, boxstyle="round,pad=0,rounding_size=0.06", fc=fc, ec="none", zorder=5))
            ax.text(W / 2 + 0.25, -L / 2 - gap - t / 2, f"rear  {mm} mm", ha="left", va="center", fontsize=13, color=INK, fontweight="bold")
        else:
            for s in (-1, 1):
                x = s * (W / 2 + gap) - (t if s < 0 else 0.0)
                ax.add_patch(FancyBboxPatch((x, -L / 2), t, L, boxstyle="round,pad=0,rounding_size=0.06", fc=fc, ec="none", zorder=5))
            ax.text(W / 2 + gap + t + 0.25, -0.6, f"side  {mm} mm", ha="left", va="center", fontsize=13, color=INK, fontweight="bold")
    ax.text(-4.5, 6.9, title, ha="left", va="top", fontsize=17, color=INK, fontweight="bold")
    weak = v["weaker_than_front"]
    burn = v["fire_when_pierced"]
    ax.text(-4.5, 6.05, f"side {weak['side']:.1f}x and rear {weak['rear']:.1f}x weaker than her front", ha="left", va="top", fontsize=11.5, color=INK2)
    ax.text(0, -5.95, f"pierced from behind she burns {round(burn['rear'] * 100)} times in 100\n(from the front: {round(burn['front'] * 100)})", ha="center", va="bottom", fontsize=11, color=INK2, linespacing=1.3)


def table(ax, d, title, shooters, targets, faces, note):
    """From how far each gun gets through: one row a gun, one cell a face."""
    R = d["reach"]["table"]
    ax.axis("off")
    ncol = len(targets) * len(faces)
    x0, cw, ch = 3.4, 1.0, 0.62
    ax.set_xlim(0, x0 + ncol * cw + 0.3 * (len(targets) - 1) + 0.1)
    ax.set_ylim(-(len(shooters)) * ch - 0.5, 2.2)
    ax.text(0, 2.1, title, ha="left", va="top", fontsize=17, color=INK, fontweight="bold")
    ax.text(0, 1.5, note, ha="left", va="top", fontsize=11.5, color=INK2)
    for ti, (t, tl) in enumerate(targets):
        gx = x0 + ti * (len(faces) * cw + 0.3)
        ax.text(gx + len(faces) * cw / 2, 0.78, tl, ha="center", va="bottom", fontsize=13, color=INK, fontweight="bold")
        for fi, f in enumerate(faces):
            ax.text(gx + fi * cw + cw / 2, 0.22, f, ha="center", va="bottom", fontsize=11, color=MUTED)
    for si, (s, sl) in enumerate(shooters):
        y = -si * ch
        ax.text(x0 - 0.2, y - ch / 2 + 0.04, sl, ha="right", va="center", fontsize=12.5, color=INK)
        for ti, (t, tl) in enumerate(targets):
            gx = x0 + ti * (len(faces) * cw + 0.3)
            for fi, f in enumerate(faces):
                m = R[s][t][f]
                if m <= 0:
                    fc, ink, text = NEVER, MUTED, "never"
                else:
                    fc, ink = shade(m, 0, 2500)
                    text = "any range" if m >= 2500 else f"{m:,} m"
                ax.add_patch(FancyBboxPatch((gx + fi * cw + 0.03, y - ch + 0.07), cw - 0.06, ch - 0.08, boxstyle="round,pad=0,rounding_size=0.06", fc=fc, ec="none"))
                ax.text(gx + fi * cw + cw / 2, y - ch / 2 + 0.03, text, ha="center", va="center", fontsize=11.5, color=ink)


def main():
    d = json.load(open(SRC))
    V = d["vehicles"]
    fig = plt.figure(figsize=(16.5, 22.0), dpi=110, facecolor=SURFACE)
    fig.text(0.04, 0.975, "Where each tank is thick, and where she is thin", ha="left", va="top", fontsize=26, color=INK, fontweight="bold")
    fig.text(0.04, 0.951, "Each blue slab is as thick as that face's armour is worth: millimetres of upright plate against a 75 mm shot, slope counted in. "
             "Seen from above, nose up.", ha="left", va="top", fontsize=13, color=INK2)
    tanks = [("Tiger", "Tiger", (6.3, 3.7)), ("Panther", "Panther", (6.9, 3.4)), ("PanzerIV", "Panzer IV", (5.9, 2.9)),
             ("Sherman", "Sherman (75 mm)", (5.9, 2.6)), ("T34", "T-34", (6.1, 3.0)), ("T34-85", "T-34-85", (6.1, 3.0))]
    for i, (name, title, size) in enumerate(tanks):
        r, c = divmod(i, 3)
        ax = fig.add_axes([0.035 + c * 0.318, 0.675 - r * 0.262, 0.31, 0.262])
        plan(ax, name, title, V[name], size)
    ax = fig.add_axes([0.04, 0.205, 0.92, 0.185])
    table(ax, d, "From how far can each Allied gun get through?",
          [("Sherman", "Sherman, 75 mm"), ("M10", "M10, 3-inch"), ("T34", "T-34, 76 mm"), ("T34-85", "T-34-85, 85 mm"), ("AntiTankGun", "57 mm anti-tank gun")],
          [("Tiger", "against a Tiger"), ("Panther", "against a Panther"), ("PanzerIV", "against a Panzer IV")], ("front", "side", "rear"),
          "The farthest range at which half the hits on that face come through. Darker is farther. This is why you go for the flank and the back.")
    ax = fig.add_axes([0.04, 0.045, 0.92, 0.155])
    table(ax, d, "And the other way: from how far the German guns get through",
          [("Tiger", "Tiger, 88 mm"), ("Panther", "Panther, long 75 mm"), ("PanzerIV", "Panzer IV, 75 mm")],
          [("Sherman", "against a Sherman"), ("T34", "against a T-34"), ("T34-85", "against a T-34-85")], ("front", "side", "rear"),
          "From the front they kill at ranges where the Allied tanks cannot answer: closing the range and getting round the side is the only cure.")
    fig.text(0.04, 0.012, "From battle-models/land/performance-land.json: standard published plate and gun figures, checked by the historian on 22 November. "
             "How often a tank burns is a judgment from wartime surveys.", ha="left", va="bottom", fontsize=10, color=MUTED)
    fig.savefig(OUT, facecolor=SURFACE)
    print(OUT)


main()
