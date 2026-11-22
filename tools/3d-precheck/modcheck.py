#!/usr/bin/env python3
"""Module check: every Unreal symbol the game's object files call must come from a module TheaterCommand.Build.cs lists
directly (Public or Private). The Mac's modular link takes no module through another's public list: run 5 of the city
glance failed to link on FImage::CopyTo (ImageCore) although Engine lists ImageCore publicly.

usage: modcheck.py <label> <tree>/unreal/TheaterCommand   (after check.py <label> ... --link)
Owner of a symbol: the module whose headers declare its class (or, for a free function or global, declare it with a
<MODULE>_API macro), from the container's Unreal headers (/home/user/ue58). Index cached in modindex.json."""
import json, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
UE = "/home/user/ue58/Engine"
CACHE = os.path.join(HERE, "modindex.json")
MARKERS = ("Public", "Classes", "Internal", "Private")
# owners the header scan cannot see (declared through macros), read by hand in the headers
KNOWN = {"TaskTrace::TaskChannel": ["Core"], "UE::GC::GIsIncrementalReachabilityPending": ["CoreUObject"]}


def module_of(path):
    parts = path.split(os.sep)
    for i in range(len(parts) - 1, 0, -1):
        if parts[i] in MARKERS:
            return parts[i - 1]
    return None


def build_index():
    classes, funcs, macros, spaces = {}, {}, {}, set()
    cls_re = re.compile(r"^\s*(?:template\s*<[^>]*>\s*)?(?:class|struct)\s+(?:alignas\s*\([^)]*\)\s*)?(?:(\w+_API)\s+)?(?:UE_DEPRECATED\([^)]*\)\s*)?(\w+)\s*(?:final\s*)?(?=[:{]|$)", re.M)
    api_re = re.compile(r"\b([A-Z0-9_]+)_API\b([^;{]*?)\b(\w+)\s*(?=\(|;|\[|=)")
    for root, _, files in os.walk(UE):
        for f in files:
            if not f.endswith((".h", ".inl")):
                continue
            p = os.path.join(root, f)
            mod = module_of(os.path.relpath(p, UE))
            if not mod:
                continue
            macros.setdefault(mod.upper() + "_API", mod)
            try:
                t = open(p, errors="ignore").read()
            except OSError:
                continue
            for ns in re.findall(r"\bnamespace\s+([A-Za-z_][\w:]*)", t):
                spaces.update(x for x in ns.split("::") if x)
            for m in cls_re.finditer(t):
                name = m.group(2)
                if name in ("final", "alignas"):
                    continue
                classes.setdefault(name, set()).add(mod)
            for m in api_re.finditer(t):
                funcs.setdefault(m.group(3), set()).add((m.group(1) + "_API", mod))
    # a function's owner: its macro's module, or the header's own module where the macro is a local alias (UE_API,
    # TASK_CORE_API and the like)
    fn = {k: sorted({macros.get(x, m) for x, m in v}) for k, v in funcs.items()}
    cl = {k: sorted(v) for k, v in classes.items()}
    for k in ("FString", "FAnsiString", "FUtf8String"):
        cl[k] = ["Core"]  # declared through UE_STRING_CLASS
    return {"classes": cl, "funcs": fn, "spaces": sorted(spaces)}


def game_names(tree):
    """Classes, structs, namespaces and functions the game's own headers declare: its symbols are its own."""
    names = set()
    for root, _, files in os.walk(os.path.join(tree, "Source")):
        for f in files:
            if f.endswith((".h", ".cpp")):
                t = open(os.path.join(root, f), errors="ignore").read()
                names.update(re.findall(r"\b(?:class|struct|namespace|enum class|enum)\s+(?:\w+_API\s+)?(\w+)", t))
    return names


def index():
    if os.path.exists(CACHE):
        return json.load(open(CACHE))
    ix = build_index()
    json.dump(ix, open(CACHE, "w"))
    return ix


def build_cs_modules(tree):
    p = os.path.join(tree, "Source", "TheaterCommand", "TheaterCommand.Build.cs")
    t = open(p).read()
    t = re.sub(r"//[^\n]*", "", t)
    mods = set()
    for m in re.finditer(r"(?:Public|Private)DependencyModuleNames\.(?:AddRange|Add)\s*\((.*?)\)\s*;", t, re.S):
        mods.update(re.findall(r'"(\w+)"', m.group(1)))
    return mods


def split_top(s, sep="::"):
    out, depth, cur, i = [], 0, "", 0
    while i < len(s):
        c = s[i]
        if c in "<(":
            depth += 1
        elif c in ">)":
            depth -= 1
        if depth == 0 and s.startswith(sep, i):
            out.append(cur)
            cur = ""
            i += len(sep)
            continue
        cur += c
        i += 1
    out.append(cur)
    return out


def qualified(sym):
    for pre in ("vtable for ", "typeinfo for ", "typeinfo name for ", "VTT for ", "guard variable for ",
                "non-virtual thunk to ", "virtual thunk to ", "construction vtable for "):
        if sym.startswith(pre):
            sym = sym[len(pre):]
    depth, cut = 0, len(sym)
    for i, c in enumerate(sym):
        if c == "<":
            depth += 1
        elif c == ">":
            depth -= 1
        elif c == "(" and depth == 0:
            cut = i
            break
    name = sym[:cut].strip()
    return [re.sub(r"<.*$", "", p).strip() for p in split_top(name)]


def main():
    label, tree = sys.argv[1], os.path.abspath(sys.argv[2])
    objdir = os.path.join(HERE, "out", label, "obj")
    ix = index()
    have = build_cs_modules(tree)
    mine = game_names(tree)
    spaces = set(ix.get("spaces", []))
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
                if vis == "DEFAULT":
                    wanted.setdefault(name, set()).add(f[:-2])
            elif bind in ("GLOBAL", "WEAK"):
                defined.add(name)
    names = sorted(n for n in wanted if n not in defined)
    plain = subprocess.run(["c++filt"], input="\n".join(names), capture_output=True, text=True).stdout.splitlines()
    need, unknown = {}, {}
    for n, p in zip(names, plain):
        if re.match(r"^(?:(?:vtable|VTT|typeinfo|typeinfo name) for |void |bool |int |unsigned |double |float |char |long )?(?:std::|__cxxabiv1::|__cxa|__gxx|__cxx|operator new|operator delete|__)", p) or "::" not in p and n == p:
            continue  # the C and C++ runtimes
        q = qualified(p)
        if any(c in mine for c in q[:-1]) or (len(q) == 1 and q[0] in mine):
            continue  # the game's own
        owners = None
        z = re.match(r"Z_Construct_U(?:Class|ScriptStruct|Enum)_(\w+?)(?:_NoRegister)?$", q[-1])
        if z:
            if z.group(1) in mine or z.group(1).split("_")[0] in mine:
                continue
            owners = ix["classes"].get(z.group(1))
        if q[-1].split("_")[0] in mine and len(q) == 1:
            continue  # the game's own generated helpers
        for comp in ([] if owners else reversed(q[:-1])):
            if comp in spaces:
                continue  # a namespace (UE, Private, CoreUObject, TaskTrace), not a class
            if comp in ix["classes"]:
                owners = ix["classes"][comp]
                break
        if owners is None and len(q) == 1 and q[0] in ix["classes"]:
            owners = ix["classes"][q[0]]  # vtable/typeinfo of a class
        if owners is None and q[-1] in ix["funcs"]:
            owners = ix["funcs"][q[-1]]
        if not owners and p in KNOWN:
            owners = KNOWN[p]
        if not owners:
            unknown.setdefault(p, set()).update(wanted[n])
            continue
        if any(o in have for o in owners):
            continue
        key = "/".join(owners)
        need.setdefault(key, []).append((p, sorted(wanted[n])))
    print(f"module check: Build.cs lists {', '.join(sorted(have))}")
    if not need:
        print("module check: every Unreal symbol the files call comes from a module Build.cs lists")
    for mod, rows in sorted(need.items()):
        print(f"  MODULE NOT IN Build.cs: {mod} ({len(rows)} symbols)")
        for p, by in rows[:8]:
            print(f"    {p}   (needed by {', '.join(by)})")
    if unknown:
        print(f"module check: {len(unknown)} symbols with no owner found in the headers (read by eye):")
        for p, by in sorted(unknown.items())[:20]:
            print(f"    {p}   ({', '.join(sorted(by))})")
    sys.exit(1 if need else 0)


if __name__ == "__main__":
    main()
