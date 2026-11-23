"""The medic's helmet discs and crosses and the brassard's cross taken off the game files, relief and all.
JSON: their triangles (found by the paints in the pre-no_cross backup: all three points 15/16 in the helmet range,
all three 16 outside it) are collapsed to zero area (all three corners the first), so no triangle index or range
moves (helmet_tris, the gore twin's cut and cap ranges). glb: the helmet meshes' marks primitives and the brassard's
cross primitive are dropped from the JSON chunk (their buffer bytes left unread)."""
import json, struct, sys, os
LODS = ("near", "mid", "far")

def json_fix(src, backup, dst):
    D = json.load(open(src)); B = json.load(open(backup))
    rep = {}
    for l in LODS:
        p = B[l]["paints"]; T = D[l]["triangles"]; f, n = B["helmet_tris"][l]
        assert T == B[l]["triangles"] and D["helmet_tris"][l] == [f, n]
        k = 0
        for i in range(len(T) // 3):
            c = T[3 * i:3 * i + 3]
            inh = f <= i < f + n
            if (inh and all(p[j] in (15, 16) for j in c)) or (not inh and all(p[j] == 16 for j in c)):
                T[3 * i + 1] = T[3 * i + 2] = c[0]; k += 1
        rep[l] = k
    old = " His M1 is plain: the four slabs"
    D["about"] = D["about"].replace(
        "the four slabs a millimetre or two proud of the shell, front, back and both sides, are painted the helmet's own coat (paint 5)",
        "the four disc-and-cross slabs that were on the shell are taken off (their triangles collapsed to zero area, so no range moves; their points left unused)")
    D["no_cross"] = {"collapsed_triangles": rep, "about": "No red cross anywhere, in paint or relief: the helmet's"
        " discs and crosses and the brassard's cross are collapsed to zero area (all three corners the same point)."}
    tmp = dst + ".tmp"
    json.dump(D, open(tmp, "w"), separators=(",", ":")); os.replace(tmp, dst)
    return rep

def glb_fix(src, backup, dst):
    """Drop, by position, the primitives that in the backup (before glb_plain) carried medic_white or medic_red on a
    helmet mesh, or medic_red on the brassard."""
    def load(path):
        b = open(path, "rb").read(); L = struct.unpack("<I", b[12:16])[0]
        return b, json.loads(b[20:20 + L]), b[20 + L:]
    b, J, rest = load(src)
    _, JB, _ = load(backup)
    nb = [m["name"] for m in JB["materials"]]
    gone = []
    for me, mb in zip(J["meshes"], JB["meshes"]):
        nm = me.get("name", ""); assert nm == mb.get("name", "") and len(me["primitives"]) == len(mb["primitives"])
        drop = ("medic_white", "medic_red") if nm.startswith("helmet") else ("medic_red",) if nm.startswith("brassard") else ()
        keep = []
        for pr, prb in zip(me["primitives"], mb["primitives"]):
            if prb.get("material") is not None and nb[prb["material"]] in drop:
                gone.append((nm, nb[prb["material"]], J["accessors"][pr["indices"]]["count"] // 3))
            else:
                keep.append(pr)
        me["primitives"] = keep
    js = json.dumps(J, separators=(",", ":")).encode(); js += b" " * (-len(js) % 4)
    out = b[:12] + struct.pack("<I", len(js)) + b"JSON" + js + rest
    out = out[:8] + struct.pack("<I", len(out)) + out[12:]
    tmp = dst + ".tmp"; open(tmp, "wb").write(out); os.replace(tmp, dst)
    return gone

if __name__ == "__main__":
    shared, backup, out = sys.argv[1:4]
    for fn in ("soldier_USA_medic.json", "soldier_USA_medic_gore.json"):
        print(fn, json_fix(os.path.join(shared, fn), os.path.join(backup, fn), os.path.join(out, fn)))
    for fn in ("soldier_USA_medic.glb", "soldier_USA_medic_LOD1.glb", "soldier_USA_medic_LOD2.glb"):
        print(fn, glb_fix(os.path.join(shared, fn), os.path.join(backup, fn), os.path.join(out, fn)))
