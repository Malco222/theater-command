"""The medic's .glb files without the red cross: the helmet's disc and cross primitives take the helmet's material, the
brassard's cross the brassard's white; the then unused medic_red material is dropped. Only the JSON chunk changes."""
import json, struct, sys, os

def fix(src, dst):
    b = open(src, "rb").read()
    L = struct.unpack("<I", b[12:16])[0]
    J = json.loads(b[20:20 + L])
    rest = b[20 + L:]
    names = [m["name"] for m in J["materials"]]
    helmet, white, red = names.index("helmet"), names.index("medic_white"), names.index("medic_red")
    moved = []
    for me in J["meshes"]:
        nm = me.get("name", "")
        for pr in me["primitives"]:
            m = pr.get("material")
            if nm.startswith("helmet") and m in (white, red):
                pr["material"] = helmet; moved.append((nm, names[m], "helmet"))
            elif nm.startswith("brassard") and m == red:
                pr["material"] = white; moved.append((nm, "medic_red", "medic_white"))
    used = {pr.get("material") for me in J["meshes"] for pr in me["primitives"]}
    assert red not in used, "medic_red still used"
    del J["materials"][red]
    for me in J["meshes"]:
        for pr in me["primitives"]:
            if pr.get("material") is not None and pr["material"] > red:
                pr["material"] -= 1
    js = json.dumps(J, separators=(",", ":")).encode()
    js += b" " * (-len(js) % 4)
    out = b[:12] + struct.pack("<I", len(js)) + b"JSON" + js + rest
    out = out[:8] + struct.pack("<I", len(out)) + out[12:]
    open(dst, "wb").write(out)
    return moved

if __name__ == "__main__":
    for src in sys.argv[2:]:
        print(os.path.basename(src), fix(src, os.path.join(sys.argv[1], os.path.basename(src))))
