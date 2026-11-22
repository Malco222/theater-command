#!/bin/bash
# Run on Brian's personal Mac through device_bash (the Linux VM there), with these two folders connected:
#   /Users/Shared/Epic Games/UE_5.8/Engine      (seen as $HOME/mnt/Engine)
#   /Users/malco/Total War WW2                  (seen as $HOME/mnt/Total War WW2)
# It only READS Unreal and the project. The one thing it writes is three archives (about 21 MB) into the project's
# Saved/precheck-export folder, which git ignores. Nothing is built, no branch is touched.
# Afterwards: device_stage_files on the three archives, rebuild.sh in the cloud, then empty the archives on the Mac
# (deleting is not allowed there: `: > file` for each).
set -e
OUT="$HOME/mnt/Total War WW2/unreal/TheaterCommand/Saved/precheck-export"
mkdir -p "$OUT"

cd "$HOME/mnt/Engine"
# 1. Unreal's own headers (about 1 GB read, 19 MB written; a few minutes)
find Source/Runtime Source/Developer Source/Editor -type f \
  \( -name '*.h' -o -name '*.inl' -o -name '*.hpp' -o -name '*.inc' -o -name '*.def' -o -name '*.ipp' \) -print0 \
  | tar --null -T - -cf - | zstd -T4 -9 -q -o "$OUT/engine-source-headers.tar.zst" -f

# 2. the few third-party and plugin folders the game's include list names, and Unreal's generated headers.
#    If setup.py later prints "missing include dir", add that folder here.
( find Source/ThirdParty/AtomicQueue Source/ThirdParty/GuidelinesSupportLibrary/GSL-1144/include \
       Source/ThirdParty/LibTiff/Source Source/ThirdParty/RapidJSON/1.1.0 Source/ThirdParty/libSampleRate/Public \
       Source/Programs/UnrealLightmass/Public -type f -print0
  find Plugins/Runtime/ProceduralMeshComponent/Source -type f \( -name '*.h' -o -name '*.inl' \) -print0
  find Plugins/Runtime/ProceduralMeshComponent/Intermediate/Build/Mac/UnrealEditor/Inc -type f -name '*.h' -print0
  find Shaders/Shared -type f -name '*.h' -print0
  find Intermediate/Build/Mac/UnrealEditor/Inc -type f -name '*.h' -print0
  printf 'Build/Build.version\0' ) \
  | tar --null -T - -cf - | zstd -T4 -9 -q -o "$OUT/engine-extra-headers.tar.zst" -f

# 3. what the Mac's last build of the game wrote: its compile settings and the generated reflection code
cd "$HOME/mnt/Total War WW2/unreal/TheaterCommand"
find Intermediate/Build/Mac -type f \
  \( -name '*.h' -o -name '*.cpp.o.rsp' -o -name '*.gch.rsp' -o -name '*.d' -o -name '*.gen.cpp' \) -print0 \
  | tar --null -T - -cf - | zstd -T4 -9 -q -o "$OUT/project-intermediate.tar.zst" -f
ls -la "$OUT"
