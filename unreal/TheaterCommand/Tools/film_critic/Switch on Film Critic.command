#!/bin/bash
# Double-click to switch on the film critic's night run on this Mac (it films the game while the Mac sits idle at
# night and checks how it looks). To switch it off, run: bash "<this folder>/install.sh" off
bash "$(dirname "$0")/install.sh" on
echo
read -n 1 -s -r -p "Press any key to close this window."
