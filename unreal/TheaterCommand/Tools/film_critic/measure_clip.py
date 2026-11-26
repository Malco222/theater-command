#!/usr/bin/env python3
"""
Film critic: measures a short clip for the kinds of movement faults a viewer notices
without being told where to look. It judges no art; it only counts:

  jump    the whole picture lurches further in one frame than the frames around it move
          (a camera that skips, a view that snaps)
  stutter while the view (or a part of it, such as the plane the camera follows) is moving, one
          frame's move differs from the running median of the frames around it by more than
          STUTTER_X of the usual move and by more than STUTTER_FLOOR px (on a 1920-wide picture):
          movement that comes in steps instead of evenly ("choppy", "jumpy fighters")
  zoom    the whole picture grows or shrinks by more than ZOOM_POP in one frame, beyond what the
          frames around it do (a zoom that flips: what a viewer calls "flicker" in a moving shot)
  pop     one part of the picture changes far more in one frame than it does before or after
          (a plane or a man appearing or vanishing, a shape swapped for another)
  freeze  the picture stands still for several frames while the clip should be moving
  flicker the whole picture brightens or darkens in one frame and comes back
  blank   a black, white or empty frame
  cut     the whole picture is replaced (a change of shot): not a fault by itself, but the
          frames either side are not compared with each other
  repeat  (a reading, not a fault) a frame identical to the one before while the clip moves. In a
          screen recording the recorder makes these; with --merge-repeats (the default for a video
          file) a repeated frame and the next share their move before stutter is judged.

How motion is read: each frame is compared with the one before at MOTION_W wide by phase
correlation with a low-pass weight (so the 8-pixel block grid of JPEG/H.264 does not make false
peaks), to a tenth of a pixel, over the whole picture and over a GRID of tiles. The view's move is
a fit of the agreeing tiles (move, zoom and roll); the tiles' own moves let a part of the picture
(the followed plane) be judged on its own. All sizes in the output are in pixels of the source,
and `*_1920` fields are the same on a 1920-wide picture (the scale the team's jump rule uses).

Input: a folder of numbered pictures (jpg/png, played in name order) or a video file.
Output: a JSON file of every frame-to-frame reading and the faults found, and a contact
sheet (a grid of small frames, faulty ones outlined in red and named) for the morning report.

Only numpy and Pillow are needed (ffmpeg too for a video). Usage:
  measure_clip.py FRAMES_DIR_OR_VIDEO --out RESULT.json [--sheet SHEET.jpg] [--fps 10]
                  [--times TIMES.json] [--expect-motion] [--label "raid opening"]
                  [--merge-repeats | --no-merge-repeats] [--stutter-x 0.5] [--stutter-floor 4]
See CHECKER.md for why each limit is what it is (tuned on three real game recordings).
"""
import argparse, json, math, os, subprocess, sys, tempfile
import numpy as np
from PIL import Image, ImageDraw, ImageFont

WORK_W = 320    # pops, flicker, freeze and cuts are read at this width: enough for a lurch, cheap enough for hundreds
MOTION_W = 640  # motion (jump, stutter, zoom) is read at this width, to a tenth of a pixel
GRID = (3, 4)   # rows, columns of tiles for the parts of the picture

# Limits. Pixel limits are on a 1920-wide picture (scaled to the clip's own width).
DEFAULTS = {
    "stutter_x": 0.5,       # a frame's move may differ from the running median by up to half the usual move
    "stutter_floor": 4.0,   # ... and a difference must be over 4 px on 1920 to count (under it nobody sees it)
    "move_floor": 1.0,      # the view or a part counts as moving when it moves 1 px on 1920 a frame on average
    "window": 5,            # frames each side of a frame that make its running median
    "zoom_pop": 0.08,       # the picture's size changes over 8% in one frame beyond what the frames around do
    "tile_conf": 10.0,      # a tile's reading counts when its correlation peak stands this many spreads above the rest
    "tile_two": 0.35,       # a part is judged on its own only while it holds ONE motion: the next-best correlation
                            # peak under 0.35 of the best in 4 frames of 5 around it (one motion reads 0.18-0.33;
                            # men running over grass that pans read 0.35-0.6 and flip between the two moves)
    "tile_texture": 2.5,    # ... and the tile has detail (mean grey step between neighbouring pixels, x plus y, over 2.5):
                            # sky, haze and flat sea are skipped, where only the compression's block grid would be read
}
LOWPASS = 0.06  # cycles a pixel at MOTION_W: the correlation keeps shapes over ~16 px and drops the 8 px block grid


def load_frames(src):
    if os.path.isdir(src):
        names = sorted(n for n in os.listdir(src) if n.lower().endswith((".jpg", ".jpeg", ".png")))
        return [os.path.join(src, n) for n in names], None
    tmp = tempfile.mkdtemp(prefix="clip-")
    subprocess.run(["ffmpeg", "-loglevel", "error", "-i", src, "-vf", f"scale={WORK_W * 2}:-2", "-q:v", "2",
                    os.path.join(tmp, "f%05d.jpg")], check=True)
    names = sorted(os.listdir(tmp))
    return [os.path.join(tmp, n) for n in names], tmp


def grey_at(im, width):
    w, h = im.size
    width = min(width, w)
    small = im.resize((width, max(8, round(h * width / w))), Image.BILINEAR)
    return np.asarray(small, dtype=np.float32)


def grey(path):
    im = Image.open(path).convert("L")
    return grey_at(im, WORK_W), grey_at(im, MOTION_W), im.size


class Correlator:
    """Phase correlation with a Hann window and a low-pass weight, cached per picture shape."""

    def __init__(self):
        self.cache = {}

    def parts(self, shape):
        if shape not in self.cache:
            h, w = shape
            win = np.outer(np.hanning(h), np.hanning(w)).astype(np.float32)
            fy = np.fft.fftfreq(h)[:, None]
            fx = np.fft.fftfreq(w)[None, :]
            lp = np.exp(-(fx ** 2 + fy ** 2) / (2 * LOWPASS ** 2)).astype(np.float32)
            self.cache[shape] = (win, lp)
        return self.cache[shape]

    def spectrum(self, a):
        win, _ = self.parts(a.shape)
        return np.fft.fft2((a - a.mean()) * win)

    def shift(self, fa, fb, shape, second=False):
        """How far picture b is moved from a (pixels), and how sure (the peak in spreads above the surface).
        With second=True also the height of the next-best peak as a share of the best (two motions in one
        picture, such as men running across grass that pans, give two peaks of a like height)."""
        _, lp = self.parts(shape)
        cross = fa * np.conj(fb)
        cross /= np.abs(cross) + 1e-9
        r = np.real(np.fft.ifft2(cross * lp))
        py, px = np.unravel_index(np.argmax(r), r.shape)
        h, w = r.shape
        conf = float((r[py, px] - r.mean()) / (r.std() + 1e-12))

        def sub(m, c, p):
            den = m - 2 * c + p
            return 0.0 if abs(den) < 1e-12 else max(-0.5, min(0.5, 0.5 * (m - p) / den))

        oy = sub(r[(py - 1) % h, px], r[py, px], r[(py + 1) % h, px])
        ox = sub(r[py, (px - 1) % w], r[py, px], r[py, (px + 1) % w])
        dy, dx = py + oy, px + ox
        if dy > h / 2:
            dy -= h
        if dx > w / 2:
            dx -= w
        if not second:
            return -float(dx), -float(dy), conf
        rr = np.roll(np.roll(r, h // 2 - py, 0), w // 2 - px, 1)
        base = float(np.median(rr))
        rr[h // 2 - 4:h // 2 + 5, w // 2 - 4:w // 2 + 5] = -np.inf
        ratio = (float(rr.max()) - base) / (float(r[py, px]) - base + 1e-12)
        return -float(dx), -float(dy), conf, ratio


def frame_parts(cor, m):
    """Spectra of the whole picture and of each tile, with each tile's detail (how much texture it has)."""
    h, w = m.shape
    rows, cols = GRID
    th, tw = h // rows, w // cols
    tiles = []
    for i in range(rows):
        for j in range(cols):
            t = m[i * th:(i + 1) * th, j * tw:(j + 1) * tw]
            detail = float(np.abs(np.diff(t, axis=1)).mean() + np.abs(np.diff(t, axis=0)).mean())
            tiles.append((cor.spectrum(t), detail, ((j + 0.5) * tw - w / 2, (i + 0.5) * th - h / 2)))
    return {"whole": cor.spectrum(m), "tiles": tiles, "tile_shape": (th, tw), "shape": m.shape}


def fit_view(vecs, pos):
    """Least-squares move, zoom and roll of the whole view from the tiles' moves, dropping tiles that disagree
    (a plane the camera follows moves against the ground behind it). Returns tx, ty, zoom, roll, inliers."""
    idx = list(range(len(vecs)))
    for _ in range(2):
        if len(idx) < 4:
            return None
        A, b = [], []
        for k in idx:
            x, y = pos[k]
            A.append([1, 0, x, -y]); b.append(vecs[k][0])
            A.append([0, 1, y, x]); b.append(vecs[k][1])
        sol, *_ = np.linalg.lstsq(np.array(A, dtype=np.float64), np.array(b, dtype=np.float64), rcond=None)
        tx, ty, s, r = sol
        res = [math.hypot(vecs[k][0] - (tx + s * pos[k][0] - r * pos[k][1]),
                          vecs[k][1] - (ty + s * pos[k][1] + r * pos[k][0])) for k in idx]
        lim = max(1.0, 2.5 * float(np.median(res)))
        keep = [k for k, e in zip(idx, res) if e <= lim]
        if len(keep) == len(idx):
            break
        idx = keep
    if len(idx) < 4:
        return None
    return float(tx), float(ty), float(s), float(r), idx


def read_motion(cor, pa, pb, limits):
    """The view's move between two frames (MOTION_W px), how sure, its zoom and roll, and each tile's move."""
    gdx, gdy, gconf = cor.shift(pa["whole"], pb["whole"], pa["shape"])
    vecs, pos, tile_out = [], [], []
    for k, ((fa, sa, p), (fb, sb, _)) in enumerate(zip(pa["tiles"], pb["tiles"])):
        if min(sa, sb) < limits["tile_texture"]:
            tile_out.append(None)
            continue
        dx, dy, conf, ratio = cor.shift(fa, fb, pa["tile_shape"], second=True)
        th, tw = pa["tile_shape"]
        if conf < limits["tile_conf"] or abs(dx) > tw * 0.4 or abs(dy) > th * 0.4:
            tile_out.append(None)
            continue
        tile_out.append((dx, dy, ratio))
        vecs.append((dx, dy)); pos.append(p)
    fit = fit_view(vecs, pos) if len(vecs) >= 4 else None
    out = {"gdx": gdx, "gdy": gdy, "gconf": gconf, "tiles": tile_out, "zoom": None, "roll": None}
    if fit and len(fit[4]) >= max(4, 0.5 * len(vecs)):
        tx, ty, s, r, inl = fit
        # a lurch bigger than the tiles can follow: the whole picture's reading, when it is sure, wins
        if gconf > 12 and math.hypot(gdx - tx, gdy - ty) > 6 and math.hypot(gdx, gdy) > min(pa["tile_shape"]) * 0.3:
            out.update(dx=gdx, dy=gdy, sure=min(1.0, gconf / 25.0), how="whole")
        else:
            out.update(dx=tx, dy=ty, sure=len(inl) / len(pa["tiles"]), how="tiles", zoom=s, roll=r)
    else:
        out.update(dx=gdx, dy=gdy, sure=min(1.0, gconf / 25.0) if gconf > 8 else 0.0, how="whole")
    return out


def shifted_diff(a, b, dx, dy):
    """Mean grey difference after laying b back over a by the shift found, on the part both cover; and the same with
    each picture's own brightness taken away (a picture that only brightens is not a new picture)."""
    dx, dy = int(round(dx)), int(round(dy))
    h, w = a.shape
    ys = slice(max(0, dy), min(h, h + dy)); yb = slice(max(0, -dy), min(h, h - dy))
    xs = slice(max(0, dx), min(w, w + dx)); xb = slice(max(0, -dx), min(w, w - dx))
    pa, pb = a[yb, xb], b[ys, xs]
    if pa.size == 0:
        return 255.0, None, 255.0
    d = np.abs(pa - pb)
    struct = float(np.abs((pa - pa.mean()) - (pb - pb.mean())).mean())
    return float(d.mean()), d, struct


def tiles(d, n=6):
    """The difference picture's mean in an n by n grid."""
    if d is None:
        return None
    h, w = d.shape
    out = np.zeros((n, n), dtype=np.float32)
    for i in range(n):
        for j in range(n):
            t = d[i * h // n:(i + 1) * h // n, j * w // n:(j + 1) * w // n]
            out[i, j] = t.mean() if t.size else 0.0
    return out


def measure(paths, fps=None, expect_motion=False, times=None, limits=None, merge_repeats=False):
    """Read every frame pair and find the faults. times: each frame's real seconds (from the facts file's `strip`
    lines), so that a move is judged per second of real time, not per frame, when frames are not evenly spaced."""
    lim = dict(DEFAULTS)
    lim.update({k: v for k, v in (limits or {}).items() if v is not None})
    frames, motion_frames, sizes = [], [], []
    for p in paths:
        g, m, s = grey(p)
        frames.append(g)
        motion_frames.append(m)
        sizes.append(s)
    if len(frames) < 3:
        return {"summary": {"frames": len(frames), "counts": {}}, "readings": [], "faults": [],
                "note": "too few frames to judge"}
    full_w = sizes[0][0]
    scale = full_w / WORK_W
    mscale = full_w / motion_frames[0].shape[1]
    k1920 = 1920.0 / full_w
    cor = Correlator()
    readings = []
    prev_parts = frame_parts(cor, motion_frames[0])
    for i in range(1, len(frames)):
        a, b = frames[i - 1], frames[i]
        parts = frame_parts(cor, motion_frames[i]) if motion_frames[i].shape == motion_frames[i - 1].shape else None
        if a.shape != b.shape or parts is None:
            readings.append({"i": i, "cut": True, "why": "size changed"})
            prev_parts = frame_parts(cor, motion_frames[i])
            continue
        mo = read_motion(cor, prev_parts, parts, lim)
        prev_parts = parts
        dx, dy = mo["dx"] * mscale, mo["dy"] * mscale
        raw = float(np.abs(a - b).mean())
        res, d, struct = shifted_diff(a, b, dx / scale, dy / scale)
        r = {
            "i": i, "dx": round(dx, 2), "dy": round(dy, 2), "move": round(math.hypot(dx, dy), 2),
            "sure": round(mo["sure"], 3), "how": mo["how"],
            "zoom": None if mo["zoom"] is None else round(mo["zoom"], 4),
            "roll_deg": None if mo["roll"] is None else round(math.degrees(mo["roll"]), 3),
            "tv": [None if t is None else [round(t[0] * mscale, 2), round(t[1] * mscale, 2), round(t[2], 2)]
                   for t in mo["tiles"]],
            "raw": round(raw, 3), "residual": round(res, 3), "struct": round(struct, 3), "lum": round(float(b.mean()), 2),
            "lum_prev": round(float(a.mean()), 2), "std": round(float(b.std()), 2),
            "tiles": tiles(d).round(2).tolist() if d is not None else None,
        }
        if times is not None and i < len(times) and times[i] is not None and times[i - 1] is not None:
            r["dt"] = round(float(times[i]) - float(times[i - 1]), 4)
        readings.append(r)
    faults = []
    n = len(readings)
    jump_floor = 0.03 * full_w  # a lurch of 3% of the frame's width in one frame is seen at once

    for r in readings:
        if r.get("cut"):
            continue
        # a cut: the picture is replaced, not moved (and not only brightened or darkened: that is a flicker)
        if r["residual"] > 40 and r["raw"] > 40 and r["struct"] > 30:
            r["cut"] = True

    def around(k, w):
        """Readings within w frames of k on the same side of any cut, not k itself."""
        out = []
        for step in (-1, 1):
            j = k + step
            while 0 <= j < n and abs(j - k) <= w and not readings[j].get("cut"):
                out.append(j)
                j += step
        return sorted(out)

    # repeats: a frame identical to the one before while the frames around it change
    for k, r in enumerate(readings):
        if r.get("cut"):
            continue
        nb = [readings[j]["raw"] for j in around(k, lim["window"])]
        if len(nb) >= 4:
            usual = float(np.median(nb))
            if usual > 0.6 and r["raw"] < 0.35 * usual:
                r["repeat"] = True

    # the move a frame is judged by: per frame, or per real time when frames are not evenly spaced,
    # with a repeated frame and the next sharing their move when the recorder made the repeat
    dts = [r.get("dt") for r in readings if not r.get("cut") and r.get("dt")]
    dt_usual = float(np.median(dts)) if dts else None
    ntile = GRID[0] * GRID[1]
    view = [None] * n
    tilev = [[None] * ntile for _ in range(n)]
    tiler = [[None] * ntile for _ in range(n)]
    for k, r in enumerate(readings):
        if r.get("cut"):
            continue
        f = 1.0
        if dt_usual and r.get("dt") and r["dt"] > 0:
            f = dt_usual / r["dt"]
        view[k] = np.array([r["dx"], r["dy"]]) * f
        for j, t in enumerate(r["tv"]):
            if t is not None:
                tilev[k][j] = np.array(t[:2]) * f
                tiler[k][j] = t[2]
    if merge_repeats:
        for k, r in enumerate(readings):
            if r.get("repeat") and k + 1 < n and not readings[k + 1].get("cut"):
                half = (view[k] + view[k + 1]) / 2
                view[k], view[k + 1] = half, half.copy()
                for j in range(ntile):
                    if tilev[k][j] is not None and tilev[k + 1][j] is not None:
                        hv = (tilev[k][j] + tilev[k + 1][j]) / 2
                        tilev[k][j], tilev[k + 1][j] = hv, hv.copy()
                    elif tilev[k + 1][j] is not None:
                        tilev[k][j] = tilev[k + 1][j] = tilev[k + 1][j] / 2

    # each frame's clock (real seconds when known, else frame numbers) for the curve the frames around it make
    clock = [0.0] * n
    for k in range(1, n):
        dtk = readings[k].get("dt")
        clock[k] = clock[k - 1] + (dtk / dt_usual if (dt_usual and dtk) else 1.0)
    floor = lim["stutter_floor"] / k1920
    move_floor = lim["move_floor"] / k1920

    def expected(series, k, idx):
        """The move frame k should have from the frames around it (not itself): a curve (quadratic) through them,
        fitted twice with the worst-fitting dropped, so smooth easing and swaying are expected, not faulted.
        Returns (expected vector, the neighbours' moves) or None."""
        pts = [(j, series[j]) for j in idx if series[j] is not None]
        if len(pts) < 4:
            return None
        ts = np.array([(clock[j] - clock[k]) for j, _ in pts], dtype=np.float64)
        vs = np.stack([v for _, v in pts]).astype(np.float64)
        if len(pts) < 6:
            return np.median(vs, axis=0), vs
        keep = np.ones(len(ts), dtype=bool)
        for _ in range(2):
            A = np.vstack([np.ones(keep.sum()), ts[keep], ts[keep] ** 2]).T
            coef, *_ = np.linalg.lstsq(A, vs[keep], rcond=None)
            fit = np.vstack([np.ones(len(ts)), ts, ts ** 2]).T @ coef
            err = np.hypot(*(vs - fit).T)
            lim2 = max(2.5 * float(np.median(err[keep])), 0.25 * float(np.median(np.hypot(*vs.T))) + 0.05)
            newkeep = err <= lim2
            if newkeep.sum() < 5 or (newkeep == keep).all():
                break
            keep = newkeep
        return coef[0], vs  # the curve's value at frame k (t = 0)

    def uneven(series, k, idx):
        """(deviation px, usual px) when series[k] is a stutter against what the frames around it lead to expect."""
        if series[k] is None:
            return None
        e = expected(series, k, idx)
        if e is None:
            return None
        base, nb = e
        # the usual move: the median's, or the mean size of every move in the window with this frame's own (so that
        # a part that moves only in steps, still between them, still counts as moving)
        withk = np.vstack([nb, series[k][None, :]])
        usual = max(float(np.hypot(*base)), float(np.mean(np.hypot(withk[:, 0], withk[:, 1]))))
        if usual < move_floor:
            return None
        dev = float(np.hypot(*(series[k] - base)))
        if dev > lim["stutter_x"] * usual and dev > floor:
            return dev, usual
        return None

    spread = []
    stutter_frames = set()
    for k, r in enumerate(readings):
        if r.get("cut"):
            continue
        idx = around(k, lim["window"])
        lo_hi = [readings[j]["move"] for j in idx]
        base = float(np.median(lo_hi)) if lo_hi else 0.0
        # jump: much further than the frames around it, and far enough to see
        is_jump = r["move"] > max(jump_floor, 3.0 * base + 0.01 * full_w) and r["sure"] > 0.2
        if is_jump:
            faults.append({"kind": "jump", "frame": r["i"], "size_px": round(r["move"], 1),
                           "usual_px": round(base, 1), "size_1920": round(r["move"] * k1920, 1),
                           "words": f"the whole view lurches {r['move']:.0f} px in one frame where the frames around it move about {base:.0f} px"})
        # stutter of the whole view
        if not is_jump and r["sure"] > 0.2:
            u = uneven(view, k, idx)
            e = expected(view, k, idx) if view[k] is not None else None
            if e is not None:
                b, nbv = e
                us = max(float(np.hypot(*b)), float(np.mean(np.hypot(*np.vstack([nbv, view[k][None, :]]).T))))
                if us >= move_floor:
                    spread.append(float(np.hypot(*(view[k] - b))) / us)
            if u:
                stutter_frames.add(k)
                faults.append({"kind": "stutter", "frame": r["i"], "where": "whole view",
                               "dev_px": round(u[0], 1), "usual_px": round(u[1], 1),
                               "dev_1920": round(u[0] * k1920, 1),
                               "words": f"the view moves {u[0] * k1920:.0f} px (on 1920) off its even pace in one frame, where it usually moves {u[1] * k1920:.0f} px a frame"})
        # stutter of a part of the picture (the plane the camera follows, a tank): its own move comes in steps
        if not is_jump and k not in stutter_frames:
            worst = None
            for j in range(ntile):
                rat = [tiler[q][j] for q in idx + [k] if tiler[q][j] is not None]
                if len(rat) < 5 or float(np.percentile(rat, 80)) > lim["tile_two"] or (tiler[k][j] or 1) > 1.3 * lim["tile_two"]:
                    continue  # two motions in this part (or no reading): its move cannot be told apart
                col = [tilev[q][j] for q in range(n)]
                u = uneven(col, k, idx)
                if u and (worst is None or u[0] > worst[1][0]):
                    worst = (j, u)
            if worst:
                j, u = worst
                cell = (j // GRID[1], j % GRID[1])
                wh = where_words(cell, GRID)
                stutter_frames.add(k)
                faults.append({"kind": "stutter", "frame": r["i"], "where": wh, "tile": [cell[0], cell[1]],
                               "dev_px": round(u[0], 1), "usual_px": round(u[1], 1),
                               "dev_1920": round(u[0] * k1920, 1),
                               "words": f"what is in the {wh} of the picture moves in a step of {u[0] * k1920:.0f} px (on 1920) instead of evenly"})
        # zoom: the picture grows or shrinks in one frame beyond what the frames around it do
        if r.get("zoom") is not None:
            zs = [readings[j]["zoom"] for j in idx if readings[j].get("zoom") is not None]
            if len(zs) >= 3:
                zb = float(np.median(zs))
                if abs(r["zoom"] - zb) > lim["zoom_pop"]:
                    faults.append({"kind": "zoom", "frame": r["i"], "size": round(r["zoom"] - zb, 3),
                                   "words": f"the picture {'grows' if r['zoom'] > zb else 'shrinks'} by {abs(r['zoom'] - zb) * 100:.0f}% in one frame"})
        # pop: one tile changes far more than it does in the frames around it
        lo, hi = max(0, k - 5), min(n, k + 6)
        if r.get("tiles") is not None:
            t = np.array(r["tiles"])
            hist = [np.array(readings[j]["tiles"]) for j in range(lo, hi)
                    if j != k and readings[j].get("tiles") is not None and not readings[j].get("cut")]
            if len(hist) >= 3:
                hist = np.stack(hist)
                med = np.median(hist, axis=0)
                mad = np.median(np.abs(hist - med), axis=0) + 0.75
                z = (t - med) / mad
                worst = np.unravel_index(np.argmax(z), z.shape)
                if z[worst] > 8 and t[worst] - med[worst] > 6 and (z > 8).sum() <= 4:
                    faults.append({"kind": "pop", "frame": r["i"], "where": where_words(worst, t.shape),
                                   "tile": [int(worst[0]), int(worst[1])], "strength": round(float(z[worst]), 1),
                                   "words": f"something in the {where_words(worst, t.shape)} of the picture changes suddenly in one frame"})
        # flicker: the whole picture's brightness jumps and comes back
        if 0 < k < n - 1 and not readings[k + 1].get("cut"):
            d1 = r["lum"] - r["lum_prev"]
            d2 = readings[k + 1]["lum"] - readings[k + 1]["lum_prev"]
            if abs(d1) > 10 and abs(d2) > 10 and d1 * d2 < 0:
                faults.append({"kind": "flicker", "frame": r["i"], "size": round(d1, 1),
                               "words": f"the whole picture {'brightens' if d1 > 0 else 'darkens'} by {abs(d1):.0f} levels for one frame"})
        # blank
        if r["lum"] < 6 or r["lum"] > 249 or r["std"] < 2.5:
            faults.append({"kind": "blank", "frame": r["i"], "words": "an empty (black, white or flat) frame"})
    # freeze: 3 or more frames in a row with nothing moving at all, while the frames around them visibly move (a shot
    # that holds a plane still against haze changes little in any frame: that is not a freeze); and, when each frame's
    # real time is known, a gap between two frames over 100 ms and 3 times the usual (the game stood still that long)
    run = 0
    for k, r in enumerate(readings):
        still = (not r.get("cut")) and r["raw"] < 0.25
        run = run + 1 if still else 0
        if run == 3 and expect_motion:
            nb = [readings[j]["raw"] for j in range(max(0, k - 12), min(n, k + 10))
                  if not readings[j].get("cut") and readings[j]["raw"] >= 0.25]
            if len(nb) >= 4 and float(np.median(nb)) > 1.0:
                faults.append({"kind": "freeze", "frame": r["i"] - 2,
                               "words": "the picture stands completely still for several frames while it should be moving"})
        if dt_usual and r.get("dt") and r["dt"] > max(0.1, 3 * dt_usual):
            faults.append({"kind": "freeze", "frame": r["i"], "ms": round(r["dt"] * 1000),
                           "words": f"the picture stands still for {r['dt'] * 1000:.0f} ms between two frames"})
    faults.sort(key=lambda f: f["frame"])
    moves = np.array([r.get("move", 0.0) for r in readings if not r.get("cut")] or [0.0])
    resid = np.array([r.get("residual", 0.0) for r in readings if not r.get("cut")] or [0.0])
    kinds = ("jump", "stutter", "zoom", "pop", "freeze", "flicker", "blank")
    summary = {
        "frames": len(frames), "fps": fps, "width": full_w,
        "move_px_median": round(float(np.median(moves)), 2),
        "move_1920_median": round(float(np.median(moves)) * k1920, 2),
        "residual_median": round(float(np.median(resid)), 2),
        "cuts": sum(1 for r in readings if r.get("cut")),
        "repeats": sum(1 for r in readings if r.get("repeat")),
        "merge_repeats": bool(merge_repeats),
        "stutter_spread_median": round(float(np.median(spread)), 3) if spread else None,
        "stutter_spread_p90": round(float(np.percentile(spread, 90)), 3) if spread else None,
        "stutter_view": sum(1 for f in faults if f["kind"] == "stutter" and f["where"] == "whole view"),
        "stutter_part": sum(1 for f in faults if f["kind"] == "stutter" and f["where"] != "whole view"),
        "counts": {k: sum(1 for f in faults if f["kind"] == k) for k in kinds},
        "limits": lim,
    }
    secs = None
    if times is not None and len(times) >= 2 and times[0] is not None and times[-1] is not None:
        secs = float(times[-1]) - float(times[0])
    elif fps:
        secs = len(frames) / fps
    if secs:
        summary["seconds"] = round(secs, 2)
        summary["per_minute"] = {k: round(summary["counts"][k] * 60.0 / secs, 1) for k in kinds}
        summary["jumps_per_minute"] = summary["per_minute"]["jump"]
    return {"summary": summary, "faults": faults, "readings": readings}


def judge(result, rule_limits):
    """Compare a measure result with a rule's clip limits [{measure, max, per_minute}] -> list of breaches."""
    out = []
    s = result.get("summary", {})
    for lim in rule_limits or []:
        m = lim.get("measure")
        mx = lim.get("max", 0)
        if lim.get("per_minute"):
            got = (s.get("per_minute") or {}).get(m)
        else:
            got = (s.get("counts") or {}).get(m)
        if got is None:
            continue
        if got > mx:
            out.append({"measure": m, "got": got, "max": mx, "per_minute": bool(lim.get("per_minute"))})
    return out


def where_words(cell, shape):
    i, j = cell
    nr, nc = shape[0], shape[1] if len(shape) > 1 else shape[0]
    y, x = (i + 0.5) / nr, (j + 0.5) / nc
    row = "top" if y < 1 / 3 else ("bottom" if y > 2 / 3 else "middle")
    col = "left" if x < 1 / 3 else ("right" if x > 2 / 3 else "centre")
    return "centre" if (row, col) == ("middle", "centre") else f"{row} {col}"


def sheet(paths, faults, out, cols=8, thumb_w=240, label=None):
    flagged = {}
    for f in faults:
        flagged.setdefault(f["frame"], []).append(f["kind"])
    # keep the sheet small: every flagged frame and its neighbours, else an even sample
    pick = set()
    for fr in sorted(flagged)[:cols * 2]:
        pick.update({fr - 1, fr, fr + 1})
    if len(pick) < cols * 2:
        step = max(1, len(paths) // (cols * 3))
        pick.update(range(0, len(paths), step))
    pick = sorted(p for p in pick if 0 <= p < len(paths))[:cols * 6]
    first = Image.open(paths[0])
    th = round(first.size[1] * thumb_w / first.size[0])
    rows = math.ceil(len(pick) / cols)
    top = 28 if label else 0
    canvas = Image.new("RGB", (cols * thumb_w, rows * (th + 16) + top), (20, 20, 20))
    draw = ImageDraw.Draw(canvas)
    if label:
        draw.text((8, 8), label, fill=(235, 235, 235))
    for n, p in enumerate(pick):
        im = Image.open(paths[p]).convert("RGB").resize((thumb_w, th))
        x, y = (n % cols) * thumb_w, top + (n // cols) * (th + 16)
        canvas.paste(im, (x, y))
        kinds = flagged.get(p)
        if kinds:
            draw.rectangle([x, y, x + thumb_w - 1, y + th - 1], outline=(230, 40, 40), width=4)
        draw.text((x + 4, y + th + 2), f"{p}" + (" " + ",".join(sorted(set(kinds))) if kinds else ""),
                  fill=(255, 90, 90) if kinds else (200, 200, 200))
    canvas.save(out, quality=85)


FAULT_ORDER = {"jump": 0, "zoom": 1, "stutter": 2, "flicker": 3, "freeze": 4, "pop": 5, "blank": 6}
FAULT_WORDS = {"jump": "the view jumps", "zoom": "the picture zooms at once", "stutter": "uneven step", "flicker": "flicker",
               "freeze": "the picture freezes", "pop": "something pops", "blank": "empty frame"}


def big_font(size):
    try:
        return ImageFont.load_default(size=size)
    except TypeError:  # Pillow before 10.1
        return ImageFont.load_default()


def focus_sheet(paths, faults, out, rows=3, thumb_w=320, label=None):
    """A sheet readable on a phone: for the worst faults (up to `rows`), the frame before, the faulty frame (red) and
    the frame after, each row named in words. Returns False when there is no fault to show."""
    picked, seen = [], set()
    for f in sorted(faults, key=lambda f: (FAULT_ORDER.get(f["kind"], 9), f["frame"])):
        if f["frame"] in seen or not (0 <= f["frame"] < len(paths)):
            continue
        picked.append(f)
        seen.update({f["frame"] - 1, f["frame"], f["frame"] + 1})
        if len(picked) == rows:
            break
    if not picked:
        return False
    first = Image.open(paths[0])
    th = round(first.size[1] * thumb_w / first.size[0])
    head = 34
    top = 40 if label else 0
    canvas = Image.new("RGB", (3 * thumb_w + 16, top + len(picked) * (th + head) + 8), (20, 20, 20))
    draw = ImageDraw.Draw(canvas)
    f_big, f_small = big_font(22), big_font(17)
    if label:
        draw.text((8, 8), label, fill=(235, 235, 235), font=f_big)
    for r, f in enumerate(picked):
        y = top + r * (th + head)
        draw.text((8, y + 6), f"frame {f['frame']}: {FAULT_WORDS.get(f['kind'], f['kind'])}"
                  + (f" ({f['where']})" if f.get("where") and f["where"] != "whole view" else ""),
                  fill=(255, 120, 110), font=f_big)
        for c, k in enumerate((f["frame"] - 1, f["frame"], f["frame"] + 1)):
            x = 4 + c * (thumb_w + 4)
            if 0 <= k < len(paths):
                im = Image.open(paths[k]).convert("RGB").resize((thumb_w, th))
                canvas.paste(im, (x, y + head))
                if k == f["frame"]:
                    draw.rectangle([x, y + head, x + thumb_w - 1, y + head + th - 1], outline=(235, 40, 40), width=5)
                draw.text((x + 6, y + head + th - 24), ("before", "this frame", "after")[c], fill=(255, 255, 255),
                          font=f_small, stroke_width=2, stroke_fill=(0, 0, 0))
    canvas.save(out, quality=85)
    return True


def main():
    ap = argparse.ArgumentParser(description="Measure a clip (folder of frames or a video) for movement faults.")
    ap.add_argument("src")
    ap.add_argument("--out", required=True)
    ap.add_argument("--sheet")
    ap.add_argument("--fps", type=float)
    ap.add_argument("--times", help="JSON list of each frame's real seconds (same order as the frames)")
    ap.add_argument("--expect-motion", action="store_true")
    ap.add_argument("--label")
    ap.add_argument("--merge-repeats", dest="merge", action="store_true", default=None,
                    help="a repeated frame and the next share their move (default for a video file)")
    ap.add_argument("--no-merge-repeats", dest="merge", action="store_false")
    ap.add_argument("--stutter-x", type=float)
    ap.add_argument("--stutter-floor", type=float, help="px on a 1920-wide picture")
    ap.add_argument("--move-floor", type=float, help="px on a 1920-wide picture")
    ap.add_argument("--zoom-pop", type=float)
    ap.add_argument("--readings", action="store_true", help="keep every frame's reading in the JSON (large)")
    a = ap.parse_args()
    paths, tmp = load_frames(a.src)
    times = None
    if a.times:
        with open(a.times) as fh:
            times = json.load(fh)
    merge = a.merge if a.merge is not None else (tmp is not None)
    result = measure(paths, a.fps, a.expect_motion, times=times, merge_repeats=merge,
                     limits={"stutter_x": a.stutter_x, "stutter_floor": a.stutter_floor,
                             "move_floor": a.move_floor, "zoom_pop": a.zoom_pop})
    result["source"] = a.src
    if a.label:
        result["label"] = a.label
    if not a.readings:
        for r in result.get("readings", []):
            r.pop("tiles", None)
            r.pop("tv", None)
    with open(a.out, "w") as fh:
        json.dump(result, fh, indent=1)
    if a.sheet and paths:
        sheet(paths, result["faults"], a.sheet, label=a.label)
    s = result.get("summary", {})
    print(json.dumps({"source": a.src, **{k: v for k, v in s.items() if k != "limits"}}))


if __name__ == "__main__":
    main()
