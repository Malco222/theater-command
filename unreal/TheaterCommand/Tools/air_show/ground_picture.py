# A top-down picture of a stage's ground as the scene paints it (AirGround), from the cloud test's dump:
#   AIR_GROUND_DUMP=1 Tools/air_show/run.sh Content/Data/AirBattles/oahu.json
#   python3 Tools/air_show/ground_picture.py $TMPDIR/air_show/ground-oahu.bin out.png [x0 y0 x1 y1 metres-a-pixel]
# Colours as painted point by point (no light, no grain): what the art director asked to see of the harbour.
# The dump's last part is the yard's paving (the pads and the apron as AirStage lays them, AirGround::Paving).
# STAGE=Content/Data/Stages/oahu.json lays the stage's yard blocks (seen from above: roofs, the docks, the quays, in
# AirStage's paints) and its test ships (grey outlines) over the ground.
import json, math, os, struct, sys
import numpy as np
from PIL import Image

src, out = sys.argv[1], sys.argv[2]
x0, y0, x1, y1, mpp = (float(v) for v in (sys.argv[3:8] if len(sys.argv) >= 8 else (-3500, -2500, 2500, 2000, 2)))
W, H = int((x1 - x0) / mpp), int((y1 - y0) / mpp)
img = np.zeros((H, W, 3), dtype=np.float32)
img[:] = (0x1F, 0x57, 0x7A)  # (the stage's sea paint, AirStage.cpp)
depth = np.full((H, W), -1e9, dtype=np.float32)
b = open(src, 'rb').read()
o = 0
def word():
    global o
    v = struct.unpack_from('<I', b, o)[0]; o += 4; return v
for _ in range(word()):
    n = word(); name = b[o:o + n].decode(); o += n
    np_ = word()
    rec = np.frombuffer(b, dtype=np.dtype([('p', '<f4', 3), ('c', 'u1', 4)]), count=np_, offset=o); o += np_ * 16
    nt = word()
    tris = np.frombuffer(b, dtype='<i4', count=nt, offset=o).reshape(-1, 3); o += nt * 4
    P, C = rec['p'].astype(np.float64), rec['c'][:, :3].astype(np.float32)
    # (the shallows and reef a little over the sea, the land over them: drawn by height, as the scene's depth would)
    lift = {'shallows': 0.0, 'reef': 0.1, 'land': 1.0, 'works': 2.0, 'paving': 3.0}.get(name, 0.0)
    px = (P[:, 0] - x0) / mpp
    py = (y1 - P[:, 1]) / mpp
    for a, bb, c in tris:
        xs, ys = px[[a, bb, c]], py[[a, bb, c]]
        lo_x, hi_x = int(max(0, np.floor(xs.min()))), int(min(W - 1, np.ceil(xs.max())))
        lo_y, hi_y = int(max(0, np.floor(ys.min()))), int(min(H - 1, np.ceil(ys.max())))
        if lo_x > hi_x or lo_y > hi_y:
            continue
        gx, gy = np.meshgrid(np.arange(lo_x, hi_x + 1) + 0.5, np.arange(lo_y, hi_y + 1) + 0.5)
        d = (ys[1] - ys[2]) * (xs[0] - xs[2]) + (xs[2] - xs[1]) * (ys[0] - ys[2])
        if abs(d) < 1e-9:
            continue
        l0 = ((ys[1] - ys[2]) * (gx - xs[2]) + (xs[2] - xs[1]) * (gy - ys[2])) / d
        l1 = ((ys[2] - ys[0]) * (gx - xs[2]) + (xs[0] - xs[2]) * (gy - ys[2])) / d
        l2 = 1 - l0 - l1
        inside = (l0 >= -1e-6) & (l1 >= -1e-6) & (l2 >= -1e-6)
        if not inside.any():
            continue
        z = l0 * P[a, 2] + l1 * P[bb, 2] + l2 * P[c, 2] + lift
        sub = depth[lo_y:hi_y + 1, lo_x:hi_x + 1]
        win = inside & (z >= sub)
        col = l0[..., None] * C[a] + l1[..., None] * C[bb] + l2[..., None] * C[c]
        img[lo_y:hi_y + 1, lo_x:hi_x + 1][win] = col[win]
        sub[win] = z[win]
picture = Image.fromarray(np.clip(img, 0, 255).astype(np.uint8))
if os.environ.get('STAGE'):
    from PIL import ImageDraw
    stage = json.load(open(os.environ['STAGE']))
    draw = ImageDraw.Draw(picture)

    def rect(x, y, length, width, heading, ahead=0.0, aside=0.0):
        h = math.radians(heading)
        u, v = (math.sin(h), math.cos(h)), (math.cos(h), -math.sin(h))
        cx, cy = x + u[0] * ahead + v[0] * aside, y + u[1] * ahead + v[1] * aside
        pts = [(cx + u[0] * length * a + v[0] * width * b, cy + u[1] * length * a + v[1] * width * b) for a, b in ((-.5, -.5), (.5, -.5), (.5, .5), (-.5, .5))]
        return [((px - x0) / mpp, (y1 - py) / mpp) for px, py in pts]

    concrete, floor, steel, walls, roof = (0x8F, 0x8B, 0x83), (0x4A, 0x48, 0x44), (0x5C, 0x60, 0x62), (0x8C, 0x8A, 0x80), (0x6F, 0x6E, 0x68)
    yard = stage.get('yard', [])
    for x, y, g, l, w, hh, hd, kind in yard:
        if kind == 1:
            draw.polygon(rect(x, y, l, w, hd), fill=concrete)
            draw.polygon(rect(x, y, l - 20, w - 15, hd), fill=floor)
            draw.polygon(rect(x, y, 10, w, hd, ahead=l / 2 - 5), fill=steel)
        elif kind == 3:
            draw.polygon(rect(x, y, l, w, hd), fill=steel)
        elif kind == 4:
            draw.polygon(rect(x, y, l, w, hd), fill=concrete, outline=(0x50, 0x50, 0x4C))
    for s_ in (stage.get('test') or {}).get('ships', []):
        long_, beam = {'bb': (190, 32), 'ca': (180, 19), 'dd': (106, 10), 'ss': (95, 8), 'ag': (160, 27)}.get(s_['cls'], (150, 18))
        draw.polygon(rect(s_['at'][0], s_['at'][1], long_, beam, s_['face']), fill=(0x7E, 0x82, 0x84), outline=(0x30, 0x30, 0x30))
    for x, y, g, l, w, hh, hd, kind in yard:
        if kind == 0:
            draw.polygon(rect(x, y, l, w, hd), fill=walls)
            draw.polygon(rect(x, y, l, w * 0.8, hd), fill=roof)
        elif kind == 3:
            for side in (-1, 1):
                draw.polygon(rect(x, y, l, 3, hd, aside=side * (w / 2 - 1.5)), fill=(0x48, 0x4B, 0x4D))
picture.save(out)
print(out, W, H)
