#!/usr/bin/env python3
"""Turns the Mac build's own compile settings into settings for a syntax-only check on Linux.

Reads the response files and definition headers that Unreal's build tool wrote on the Mac (copied to
/home/user/ue58/project) and writes, under /home/user/precheck:
  defs/SharedDefinitions.Linux.h   the shared definitions with the Mac platform swapped for Linux
  defs/Definitions.TheaterCommand.h  the module's own definitions on top of those
  includes.rsp                     every include directory, as absolute paths in this container
The project's own paths are written as @PROJECT@ and @GENERATED@ and filled in per tree by check.py.
"""
import os, re, sys

ENGINE = "/home/user/ue58/Engine"
MAC_PROJECT = "/Users/malco/Total War WW2/unreal/TheaterCommand"
INTER = "/home/user/ue58/project/Intermediate/Build/Mac"
OUT = "/home/user/precheck"

shared = open(f"{INTER}/arm64/TheaterCommandEditor/Development/UnrealEd/SharedDefinitions.UnrealEd.Project.ValApi.ValExpApi.Cpp20.h").read()
drop = ("PLATFORM_APPLE", "UE_PLATFORM_APPLE", "PLATFORM_MAC", "UE_PLATFORM_MAC", "HAS_METAL", "UBT_COMPILED_PLATFORM",
        "UBT_COMPILED_ARCHITECTURE", "WITH_APPLE_CONTACTS", "UE_USE_SWIFT_UI_MAIN")
lines = []
for line in shared.splitlines():
    m = re.match(r"#define\s+(\w+)", line)
    if m and m.group(1) in drop:
        continue
    lines.append(line)
linux = """
// (added for the cloud check: the Linux platform in place of the Mac)
#define UBT_COMPILED_PLATFORM Linux
#define UBT_COMPILED_ARCHITECTURE x64
#define OVERRIDE_PLATFORM_HEADER_NAME Linux
#define PLATFORM_LINUX 1
#define UE_PLATFORM_LINUX 1
#define PLATFORM_UNIX 1
#define UE_PLATFORM_UNIX 1
#define LINUX 1
"""
os.makedirs(f"{OUT}/defs", exist_ok=True)
open(f"{OUT}/defs/SharedDefinitions.Linux.h", "w").write("\n".join(lines) + "\n" + linux)

module = open(f"{INTER}/arm64/UnrealEditor/Development/TheaterCommand/Definitions.TheaterCommand.h").read()
module = re.sub(r'#include "SharedDefinitions[^"]*"', '#include "SharedDefinitions.Linux.h"', module)
open(f"{OUT}/defs/Definitions.TheaterCommand.h", "w").write(module)

rsp = open(f"{INTER}/arm64/UnrealEditor/Development/TheaterCommand/CampaignHUD.cpp.o.rsp").read().splitlines()
seen, out = set(), []
for line in rsp:
    m = re.match(r'-(I|isystem)"?([^"]+)"?$', line)
    if not m:
        continue
    kind, path = m.group(1), m.group(2)
    if path.startswith(MAC_PROJECT + "/Intermediate/Build/Mac/UnrealEditor/Inc/TheaterCommand/UHT"):
        path = "@GENERATED@"
    elif path.startswith(MAC_PROJECT + "/Intermediate/Build/Mac/arm64/TheaterCommandEditor/Development/UnrealEd"):
        path = f"{OUT}/defs"
    elif path.startswith(MAC_PROJECT):
        path = "@PROJECT@" + path[len(MAC_PROJECT):]
    elif path.endswith("LibTiff/Source/Mac"):
        continue
    else:
        path = os.path.normpath(os.path.join(ENGINE, "Source", path))
    key = (kind, path)
    if key in seen:
        continue
    seen.add(key)
    if not path.startswith("@") and not os.path.isdir(path):
        print("missing include dir:", path, file=sys.stderr)
        continue
    out.append(f'-{kind}"{path}"' if kind == "I" else f'-isystem"{path}"')
open(f"{OUT}/includes.rsp", "w").write("\n".join(out) + "\n")
print(len(out), "include dirs written")
