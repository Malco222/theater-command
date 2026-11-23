"""Motorcycles: the despatch riders' solo machines and the sidecar combinations that carried a machine gun ahead of
the columns.

    R75           the German heavy sidecar machine (BMW R 75, 1941-44): a flat twin across her frame, telescopic
                  forks, the air filter on her tank, steel panniers, and a driven sidecar with an MG 34 on its nose
                  and the spare wheel on its tail. The swap `sidecar` takes the sidecar off: a solo machine.
    M72           the Soviet M-72 (1942-45) from the same numbers: a side-valve flat twin, plunger springs at her back
                  wheel, knee pads on her tank, no panniers, a sidecar that is not driven (its wheel well ahead of
                  hers) with a DT on its nose. Soviet green, no markings. The same swap `sidecar`.

PARTS   vehicle (root) > hull (the machine on her wheels: rocks; a solo machine also leans, the game rolls the root)
        > steer (the forks, the bars and the lamp: the game steers) > wheel_front; hull > sidecar (the tub and its
        chassis) > mg_mount > mg_cradle; vehicle > wheel_back, wheel_side (the game rolls them)
CLIPS   a lorry's (idle, start, drive, move_off, stop, hit, knocked_out, burn) and mg_burst
SOCKETS driver (the top of the saddle), grip.driver.l/r (his hands), foot.l/r (his boots); rider.1 (the man on the
        pillion); gunner (the man in the sidecar, seated, his hands on grip.gunner.l/r); mg; engine, exhaust.N, fire.N,
        hit.*, dust.*
Every shape is our own mesh, made from numbers here.
"""

import math

from mathutils import Vector

from light_cars import LIGHT_WORN, LightWorks
from motorpool import LEFT, RIGHT, X, Y, Z
from tanks_ger import coats


class BikeWorks(LightWorks):
    def spoke_wheel(self, name, parent, c, r, w, rim=None, spokes=40, hub=0.085, drum=0.09):
        """A wire-spoked wheel on a tyre, a part that rolls: the rim, the brake drum at its hub, the spokes laced from
        the hub's two flanges to the rim."""
        c = Vector(c)
        rim = rim or r - w * 0.95
        self.part(name, parent, c, axis=Y, kind="spin", note=f"rolls: + forward; radius {r:.3f} m")
        self.wheels.append((name, r))
        self.tyre(c, Y, r, rim, w, lugs=self.n(18, 8))
        self.lathe(c, Y, [(w * 0.45, rim - 0.01), (w * 0.45, rim + 0.004), (w * 0.3, rim + 0.006), (-w * 0.3, rim + 0.006), (-w * 0.45, rim + 0.004),
                          (-w * 0.45, rim - 0.01)], "steel", self.n(32, 10))                                           # the rim
        self.lathe(c, Y, [(drum / 2, 0.0), (drum / 2, hub), (drum / 2 - 0.01, hub + 0.008), (-drum / 2, hub + 0.008), (-drum / 2, 0.0)], "shade", self.n(18, 8))   # the hub and its brake drum
        self.lathe(c, Y, [(drum / 2 + 0.03, 0.0), (drum / 2 + 0.03, 0.025), (-drum / 2 - 0.03, 0.025), (-drum / 2 - 0.03, 0.0)], "steel", 8)                    # the spindle
        if self.fine:
            for k in range(spokes):
                side = 1.0 if k % 2 == 0 else -1.0
                a = 2 * math.pi * k / spokes
                b = a + side * 0.55 * (1 if (k // 2) % 2 == 0 else -1)                                  # (laced: each crosses its neighbours)
                p0 = c + Y * (side * drum * 0.42) + (X * math.cos(a) + Z * math.sin(a)) * (hub + 0.004)
                p1 = c + Y * (side * 0.004) + (X * math.cos(b) + Z * math.sin(b)) * (rim - 0.008)
                self.rod(p0, p1, 0.0024, "steel", 4, caps=False)
        return name

    def mudguard_y(self, c, r0, a0, a1, half, paint="upper", valance=0.0):
        """A mudguard over a motorcycle's wheel at c (x, z) in her own plane: an arc of plate from a0 to a1 degrees
        (0 forward, 90 over the top), `half` wide either side; valance: its edges turned down that far."""
        self.mudguard(c, r0, a0, a1, -half, half, paint=paint, step=9)
        if valance and self.fine:
            for s in (LEFT, RIGHT):
                pts = [(c[0] + (r0 - valance) * math.cos(math.radians(a)), c[1] + (r0 - valance) * math.sin(math.radians(a))) for a in range(int(a0), int(a1) + 1, 9)]
                outer = [(c[0] + r0 * math.cos(math.radians(a)), c[1] + r0 * math.sin(math.radians(a))) for a in range(int(a1), int(a0) - 1, -9)]
                self.extrude(pts + outer, s * half - 0.004, s * half + 0.004, paint)

    def bike_clips(self, solo_note):
        self.carrier_clips("lorry_idle_loop", "lorry_drive_loop", mg="mg" if any(s["name"] == "mg" for s in self.m.sockets) else None, soft=1.6)
        ctl = self.m.info["file"]["controls"]
        ctl["steer"] = {"parts": ["steer"], "what": "the game steers the forks inside their lock: + to the right. " + solo_note,
                        "limits": self.m.parts[self.m.part_at["steer"]].limits}
        for p in ("steer",):
            self.m.clips["knocked_out"]["tracks"].append({"part": p, "turn": [[0.0, 0.0], [0.2, 0.0], [0.6, 25.0], [0.75, 21.0], [0.9, 23.0], [3.0, 23.0]]})


def flat_twin(v, x, z, bore=0.068, out=0.33, fins=7, head="ohv"):
    """A flat twin's two cylinders standing out across her, finned. head "ohv": the heads and their rocker covers at
    the ends (the R 75); "sv": a side-valve twin's flat finned heads and the valve chest under each cylinder (the M-72)."""
    for s in (LEFT, RIGHT):
        a = Vector((x, s * 0.11, z))
        prof = [(0.0, 0.0), (0.0, bore + 0.006)]
        L = out - 0.11 - 0.07
        for k in range(fins if v.fine else 1):
            u0, u1 = L * k / fins, L * (k + 0.5) / fins
            prof += [(u0, bore + 0.024), (u1, bore + 0.024), (u1, bore + 0.006), (L * (k + 1) / fins, bore + 0.006)] if v.fine else []
        prof += [(L, bore + 0.02), (L, 0.0)]
        v.lathe(a, Y * s, prof[::-1], "steel", v.n(18, 8))
        hd = a + Y * s * L
        if head == "sv":
            prof = [(0.0, 0.0), (0.0, 0.088)]
            for k in range(3 if v.fine else 1):
                prof += [(0.008 + 0.014 * k, 0.088), (0.008 + 0.014 * k, 0.098), (0.014 + 0.014 * k, 0.098), (0.014 + 0.014 * k, 0.088)] if v.fine else []
            prof += [(0.05, 0.088), (0.05, 0.03), (0.055, 0.0)]
            v.lathe(hd, Y * s, prof[::-1], "steel", v.n(18, 8))                                                       # the flat finned head
            v.slab((x - 0.045, s * 0.13, z - bore - 0.05), (x + 0.045, s * (0.11 + L * 0.8), z - bore + 0.01), "steel", 0.01)   # the valve chest under it
            if v.fine:
                v.bolts(hd + Y * s * 0.052, Y * s, 0.06, 6, r=0.007, paint="steel")
        else:
            v.lathe(hd, Y * s, [(0.075, 0.0), (0.075, 0.06), (0.05, 0.085), (0.0, 0.09), (0.0, 0.0)], "steel", v.n(18, 8))     # the head and rocker cover
            if v.fine:
                v.bolts(hd + Y * s * 0.072, Y * s, 0.05, 4, r=0.008, paint="steel")


# =====================================================================================================================
def r75(fine=True, tag="GER", paint=None, sidecar=True):
    """The BMW R 75 and its driven sidecar: 2.40 m long, 1.73 m wide, 1.00 m to her bars. Three men: the driver,
    one on the pillion, the gunner in the sidecar behind his MG 34. Dark grey to early 1943, dark yellow after, the
    desert coats in Africa: a paint swap."""
    return _flat_twin_combo("r75", fine, tag, paint, sidecar)


def m72(fine=True, tag="SOV", paint=None, sidecar=True):
    """The Soviet M-72 (1942-45; built at Moscow, then Irbit and Gorky) and her sidecar: 2.42 m long, 1.60 m wide,
    1.00 m to her bars. A side-valve flat twin, plunger springs at her back wheel, knee pads on her tank, no panniers;
    the sidecar is not driven and its wheel stands well ahead of hers. Three men, the gunner behind a DT on the
    sidecar's nose. Soviet green, no markings."""
    return _flat_twin_combo("m72", fine, tag, paint, sidecar)


def _flat_twin_combo(kind, fine, tag, paint, sidecar):
    """The two flat-twin sidecar machines from one set of numbers: kind "r75" (Germany) or "m72" (the Soviet Union)."""
    r75k = kind == "r75"
    if r75k:
        from tanks_ger import GER_COATS
        v = BikeWorks("R75", "BMW R 75 with sidecar", "bike", tag, fine, paint=GER_COATS[paint or "grey"][0])
    else:
        v = BikeWorks("M72", "M-72 with sidecar", "bike", tag, fine, paint=paint)
    v.dice = 0.2
    v.worn_kw = dict(LIGHT_WORN, ground="field", foot=0.30, top=0.62, shade_knee=0.3, shade_reach=0.35)   # (dust to her hubs, her tank and the tub's top clean)
    v.root(hull_pivot=(0.0, 0.0, 0.40), rock=(-4.0, 4.0))
    if r75k:
        R, W = 0.31, 0.11                                # 4.50-16
        xf, xr = 0.72, -0.72
        ys, xsw = -1.10, -0.60                           # the sidecar wheel: out to her right, a little ahead of the back wheel (driven)
    else:
        R, W = 0.335, 0.095                              # 3.75-19
        xf, xr = 0.70, -0.70
        ys, xsw = -1.04, -0.32                           # the sidecar wheel: well ahead of the back wheel (not driven)
    yt = -0.68 if r75k else -0.64                        # the sidecar's middle

    # ---- the back wheel and its drive; the frame; the engine, the box, the shaft -----------------------------------
    v.spoke_wheel("wheel_back", "vehicle", (xr, 0.0, R), R, W)
    v.use("hull")
    for s in (LEFT, RIGHT):                                                                    # the cradle frame, two tubes
        v.tube([(0.47, s * 0.05, 0.84), (0.44, s * 0.08, 0.60), (0.36, s * 0.10, 0.22), (-0.22, s * 0.10, 0.20), (-0.34, s * 0.10, 0.36), (-0.36, s * 0.10, 0.66)], 0.018, "upper", 8)
        if r75k:
            v.tube([(-0.36, s * 0.10, 0.66), (xr, s * 0.08, R + 0.02)], 0.016, "upper", 8)
            v.tube([(-0.22, s * 0.10, 0.20), (xr, s * 0.08, R - 0.02)], 0.016, "upper", 8)
        else:                                                                                  # the plunger boxes: the axle rides between springs in two tall tubes
            v.tube([(-0.36, s * 0.10, 0.66), (xr, s * 0.09, R + 0.15)], 0.016, "upper", 8)
            v.tube([(-0.22, s * 0.10, 0.20), (xr, s * 0.09, R - 0.11)], 0.016, "upper", 8)
            v.lathe((xr, s * 0.09, R - 0.12), Z, [(0.28, 0.0), (0.28, 0.026), (0.20, 0.03), (0.08, 0.03), (0.0, 0.026), (0.0, 0.0)], "upper", v.n(12, 6))
        v.rod((xr, s * 0.08, R), (xr, s * 0.065, R), 0.02, "steel", 8)
    v.tube([(0.50, 0.0, 0.86), (0.10, 0.0, 0.76), (-0.36, 0.0, 0.66)], 0.02, "upper", 8)            # the top tube, under the tank
    v.lathe((0.49, 0.0, 0.78), Vector((-0.22, 0.0, 0.54)), [(0.14, 0.0), (0.14, 0.032), (0.0, 0.032), (0.0, 0.0)], "upper", 10)   # the steering head
    v.hull([(0.42, 0.12, 0.24), (0.42, 0.12, 0.48), (0.16, 0.12, 0.24), (0.16, 0.12, 0.52), (0.30, 0.10, 0.56), (0.30, 0.10, 0.20)], "upper", 0.02)   # the crankcase
    flat_twin(v, 0.33, 0.40, head="ohv" if r75k else "sv")
    v.hull([(0.16, 0.10, 0.26), (0.16, 0.10, 0.50), (-0.10, 0.10, 0.28), (-0.10, 0.10, 0.48)], "upper", 0.02)               # the gearbox
    v.rod((-0.10, -0.09, 0.34), (xr + 0.06, -0.08, R + 0.02), 0.03, "upper", 8)                                       # the shaft, in its tube on her right
    v.ball((xr, -0.10, R), 0.075, "upper", 10, 8, squash=0.9)                                                          # the final drive
    for s in (LEFT, RIGHT):                                                                                              # the carburettors, behind the cylinders
        v.slab((0.17, s * 0.16, 0.40), (0.24, s * 0.22, 0.46), "steel", 0.01)
    # the exhausts: down from each head and back to a silencer each side
    for k, s in enumerate((LEFT, RIGHT)):
        v.tube([(0.36, s * 0.27, 0.36), (0.42, s * 0.24, 0.26), (0.30, s * 0.20, 0.18), (-0.10, s * 0.21, 0.20)], 0.018, "rust", 6)
        v.rod((-0.10, s * 0.21, 0.20), (-0.55, s * 0.21, 0.27), 0.042, "rust", v.n(14, 6))
        v.socket(f"exhaust.{k + 1}", (-0.57, s * 0.21, 0.27), (-1, 0, 0.1))
    # the tank, the air filter on it, the saddle on its springs, the pillion, the panniers
    v.hull([(0.44, 0.08, 0.80), (0.44, 0.08, 0.90), (0.36, 0.13, 0.74), (0.36, 0.12, 0.93), (-0.06, 0.13, 0.70), (-0.06, 0.11, 0.86), (-0.12, 0.08, 0.72), (-0.12, 0.07, 0.82)], "upper", 0.03)
    if r75k:
        v.lathe((0.20, 0.0, 0.92), Z, [(0.075, 0.0), (0.075, 0.09), (0.06, 0.095), (0.0, 0.095), (0.0, 0.0)], "upper", v.n(20, 8))   # the air filter
    else:
        v.lathe((0.06, 0.0, 0.54), Z, [(0.10, 0.0), (0.10, 0.06), (0.0, 0.06), (0.0, 0.0)], "upper", v.n(16, 8))         # the air filter, on the gearbox under the tank
        for s in (LEFT, RIGHT):
            v.slab((-0.02, s * 0.118, 0.74), (0.16, s * 0.142, 0.84), "rubber", 0.01)                                  # the knee pads
    if fine:
        v.slab((0.04, -0.06, 0.88), (0.14, 0.06, 0.885), "shade")                                                       # the toolbox's lid in the tank
        v.lathe((0.40, 0.04, 0.925), Z, [(0.02, 0.0), (0.02, 0.022), (0.0, 0.022), (0.0, 0.0)], "steel", 8)             # the filler cap
    v.hull([(-0.08, 0.05, 0.80), (-0.08, 0.05, 0.84), (-0.36, 0.13, 0.80), (-0.36, 0.13, 0.86), (-0.30, 0.0, 0.88)], "leather", 0.03)   # the saddle
    for s in (LEFT, RIGHT):
        v.rod((-0.32, s * 0.08, 0.66), (-0.32, s * 0.08, 0.80), 0.018, "steel", 8, rings=0.02 if fine else None)       # its springs
    v.mudguard_y((xr, R), R + 0.045, 10.0, 172.0, 0.07, valance=0.03)
    v.hull([(-0.50, 0.08, 0.82), (-0.50, 0.08, 0.86), (-0.78, 0.09, 0.78), (-0.78, 0.09, 0.84)], "leather", 0.025)       # the pillion pad
    v.tube([(-0.44, 0.09, 0.84), (-0.44, 0.09, 0.90), (-0.44, -0.09, 0.90), (-0.44, -0.09, 0.84)], 0.009, "steel", 5)  # its grab handle
    for s in (LEFT, RIGHT):
        if r75k:
            v.bin((-0.90, s * 0.13, 0.40), (-0.58, s * 0.27, 0.68), paint="upper", bevel=0.012, latches=1, lid_side=Z)
        else:
            v.rod((-0.52, s * 0.10, 0.36), (-0.52, s * 0.22, 0.36), 0.016, "rubber", 8)                               # the pillion's footrests
        v.rod((0.04, s * 0.17, 0.30), (0.04, s * 0.30, 0.30), 0.018, "rubber", 8)                                     # the footrests
    v.socket("engine", (0.30, 0.0, 0.40), Z, part="hull", note="the engine: smoke and the first fire")
    v.socket("fire.1", (0.20, 0.0, 0.85), Z, part="hull", note="the tank")

    # ---- the forks, the bars, the lamp, the front wheel (the game steers them) ---------------------------------------
    d = Vector((0.22, 0.0, -0.53)).normalized()
    v.part("steer", "hull", (0.49, 0.0, 0.82), axis=tuple(d), limits=[-35.0, 35.0], kind="turn", note="the forks on the steering head: + steers to the right")
    for s in (LEFT, RIGHT):
        v.rod((xf - 0.005, s * 0.085, R + 0.02), (0.52, s * 0.085, 0.86), 0.026, "upper", 10)                            # the fork legs
        v.rod((0.62, s * 0.085, 0.62), (0.53, s * 0.085, 0.86), 0.036, "upper", 10)
    v.slab((0.47, -0.11, 0.85), (0.58, 0.11, 0.89), "upper", 0.01)                                                       # the top yoke
    v.tube([(0.50, 0.0, 0.92), (0.48, 0.12, 0.95), (0.40, 0.30, 0.99), (0.37, 0.38, 1.00)], 0.012, "steel", 6)          # the bars
    v.tube([(0.50, 0.0, 0.92), (0.48, -0.12, 0.95), (0.40, -0.30, 0.99), (0.37, -0.38, 1.00)], 0.012, "steel", 6)
    for s in (LEFT, RIGHT):
        v.rod((0.385, s * 0.36, 1.0), (0.36, s * 0.47, 1.005), 0.017, "rubber", 8)
    v.headlamp((0.64, 0.0, 0.84), X, 0.09, stalk=0.0, cover=True)
    v.headlamp((0.86, 0.10, 0.66), X, 0.035, stalk=0.02, cover=True)                                                    # the blackout lamp on the guard
    v.mudguard_y((xf, R), R + 0.045, -25.0, 150.0, 0.065, valance=0.035)
    v.spoke_wheel("wheel_front", "steer", (xf, 0.0, R), R, W)
    v.use("hull")
    grips = [Vector((0.37, s * 0.42, 1.003)) for s in (LEFT, RIGHT)]

    # ---- the sidecar: the tub on its chassis, its wheel driven off her back axle, the gun on its nose -------------
    if sidecar:
        v.part("sidecar", "hull", (0.0, yt, 0.40), kind="fixed", note="the sidecar: its tub, chassis and gun (the swap `sidecar` takes it off)")
        v.rod((0.36, -0.10, 0.24), (0.30, yt + 0.10, 0.34), 0.02, "upper", 8)                                          # the chassis' arms out from her frame
        v.rod((-0.34, -0.10, 0.40), (-0.30, yt + 0.10, 0.36), 0.02, "upper", 8)
        v.rod((0.30, yt + 0.10, 0.34), (-0.70, yt + 0.10, 0.34), 0.02, "upper", 8)
        v.rod((0.30, yt + 0.10, 0.34), (xsw, ys + 0.12, 0.30), 0.02, "upper", 8)
        if r75k:
            v.rod((xr, -0.13, R), (xsw, ys + 0.10, R), 0.022, "steel", 8)                                               # the shaft that drives its wheel
        else:
            v.rod((xsw, ys + 0.10, R), (xsw, yt - 0.05, 0.34), 0.02, "upper", 8)                                        # its axle's arm in to the chassis
        keys = [{"z": 0.36, "xc": 0.0, "af": 0.74, "ar": 0.78, "b": 0.20, "n": 2.2, "yc": yt},
                {"z": 0.50, "xc": 0.02, "af": 0.92, "ar": 0.90, "b": 0.29, "n": 2.5, "yc": yt},
                {"z": 0.68, "xc": 0.04, "af": 0.84, "ar": 0.90, "b": 0.29, "n": 2.6, "yc": yt}]
        v.cast(keys, "upper", sides=v.n(40, 14), top=0.05, steps=2, between=2)
        zt = 0.73
        v.prism([(0.02 + 0.23 * math.cos(a), yt + 0.24 * math.sin(a)) for a in [2 * math.pi * k / 20 for k in range(20)]], zt - 0.005, zt + 0.003, "black")  # the well where he sits
        v.tube([Vector((0.02 + 0.24 * math.cos(a), yt + 0.25 * math.sin(a), zt + 0.01)) for a in [2 * math.pi * k / 20 for k in range(20)]], 0.014, "leather", 6, closed=True)
        v.hull([(-0.14, yt + 0.20, zt), (-0.20, yt + 0.20, zt), (-0.24, yt + 0.18, zt + 0.22), (-0.30, yt + 0.18, zt + 0.22),
                (-0.14, yt - 0.20, zt), (-0.20, yt - 0.20, zt), (-0.24, yt - 0.18, zt + 0.22), (-0.30, yt - 0.18, zt + 0.22)], "leather", 0.03, both=False)   # his backrest
        sc = Vector((-0.62, yt, zt + 0.07)) if r75k else Vector((-0.64, yt, zt + 0.10))
        tn = Vector((-0.30, 0.0, 0.95)).normalized() if r75k else Vector((-0.50, 0.0, 0.87)).normalized()   # (the M-72's stands up more, on her tail)
        v.tyre(sc, tn, R, R - W * 0.95, W, lugs=v.n(18, 8))                                                             # the spare, on the tail
        v.lathe(sc, tn, [(W * 0.4, 0.0), (W * 0.4, R - W), (-W * 0.4, R - W), (-W * 0.4, 0.0)], "upper", v.n(20, 8))
        v.use("sidecar")
        v.mudguard((xsw, R), R + 0.05, 15.0, 165.0, ys - 0.07, ys + 0.07, paint="upper", step=12)                      # its wheel's guard
        v.headlamp((xsw + 0.36, ys + 0.02, 0.56), X, 0.03, stalk=0.02)
        if fine:
            v.tube([(0.62, yt + 0.22, zt + 0.02), (0.66, yt + 0.22, zt + 0.10), (0.66, yt - 0.22, zt + 0.10), (0.62, yt - 0.22, zt + 0.02)], 0.01, "steel", 5)   # the grab rail
        v.post_gun("mg34" if r75k else "dt", (0.42, yt, zt + 0.04), 0.20, arc=(-80.0, 80.0), lay=(-10.0, 50.0), parent="sidecar")
        v.use("sidecar")
        v.socket("gunner", (-0.10, yt, 0.50), X, part="sidecar",
                 note="the gunner, seated low in the sidecar: the top of his cushion, 0.12 over the tub's floor; his legs forward inside the tub (0.9 of room ahead), his hands on grip.gunner.l and grip.gunner.r")
        v.slab((-0.24, yt - 0.20, 0.44), (0.06, yt + 0.20, 0.50), "leather", 0.02)                                     # his cushion, on the tub's floor
        v.socket("hit.right", (0.0, yt - 0.30, 0.55), -Y, part="sidecar")
        v.socket("fire.2", (0.0, yt, 0.75), Z, part="sidecar", note="out of the sidecar")
        v.spoke_wheel("wheel_side", "vehicle", (xsw, ys, R), R, W)
        v.use("hull")

    # ---- the men, the marks, what happens to her -----------------------------------------------------------------
    v.socket("driver", (-0.24, 0.0, 0.86), X, part="hull", note="the driver astride the saddle (a pose of its own, not the seated man's): the top of the saddle under him, his boots on foot.l and foot.r, his hands on grip.driver.l and grip.driver.r")
    v.socket("grip.driver.l", grips[0], Z, part="steer", note="the driver's left hand on the bar's grip")
    v.socket("grip.driver.r", grips[1], Z, part="steer", note="the driver's right hand on the bar's grip")
    for letter, s in (("l", LEFT), ("r", RIGHT)):
        v.socket(f"foot.{letter}", (0.04, s * 0.25, 0.32), X, part="hull", note="the driver's boot on the footrest")
    v.socket("rider.1", (-0.64, 0.0, 0.86), X, part="hull", note="the man on the pillion, astride: his hands on the grab handle ahead of him, his boots on " + ("the panniers' rests" if r75k else "his footrests (-0.52, 0.16, 0.36)"))
    v.socket("hit.front", (0.70, 0.0, 0.70), X, part="hull")
    v.socket("hit.left", (0.0, 0.25, 0.50), Y, part="hull")
    v.socket("hit.rear", (-0.92, 0.0, 0.60), -X, part="hull")
    if not sidecar:
        v.socket("hit.right", (0.0, -0.25, 0.50), -Y, part="hull")
    v.socket("dust.left", (xr - 0.35, 0.0, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", ((xsw if sidecar else xr) - 0.35, ys if sidecar else 0.0, 0.05), (-1, 0, 0.3), part="vehicle")
    if r75k:
        v.mark(tag, (-0.74, 0.276, 0.56), Y, 0.06, part="hull")
    v.bike_clips("With the sidecar on she stands on her three wheels and never leans; with the `sidecar` swap off she is a solo machine: "
                 "lean her into a turn by rolling her root, and stand her on her side stand when she stops (lean her 8 degrees to her left).")
    v.gun_controls("The gun is on the sidecar's nose; the gunner sits in the sidecar behind it and turns his shoulders with it.")
    gun = "MG 34" if r75k else "DT"
    v.worn(**v.worn_kw)
    swaps = {"how": ("paint: put the chosen coat's colours in place of the paints of the same names. " if r75k else "") +
                    "sidecar: off scales the sidecar's parts to nothing (a part scaled to nothing is not drawn): a solo machine, her driver and the man on her pillion."}
    if r75k:
        swaps["paint"] = coats(v, paint or "grey")
    if sidecar:
        swaps["sidecar"] = {"default": "on", "choices": {"on": {"what": f"the sidecar and its {gun} (the motorcycle troops, the scouts)", "parts": ["sidecar", "wheel_side", "mg_mount", "mg_cradle"]},
                                                         "off": {"what": "a solo machine: a despatch rider", "parts": []}}}
    v.m.info.setdefault("file", {})["swaps"] = swaps
    return v
