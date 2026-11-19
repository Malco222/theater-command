"""The Soviet rifleman of the Eastern Front battles, made beside soldier.py (which stays as it is).

    python soldier_sov.py                     build, rig, animate and export the Soviet man, then his picture
    python soldier_sov.py --only export       (steps: export, picture; --fast for a small test picture)

He is soldier.py's man in every bone and clip: the same MakeHuman body, skeleton, mocap and weapon work, so the game
plays him with the German's clips. What is his own is laid on in this file, by wrapping three of soldier.py's makers
before `assemble` calls them:

- the shirt-tunic of 1943 (a pullover worn outside the trousers): a stand collar in the tunic's own cloth, a short
  placket of three buttons from the collar to the breast, shoulder boards, and no pockets (soldier.py's tunic has a
  button strip to the hem and four pockets: those shapes are taken off);
- breeches full in the thigh, tucked into tall boots;
- his kit: belt, ammunition pouches, bread bag, water bottle and entrenching tool as soldier.py makes them, no Y-straps,
  and the greatcoat rolled and worn as a ring over the LEFT shoulder to the right hip (clear of the rifle's butt);
- the steel helmet of 1940 (soldier.py's "soviet" helmet);
- his colours for the pictures (the game paints him by number from the unit table: cloth 7B7748).

Everything is our own shape. Files go to battle-models/land/soldier/ as soldier_SOV*.glb and soldier_SOV.json.
"""

import json
import math
import os
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)

import soldier as S  # noqa: E402  (imports bpy first)
import bpy  # noqa: E402
import numpy as np  # noqa: E402
import kit  # noqa: E402
from sculpt import HalfSpace, Layer, RoundBox, RoundCone, Sphere, frame_from, on_surface  # noqa: E402
from soldier import Both  # noqa: E402

NATION = "SOV"

# ---- his colours in the pictures (studio only: the game colours him by paint number)
S.LOOK[NATION] = {
    "tunic": "7B7748",      # the unit table's cloth: a pale khaki green, sun-faded
    "trousers": "6C6943",   # the breeches a shade darker
    "boots": "26211D",      # blackened boots
    "lower": "2C2824",
    "straps": "5B4A33",     # brown leather belt and pouches
    "helmet": "4E5A3A",     # the helmet's green (the vehicles' 4F5E3E)
    "pack": "7A7154",       # canvas bread bag and shovel carrier
    "skin": "BDA08E",
    "wood": "6B4429",
    "steel": "34363A",
    "eye": "2A2420",
    "leather": "3A2E25",
    "blanket": "675D4E",    # the greatcoat's grey-brown wool, rolled
}
S.NATION_SEED[NATION] = 19          # (his face: one of MakeHuman's mixes no other army's file uses)

S.SCRATCH = "/home/user/art/soldier_sov/"
S.STAGE = os.path.join(S.SCRATCH, "out")


def _is_box(p, half, tol=1e-6):
    return isinstance(p, RoundBox) and np.allclose(p.half, half, atol=tol)


# ================================================================================================ the uniform
_uniform_layers = S.uniform_layers


def uniform_layers(man, J, spec, LM):
    layers = _uniform_layers(man, J, spec, LM)
    if spec is not S.NATION_KIT[NATION]:
        return layers
    tunic, trousers, collar = layers[0], layers[1], layers[3]
    Z = LM["Z"]
    # ---- off: the button strip to the hem and the four pockets with their flaps (soldier.py's tunic)
    pocket_halves = [(0.006, 0.05, 0.058), (0.006, 0.06, 0.07), (0.0075, 0.054, 0.018), (0.0075, 0.064, 0.018)]
    keep = []
    gone = 0
    for how, p, k in tunic.ops:
        strip = isinstance(p, RoundBox) and abs(p.half[0] - 0.006) < 1e-6 and abs(p.half[1] - 0.014) < 1e-6 and p.half[2] > 0.08
        if how == "add" and (strip or any(_is_box(p, h) for h in pocket_halves)):
            gone += 1
            continue
        keep.append((how, p, k))
    tunic.ops = keep
    LM["pockets"] = []
    print(f"  SOV: {gone} shapes of the German tunic's front taken off (strip, pockets, flaps)")
    # ---- the stand collar in the tunic's own cloth
    collar.paint = "tunic"
    # ---- the short placket: from the collar down to the breast, a little proud, its edge to the wearer's left
    nmid, nfwd, nrx, nry = LM["neck_ring"]
    top = nmid + nfwd * nrx
    low = Z(1.3)
    p, nrm = on_surface(tunic, (0.4, 0.004, (top[2] - 0.02 + low) / 2), (-1.0, 0.0, 0.0))
    half = (top[2] - 0.02 - low) / 2
    tunic.add(RoundBox(p - nrm * 0.003, (0.0055, 0.016, half), 0.004, frame_from(nrm, (0.0, 0.0, 1.0))), 0.006)
    LM["placket"] = (top[2] - 0.035, low + 0.015)
    # ---- the breeches: full in the thigh, the fullness on the outside, drawn in above the knee to the boot
    # (only below the tunic's hem, so none of it shows through the skirt)
    below_hem = HalfSpace((0.0, 0.0, LM["hem"] - 0.01), (0.0, 0.0, 1.0))
    for s, side in (("l", 1.0), ("r", -1.0)):
        hip, knee = np.array(J["thigh_" + s]), np.array(J["calf_" + s])
        at = lambda t, out, fwd: hip + (knee - hip) * t + np.array((fwd, side * out, 0.0))
        a, b, c = at(0.36, 0.014, 0.0), at(0.55, 0.03, 0.004), at(0.78, 0.016, 0.0)      # (the fullness to the side)
        trousers.add(Both(RoundCone(a, b, 0.07, 0.083), below_hem), 0.04)
        trousers.add(Both(RoundCone(b, c, 0.083, 0.062), below_hem), 0.04)
    return layers


S.uniform_layers = uniform_layers


# ================================================================================================ the kit
_build_kit = S.build_kit


def build_kit(J, spec, body_ls, Z=None):
    if spec is not S.NATION_KIT[NATION]:
        return _build_kit(J, spec, body_ls, Z)
    out = _build_kit(J, spec, body_ls, Z)
    Z = Z or (lambda z: z)
    tunic, collar = body_ls[0], body_ls[3]
    for name in ("straps", "details", "roll"):              # (no Y-straps; his own buttons and roll below)
        ob = out.pop(name, (None, None))[0]
        if ob is not None:
            bpy.data.objects.remove(ob)
    # ---- the small things: three buttons down the placket, one on each shoulder board, a star on the buckle
    g = S.Geo()
    for z in (Z(1.418), Z(1.373), Z(1.328)):
        p, nrm = on_surface(tunic, (0.3, 0.004, z), (-1.0, 0.0, 0.0))
        g.tube(p - nrm * 0.001, p + nrm * 0.0032, 0.0072, 0.0062, "steel", 8)
    for side in (1.0, -1.0):
        p, nrm = on_surface(tunic, (J["clavicle_l"][0], side * 0.1, Z(1.62)), (0.0, 0.0, -1.0))
        g.tube(p - nrm * 0.001, p + nrm * 0.0035, 0.0068, 0.0058, "steel", 8)
    out["details"] = (g.build("details"), None)
    # ---- the greatcoat rolled into a ring, over the left shoulder and down across the chest and back to the right hip
    L = Layer("roll", "blanket")
    pts = []
    for k in range(17):
        t = 2.0 * math.pi * k / 16.0
        lift = math.sin(t)
        pts.append((0.165 * math.cos(t) - 0.02, 0.03 + 0.185 * lift, Z(1.2 + 0.255 * lift)))
    for k in range(16):
        L.add(RoundCone(pts[k], pts[k + 1], 0.047, 0.047), 0.02)
    # the two ends tied where they meet at the right hip, the tie a little proud
    tie = np.array(pts[12])
    L.add(Sphere(tie + np.array((0.0, -0.006, 0.0)), 0.053), 0.012)
    out["roll"] = (S.mesh_from_layers("roll", [L], 0.005, 300), None)
    return out


S.build_kit = build_kit


# ================================================================================================ the files
def export_sov(info):
    """soldier_SOV.glb (LOD0, every clip), soldier_SOV_LOD1.glb and _LOD2.glb (same skeleton, no clips) and
    soldier_SOV.json (the game's own form). No .fbx: the game reads the json and the glb (and the share is near full)."""
    t0 = time.time()
    sc = kit.fresh(S.FPS)
    m = S.assemble(NATION)
    S.bake_clips(m)
    socks = m["sockets"]
    lods = {0: S.man_objects(m), 1: S.make_lods(m, 1), 2: S.make_lods(m, 2)}
    sc.frame_set(0)
    n = {lod: sum(S.tris_of(o) for o in lods[lod]) for lod in lods}
    info["tris"] = n
    info["parts"] = {o.name.split(".")[0]: S.tris_of(o) for o in lods[0]}
    info["short"] = {k: float(v) for k, v in m["short"].items()}
    S.export_glb(S.stage_path(f"soldier_{NATION}.glb"), [m["arm"]] + lods[0] + socks)
    for lod in (1, 2):
        S.export_glb(S.stage_path(f"soldier_{NATION}_LOD{lod}.glb"), [m["arm"]] + lods[lod] + socks, anim=False)
    data = S.write_game_json(S.stage_path(f"soldier_{NATION}.json"), m, lods)
    data["about"] = data["about"].replace("soldier.py.", "soldier_sov.py on soldier.py.")
    with open(S.stage_path(f"soldier_{NATION}.json"), "w") as f:
        json.dump(data, f, separators=(",", ":"))
    info["json"] = {k: len(data[k]["triangles"]) // 3 for k in ("near", "mid", "far")}
    info["high"] = data["high"]
    for n_ in (f"soldier_{NATION}.glb", f"soldier_{NATION}_LOD1.glb", f"soldier_{NATION}_LOD2.glb", f"soldier_{NATION}.json"):
        S.deliver(n_)
    info["export_s"] = round(time.time() - t0)
    print(f"  {NATION} exported in {time.time() - t0:.0f} s: {n}")
    return info


# ================================================================================================ his picture
def picture(info, fast=False):
    """The Soviet man and the German side by side at ease, from the front quarter and from behind, on the studio
    backdrop: one sheet, pictures/soldier_SOV.png."""
    kit.fresh(S.FPS)
    men = []
    for nation, y in (("SOV", -0.55), ("GER", 0.55)):
        m = S.assemble(nation)
        S.studio_smooth(m)
        for mt in bpy.data.materials:
            if not mt.name.startswith(("SOV_", "GER_", "backdrop", "ground")):
                mt.name = nation + "_" + mt.name
        m["arm"].location = (0.0, y, 0.0)
        P = S.base_stand()
        P["foot_l"] = {"at": (0.12, 0.12, S.ANKLE), "yaw": 6.0}
        P["foot_r"] = {"at": (-0.06, -0.12, S.ANKLE), "yaw": -8.0}
        S.pose_once(m["arm"], m["rig"], P, 0)
        men.append(m)
    kit.studio()
    kit.backdrop("CFC6B4")
    cam = kit.camera(lens=50)
    S.shared_render(640 if fast else 1100, 760 if fast else 1300, 12 if fast else 48)
    shots = []
    for key, yaw, pitch, dist in (("front", 25.0, 6.0, 4.6), ("back", 200.0, 8.0, 4.6), ("near", 15.0, 4.0, 2.1)):
        kit.orbit(cam, (0.0, -0.2 if key == "near" else 0.0, 1.0 if key != "near" else 1.3), dist, pitch, yaw)
        path = os.path.join(S.SCRATCH, "pictures", f"sov_{key}.png")
        os.makedirs(os.path.dirname(path), exist_ok=True)
        kit.still(path)
        shots.append(path)
    out = S.stage_path("pictures", f"soldier_{NATION}.png")
    kit.sheet(shots, out, cols=3, labels=["Soviet rifleman (left) beside the German, at ease", "From behind: the rolled greatcoat",
                                          "Close: stand collar, short placket, shoulder boards"],
              title="Soviet rifleman: soldier_sov.py on soldier.py (our own design)")
    if not fast:
        S.deliver(f"pictures/soldier_{NATION}.png")
    info["picture"] = out
    return out


def main():
    a = kit.args()
    fast = "--fast" in a
    steps = a[a.index("--only") + 1].split(",") if "--only" in a else ["export", "picture"]
    p = S.stage_path(f"export_info_{NATION}.json")
    info = json.load(open(p)) if os.path.exists(p) else {}
    for step in steps:
        t0 = time.time()
        if step == "export":
            export_sov(info)
        elif step == "picture":
            picture(info, fast)
        with open(p, "w") as f:
            json.dump(info, f, indent=1)
        print(f"step {step}: {time.time() - t0:.0f} s")
    ok = S.check_delivered()
    print("delivered" if ok else "SOME FILES DID NOT LAND")


if __name__ == "__main__":
    main()
