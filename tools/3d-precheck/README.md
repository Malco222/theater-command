# The 3D pre-check

A cloud check of the 3D game's C++ before a batch reaches Brian's Mac, so the Mac never spends a build on a mistake
a compiler could have found first. Think of it as a spell-checker that uses the Mac's own dictionary: the same
compiler family, Unreal 5.8.4's own headers and the Mac build's own settings, with nothing built.

Brian approved copying Unreal's reference files off his Mac once, reading only ("Yes, copy them once", 17 November 2026).

## What it checks, per head (about 90 seconds)

| Step | What it catches |
|---|---|
| Compile check | Every .cpp of the game module against Unreal 5.8.4's headers with the Mac's warning set. Only what is new against the base a30710a (the last head the Mac built before the check began) is reported. |
| Link step | Anything declared and used but never written, and calls into Unreal functions that are not exported. |
| Unity guard | Counts the module's .cpp files. At 32 Unreal's build tool glues them into combined units and same-named file-local helpers clash. |
| Data files | Each changed .json under Content must still parse. |
| Rules side | When the bridge, the rules or the data changed: the committed bundle must equal a fresh build, and the bridge type-check must show nothing new. |
| Merge | Whether the head fast-forwards from origin/unreal. |

## How to run it

    cd /home/user/precheck
    ./head.sh <branch> <sha>            # against a30710a
    ./head.sh <branch> <sha> <base-sha> # against another base

It prints `VERDICT: PASS`, `PASS with warnings` or `FAIL`, with each new error's file, line and source text.
To check two heads together, merge them in a scratch worktree and run
`python3 check.py combined <worktree>/unreal/TheaterCommand --link` then `python3 compare.py base-a30710a combined`.

## What it cannot see

- Code inside `#if PLATFORM_MAC` (RulesHost.cpp, two places in CampaignPlayerController.cpp) and Objective-C++:
  Linux stands in for the Mac platform.
- Unreal's header tool. Reflection macros (UCLASS, UPROPERTY, UFUNCTION) are checked only as C++, on the Mac's last
  generated headers moved to the new line numbers. A brand-new reflected header gets a stand-in and a note.
- New Build.cs dependencies: the include list is the Mac snapshot's. head.sh prints a note when Build.cs changes.
- Whether the game runs. It is a compile and link check plus a reader's eye, not a playtest.

## The unity trap (found 17 November 2026, reported to the coordinator)

The module has 28 .cpp files once TheaterStillMesh.cpp lands. At 32 (MinGameModuleSourceFilesForUnityBuild in the
Mac's TargetRules.cs) the Mac builds them glued together, and 13 file-local helper names exist in two files:
FromHex, ToByte, White, Black, Gold, Faint, Brass, DeskInk, Paper, Cream, Alarm, Stone, Field (31 errors in a trial).
Cure, one line, before file 32: `bUseUnity = false;` in Source/TheaterCommand/TheaterCommand.Build.cs.

## Rebuilding it in a fresh container (about ten minutes, one Allow from Brian)

Unreal's headers live only inside the container (/home/user/ue58). They are Epic's and must never be copied to
shared project storage or to the repository. A new container needs them copied from the Mac again:

1. Connect two folders on Brian's personal Mac (never his company Mac), reading only:
   `/Users/Shared/Epic Games/UE_5.8/Engine` and `/Users/malco/Total War WW2`.
2. Run `mac-export.sh` there through device_bash. It reads Unreal and writes three archives (about 21 MB) into the
   project's `unreal/TheaterCommand/Saved/precheck-export` folder, which git ignores. Use no git command that
   writes, and nothing that builds: the builder's checkout lives in that folder.
3. Bring the three archives over with device_stage_files, then run
   `bash rebuild.sh "<the folder they landed in>"` from this folder. It installs clang 20, unpacks, derives the
   Linux settings from the Mac's, builds the precompiled header and checks the base, which must come out clean.
4. Empty the archives on the Mac (deleting is not allowed there: `: > file` for each).

After the Mac builds a head that changes a reflected header a lot, repeat steps 2 to 4 to refresh the snapshot.

## Files

- `head.sh` one head, start to verdict. `check.py` the compile and link check. `compare.py` what is new against a base.
- `setup.py` turns the Mac build's settings into Linux ones. `base-flags.rsp` the compiler flags (the Mac's warnings).
- `rebuild.sh`, `mac-export.sh` as above.

## Update 22 Nov 2026 (second pre-check thread)

- `modcheck.py` (new): the link check for Unreal modules. With `--link`, check.py runs it after the link step. Every Unreal symbol the game's objects call must come from a module that `TheaterCommand.Build.cs` lists directly. The Mac's modular link takes no module through another module's public list; map run 5 failed on FImage::CopyTo (ImageCore) this way. The index of owners is built from the container's Unreal headers into `modindex.json`. That file is derived from Epic's headers, so it is never copied here or to the repository. A miss prints `MODULE NOT IN Build.cs: X` and compare.py's VERDICT turns to FAIL.
- `cap.sh`: the Mac's warning-cap pass (shadowed names and the rest) on one file. Usage: `bash cap.sh <label> <File.cpp>`, with `.cpp` in the name.
- Run the timing rows (land_play, land_live, air_show, land_field) alone. Compiles on the same machine push the clock rows over their limits.
- Run `build_rules.sh` and `build_land_rules.sh` from the repository's top folder. From elsewhere, esbuild's "// path" comments differ, though the code is the same.
