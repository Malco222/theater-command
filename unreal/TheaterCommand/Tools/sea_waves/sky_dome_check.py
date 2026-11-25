#!/usr/bin/env python3
"""M_SkyDome, held to the cloud dome the game lays (the release 2 ocean plan, step 13).

Runs Tools/build_content.py through the stand-in Unreal module (fake_unreal.py, as graph_pin.py runs it) and checks the
material the fleet's cloud dome is drawn with:
  - it is see-through, unlit and two-sided, as a dome seen from inside must be;
  - it reads its picture by the way the eye looks (the engine's camera vector), not by the dome's own quads, so where
    its 48 by 12 quads fall never shows;
  - across the picture is the bearing (an arctangent of two, over a turn, its fraction taken) and down it one less the
    sine of the height (a normalize and a one-minus), as SeaSky::DomeUv works them out;
  - the picture's light is doubled, the sheet of cloud shapes' own measure (SeaSky::Dome keeps half the light);
  - the clouds are carried by "Drift" and read at "LayerHeight";
  - every parameter Battle/SeaWeather.cpp sets on it is one the material has, and every parameter the material has is
    one the game sets (so nothing is set by a name the material does not know, which the engine would quietly ignore).
It also checks the same for the parameters the game sets on M_DeckShade, which the clouds' shade is laid with.

Usage (from the project, TheaterCommand): python3 -I Tools/sea_waves/sky_dome_check.py
"""

import os
import re
import sys

sys.dont_write_bytecode = True

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
SRC = os.path.join(ROOT, "Source", "TheaterCommand")

import importlib.util

_spec = importlib.util.spec_from_file_location("graph_pin_for_dome", os.path.join(HERE, "graph_pin.py"))
graph_pin = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(graph_pin)


class Checks:
    def __init__(self):
        self.count = 0
        self.failed = []

    def check(self, what, ok, detail=""):
        self.count += 1
        if not ok:
            self.failed.append(what)
        print(("  ok   " if ok else "  FAIL ") + what + (": " + detail if detail else ""))


def material(fake, name):
    for asset in fake.ASSETS:
        if getattr(asset, "_asset", None) and asset._asset[0] == name:
            return asset
    return None


def kinds(asset):
    return [n._kind for n in asset._nodes]


def parameters(asset):
    """The material's parameters by name: name -> the kind of node that holds it."""
    out = {}
    for node in asset._nodes:
        name = node._props.get("parameter_name")
        if name:
            out[str(name)] = node._kind
    return out


def set_in_source(rel, material_names):
    """The parameters the game sets, by the material instance each is set on: {variable: {name, ...}}.

    A line like DomeInk->SetScalarParameterValue(TEXT("DayLight"), ...) or Mid->SetTextureParameterValue(TEXT("Picture"),
    ...) inside a function that made its instance from a material gives that material's parameters. The variable a
    function's instance is kept in is found from the Create call (UMaterialInstanceDynamic::Create(Ink, ...)), and the
    material from the LoadObject path above it.
    """
    with open(os.path.join(SRC, rel), encoding="utf-8") as f:
        text = f.read()
    out = {name: set() for name in material_names}
    # the file cut into functions at each definition at the head of a line
    marks = [m.start() for m in re.finditer(r"^\w[\w:<>\*&, ]*::\w+\(", text, re.M)]
    marks.append(len(text))
    for k in range(len(marks) - 1):
        body = text[marks[k]:marks[k + 1]]
        for name in material_names:
            if ("/" + name + "." + name) not in body:
                continue
            for m in re.finditer(r"Set(?:Scalar|Vector|Texture)ParameterValue\s*\(\s*TEXT\(\"(\w+)\"\)", body):
                out[name].add(m.group(1))
    return out


def main():
    checks = Checks()
    fake = graph_pin.run_build("sky-dome")
    dome = material(fake, "M_SkyDome")
    checks.check("Tools/build_content.py builds M_SkyDome", dome is not None)
    if dome is None:
        print("sky_dome_check: %d checks, %d failed" % (checks.count, len(checks.failed)))
        return 1
    props = dome._props
    blend = props.get("blend_mode")
    shading = props.get("shading_model")
    checks.check("it is see-through, unlit and two-sided",
                 getattr(blend, "name", "") == "BLEND_TRANSLUCENT" and getattr(shading, "name", "") == "MSM_UNLIT"
                 and props.get("two_sided") is True,
                 "%s, %s, two-sided %s" % (getattr(blend, "name", "?"), getattr(shading, "name", "?"), props.get("two_sided")))
    had = kinds(dome)
    checks.check("the picture is read by the way the eye looks, not by the dome's quads",
                 "MaterialExpressionCameraVectorWS" in had and "MaterialExpressionTextureCoordinate" not in had,
                 ", ".join(k[len("MaterialExpression"):] for k in had if "Camera" in k or "TextureCoordinate" in k) or "none")
    checks.check("across the picture is the bearing over a turn: an arctangent of two, its fraction taken",
                 "MaterialExpressionArctangent2" in had and "MaterialExpressionFrac" in had)
    checks.check("down it is one less the sine of the height: the way made a metre long, its up taken from one",
                 "MaterialExpressionNormalize" in had and "MaterialExpressionOneMinus" in had)
    doubled = [n for n in dome._nodes if n._kind == "MaterialExpressionMultiply" and 2.0 in (n._props.get("const_a"), n._props.get("const_b"))]
    checks.check("the picture's light is doubled, the sheet of cloud shapes' own measure", bool(doubled),
                 "%d node(s) times two" % len(doubled))
    have = parameters(dome)
    wanted = {"Picture", "Brightness", "DayLight", "LayerHeight", "Drift"}
    checks.check("it has the parameters the dome is drawn by", wanted <= set(have),
                 ", ".join(sorted(have)) or "none")
    checks.check("its picture is read as plain numbers, not as colours",
                 have.get("Picture") == "MaterialExpressionTextureSampleParameter2D"
                 and any(getattr(n._props.get("sampler_type"), "name", "").endswith(("MASKS", "LINEAR_COLOR"))
                         for n in dome._nodes if str(n._props.get("parameter_name")) == "Picture"),
                 str(next((getattr(n._props.get("sampler_type"), "name", "?") for n in dome._nodes
                           if str(n._props.get("parameter_name")) == "Picture"), "?")))
    # What the game sets, against what each material has.
    shade = material(fake, "M_DeckShade")
    checks.check("Tools/build_content.py builds M_DeckShade, which the clouds' shade is laid with", shade is not None)
    sets = set_in_source("Battle/SeaWeather.cpp", ["M_SkyDome", "M_DeckShade", "M_Highlight"])
    for name, asset in (("M_SkyDome", dome), ("M_DeckShade", shade)):
        if asset is None:
            continue
        mine = parameters(asset)
        asked = sets.get(name, set())
        checks.check("every parameter Battle/SeaWeather.cpp sets on %s is one the material has" % name,
                     asked and asked <= set(mine), ", ".join(sorted(asked - set(mine))) or ", ".join(sorted(asked)))
    mine = parameters(dome)
    checks.check("and every parameter M_SkyDome has is one the game sets", set(mine) <= sets.get("M_SkyDome", set()),
                 ", ".join(sorted(set(mine) - sets.get("M_SkyDome", set()))) or "all set")
    print("sky_dome_check: %d checks, %d failed" % (checks.count, len(checks.failed)))
    return 0 if not checks.failed else 1


if __name__ == "__main__":
    sys.exit(main())
