"""Tank helper: triangles per part at each LOD, the belt's path numbers, one wheel, sprocket and idler.
    python tank_t_tris.py"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sys, time, os
import tank, kit
kit.fresh()
for lod in (0, 1, 2):
    g = tank.Geo(); rows = []
    for fn in (tank.campaign_parts, tank.running_gear, tank.driver_hatch, tank.bow_gun, tank.lamps, tank.mudguards, tank.spare_links,
               tank.shackles, tank.exhaust, tank.engine_deck, tank.tools_and_cable, tank.engine_door, tank.stowage, tank.hull_welds, tank.turret_fittings, tank.aerial, tank.gun):
        a = g.tris(); fn(g, lod); rows.append((fn.__name__, g.tris() - a))
    print("LOD", lod, g.tris(), rows)
print("path", tank.PATH.length, tank.PATH.links, tank.PATH.pitch, tank.PATH.teeth, tank.PATH.drive_links, tank.PATH.drive_metres)
g = tank.Geo(); print("belt kept", tank.belt_links(g, 1.0), "of", tank.PATH.links, g.tris())
g = tank.Geo(); tank.road_wheel(g, tank.Vector((0,1.15,0.41)), 1.0, "w", 0); print("wheel", g.tris())
g = tank.Geo(); tank.sprocket(g, 1.0, "s", 0); print("sprocket", g.tris())
g = tank.Geo(); tank.idler(g, 1.0, "s", 0); print("idler", g.tris())
