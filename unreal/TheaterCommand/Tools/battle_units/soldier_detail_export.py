"""The detail pilot (soldier_detail.py's German rifleman, passed by the art director 24 November 2026, round 2) in the
game's own form, as a file of its own beside the German's: soldier_GER_detail.json. Release 2, switched off: the game
draws it only with -TheaterLandDetailMen (Content/Models/Soldiers/Detail/soldier_GER.json); soldier_GER.json is not
touched.

Two steps (then the file chain: variety, shade, brim_mend, face_mesh and face_detail, pack_range, falls, run_styles,
field_clips, riders fit, the gore twin; the order and commands are in the README's "The detailed rifleman" section):

  --export   (Blender python) the man made as soldier.py's export_all makes him (assemble, bake_clips, make_lods),
             with soldier_detail.py's patches; his tunic and shoulder-strap buttons given a paint of their own
             (`button`, a dull pebbled grey, not the army's dark steel: at a metre they had read as holes); written by
             soldier.py's write_game_json to WORK/soldier_GER_detail.fresh.json. Refuses unless the triangles match
             the man the art director passed (/home/user/art/detail/tris.json).
  --combine  (any python) the fresh file's geometry, bones and sockets with the shipping German's clips and tables
             (deaths, run styles, riders, bail: each later tool strips and replaces its own, so the file is whole at
             every step) to WORK/soldier_GER_detail.json, then copied to the shared folder (a new name: nothing of the
             German's is written).

    cd Tools/battle_units
    DETAIL_WORK=/home/user/art/detail_export/ PYTHONPATH=/home/user/work/looks/battle_units:. \\
        /home/user/bpyenv/bin/python soldier_detail_export.py --export
    /home/user/bpyenv/bin/python soldier_detail_export.py --combine

And two mends on the shared file once the chain has run (any python with numpy and scipy):

  --mend-buttons [path]  turn right way out any button made inside out (its triangles wound inward and its facings
                         pointing in). The first export (24 November) had the two shoulder-strap buttons so: dome()
                         winds inward and their object was built without recalc. soldier_detail.py now winds them
                         outward, so a fresh export needs none of it. On the man and on his gore twin.
  --reshade-mended [path]  then the mended buttons' shade baked again (shade.py on the mended file, put on their
                         points only, their crowns glinted), and the twin given the same.
  --glint [path]         the buttons' small highlight on the man's near tint, after shade.py --deliver. Not on the
                         gore twin: shade.py --twin takes his tint from his file, glint and all.
"""
import hashlib
import json
import os
import shutil
import sys
import time

WORK = os.environ.setdefault("DETAIL_WORK", "/home/user/art/detail_export/")
SHARED = "/mnt/project-files/battle-models/land/soldier/"
NAME = "soldier_GER_detail"
BACKUP = "/home/user/art/looks/backup/"
PASSED = "/home/user/art/detail/tris.json"         # (the build the art director passed, round 2)
BUTTON, BUTTON_RGB = 48, "86877F"                   # (dull pebbled grey metal: about 70-90 lit, the AD's carry)
BUTTON_OBJECTS = ("details", "shoulder_straps")     # (their steel is the buttons; buckles, studs and rings stay steel)
GLINT, GLINT_COS = 1.35, 0.9                       # (the crown's lift, and how near its axis a point faces)
FROM_FRESH = ("about", "long", "high", "marks", "near", "mid", "far", "bones", "sockets")
FROM_BASE = ("clips", "deaths", "about_deaths", "about_run_styles", "ride_clips", "about_riders", "bail_clips",
             "about_bail")
REBUILT_LATER = ("faces", "helmets", "inks", "helmet_tris", "about_variety", "about_wear", "face_mesh", "face_detail",
                 "pack_tris", "about_pack")


def export():
    import soldier_detail as SD
    import bpy
    import kit
    S = SD.S
    assert SD.WORK.rstrip("/").endswith("detail_export"), SD.WORK
    sc = kit.fresh(S.FPS)
    m = S.assemble("GER")
    S.bake_clips(m)
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    sc.frame_set(0)
    n = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    parts = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    ref = json.load(open(PASSED))
    print("built", n, flush=True)
    if n != {int(k): v for k, v in ref["tris"].items()} or parts != ref["parts"]:
        raise SystemExit(f"not the man the art director passed: {n} against {ref['tris']}")
    # the buttons' own paint
    S.FIGURE_PAINTS["button"] = BUTTON
    mat = bpy.data.materials.new("button")
    moved = 0
    for lod in lods.values():
        for ob in lod:
            if not ob.name.startswith(BUTTON_OBJECTS):
                continue
            me = ob.data
            names = [mt.name.split(".")[0] for mt in me.materials]
            if "steel" not in names:
                continue
            si = names.index("steel")
            me.materials.append(mat)
            bi = len(me.materials) - 1
            for p in me.polygons:
                if p.material_index == si:
                    p.material_index = bi
                    moved += 1
    print(f"buttons: {moved} faces to paint {BUTTON}", flush=True)
    os.makedirs(WORK, exist_ok=True)
    path = os.path.join(WORK, NAME + ".fresh.json")
    data = S.write_game_json(path, m, lods)
    D = json.load(open(path))
    D["new_paints"] = {"button": BUTTON}
    D["colours"] = {"button": BUTTON_RGB}
    _write(path, D)
    print("wrote", path, {k: (len(D[k]["points"]) // 3, len(D[k]["triangles"]) // 3) for k in ("near", "mid", "far")},
          flush=True)
    return data


def _write(path, D):
    tmp = path + ".tmp"
    with open(tmp, "w") as f:
        json.dump(D, f, separators=(",", ":"))
    json.load(open(tmp))
    os.replace(tmp, path)


def _sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def combine():
    fresh = json.load(open(os.path.join(WORK, NAME + ".fresh.json")))
    base_path = os.path.join(SHARED, "soldier_GER.json")
    base_sha = _sha(base_path)
    base = json.load(open(base_path))
    if [(b["name"], b.get("parent")) for b in fresh["bones"]] != [(b["name"], b.get("parent")) for b in base["bones"]]:
        raise SystemExit("the skeleton differs from the German's: his clips would not fit")
    out = {k: v for k, v in base.items() if k not in REBUILT_LATER and k not in FROM_FRESH
           and k not in ("new_paints", "colours")}
    for k in FROM_FRESH:
        out[k] = fresh[k]
    for k in FROM_BASE:
        if k in base:
            out[k] = base[k]
    out["new_paints"] = dict(fresh.get("new_paints", {}))
    out["colours"] = dict(fresh.get("colours", {}))
    out["name"] = "Battle rifleman, GER (detail pilot)"

    # soldier.py's own clips: the fresh ones where they differ from the shipping file's (beside the pouches' seats)
    def bare(c):
        c = dict(c)
        c["tracks"] = [t for t in c.get("tracks", []) if not str(t.get("part_name", t.get("part", ""))).startswith("pouch")]
        return c                          # (compared as numbers: a written 0.0 and -0.0 are the same key)
    diff, swapped = [], 0
    clips = dict(base["clips"])
    for k, c in fresh["clips"].items():
        if k not in clips:
            clips[k] = c
            diff.append(k + " (new)")
        elif bare(c) != bare(clips[k]):
            clips[k] = c
            diff.append(k)
        elif c != clips[k]:
            clips[k] = c                  # (only the pouches' seats differ: the new pouches ride where they are)
            swapped += 1
    out["clips"] = clips
    print(f"soldier.py clips: {len(fresh['clips'])}; taken fresh where they differ: {diff or 'none'}; "
          f"pouch seats only: {swapped}")
    if out.get("deaths"):
        def names(v):                     # (deaths: {stance: {kind: [clip names]}}, at any depth)
            if isinstance(v, str):
                return [v]
            if isinstance(v, dict):
                return [n for x in v.values() for n in names(x)]
            if isinstance(v, list):
                return [n for x in v for n in names(x)]
            return []
        named = names(out["deaths"])
        missing = sorted({d for d in named if d not in clips})
        if missing:
            print("  (deaths naming clips not in the file:", missing[:8], ")")
    path = os.path.join(WORK, NAME + ".json")
    _write(path, out)
    dest = os.path.join(SHARED, NAME + ".json")
    if os.path.exists(dest):
        raise SystemExit(f"{dest} exists: the chain has begun on it; move it aside first")
    tmp = dest + ".part"
    with open(path, "rb") as a, open(tmp, "wb") as b:
        b.write(a.read())
    os.replace(tmp, dest)
    assert _sha(base_path) == base_sha
    print("wrote", path, "and", dest, {k: len(out[k]["points"]) // 3 for k in ("near", "mid", "far")},
          f"{len(clips)} clips")


def _is_twin(path, D):
    return path.endswith("_gore.json") or "gore" in D or "gore_from" in D


def _backup(path):
    os.makedirs(BACKUP, exist_ok=True)
    bk = os.path.join(BACKUP, os.path.basename(path) + time.strftime(".%m%d_%H%M%S"))
    shutil.copy2(path, bk)
    return bk


def _put(path, D, h0, what):
    """Write D over path (backed up first; refused if the file changed since it was read), read it back."""
    if _sha(path) != h0:
        raise SystemExit(f"{path} changed while {what} ran: not written; run it again")
    bk = _backup(path)
    text = json.dumps(D, separators=(",", ":"), allow_nan=False)
    tmp = path + ".part.json"
    with open(tmp, "w") as f:
        f.write(text)
    json.loads(open(tmp).read())
    os.replace(tmp, path)
    time.sleep(10)                         # (the shared folder is a network mount: read it back after it settles)
    ok = open(path).read() == text
    print(f"{what}: wrote {path} ({len(text):,} bytes); read back {'ok' if ok else 'MISMATCH'}; backup {bk}")
    if not ok:
        raise SystemExit(f"{path}: read back differs")


def _fd_sha(a):
    """face_detail.py's own fingerprint of a tint array (its undo takes its colouring back off only while this
    matches what it recorded): kept in step when a later pass changes tint off its points."""
    import numpy as np
    return hashlib.sha1(np.round(np.asarray(a, float), 4).tobytes()).hexdigest()[:16]


def _fd_keep(D, lod, before):
    """After a pass changed D[lod]'s tint only on points face_detail did not colour: if face_detail's marker matched
    the tint as it was (`before`), move it on to the tint as it is now, so its undo still works."""
    rec = (D.get("face_detail") or {}).get("lods", {}).get(lod)
    if rec and rec.get("tint_after") == _fd_sha(before):
        rec["tint_after"] = _fd_sha(D[lod]["tint"])
        return True
    return False


def _button_groups(D, paint):
    """The buttons, from the near level: (centre, outward axis, near points) for each, its points clustered within
    6 mm (a shoulder-strap button is four pieces, its crown, two bands and its foot, each with points of its own along
    the hard edges; two buttons stand 3 cm or more apart). The axis is the button's thinnest direction (a dome is
    wider than it is high), within a degree of its own facings' sum on every button of the first file. Which way is
    out: its facings' sum, turned round if the button is inside out (most points facing toward its centre); for a
    closed button, whose facings sum to almost nothing, the facing of the cloth it is sewn on (its 16 nearest points
    of other paints within 3 cm: too rough for the axis itself, up to 79 degrees off by a pocket's flap)."""
    import numpy as np
    from scipy.cluster.hierarchy import fcluster, linkage
    from scipy.spatial import cKDTree
    m = D["near"]
    T = np.array(m["triangles"]).reshape(-1, 3)
    pa = np.array(m["paints"])
    X = np.array(m["points"]).reshape(-1, 3)
    F = np.array(m["facings"]).reshape(-1, 3)
    v = np.unique(T[(pa[T] == paint).all(1)].ravel())
    if len(v) < 2:
        return []
    lab = fcluster(linkage(X[v], "single"), 0.006, "distance")
    others = np.where((pa != paint) & ~np.isin(pa, (35, 36, 37)))[0]      # (not a gore twin's wound caps)
    tree = cKDTree(X[others])
    out = []
    for k in np.unique(lab):
        u = v[lab == k]
        if len(u) < 4:
            continue
        c = X[u].mean(0)
        P = X[u] - c
        ax = np.linalg.eigh(P.T @ P)[1][:, 0]
        fs = F[u].sum(0)
        if np.linalg.norm(fs) / len(u) > 0.05:
            ref = fs if np.mean(np.sum(F[u] * P, 1) > 0) >= 0.5 else -fs
        else:
            dist, idx = tree.query(c, k=16, distance_upper_bound=0.03)
            idx = idx[np.isfinite(dist)]
            ref = F[others[idx]].sum(0) if len(idx) else fs
        if not np.all(np.isfinite(ax)) or abs(float(ax @ ref)) < 1e-9:
            continue
        out.append((c, ax if ax @ ref > 0 else -ax, u))
    return out


def _glint(D):
    """Lift each button's crown in D's near tint (in memory); returns the report. Point by point, so it can be run
    again on a file that has it: a crown point still at its lifted value is left; one whose tint has been baked again
    is lifted afresh; a point that was crown and is no longer (its button mended) and still lifted is put back. Only
    the numbers it changes are rounded (to 4 places, as face_detail writes them)."""
    import numpy as np
    m = D["near"]
    paint = D.get("new_paints", {}).get("button", BUTTON)
    X = np.array(m["points"]).reshape(-1, 3)
    F = np.array(m["facings"]).reshape(-1, 3)
    tint = np.array(m["tint"], float).reshape(-1, 3)
    groups = _button_groups(D, paint)
    if not groups:
        raise SystemExit("no buttons found at near")
    inside_out = sum(np.mean(np.sum(F[u] * (X[u] - c), 1) > 0) < 0.5 for c, _, u in groups)
    if inside_out:
        raise SystemExit(f"{inside_out} buttons inside out: --mend-buttons first")
    old = D.get("button_glint") or {}
    if old and "crown" not in old:
        raise SystemExit(f"a glint from before its marker kept its crown ({old}): put the near tint back as it was "
                         "baked (shade.py), take button_glint out of the file, then glint")
    was = {int(i): np.array(b, float) for i, b in zip(old.get("crown", []), np.reshape(old.get("base", []), (-1, 3)))}
    if old and len(tint) != old.get("near_points", len(tint)):
        was = {}                              # (a re-made mesh: the old crown's numbers name other points)
    lift = old.get("lift", GLINT)
    crown = sorted(int(i) for c, ax, u in groups for i in u[F[u] @ ax > GLINT_COS])
    base, changed = {}, {"lifted": 0, "kept": 0, "put back": 0}
    flat = list(m["tint"])
    for i in crown:
        cur = tint[i]
        if i in was and np.allclose(cur, was[i] * lift, atol=0.0011):
            base[i] = was[i]
            changed["kept"] += 1
            continue
        base[i] = cur.copy()
        for j in range(3):
            flat[3 * i + j] = round(float(cur[j] * GLINT), 4)
        changed["lifted"] += 1
    for i, b in was.items():
        if i not in base and i < len(tint) and np.allclose(tint[i], b * lift, atol=0.0011):
            for j in range(3):
                flat[3 * i + j] = round(float(b[j]), 4)
            changed["put back"] += 1
    before = m["tint"]
    m["tint"] = flat
    changed["face_detail marker kept"] = _fd_keep(D, "near", before)
    D["button_glint"] = {"lift": GLINT, "cos": GLINT_COS, "buttons": len(groups), "points": len(crown),
                         "near_points": len(tint), "crown": crown,
                         "base": [round(float(x), 4) for i in crown for x in base[i]]}
    return changed


def mend_buttons(path=None):
    """Any button made inside out, turned right way out at every level: its triangles' winding swapped and its points'
    facings turned round (they are the button's own: each point carries one paint). Near and mid, where a button is a
    closed lump, by its own centre (most triangles and facings point away from it on a sound one); far, where it is a
    card or two, by the axis of the near button it stands for. Refuses a file that has had it."""
    import numpy as np
    path = path or os.path.join(SHARED, NAME + ".json")
    h0 = _sha(path)
    D = json.load(open(path))
    if "button_mend" in D:
        raise SystemExit(f"{path} has been mended already ({D['button_mend']})")
    paint = D.get("new_paints", {}).get("button")
    if paint is None:
        raise SystemExit(f"{path} has no button paint")
    groups = _button_groups(D, paint)
    if not groups:
        raise SystemExit(f"{path}: no buttons found at near")
    C = np.array([g[0] for g in groups])
    done = {}
    for lod in ("near", "mid", "far"):
        m = D[lod]
        T = np.array(m["triangles"]).reshape(-1, 3)
        pa = np.array(m["paints"])
        X = np.array(m["points"]).reshape(-1, 3)
        F = np.array(m["facings"]).reshape(-1, 3)
        bt = np.where((pa[T] == paint).all(1))[0]
        if not len(bt):
            continue
        cen = X[T[bt]].mean(1)
        d = np.linalg.norm(cen[:, None, :] - C[None], axis=2)
        which = d.argmin(1)
        flipped_t, flipped_p, mended, at = 0, 0, 0, set()
        for k in np.unique(which):
            tr = bt[(which == k) & (d.min(1) < 0.02)]
            if not len(tr):
                continue
            a, b, c = X[T[tr, 0]], X[T[tr, 1]], X[T[tr, 2]]
            n = -np.cross(b - a, c - a)           # (the file's front face: the reverse of (b - a) x (c - a))
            u = np.unique(T[tr].ravel())
            if lod != "far" and len(tr) >= 4:
                ctr = X[u].mean(0)
                t_in = np.mean(np.sum(n * ((a + b + c) / 3 - ctr), 1) > 0) < 0.5
                p_in = np.mean(np.sum(F[u] * (X[u] - ctr), 1) > 0) < 0.5
                flip_t = tr if t_in else tr[:0]
                flip_p = u if p_in else u[:0]
            else:
                ax = groups[k][1]
                flip_t = tr[n @ ax < 0]
                flip_p = u[F[u] @ ax < 0]
            if len(flip_t) or len(flip_p):
                mended += 1
            T[flip_t] = T[flip_t][:, [0, 2, 1]]
            fl = m["facings"]                     # (turned round in the file's own numbers: nothing else moves)
            for i in flip_p:
                for j in range(3):
                    x = fl[3 * int(i) + j]
                    fl[3 * int(i) + j] = -x if x else x
            flipped_t += len(flip_t)
            flipped_p += len(flip_p)
            if len(flip_t) or len(flip_p):
                at.update(int(i) for i in u)      # (the whole button: its wear arrays are baked again)
        m["triangles"] = T.ravel().tolist()
        done[lod] = {"buttons": mended, "triangles": flipped_t, "points": flipped_p, "at": sorted(int(i) for i in at)}
    print("mend_buttons:", {k: {a: b for a, b in v.items() if a != "at"} for k, v in done.items()})
    if not any(v["triangles"] or v["points"] for v in done.values()):
        print("nothing inside out: file left as it is")
        return done
    D["button_mend"] = done
    _put(path, D, h0, "mend_buttons")
    if not _is_twin(path, D):
        print("next: --reshade-mended (bakes the mended buttons' shade again, and only theirs: shade.py --deliver "
              "would put back the whole bake over face_detail's colouring and the glint); mend the twin first")
    return done


def reshade_mended(path=None, twin=None):
    """After --mend-buttons on the man and his twin: the mended buttons' wear arrays (shade, shade_clean, dirt,
    dirt_wet, wet, snow, tint), baked facing inward, baked again by shade.py on the mended man and put on those points
    only (every other number stays as it is), their crowns glinted (_glint: the other buttons keep theirs), and the
    same numbers given to the twin on the same points (his own points are the man's, in the same order)."""
    import numpy as np
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    import shade
    path = path or os.path.join(SHARED, NAME + ".json")
    twin = twin or path[:-5] + "_gore.json"
    h0, hy = _sha(path), _sha(twin)
    D, Y = json.load(open(path)), json.load(open(twin))
    bm, by = D.get("button_mend"), Y.get("button_mend")
    if not bm or not by:
        raise SystemExit("mend both files first (--mend-buttons on the man and on the twin)")
    if bm.get("shade") or by.get("shade"):
        raise SystemExit(f"re-baked already: {bm.get('shade')}")
    if any(bm[lod].get("at") != by[lod].get("at") for lod in ("near", "mid", "far") if lod in bm):
        raise SystemExit("the man and the twin were mended on different points")
    t0 = time.time()
    res = shade.shade_file(D, log=lambda *a: None)
    report = {}
    for lod in ("near", "mid", "far"):
        at = (bm.get(lod) or {}).get("at") or []
        m = D[lod]
        before = list(m["tint"])
        for k in shade.WEAR_KEYS:
            v = np.asarray(res[lod][k], float).reshape(len(m["paints"]), -1)
            w = v.shape[1]
            for i in at:
                for j in range(w):
                    m[k][w * i + j] = round(float(v[i, j]), shade.ROUND[k])
        if lod != "near":
            _fd_keep(D, lod, before)
        report[lod] = {"points": len(at), "shade": round(float(np.mean([m["shade"][i] for i in at])), 3) if at else None}
    nb = list(D["near"]["tint"])
    report["glint"] = _glint(D)
    _fd_keep(D, "near", nb)                  # (in case: _glint keeps it already)
    stamp = time.strftime("%d %b %H:%M")
    bm["shade"] = f"wear arrays baked again on the mended points only (shade.py on the mended file, {stamp}); glinted"
    for lod in ("near", "mid", "far"):
        n0 = len(D[lod]["paints"])
        if Y[lod]["points"][:3 * n0] != D[lod]["points"] or Y[lod]["facings"][:3 * n0] != D[lod]["facings"]:
            raise SystemExit(f"the twin's {lod} is not the man's in its first {n0} points: not his twin")
        at = (bm.get(lod) or {}).get("at") or []
        for k in shade.WEAR_KEYS:
            w = 3 if k == "tint" else 1
            for i in at:
                Y[lod][k][w * i:w * i + w] = D[lod][k][w * i:w * i + w]
    paint = D["new_paints"]["button"]                # (the twin's buttons follow the man's glint, as shade --twin gives)
    for i, pt in enumerate(D["near"]["paints"]):
        if pt == paint:
            Y["near"]["tint"][3 * i:3 * i + 3] = D["near"]["tint"][3 * i:3 * i + 3]
    by["shade"] = f"the man's re-baked wear arrays on the mended points, and his glint ({stamp})"
    print(f"reshade_mended: baked in {time.time() - t0:.0f} s", json.dumps(report))
    _put(path, D, h0, "reshade_mended (man)")
    _put(twin, Y, hy, "reshade_mended (twin)")
    return report


def glint(path=None):
    """The buttons' small highlight (the art director's carry: dull pebbled metal with a small highlight): the near
    tint of each button's crown (the points facing within about 25 degrees of the surface it is sewn on) lifted by
    GLINT. Run on the man's file once its tint is final (after shade.py --deliver and face_detail). Not on his gore
    twin: shade.py --twin takes the twin's tint from the man's file, glint and all (while shade.py's stage for the man
    is older than his file: face_mesh adds points). Safe to run again (see _glint)."""
    path = path or os.path.join(SHARED, NAME + ".json")
    h0 = _sha(path)
    D = json.load(open(path))
    if _is_twin(path, D):
        raise SystemExit(f"{path} is a gore twin: shade.py --twin gives it the man's glint; not lifted twice")
    rep = _glint(D)
    print("glint:", rep, D["button_glint"]["buttons"], "buttons,", D["button_glint"]["points"], "crown points")
    if not rep["lifted"] and not rep["put back"]:
        print("nothing to change: file left as it is")
        return rep
    _put(path, D, h0, "glint")
    return rep


if __name__ == "__main__":
    if "--export" in sys.argv:
        export()
    if "--combine" in sys.argv:
        combine()
    for flag, fn in (("--mend-buttons", mend_buttons), ("--reshade-mended", reshade_mended), ("--glint", glint)):
        if flag in sys.argv:
            i = sys.argv.index(flag) + 1
            fn(sys.argv[i] if len(sys.argv) > i and not sys.argv[i].startswith("--") else None)
