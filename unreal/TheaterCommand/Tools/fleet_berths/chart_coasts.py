# Fleets that lie apart, their cloud test (Tools/fleet_berths/run.sh): the chart's own coasts and seas, written out
# plainly for the test to read. Reads src/generated/campaign-map.json and changes nothing.
# Usage: chart_coasts.py <campaign-map.json> <out.txt>
import json
import sys


def main():
    with open(sys.argv[1], encoding="utf-8") as chart_file:
        chart = json.load(chart_file)
    lines = ["table %s %s" % (chart["width"], chart["height"])]
    zones = chart["seas"]["zones"]
    lines.append("seas %d" % len(zones))
    for zone in zones:
        lines.append("%s %s %s" % (zone["x"], zone["y"], zone["name"].replace(" ", "_")))
    # (each piece of land is its outline, then any holes cut in it: the outline is the coast)
    rings = [piece[0] for piece in chart["land"] if piece and len(piece[0]) >= 3]
    lines.append("rings %d" % len(rings))
    for ring in rings:
        lines.append("%d %s" % (len(ring), " ".join("%s %s" % (point[0], point[1]) for point in ring)))
    with open(sys.argv[2], "w", encoding="ascii") as out_file:
        out_file.write("\n".join(lines) + "\n")
    print("chart: %d seas, %d coasts" % (len(zones), len(rings)))


main()
