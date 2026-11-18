"""gunarcs.py <air folder> <out.json> <out.md>: placeholder firing arcs for every hand-held gun (flexgun.N), read off
the exported model files. The air battles asked (2026-11-18): a hand-held gun's part is `kind: free` and has no limits
in its file. These are the modeller's estimates of what each window or mounting on the model could cover, not sourced
figures; the historian's ruling replaces any of them. Turrets are not here: their limits are in the model files."""
import glob, json, math, os, sys
air, out_json, out_md = sys.argv[1:4]

def arc(name, part, pivot, f, parent, last_aft):
    fx, fy, fz = f
    head = math.degrees(math.atan2(fy, fx))                  # 0 dead ahead, plus to starboard, 180 dead astern
    rest_lift = math.degrees(math.asin(max(-1.0, min(1.0, fz))))
    side = "starboard" if fy > 0 else "port"
    if abs(fy) > 0.5:                                        # a beam gun at a waist window or blister
        lo, hi = (60.0, 150.0) if fy > 0 else (-150.0, -60.0)
        return f"waist, {side}", [lo - head, hi - head], [-45.0, 40.0], "from 30 degrees forward of abeam to 60 degrees aft of it"
    if fx > 0.7:
        if abs(fy) > 0.25:                                   # a cheek gun beside the nose glazing
            lo, hi = (5.0, 80.0) if fy > 0 else (-80.0, -5.0)
            return f"nose cheek, {side}", [lo - head, hi - head], [-30.0, 30.0], "from nearly dead ahead to 80 degrees out on its own side; it cannot cross the nose"
        if fz < -0.2:
            return "under the nose, firing forward and down", [-25.0, 25.0], [-50.0, 0.0], "a cone forward and below"
        return "nose", [-30.0, 30.0], [-30.0, 25.0], "a cone about dead ahead"
    if fx < -0.7:
        if (parent == "tail" or last_aft) and abs(fz) < 0.15:
            return "tail", [-35.0, 35.0], [-35.0, 35.0], "a cone about dead astern"
        if fz < -0.15:
            return "under the fuselage, firing aft and down", [-35.0, 35.0], [-60.0, 0.0], "aft and below only"
        if name == "B17":
            return "radio room hatch, firing up and aft", [-35.0, 35.0], [10.0, 75.0], "up and aft through the roof hatch; the fin and the upper turret bound it"
        lone = "rear cockpit" if pivot[2] < 1.05 and abs(pivot[0]) < 3.0 else "upper, firing aft"
        return lone, [-75.0, 75.0], [-5.0, 65.0], "aft and to each beam above the fuselage; not forward past the wing and canopy"
    return "free", [-30.0, 30.0], [-30.0, 30.0], "a cone about its rest"

table, lines = {}, []
for path in sorted(glob.glob(os.path.join(air, "*.json"))):
    name = os.path.basename(path)[:-5]
    if name.endswith("Damage") or name.startswith("index"):
        continue
    m = json.load(open(path))
    if not isinstance(m, dict) or "parts" not in m:
        continue
    parts = m["parts"]
    guns = [(i, p) for i, p in enumerate(parts) if p["name"].startswith("flexgun.")]
    if not guns:
        continue
    aft_most = min(p["pivot"][0] for p in parts if p["name"].startswith(("flexgun.", "turret.")))
    rows = []
    for i, p in guns:
        muzzles = [s for s in m["sockets"] if s.get("part") == i and s["name"].startswith("turret.")]
        if not muzzles:
            continue
        f = [sum(s["facing"][k] for s in muzzles) / len(muzzles) for k in range(3)]
        n = math.sqrt(sum(v * v for v in f)); f = [v / n for v in f]
        station, train, lift, words = arc(name, p["name"], p["pivot"], f, parts[p["parent"]]["name"], p["pivot"][0] <= aft_most + 1e-6)
        head = math.degrees(math.atan2(f[1], f[0]))
        row = {"part": p["name"], "muzzles": [s["name"] for s in muzzles], "station": station, "rest": [round(v, 4) for v in f],
               "rest_heading": round(head, 1), "rest_lift": round(math.degrees(math.asin(f[2])), 1),
               "train": [round(train[0], 1), round(train[1], 1)], "lift": lift, "words": words}
        rows.append(row)
        lines.append(f"| {name} | `{p['name']}` | {', '.join('`' + s['name'] + '`' for s in muzzles)} | {station} | {row['rest_heading']:.0f} | {row['train'][0]:.0f} to {row['train'][1]:.0f} | {lift[0]:.0f} to {lift[1]:.0f} |")
    table[name] = rows
about = ("Placeholder firing arcs for every hand-held gun (flexgun.N) on the battle aircraft: the aircraft modeller's estimates of what each window or "
         "mounting on the model could cover, NOT sourced figures. The historian's ruling replaces any row. Axes as the model files: x forward, y right "
         "(starboard), z up. rest: the way the gun points at rest (its muzzle socket's facing). rest_heading: that way in the level plane, 0 dead ahead, "
         "plus to starboard (clockwise seen from above), 180 dead astern. train: how far it swings in the level plane either side of rest_heading, in "
         "degrees, plus clockwise seen from above (the sign a turret part uses). lift: how low and how high it points, in degrees above the aircraft's "
         "own level (not measured from rest). A gun's part is `kind: free` and swings about its pivot (its breech). Turrets are not here: their limits "
         "are in the model files (`turret.N` trains, `turret.N.guns` lifts).")
json.dump({"about": about, "made": "2026-11-18", "aircraft": table}, open(out_json, "w"), indent=1)
open(out_md, "w").write("| Aircraft | Part | Muzzles | Station | Rest heading | Train about rest | Lift |\n|---|---|---|---|---|---|---|\n" + "\n".join(lines) + "\n")
print(len(lines), "guns on", len(table), "aircraft")
