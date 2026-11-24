"""A pack range in every soldier file (the effects thread, 24 November 2026, release 2): `pack_tris`, as `helmet_tris`,
{near: [first, count], mid: [...], far: [...]}: the triangles of his pack and all his webbing, one run at each level,
so the game can leave them out (a downed pilot on foot, a crew bailed out of a tank, with no pack on his back) and
draw nothing in their place: his cloth runs whole under all of it.

In the range: the belt and its buckle, the Y-straps or suspenders and their buckles and rings, the pouches and their
straps and studs, the bread bag, the canteen, the mess tin, the shovel, the bayonet, the gas-mask can, the blanket or
coat roll, the pack; the radio man's set, the medic's bags, the bazooka men's rocket bags, whatever a file carries on
its belt or back. Out of it: the cloth, the boots, the skin, the buttons and badges on the cloth (the collar patches,
the shoulder boards' buttons, the pockets' buttons), the helmet and its chin strap (on the head bone), the weapon.

Nothing in a file is changed but the order of the triangles between his body and his weapon (soldier.py writes the
body first, then the kit piece by piece, then the weapon, the helmet, and later passes add the close face after it):
the kit's triangles that are not webbing go first, as they were, then the webbing, as it was. Points, paints, shade,
wear, weights, the helmet range, face_mesh's first_new_triangle, every clip: as they were. Run again, it gives the same.

How a piece is told (each piece of the mesh, a run of triangles joined by their corners, is one strap, one pouch, one
button's dome...): in the kit's run, on any bone but the head, the weapon and the bolt, not cloth or skin; a piece
more than 2.6 cm across is kit; the little ones (a button is ten pieces under 2 cm) go by their clusters: a cluster
that comes within 4 mm of the kit is the kit's (a buckle on its strap, a stud on its pouch, a ring), the rest are
buttons and badges on the cloth.

    python3 pack_range.py --stage DIR [names...]   write each file with its range to DIR (and say what it kept)
    python3 pack_range.py --deliver [names...]     the shared soldier files (read fresh, backed up, written whole)
    python3 pack_range.py --into DIR [names...]    the soldier files in another folder (the game's Content copies)
    python3 pack_range.py --check [names...]       check the shared files' ranges

names: soldier_GER, soldier_USA_radio_gore...; none: every soldier file with a helmet range (not the sailors, who carry
no kit, nor the tank crews).
"""
import glob
import json
import os
import sys
import time
from collections import Counter

import numpy as np

SHARED = "/mnt/project-files/battle-models/land/soldier/"
BACKUP = "/home/user/art/looks/backup/pack_range/"
BODY = {0, 1, 2, 3, 7, 10, 11, 15, 16, 17, 20, 22, 46, 47}
# (tunic, trousers, boots, lower, skin, coat, coat_dark; the medic's armband (medic_white, medic_red), the NCO's
#  stripes (rank), the paratrooper's knee and elbow patches (reinforce) and sleeve flag (flag_blue); the eyes)
SMALL = 0.026
NEAR = 0.004
OFF_CLOTH = 0.02                       # (a button or badge lies on the cloth: within 2 cm of it, as over the breastbone)
NOT_KIT_BONES = ("head", "weapon", "bolt", "bipod")
SEWN_BONES = ("neck", "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r")   # (the helmet's paint here is cloth)
BADGE, BADGE_ON = 0.05, 0.015         # (a flat badge on the breast: at most 5 cm across, under 0.5 mm thick (rms),
                                       #  lying within 1.5 cm of his body's cloth (on the lapel))
BUTTON, BUTTON_ON = 0.017, 0.003       # (a button: at most 17 mm across, within 3 mm of the cloth)
ABOUT = ("pack_tris (pack_range.py): his pack and all his webbing (belt, straps, pouches, bags, canteen, shovel, "
         "bayonet, gas-mask can, rolls, a radio or aid bags), one run of triangles at each level [first, count], as "
         "helmet_tris: leave it out for a man with no pack (a downed pilot, a crew out of a tank) and draw nothing in its "
         "place, the cloth is whole under it. Buttons, badges, the helmet's chin strap and the weapon are not in it.")


def pieces(T, n):
    """Each triangle's piece: the triangles joined by their corners."""
    par = np.arange(n)

    def find(a):
        r = a
        while par[r] != r:
            r = par[r]
        while par[a] != r:
            par[a], a = r, par[a]
        return r
    for a, b, c in T:
        ra = find(a)
        par[find(b)] = ra
        par[find(c)] = ra
    root = np.array([find(t[0]) for t in T])
    out = {}
    for i, r in enumerate(root):
        out.setdefault(int(r), []).append(i)
    return [np.array(v) for v in out.values()]


def classify(d, lod):
    """(B, W, pack: the triangle indices in [B, W) that are webbing, notes)."""
    names = [b["name"] for b in d["bones"]]
    # (his cloth: BODY, and the buttons' own paint where a file has one (the detailed rifleman's `button`); and the
    #  helmet's paint where it is sewn on him: the collar tabs and patches, the shoulder straps, the snow suit's hood
    #  hem, on the neck, collarbones and upper arms, or on the chest bone in front of it (the Italian's lapel patches).
    #  On his back or hips the helmet's paint is kit: the German's gas-mask can, the American's back pieces, the
    #  paratrooper's leg bag)
    #  A flat badge on the breast in another paint is sewn on too (the desert Italian's lapel patches, in the straps'
    #  paint). And a button (a hardware paint: steel, the American jacket's brown `leather`, a file's own `button`)
    #  on his front or shoulders, sewn on the cloth, stays though a strap lies against it: the Chinese tunic's buttons
    #  under his bandolier, the American's shoulder-strap buttons under his suspenders, the paratrooper's pocket snap.
    buttons = {v for k, v in d.get("new_paints", {}).items() if k == "button"}
    cloth = set(BODY) | buttons
    hardware = {9, 12} | buttons
    heads = {b["name"]: b.get("head") for b in d["bones"]}
    chest_x = heads["chest"][0] if heads.get("chest") else 0.0

    def front(bone, x, trunk=("chest",)):
        return bone in SEWN_BONES or (bone in trunk and x > chest_x)

    def body(paint, bone, P):
        if paint in cloth:
            return True
        x = float(P[:, 0].mean())
        if paint == 5 and front(bone, x):
            return True
        if bone == "chest" and x > chest_x and paint not in hardware and len(P) >= 3:    # (not a facet of a ring)
            ext = P.max(0) - P.min(0)
            flat = np.linalg.svd(P - P.mean(0), compute_uv=False)[-1] / np.sqrt(len(P))
            return (float(ext.max()) <= BADGE and float(flat) <= 0.0005
                    and float(body_tree[0].query(P, k=1)[0].max()) <= BADGE_ON)
        return False
    body_tree = []                                # (his body's surface, once its run's end B is known below)
    m = d[lod]
    T = np.array(m["triangles"]).reshape(-1, 3)
    pa = np.array(m["paints"])
    part = np.array(m["part"])
    X = np.array(m["points"]).reshape(-1, 3)
    hf = d["helmet_tris"][lod][0]
    first = pa[T[:, 0]]
    idx = np.arange(len(T))
    tri_bone = np.array([np.bincount(part[t]).argmax() for t in T])
    # (the body's run ends with his hands: the skin on the hand and finger bones)
    hb = [i for i, nm in enumerate(names) if nm.startswith(("hand_", "fingers_", "thumb_"))]
    B = int(np.where((idx < hf) & (first == 7) & np.isin(tri_bone, hb))[0].max()) + 1
    from scipy.spatial import cKDTree               # (run with scipy: the bpy environment's python has it)
    # each surface read as points over its triangles (a 7 x 7 grid in each): the distance to a surface, not to its
    # corners (a snap on a bag's broad face had stood 5 mm from the bag's nearest corner and was kept, left floating)
    bc = np.array([(i / 7, j / 7, 1 - i / 7 - j / 7) for i in range(8) for j in range(8 - i)])

    def surface(tris):
        tris = np.asarray(tris, dtype=np.int64)
        return cKDTree(np.einsum("kb,tbc->tkc", bc, X[T[tris]]).reshape(-1, 3)) if len(tris) else None
    P = pieces(T, len(pa))
    while True:                                   # (and runs on to the end of every piece begun before it)
        B2 = max([B] + [int(tr[tr < hf].max()) + 1 for tr in P if tr.min() < B and (tr < hf).any()])
        if B2 == B:
            break
        B = B2
    # (the radio man's handset and the bazooka loader's rocket ride the bolt bone, their carbines having none that
    # moves: the handset goes with the set, the rocket with its bag)
    own = ("bolt",) if "whip_1" in names or "bazooka loader" in d.get("name", "") else ()
    body_tree.append(surface(np.arange(B)))
    W = hf                                        # (the run from his hands to his helmet: kit, gun, and what was
    rows = []                                     #  added after the gun: the NCO's chin strap, the radio's whip)
    for tr in P:
        inside = (tr >= B) & (tr < W)
        if not inside.any():
            continue
        v = np.unique(T[tr].ravel())
        bone_ = names[np.bincount(part[v]).argmax()]
        paint_ = Counter(first[tr].tolist()).most_common(1)[0][0]
        if not inside.all() and (bone_ in NOT_KIT_BONES or body(paint_, bone_, X[v]) or paint_ == 5):
            continue                              # (the snow suit's hood, run on into the helmet's: not his kit, kept)
        if not inside.all():
            raise SystemExit(f"{lod}: a piece runs across the kit's edge at {B} or {W} "
                             f"({tr.min()}-{tr.max()}, paint {Counter(first[tr].tolist()).most_common(1)})")
        rows.append({"tr": tr, "v": v, "paint": Counter(first[tr].tolist()).most_common(1)[0][0],
                     "bone": names[np.bincount(part[v]).argmax()], "lo": X[v].min(0), "hi": X[v].max(0)})
    for r in rows:
        r["size"] = float((r["hi"] - r["lo"]).max())
        r["kit"] = ((r["bone"] not in NOT_KIT_BONES or r["bone"] in own)
                    and not body(r["paint"], r["bone"], X[r["v"]]))
    big = [r for r in rows if r["kit"] and r["size"] > SMALL]
    kit_tree = surface(np.concatenate([r["tr"] for r in big])) if big else None
    stay = [np.arange(B)] + [r["tr"] for r in rows if not r["kit"] and r["bone"] not in ("weapon", "bolt", "bipod")]
    cloth_tree = surface(np.concatenate(stay))
    small = [r for r in rows if r["kit"] and r["size"] <= SMALL]
    lab = list(range(len(small)))

    def f(a):
        while lab[a] != a:
            lab[a] = lab[lab[a]]
            a = lab[a]
        return a
    for i in range(len(small)):
        for j in range(i + 1, len(small)):
            if np.all(small[i]["lo"] - 0.001 <= small[j]["hi"]) and np.all(small[j]["lo"] - 0.001 <= small[i]["hi"]):
                lab[f(i)] = f(j)
    groups = {}
    for i, r in enumerate(small):
        groups.setdefault(f(i), []).append(r)
    for g in groups.values():
        pts = np.concatenate([X[r["v"]] for r in g])
        dk = float(kit_tree.query(pts, k=1)[0].min()) if kit_tree is not None else 1.0
        dc = float(cloth_tree.query(pts, k=1)[0].min())
        # with his kit: touching a bag, strap or set (a buckle, a snap, the radio's whip); or not lying on the cloth
        # that stays, so it would hang in the air without the kit it is fixed to
        near = dk < NEAR or dc > OFF_CLOTH
        x_ = float(pts[:, 0].mean())
        if (near and dc <= BUTTON_ON and all(r["paint"] in hardware for r in g)
                and float((pts.max(0) - pts.min(0)).max()) <= BUTTON and x_ > chest_x
                and all(front(r["bone"], x_, ("chest", "spine_02", "spine_01")) for r in g)):
            near = False                          # (a button sewn on his front or shoulder, a strap against it: stays;
            #                                        never one behind the chest's middle: kit hardware on his back)
        for r in g:
            r["near"] = near
            r["dk"], r["dc"] = dk, dc
    pack, notes = [], Counter()
    for r in rows:
        if not r["kit"]:
            notes["head (the chin strap) or cloth"] += len(r["tr"])
            if r["size"] > SMALL and r["bone"] != "head":
                notes.setdefault("cloth pieces", []).append((int(r["paint"]), r["bone"], [round(float(v), 2) for v in (r["lo"] + r["hi"]) / 2]))
            continue
        if r["size"] > SMALL or r["near"]:
            pack.extend(r["tr"].tolist())
            if r["size"] <= SMALL and r["dk"] >= NEAR:
                notes["small pieces off the cloth, with the kit"] += len(r["tr"])
        else:
            notes["buttons and badges"] += len(r["tr"])
            notes["buttons: furthest off the cloth (mm)"] = max(notes.get("buttons: furthest off the cloth (mm)", 0.0), round(r["dc"] * 1000, 1))
    return B, W, np.array(sorted(pack), dtype=np.int64), dict(notes)


def with_range(d):
    """The file with its pack range (only each level's triangles' order and pack_tris change)."""
    out = {}
    for lod in ("near", "mid", "far"):
        B, W, pack, notes = classify(d, lod)
        T = np.array(d[lod]["triangles"]).reshape(-1, 3)
        inr = np.zeros(len(T), bool)
        inr[pack] = True
        span = np.arange(B, W)
        order = np.concatenate([np.arange(0, B), span[~inr[B:W]], span[inr[B:W]], np.arange(W, len(T))])
        T2 = T[order]
        assert sorted(map(tuple, T2.tolist())) == sorted(map(tuple, T.tolist()))
        d[lod]["triangles"] = T2.ravel().tolist()
        first = W - len(pack)
        out[lod] = [int(first), int(len(pack))]
        d.setdefault("_pack_notes", {})[lod] = {"kit": [B, W], "pack": out[lod], **notes}
    d["pack_tris"] = out
    d["about_pack"] = ABOUT
    return d


def targets(names, folder=SHARED):
    if names:
        return [os.path.join(folder, (n if n.endswith(".json") else n + ".json")) for n in names]
    out = []
    for p in sorted(glob.glob(os.path.join(folder, "soldier_*.json"))):
        b = os.path.basename(p)
        if "_sailor" in b or "_panzer" in b or "_tanker" in b:
            continue
        out.append(p)
    return out


def process(path, dest, backup=None, log=print):
    raw = open(path, "rb").read()
    d = json.loads(raw)
    if "helmet_tris" not in d:
        log(f"  {os.path.basename(path)}: no helmet range, left as it is")
        return None
    before = d.get("pack_tris")
    d = with_range(d)
    notes = d.pop("_pack_notes")
    if backup:                                # (the file as it was read)
        os.makedirs(backup, exist_ok=True)
        with open(os.path.join(backup, os.path.basename(path) + time.strftime(".%m%d_%H%M%S")), "wb") as f:
            f.write(raw)
    text = json.dumps(d, separators=(",", ":"), allow_nan=False)
    if os.path.abspath(dest) == os.path.abspath(path) and open(path, "rb").read() != raw:
        raise SystemExit(f"{path} changed while its pack range was made: not written; run it again")
    tmp = dest + ".tmp"
    with open(tmp, "w") as f:
        f.write(text)
    os.replace(tmp, dest)
    log(f"  {os.path.basename(path)}: pack {d['pack_tris']}" + (" (as before)" if before == d["pack_tris"] else "")
        + "; kept " + "; ".join(f"{k} {v.get('buttons and badges', 0)}+{v.get('head (the chin strap) or cloth', 0)}"
                                f" (buttons <= {v.get('buttons: furthest off the cloth (mm)', 0)} mm; "
                                f"{v.get('small pieces off the cloth, with the kit', 0)} off it, taken)"
                                for k, v in notes.items()))
    return d["pack_tris"]


def check(path):
    d = json.load(open(path))
    pr = d.get("pack_tris")
    if pr is None:
        return "no pack_tris"
    for lod in ("near", "mid", "far"):
        B, W, pack, _ = classify(d, lod)
        f, c = pr[lod]
        if c != len(pack) or (c and (pack[0] != f or pack[-1] != f + c - 1)) or f + c != W:
            return f"{lod}: the range {pr[lod]} is not the webbing's ({len(pack)} triangles, kit {B}-{W})"
        hf = d["helmet_tris"][lod][0]
        if f + c > hf:
            return f"{lod}: the range runs into the helmet's"
    return "ok"


def main():
    a = sys.argv[1:]
    names = [x for x in a if not x.startswith("--") and not os.path.isdir(x)]
    if "--stage" in a:
        dir_ = a[a.index("--stage") + 1]
        names = [x for x in names if x != dir_]
        os.makedirs(dir_, exist_ok=True)
        for p in targets(names):
            process(p, os.path.join(dir_, os.path.basename(p)))
    elif "--deliver" in a:
        for p in targets(names):
            process(p, p, backup=BACKUP)
        time.sleep(10)
        for p in targets(names):
            print(f"  {os.path.basename(p)}: {check(p)}")
    elif "--into" in a:
        dir_ = a[a.index("--into") + 1]
        names = [x for x in names if x != dir_]
        for p in targets(names, dir_):
            process(p, p)
            print(f"  {os.path.basename(p)}: {check(p)}")
    elif "--check" in a:
        for p in targets(names):
            print(f"  {os.path.basename(p)}: {check(p)}")


if __name__ == "__main__":
    main()
