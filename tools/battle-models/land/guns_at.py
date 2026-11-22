"""Anti-tank guns: the low towed pieces that lie in wait for tanks. Built on the field guns' kit (guns.GunWorks), with
the same parts, sockets and clips, so they drop into a gun pit's `gun` socket and go on a lorry's hook the same way.
"""

from guns import GunWorks, crew
from motorpool import LEFT, RIGHT, SIDES, X
from tanks_ger import GER_COATS, coats

coat = "upper"


def pak40(fine=True, tag="GER", paint=None):
    """7.5 cm anti-tank gun 40: a long barrel with a big double-baffle brake, very low behind an angular shield whose
    wings sweep back over pressed-steel wheels on solid tyres, round trail legs. 1.25 m high. Dark grey when she
    came (1942), dark yellow from 1943: one model, a paint swap."""
    v = GunWorks("Pak40", "7.5 cm Pak 40", "gun", tag, fine, paint=GER_COATS[paint or "grey"][0])
    # (the E7 surface pass: shade from her shape, dirt of her ground; the art director, 22 Nov 17:55Z)
    v.dice = 0.2
    v.worn_kw = dict(shade=True, shade_deep=0.42, ground="field", layers=2, marks=0.45, gear=0.6, rim=True)
    R, T = 0.45, 1.62
    v.gun_root(R)
    for letter, s in SIDES:
        v.gun_wheel(letter, s, T / 2, R, "solid", w=0.12, holes=8, bolts=6)
    v.axle(T / 2 - 0.06, r=0.05, box=(0.22, 0.34, 0.11, 0.1))
    v.saddle(0.0, 0.96, 0.17, 32.0, rate=6.0, foot=0.28)
    v.cradle(-5.0, 22.0, 0.25, 0.17, 1.05, 0.62, 0.1)
    v.piece(3.70, 0.075, 0.98, 0.098, 0.056, 0.9, brake="double", steps=((0.42, 0.078),), ring=(0.3, 0.29, 0.3), block="right", lug=0.6)
    v.handwheels((-0.3, 0.36, 0.86), (-0.42, 0.3, 0.72), r=0.1)
    v.sight((-0.2, 0.27, 0.98), high=0.18)
    v.shield(0.52, 0.46, 0.4, 1.2, lean=28.0, slot=(0.145, 0.66, None), apron=(0.2, 10.0), wing=(0.36, 38.0, 0.5), port=(0.31, 1.0, 0.11, 0.09))
    E = None
    for letter, s in SIDES:
        E, close = v.split_trail(letter, s, (-0.22, s * 0.26, R - 0.02), 2.95, 30.0, sec0=(0.17, 0.17), sec1=(0.12, 0.12), tube=(0.075, 0.055), end_z=0.12, eye=(s < 0))
    v.use("carriage")
    v.socket("dust.left", (-0.3, T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (-0.3, -T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.muzzle", (2.6, 0.0, 0.03), X, part="vehicle", note="the ground under the muzzle: the blast lifts the dust here")
    crew(v, (-0.75, 0.55, 0.0), (-1.7, -0.5, 0.0), (-2.6, 1.5, 0.0), (-2.2, -1.5, 0.0), (-1.6, -0.2, 0.0))
    v.gun_clips(0.9, E.x, lift=0.05, close=close, eye_z=0.2, track=T, low=-5.0)
    v.m.info.setdefault("file", {})["swaps"] = {"how": "paint: put the chosen coat's colours in place of the paints of the same names.", "paint": coats(v, paint or "grey")}
    return v


def anti_tank_gun(fine=True, tag="_"):
    """A 57 mm anti-tank gun, as most armies fielded one: a slim plain barrel, low on lorry wheels, a flat shield with
    its corners cut and an apron under it, short box trail legs spread wide. 1.2 m high."""
    v = GunWorks("AntiTankGun", "57 mm anti-tank gun", "gun", tag, fine)
    R, T = 0.42, 1.52
    v.gun_root(R)
    for letter, s in SIDES:
        v.gun_wheel(letter, s, T / 2, R, "pneumatic", w=0.19, rr=0.235, holes=0, bolts=6)
    v.axle(T / 2 - 0.095, r=0.045, box=(0.2, 0.3, 0.1, 0.09))
    v.saddle(0.0, 0.86, 0.15, 40.0, rate=8.0, foot=0.25)
    v.cradle(-5.0, 15.0, 0.2, 0.14, 0.9, 0.52, 0.085)
    v.piece(2.95, 0.057, 0.82, 0.078, 0.042, 0.75, steps=((0.34, 0.06),), ring=(0.23, 0.23, 0.25), block="down", lug=0.6, swell=(0.1, 0.05))
    v.handwheels((-0.28, 0.32, 0.78), (-0.4, 0.26, 0.64), r=0.095)
    v.sight((-0.2, 0.24, 0.88), high=0.17)
    v.shield(0.42, 0.63, 0.34, 1.14, lean=12.0, slot=(0.12, 0.6, None), corner=0.14, apron=(0.17, 6.0), port=(0.3, 0.96, 0.1, 0.08))
    E = None
    for letter, s in SIDES:
        E, close = v.split_trail(letter, s, (-0.2, s * 0.24, R - 0.02), 2.5, 32.0, sec0=(0.15, 0.17), sec1=(0.1, 0.11), end_z=0.12, eye=(s < 0))
    v.use("carriage")
    v.socket("dust.left", (-0.3, T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.right", (-0.3, -T / 2, 0.05), (-1, 0, 0.3), part="vehicle")
    v.socket("dust.muzzle", (2.1, 0.0, 0.03), X, part="vehicle", note="the ground under the muzzle: the blast lifts the dust here")
    crew(v, (-0.7, 0.5, 0.0), (-1.5, -0.45, 0.0), (-2.3, 1.4, 0.0), (-2.0, -1.4, 0.0), (-1.4, -0.2, 0.0))
    v.gun_clips(0.75, E.x, lift=0.045, close=close, eye_z=0.2, track=T, low=-5.0)
    v.camouflage()
    return v


_ = (LEFT, RIGHT)
