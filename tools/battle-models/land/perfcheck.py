# An independent check of a built sheet: the tables are recomputed from the sheet's own plates and guns by the rule
# as the sheet words it, and compared; then old v new for the pairs that must not have moved.
import json, sys, math
old = json.load(open(sys.argv[1])); new = json.load(open(sys.argv[2]))
V, G, D, Rch, RG = new["vehicles"], new["guns"], new["duels"]["table"], new["reach"]["table"], new["ranges_m"]
OV, OG, OD, ORch = old["vehicles"], old["guns"], old["duels"]["table"], old["reach"]["table"]
fails = []
def ok(c, msg, quiet=False):
    if not c: fails.append(msg); print("FAIL", msg)
    elif not quiet: print("ok  ", msg)
F = ("front", "side", "rear")
# 1. shares
n = 0
for name, v in V.items():
    for f in F:
        s = sum(p["share"] for p in v["armour"][f]); n += 1
        ok(abs(s - 1) < 1e-9 or not v["armour"][f], f"shares {name} {f} = {s}", True)
for m, rows in new["shared_by_nation"]["table"].items():
    for nat, row in rows.items():
        arm = (row.get("as") or {}).get("armour")
        if arm:
            for f in F:
                s = sum(p["share"] for p in arm[f]); n += 1
                ok(abs(s - 1) < 1e-9, f"shares {m}@{nat} {f} = {s}", True)
print("ok   plate shares sum to 1 on", n, "faces")
# 2. recompute by the rule as worded
def pierce(g, m):
    t = g["pierce_30deg_mm"]
    if m <= RG[0]: return t[0]
    for i in range(len(RG) - 1):
        if m <= RG[i + 1]:
            u = (m - RG[i]) / (RG[i + 1] - RG[i]); return t[i] + (t[i + 1] - t[i]) * u
    return max(0.0, t[-1] + (t[-1] - t[-2]) / (RG[-1] - RG[-2]) * (m - RG[-1]))
def through(g, plates, q, m, hollow=True):
    sq = pierce(g, m) * 1.2
    if hollow and g["heat_mm"] and m <= g["heat_to_m"]: sq = max(sq, g["heat_mm"])
    if sq <= 0: return 0.0
    tot = 0.0
    for p in plates:
        if p["mm"] <= 0: tot += p["share"]; continue
        k = min(0.5, max(0.0, 0.5 * p["mm"] / g["calibre_mm"] - 0.2))
        need = p["mm"] * q / math.cos(math.radians(min(p["slope_deg"], 85))) ** (1 + k)
        x = min(1.0, max(0.0, (sq / need - 0.85) / 0.30)); tot += p["share"] * x * x * (3 - 2 * x)
    return tot
cells = bad = rcells = rbad = 0
for s, row in D.items():
    g = G[V[s]["gun"]]; g2 = G.get(V[s].get("second_gun") or "")
    for t, c in row.items():
        a = V[t]["armour"]
        for hollow, tab, rt in ((True, c, Rch[s][t]),) + (((False, c["before_hollow_charge"], Rch[s][t]["before_hollow_charge"]),) if "before_hollow_charge" in c else ()):
            for f in F:
                mine = [round(max(through(g, a[f], a["quality"], m, hollow), through(g2, a[f], a["quality"], m, hollow) if g2 else 0), 2) for m in RG]
                cells += 5
                if mine != tab[f]: bad += 1; print("FAIL duel", s, t, f, hollow, mine, tab[f]) if bad < 8 else None
                far = 0
                for m in range(50, 2501, 50):
                    c1 = through(g, a[f], a["quality"], m, hollow); c2 = through(g2, a[f], a["quality"], m, hollow) if (g2 and hollow) else 0
                    if c1 >= 0.5 or c2 >= 0.5: far = m
                rcells += 1
                if far != rt[f]: rbad += 1; print("FAIL reach", s, t, f, hollow, far, rt[f]) if rbad < 8 else None
ok(bad == 0, f"duels recomputed from the sheet's own plates and guns: {cells} cells, {bad} rows differ")
ok(rbad == 0, f"reach recomputed: {rcells} cells, {rbad} differ")
# worth
wb = 0
for name, v in V.items():
    a = v["armour"]
    for f in F:
        thin = 0.0
        for p in a[f]:
            if p["mm"] > 0:
                k = min(0.5, max(0.0, 0.5 * p["mm"] / 75.0 - 0.2)); thin += p["share"] / (p["mm"] * a["quality"] / math.cos(math.radians(min(p["slope_deg"], 85))) ** (1 + k))
        w = round(1 / thin) if thin else 0
        if w != v["worth_mm"][f]: wb += 1; print("FAIL worth", name, f, w, v["worth_mm"][f])
ok(wb == 0, "worth_mm recomputed for every face")
# 3. later rounds
for gn, g in G.items():
    for r in g["later_rounds"]:
        ok(abs(r["times"] - round(r["pierce_30deg_at_1000_m"] / g["pierce_30deg_mm"][2], 2)) < 1e-9, f"later round {gn} {r['shell']} x{r['times']} from {r['from']}", True)
print("ok   later rounds' multipliers:", {gn: [(r["from"], r["times"]) for r in g["later_rounds"]] for gn, g in G.items() if g["later_rounds"]})
# 4. use_row
T = new["shared_by_nation"]["table"]; uses = []
for m, rows in T.items():
    for nat, row in rows.items():
        u = row.get("use_row")
        if u:
            uses.append((nat, m, u["row"], u["from"]))
            if "@" in u["row"]:
                mm, nn = u["row"].split("@"); ok("as" in T[mm][nn], f"use_row {nat} {m} -> {u['row']}", True)
            else: ok(u["row"] in V, f"use_row {nat} {m} -> {u['row']}", True)
        l = row.get("later")
        if l: print("     later lean:", nat, m, row["gun"], "->", l)
print("ok   use_row:", uses)
# 5. what must NOT have moved
moved = set()
for s in D:
    if s not in OD: continue
    for t in D[s]:
        if t not in OD[s]: continue
        if D[s][t] != OD[s][t] or Rch[s][t] != ORch[s][t]: moved.add((s, t))
changed_guns = {gn for gn in G if gn in OG and G[gn]["pierce_30deg_mm"] != OG[gn]["pierce_30deg_mm"] or (gn in OG and (G[gn]["heat_mm"], G[gn]["heat_to_m"], G[gn]["calibre_mm"]) != (OG[gn]["heat_mm"], OG[gn]["heat_to_m"], OG[gn]["calibre_mm"]))}
changed_arm = {n for n in V if n in OV and V[n]["armour"] != OV[n]["armour"]}
print("     guns whose piercing changed:", sorted(changed_guns)); print("     vehicles whose plates changed:", sorted(changed_arm))
unexpected = [(s, t) for s, t in moved if not (t in changed_arm or V[s]["gun"] in changed_guns or (V[s].get("second_gun") in changed_guns))]
ok(not unexpected, f"every moved duel is explained by a changed plate or gun ({len(moved)} pairs moved; unexplained: {unexpected[:6]})")
ok(changed_guns == {"qf2"}, "only the 2-pounder's piercing row changed")
ok(changed_arm == {"T34", "T34-85", "CharB1"}, "only the T-34s' and the Char B1's plates changed")
for n in V:
    if n in OV:
        for key in ("fire_when_pierced", "explodes_of_fires", "kills_when_it_pierces", "turret_degrees_a_second", "gun_arc_degrees", "radio", "reliability", "soft_ground", "gun", "command", "optics"):
            if V[n].get(key) != OV[n].get(key): print(f"     {n}.{key}: {OV[n].get(key)} -> {V[n].get(key)}")
print("     new rows:", sorted(set(V) - set(OV)), "new guns:", sorted(set(G) - set(OG)), "gone:", sorted(set(OV) - set(V)), sorted(set(OG) - set(G)))
for s, t in (("Sherman", "Tiger"), ("Sherman", "Panther"), ("T34", "Tiger"), ("T34-85", "Tiger"), ("M10", "Tiger"), ("M10", "Panther"), ("Tiger", "Sherman"), ("Panther", "Sherman"), ("PanzerIV", "Sherman"), ("AntiTankGun", "Tiger")):
    ok(D[s][t] == OD[s][t] and Rch[s][t] == ORch[s][t], f"unchanged: {s} > {t} reach {Rch[s][t]}", True)
print("ok   the pairs Brian was shown (Sherman, T-34, M10 against Tiger and Panther, and back) are unchanged")
print("RESULT:", "PASS" if not fails else f"{len(fails)} FAIL")
