"""Move an aircraft model's wing markings (Content/Models/<name>.json, its "marks" of place wing) to a new span station,
chord place and size, laid on the skin as the model's maker lays them: 12 mm off the skin along the skin's facing
there, facing that way. Only the "marks" list of the file is rewritten; every other byte stays as it was.

    python3 Tools/lay_wing_marks.py Content/Models/Bf109.json --up 3.60 --upr 0.475 --low 3.55 --lowr 0.45 [--frac 0.5] [--dry]

--up/--low: metres from the centre line to the middle of the upper/lower marking (each side keeps its own hand);
--upr/--lowr: the marking's radius; --frac: the middle's place along the local chord, 0 the trailing edge, 1 the
leading edge (0.5, mid-chord, is where the period's markings sat). It prints the local chord and how much of each
marking's breadth that takes.
"""
import json, numpy as np


def tris_at_y(P, T, y):
    A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    lo = np.minimum(np.minimum(A[:, 1], B[:, 1]), C[:, 1]); hi = np.maximum(np.maximum(A[:, 1], B[:, 1]), C[:, 1])
    return T[(lo <= y + 1e-9) & (hi >= y - 1e-9)]


def hits(P, T, x, y):
    if len(T) == 0:
        return []
    A, B, C = P[T[:, 0]], P[T[:, 1]], P[T[:, 2]]
    v0 = C[:, :2] - A[:, :2]; v1 = B[:, :2] - A[:, :2]; v2 = np.array([x, y]) - A[:, :2]
    d00 = (v0 * v0).sum(1); d01 = (v0 * v1).sum(1); d11 = (v1 * v1).sum(1); d20 = (v2 * v0).sum(1); d21 = (v2 * v1).sum(1)
    den = d00 * d11 - d01 * d01
    ok = np.abs(den) > 1e-14
    sden = np.where(ok, den, 1.0)
    u = (d11 * d20 - d01 * d21) / sden; v = (d00 * d21 - d01 * d20) / sden
    inside = ok & (u >= -1e-9) & (v >= -1e-9) & (u + v <= 1 + 1e-9)
    out = []
    for i in np.nonzero(inside)[0]:
        z = A[i, 2] + u[i] * (C[i, 2] - A[i, 2]) + v[i] * (B[i, 2] - A[i, 2])
        nrm = np.cross(B[i] - A[i], C[i] - A[i]); nrm = nrm / (np.linalg.norm(nrm) + 1e-12)
        out.append((float(z), nrm))
    return out


def skin(P, T, x, y, upper):
    h = [(z, n) for z, n in hits(P, T, x, y) if (n[2] > 0.3 if upper else n[2] < -0.3)]
    if not h:
        return None
    return max(h, key=lambda q: q[0]) if upper else min(h, key=lambda q: q[0])


def chord_at(P, T, y, upper=True, step=0.01):
    """Leading and trailing edge x at span station y: the longest unbroken run of x where the upper (or lower) skin is
    crossed (the wing, not the fuselage or a nacelle)."""
    Ty = tris_at_y(P, T, y)
    run = [x for x in np.arange(P[:, 0].min(), P[:, 0].max(), step) if skin(P, Ty, x, y, upper) is not None]
    if not run:
        return None
    runs, start = [], run[0]
    for a, b in zip(run, run[1:]):
        if b - a > step * 1.5:
            runs.append((start, a)); start = b
    runs.append((start, run[-1]))
    return max(runs, key=lambda r: r[1] - r[0])


def splice_marks(path, marks, out):
    raw = open(path).read()
    key = '"marks":'
    i = raw.index(key) + len(key)
    depth, j = 0, i
    while True:
        ch = raw[j]
        if ch == '[':
            depth += 1
        elif ch == ']':
            depth -= 1
            if depth == 0:
                break
        j += 1
    text = raw[:i] + json.dumps(marks, separators=(',', ':')) + raw[j + 1:]
    json.loads(text)
    with open(out, 'w') as fh:
        fh.write(text)


def move(path, upper_y=None, lower_y=None, upper_r=None, lower_r=None, frac=0.5, out=None, dry=False, hair=0.012):
    d = json.load(open(path))
    P = np.array(d['near']['points'], float).reshape(-1, 3)
    T = np.array(d['near']['triangles'], int).reshape(-1, 3)
    report = []
    for m in d['marks']:
        f = m['facing']
        if abs(f[2]) < 0.5 or m.get('place') not in (None, 'wing'):
            continue
        up = f[2] > 0
        new_y, new_r = (upper_y, upper_r) if up else (lower_y, lower_r)
        if new_y is None:
            continue
        y = (1.0 if m['at'][1] > 0 else -1.0) * new_y
        c = chord_at(P, T, y, up)
        if c is None:
            report.append(f"{m['tag']} y {y:+.2f}: no wing there, left as it was")
            continue
        x = c[0] + frac * (c[1] - c[0])
        s = skin(P, tris_at_y(P, T, y), x, y, up)
        if s is None:
            report.append(f"{m['tag']} y {y:+.2f}: no skin, left as it was")
            continue
        z, n = s
        r = new_r if new_r is not None else m['radius']
        old = (m['at'][1], m['at'][0], m['radius'])
        m['at'] = [round(float(v), 3) for v in (x + hair * n[0], y + hair * n[1], z + hair * n[2])]
        m['facing'] = [round(float(v), 3) for v in n]
        m['radius'] = round(r, 3)
        report.append(f"{m['tag']:9s} {'upper' if up else 'lower'} y {old[0]:+.2f} -> {y:+.2f}, x {old[1]:+.2f} -> {x:+.2f}, "
                      f"radius {old[2]} -> {r}; chord {c[0]:+.2f} to {c[1]:+.2f} ({c[1] - c[0]:.2f} m), the marking {200 * r / (c[1] - c[0]):.0f}% of it")
    if not dry:
        splice_marks(path, d['marks'], out or path)
    return report


if __name__ == '__main__':
    import argparse
    a = argparse.ArgumentParser()
    a.add_argument('path'); a.add_argument('--up', type=float); a.add_argument('--low', type=float)
    a.add_argument('--upr', type=float); a.add_argument('--lowr', type=float); a.add_argument('--frac', type=float, default=0.5)
    a.add_argument('--out'); a.add_argument('--dry', action='store_true')
    o = a.parse_args()
    for line in move(o.path, o.up, o.low, o.upr, o.lowr, o.frac, o.out, o.dry):
        print(line)
