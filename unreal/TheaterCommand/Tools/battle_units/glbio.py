import json, struct, numpy as np


def glb(path):
    """The meshes' primitives of a .glb: positions, normals, triangles, material name (glTF's own frame, y up)."""
    b = open(path, "rb").read()
    L = struct.unpack("<I", b[12:16])[0]
    J = json.loads(b[20:20 + L])
    off = 20 + L
    L2 = struct.unpack("<I", b[off:off + 4])[0]
    bin_ = b[off + 8: off + 8 + L2]

    def acc(i):
        a = J["accessors"][i]
        bv = J["bufferViews"][a["bufferView"]]
        n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}[a["type"]]
        dt = {5126: np.float32, 5123: np.uint16, 5125: np.uint32}[a["componentType"]]
        start = bv.get("byteOffset", 0) + a.get("byteOffset", 0)
        arr = np.frombuffer(bin_, dt, a["count"] * n, start)
        return arr.reshape(-1, n) if n > 1 else arr

    out = {}
    for p in J["meshes"][0]["primitives"]:
        out[J["materials"][p["material"]]["name"]] = {
            "P": acc(p["attributes"]["POSITION"]).astype(float), "N": acc(p["attributes"]["NORMAL"]).astype(float),
            "T": acc(p["indices"]).astype(int).reshape(-1, 3)}
    return out


def to_json(v):
    """glTF (x, y up, z) to the soldier json's axes (x forward, y to his right, z up)."""
    return np.asarray(v, float)[:, [0, 2, 1]]
