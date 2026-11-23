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
from motorpool import LEFT, RIGHT, X, Y, Z, desert_swap
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


def v_twin(v, x, z, lean=22.5, core=0.05, barrel=0.15, fins=8):
    """A V-twin's two cylinders, one ahead of the other and leaning apart (the Harley's 45 degrees between them):
    finned barrels and a side-valve's flat finned heads, the valve chest along her right."""
    for sgn in (1.0, -1.0):
        th = math.radians(lean) * sgn
        d = Vector((math.sin(th), 0.0, math.cos(th)))
        a = Vector((x + 0.035 * sgn, 0.0, z))
        L = barrel
        prof = [(0.0, 0.0), (0.0, core + 0.006)]
        for k in range(fins if v.fine else 1):
            u0, u1 = L * k / fins, L * (k + 0.5) / fins
            prof += [(u0, core + 0.03), (u1, core + 0.03), (u1, core + 0.006), (L * (k + 1) / fins, core + 0.006)] if v.fine else []
        prof += [(L, core + 0.026), (L, 0.0)]
        v.lathe(a, d, prof[::-1], "steel", v.n(18, 8))                                                 # the finned barrel
        hd = a + d * L
        hp = [(0.0, 0.0), (0.0, 0.082)]
        for k in range(3 if v.fine else 1):
            hp += [(0.008 + 0.013 * k, 0.082), (0.008 + 0.013 * k, 0.09), (0.013 + 0.013 * k, 0.09), (0.013 + 0.013 * k, 0.082)] if v.fine else []
        hp += [(0.048, 0.082), (0.048, 0.03), (0.054, 0.0)]
        v.lathe(hd, d, hp[::-1], "steel", v.n(18, 8))                                                  # the flat head
        if v.fine:
            v.bolts(hd + d * 0.05, d, 0.055, 6, r=0.007, paint="steel")
            v.rod(hd + d * 0.03 + Vector((0.0, -0.06, 0.0)), hd + d * 0.03 + Vector((0.0, -0.10, 0.0)), 0.012, "steel", 6)   # the plug
    v.slab((x - 0.14, -0.05, z - 0.03), (x + 0.14, -0.11, z + 0.07), "steel", 0.015)                   # the valve chest on her right
    if v.fine:
        for k in range(4):
            xx = x - 0.10 + 0.066 * k
            v.rod((xx, -0.085, z + 0.07), (xx, -0.085, z + 0.13), 0.013, "steel", 6)                    # the valve spring covers


# =====================================================================================================================
def wla(fine=True, tag="USA", paint=None):
    """The Harley-Davidson WLA (1940-45), the American army's solo machine: 2.24 m long, 0.98 m across her bars,
    1.04 m high. A side-valve V-twin, a springer front fork, a rigid back end under a sprung saddle, the big oil-bath
    air cleaner on her right, footboards and crash bars, a luggage rack with leather saddlebags, the Thompson's
    scabbard down her right fork and an ammunition box on her left. Olive drab, a white star on her tank."""
    v = BikeWorks("WLA", "Harley-Davidson WLA", "bike", tag, fine, paint=paint)
    v.dice = 0.2
    v.worn_kw = dict(LIGHT_WORN, ground="field", foot=0.30, top=0.62, shade_knee=0.3, shade_reach=0.35)
    v.root(hull_pivot=(0.0, 0.0, 0.40), rock=(-4.0, 4.0))
    R, W = 0.33, 0.10                                    # 4.00-18
    xf, xr = 0.73, -0.73

    # ---- the back wheel; the frame; the engine, the box, the chains ------------------------------------------------
    v.spoke_wheel("wheel_back", "vehicle", (xr, 0.0, R), R, W)
    v.use("hull")
    v.tube([(0.47, 0.0, 0.92), (0.40, 0.0, 0.70), (0.30, 0.0, 0.26)], 0.02, "upper", 8)                    # the down tube
    for s in (LEFT, RIGHT):
        v.tube([(0.30, 0.0, 0.26), (0.26, s * 0.07, 0.18), (-0.24, s * 0.07, 0.18), (xr, s * 0.08, R - 0.01)], 0.018, "upper", 8)   # the cradle and the chain stays
        v.tube([(-0.26, s * 0.03, 0.70), (-0.40, s * 0.07, 0.62), (xr, s * 0.08, R + 0.02)], 0.016, "upper", 8)                       # the seat stays
        v.rod((xr, s * 0.08, R), (xr, s * 0.065, R), 0.02, "steel", 8)
    v.tube([(0.50, 0.0, 0.92), (0.10, 0.0, 0.80), (-0.26, 0.0, 0.72)], 0.02, "upper", 8)                   # the backbone, under the tank
    v.tube([(-0.26, 0.0, 0.72), (-0.22, 0.0, 0.40), (-0.24, 0.0, 0.18)], 0.02, "upper", 8)                 # the seat post tube
    v.lathe((0.49, 0.0, 0.84), Vector((-0.25, 0.0, 0.55)), [(0.14, 0.0), (0.14, 0.032), (0.0, 0.032), (0.0, 0.0)], "upper", 10)   # the steering head
    v.hull([(0.16, 0.08, 0.28), (0.16, 0.08, 0.40), (0.04, 0.09, 0.20), (0.04, 0.09, 0.44), (-0.12, 0.08, 0.26), (-0.12, 0.08, 0.40)], "upper", 0.02)   # the crankcase
    v_twin(v, 0.02, 0.42)
    v.hull([(-0.12, 0.08, 0.28), (-0.12, 0.08, 0.44), (-0.22, 0.08, 0.28), (-0.22, 0.08, 0.44)], "upper", 0.02)           # the gearbox
    v.hull([(0.10, 0.10, 0.26), (0.10, 0.10, 0.36), (-0.04, 0.13, 0.22), (-0.04, 0.13, 0.40), (-0.30, 0.13, 0.26), (-0.30, 0.13, 0.42),
            (0.10, 0.15, 0.27), (-0.30, 0.15, 0.28), (-0.30, 0.15, 0.40)], "upper", 0.015, both=False)                    # the primary chain's case, on her left
    v.tube([(-0.22, 0.12, 0.30), (xr, 0.12, R + 0.07), (xr, 0.12, R - 0.07), (-0.22, 0.12, 0.26)], 0.012, "shade", 5, closed=True)   # the drive chain
    v.slab((-0.12, -0.09, 0.14), (0.34, 0.09, 0.16), "steel", 0.006)                                                      # the skid plate
    v.slab((-0.02, -0.07, 0.48), (0.06, -0.12, 0.56), "steel", 0.01)                                                       # the carburettor, between the cylinders
    v.lathe((0.02, -0.12, 0.53), -Y, [(0.08, 0.0), (0.08, 0.105), (0.07, 0.112), (0.0, 0.112), (0.0, 0.0)], "upper", v.n(24, 10))   # the oil-bath air cleaner
    if fine:
        v.bolts((0.02, -0.20, 0.53), -Y, 0.0, 1, r=0.018, paint="steel")
        v.rod((-0.20, -0.12, 0.40), (-0.14, -0.20, 0.22), 0.012, "steel", 6)                                              # the kick-starter
        v.rod((-0.14, -0.20, 0.22), (-0.06, -0.21, 0.22), 0.014, "rubber", 6)
    # the exhaust: both cylinders down her right to one long silencer
    v.tube([(0.14, -0.04, 0.50), (0.24, -0.10, 0.36), (0.18, -0.14, 0.20), (-0.20, -0.15, 0.20)], 0.018, "rust", 6)
    v.tube([(-0.09, -0.04, 0.50), (-0.05, -0.12, 0.36), (-0.02, -0.15, 0.22)], 0.016, "rust", 6)
    v.rod((-0.20, -0.15, 0.20), (-0.78, -0.16, 0.30), 0.042, "rust", v.n(14, 6))
    v.rod((-0.78, -0.16, 0.30), (-0.86, -0.16, 0.315), 0.014, "rust", 6)
    v.socket("exhaust.1", (-0.87, -0.16, 0.315), (-1, 0, 0.1))
    # the tank, its dash, the shift lever; the saddle on its post spring; the guard, the rack, the saddlebags
    v.hull([(0.44, 0.07, 0.84), (0.44, 0.07, 0.92), (0.36, 0.125, 0.78), (0.36, 0.115, 0.96), (0.00, 0.125, 0.74), (0.00, 0.105, 0.90),
            (-0.10, 0.07, 0.76), (-0.10, 0.06, 0.84)], "upper", 0.03)                                                     # the twin tank (fuel left, oil right)
    v.hull([(0.30, 0.05, 0.95), (0.30, 0.05, 0.965), (0.06, 0.05, 0.91), (0.06, 0.05, 0.93)], "upper", 0.01)              # the dash along its top
    if fine:
        v.lathe((0.20, 0.0, 0.955), Vector((0.0, 0.0, 1.0)), [(0.012, 0.0), (0.012, 0.04), (0.0, 0.04), (0.0, 0.0)], "black", 12)   # the speedometer
        for s in (LEFT, RIGHT):
            v.lathe((0.38, s * 0.07, 0.94), Z, [(0.02, 0.0), (0.02, 0.025), (0.0, 0.025), (0.0, 0.0)], "steel", 8)        # the caps
    v.rod((0.14, 0.125, 0.80), (0.20, 0.17, 1.00), 0.009, "steel", 6)                                                     # the shift lever
    v.ball((0.20, 0.17, 1.00), 0.018, "black", 8, 6)
    v.hull([(-0.16, 0.06, 0.78), (-0.16, 0.06, 0.82), (-0.42, 0.15, 0.80), (-0.42, 0.15, 0.86), (-0.36, 0.0, 0.88)], "leather", 0.03)   # the saddle
    v.rod((-0.20, 0.0, 0.78), (-0.24, 0.0, 0.60), 0.016, "steel", 8, rings=0.022 if fine else None)                       # its post spring
    for s in (LEFT, RIGHT):
        v.rod((-0.40, s * 0.10, 0.78), (-0.46, s * 0.08, 0.66), 0.012, "steel", 6)
    v.mudguard_y((xr, R), R + 0.045, 0.0, 165.0, 0.075, valance=0.04)
    for s in (LEFT, RIGHT):                                                                                                # the luggage rack
        v.tube([(-0.50, s * 0.13, 0.80), (-1.00, s * 0.13, 0.80), (-1.00, s * 0.13, 0.66)], 0.01, "upper", 6)
        v.rod((-0.62, s * 0.13, 0.80), (xr, s * 0.08, R + 0.05), 0.01, "upper", 6)
    if fine:
        for k in range(4):
            xx = -0.56 - 0.13 * k
            v.rod((xx, -0.13, 0.80), (xx, 0.13, 0.80), 0.008, "upper", 5)
    for s in (LEFT, RIGHT):                                                                                                # the saddlebags
        v.bin((-0.92, s * 0.15, 0.46), (-0.56, s * 0.28, 0.78), paint="leather", bevel=0.02, latches=2, lid_side=Y * s)
    for s in (LEFT, RIGHT):                                                                                                # the footboards and crash bars
        v.slab((0.02, s * 0.17, 0.22), (0.34, s * 0.32, 0.24), "rubber", 0.01)
        v.tube([(0.38, s * 0.03, 0.66), (0.44, s * 0.24, 0.56), (0.42, s * 0.31, 0.36), (0.34, s * 0.30, 0.22), (0.26, s * 0.09, 0.18)], 0.014, "upper", 6)
    v.socket("engine", (0.02, 0.0, 0.42), Z, part="hull", note="the engine: smoke and the first fire")
    v.socket("fire.1", (0.20, 0.0, 0.86), Z, part="hull", note="the tank")

    # ---- the springer fork, the bars, the lamps, the scabbard, the front wheel (the game steers them) -------------
    d = Vector((0.25, 0.0, -0.55)).normalized()
    v.part("steer", "hull", (0.49, 0.0, 0.84), axis=tuple(d), limits=[-35.0, 35.0], kind="turn", note="the springer fork on the steering head: + steers to the right")
    for s in (LEFT, RIGHT):
        v.rod((xf - 0.02, s * 0.075, R + 0.02), (0.47, s * 0.075, 0.90), 0.016, "upper", 8)                                 # the rigid back legs
        v.rod((xf + 0.07, s * 0.075, R + 0.06), (0.57, s * 0.075, 0.90), 0.013, "upper", 8)                                 # the sprung front legs
        v.rod((xf + 0.07, s * 0.075, R + 0.06), (xf, s * 0.075, R), 0.014, "steel", 6)                                      # the rocker to the axle
        v.rod((0.56, s * 0.075, 0.86), (0.55, s * 0.075, 0.96), 0.022, "steel", 8, rings=0.03 if fine else None)            # the springs
    v.slab((0.44, -0.10, 0.90), (0.60, 0.10, 0.93), "upper", 0.01)                                                           # the yokes
    v.slab((0.48, -0.10, 0.96), (0.60, 0.10, 0.99), "upper", 0.01)
    v.tube([(0.50, 0.0, 1.00), (0.46, 0.14, 1.02), (0.36, 0.34, 1.04), (0.30, 0.42, 1.05)], 0.012, "steel", 6)            # the bars
    v.tube([(0.50, 0.0, 1.00), (0.46, -0.14, 1.02), (0.36, -0.34, 1.04), (0.30, -0.42, 1.05)], 0.012, "steel", 6)
    for s in (LEFT, RIGHT):
        v.rod((0.31, s * 0.40, 1.05), (0.27, s * 0.50, 1.055), 0.017, "rubber", 8)
    v.headlamp((0.66, 0.0, 0.94), X, 0.085, stalk=0.03, cover=True)
    v.headlamp((0.70, 0.13, 0.80), X, 0.035, stalk=0.02, cover=True)                                                      # the blackout driving lamp
    v.mudguard_y((xf, R), R + 0.05, 10.0, 150.0, 0.065)                                                                    # (no skirts: the army cut them off)
    v.hull([(0.42, -0.17, 1.00), (0.42, -0.23, 1.00), (0.38, -0.17, 0.96), (0.38, -0.23, 0.96),
            (0.94, -0.16, 0.50), (0.94, -0.20, 0.50), (0.90, -0.16, 0.47), (0.90, -0.20, 0.47)], "leather", 0.015, both=False)   # the Thompson's scabbard
    v.hull([(0.42, -0.18, 1.00), (0.42, -0.22, 1.00), (0.34, -0.18, 1.10), (0.34, -0.22, 1.10), (0.31, -0.18, 1.07), (0.31, -0.22, 1.07)], "wood", 0.01, both=False)   # its butt
    v.rod((0.52, -0.08, 0.86), (0.55, -0.17, 0.86), 0.01, "steel", 5)
    v.rod((0.75, -0.08, 0.62), (0.79, -0.17, 0.64), 0.01, "steel", 5)
    v.bin((0.60, 0.10, 0.70), (0.72, 0.18, 0.86), paint="upper", bevel=0.01, latches=1, lid_side=Z)                         # the ammunition box
    v.spoke_wheel("wheel_front", "steer", (xf, 0.0, R), R, W)
    v.use("hull")
    grips = [Vector((0.29, s * 0.45, 1.053)) for s in (LEFT, RIGHT)]

    # ---- the man, the marks, what happens to her -------------------------------------------------------------------
    v.socket("driver", (-0.30, 0.0, 0.86), X, part="hull", note="the rider astride the saddle (a pose of its own, not the seated man's): the top of the saddle under him, his boots on foot.l and foot.r, his hands on grip.driver.l and grip.driver.r")
    v.socket("grip.driver.l", grips[0], Z, part="steer", note="the rider's left hand on the bar's grip")
    v.socket("grip.driver.r", grips[1], Z, part="steer", note="the rider's right hand on the bar's grip")
    for letter, s in (("l", LEFT), ("r", RIGHT)):
        v.socket(f"foot.{letter}", (0.16, s * 0.25, 0.24), X, part="hull", note="the rider's boot on the footboard")
    v.socket("weapon", (0.36, -0.20, 1.06), (-0.4, 0, 0.9), part="steer", note="the Thompson's butt out of its scabbard: where his hand goes to draw it")
    v.socket("hit.front", (0.70, 0.0, 0.70), X, part="hull")
    v.socket("hit.left", (0.0, 0.25, 0.50), Y, part="hull")
    v.socket("hit.right", (0.0, -0.25, 0.50), -Y, part="hull")
    v.socket("hit.rear", (-1.0, 0.0, 0.60), -X, part="hull")
    v.socket("dust.left", (xr - 0.35, 0.05, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (xr - 0.35, -0.05, 0.05), (-1, 0, 0.3), part="vehicle")
    for s in (LEFT, RIGHT):
        v.mark(tag, (0.18, s * 0.124, 0.85), Y * s, 0.055, part="hull")
    v.bike_clips("A solo machine: lean her into a turn by rolling her root, and stand her on her side stand when she stops (lean her 8 degrees to her left).")
    v.worn(**v.worn_kw)
    v.m.info.setdefault("file", {})["swaps"] = {}
    return v


def barrel(v, a, d, core=0.05, L=0.17, fins=8, head="sv", side=None):
    """One finned cylinder from `a` along `d`, and its head: "sv" a side-valve's flat finned head, "ohv" a head with
    its rocker box. side: the way a side-valve's valve chest stands out (her right: -Y)."""
    a, d = Vector(a), Vector(d).normalized()
    prof = [(0.0, 0.0), (0.0, core + 0.006)]
    for k in range(fins if v.fine else 1):
        u0, u1 = L * k / fins, L * (k + 0.5) / fins
        prof += [(u0, core + 0.03), (u1, core + 0.03), (u1, core + 0.006), (L * (k + 1) / fins, core + 0.006)] if v.fine else []
    prof += [(L, core + 0.026), (L, 0.0)]
    v.lathe(a, d, prof[::-1], "steel", v.n(18, 8))
    hd = a + d * L
    if head == "sv":
        hp = [(0.0, 0.0), (0.0, core + 0.032)]
        for k in range(3 if v.fine else 1):
            hp += [(0.008 + 0.013 * k, core + 0.032), (0.008 + 0.013 * k, core + 0.042), (0.013 + 0.013 * k, core + 0.042), (0.013 + 0.013 * k, core + 0.032)] if v.fine else []
        hp += [(0.05, core + 0.032), (0.05, 0.03), (0.056, 0.0)]
        v.lathe(hd, d, hp[::-1], "steel", v.n(18, 8))
        if side is not None:
            sd = Vector(side).normalized()
            c = a + d * (L * 0.45) + sd * (core + 0.03)
            u = d.cross(sd).normalized()
            v.hull([c + d * L * 0.5 + u * 0.035, c - d * L * 0.5 + u * 0.035, c + d * L * 0.5 - u * 0.035, c - d * L * 0.5 - u * 0.035,
                    c + sd * 0.04 + d * L * 0.4 + u * 0.03, c + sd * 0.04 - d * L * 0.4 + u * 0.03, c + sd * 0.04 + d * L * 0.4 - u * 0.03,
                    c + sd * 0.04 - d * L * 0.4 - u * 0.03], "steel", 0.01, both=False)                    # the valve chest
    else:
        v.lathe(hd, d, [(0.07, 0.0), (0.07, core + 0.02), (0.04, core + 0.03), (0.0, core + 0.03), (0.0, 0.0)], "steel", v.n(18, 8))
        u = Vector((0.0, 1.0, 0.0)) if abs(d.y) < 0.9 else Vector((1.0, 0.0, 0.0))
        w = d.cross(u).normalized()
        c = hd + d * 0.07
        v.hull([c + u * 0.07 + w * 0.06, c - u * 0.07 + w * 0.06, c + u * 0.07 - w * 0.06, c - u * 0.07 - w * 0.06,
                c + d * 0.05 + u * 0.06 + w * 0.05, c + d * 0.05 - u * 0.06 + w * 0.05, c + d * 0.05 + u * 0.06 - w * 0.05,
                c + d * 0.05 - u * 0.06 - w * 0.05], "steel", 0.012, both=False)                              # the rocker box
    return hd


# =====================================================================================================================
def m20(fine=True, tag="UK", paint=None):
    """The BSA M20 (1937-45), the British army's solo machine: 2.13 m long, 0.74 m across her bars, 0.97 m high. A
    tall side-valve single, a girder fork with one spring, a rigid back end, the air filter on her tank, a carrier
    and pannier bags. Khaki green; Light Stone in the desert (a paint swap)."""
    return _single("m20", fine, tag, paint, sidecar=False)


def alce(fine=True, tag="ITA", paint=None):
    """The Moto Guzzi Alce (1939-45), Italy's solo machine for two: 2.22 m long, 0.82 m across her bars. A horizontal
    single with its flywheel outside on her left, a girder fork, a sprung back end (the springs in boxes under her),
    the rider's saddle and a second saddle behind with its own bars, for a man who could fire from it. Italian khaki."""
    return _single("alce", fine, tag, paint, sidecar=False)


def motorcycle(fine=True, tag="_", paint=None):
    """The plain motorcycle every army without a model of its own rides (France, Poland, Japan before her Rikuo, the
    small armies): an upright single of the 1930s, a girder fork, a rigid back end, and a plain sidecar the swap
    `sidecar` puts on. Each nation's own coat."""
    return _single("plain", fine, tag, paint, sidecar=True)


def _single(kind, fine, tag, paint, sidecar):
    names = {"m20": ("M20", "BSA M20"), "alce": ("Alce", "Moto Guzzi Alce"), "plain": ("Motorcycle", "Motorcycle (any army)")}
    v = BikeWorks(*names[kind], "bike", tag, fine, paint=paint)
    v.dice = 0.2
    v.worn_kw = dict(LIGHT_WORN, ground="field", foot=0.30, top=0.62, shade_knee=0.3, shade_reach=0.35)
    v.root(hull_pivot=(0.0, 0.0, 0.40), rock=(-4.0, 4.0))
    guzzi = kind == "alce"
    R, W = {"m20": (0.324, 0.085), "alce": (0.34, 0.09), "plain": (0.33, 0.09)}[kind]
    xf, xr = {"m20": (0.685, -0.685), "alce": (0.74, -0.74), "plain": (0.70, -0.70)}[kind]
    ztk = 0.92 if not guzzi else 0.94                    # the tank's top

    # ---- the back wheel; the frame; the engine, the box, the chains ------------------------------------------------
    v.spoke_wheel("wheel_back", "vehicle", (xr, 0.0, R), R, W, spokes=36)
    v.use("hull")
    v.tube([(0.45, 0.0, 0.88), (0.36, 0.0, 0.62), (0.22, 0.0, 0.24)], 0.02, "upper", 8)                   # the down tube
    v.tube([(0.47, 0.0, 0.90), (0.10, 0.0, 0.82), (-0.24, 0.0, 0.76)], 0.02, "upper", 8)                  # the top tube
    v.tube([(-0.24, 0.0, 0.76), (-0.22, 0.0, 0.40), (-0.24, 0.0, 0.20)], 0.02, "upper", 8)                # the seat tube
    for s in (LEFT, RIGHT):
        v.tube([(0.22, 0.0, 0.24), (0.18, s * 0.07, 0.17), (-0.24, s * 0.07, 0.19)], 0.017, "upper", 8)    # the cradle
        if guzzi:                                                                                          # the swinging arm, its springs in the boxes under her
            v.tube([(-0.26, s * 0.08, 0.30), (xr, s * 0.08, R)], 0.018, "upper", 8)
            v.tube([(-0.30, s * 0.08, 0.62), (xr, s * 0.08, R + 0.04)], 0.014, "upper", 8)
            v.slab((-0.62, s * 0.06, 0.15), (-0.20, s * 0.12, 0.24), "upper", 0.012)
        else:
            v.tube([(-0.24, s * 0.07, 0.19), (xr, s * 0.08, R - 0.01)], 0.016, "upper", 8)                   # the chain stays
            v.tube([(-0.24, s * 0.03, 0.72), (-0.40, s * 0.07, 0.62), (xr, s * 0.08, R + 0.02)], 0.016, "upper", 8)   # the seat stays
        v.rod((xr, s * 0.08, R), (xr, s * 0.065, R), 0.02, "steel", 8)
    v.lathe((0.47, 0.0, 0.82), Vector((-0.24, 0.0, 0.55)), [(0.13, 0.0), (0.13, 0.032), (0.0, 0.032), (0.0, 0.0)], "upper", 10)   # the steering head
    if guzzi:                                                                                              # the crankcase, the barrel forward, the flywheel out on her left
        v.hull([(0.10, 0.08, 0.24), (0.10, 0.08, 0.44), (-0.06, 0.09, 0.20), (-0.06, 0.09, 0.48), (-0.22, 0.08, 0.26), (-0.22, 0.08, 0.44)], "upper", 0.02)
        barrel(v, (0.10, 0.0, 0.36), X, core=0.055, L=0.18, fins=9, head="ohv")
        v.lathe((-0.02, 0.10, 0.34), Y, [(0.03, 0.0), (0.03, 0.15), (0.02, 0.16), (0.0, 0.16), (0.0, 0.0)], "steel", v.n(32, 12))     # the outside flywheel
        v.rod((-0.02, 0.08, 0.34), (-0.02, 0.15, 0.34), 0.03, "steel", 8)
        crank = Vector((-0.02, 0.0, 0.34))
    else:
        v.hull([(0.14, 0.08, 0.22), (0.14, 0.08, 0.38), (0.02, 0.09, 0.18), (0.02, 0.09, 0.42), (-0.10, 0.08, 0.22), (-0.10, 0.08, 0.38)], "upper", 0.02)
        barrel(v, (0.04, 0.0, 0.40), (0.06, 0.0, 1.0), core=0.06, L=0.20 if kind == "m20" else 0.17, fins=10,
               head="sv" if kind == "m20" else "ohv", side=-Y if kind == "m20" else None)
        if kind == "m20":
            v.lathe((-0.10, 0.0, 0.46), -X, [(0.09, 0.0), (0.09, 0.04), (0.0, 0.04), (0.0, 0.0)], "black", 10)  # the magneto, behind the barrel
        else:
            v.tube([(0.02, 0.07, 0.42), (0.04, 0.07, 0.60)], 0.012, "steel", 6)                             # the push-rod tube
        crank = Vector((0.02, 0.0, 0.30))
    v.hull([(-0.12, 0.08, 0.26), (-0.12, 0.08, 0.44), (-0.26, 0.08, 0.26), (-0.26, 0.08, 0.44)], "upper", 0.02)               # the gearbox
    v.hull([(crank.x + 0.08, 0.10, 0.24), (crank.x + 0.08, 0.10, 0.38), (crank.x, 0.13, 0.20), (crank.x, 0.13, 0.42), (-0.28, 0.13, 0.26), (-0.28, 0.13, 0.42),
            (crank.x + 0.08, 0.15, 0.26), (-0.28, 0.15, 0.28), (-0.28, 0.15, 0.40)], "upper", 0.015, both=False) if not guzzi else None   # the primary chain's case, on her left
    v.tube([(-0.22, -0.12, 0.30), (xr, -0.12, R + 0.07), (xr, -0.12, R - 0.07), (-0.22, -0.12, 0.26)], 0.012, "shade", 5, closed=True)   # the drive chain, on her right
    # the exhaust: from the head down her right to a long silencer
    if guzzi:
        v.tube([(0.34, -0.02, 0.30), (0.36, -0.10, 0.22), (0.20, -0.15, 0.18), (-0.20, -0.16, 0.19)], 0.02, "rust", 6)
    else:
        v.tube([(0.10, -0.06, 0.60), (0.22, -0.12, 0.50), (0.26, -0.14, 0.30), (0.14, -0.16, 0.20), (-0.20, -0.16, 0.20)], 0.02, "rust", 6)
    v.rod((-0.20, -0.16, 0.20), (-0.80, -0.17, 0.30), 0.04, "rust", v.n(14, 6))
    v.socket("exhaust.1", (-0.82, -0.17, 0.30), (-1, 0, 0.1))
    # the tank (and on the M20 its air filter); the saddle; the guard; the carrier and its bags
    v.hull([(0.44, 0.07, ztk - 0.10), (0.44, 0.07, ztk - 0.01), (0.36, 0.125, ztk - 0.15), (0.36, 0.115, ztk), (-0.04, 0.125, ztk - 0.18),
            (-0.04, 0.105, ztk - 0.03), (-0.12, 0.07, ztk - 0.16), (-0.12, 0.06, ztk - 0.08)], "upper", 0.03)
    for s in (LEFT, RIGHT):
        v.slab((-0.04, s * 0.112, ztk - 0.16), (0.12, s * 0.135, ztk - 0.07), "rubber", 0.01)               # the knee grips
    if kind == "m20":
        v.lathe((0.16, 0.0, ztk - 0.005), Z, [(0.10, 0.0), (0.10, 0.07), (0.09, 0.08), (0.0, 0.08), (0.0, 0.0)], "upper", v.n(20, 8))   # the air filter on her tank
        v.tube([(0.16, -0.075, ztk + 0.03), (0.22, -0.12, ztk - 0.08), (0.10, -0.10, 0.62)], 0.016, "rubber", 6)                         # its hose down to the carburettor
    elif fine:
        v.lathe((0.36, 0.0, ztk - 0.005), Z, [(0.02, 0.0), (0.02, 0.022), (0.0, 0.022), (0.0, 0.0)], "steel", 8)                        # the filler cap
    v.hull([(-0.14, 0.06, 0.78), (-0.14, 0.06, 0.82), (-0.38, 0.14, 0.80), (-0.38, 0.14, 0.86), (-0.32, 0.0, 0.88)], "leather", 0.03)   # the saddle
    for s in (LEFT, RIGHT):
        v.rod((-0.36, s * 0.08, 0.80), (-0.36, s * 0.08, 0.66), 0.016, "steel", 6, rings=0.022 if fine else None)
    v.mudguard_y((xr, R), R + 0.045, 0.0, 165.0, 0.065, valance=0.03)
    if guzzi:                                                                                              # the second saddle and its bars
        v.hull([(-0.52, 0.06, 0.80), (-0.52, 0.06, 0.84), (-0.74, 0.13, 0.82), (-0.74, 0.13, 0.88), (-0.68, 0.0, 0.90)], "leather", 0.03)
        v.rod((-0.62, 0.0, 0.80), (-0.62, 0.0, 0.64), 0.016, "steel", 6, rings=0.022 if fine else None)
        v.tube([(-0.44, -0.24, 0.98), (-0.42, -0.12, 0.96), (-0.40, 0.0, 0.92), (-0.42, 0.12, 0.96), (-0.44, 0.24, 0.98)], 0.011, "steel", 6)
        v.rod((-0.40, 0.0, 0.92), (-0.30, 0.0, 0.80), 0.014, "steel", 6)
        for s in (LEFT, RIGHT):
            v.rod((-0.45, s * 0.22, 0.98), (-0.47, s * 0.30, 0.985), 0.016, "rubber", 6)
            v.rod((-0.62, s * 0.10, 0.34), (-0.62, s * 0.22, 0.34), 0.016, "rubber", 8)                      # his footrests
            v.bin((-0.98, s * 0.14, 0.44), (-0.70, s * 0.25, 0.70), paint="leather", bevel=0.02, latches=1, lid_side=Z)   # the saddlebags
    else:
        for s in (LEFT, RIGHT):                                                                            # the carrier and its pannier frames
            v.tube([(-0.44, s * 0.11, 0.76), (-0.92, s * 0.11, 0.76), (-0.92, s * 0.11, 0.62)], 0.01, "upper", 6)
            v.rod((-0.60, s * 0.11, 0.76), (xr, s * 0.08, R + 0.05), 0.01, "upper", 6)
            if kind == "m20":
                v.tube([(-0.46, s * 0.12, 0.74), (-0.46, s * 0.26, 0.70), (-0.46, s * 0.26, 0.40), (-0.86, s * 0.26, 0.40), (-0.86, s * 0.26, 0.70),
                        (-0.86, s * 0.12, 0.74)], 0.008, "upper", 5)
                v.bin((-0.84, s * 0.15, 0.42), (-0.48, s * 0.25, 0.72), paint="canvas", bevel=0.025, latches=2, lid_side=Y * s)   # the pannier bags
        if fine:
            for k in range(3):
                xx = -0.56 - 0.12 * k
                v.rod((xx, -0.11, 0.76), (xx, 0.11, 0.76), 0.008, "upper", 5)
    for s in (LEFT, RIGHT):
        v.rod((0.06, s * 0.15, 0.30), (0.06, s * 0.28, 0.30), 0.016, "rubber", 8)                              # the footrests
    v.slab((-0.94 if not guzzi else -1.0, -0.10, 0.50), (-0.92 if not guzzi else -0.98, 0.10, 0.64), "upper", 0.005)   # the plate on her back guard
    v.lathe((-0.95 if not guzzi else -1.01, 0.0, 0.66), -X, [(0.025, 0.0), (0.025, 0.03), (0.0, 0.03), (0.0, 0.0)], "red", 8)
    v.socket("engine", (crank.x + 0.05, 0.0, 0.42), Z, part="hull", note="the engine: smoke and the first fire")
    v.socket("fire.1", (0.20, 0.0, ztk - 0.06), Z, part="hull", note="the tank")

    # ---- the girder fork, the bars, the lamp, the front wheel (the game steers them) -------------------------------
    d = Vector((0.24, 0.0, -0.55)).normalized()
    v.part("steer", "hull", (0.47, 0.0, 0.82), axis=tuple(d), limits=[-35.0, 35.0], kind="turn", note="the girder fork on the steering head: + steers to the right")
    for s in (LEFT, RIGHT):
        v.tube([(xf, s * 0.075, R), (0.62, s * 0.075, 0.62), (0.52, s * 0.075, 0.90)], 0.016, "upper", 6)      # the girder's back blade
        v.tube([(xf + 0.03, s * 0.075, R + 0.03), (0.66, s * 0.075, 0.64), (0.58, s * 0.075, 0.90)], 0.013, "upper", 6)   # and its front blade
        for z in (0.48, 0.66, 0.84):                                                                          # the girder's ties
            t = (z - R) / (0.90 - R)
            xa = xf + (0.52 - xf) * t
            v.rod((xa, s * 0.075, z), (xa + 0.06, s * 0.075, z + 0.01), 0.008, "upper", 4)
        v.rod((0.50, s * 0.075, 0.90), (0.60, s * 0.075, 0.91), 0.014, "steel", 6)                            # the top links
    v.rod((0.57, 0.0, 0.86), (0.56, 0.0, 0.98), 0.024, "steel", 8, rings=0.032 if fine else None)             # the spring
    v.slab((0.44, -0.10, 0.90), (0.56, 0.10, 0.93), "upper", 0.01)
    bw = 0.36 if kind == "m20" else 0.40
    v.tube([(0.50, 0.0, 0.96), (0.47, 0.12, 0.98), (0.38, bw - 0.06, 1.00), (0.34, bw, 1.00)], 0.011, "steel", 6)
    v.tube([(0.50, 0.0, 0.96), (0.47, -0.12, 0.98), (0.38, -(bw - 0.06), 1.00), (0.34, -bw, 1.00)], 0.011, "steel", 6)
    for s in (LEFT, RIGHT):
        v.rod((0.345, s * (bw - 0.02), 1.0), (0.32, s * (bw + 0.08), 1.003), 0.016, "rubber", 8)
    v.headlamp((0.64, 0.0, 0.86), X, 0.08, stalk=0.03, cover=True)
    v.mudguard_y((xf, R), R + 0.045, -20.0, 150.0, 0.06, valance=0.03)
    v.spoke_wheel("wheel_front", "steer", (xf, 0.0, R), R, W, spokes=36)
    v.use("hull")
    grips = [Vector((0.33, s * (bw + 0.04), 1.003)) for s in (LEFT, RIGHT)]

    # ---- the plain machine's sidecar: a sheet tub on a tube chassis, its wheel well ahead of hers -------------------
    ys, xsw, yt = -1.00, -0.30, -0.64
    if sidecar:
        v.part("sidecar", "hull", (0.0, yt, 0.40), kind="fixed", note="the sidecar: its tub and chassis (the swap `sidecar` takes it off)")
        v.rod((0.30, -0.08, 0.24), (0.28, yt + 0.10, 0.34), 0.02, "upper", 8)
        v.rod((-0.30, -0.08, 0.40), (-0.30, yt + 0.10, 0.36), 0.02, "upper", 8)
        v.rod((0.28, yt + 0.10, 0.34), (-0.70, yt + 0.10, 0.34), 0.02, "upper", 8)
        v.rod((0.28, yt + 0.10, 0.34), (xsw, ys + 0.10, R), 0.02, "upper", 8)
        v.rod((xsw, ys + 0.10, R), (xsw, yt - 0.05, 0.34), 0.02, "upper", 8)
        keys = [{"z": 0.38, "xc": 0.0, "af": 0.70, "ar": 0.70, "b": 0.20, "n": 2.0, "yc": yt},
                {"z": 0.52, "xc": 0.02, "af": 0.86, "ar": 0.80, "b": 0.28, "n": 2.4, "yc": yt},
                {"z": 0.68, "xc": 0.04, "af": 0.78, "ar": 0.80, "b": 0.28, "n": 2.5, "yc": yt}]
        v.cast(keys, "upper", sides=v.n(36, 12), top=0.05, steps=2, between=2)
        zt = 0.73
        v.prism([(0.0 + 0.24 * math.cos(a), yt + 0.22 * math.sin(a)) for a in [2 * math.pi * k / 20 for k in range(20)]], zt - 0.005, zt + 0.003, "black")
        v.tube([Vector((0.0 + 0.25 * math.cos(a), yt + 0.23 * math.sin(a), zt + 0.01)) for a in [2 * math.pi * k / 20 for k in range(20)]], 0.013, "leather", 6, closed=True)
        v.hull([(-0.16, yt + 0.18, zt), (-0.22, yt + 0.18, zt), (-0.26, yt + 0.16, zt + 0.20), (-0.32, yt + 0.16, zt + 0.20),
                (-0.16, yt - 0.18, zt), (-0.22, yt - 0.18, zt), (-0.26, yt - 0.16, zt + 0.20), (-0.32, yt - 0.16, zt + 0.20)], "leather", 0.03, both=False)
        v.slab((-0.26, yt - 0.18, 0.44), (0.04, yt + 0.18, 0.50), "leather", 0.02)
        v.use("sidecar")
        v.mudguard((xsw, R), R + 0.05, 15.0, 165.0, ys - 0.065, ys + 0.065, paint="upper", step=12)
        v.headlamp((0.62, yt, 0.62), X, 0.03, stalk=0.02)
        v.socket("rider.2", (-0.12, yt, 0.50), X, part="sidecar", note="the man in the sidecar, seated low: the top of his cushion, his legs forward inside the tub")
        v.socket("hit.right", (0.0, yt - 0.28, 0.55), -Y, part="sidecar")
        v.socket("fire.2", (0.0, yt, 0.75), Z, part="sidecar", note="out of the sidecar")
        v.spoke_wheel("wheel_side", "vehicle", (xsw, ys, R), R, W, spokes=36)
        v.use("hull")

    # ---- the men, the marks, what happens to her -------------------------------------------------------------------
    v.socket("driver", (-0.28, 0.0, 0.86), X, part="hull", note="the rider astride the saddle (a pose of its own, not the seated man's): the top of the saddle under him, his boots on foot.l and foot.r, his hands on grip.driver.l and grip.driver.r")
    v.socket("grip.driver.l", grips[0], Z, part="steer", note="the rider's left hand on the bar's grip")
    v.socket("grip.driver.r", grips[1], Z, part="steer", note="the rider's right hand on the bar's grip")
    for letter, s in (("l", LEFT), ("r", RIGHT)):
        v.socket(f"foot.{letter}", (0.06, s * 0.22, 0.31), X, part="hull", note="the rider's boot on the footrest")
    if guzzi:
        v.socket("rider.1", (-0.64, 0.0, 0.88), X, part="hull", note="the man on the second saddle, astride: his hands on its own bars (-0.45, 0.24, 0.98), his boots on his footrests; he may fire his rifle from there")
    v.socket("hit.front", (0.70, 0.0, 0.70), X, part="hull")
    v.socket("hit.left", (0.0, 0.25, 0.50), Y, part="hull")
    if not sidecar:
        v.socket("hit.right", (0.0, -0.25, 0.50), -Y, part="hull")
    v.socket("hit.rear", (-0.95, 0.0, 0.60), -X, part="hull")
    v.socket("dust.left", (xr - 0.35, 0.05, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", ((xsw if sidecar else xr) - 0.35, ys if sidecar else -0.05, 0.05), (-1, 0, 0.3), part="vehicle")
    if kind == "m20":
        for s in (LEFT, RIGHT):
            v.mark(tag, (0.16, s * 0.124, ztk - 0.09), Y * s, 0.05, part="hull")
    v.bike_clips(("With the sidecar on she stands on her three wheels and never leans; with the `sidecar` swap off she is a solo machine: " if sidecar else "A solo machine: ") +
                 "lean her into a turn by rolling her root, and stand her on her stand when she stops (lean her 8 degrees to her left).")
    v.worn(**v.worn_kw)
    swaps = {}
    if kind == "m20":
        swaps = {"how": "paint: put the chosen coat's colours in place of the paints of the same names.", "paint": desert_swap(tag)}
    if sidecar:
        swaps = {"how": "sidecar: on draws the sidecar's parts; off (the default) scales them to nothing (a part scaled to nothing is not drawn): a solo machine.",
                 "sidecar": {"default": "off", "choices": {"on": {"what": "a plain sidecar for a second man (the motorcycle troops, the scouts)", "parts": ["sidecar", "wheel_side"]},
                                                          "off": {"what": "a solo machine: a despatch rider", "parts": []}}}}
    v.m.info.setdefault("file", {})["swaps"] = swaps
    return v
