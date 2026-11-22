#!/usr/bin/env python3
"""Compile-checks one tree of the 3D game's C++ against Unreal 5.8's real headers, without building anything.

  check.py LABEL /path/to/unreal/TheaterCommand [file.cpp ...]

Every .cpp under Source/TheaterCommand (or only the ones named) is parsed and type-checked by clang with the Mac
build's own warning set, the editor's shared precompiled header, and the Linux platform in place of the Mac.
The diagnostics go to out/LABEL/<file>.log and a summary to out/LABEL/summary.json.

What this cannot see: code inside `#if PLATFORM_MAC`, Objective-C++, the linker, and Unreal's header tool
(reflection macros are checked only as C++, on the Mac's last generated headers moved to the right lines).
"""
import concurrent.futures, json, os, re, shutil, subprocess, sys, time

HERE = "/home/user/precheck"
SNAP_GENERATED = "/home/user/ue58/project/Intermediate/Build/Mac/UnrealEditor/Inc/TheaterCommand/UHT"
CLANG = "clang++-20"
PCH = f"{HERE}/pch/SharedPCH.Linux.h.pch"

CLASS_TEMPLATE = '''// (synthesized by the cloud check: the Mac has not generated this header yet)
#include "UObject/ObjectMacros.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "UObject/ScriptMacros.h"
PRAGMA_DISABLE_DEPRECATION_WARNINGS
{bodies}
#undef CURRENT_FILE_ID
#define CURRENT_FILE_ID {fid}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
'''
CLASS_BODY = '''struct Z_Construct_UClass_{name}_Statics;
THEATERCOMMAND_API UClass* Z_Construct_UClass_{name}(ETypeConstructPhase);
#define {fid}_{prolog}_PROLOG
#define {fid}_{line}_GENERATED_BODY \\
PRAGMA_DISABLE_DEPRECATION_WARNINGS \\
public: \\
private: \\
	friend struct ::Z_Construct_UClass_{name}_Statics; \\
	friend THEATERCOMMAND_API UClass* ::Z_Construct_UClass_{name}(ETypeConstructPhase); \\
public: \\
	DECLARE_CLASS2({name}, {parent}, COMPILED_IN_FLAGS(0), CASTCLASS_None, TEXT("/Script/TheaterCommand"), Z_Construct_UClass_{name}) \\
	DECLARE_SERIALIZER({name}) \\
	{name}({name}&&) = delete; \\
	{name}(const {name}&) = delete; \\
	DECLARE_VTABLE_PTR_HELPER_CTOR(NO_API, {name}); \\
	DEFINE_VTABLE_PTR_HELPER_CTOR_CALLER({name}); \\
{ctor}	{call}({name}) \\
	NO_API virtual ~{name}(); \\
private: \\
PRAGMA_ENABLE_DEPRECATION_WARNINGS
class {name};
'''


def find_headers(source):
    out = {}
    for root, _, files in os.walk(source):
        for f in files:
            if f.endswith(".h"):
                out[f[:-2]] = os.path.join(root, f)
    return out


def macro_lines(text):
    """Line numbers (from 1) of the reflection macros in a header, in order."""
    prolog, body = [], []
    for i, line in enumerate(text.splitlines(), 1):
        s = line.strip()
        if re.match(r"(UCLASS|USTRUCT|UINTERFACE)\s*\(", s):
            prolog.append(i)
        if re.match(r"GENERATED_(UCLASS_|USTRUCT_|UINTERFACE_|IINTERFACE_)?BODY\s*\(", s):
            body.append(i)
    return prolog, body


def prepare_generated(tree, outdir, notes):
    """The Mac's generated headers, with their line keys moved to where the macros stand in this tree."""
    gen = os.path.join(outdir, "generated")
    shutil.rmtree(gen, ignore_errors=True)
    os.makedirs(gen)
    headers = find_headers(os.path.join(tree, "Source"))
    needed = {}
    for name, path in headers.items():
        text = open(path, encoding="utf-8", errors="replace").read()
        if f'"{name}.generated.h"' in text:
            needed[name] = (path, text)
    for name, (path, text) in sorted(needed.items()):
        prolog, body = macro_lines(text)
        snap = os.path.join(SNAP_GENERATED, f"{name}.generated.h")
        if os.path.exists(snap):
            g = open(snap).read()
            old_prolog = sorted({int(m) for m in re.findall(r"_h_(\d+)_PROLOG\b", g)})
            old_body = sorted({int(m) for m in re.findall(r"_h_(\d+)_GENERATED_BODY\b", g)})
            if len(old_prolog) != len(prolog) or len(old_body) != len(body):
                notes.append(f"{name}.h: the number of reflected types changed since the Mac's last build "
                             f"({len(old_body)} then, {len(body)} now): its generated header is out of date here")
            mapping = dict(zip(old_prolog, prolog))
            mapping.update(dict(zip(old_body, body)))
            moved = {k: v for k, v in mapping.items() if k != v}
            # two passes through a placeholder, so that a line moved onto another's old number is not moved twice
            g = re.sub(r"_h_(\d+)_", lambda m: f"_h_@{mapping.get(int(m.group(1)), int(m.group(1)))}@_", g)
            g = g.replace("_h_@", "_h_").replace("@_", "_")
            open(os.path.join(gen, f"{name}.generated.h"), "w").write(g)
            if moved:
                notes.append(f"{name}.h: reflection macros moved lines {moved}; generated header re-keyed")
        else:
            # A header the Mac has never generated for: a plain class body, enough for the C++ to be checked.
            fid = "FID_cloud_" + re.sub(r"\W", "_", os.path.relpath(path, tree))
            bodies = []
            lines = text.splitlines()
            for p, b in zip(prolog, body):
                decl = " ".join(lines[p:b])
                m = re.search(r"class\s+(?:\w+_API\s+)?(\w+)\s*(?:final\s*)?:\s*public\s+(\w+)", decl)
                if not m:
                    notes.append(f"{name}.h: could not read the class at line {p}; no generated header made")
                    continue
                # (the header tool calls the constructor the class declares: a plain one, or one taking the object
                # initializer; with neither it declares the second itself)
                cls = m.group(1)
                if re.search(r"\b%s\s*\(\s*\)\s*;" % cls, text):
                    ctor, call = "", "DEFINE_DEFAULT_CONSTRUCTOR_CALL"
                elif re.search(r"\b%s\s*\(\s*const\s+FObjectInitializer\s*&" % cls, text):
                    ctor, call = "", "DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL"
                else:
                    ctor = "\tNO_API %s(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()); \\\n" % cls
                    call = "DEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL"
                bodies.append(CLASS_BODY.format(name=cls, parent=m.group(2), fid=fid, prolog=p, line=b, ctor=ctor, call=call))
            open(os.path.join(gen, f"{name}.generated.h"), "w").write(CLASS_TEMPLATE.format(bodies="\n".join(bodies), fid=fid))
            notes.append(f"{name}.h: new reflected header, not yet built on the Mac: a stand-in generated header was made "
                         f"(Unreal's header tool has not checked its UCLASS/UPROPERTY/UFUNCTION lines)")
    return gen


def compile_one(args):
    tree, gen, cpp, logdir, objdir = args
    inc = open(f"{HERE}/includes.rsp").read().replace("@GENERATED@", gen).replace("@PROJECT@", tree)
    rsp = os.path.join(logdir, os.path.basename(cpp) + ".rsp")
    mode = f'-c\n-O0\n-o "{os.path.join(objdir, os.path.basename(cpp))}.o"\n' if objdir else "-fsyntax-only\n"
    open(rsp, "w").write(open(f"{HERE}/base-flags.rsp").read() + mode + inc +
                         f'-include-pch "{PCH}"\n-include "{HERE}/defs/Definitions.TheaterCommand.h"\n"{cpp}"\n')
    t = time.time()
    p = subprocess.run([CLANG, f"@{rsp}"], capture_output=True, text=True)
    log = p.stderr
    open(os.path.join(logdir, os.path.basename(cpp) + ".log"), "w").write(log)
    return cpp, p.returncode, log, time.time() - t


DIAG = re.compile(r"^(?P<file>/[^:]+):(?P<line>\d+):(?P<col>\d+): (?P<kind>error|warning|fatal error): (?P<msg>.*)$")


def parse(log, tree):
    """Diagnostics as dicts; the text of the source line is kept so that two trees can be compared by content."""
    out = []
    lines = log.splitlines()
    for i, line in enumerate(lines):
        m = DIAG.match(line)
        if not m:
            continue
        d = m.groupdict()
        d["line"] = int(d["line"])
        rel = os.path.relpath(d["file"], tree) if d["file"].startswith(tree) else d["file"].replace("/home/user/ue58/", "UE:")
        d["file"] = rel
        src = ""
        if i + 1 < len(lines) and re.match(r"^\s*\d+ \| ", lines[i + 1]):
            src = re.sub(r"^\s*\d+ \| ", "", lines[i + 1]).strip()
        d["src"] = src
        notes = []
        j = i + 1
        while j < len(lines) and not DIAG.match(lines[j]):
            n = re.match(r"^(/[^:]+):(\d+):(\d+): note: (.*)$", lines[j])
            if n:
                notes.append(n.group(4))
            j += 1
        d["notes"] = notes[:4]
        out.append(d)
    return out


GENERATED_NAMES = re.compile(r"^__dso_handle$|GetPrivateStaticClass|StaticClass\(\)|Z_Construct_|__DefaultConstructor|__VTableCtorCaller|StaticRegisterNatives|StaticStruct\(\)|Z_Registration_")


def link_check(objdir, notes):
    """What the Mac's linker would call undefined: a symbol a file needs that is hidden (not exported by Unreal, or one
    of the game's own) and that no file of the game defines."""
    defined, wanted = set(), {}
    for f in sorted(os.listdir(objdir)):
        if not f.endswith(".o"):
            continue
        out = subprocess.run(["readelf", "-sW", os.path.join(objdir, f)], capture_output=True, text=True).stdout
        for line in out.splitlines():
            parts = line.split()
            if len(parts) < 8 or not parts[0].rstrip(":").isdigit():
                continue
            bind, vis, ndx, name = parts[4], parts[5], parts[6], parts[7]
            if ndx == "UND":
                if vis == "HIDDEN":
                    wanted.setdefault(name, set()).add(f[:-2])
            elif bind in ("GLOBAL", "WEAK"):
                defined.add(name)
    missing = {n: w for n, w in wanted.items() if n not in defined}
    if not missing:
        return []
    names = sorted(missing)
    plain = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.splitlines()
    out = []
    for n, p in zip(names, plain):
        if GENERATED_NAMES.search(p):
            continue  # made by Unreal's header tool on the Mac
        out.append({"symbol": p, "needed_by": sorted(missing[n])})
    return out


def main():
    args = sys.argv[1:]
    objects = "--link" in args
    args = [a for a in args if a != "--link"]
    label, tree = args[0], os.path.abspath(args[1])
    only = set(args[2:])
    outdir = f"{HERE}/out/{label}"
    logdir = os.path.join(outdir, "logs")
    os.makedirs(logdir, exist_ok=True)
    notes = []
    objdir = os.path.join(outdir, "obj") if objects else None
    if objdir:
        shutil.rmtree(objdir, ignore_errors=True)
        os.makedirs(objdir)
    gen = prepare_generated(tree, outdir, notes)
    cpps = []
    for root, _, files in os.walk(os.path.join(tree, "Source", "TheaterCommand")):
        for f in sorted(files):
            if f.endswith(".cpp") and (not only or f in only):
                cpps.append(os.path.join(root, f))
    cpps.sort()
    results = {}
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        for cpp, code, log, secs in pool.map(compile_one, [(tree, gen, c, logdir, objdir) for c in cpps]):
            diags = parse(log, tree)
            results[os.path.relpath(cpp, tree)] = {"code": code, "seconds": round(secs, 1), "diags": diags}
            errs = sum(1 for d in diags if d["kind"] != "warning")
            warns = len(diags) - errs
            print(f"{'ok  ' if code == 0 else 'FAIL'} {os.path.basename(cpp):36s} {secs:5.1f}s  {errs} errors, {warns} warnings", flush=True)
    undefined = None
    if objdir:
        # the reflection code the Mac generated last, so that what it defines is not counted as missing
        have = set(find_headers(os.path.join(tree, "Source")))
        gens = [os.path.join(SNAP_GENERATED, f) for f in sorted(os.listdir(SNAP_GENERATED))
                if f.endswith(".gen.cpp") and (f.endswith(".init.gen.cpp") or f[:-8] in have)]
        with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
            for cpp, code, log, secs in pool.map(compile_one, [(tree, gen, c, logdir, objdir) for c in gens]):
                if code != 0:
                    notes.append(f"{os.path.basename(cpp)}: the Mac's last generated code no longer compiles against this tree's header (the header tool will regenerate it there)")
        undefined = link_check(objdir, notes)
        print(f"link step: {len(undefined)} symbols needed but neither exported by Unreal nor defined in the game")
        for u in undefined:
            print(f"  UNDEFINED {u['symbol']}   (needed by {', '.join(u['needed_by'])})")
        # every Unreal symbol called must come from a module Build.cs lists directly (the Mac links no module through
        # another's public list: run 5's FImage::CopyTo, ImageCore)
        mc = subprocess.run([sys.executable, os.path.join(HERE, "modcheck.py"), label, tree], capture_output=True, text=True)
        print(mc.stdout.rstrip())
        if mc.returncode != 0:
            notes.append("MODULE CHECK FAILED: a module the code calls is not in TheaterCommand.Build.cs (see 'MODULE NOT IN Build.cs')")
    json.dump({"tree": tree, "notes": notes, "results": results, "undefined": undefined}, open(os.path.join(outdir, "summary.json"), "w"), indent=1)
    for n in notes:
        print("note:", n)
    bad = [k for k, v in results.items() if v["code"] != 0]
    print(f"{len(cpps)} files, {len(bad)} with errors")


if __name__ == "__main__":
    main()
