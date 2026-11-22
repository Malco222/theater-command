"""The men's picture sheets as PNGs, each with its one small caption (land_men_picture paints them as PPM).

Usage: python3 land_men_sheet.py <folder with falls.ppm and battle.ppm> <folder for the PNGs>
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

CAPTIONS = {
    'falls': ('land_men_falls.png',
              'The falls as the crowd plays them: hit standing, running, kneeling (each man\'s fall picked from his '
              'file\'s deaths map); an American, then a German; from the hit to the body laid and frozen.'),
    'battle': ('land_men_battle.png',
               'Normandy, July 1944, the recorded dry run at 3 min 7 s: an American rifleman hit and going down among '
               'his section. Men from the crowd\'s own skinning; ground, sky and light are stand-ins.'),
}


def font(size):
    for path in ('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', '/System/Library/Fonts/Helvetica.ttc'):
        if os.path.exists(path):
            return ImageFont.truetype(path, size)
    return ImageFont.load_default()


def main():
    src, dst = sys.argv[1], sys.argv[2]
    os.makedirs(dst, exist_ok=True)
    for key, (name, text) in CAPTIONS.items():
        pic = Image.open(os.path.join(src, key + '.ppm')).convert('RGB')
        strip = 30
        sheet = Image.new('RGB', (pic.width, pic.height + strip), (236, 236, 236))
        sheet.paste(pic, (0, 0))
        draw = ImageDraw.Draw(sheet)
        f = font(14)
        draw.text((10, pic.height + 7), text, fill=(45, 45, 45), font=f)
        out = os.path.join(dst, name)
        sheet.save(out, optimize=True)
        print('wrote %s (%dx%d, %d bytes)' % (out, sheet.width, sheet.height, os.path.getsize(out)))


if __name__ == '__main__':
    main()
