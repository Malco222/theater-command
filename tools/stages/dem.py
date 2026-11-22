# The Terrarium elevation tiles (Mapzen, AWS Open Data) as one height picture in metres, fetched once into WORK/tiles.
import math, os, urllib.request
from PIL import Image
import numpy as np
WORK = os.environ.get('STAGE_WORK', '/tmp/stage-work')
T = os.path.join(WORK, 'tiles')


def fetch(z, tx, ty):
    path = f'{T}/{z}_{tx}_{ty}.png'
    if not os.path.exists(path):
        os.makedirs(T, exist_ok=True)
        urllib.request.urlretrieve(f'https://s3.amazonaws.com/elevation-tiles-prod/terrarium/{z}/{tx}/{ty}.png', path)
    return path


def mosaic(z, la0, la1, lo0, lo1):
    def tile(lat, lon):
        n = 2 ** z
        return (lon + 180) / 360 * n, (1 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2 * n
    x0, y1 = tile(la0, lo0)
    x1, y0 = tile(la1, lo1)
    tx0, tx1, ty0, ty1 = int(x0), int(x1), int(y0), int(y1)
    W, H = (tx1 - tx0 + 1) * 256, (ty1 - ty0 + 1) * 256
    m = np.zeros((H, W))
    for tx in range(tx0, tx1 + 1):
        for ty in range(ty0, ty1 + 1):
            a = np.asarray(Image.open(fetch(z, tx, ty)).convert('RGB')).astype(np.float64)
            m[(ty - ty0) * 256:(ty - ty0 + 1) * 256, (tx - tx0) * 256:(tx - tx0 + 1) * 256] = a[:, :, 0] * 256 + a[:, :, 1] + a[:, :, 2] / 256 - 32768
    px0, px1, py0, py1 = int((x0 - tx0) * 256), int((x1 - tx0) * 256), int((y0 - ty0) * 256), int((y1 - ty0) * 256)
    return m[py0:py1, px0:px1]
