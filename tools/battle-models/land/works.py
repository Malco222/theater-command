"""The works: where a vehicle is put together, given its clips, and sent out.

    v = Works("PanzerIV", "Panzer IV", "tank", "GER", fine)     # motorpool + gear + fittings
    ... build ...
    v.tank_clips(...)                                           # the clips every tank has
    ob, arm = to_scene(v.m)                                      # the Blender mesh on its skeleton, for pictures and export

PARTS of a tracked vehicle (names the game can rely on):
    vehicle (root, on the ground under her middle)
      hull (rocks on her springs) > turret (trains) > gun (lifts) > barrel (runs back)
                                  > hatch_commander, hatch_loader ...      hull > hatch_driver, hatch_radio ...
      wheel_l1.. wheel_r1.., sprocket_l/r, idler_l/r, roller_l1.. (kind spin; radius in `running`)
      track_l, track_r > track_l.001 ... (the links)
"""

import math

import bpy
from mathutils import Quaternion, Vector

import rigid
from fittings import Fittings
from gear import Gear
from motorpool import LEFT, RIGHT, SIDES, X, Y, Z, Shop
from rigid import hex_rgb


class Works(Shop, Gear, Fittings):
    def root(self, hull_pivot=(0.0, 0.0, 0.9), rock=(-4.0, 4.0)):
        self.part("vehicle", None, (0, 0, 0), kind="root", note="on the ground under her middle; the game moves and turns this")
        self.part("hull", "vehicle", hull_pivot, axis=(0, -1, 0), limits=list(rock), kind="turn", note="rocks on her springs: + nose up (clips also roll and bob her)")
        return self

    # ---- clips every tracked or wheeled vehicle has -------------------------------------------------
    def spring(self, t, kick, freq=2.4, damp=3.2):
        """How a sprung hull answers a knock at t = 0: a rocking that dies away."""
        return kick * math.exp(-damp * t) * math.sin(2 * math.pi * freq * t)

    def hull_keys(self, long, pitch=None, roll=None, bob=None, fps=30):
        """Keys for the hull from functions of time (degrees, degrees, metres): one `rot` and one `move` track."""
        hp = self.m.parts[self.m.part_at["hull"]]
        rot_keys, mv = [], []
        n = max(2, int(round(long * fps)))
        for i in range(n + 1):
            t = long * i / n
            q = Quaternion(hp.axis, math.radians(pitch(t) if pitch else 0.0)) @ Quaternion(X, math.radians(roll(t) if roll else 0.0))
            rot_keys.append([t, q.w, q.x, q.y, q.z])
            mv.append([t, 0.0, 0.0, bob(t) if bob else 0.0])
        return {"part": "hull", "rot": rot_keys, "move": mv}

    def tank_clips(self, recoil=0.35, kick=0.9, gun_low=-8.0, hatches=(), traverse=14.0, idle_cue="tank_idle_loop", drive_cue="tank_loop",
                   gun_cue="tank_gun_01", blow=(-0.9, 2.9, 0.42, 172.0), has_turret=True, mg="coax", exhausts=None, fire_sockets=None):
        """The clips a tank has. Nothing here moves her over the ground or trains her turret: the game does that (the
        root, the `turret` and `gun` parts, the wheels and the track clips). These are what her own body does.

        recoil: metres the barrel runs back; kick: degrees the hull rocks to the shot; gun_low: where the gun droops to
        when she is knocked out; hatches: lids that are thrown open when she is knocked out, the commander's first;
        blow: where the turret comes to rest when her ammunition goes (back, to the left, up, turned over degrees)."""
        m = self.m
        ex = exhausts or [s["name"] for s in m.sockets if s["name"].startswith("exhaust")]
        fires = fire_sockets or [s["name"] for s in m.sockets if s["name"].startswith("fire.")]
        two_pi = 2 * math.pi
        ev = lambda t, fx, socket=None, cue=None: {"t": round(t, 3), "fx": fx, "socket": socket, "cue": cue}

        # idle: the engine ticking over shakes her a very little
        m.clips["idle"] = {"long": 2.0, "loop": True, "about": "engine running, standing still",
                           "tracks": [self.hull_keys(2.0, roll=lambda t: 0.05 * math.sin(two_pi * 5 * t / 2.0 * 2), bob=lambda t: 0.0012 * math.sin(two_pi * 7 * t / 2.0 * 2))],
                           "events": [ev(0.0, "engine_idle", "engine", idle_cue)] + [ev(0.0, "exhaust_idle", e) for e in ex]}
        m.clips["start"] = {"long": 1.5, "loop": False, "after": "hold", "about": "the engine is started: she shudders, a gout of smoke from the exhaust",
                            "tracks": [self.hull_keys(1.5, roll=lambda t: 0.35 * math.exp(-2.2 * t) * math.sin(two_pi * 6 * t), bob=lambda t: 0.004 * math.exp(-2.0 * t) * math.sin(two_pi * 8 * t))],
                            "events": [ev(0.0, "engine_start", "engine", "tank_start")] + [ev(0.15, "exhaust_start", e) for e in ex]}
        # drive: she pitches and rolls gently as the ground passes under her (whole turns in the loop, so it joins)
        m.clips["drive"] = {"long": 2.0, "loop": True, "about": "under way: the hull's own motion (the game rolls the wheels, runs the tracks and moves her)",
                            "tracks": [self.hull_keys(2.0, pitch=lambda t: 0.38 * math.sin(two_pi * t / 2.0 * 2) + 0.16 * math.sin(two_pi * t / 2.0 * 5 + 1.0),
                                                      roll=lambda t: 0.3 * math.sin(two_pi * t / 2.0 * 1 + 0.6) + 0.12 * math.sin(two_pi * t / 2.0 * 4),
                                                      bob=lambda t: 0.006 * math.sin(two_pi * t / 2.0 * 3 + 0.3) + 0.003 * math.sin(two_pi * t / 2.0 * 7))],
                            "events": [ev(0.0, "engine_drive", "engine", drive_cue)] + [ev(0.0, "exhaust_drive", e) for e in ex]
                            + [ev(0.0, "dust_trail", "dust.left"), ev(0.0, "dust_trail", "dust.right")]}
        m.clips["move_off"] = {"long": 1.2, "loop": False, "after": "hold", "about": "she moves off: the nose lifts as the tracks bite, and settles",
                               "tracks": [self.hull_keys(1.2, pitch=lambda t: 1.3 * math.exp(-3.0 * t) * math.sin(two_pi * 1.1 * t) * (1.0 if t > 0 else 0.0), bob=lambda t: 0.004 * math.exp(-3 * t) * math.sin(two_pi * 2.0 * t))],
                               "events": [ev(0.0, "exhaust_rev", e) for e in ex]}
        m.clips["stop"] = {"long": 1.2, "loop": False, "after": "hold", "about": "she brakes: the nose dips and comes back",
                           "tracks": [self.hull_keys(1.2, pitch=lambda t: -1.7 * math.exp(-3.2 * t) * math.sin(two_pi * 1.15 * t), bob=lambda t: -0.005 * math.exp(-3 * t) * math.sin(two_pi * 2.0 * t))],
                           "events": [ev(0.0, "brakes", "dust.left"), ev(0.0, "brakes", "dust.right")]}

        # fire: the barrel runs back and out again; the hull's answer is a clip of its own
        def run_back(t):
            if t < 0.06:
                return recoil * rigid.smooth(t / 0.06)
            return recoil * max(0.0, 1.0 - rigid.smooth((t - 0.06) / 0.7))
        keys = [[t, run_back(t)] for t in (0.0, 0.015, 0.03, 0.045, 0.06, 0.12, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.76, 2.5)]
        m.clips["fire"] = {"long": 2.5, "loop": False, "after": "hold", "about": "the main gun fires: flash and blast at 0.0, the barrel runs back and out, the empty case a moment after",
                           "tracks": [{"part": "barrel", "slide": keys}],
                           "events": [ev(0.0, "muzzle_flash_big", "muzzle", gun_cue), ev(0.0, "muzzle_blast_dust", "muzzle"), ev(1.2, "case_clang", "case", "tank_shell_case")]}
        m.clips["fire.rock"] = {"long": 1.6, "loop": False, "after": "hold",
                                "about": "the hull's answer to the shot, for a gun trained ahead (nose up, then settling). With the turret trained off, turn this about the turret's bearing or play it weaker",
                                "tracks": [self.hull_keys(1.6, pitch=lambda t: kick * math.exp(-3.4 * t) * math.sin(two_pi * 1.5 * t) * 1.25, bob=lambda t: -0.006 * math.exp(-4 * t) * math.sin(two_pi * 3 * t))],
                                "events": []}
        if mg and any(s["name"] == mg for s in m.sockets):
            m.clips["mg_burst"] = {"long": 1.0, "loop": False, "after": "hold", "about": "a burst from the machine gun beside the main gun: ten rounds",
                                   "tracks": [], "events": [ev(0.0, "muzzle_flash", mg, "tank_mg_burst")] + [ev(0.075 * k, "muzzle_flash", mg) for k in range(1, 10)]}
        if any(s["name"] == "hull_mg" for s in m.sockets):
            m.clips["hull_mg_burst"] = {"long": 1.0, "loop": False, "after": "hold", "about": "a burst from the hull machine gun",
                                        "tracks": [], "events": [ev(0.0, "muzzle_flash", "hull_mg", "tank_mg_burst")] + [ev(0.075 * k, "muzzle_flash", "hull_mg") for k in range(1, 10)]}
        # hit but still fighting
        m.clips["hit"] = {"long": 0.9, "loop": False, "after": "hold", "about": "struck, not stopped: she jolts",
                          "tracks": [self.hull_keys(0.9, pitch=lambda t: 0.8 * math.exp(-5 * t) * math.sin(two_pi * 2.2 * t), roll=lambda t: 1.1 * math.exp(-5 * t) * math.sin(two_pi * 1.9 * t + 0.4))],
                          "events": [ev(0.0, "hit_sparks", None, "shell_impact_near")]}

        # hatches
        def lid_keys(name, t0, fast=False):
            p = m.parts[m.part_at[name]]
            top = p.limits[1]
            if fast:    # thrown open: it flies up, strikes its stop and bounces
                return [[0.0, 0.0], [t0, 0.0], [t0 + 0.10, top * 0.75], [t0 + 0.16, top], [t0 + 0.24, top * 0.86], [t0 + 0.34, top], [t0 + 0.42, top * 0.95], [t0 + 0.5, top]]
            return [[0.0, 0.0], [t0, 0.0], [t0 + 0.25, top * 0.35], [t0 + 0.6, top * 0.92], [t0 + 0.75, top]]
        hs = [h for h in hatches if h in m.part_at]
        if hs:
            m.clips["open_up"] = {"long": 1.0, "loop": False, "after": "hold", "about": "the commander's hatch is opened (she travels opened up; play it backward to close down for action)",
                                  "tracks": [{"part": hs[0], "turn": lid_keys(hs[0], 0.1)}], "events": [ev(0.1, "hatch_open", "commander")]}
        # knocked out: struck, the gun droops, the crew's hatches are thrown open, smoke, then fire from the engine
        tr = [self.hull_keys(3.0, pitch=lambda t: 1.0 * math.exp(-4.5 * t) * math.sin(two_pi * 2.0 * t) - 0.5 * rigid.smooth(t / 1.2),
                             roll=lambda t: 1.4 * math.exp(-4.5 * t) * math.sin(two_pi * 1.7 * t + 0.3) + 1.2 * rigid.smooth(t / 1.5),
                             bob=lambda t: -0.02 * rigid.smooth(t / 1.0))]
        if "gun" in m.part_at:
            tr.append({"part": "gun", "turn": [[0.0, 0.0], [0.25, 0.0], [0.9, gun_low * 0.8], [1.2, gun_low], [1.35, gun_low * 0.93], [1.5, gun_low]]})
        for k, h in enumerate(hs):
            tr.append({"part": h, "turn": lid_keys(h, 0.35 + 0.22 * k, fast=(k == 0))})
        m.clips["knocked_out"] = {"long": 3.0, "loop": False, "after": "hold",
                                  "about": "knocked out: struck at 0.0, hatches thrown open as the crew bail out, the gun droops, smoke, then fire at the engine; holds its last pose (follow with `burn`)",
                                  "tracks": tr,
                                  "events": [ev(0.0, "hit", None, "tank_knocked_out"), ev(0.35, "hatch_blown", "commander"), ev(0.5, "smoke_burst", "engine"),
                                             ev(0.9, "bail_out", "commander"), ev(1.2, "fire_start", "engine", "fire_start"), ev(1.6, "smoke_loop", "engine")]}
        m.clips["burn"] = {"long": 2.0, "loop": True, "about": "burning: fire at the engine and out of her hatches, black smoke (loop after knocked_out or brew_up)",
                           "tracks": [], "events": [ev(0.0, "fire_loop", "engine", "fire_loop")] + [ev(0.0, "fire_loop", f) for f in fires] + [ev(0.0, "smoke_loop", "engine")]}
        # brew up: her ammunition goes. A jet of flame from the hatches, then the blast lifts the turret off its ring
        if has_turret and "turret" in m.part_at:
            back, left, rest, over = blow
            ring = m.parts[m.part_at["turret"]].pivot
            t_blast, t_fly = 0.55, 1.25
            g = 9.81
            land_z = rest - ring.z                         # how far below the ring the turret's own origin comes to rest
            vz = (land_z + 0.5 * g * t_fly * t_fly) / t_fly
            axis = Vector((1.0, 0.25, 0.0)).normalized()
            rk, mk = [[0.0, 1.0, 0.0, 0.0, 0.0], [t_blast, 1.0, 0.0, 0.0, 0.0]], [[0.0, 0.0, 0.0, 0.0], [t_blast, 0.0, 0.0, 0.0]]
            n = 14
            for i in range(1, n + 1):
                a = i / n
                t = a * t_fly
                q = Quaternion(axis, math.radians(over * a ** 0.9))
                rk.append([t_blast + t, q.w, q.x, q.y, q.z])
                mk.append([t_blast + t, back * a, left * a, vz * t - 0.5 * g * t * t])
            # it strikes, bounces a hand's breadth and lies
            q = Quaternion(axis, math.radians(over + 6.0))
            rk += [[t_blast + t_fly + 0.12, q.w, q.x, q.y, q.z]]
            mk += [[t_blast + t_fly + 0.12, back * 1.04, left * 1.05, land_z + 0.10]]
            q = Quaternion(axis, math.radians(over + 3.0))
            rk += [[t_blast + t_fly + 0.26, q.w, q.x, q.y, q.z], [4.0, q.w, q.x, q.y, q.z]]
            mk += [[t_blast + t_fly + 0.26, back * 1.06, left * 1.08, land_z], [4.0, back * 1.06, left * 1.08, land_z]]
            tr = [{"part": "turret", "rot": rk, "move": mk},
                  self.hull_keys(4.0, pitch=lambda t: (1.6 * math.exp(-4 * (t - t_blast)) * math.sin(two_pi * 1.6 * (t - t_blast)) if t > t_blast else 0.0),
                                 roll=lambda t: (1.2 * math.exp(-4 * (t - t_blast)) * math.sin(two_pi * 1.4 * (t - t_blast) + 0.5) if t > t_blast else 0.0),
                                 bob=lambda t: (-0.05 * math.exp(-5 * (t - t_blast)) * math.sin(two_pi * 2.2 * (t - t_blast)) - 0.03 * rigid.smooth((t - t_blast) / 0.6) if t > t_blast else 0.0))]
            for k, h in enumerate(hs):
                tr.append({"part": h, "turn": lid_keys(h, 0.05 + 0.04 * k, fast=True)})
            m.clips["brew_up"] = {"long": 4.0, "loop": False, "after": "hold",
                                  "about": "her ammunition goes: flame jets from the hatches, then the blast throws the turret off its ring to lie beside her; holds its last pose (follow with `burn`)",
                                  "tracks": tr,
                                  "events": [ev(0.0, "hit", None, "tank_knocked_out"), ev(0.05, "flame_jet", "fire.2")] + [ev(0.1, "flame_jet", f) for f in fires if f != "fire.2"]
                                  + [ev(t_blast, "magazine_blast", "ring", "explosion_debris"), ev(t_blast + 0.05, "fire_loop", "ring", "fire_loop"), ev(t_blast + t_fly, "turret_lands", "ring", "shell_impact_near"),
                                     ev(t_blast + 0.3, "smoke_column", "ring")]}
        m.info.setdefault("file", {})["controls"] = {
            "turret": {"part": "turret", "what": "trained by the game: + to the right (clockwise from above)", "degrees_per_second": traverse},
            "gun": {"part": "gun", "what": "laid by the game inside its limits: + muzzle up"},
            "hatches": hs,
        }
        return self

    def running_table(self):
        """What the game needs to roll her: every spinning part with its radius, and each track's links and pitch."""
        return {"wheels": [{"part": n, "radius": round(r, 4)} for n, r in self.wheels], "tracks": list(self.belts.values()),
                "how": "turn each wheel by (distance run on its side) / radius, in radians; play tracks.left and tracks.right at (speed of that side) / stride * long"}


# ---- to Blender --------------------------------------------------------------------------------------

def _math(nt, op, a=None, b=None, clamp=False, c=None):
    n = nt.nodes.new("ShaderNodeMath")
    n.operation, n.use_clamp = op, clamp
    for k, v in enumerate((a, b, c)):
        if v is None:
            continue
        if isinstance(v, (int, float)):
            n.inputs[k].default_value = v
        else:
            nt.links.new(v, n.inputs[k])
    return n.outputs[0]


def _mix(nt, blend="MIX", fac=1.0):
    n = nt.nodes.new("ShaderNodeMix")
    n.data_type, n.blend_type = "RGBA", blend
    n.inputs[0].default_value = fac
    return n, n.inputs[0], n.inputs[6], n.inputs[7], n.outputs[2]


def _noise(nt, vec, scale, detail=4.0, rough=0.55, stretch=None):
    nz = nt.nodes.new("ShaderNodeTexNoise")
    nz.inputs["Scale"].default_value = scale
    nz.inputs["Detail"].default_value = detail
    nz.inputs["Roughness"].default_value = rough
    if stretch:
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = stretch
        nt.links.new(vec, mp.inputs["Vector"])
        vec = mp.outputs["Vector"]
    nt.links.new(vec, nz.inputs["Vector"])
    return nz.outputs[0]


def _ramp(nt, fac, lo, hi, a=0.0, b=1.0):
    mr = nt.nodes.new("ShaderNodeMapRange")
    mr.clamp = True
    mr.inputs["From Min"].default_value, mr.inputs["From Max"].default_value = lo, hi
    mr.inputs["To Min"].default_value, mr.inputs["To Max"].default_value = a, b
    nt.links.new(fac, mr.inputs["Value"])
    return mr.outputs["Result"]


DUST = "8A7A5C"


def land_look(model, dust=1.0, wear=1.0, dust_hex=DUST, dust_top=1.35):
    """What the pictures add to a paint that the numbers do not carry: dust that lies thick low down and thin on every
    upward face, paint rubbed to steel on the edges, rain marks, the shine of worn track. Returns the `look` for
    rigid.to_blender. (The game does its own; the file carries wear and soot point by point.)"""
    lo = Vector((min(v.x for v in model.verts), min(v.y for v in model.verts), min(v.z for v in model.verts)))
    hi = Vector((max(v.x for v in model.verts), max(v.y for v in model.verts), max(v.z for v in model.verts)))
    size = hi - lo
    dust_rgb = hex_rgb(dust_hex)

    def look(paint, mat, nt, bsdf, mix):
        key = paint.split(".")[0]
        tc = nt.nodes.new("ShaderNodeTexCoord")
        # the point's own place on the model, in metres (it rides with the part when a turret turns)
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = size
        mp.inputs["Location"].default_value = lo
        nt.links.new(tc.outputs["Generated"], mp.inputs["Vector"])
        pos = mp.outputs["Vector"]
        sep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(pos, sep.inputs[0])
        geo = nt.nodes.new("ShaderNodeNewGeometry")
        nsep = nt.nodes.new("ShaderNodeSeparateXYZ")
        nt.links.new(geo.outputs["Normal"], nsep.inputs[0])
        big = _noise(nt, pos, 1.3, 5.0)
        fine = _noise(nt, pos, 22.0, 6.0, 0.6)
        col = mix.outputs[2]
        if key in ("upper", "upper2", "shade", "white", "red", "yellow", "blue", "wood", "canvas", "rubber", "track", "rust", "cloth", "rope", "leather"):
            # how dusty: thick by the ground, thinning to the height of the track guards; thin on anything that looks up
            mid = _noise(nt, pos, 4.5, 4.0)
            low = _ramp(nt, sep.outputs[2], 0.15, dust_top, 1.0, 0.0)
            low = _math(nt, "MULTIPLY", _math(nt, "POWER", low, 1.5), _ramp(nt, mid, 0.3, 0.7, 0.6, 1.0))
            up = _math(nt, "MULTIPLY", _ramp(nt, nsep.outputs[2], 0.45, 0.98, 0.0, 0.26), _ramp(nt, mid, 0.3, 0.7, 0.5, 1.0))
            grime = _math(nt, "MULTIPLY", _math(nt, "MAXIMUM", low, up), dust * (1.0 if key != "track" else 1.15), clamp=True)
            base = col
            if key in ("upper", "upper2", "shade"):
                # the coat is never one colour: faded here, darker there; rain has run down it
                _, _, a, b, o = _mix(nt, "MULTIPLY", 1.0)
                nt.links.new(base, a)
                tone = nt.nodes.new("ShaderNodeValToRGB")
                tone.color_ramp.elements[0].position, tone.color_ramp.elements[0].color = 0.3, (0.88, 0.88, 0.87, 1)
                tone.color_ramp.elements[1].position, tone.color_ramp.elements[1].color = 0.72, (1.08, 1.08, 1.07, 1)
                nt.links.new(big, tone.inputs["Fac"])
                nt.links.new(tone.outputs["Color"], b)
                streak = _noise(nt, pos, 1.0, 3.0, 0.5, stretch=(26.0, 26.0, 1.6))
                side_on = _ramp(nt, _math(nt, "ABSOLUTE", nsep.outputs[2]), 0.0, 0.55, 1.0, 0.0)
                _, f2, a2, b2, o2 = _mix(nt, "MULTIPLY")
                nt.links.new(_math(nt, "MULTIPLY", side_on, 0.55 * wear), f2)
                nt.links.new(o, a2)
                st = nt.nodes.new("ShaderNodeValToRGB")
                st.color_ramp.elements[0].position, st.color_ramp.elements[0].color = 0.38, (0.72, 0.70, 0.66, 1)
                st.color_ramp.elements[1].position, st.color_ramp.elements[1].color = 0.62, (1.06, 1.06, 1.05, 1)
                nt.links.new(streak, st.inputs["Fac"])
                nt.links.new(st.outputs["Color"], b2)
                base = o2
                # edges: paint rubbed thin, then through to dark steel where it is knocked
                edge = _ramp(nt, geo.outputs["Pointiness"], 0.515, 0.60, 0.0, 1.0)
                chip = _math(nt, "MULTIPLY", edge, _ramp(nt, fine, 0.42, 0.62, 0.0, 1.0), clamp=True)
                _, f3, a3, b3, o3 = _mix(nt, "MIX")
                nt.links.new(_math(nt, "MULTIPLY", edge, 0.22 * wear), f3)
                nt.links.new(base, a3)
                b3.default_value = (0.62, 0.62, 0.58, 1.0)
                _, f4, a4, b4, o4 = _mix(nt, "MIX")
                nt.links.new(_math(nt, "MULTIPLY", chip, 0.75 * wear, clamp=True), f4)
                nt.links.new(o3, a4)
                b4.default_value = (0.045, 0.04, 0.038, 1.0)
                base = o4
                rough = _math(nt, "ADD", _ramp(nt, big, 0.2, 0.8, 0.52, 0.68), _math(nt, "MULTIPLY", grime, 0.3), clamp=True)
                nt.links.new(rough, bsdf.inputs["Roughness"])
            elif key == "track":
                # steel links: dark, rusty in the hollows, polished where they meet the ground and the wheels
                _, f2, a2, b2, o2 = _mix(nt, "MIX")
                nt.links.new(_ramp(nt, fine, 0.35, 0.7, 0.0, 0.55), f2)
                nt.links.new(base, a2)
                b2.default_value = (*hex_rgb("5A3A28"), 1.0)
                edge = _ramp(nt, geo.outputs["Pointiness"], 0.52, 0.62, 0.0, 1.0)
                _, f3, a3, b3, o3 = _mix(nt, "MIX")
                nt.links.new(_math(nt, "MULTIPLY", edge, 0.8), f3)
                nt.links.new(o2, a3)
                b3.default_value = (0.36, 0.36, 0.37, 1.0)
                base = o3
                nt.links.new(_math(nt, "MULTIPLY", edge, 0.9), bsdf.inputs["Metallic"])
                nt.links.new(_ramp(nt, edge, 0.0, 1.0, 0.8, 0.38), bsdf.inputs["Roughness"])
            elif key == "wood":
                wave = nt.nodes.new("ShaderNodeTexWave")
                wave.inputs["Scale"].default_value = 9.0
                wave.inputs["Distortion"].default_value = 5.0
                wave.inputs["Detail"].default_value = 3.0
                nt.links.new(pos, wave.inputs["Vector"])
                _, _, a2, b2, o2 = _mix(nt, "MULTIPLY", 0.55)
                nt.links.new(base, a2)
                rr = nt.nodes.new("ShaderNodeValToRGB")
                rr.color_ramp.elements[0].color, rr.color_ramp.elements[1].color = (0.6, 0.55, 0.5, 1), (1.15, 1.12, 1.1, 1)
                nt.links.new(wave.outputs[1], rr.inputs["Fac"])
                nt.links.new(rr.outputs["Color"], b2)
                base = o2
                bsdf.inputs["Roughness"].default_value = 0.8
            elif key in ("canvas", "cloth", "rope"):
                bsdf.inputs["Roughness"].default_value = 0.95
                weave = _noise(nt, pos, 160.0, 2.0, 0.5)
                bump = nt.nodes.new("ShaderNodeBump")
                bump.inputs["Strength"].default_value = 0.25
                bump.inputs["Distance"].default_value = 0.003
                nt.links.new(_math(nt, "ADD", weave, _math(nt, "MULTIPLY", big, 3.0)), bump.inputs["Height"])
                nt.links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])
                _, _, a2, b2, o2 = _mix(nt, "MULTIPLY", 0.5)
                nt.links.new(base, a2)
                rr = nt.nodes.new("ShaderNodeValToRGB")
                rr.color_ramp.elements[0].color, rr.color_ramp.elements[1].color = (0.7, 0.7, 0.7, 1), (1.15, 1.15, 1.15, 1)
                nt.links.new(big, rr.inputs["Fac"])
                nt.links.new(rr.outputs["Color"], b2)
                base = o2
            elif key == "rubber":
                bsdf.inputs["Roughness"].default_value = 0.88
            elif key == "rust":
                _, _, a2, b2, o2 = _mix(nt, "MULTIPLY", 0.8)
                nt.links.new(base, a2)
                rr = nt.nodes.new("ShaderNodeValToRGB")
                rr.color_ramp.elements[0].color, rr.color_ramp.elements[1].color = (0.45, 0.4, 0.4, 1), (1.4, 1.25, 1.1, 1)
                nt.links.new(fine, rr.inputs["Fac"])
                nt.links.new(rr.outputs["Color"], b2)
                base = o2
                bsdf.inputs["Roughness"].default_value = 0.9
            # the dust goes on last, over everything
            _, f9, a9, b9, o9 = _mix(nt, "MIX")
            nt.links.new(_math(nt, "MULTIPLY", grime, 0.86, clamp=True), f9)
            nt.links.new(base, a9)
            b9.default_value = (*dust_rgb, 1.0)
            nt.links.new(o9, bsdf.inputs["Base Color"])
        elif key in ("steel", "bright", "brass"):
            bsdf.inputs["Metallic"].default_value = 0.85 if key != "steel" else 0.7
            nt.links.new(_ramp(nt, fine, 0.2, 0.8, 0.3, 0.55), bsdf.inputs["Roughness"])
        elif key in ("glass", "lamp"):
            bsdf.inputs["Roughness"].default_value = 0.06
            if "Specular IOR Level" in bsdf.inputs:
                bsdf.inputs["Specular IOR Level"].default_value = 0.9
        elif key == "black":
            bsdf.inputs["Roughness"].default_value = 0.95
            if "Specular IOR Level" in bsdf.inputs:
                bsdf.inputs["Specular IOR Level"].default_value = 0.1
    return look


def to_scene(model, look="land", crease=50.0, weighted=True, **look_kw):
    """The model as a mesh on its skeleton (rigid.to_blender), then given weighted normals: a flat plate stays flat and
    its cut-back edge carries all the rounding, which is what makes an edge look rolled rather than drawn."""
    lk = land_look(model, **look_kw) if look == "land" else look
    ob, arm = rigid.to_blender(model, lk, crease=crease)
    if weighted:
        mod = ob.modifiers.new("wn", "WEIGHTED_NORMAL")
        mod.keep_sharp = True
        mod.weight = 50
        mod.mode = "FACE_AREA"
        bpy.context.view_layer.objects.active = ob
        ob.select_set(True)
        while ob.modifiers[0].name != "wn":
            bpy.ops.object.modifier_move_up(modifier="wn")
        bpy.ops.object.modifier_apply(modifier="wn")
    return ob, arm
