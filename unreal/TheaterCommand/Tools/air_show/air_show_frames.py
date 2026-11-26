#!/usr/bin/env python3
"""Check pictures of a played air battle (the track air_show_test writes): not art, a look at the flight.

  python3 Tools/air_show/air_show_frames.py <key>-track.csv [out-dir]

1. The whole battle from above: every aircraft's path, the falls, the flak, the ships.
2. The thick of the fight at the moment the most guns fire, seen from the side and above, each aircraft drawn as a
   cross of fuselage and wings at its true heading, climb and bank, its guns' lines, the flak, the parachutes.
3. Three fighters' passes up close: from above and from the side, the path shaded by bank.
"""
import csv
import math
import os
import sys
from collections import defaultdict

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

RAID, DEFENCE = "#b0563a", "#3a6ab0"


def load(path):
    rows = []
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            for k in ("t", "x", "y", "z", "heading", "pitch", "bank"):
                r[k] = float(r[k]) if r[k] not in ("", None) else 0.0
            rows.append(r)
    return rows


def axes_of(h, p, b):
    h, p, b = math.radians(h), math.radians(p), math.radians(b)
    fwd = (math.sin(h) * math.cos(p), math.cos(h) * math.cos(p), math.sin(p))
    right = (math.cos(h), -math.sin(h), 0.0)
    up = (right[1] * fwd[2] - right[2] * fwd[1], right[2] * fwd[0] - right[0] * fwd[2], right[0] * fwd[1] - right[1] * fwd[0])
    rb = tuple(right[i] * math.cos(b) - up[i] * math.sin(b) for i in range(3))
    return fwd, rb


def main():
    path = sys.argv[1]
    out = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(path) or "."
    key = os.path.basename(path).replace("-track.csv", "")
    rows = load(path)
    planes = [r for r in rows if r["kind"] == "plane"]
    fx = [r for r in rows if r["kind"].startswith("fx:")]
    things = [r for r in rows if r["kind"].startswith("thing:")]

    # 1. From above.
    paths = defaultdict(list)
    for r in planes:
        if r["shown"] == "1" and r["ground"] == "0":
            paths[r["key"]].append(r)
    fig, ax = plt.subplots(figsize=(9, 9))
    for k, pts in paths.items():
        col = RAID if pts[0]["side"] == "0" else DEFENCE
        ax.plot([p["x"] / 1000 for p in pts], [p["y"] / 1000 for p in pts], color=col, lw=0.35, alpha=0.55)
    fl = [r for r in fx if r["kind"] == "fx:flak"]
    ax.scatter([r["x"] / 1000 for r in fl], [r["y"] / 1000 for r in fl], s=3, c="#555", alpha=0.35, label="flak bursts")
    falls = [r for r in fx if r["kind"] in ("fx:blast", "fx:splash", "fx:crash")]
    ax.scatter([r["x"] / 1000 for r in falls], [r["y"] / 1000 for r in falls], s=14, marker="x", c="k", lw=0.8, label="blasts and splashes")
    bombs = [r for r in fx if r["kind"] == "fx:bombhit"]
    ax.scatter([r["x"] / 1000 for r in bombs], [r["y"] / 1000 for r in bombs], s=10, c="#d08000", label="bombs landing")
    ax.set_aspect("equal")
    ax.set_xlabel("km east")
    ax.set_ylabel("km north")
    ax.set_title(f"{key}: every aircraft's path from above (raid red, defence blue)")
    ax.legend(loc="lower right", fontsize=8)
    fig.tight_layout()
    fig.savefig(os.path.join(out, f"{key}-1-from-above.png"), dpi=110)
    plt.close(fig)

    # 2. The thick of the fight: the sample with the most gun lines.
    guns_at = defaultdict(list)
    for r in fx:
        if r["kind"] == "fx:gun":
            guns_at[r["t"]].append(r)
    if guns_at:
        t = max(guns_at, key=lambda k: len(guns_at[k]))
        now = [r for r in planes if abs(r["t"] - t) < 1e-6 and r["shown"] == "1"]
        gx = [r for r in guns_at[t]]
        cx = sum(r["x"] for r in gx) / len(gx)
        cy = sum(r["y"] for r in gx) / len(gx)
        cz = sum(r["z"] for r in gx) / len(gx)
        near = [r for r in now if abs(r["x"] - cx) < 2500 and abs(r["y"] - cy) < 2500]
        fig = plt.figure(figsize=(12, 7))
        ax3 = fig.add_subplot(111, projection="3d")
        for r in near:
            fwd, rb = axes_of(r["heading"], r["pitch"], r["bank"])
            span = float(r["extra"] or 12.0) * (4.0 if r["fighter"] == "1" else 3.0)  # drawn large to read
            col = RAID if r["side"] == "0" else DEFENCE
            if r["falling"] == "1":
                col = "#222"
            x, y, z = r["x"], r["y"], r["z"]
            ax3.plot([x - fwd[0] * span * 0.8, x + fwd[0] * span * 0.8], [y - fwd[1] * span * 0.8, y + fwd[1] * span * 0.8],
                     [z - fwd[2] * span * 0.8, z + fwd[2] * span * 0.8], color=col, lw=1.6)
            ax3.plot([x - rb[0] * span, x + rb[0] * span], [y - rb[1] * span, y + rb[1] * span], [z - rb[2] * span, z + rb[2] * span],
                     color=col, lw=1.6)
        for r in gx:
            if abs(r["x"] - cx) > 2500 or abs(r["y"] - cy) > 2500:
                continue
            ex, ey, ez = (float(v) for v in r["extra"].split(";"))
            ax3.plot([r["x"], ex], [r["y"], ey], [r["z"], ez], color="#e8b400", lw=0.7)
        fl = [r for r in fx if r["kind"] == "fx:flak" and abs(r["t"] - t) < 6 and abs(r["x"] - cx) < 2500 and abs(r["y"] - cy) < 2500]
        ax3.scatter([r["x"] for r in fl], [r["y"] for r in fl], [r["z"] for r in fl], s=60, c="#444", alpha=0.35)
        ch = [r for r in things if r["kind"] == "thing:chute" and abs(r["t"] - t) < 1e-6]
        ax3.scatter([r["x"] for r in ch], [r["y"] for r in ch], [r["z"] for r in ch], s=25, c="white", edgecolors="k", lw=0.5)
        ax3.set_xlim(cx - 2500, cx + 2500)
        ax3.set_ylim(cy - 2500, cy + 2500)
        ax3.set_zlim(cz - 1500, cz + 1500)
        ax3.view_init(elev=14, azim=-60)
        ax3.set_title(f"{key}: the fight at battle second {t:.0f} ({len(near)} aircraft, {len(gx)} guns firing; aircraft drawn 4x size)")
        fig.tight_layout()
        fig.savefig(os.path.join(out, f"{key}-2-the-fight.png"), dpi=110)
        plt.close(fig)

    # 3. Three fighters' passes: the fighters that fired most, 90 battle seconds round their busiest moment.
    fired = defaultdict(int)
    for r in fx:
        if r["kind"] == "fx:gun":
            fired[r["key"]] += 1
    by_key = defaultdict(list)
    for r in planes:
        by_key[r["key"]].append(r)
    # gun keys are not plane keys: pick the fighters nearest the busiest gun moments instead.
    busiest = sorted(guns_at, key=lambda k: -len(guns_at[k]))[:3] if guns_at else []
    picks = []
    for t in busiest:
        g = guns_at[t][0]
        best = min((r for r in planes if abs(r["t"] - t) < 1e-6 and r["fighter"] == "1"),
                   key=lambda r: (r["x"] - g["x"]) ** 2 + (r["y"] - g["y"]) ** 2 + (r["z"] - g["z"]) ** 2, default=None)
        if best and best["key"] not in [p[0] for p in picks]:
            picks.append((best["key"], t))
    if picks:
        fig, axs = plt.subplots(len(picks), 2, figsize=(12, 4 * len(picks)), squeeze=False)
        for i, (k, t) in enumerate(picks):
            pts = [r for r in by_key[k] if t - 45 <= r["t"] <= t + 45 and r["shown"] == "1"]
            if not pts:
                continue
            col = [min(1.0, abs(r["bank"]) / 90.0) for r in pts]
            a = axs[i][0]
            a.scatter([r["x"] for r in pts], [r["y"] for r in pts], c=col, cmap="viridis", s=6)
            a.plot([r["x"] for r in pts], [r["y"] for r in pts], color="#999", lw=0.4)
            a.set_aspect("equal")
            a.set_title(f"fighter {k} from above, {t - 45:.0f} to {t + 45:.0f} s (colour: bank, yellow 90 degrees)", fontsize=9)
            b = axs[i][1]
            b.plot([r["t"] for r in pts], [r["z"] for r in pts], color="#333", lw=1)
            b2 = b.twinx()
            b2.plot([r["t"] for r in pts], [r["bank"] for r in pts], color="#c06", lw=0.6)
            b.set_title("height (black, m) and bank (red, degrees) against battle time", fontsize=9)
        fig.tight_layout()
        fig.savefig(os.path.join(out, f"{key}-3-passes.png"), dpi=100)
        plt.close(fig)
    print("pictures in", out)


if __name__ == "__main__":
    main()
