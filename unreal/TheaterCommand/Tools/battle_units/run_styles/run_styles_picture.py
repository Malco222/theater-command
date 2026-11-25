"""Pictures of the run styles, in the game's colours (faces/facecam.py's look and raster):

    python3 run_styles_picture.py OUT_DIR [soldier_USA] [STAGE_DIR]

  runs_side_by_side.jpg   his own run and the three new ones, at the same moments, running at the same speed
  squad_before_after.gif  six men running together at 3 m/s: before (one run for all, as the game drew it, each man
                          only starting at another point in it) and after (each man his own runner, as LandMen picks
                          them with the switch on: so his own stride, and at one speed his own beat)

STAGE_DIR (optional): take the new clips from STAGE_DIR/<name>.run_styles.json rather than the shared file.
"""
import json
import math
import os
import sys

import numpy as np
from PIL import Image, ImageDraw, ImageFont

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, "..", "faces"))
import run_styles as R  # noqa: E402

_argv = list(sys.argv)
sys.argv = ["x"]
import facecam  # noqa: E402
from faceview import raster  # noqa: E402
sys.argv = _argv

SHARED = R.SHARED
STYLES = ["run", "run_b", "run_c", "run_d"]
LABEL = {"run": "his own run (CMU 35_18)", "run_b": "run_b: long, driving stride (CMU 09_02)",
         "run_c": "run_c: short, quick, bouncy (CMU 16_36)", "run_d": "run_d: high knees (CMU 09_03)"}
V = 3.0                      # (the squad's speed over the ground, m/s)
try:
    FONT = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
    SMALL = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 12)
except Exception:
    FONT = SMALL = None


def unit01(x):
    x = (x ^ (x >> 33)) * 0xFF51AFD7ED558CCD & 0xFFFFFFFFFFFFFFFF
    x = (x ^ (x >> 33)) * 0xC4CEB9FE1A85EC53 & 0xFFFFFFFFFFFFFFFF
    x ^= x >> 33
    return (x >> 11) / float(1 << 53)


class Man:
    def __init__(self, D, row, lod, face):
        self.D = D
        self.sk = R.Skel(D["bones"])
        self.P0, self.N0, self.T, self.C = facecam.look(D, row, lod, face, True)
        m = D[lod]
        self.a = np.asarray(m["part"], int)
        self.b = np.asarray(m["part2"], int)
        self.w = np.asarray(m["w2"], float)[:, None]

    def posed(self, clip, t):
        G, X = self.sk.fk(*R.clip_pose(self.D["clips"][clip], t))
        H = self.sk.H0
        pa = X[self.a] + np.einsum("nij,nj->ni", G[self.a], self.P0 - H[self.a])
        pb = X[self.b] + np.einsum("nij,nj->ni", G[self.b], self.P0 - H[self.b])
        P = pa * (1 - self.w) + pb * self.w
        N = np.einsum("nij,nj->ni", G[self.a], self.N0)
        return P, N


def scene(parts, size, scale, centre, yaw, down=10.0):
    """Men (P, N, T, C, offset) together, turned `yaw` degrees and seen `down` degrees from level, one raster."""
    a = math.radians(yaw)
    Rz = np.array(((math.cos(a), -math.sin(a), 0.0), (math.sin(a), math.cos(a), 0.0), (0.0, 0.0, 1.0)))
    b = math.radians(down)
    Rz = np.array(((1.0, 0.0, 0.0), (0.0, math.cos(b), -math.sin(b)), (0.0, math.sin(b), math.cos(b)))) @ Rz
    Ps, Ns, Ts, Cs, n = [], [], [], [], 0
    for P, N, T, C, off in parts:
        Ps.append((P + np.asarray(off)) @ Rz.T)
        Ns.append(N @ Rz.T)
        Ts.append(T + n)
        Cs.append(C)
        n += len(P)
    return raster(np.concatenate(Ps), np.concatenate(Ns), np.concatenate(Ts), np.concatenate(Cs), "side",
                  size=size, scale=scale, centre=centre)


def ground(x0, x1, y0, y1, shift, step=0.5):
    """Ground marks moving back under the men: dark and light strips, as triangles."""
    P, T, C = [], [], []
    x = x0 - (shift % (2 * step))
    k = 0
    while x < x1:
        col = (96, 104, 84) if k % 2 == 0 else (118, 126, 102)
        i = len(P)
        P += [(x, y0, -0.002), (x + step, y0, -0.002), (x + step, y1, -0.002), (x, y1, -0.002)]
        T += [(i, i + 1, i + 2), (i, i + 2, i + 3)]
        C += [col] * 4
        x += step
        k += 1
    P = np.array(P, float)
    return P, np.tile((0.0, 0.0, 1.0), (len(P), 1)), np.array(T), np.array(C, float), (0.0, 0.0, 0.0)


def load(name, stage):
    D = json.load(open(os.path.join(SHARED, name + ".json")))
    if stage:
        D["clips"].update(json.load(open(os.path.join(stage, name + ".run_styles.json"))))
    return D


def side_by_side(D, out, row="USA"):
    man = Man(D, row, "near", 4)
    cols = 6
    tw, th = 150, 230
    sheet = Image.new("RGB", (tw * cols + 190, th * len(STYLES) + 34), (236, 233, 226))
    d = ImageDraw.Draw(sheet)
    d.text((8, 8), f"The same man, four runs, at the same moments (every 0.1 s), all at {V:.0f} m/s", fill=(20, 20, 20),
           font=FONT)
    for r_, st in enumerate(STYLES):
        c = D["clips"][st]
        for k in range(cols):
            t = k * 0.1
            ph = (t * V / c["stride"]) % 1.0
            P, N = man.posed(st, ph * c["long"])
            g = ground(-0.9, 0.9, -0.5, 0.5, V * t)
            img = scene([(P, N, man.T, man.C, (0, 0, 0)), g], 2 * th, 2 * th / 2.15, (0.0, 0.0, 0.92), -28.0)
            im = Image.fromarray(img).resize((th, th), Image.LANCZOS).crop(((th - tw) // 2, 0, (th - tw) // 2 + tw, th))
            sheet.paste(im, (190 + k * tw, 34 + r_ * th))
        d.text((8, 34 + r_ * th + th // 2 - 20), "\n".join(LABEL[st].split(": ")), fill=(20, 20, 20), font=SMALL)
        d.text((8, 34 + r_ * th + th // 2 + 14), f"stride {c['stride']:.2f} m\n{2 * V / c['stride']:.1f} steps a second",
               fill=(70, 70, 70), font=SMALL)
    sheet.save(out, quality=86)


def squad(D, out, row="USA", seed=11, frames=30, fps=15, picks=(0, 1, 2, 3, 1, 2)):
    man = Man(D, row, "mid", 0)
    men = []
    for k in range(6):
        who = seed * 1000 + k
        y = (k - 2.5) * 1.6
        x = 0.6 * math.sin(k * 2.1) - 0.15 * k
        phase = unit01(who ^ 0x57E9)
        # (the game picks by his seed, Unit01(seed ^ 0x5717E5); here all four runners are put in the six for the
        # picture's sake)
        men.append(dict(x=x, y=y, phase=phase, style=STYLES[picks[k] % len(STYLES)]))
    ims = []
    W, H = 760, 300
    for f in range(frames):
        t = f / fps
        panels = []
        for after in (False, True):
            parts = []
            for m in men:
                st = m["style"] if after else "run"
                c = D["clips"][st]
                ph = (m["phase"] + t * V / c["stride"]) % 1.0
                P, N = man.posed(st, ph * c["long"])
                parts.append((P, N, man.T, man.C, (m["x"], m["y"], 0.0)))
            parts.append(ground(-12.0, 12.0, -12.0, 12.0, V * t, step=0.75))
            img = scene(parts, 2 * W, 2 * W / 10.0, (0.0, 0.0, 0.75), -62.0, down=18.0)
            im = Image.fromarray(img).resize((W, W), Image.LANCZOS)
            im = im.crop((0, W // 2 - H // 2 - 30, W, W // 2 + H // 2 - 30))
            d = ImageDraw.Draw(im)
            d.rectangle((0, 0, W, 22), fill=(30, 30, 30))
            d.text((8, 3), "AFTER: each man his own runner (his own stride and beat)" if after else
                   "BEFORE: one run for everyone (each man only starts at another point in it)", fill=(240, 240, 240), font=SMALL)
            panels.append(np.array(im))
        ims.append(Image.fromarray(np.concatenate(panels, 0)).convert("P", palette=Image.ADAPTIVE, colors=96))
    ims[0].save(out, save_all=True, append_images=ims[1:], duration=int(1000 / fps), loop=0, optimize=True)


if __name__ == "__main__":
    out_dir = sys.argv[1]
    name = sys.argv[2] if len(sys.argv) > 2 else "soldier_USA"
    stage = sys.argv[3] if len(sys.argv) > 3 else None
    os.makedirs(out_dir, exist_ok=True)
    D = load(name, stage)
    side_by_side(D, os.path.join(out_dir, "runs_side_by_side.jpg"))
    squad(D, os.path.join(out_dir, "squad_before_after.gif"))
