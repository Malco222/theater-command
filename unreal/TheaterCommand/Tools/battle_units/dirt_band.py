"""The art director's band for the men's worn look (release 2, tc.Look.Worn; her ruling of 25 November 14:50Z, passed on
by the graphics thread): a man's boots take 30 in 100 of the field's own ground by W 0.75 and no more than 50 at W 1, in
every army, and his knee fronts run 5 to 8 under his boots, as the American's do.

The worn look moves a point 0.85 x dirt x W of the way to the soil (the files' own recipe, about_wear), so at W 1 the
boots read 0.85 times the mean of their points' `dirt`. The American files read 47 to 48 in 100 there. The armies in
tall boots read less, because the boot's shaft reaches the knee and shade.py's dirt falls off with height, so the mean
over the whole boot is lower: the German 34, the Soviet 35, the Soviet in felt boots 31. This raises their `dirt` (and
`dirt_wet` by the same, so rain still throws the mud higher) on the boots' points and the knee fronts, level by level,
to the American's 47.5 (boots) and 6.5 under that (knee fronts), never lowering a point and never past 1. (The
paratrooper's knee fronts read 14 against his boots' 42: their points carry next to no dirt, so no gain can lift them;
left as they are.) Nothing else
changes: the baked dry look (`shade`, `tint`), which the game draws with the worn look off, is as it was.

Which points: as the game's test reads them (Tools/land_show/land_worn_men_test.cpp, BootsAndKnees): boots are paint 2
below 0.8 m (at mid and far the files paint the eyes as boots); knee fronts are on a thigh or a calf, within 10 cm of a
calf's head, facing forward (x over 0.3), not skin. A gore twin takes its man's gains on the same points.

    python3 dirt_band.py --measure [folder]           every file's boots and knee fronts at W 1, each level
    python3 dirt_band.py --mend NAME... [--content]   mend the shared files (and their twins): backup, written whole,
                                                      read back; with --content, the game's copies in
                                                      Content/Models/Soldiers (and Detail/) too, when they match

Each mended file records `dirt_band` (the gains by level and the readings before and after).
"""
import json
import os
import shutil
import sys
import time

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SHARED = "/mnt/project-files/battle-models/land/soldier/"
CONTENT = os.path.normpath(os.path.join(HERE, "..", "..", "Content", "Models", "Soldiers"))
BACKUP = "/home/user/art/looks/backup/dirt_band/"
MOVE = 0.85                 # (the recipe: the share of the way to the soil a point of dirt 1 goes at W 1)
BOOTS = 0.475               # (the American's boots at W 1)
UNDER = 0.065               # (his knee fronts this far under his boots)
FLOOR = 0.40                # (a file under this at W 1 is under the art director's 30 by W 0.75)
LEVELS = ("near", "mid", "far")


def points_of(D, L):
    """The boots' and the knee fronts' points of level L (as the game's test reads them)."""
    M = D[L]
    P = np.array(M["points"], float).reshape(-1, 3)
    F = np.array(M["facings"], float).reshape(-1, 3)
    paint = np.array(M["paints"])
    part = np.array(M["part"])
    names = [b["name"] for b in D["bones"]]
    ix = {n: i for i, n in enumerate(names)}
    boots = np.where((paint == 2) & (P[:, 2] < 0.8))[0]
    legs = [ix[n] for n in ("thigh_l", "thigh_r", "calf_l", "calf_r") if n in ix]
    near = np.zeros(len(P), bool)
    for c in ("calf_l", "calf_r"):
        if c in ix:
            near |= np.linalg.norm(P - np.array(D["bones"][ix[c]]["head"], float), axis=1) < 0.10
    knees = np.where(np.isin(part, legs) & (paint != 7) & (F[:, 0] > 0.3) & near)[0]
    return boots, knees


def reading(D, L):
    """At W 1, dry: how far toward the soil the boots and the knee fronts go on the mean (0 to 1), or None."""
    if "dirt" not in D.get(L, {}):
        return None
    boots, knees = points_of(D, L)
    d = np.array(D[L]["dirt"], float)
    return (MOVE * float(d[boots].mean()) if len(boots) else None,
            MOVE * float(d[knees].mean()) if len(knees) else None, len(boots), len(knees))


def gain_for(d, target):
    """The one gain g (at least 1) that brings MOVE x mean(min(1, d x g)) to target (as near as the cap at 1 allows)."""
    if len(d) == 0 or MOVE * d.mean() >= target:
        return 1.0
    lo, hi = 1.0, 8.0
    for _ in range(60):
        g = 0.5 * (lo + hi)
        if MOVE * np.minimum(1.0, d * g).mean() < target:
            lo = g
        else:
            hi = g
    return round(hi, 4)


def gains(D):
    """Each level's gains for the boots and the knee fronts, or {} when the file is in the band already. The boots are
    raised where they read under FLOOR at W 1, and with them the knee fronts, to UNDER under the boots' new reading
    (where they are more than 8 under it). A file whose boots are in the band is left as it is."""
    out = {}
    for L in LEVELS:
        r = reading(D, L)
        if r is None or r[0] is None:
            continue
        boots, knees = points_of(D, L)
        d = np.array(D[L]["dirt"], float)
        gb = gain_for(d[boots], BOOTS) if r[0] < FLOOR else 1.0
        bb = MOVE * float(np.minimum(1.0, d[boots] * gb).mean())
        gk = 1.0
        if gb != 1.0 and len(knees) and r[1] < bb - 0.08:
            # (the knee fronts that are boots too keep the boots' gain: the knees' gain is found for the mean over all)
            both = np.isin(knees, boots)
            lo, hi = 1.0, 8.0
            for _ in range(60):
                g = 0.5 * (lo + hi)
                kk = np.where(both, max(g, gb), g)
                if MOVE * np.minimum(1.0, d[knees] * kk).mean() < bb - UNDER:
                    lo = g
                else:
                    hi = g
            gk = round(hi, 4) if MOVE * np.minimum(1.0, d[knees] * np.where(both, gb, 1.0)).mean() < bb - UNDER else 1.0
        if gb != 1.0 or gk != 1.0:
            out[L] = {"boots": gb, "knees": gk}
    return out


def apply(D, G):
    """Raise dirt and dirt_wet on each level's boots and knee fronts by G's gains (never past 1)."""
    for L, g in G.items():
        boots, knees = points_of(D, L)
        # (a tall boot's top is a knee front too: such a point takes the larger gain once, not both one after the other)
        k = np.ones(len(D[L]["dirt"]))
        k[knees] = g["knees"]
        k[boots] = np.maximum(k[boots], g["boots"])
        for key in ("dirt", "dirt_wet"):
            a = np.array(D[L][key], float)
            D[L][key] = [round(float(v), 2) for v in np.minimum(1.0, np.maximum(a, a * k))]


def record(D, G, before):
    after = {L: reading(D, L) for L in G}
    D["dirt_band"] = {
        "about": ("The art director's band for the worn look (25 Nov 2026): boots 30 in 100 of the way to the field's "
                  "ground by W 0.75, at most 50 at W 1, knee fronts 5 to 8 under. dirt and dirt_wet raised on the boots "
                  "and knee fronts by these gains (dirt_band.py); the baked dry look (shade, tint) is unchanged."),
        "gains": G,
        "w1_before": {L: [round(100 * before[L][0], 1), round(100 * before[L][1], 1)] for L in G if before[L][1] is not None},
        "w1_after": {L: [round(100 * after[L][0], 1), round(100 * after[L][1], 1)] for L in G if after[L][1] is not None}}


def mend_pair(man_path, twin_path, out_man, out_twin):
    D = json.load(open(man_path))
    if "dirt_band" in D:
        print(f"{os.path.basename(man_path)}: mended already ({D['dirt_band']['gains']})")
        return None
    G = gains(D)
    if not G:
        print(f"{os.path.basename(man_path)}: in the band, left as it is")
        return None
    before = {L: reading(D, L) for L in G}
    apply(D, G)
    record(D, G, before)
    for L in G:
        b, a = before[L], reading(D, L)
        print(f"{os.path.basename(man_path)} {L}: boots {100 * b[0]:.1f} -> {100 * a[0]:.1f}, knee fronts "
              f"{100 * (b[1] or 0):.1f} -> {100 * (a[1] or 0):.1f} (gains {G[L]['boots']}, {G[L]['knees']})")
    out = [(out_man, D)]
    if twin_path and os.path.exists(twin_path):
        T = json.load(open(twin_path))
        if "dirt_band" not in T:
            tb = {L: reading(T, L) for L in G}
            apply(T, G)
            record(T, G, tb)
            out.append((out_twin, T))
    for path, X in out:
        tmp = path + ".tmp_dirt_band"
        with open(tmp, "w") as f:
            json.dump(X, f, separators=(",", ":"))
        os.replace(tmp, path)
    return G


def measure(folder):
    for f in sorted(os.listdir(folder)):
        if not f.startswith("soldier_") or not f.endswith(".json") or f.endswith("_gore.json"):
            continue
        D = json.load(open(os.path.join(folder, f)))
        line = []
        for L in LEVELS:
            r = reading(D, L)
            if r and r[0] is not None:
                line.append(f"{L} boots {100 * r[0]:5.1f} knees {100 * (r[1] or 0):5.1f}")
        print(f"{f:34s} " + " | ".join(line) + ("  (mended)" if "dirt_band" in D else ""))


def sha(path):
    import hashlib
    return hashlib.sha1(open(path, "rb").read()).hexdigest()


def main(argv):
    if "--measure" in argv:
        rest = [a for a in argv if not a.startswith("--")]
        measure(rest[0] if rest else SHARED)
        return 0
    if "--mend" in argv:
        names = [a for a in argv if not a.startswith("--")]
        content = "--content" in argv
        os.makedirs(BACKUP, exist_ok=True)
        stamp = time.strftime("%m%d_%H%M%S", time.gmtime())
        for n in names:
            man, twin = SHARED + n + ".json", SHARED + n + "_gore.json"
            games = []
            if content:
                for g in (os.path.join(CONTENT, n + ".json"), os.path.join(CONTENT, "Detail", n.replace("_detail", "") + ".json")
                          if n.endswith("_detail") else None):
                    if g and os.path.exists(g):
                        games.append(g)
            same = {g: sha(g) == sha(man) for g in games}
            for p in (man, twin):
                if os.path.exists(p):
                    shutil.copy2(p, os.path.join(BACKUP, os.path.basename(p) + "." + stamp))
            G = mend_pair(man, twin, man, twin)
            if G is None:
                continue
            time.sleep(10)
            json.load(open(man))
            if os.path.exists(twin):
                json.load(open(twin))
            for g in games:
                if not same[g]:
                    print(f"  {g}: not the shared file's bytes before the mend, left for a look by hand")
                    continue
                gt = g[:-5] + "_gore.json"
                shutil.copy2(man, g + ".tmp_dirt_band")
                os.replace(g + ".tmp_dirt_band", g)
                if os.path.exists(gt) and os.path.exists(twin):
                    shutil.copy2(twin, gt + ".tmp_dirt_band")
                    os.replace(gt + ".tmp_dirt_band", gt)
                print(f"  into {os.path.relpath(g, CONTENT)} (and its twin)")
        return 0
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
