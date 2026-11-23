import json, os, sys, numpy as np
argv = list(sys.argv); sys.argv = ['x']
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'facecam.py')).read().split('if __name__')[0])
from PIL import Image, ImageDraw, ImageFont
S = "/mnt/project-files/battle-models/land/soldier/"
out, stage = argv[1], argv[2]
MEN = [("soldier_GER.json", "GER", 2, "German rifleman"), ("soldier_USA.json", "USA", 4, "American rifleman (moustache)"),
       ("soldier_UK_sailor.json", "UK", 0, "British sailor (had no brows)"), ("soldier_JAP.json", "JAP", 1, "Japanese rifleman")]
try:
    font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
    small = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 13)
except Exception:
    font = small = None
W = 260
rows = []
for f, row, k, label in MEN:
    tl = []
    for tag, p, v in (("before", S + f, "front"), ("after", stage + f, "front"), ("after", stage + f, "q")):
        D = json.load(open(p))
        hb = [b for b in D["bones"] if b["name"] == "head"][0]["head"]
        c = (hb[0] + 0.08, 0.0, hb[2] + 0.01)
        t = tile(D, row, "near", k, v, 5, size=2 * W, zoom=1.55, helmet=True, centre=c)
        im = Image.fromarray(t).resize((W, W), Image.LANCZOS)
        d = ImageDraw.Draw(im)
        d.rectangle((0, 0, W, 20), fill=(30, 30, 30))
        d.text((6, 2), ("BEFORE" if tag == "before" else "AFTER") + (" (turned)" if v == "q" else ""), fill=(240, 240, 240), font=small)
        tl.append(np.array(im))
    r = np.concatenate(tl, 1)
    lab = Image.new("RGB", (r.shape[1], 24), (235, 232, 225))
    ImageDraw.Draw(lab).text((6, 3), label, fill=(20, 20, 20), font=font)
    rows += [np.array(lab), r]
img = Image.fromarray(np.concatenate(rows, 0))
head = Image.new("RGB", (img.width, 30), (235, 232, 225))
ImageDraw.Draw(head).text((6, 6), "Close-range faces, before and after (game colours, within about 9 m)", fill=(0, 0, 0), font=font)
img = Image.fromarray(np.concatenate([np.array(head), np.array(img)], 0))
img.save(out, quality=86)
print(img.size)
