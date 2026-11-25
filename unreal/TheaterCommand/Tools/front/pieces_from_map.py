#!/usr/bin/env python3
"""The game's own map as pieces of border, for Tools/front/run.sh (front_test reads the file this writes).

Finds the stretches of line two provinces share as the table does (ACampaignMapActor::FindSharedEdges): each edge of
each province's outline, probed a third of a unit outward; an edge under 0.05 long is left out; each shared line once,
from the lower-numbered province into the higher. Then who holds each province (the map's 1939 owners, with any
--hold NAME:TAG over them), the wars, and the player.

Usage: pieces_from_map.py MAP.json OUT.txt --player GER --war GER:POL --war GER:FRA [--hold Warsaw:GER ...]
"""
import argparse
import json
import math


def inside(x, y, ring):
    hit = False
    n = len(ring)
    for i in range(n):
        x1, y1 = ring[i]
        x2, y2 = ring[(i + 1) % n]
        if (y1 > y) != (y2 > y):
            if x1 + (y - y1) * (x2 - x1) / (y2 - y1) > x:
                hit = not hit
    return hit


def main():
    ask = argparse.ArgumentParser()
    ask.add_argument("map")
    ask.add_argument("out")
    ask.add_argument("--player", default="")
    ask.add_argument("--war", action="append", default=[])
    ask.add_argument("--hold", action="append", default=[])
    said = ask.parse_args()
    with open(said.map) as f:
        data = json.load(f)
    regions = data["regions"]
    rings, boxes = {}, {}
    for r in regions:
        polys = r["polys"]
        if isinstance(polys, str):
            polys = json.loads(polys)
        rings[r["id"]] = [p[0] for p in polys]
        xs = [q[0] for ring in rings[r["id"]] for q in ring]
        ys = [q[1] for ring in rings[r["id"]] for q in ring]
        boxes[r["id"]] = (min(xs), min(ys), max(xs), max(ys))

    def region_at(x, y):
        for rid, rs in rings.items():
            b = boxes[rid]
            if b[0] <= x <= b[2] and b[1] <= y <= b[3] and any(inside(x, y, ring) for ring in rs):
                return rid
        return None

    lines = []
    for rid in sorted(rings):
        for ring in rings[rid]:
            n = len(ring)
            area = sum(ring[i][0] * ring[(i + 1) % n][1] - ring[(i + 1) % n][0] * ring[i][1] for i in range(n))
            outward = 1.0 if area >= 0 else -1.0
            for i in range(n):
                a, b = ring[i], ring[(i + 1) % n]
                length = math.dist(a, b)
                if length < 0.05:
                    continue
                dx, dy = (b[0] - a[0]) / length, (b[1] - a[1]) / length
                sx, sy = dy * outward, -dx * outward
                other = region_at((a[0] + b[0]) / 2 + sx * 0.35, (a[1] + b[1]) / 2 + sy * 0.35)
                if other is None or other <= rid:
                    continue
                lines.append(f"piece {a[0]} {a[1]} {b[0]} {b[1]} {sx} {sy} {rid} {other}")
    held = {r["id"]: r["owner"] for r in regions}
    by_name = {r["name"]: r["id"] for r in regions}
    for one in said.hold:
        name, tag = one.split(":")
        held[by_name[name]] = tag
    for rid in sorted(held):
        lines.append(f"holds {rid} {held[rid]}")
    for one in said.war:
        a, b = one.split(":")
        lines.append(f"war {a} {b}")
    if said.player:
        lines.append(f"player {said.player}")
    with open(said.out, "w") as f:
        f.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
