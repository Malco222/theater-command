"""The room given back (the release 2 ocean plan, step 0: measure and guard rails).

A sea battle borrows the campaign room's lamps, sky light, fog and atmosphere, and must give every one back as it was
(ASeaBattle::TakeTheRoom and GiveBackTheRoom; the view under the water's GoUnder and ComeUp). This lists every setter
the sea's code calls on one of them, in Battle/SeaBattle.cpp, FleetSea.cpp, SeaWeather.cpp and SeaUnder.cpp (the two
middle ones once written), and fails unless each has a kept field that the giving back restores:
- a setter is a Set...() call, or a write to a field, on a lamp (UDirectionalLightComponent), a sky light, a fog
  (UExponentialHeightFogComponent) or an atmosphere, known by how the variable is declared there (or, for a member,
  in the file's header); a helper in another file handed the component (BattleLook::AirHaze) is followed one deep;
- it is restored when the giving back (GiveBackTheRoom; ComeUp under the water; GiveBack... in the new files) sets the
  same field on the same kind of component from a kept value: Kept...X that the taking side fills (Kept.X = ...), or a
  member saved from the component itself (bKeyShadows = Key->CastShadows);
- a component the scene makes for itself (NewObject, or GetComponents on its own actor) goes with the scene and is
  listed as its own, not given back;
- a pair the battle look keeps for itself (BattleLook::AirSkyBegin) must be closed in the giving back (AirSkyEnd).
Step 11 widens it to the lens and console variables. "--self-test" proves the scan on made-up code that must fail and
pass.

Usage (from the project, TheaterCommand): python3 -I Tools/sea_waves/room_scan.py [--self-test]
"""

import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "Source", "TheaterCommand")

KINDS = {
    "UDirectionalLightComponent": "lamp",
    "USkyLightComponent": "sky light",
    "UExponentialHeightFogComponent": "fog",
    "USkyAtmosphereComponent": "atmosphere",
}
SCANNED = ["Battle/SeaBattle.cpp", "Battle/FleetSea.cpp", "Battle/SeaWeather.cpp", "Battle/SeaUnder.cpp"]
# The giving back, by file (each file's own, and always the battle's GiveBackTheRoom).
GIVERS = {
    "Battle/SeaBattle.cpp": ["ASeaBattle::GiveBackTheRoom"],
    "Battle/SeaUnder.cpp": ["FSeaUnder::ComeUp"],
    "Battle/FleetSea.cpp": ["GiveBack"],
    "Battle/SeaWeather.cpp": ["GiveBack"],
}
MAIN_GIVER = ("Battle/SeaBattle.cpp", "ASeaBattle::GiveBackTheRoom")
# Setters that set one field by another name.
FIELD_OF = {
    "SetWorldRotation": "Rotation", "SetRelativeRotation": "Rotation", "SetWorldTransform": "Rotation",
    "SetRelativeTransform": "Rotation", "SetLightColor": "LightColor", "SetCastShadows": "CastShadows",
}
# Pairs the battle look keeps for itself: begun in the taking, ended in the giving back.
PAIRS = [("BattleLook::AirSkyBegin", "BattleLook::AirSkyEnd")]
# What the scan must find in today's code, so that it never passes by finding nothing (file, kind, field).
FLOOR = [
    ("Battle/SeaBattle.cpp", "fog", "StartDistance"), ("Battle/SeaBattle.cpp", "fog", "FogDensity"),
    ("Battle/SeaBattle.cpp", "fog", "FogInscatteringColor"), ("Battle/SeaBattle.cpp", "lamp", "Intensity"),
    ("Battle/SeaBattle.cpp", "lamp", "Rotation"), ("Battle/SeaBattle.cpp", "lamp", "LightSourceAngle"),
    ("Battle/SeaBattle.cpp", "lamp", "Temperature"), ("Battle/SeaBattle.cpp", "sky light", "Intensity"),
    ("Battle/SeaUnder.cpp", "lamp", "CastShadows"), ("Battle/SeaUnder.cpp", "fog", "FogDensity"),
]


# ---- Reading C++ ----------------------------------------------------------------------------------------------------------

def strip(text):
    """The code with its comments and the insides of its strings and characters blanked (newlines kept), so nothing in
    a comment or a log line is taken for a call."""
    out = list(text)
    i, n = 0, len(text)

    def blank(a, b):
        for k in range(a, min(b, n)):
            if out[k] != "\n":
                out[k] = " "

    while i < n:
        if text.startswith("//", i):
            j = text.find("\n", i)
            j = n if j < 0 else j
            blank(i, j)
            i = j
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            blank(i, j)
            i = j
        elif text[i] in "\"'":
            q = text[i]
            j = i + 1
            while j < n and text[j] != q and text[j] != "\n":
                j += 2 if text[j] == "\\" else 1
            blank(i + 1, j)
            i = j + 1
        else:
            i += 1
    return "".join(out)


HEADER = re.compile(r"^(?![\s#{}/])(?!(?:namespace|struct|class|using|template|enum|typedef|static_assert)\b)(?!.*;\s*$)"
                    r".*?(?<![\w:])((?:\w+::)*~?\w+)\s*\(", re.M)


def regions(code):
    """The file cut at each definition at the head of a line: (qualified name, start, end)."""
    marks = [(m.group(1), m.start()) for m in HEADER.finditer(code)]
    out = []
    for k, (name, start) in enumerate(marks):
        end = marks[k + 1][1] if k + 1 < len(marks) else len(code)
        out.append((name, start, end))
    if not marks:
        out.append(("(file)", 0, len(code)))
    elif marks[0][1] > 0:
        out.insert(0, ("(file)", 0, marks[0][1]))
    return out


def matching(code, i, open_="(", close=")"):
    depth = 0
    for j in range(i, len(code)):
        if code[j] == open_:
            depth += 1
        elif code[j] == close:
            depth -= 1
            if depth == 0:
                return j
    return len(code)


KIND_ALT = "|".join(KINDS)


def typed(code):
    """The variables of a stretch of code with a component's kind: name -> (kind, how: 'ptr' or 'ref', origin)."""
    out = {}
    for m in re.finditer(r"\b(" + KIND_ALT + r")\s*(?:const\s*)?([*&])\s*(\w+)", code):
        out[m.group(3)] = [KINDS[m.group(1)], "ref" if m.group(2) == "&" else "ptr", "room"]
    for m in re.finditer(r"\b(\w+)\s*=\s*(?:Cast|CastChecked|NewObject)\s*<\s*(" + KIND_ALT + r")\s*>", code):
        out.setdefault(m.group(1), [KINDS[m.group(2)], "ptr", "room"])
    for m in re.finditer(r"\b(\w+)\s*=\s*NewObject\s*<", code):
        if m.group(1) in out:
            out[m.group(1)][2] = "own"
    own = {m.group(1) for m in re.finditer(r"(?<![\w>.])GetComponents\s*\(\s*(\w+)\s*\)", code)}
    for m in re.finditer(r"for\s*\(\s*(?:const\s+)?(" + KIND_ALT + r")\s*\*\s*(\w+)\s*:\s*(\w+)\s*\)", code):
        if m.group(3) in own:
            out[m.group(2)][2] = "own"
    return {k: tuple(v) for k, v in out.items()}


def members(header_text):
    code = strip(header_text)
    out = {}
    for m in re.finditer(r"TWeakObjectPtr\s*<\s*(" + KIND_ALT + r")\s*>\s+(\w+)", code):
        out[m.group(2)] = (KINDS[m.group(1)], "ptr", "room")
    for m in re.finditer(r"(?:TObjectPtr\s*<\s*(" + KIND_ALT + r")\s*>|\b(" + KIND_ALT + r")\s*\*)\s*(\w+)\s*[;={]", code):
        out[m.group(3)] = (KINDS[m.group(1) or m.group(2)], "ptr", "room")
    return out


SETTER = re.compile(r"\b(\w+)\s*(?:\.\s*Get\s*\(\s*\))?\s*(->|\.)\s*(Set\w+)\s*\(")
WRITE = re.compile(r"\b(\w+)\s*(->|\.)\s*([A-Z]\w*)\s*=(?!=)")


def setters_in(code, names):
    """Each setter call or field write on a known component in code: (receiver, kind, origin, field, call, argument, at)."""
    found = []
    for m in SETTER.finditer(code):
        var, arrow, call = m.group(1), m.group(2), m.group(3)
        if var not in names:
            continue
        kind, how, origin = names[var]
        if arrow == "." and how != "ref" and ".Get" not in m.group(0).replace(" ", ""):
            continue
        close = matching(code, m.end() - 1)
        found.append((var, kind, origin, FIELD_OF.get(call, call[3:]), call + "()", code[m.end():close], m.start()))
    for m in WRITE.finditer(code):
        var, arrow, field = m.group(1), m.group(2), m.group(3)
        if var not in names:
            continue
        kind, how, origin = names[var]
        if arrow == "." and how != "ref":
            continue
        end = code.find(";", m.end())
        found.append((var, kind, origin, field, field + " =", code[m.end():end], m.start()))
    return found


class Source:
    """The files the scan reads: from the tree, or made up (the self-test)."""

    def __init__(self, files=None):
        self.files = files

    def read(self, rel):
        if self.files is not None:
            return self.files.get(rel)
        path = os.path.join(SRC, rel)
        if not os.path.exists(path):
            return None
        with open(path, encoding="utf-8") as f:
            return f.read()

    def all_cpp(self):
        if self.files is not None:
            return [k for k in self.files if k.endswith(".cpp")]
        out = []
        for base, _, names in os.walk(SRC):
            out += [os.path.relpath(os.path.join(base, n), SRC) for n in names if n.endswith(".cpp")]
        return sorted(out)


def helper_setters(source, qualified, kind):
    """The setters a helper in another file calls on the component of this kind it is handed (one deep)."""
    for rel in source.all_cpp():
        text = source.read(rel)
        if text is None or qualified + "(" not in text.replace(" (", "("):
            continue
        code = strip(text)
        for name, start, end in regions(code):
            if name != qualified:
                continue
            body = code[start:end]
            params = code[start:matching(code, code.find("(", start))]
            handed = {v: t for v, t in typed(params).items() if t[0] == kind}
            if not handed:
                return rel, []
            return rel, [s for s in setters_in(body, handed)]
    return None, []


def scan(source, out=print):
    """Returns (setters listed, failures)."""
    listed, failures = [], []
    givers = {}  # rel -> [(region name, code)] of the giving back
    takes = []
    for rel in SCANNED:
        text = source.read(rel)
        if text is None:
            out(f"  {rel}: not written yet")
            continue
        code = strip(text)
        header = source.read(rel[:-4] + ".h")
        known = members(header) if header else {}
        saved, kept = set(), set()
        for m in re.finditer(r"\b(\w+)\s*=\s*(\w+)\s*(?:\.\s*Get\s*\(\s*\))?\s*->\s*\w+", code):
            saved.add(m.group(1))
        for m in re.finditer(r"\b(Kept\w*)\s*\.\s*(\w+)\s*=(?!=)", code):
            kept.add(m.group(2))
        for name, start, end in regions(code):
            body = code[start:end]
            names = dict(known)
            names.update(typed(body))
            giving = any(name == g or name.endswith("::" + g) or name.split("::")[-1].startswith(g) and g == "GiveBack" for g in GIVERS.get(rel, []))
            found = setters_in(body, names)
            if giving:
                givers.setdefault(rel, []).append((name, found, body, saved, kept))
                continue
            for var, kind, origin, field, call, arg, _ in found:
                takes.append((rel, name, var, kind, origin, field, call, None))
            # (a helper handed one of the room's components)
            for m in re.finditer(r"\b((?:\w+::)+\w+)\s*\(", body):
                args = body[m.end():matching(body, m.end() - 1)]
                for var, (kind, how, origin) in names.items():
                    if re.search(r"(?<![\w.>])\*?\s*" + re.escape(var) + r"\b(?:\s*\.\s*Get\s*\(\s*\))?(?!\s*(?:->|\.))", args):
                        where, inner = helper_setters(source, m.group(1), kind)
                        for ivar, ikind, _, field, call, _, _ in inner:
                            takes.append((rel, name, var, kind, origin, field, f"{m.group(1)} -> {ivar}.{call}", where))
            for begin, end_ in PAIRS:
                if re.search(re.escape(begin) + r"\s*\(", body):
                    takes.append((rel, name, begin, "pair", "room", end_, begin + "()", None))
    main_giver = []
    for rel, items in givers.items():
        for item in items:
            if (rel, item[0]) == MAIN_GIVER:
                main_giver.append((rel,) + item)
    for rel, region, var, kind, origin, field, call, via in takes:
        line = f"{rel.split('/')[-1]} {region.split('::')[-1]}: {kind} {var} {call}"
        if origin == "own":
            listed.append((rel, kind, field))
            out(f"  own  {line} (the scene's own, gone with it)")
            continue
        if kind == "pair":
            ok = any(re.search(re.escape(field) + r"\s*\(", g[3]) for g in [(rel,) + i for i in givers.get(rel, [])] + main_giver)
            out(f"  {'ok  ' if ok else 'FAIL'} {line}: {'closed by ' + field + '() in the giving back' if ok else 'never closed by ' + field + '()'}")
            if not ok:
                failures.append(line)
            continue
        listed.append((rel, kind, field))
        restored = None
        for g in [(rel,) + i for i in givers.get(rel, [])] + main_giver:
            grel, gname, gfound, _, gsaved, gkept = g
            for gvar, gkind, gorigin, gfield, gcall, garg, _ in gfound:
                if gkind != kind or gfield != field:
                    continue
                keeps = [k for k in re.findall(r"\bKept\w*\s*\.\s*(\w+)", garg) if k in gkept]
                saves = [w for w in re.findall(r"\b(\w+)\b", garg) if w in gsaved]
                if keeps or saves:
                    restored = f"{gname.split('::')[-1]}: {gvar} {gcall} from {'Kept.' + keeps[0] if keeps else saves[0]}"
                    break
            if restored:
                break
        out(f"  {'ok  ' if restored else 'FAIL'} {line}" + (f"{' (by ' + via + ')' if via else ''} -> {restored}" if restored else
                                                         f"{' (by ' + via + ')' if via else ''}: no kept {field} given back"))
        if not restored:
            failures.append(line)
    return listed, failures


# ---- The self-test: made-up code the scan must pass and fail --------------------------------------------------------------

MADE_UP_HEADER = """
struct FKeptLamp { TWeakObjectPtr<UDirectionalLightComponent> Lamp; float Intensity = 0.0f; };
TArray<FKeptLamp> KeptLamps;
TWeakObjectPtr<UDirectionalLightComponent> BattleKey;
"""
MADE_UP_GOOD = """
void ASeaBattle::TakeTheRoom()
{
	for (UDirectionalLightComponent* Light : Lights)
	{
		FKeptLamp& Kept = KeptLamps.AddDefaulted_GetRef();
		Kept.Lamp = Light;
		Kept.Intensity = Light->Intensity;
		Light->SetIntensity(7.0f); // (a comment: Light->SetTemperature(1.0f) is not a call)
	}
	USkyAtmosphereComponent* Made = NewObject<USkyAtmosphereComponent>(this);
	Made->SetMieAnisotropy(0.75f);
	UE_LOG(LogTheater, Display, TEXT("Light->SetLightColor(White) in a log line"));
}

void ASeaBattle::GiveBackTheRoom()
{
	for (const FKeptLamp& Kept : KeptLamps)
	{
		if (UDirectionalLightComponent* Light = Kept.Lamp.Get())
		{
			Light->SetIntensity(Kept.Intensity);
		}
	}
}
"""


def self_test():
    count, bad = 0, []

    def expect(name, files, fails, listed_at_least=1):
        nonlocal count
        count += 1
        quiet = []
        listed, failures = scan(Source(files), out=quiet.append)
        ok = (len(failures) > 0) == fails and len(listed) >= listed_at_least
        if not ok:
            bad.append(name)
            print(f"  FAILED self-test: {name}")
            for line in quiet:
                print("   ", line)

    good = {"Battle/SeaBattle.cpp": MADE_UP_GOOD, "Battle/SeaBattle.h": MADE_UP_HEADER}
    expect("a lamp kept and given back passes; the scene's own atmosphere, comments and log lines ask nothing", good, False, 2)
    unkept = dict(good)
    unkept["Battle/SeaBattle.cpp"] = MADE_UP_GOOD.replace("Light->SetIntensity(7.0f);", "Light->SetIntensity(7.0f);\n\t\tLight->SetTemperature(4900.0f);")
    expect("a setter with nothing given back fails", unkept, True)
    constant = dict(good)
    constant["Battle/SeaBattle.cpp"] = MADE_UP_GOOD.replace("Light->SetIntensity(Kept.Intensity);", "Light->SetIntensity(1.0f);")
    expect("a giving back from a constant, not a kept field, fails", constant, True)
    unfilled = dict(good)
    unfilled["Battle/SeaBattle.cpp"] = MADE_UP_GOOD.replace("Kept.Intensity = Light->Intensity;", "")
    expect("a kept field the taking never fills fails", unfilled, True)
    member = dict(good)
    member["Battle/SeaBattle.cpp"] = MADE_UP_GOOD.replace("Made->SetMieAnisotropy(0.75f);", "Made->SetMieAnisotropy(0.75f);\n\tBattleKey->SetCastShadows(false);")
    expect("a setter on a member lamp (from the header) with nothing given back fails", member, True)
    helper = dict(good)
    helper["Battle/SeaBattle.cpp"] = MADE_UP_GOOD.replace("Made->SetMieAnisotropy(0.75f);", "Made->SetMieAnisotropy(0.75f);\n\tBattleLook::Lean(*BattleKey.Get(), 2.0);")
    helper["Campaign/Look.cpp"] = "void BattleLook::Lean(UDirectionalLightComponent& Lamp, double By)\n{\n\tLamp.SetLightSourceAngle(By);\n}\n"
    expect("a helper handed a lamp, setting it, with nothing given back fails", helper, True)
    paired = dict(good)
    paired["Battle/SeaBattle.cpp"] = MADE_UP_GOOD.replace("Made->SetMieAnisotropy(0.75f);", "BattleLook::AirSkyBegin(*this, Light);")
    expect("the battle look's sky begun and never ended fails", paired, True)
    closed = dict(paired)
    closed["Battle/SeaBattle.cpp"] = paired["Battle/SeaBattle.cpp"].replace("void ASeaBattle::GiveBackTheRoom()\n{", "void ASeaBattle::GiveBackTheRoom()\n{\n\tBattleLook::AirSkyEnd();")
    expect("the battle look's sky begun and ended passes", closed, False)
    print(f"room_scan self-test: {count} cases, {len(bad)} failed")
    return not bad


def main(argv):
    if "--self-test" in argv:
        return 0 if self_test() else 1
    print("room_scan: every setter the sea's code calls on the room's lamps, sky light, fog and atmosphere")
    listed, failures = scan(Source())
    missing = [f for f in FLOOR if f not in listed]
    for rel, kind, field in missing:
        print(f"  FAIL the scan no longer finds {kind} {field} in {rel} (it would pass by finding nothing)")
    print(f"room_scan: {len(listed)} setters, {len(failures) + len(missing)} not given back or not found")
    return 0 if not failures and not missing else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
