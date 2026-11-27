# The 1941 Navy Yard and Ford Island's seaplane base as grey blocks, for the 3D game's Pearl Harbor stage (the art
# director on build 1734 asked for hangars, sheds and the dry docks; the historian's table of 19 November, kept in
# the project's historian/pearl-1941-yard-blocks.md, gives each block from today's mapped footprints, marked H, M, L).
#
# A row of the stage file's "yard": x, y, ground z, length, width, height, the compass heading of the length, kind.
#   0 a shop or a hangar: walls and a roof on a concrete pad 12 m wider all round
#   1 a dry dock: the coping (length by width) round its basin (20 m shorter, 15 m narrower), drawn drained and
#     dark; the caisson is the coping's end on the heading's side
#   2 a paved apron, flat
#   3 a floating dry dock: its pontoon at the water and two wing walls the height high
#   4 a mooring quay: a concrete block standing in the water, its top the height above the sea
# The ground under a block is the highest of the land under its footprint (a shop's with its pad), found every 10 m
# by mesh.py's zat: the slab's top clears the land and its skirt reaches down where the land falls (the 3D game
# takes the skirts 4.5 m down; the cloud pre-check on round 6 found pads under the land when the ground was the
# lowest of a few points).
import math

SHOP, DOCK, APRON, FLOATING, QUAY = 0, 1, 2, 3, 4

OAHU = [
    # name, x, y, length, width, height, heading, kind
    # Dry Dock No. 1: the record's basin of 1,001 by 138 ft inside its coping; its axis 31/211 (the historian's
    # correction to berths.py). Pennsylvania, Cassin and Downes lie in it. Dry Docks 2 and 3 were building: left out.
    ('Dry Dock No. 1', -700.0, -1410.0, 330, 60, 0, 31, DOCK),
    # YFD-2 round Shaw, off the beach west of Dry Dock No. 1 where Nevada beached, parallel to it (the historian on
    # 19 November from three 1941 photo captions; place and size L/M). Shaw lies in it bow west.
    ('YFD-2', -985.0, -1250.0, 160, 40, 12, 95, FLOATING),
    # The shop rows by the 1010 Dock (1919-41; one or two may be post-war rebuilds on the same footprints).
    ('Shipfitting and welding shop', -180.0, -1290.0, 170, 100, 18, 121, SHOP),
    ('Inside/Outside Machine Shop', -330.0, -1160.0, 160, 70, 15, 121, SHOP),
    ('Inside Machine Shop', -340.0, -1070.0, 90, 70, 11, 31, SHOP),
    ('Building 1', -320.0, -1490.0, 140, 70, 13, 121, SHOP),
    ('Building 167', -130.0, -1510.0, 160, 60, 9, 122, SHOP),
    ('Sheet Metal Shop', 120.0, -1200.0, 150, 50, 10, 31, SHOP),
    ('Central Tool Shop', 180.0, -1250.0, 150, 50, 7, 31, SHOP),
    ('Foundry', -560.0, -1570.0, 130, 40, 12, 121, SHOP),
    # Ford Island's seaplane base at the island's south end: the apron, and the three hangars that stood that day
    # (79 still carries the attack's bullet holes). The burned Hangar 6 and the tower (finished 1942) are left out.
    ('Seaplane apron', -1170.0, -520.0, 570, 230, 0, 50, APRON),
    ('Hangar 79', -1140.0, -580.0, 100, 70, 20, 141, SHOP),
    ('Hangar 37', -1020.0, -450.0, 70, 50, 15, 123, SHOP),
    ('Hangar 54', -1280.0, -680.0, 120, 50, 18, 157, SHOP),
    # Maryland's two 1941 mooring quays at F-5, like Arizona's and Tennessee's (size M), in place of today's piers there.
    ('F-5 quay, north', -135.0, -113.0, 15, 8, 2.0, 55, QUAY),
    ('F-5 quay, south', -202.0, -166.0, 15, 8, 2.0, 55, QUAY),
]

# Today's mapped piers that stand where 1941 ships lay, left out of the stage's works (mesh.py's piers skips a pier
# whose middle is within 20 m of one of these): the historian, 19 November.
CUT = {'oahu': [
    (-150.8, -151.3),  # the F-5 pier (Missouri's today): Maryland's hull lies on it
    (-135.2, -113.2),  # the two S378 pieces beside it, where the 1941 quays go back
    (-201.6, -165.9),
    (-836.1, 746.4),   # S382 at F-12: Raleigh's hull lies on it (and 5% of Detroit's)
]}

# The airfields whose hangars the yard draws (mesh.py lays no row of stand-in hangars there).
DRAWN_FIELDS = {'oahu': {'Ford Island'}}


def footprint(x, y, length, width, heading, step=10.0):
    """Points over a block's footprint, every step metres, its edges and corners among them."""
    h = math.radians(heading)
    u = (math.sin(h), math.cos(h))
    v = (u[1], -u[0])
    na, nb = max(1, math.ceil(length / step)), max(1, math.ceil(width / step))
    out = []
    for i in range(na + 1):
        for j in range(nb + 1):
            a, b = length * (i / na - 0.5), width * (j / nb - 0.5)
            out.append((x + u[0] * a + v[0] * b, y + u[1] * a + v[1] * b))
    return out


def yard(key, zat):
    """The stage file's yard rows for a stage (none but Oahu's), the ground under each found by zat(x, y)."""
    if key != 'oahu':
        return []
    out = []
    for _, x, y, length, width, height, heading, kind in OAHU:
        if kind in (FLOATING, QUAY):
            ground = 0.0
        else:
            pad = 24 if kind == SHOP else 0
            ground = max(zat(px, py) for px, py in footprint(x, y, length + pad, width + pad, heading))
        out.append([round(x, 1), round(y, 1), round(ground, 1), length, width, height, heading, kind])
    return out
