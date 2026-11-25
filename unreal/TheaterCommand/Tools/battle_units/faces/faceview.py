"""Picture helpers for the face tools: a numpy z-buffer raster (from the far-levels checks) and a tilted camera."""
import numpy as np

def raster(P, N, T, col, view, size=300, scale=150.0, centre=(0, 0, 0.9)):
    # view: 'front' looks along -x (x forward toward camera), 'side' looks along +y
    c = np.array(centre)
    if view == "front":
        u, v, d = -(P[:, 1] - c[1]), P[:, 2] - c[2], P[:, 0]
        L = np.array([0.6, -0.3, 0.75])
    elif view == "side":
        u, v, d = (P[:, 0] - c[0]), P[:, 2] - c[2], -P[:, 1]
        L = np.array([0.5, -0.6, 0.6])
    else:  # three-quarter
        a = np.radians(35)
        x2 = P[:, 0] * np.cos(a) - P[:, 1] * np.sin(a)
        y2 = P[:, 0] * np.sin(a) + P[:, 1] * np.cos(a)
        u, v, d = -(y2 - 0), P[:, 2] - c[2], x2
        L = np.array([0.6, -0.3, 0.75])
    L = L / np.linalg.norm(L)
    X = size / 2 + u * scale
    Y = size / 2 - v * scale
    img = np.full((size, size, 3), (138, 150, 166), float)
    zb = np.full((size, size), -1e9)
    lam = 0.45 + 0.55 * np.clip(N @ L, 0, 1)
    for t in T:
        xs, ys = X[t], Y[t]
        x0, x1 = int(max(0, np.floor(xs.min()))), int(min(size - 1, np.ceil(xs.max())))
        y0, y1 = int(max(0, np.floor(ys.min()))), int(min(size - 1, np.ceil(ys.max())))
        if x1 < x0 or y1 < y0:
            continue
        gx, gy = np.meshgrid(np.arange(x0, x1 + 1) + 0.5, np.arange(y0, y1 + 1) + 0.5)
        (ax, bx, cx), (ay, by, cy) = xs, ys
        den = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy)
        if abs(den) < 1e-12:
            continue
        w0 = ((by - cy) * (gx - cx) + (cx - bx) * (gy - cy)) / den
        w1 = ((cy - ay) * (gx - cx) + (ax - cx) * (gy - cy)) / den
        w2 = 1 - w0 - w1
        m = (w0 >= 0) & (w1 >= 0) & (w2 >= 0)
        if not m.any():
            continue
        z = w0 * d[t[0]] + w1 * d[t[1]] + w2 * d[t[2]]
        sub = zb[y0:y1 + 1, x0:x1 + 1]
        m &= z > sub
        sub[m] = z[m]
        cc = (w0[..., None] * (col[t[0]] * lam[t[0]]) + w1[..., None] * (col[t[1]] * lam[t[1]]) + w2[..., None] * (col[t[2]] * lam[t[2]]))
        img[y0:y1 + 1, x0:x1 + 1][m] = cc[m]
    return img.astype(np.uint8)


CAM = {"front": (1.0, 0.0, 0.0), "side": (0.0, -1.0, 0.0), "q": (np.cos(np.radians(35)), -np.sin(np.radians(35)), 0.0)}


def tilted(P, N, T, C, view, centre, down):
    """The scene turned so the camera looks `down` degrees from level (the game's close camera looks 16 to 24 degrees
    down), about the view's centre."""
    if not down:
        return P, N, T, C
    c = np.array(CAM[view])
    k = np.cross((0.0, 0.0, 1.0), c)
    k /= np.linalg.norm(k)
    a = np.radians(down)
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    R = np.eye(3) + np.sin(a) * K + (1 - np.cos(a)) * K @ K
    o = np.array(centre)
    return (P - o) @ R.T + o, N @ R.T, T, C


