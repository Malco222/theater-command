#!/usr/bin/env python3
"""M_AirPaint's nodes against AirSheen's law (row flyB17Sheen, the B-17 paint sheen).

Runs Tools/build_content.py's own "if air:" block (the nodes of M_AirPaint) on stand-in node functions, as plain
numbers, and compares its roughness, specular and metal with AirSheen::Law and AirSheen::GuardOf as
air_b17_sheen_grid prints them, case for case. So a node wired to the wrong pin, two inputs swapped or a constant
changed on one side alone fails here, where air_b17_sheen_test's S6 (names, defaults and constants) would pass.
The parameters the game never sets (SheenEdge, SheenLowSun, SheenGlassBelow) take the script's own defaults, so they
are checked against AirSheen::Tune's too. Also: nothing carried (U = V = 0) is the ink exactly.
No Unreal here: "unreal" is a stand-in that only names node kinds. Usage:
    air_b17_sheen_grid > grid.txt
    python3 -I -B Tools/air_show/air_b17_sheen_material.py Tools/build_content.py grid.txt
"""
import sys
import textwrap


class Kind:
    def __init__(self, name):
        self.name = name


class Unreal:
    """The stand-in for the unreal module: unreal.MaterialExpressionX is a kind named X."""

    def __getattr__(self, name):
        return Kind(name)

    class MaterialProperty:
        MP_ROUGHNESS = "rough"
        MP_SPECULAR = "spec"
        MP_METALLIC = "metal"


class Node:
    def __init__(self, kind, **props):
        self.kind = kind
        self.ins = {}
        self.consts = {}
        self.props = props
        self.param = None
        self.default = None

    def set_editor_property(self, key, value):
        self.props[key] = value


def block_of(src):
    start = src.index("\n    if air:\n        # M_AirPaint")
    end = src.index("\n    finish(m)", start)
    return textwrap.dedent(src[start + 1:end])


def build(src):
    """The block run on stand-in nodes: the nodes behind roughness, specular and metal."""
    out = {}

    def node(m, kind, x, y, **props):
        return Node(kind.name, **props)

    def scalar(m, name, default, x, y):
        n = Node("Scalar")
        n.param, n.default = name, default
        return n

    def vector(m, name, default, x, y):
        n = Node("Vector")
        n.param, n.default = name, default
        return n

    def connect(s, o, t, pin):
        t.ins[pin] = (s, o)

    def _feed(m, t, pin, v):
        if isinstance(v, (int, float)):
            t.consts[pin] = float(v)
        else:
            s, o = v if isinstance(v, tuple) else (v, "")
            connect(s, o, t, pin)

    def one_in(m, kind, s, x=0, y=0):
        n = Node(kind.name)
        connect(*(s if isinstance(s, tuple) else (s, "")), n, "")
        return n

    def node_from(m, kind, s, x, y, **props):
        n = Node(kind.name, **props)
        connect(*(s if isinstance(s, tuple) else (s, "")), n, "")
        return n

    def two(name):
        def make(m, a, b, x=0, y=0):
            n = Node(name)
            _feed(m, n, "A", a)
            _feed(m, n, "B", b)
            return n
        return make

    def lerp(m, a, b, t, x=0, y=0):
        n = Node("MaterialExpressionLinearInterpolate")
        _feed(m, n, "A", a)
        _feed(m, n, "B", b)
        _feed(m, n, "Alpha", t)
        return n

    def to_property(s, o, prop):
        out[prop] = s

    chipped = Node("Const")
    chipped.default = 0.0
    env = dict(unreal=Unreal(), node=node, scalar=scalar, vector=vector, connect=connect, _feed=_feed, one_in=one_in,
               node_from=node_from, times=two("MaterialExpressionMultiply"), plus=two("MaterialExpressionAdd"), lerp=lerp,
               to_property=to_property, chipped=chipped, m=None, air=True)
    exec(compile(block_of(src), "build_content.py if-air block", "exec"), env)
    return out


def sat(x):
    return tuple(sat(p) for p in x) if isinstance(x, tuple) else min(max(x, 0.0), 1.0)


def op(f, a, b):
    if isinstance(a, tuple) and isinstance(b, tuple):
        if len(a) != len(b):
            raise ValueError("vector sizes differ")
        return tuple(f(p, q) for p, q in zip(a, b))
    if isinstance(a, tuple):
        return tuple(f(p, b) for p in a)
    if isinstance(b, tuple):
        return tuple(f(a, q) for q in b)
    return f(a, b)


def ev(n, ctx, memo):
    if id(n) in memo:
        return memo[id(n)]

    def pin(p):
        if p in n.ins:
            return ev(n.ins[p][0], ctx, memo)
        if p in n.consts:
            return n.consts[p]
        raise KeyError("%s: pin %s not fed" % (n.kind, p))
    k = n.kind
    if k == "Scalar":
        r = ctx["params"].get(n.param, n.default)
    elif k == "Vector":
        r = tuple(ctx["params"].get(n.param, n.default)[:3])  # (its "" output taken as its RGB)
    elif k == "Const":
        r = n.default
    elif k == "MaterialExpressionTextureCoordinate":
        r = (ctx["U"], ctx["V"])
    elif k == "MaterialExpressionVertexNormalWS":
        r = ctx["N"]
    elif k == "MaterialExpressionCameraVectorWS":
        r = ctx["E"]
    elif k == "MaterialExpressionComponentMask":
        s = pin("")
        picks = [c for c, key in enumerate("rgba") if n.props.get(key)]
        r = s[picks[0]] if len(picks) == 1 else tuple(s[c] for c in picks)
    elif k == "MaterialExpressionMultiply":
        r = op(lambda p, q: p * q, pin("A"), pin("B"))
    elif k == "MaterialExpressionAdd":
        r = op(lambda p, q: p + q, pin("A"), pin("B"))
    elif k == "MaterialExpressionMax":
        r = op(max, pin("A"), pin("B"))
    elif k == "MaterialExpressionLinearInterpolate":
        a, b, t = pin("A"), pin("B"), pin("Alpha")
        r = a + (b - a) * t
    elif k == "MaterialExpressionSaturate":
        r = sat(pin(""))
    elif k == "MaterialExpressionOneMinus":
        r = op(lambda p, q: p - q, 1.0, pin(""))
    elif k == "MaterialExpressionDotProduct":
        a, b = pin("A"), pin("B")
        if len(a) != len(b):
            raise ValueError("dot of two sizes")
        r = sum(p * q for p, q in zip(a, b))
    else:
        raise ValueError("a node kind this check does not know: %s (teach it, do not skip it)" % k)
    memo[id(n)] = r
    return r


def main(argv):
    if len(argv) != 2:
        print(__doc__, file=sys.stderr)
        return 2
    with open(argv[0]) as f:
        out = build(f.read())
    fails = []
    if sorted(out) != ["metal", "rough", "spec"]:
        fails.append("the block feeds %s, not roughness, specular and metal" % sorted(out))
        print("air_b17_sheen_material: FAILED", fails)
        return 1

    def material(tune, ink, s, e, n, u, v):
        params = {"Roughness": ink[0], "Specular": ink[1], "SheenGain": tune[0], "SheenFloor": tune[1], "SheenSpec": tune[2],
                  "SheenMetal": tune[3], "SheenMetalFrom": tune[4], "SheenGlare": tune[5], "SheenGraze": tune[6],
                  "SheenSun": (s[0], s[1], s[2], 0.0)}
        ctx = dict(U=u, V=v, N=n, E=e, params=params)
        memo = {}
        return ev(out["rough"], ctx, memo), ev(out["spec"], ctx, memo), ev(out["metal"], ctx, memo)

    worst, cases, plain, plain_bad = 0.0, 0, 0, 0
    with open(argv[1]) as f:
        for line in f:
            x = [float(t) for t in line.split()]
            if len(x) != 24:
                fails.append("a grid line of %d numbers" % len(x))
                break
            tune, ink, s, e, n, u, v = x[0:7], x[7:9], x[9:12], x[12:15], x[15:18], x[18], x[19]
            r, sp, mt = material(tune, ink, tuple(s), tuple(e), tuple(n), u, v)
            worst = max(worst, abs(r - x[21]), abs(sp - x[22]), abs(mt - x[23]))
            cases += 1
            if u == 0.0 and v == 0.0:
                plain += 1
                plain_bad += 0 if (r, sp, mt) == (ink[0], ink[1], 0.0) else 1
    ok = cases > 0 and worst <= 1e-9
    print("  %s M_AirPaint's nodes give AirSheen::Law and GuardOf's numbers: %d cases, worst difference %.3g" % ("ok  " if ok else "FAIL", cases, worst))
    if not ok:
        fails.append("the material and the law differ")
    ok = plain > 0 and plain_bad == 0
    print("  %s nothing carried (U = V = 0) is the ink exactly in the nodes: %d of %d cases otherwise" % ("ok  " if ok else "FAIL", plain_bad, plain))
    if not ok:
        fails.append("U = V = 0 is not the ink")
    print("air_b17_sheen_material: %s" % ("FAILED" if fails else "ok"))
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
