#!/bin/bash
# Rebuilds the cloud pre-check in a fresh container, in about five minutes:
#   rebuild.sh <folder holding the three .tar.zst files copied from the Mac> [clone of the repository]
# The three files are made on the Mac by mac-export.sh (reading only) and brought over with device_stage_files.
# Unreal's headers stay inside this container (/home/user/ue58). They are Epic's: never copy them to shared storage.
set -euo pipefail
SRC="$1"; CLONE="${2:-/home/user/theater-command}"
KIT="$(cd "$(dirname "$0")" && pwd)"
PRE=/home/user/precheck; UE=/home/user/ue58; BASE=67e36d03

command -v clang++-20 > /dev/null && command -v zstd > /dev/null && command -v readelf > /dev/null || {
  apt-get update -qq; apt-get install -y -qq clang-20 libc++-20-dev libc++abi-20-dev zstd binutils; }

mkdir -p "$UE/Engine" "$UE/project" "$PRE/defs" "$PRE/pch" "$PRE/try" "$PRE/out"
zstd -dc "$SRC/engine-source-headers.tar.zst" | tar -xf - -C "$UE/Engine"
zstd -dc "$SRC/engine-extra-headers.tar.zst" | tar -xf - -C "$UE/Engine"
zstd -dc "$SRC/project-intermediate.tar.zst" | tar -xf - -C "$UE/project"

if [ "$KIT" != "$PRE" ]; then cp "$KIT"/{setup.py,check.py,compare.py,head.sh,rebuild.sh,mac-export.sh,base-flags.rsp} "$PRE/"; fi
chmod +x "$PRE/head.sh" "$PRE/rebuild.sh"

# the base tree every head is compared with: the last head the Mac built before the pre-check began
git -C "$CLONE" fetch -q origin unreal
[ -d /home/user/wt/base ] || git -C "$CLONE" worktree add -f --detach /home/user/wt/base "$BASE" > /dev/null
[ -d /home/user/wt/base/node_modules ] || ( cd /home/user/wt/base && npm ci --ignore-scripts > /dev/null 2>&1 )

cd "$PRE"
python3 setup.py
grep -v '^-x$' base-flags.rsp | grep -v '^c++$' | grep -v '^-x c++$' > pch-flags.rsp
printf '#pragma once\n#include "SharedDefinitions.Linux.h"\n#include "UnrealEdSharedPCH.h"\n' > defs/SharedPCH.Linux.h
sed -e "s#@PROJECT@#/home/user/wt/base/unreal/TheaterCommand#" \
    -e "s#@GENERATED@#$UE/project/Intermediate/Build/Mac/UnrealEditor/Inc/TheaterCommand/UHT#" includes.rsp > try/inc.rsp
echo "building the precompiled header (about half a minute)"
clang++-20 -x c++-header @pch-flags.rsp @try/inc.rsp defs/SharedPCH.Linux.h -o pch/SharedPCH.Linux.h.pch

echo "checking the base $BASE: it must come out with 0 files with errors and 0 symbols at the link step"
python3 check.py "base-$BASE" /home/user/wt/base/unreal/TheaterCommand --link | grep -v '^ok '
