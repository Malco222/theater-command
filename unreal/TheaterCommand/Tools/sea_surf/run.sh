#!/bin/bash
# Surf on the campaign map's coasts, its cloud test (release 2, tc.Map.Surf): runs the real build_sea out of
# Tools/build_content.py against a stand-in for Unreal's editor that only writes the material's graph down, and works
# the graph out as numbers. M_Sea, built with no breakers, is node for node the M_Sea of before; M_SeaSurf with its
# measure at nothing draws exactly M_Sea; what M_SeaSurf draws is the sum the breakers were judged on, to the last
# decimals; there are none over the open sea nor where the game puts the surf out, a fifth of them in a bay's lee,
# and parts of every crest are missing; and they cost three more reads of the Waves picture. No engine, no compiler:
# python3 and its own library.
# Usage: Tools/sea_surf/run.sh
set -euo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
"${PYTHON:-python3}" "$HERE/sea_surf_test.py" "$HERE/../build_content.py" "$@"
