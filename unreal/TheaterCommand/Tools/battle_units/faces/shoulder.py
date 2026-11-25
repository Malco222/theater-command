"""Four men at 4 m, head and shoulders, at the game's pixel density (1920 px across a 90-degree view: 240 px a metre
at 4 m), drawn 4x finer and averaged down, then enlarged 3x to look at; round 1 above, round 2 below."""
import json, os, sys, numpy as np
argv = list(sys.argv); sys.argv = ['x']
exec(open(os.path.join(os.path.dirname(os.path.abspath(__file__)), 'facecam.py')).read().split('if __name__')[0])
from PIL import Image, ImageDraw, ImageFont
out, r1, r2 = argv[1], argv[2], argv[3]
MEN = [("soldier_GER.json", "GER", 2, "German"), ("soldier_USA.json", "USA", 4, "American"),
       ("soldier_UK_sailor.json", "UK", 0, "British sailor"), ("soldier_JAP.json", "JAP", 1, "Japanese")]
PPM = 240.0           # (pixels a metre at 4 m)
SS = 4
W_M, H_M = 0.30, 0.30
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 14)
small = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 12)
rows = []
for tag, base in (("Round 1", r1), ("Round 2", r2)):
    tiles, ones = [], []
    for f, row, k, label in MEN:
        D = json.load(open(base + f))
        hb = [b for b in D["bones"] if b["name"] == "head"][0]["head"]
        c = (hb[0] + 0.06, 0.0, hb[2] + 0.0)
        size = int(W_M * PPM * SS)
        zoom = 1.0 / (3.2 * W_M)
        t = tile(D, row, "near", k, "q", 8, size=size, zoom=zoom, helmet=True, centre=c)
        im = Image.fromarray(t).resize((int(W_M * PPM), int(H_M * PPM)), Image.BOX)        # (as the screen has it)
        ones.append(im)
        big = im.resize((im.width * 3, im.height * 3), Image.BICUBIC)
        d = ImageDraw.Draw(big)
        d.rectangle((0, 0, big.width, 18), fill=(30, 30, 30))
        d.text((5, 2), f"{tag}: {label}", fill=(240, 240, 240), font=small)
        tiles.append(np.array(big))
    rows.append(np.concatenate(tiles, 1))
    strip = Image.new("RGB", (rows[-1].shape[1], int(H_M * PPM) + 8), (235, 232, 225))
    x = 6
    ImageDraw.Draw(strip).text((x, 4), f"{tag} at true size:", fill=(20, 20, 20), font=small)
    x = 140
    for im in ones:
        strip.paste(im, (x, 4)); x += im.width + 10
    rows.append(np.array(strip))
img = np.concatenate(rows, 0)
img = Image.fromarray(img)
head = Image.new("RGB", (img.width, 28), (235, 232, 225))
ImageDraw.Draw(head).text((6, 6), "Head and shoulders at about 4 m, game light and the game's pixels (enlarged 3x to look at)", fill=(0, 0, 0), font=font)
img = Image.fromarray(np.concatenate([np.array(head), np.array(img)], 0))
if img.width > 800:
    img = img.resize((800, int(img.height * 800 / img.width)), Image.LANCZOS)
img.save(out, quality=88)
print(img.size)
