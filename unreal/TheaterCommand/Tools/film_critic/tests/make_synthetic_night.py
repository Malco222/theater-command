#!/usr/bin/env python3
"""
Makes a small synthetic night for testing the morning chain end to end: tiny generated pictures and facts with
deliberate faults, laid out as night.sh leaves a night on the Mac.

  air-raid     (air, ran)      opening with no defenders; a camera jump, a zoom pop and a 120 ms hitch in the cam
                               facts; a bare model; an abrupt end; a film strip with an injected jump and a flicker,
                               and a second smooth strip
  map-fleets   (map, ran)      a step run (DID, DEAD, MUTE, SAID WHY, MISSING); a fleet (Force K) with its bow against its
                               course; every wake pointing left on screen; a smooth map camera
  land-attack  (land, crashed) men all running alike; a tank rocking on the spot; errors, an ensure and a crash in
                               the log; a picture shots.yaml does not list
  sea-close    (listed in shots.yaml, never played: the night stopped)

Usage: make_synthetic_night.py OUT_DIR      (writes OUT_DIR/nights/2026-11-26/ and OUT_DIR/scenes/shots.yaml)
"""
import argparse, gzip, json, math, os, random

import numpy as np
from PIL import Image, ImageDraw, ImageFilter

NIGHT = "2026-11-26"
rng = random.Random(7)


def textured(w, h, seed, base=(110, 130, 90)):
    """A ground-like picture with detail at many sizes, so the motion reader has something to hold."""
    g = np.random.default_rng(seed)
    acc = np.zeros((h, w), dtype=np.float32)
    for size, amp in ((4, 30), (16, 40), (64, 50)):
        small = g.random((max(2, h // size), max(2, w // size))).astype(np.float32)
        acc += np.asarray(Image.fromarray((small * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC),
                          dtype=np.float32) / 255.0 * amp
    rgb = np.stack([acc + base[0] - 60, acc + base[1] - 60, acc + base[2] - 60], axis=-1)
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8))


def plane(draw, cx, cy, s, colour, mark=True):
    draw.polygon([(cx - s, cy), (cx + s, cy), (cx + s * 0.2, cy - s * 0.15), (cx - s * 0.2, cy - s * 0.15)], fill=colour)
    draw.rectangle([cx - s * 0.12, cy - s * 0.6, cx + s * 0.12, cy + s * 0.5], fill=colour)
    if mark:
        for mx in (cx - s * 0.6, cx + s * 0.6):
            draw.ellipse([mx - s * 0.12, cy - s * 0.12, mx + s * 0.12, cy + s * 0.04], fill=(30, 60, 160), outline=(240, 240, 240))


def sky_picture(path, planes, size=(800, 500)):
    w, h = size
    im = Image.new("RGB", size, (150, 180, 215))
    ground = textured(w, h // 3, 11, base=(120, 140, 100))
    im.paste(ground, (0, h - h // 3))
    d = ImageDraw.Draw(im)
    for (x, y, s, col, mark) in planes:
        plane(d, x * w, y * h, s, col, mark)
    im.save(path)


def map_picture(path, ships):
    w, h = 800, 500
    im = Image.new("RGB", (w, h), (40, 80, 130))
    d = ImageDraw.Draw(im)
    d.polygon([(0, 0), (300, 0), (250, 160), (0, 220)], fill=(120, 140, 90))
    for (x, y, ang) in ships:
        dx, dy = math.cos(ang), math.sin(ang)
        bow = (x + dx * 18, y + dy * 18)
        stern = (x - dx * 18, y - dy * 18)
        d.line([stern, (x - 70, y + 2)], fill=(235, 240, 245), width=5)  # every wake drawn to the left: the bug
        d.line([stern, bow], fill=(60, 60, 60), width=9)
    im.save(path)


def land_picture(path, men=12, tank=True):
    w, h = 800, 500
    im = textured(w, h, 21, base=(120, 135, 85))
    d = ImageDraw.Draw(im)
    d.rectangle([0, 0, w, h // 4], fill=(170, 185, 200))
    for i in range(men):
        x, y = 120 + i * 45, 300 + (i % 3) * 12
        d.rectangle([x, y, x + 8, y + 26], fill=(70, 75, 50))
    if tank:
        d.rectangle([520, 220, 640, 270], fill=(80, 85, 70))
        d.rectangle([560, 200, 610, 225], fill=(80, 85, 70))
    im.save(path)


def strip(scene_dir, of, n, t0, fps, facts, jump_at=None, flicker_at=None, seed=5):
    """A strip of a smooth pan over ground, 960 px wide, with an injected lurch and a flicker."""
    big = textured(2400, 600, seed)
    x = 0.0
    for i in range(n):
        x += 9.0  # 9 px a frame at 960 wide: a steady pan
        if jump_at is not None and i == jump_at:
            x += 70.0  # the lurch: the view skips 70 px in one frame and carries on from there
        fr = big.crop((int(x), 30, int(x) + 960, 570))
        if flicker_at is not None and i == flicker_at:
            fr = Image.eval(fr, lambda v: min(255, v + 45))
        name = f"strip-{of}-{i:04d}.jpg"
        fr.save(os.path.join(scene_dir, name), quality=88)
        facts.append({"t": round(t0 + i / fps, 4), "k": "strip", "file": name, "of": of})


def cam_lines(t0, seconds, fps, scene, yaw0=0.0, yaw_rate=12.0, jump_at=None, pop_at=None, hitch_at=None,
              why="overview", subject=None):
    out = []
    t, yaw, fov = t0, yaw0, 60.0
    k = 0
    while t < t0 + seconds:
        dt = 1000.0 / fps
        if hitch_at is not None and abs(t - hitch_at) < 0.5 / fps:
            dt = 120.0
        t += dt / 1000.0
        yaw += yaw_rate * dt / 1000.0
        if jump_at is not None and abs(t - jump_at) < 0.5 / fps:
            yaw += 2.5  # 2.5 degrees in one frame on a 60 degree lens = 80 px on 1920
        if pop_at is not None and abs(t - pop_at) < 0.5 / fps:
            fov = 45.0  # the lens snaps from 60 to 45 degrees
        line = {"t": round(t, 4), "k": "cam", "scene": scene, "eye": [1000.0 + k * 2.0, 500.0, 800.0],
                "rot": [-12.0, round(yaw, 4), 0.0], "fov": fov, "dt": round(dt, 2), "why": why}
        if subject:
            line["subject"] = subject
        out.append(line)
        k += 1
    return out


def make(out):
    night = os.path.join(out, "nights", NIGHT)
    os.makedirs(night, exist_ok=True)
    scenes_json = []

    # ---- air-raid ------------------------------------------------------------------------------------------------
    sd = os.path.join(night, "air-raid")
    os.makedirs(sd, exist_ok=True)
    facts = [{"t": 0.5, "k": "run", "scene": "air", "key": "raid-hampshire-1940", "window": [1600, 1000]}]
    sky_picture(os.path.join(sd, "air-raid-00-flyover-raid.png"),
                [(0.3, 0.3, 40, (60, 60, 55), True), (0.55, 0.25, 40, (60, 60, 55), True)])
    sky_picture(os.path.join(sd, "air-raid-05-dogfight.png"),
                [(0.3, 0.4, 70, (90, 100, 80), True), (0.68, 0.42, 70, (100, 95, 85), False)])
    im = Image.new("RGB", (800, 500), (30, 32, 36))
    d = ImageDraw.Draw(im)
    d.rectangle([200, 150, 600, 350], fill=(225, 220, 205))
    d.text((230, 180), "VICTORY - the raid is turned back", fill=(20, 20, 20))
    im.save(os.path.join(sd, "air-raid-08-after-card.png"))
    for t, name in ((3.0, "air-raid-00-flyover-raid.png"), (14.0, "air-raid-05-dogfight.png"),
                    (30.0, "air-raid-08-after-card.png")):
        facts.append({"t": t, "k": "still", "name": name[9:-4], "file": name, "eye": [0, 0, 0], "rot": [0, 0, 0], "fov": 60})
    # the opening: attackers in frame, defenders only 20% of the time
    for i in range(32):
        t = 1.0 + i * 0.25
        facts.append({"t": t, "bt": t * 10, "k": "planes", "shot": "00-flyover-raid",
                      "inframe": {"attack": 6, "defend": 2 if i % 5 == 0 else 0}, "nearest_m": {"attack": 300, "defend": 4000}})
    facts.append({"t": 14.0, "bt": 140, "k": "planes", "shot": "05-dogfight", "inframe": {"attack": 2, "defend": 3}})
    # the camera: 30 fps for 20 s, a jump at 5 s, a zoom pop at 8 s, a 120 ms hitch at 10 s
    facts += cam_lines(1.0, 20.0, 30, "air", jump_at=5.0, pop_at=8.0, hitch_at=10.0, why="overview")
    # markings: the Bf 110 is bare
    for typ, nat, m in (("Spitfire I", "GB", 3), ("Hurricane I", "GB", 3), ("Bf 109 E", "DE", 4),
                        ("Bf 110 C", "DE", 0), ("He 111 H", "DE", 4), ("Do 17 Z", "DE", 4)):
        facts.append({"t": 2.0, "k": "marks", "kind": "plane", "type": typ, "nation": nat, "year": 1940, "month": 8, "marks": m})
    facts.append({"t": 2.0, "bt": 20, "k": "look", "look": "southern-england-summer", "month": 8, "year": 1940,
                  "lat": 50.9, "lon": -1.3, "snow": False, "leafless": False, "hour": 14, "weather": "fair"})
    # the end: the raiders broke at battle 300 s (real 26 s), the card at real 30 s, no offer to go on
    facts.append({"t": 26.0, "bt": 300, "k": "planes", "shot": "07-turning-back", "inframe": {"attack": 4, "defend": 2}})
    facts.append({"t": 30.0, "bt": 310, "k": "end", "how": "rout", "broke_at": 300, "ended_at": 310, "card_at": 30.0,
                  "offered_continue": False})
    strip(sd, "00-flyover-raid", 40, 3.0, 30.0, facts, jump_at=20, flicker_at=30, seed=5)
    strip(sd, "05-dogfight", 30, 14.0, 30.0, facts, seed=9)
    write_facts(sd, facts)
    write_log(sd, ["LogTheater: Air battle: Hampshire, raid (1940-08), 12 views.",
                   "LogTheater: Speed: 10.0 s, 41.3 frames a second (worst frame 121.0 ms; 14 of 413 over 33 ms, 3 over 50); game, draw, graphics chip",
                   "LogTheater: Speed: a long frame, 121 ms (game 8.1, draw 96.3, graphics chip 14.2 ms) after a cut; its jobs in ms ...: shapes 80",
                   "LogTheater: Speed: 10.0 s, 44.8 frames a second (worst frame 38.0 ms; 4 of 448 over 33 ms, 0 over 50); game, draw, graphics chip",
                   "LogTheater: Warning: Air battle: no stand-in shape for Fighter:SWE, built one mid-battle"])
    open(os.path.join(sd, "flags.txt"), "w").write("-TheaterAirBattle -TheaterShots={OUT} -FilmCriticStrip=30,1.3\n")
    scenes_json.append({"scene": "air-raid", "seconds": 212, "exit": 0, "pictures": 73, "note": ""})

    # ---- map-fleets ----------------------------------------------------------------------------------------------
    sd = os.path.join(night, "map-fleets")
    os.makedirs(sd, exist_ok=True)
    facts = [{"t": 0.4, "k": "run", "scene": "map", "key": "med-1941.steps.txt", "window": [1600, 1000]}]
    ships = [(420, 300, 0.0), (560, 380, math.pi / 2), (650, 240, math.pi), (480, 420, -math.pi / 4)]
    for i in range(1, 5):
        map_picture(os.path.join(sd, f"step-{i:03d}.png"), ships[: 1 + i % 4])
    steps = [
        {"n": 1, "line": 3, "verb": "start", "target": "IT 1941-06 seed 4", "verdict": "DID", "pass": None, "fps_low": 38, "fps_mid": 52, "game_ms": 4.1, "draw_ms": 9.0, "chip_ms": 14.2, "picture": "step-001.png"},
        {"n": 2, "line": 4, "verb": "select-sea", "target": "Ionian Sea", "verdict": "DID", "pass": True, "fps_low": 40, "fps_mid": 55, "picture": "step-002.png"},
        {"n": 3, "line": 5, "verb": "order-sea", "target": "Malta convoy", "expect": "moves", "verdict": "DEAD", "pass": False, "note": "nothing changed", "fps_low": 41, "fps_mid": 54, "picture": "step-003.png"},
        {"n": 4, "line": 6, "verb": "press", "target": "Sail at once", "verdict": "MUTE", "pass": None, "note": "dim, no tooltip", "fps_low": 39, "fps_mid": 53},
        {"n": 5, "line": 7, "verb": "press", "target": "Escort", "verdict": "SAID WHY", "said": True, "words": "No escort can reach Malta this month", "fps_low": 40, "fps_mid": 54, "picture": "step-004.png"},
        {"n": 6, "line": 8, "verb": "look-at", "target": "Tyrrhenian Sea", "verdict": "MISSING", "note": "no such place", "fps_low": 42, "fps_mid": 55},
    ]
    with open(os.path.join(sd, "steps.jsonl"), "w") as fh:
        for s in steps:
            fh.write(json.dumps(s) + "\n")
    # four fleets sailing four ways; Force K's bow points against its course; every wake drawn to the left
    fleets = [("f1", "Force H", "GB", (1, 0), (1, 0)), ("f2", "Malta convoy", "GB", (0, 1), (0, 1)),
              ("f3", "Italian 1st Squadron", "IT", (-1, 0), (-1, 0)), ("f4", "Force K", "GB", (0.7071, -0.7071), (-0.7071, 0.7071))]
    for i in range(12):
        t = 2.0 + i * 0.5
        for fid, name, nat, course, bow in fleets:
            bx, by = 400 + rng.randint(0, 300), 250 + rng.randint(0, 200)
            facts.append({"t": t, "k": "fleet", "id": fid, "name": name, "nation": nat, "sailing": True,
                          "progress": i / 12, "course": list(course), "bow": list(bow), "wake": 2, "camyaw": 0.0,
                          "bowpx": [bx, by], "tailpx": [bx - 60, by + 1]})
    for i, n in enumerate(("step-001.png", "step-002.png", "step-003.png", "step-004.png")):
        facts.append({"t": 1.0 + i * 3, "k": "still", "name": n[:-4], "file": n, "eye": [0, 0, 0], "rot": [-60, 0, 0], "fov": 50})
    facts += cam_lines(1.0, 6.0, 60, "map", yaw_rate=4.0, why="pan")
    write_facts(sd, facts)
    write_log(sd, ["LogTheater: Steps: 3 (line 5) order-sea Malta convoy: DEAD",
                   "LogTheater: Photographer: step-001 at 54.2 frames a second (1600x1000; game, draw, graphics chip)",
                   "LogTheater: Speed: 10.0 s, 53.9 frames a second (worst frame 31.0 ms; 0 of 539 over 33 ms, 0 over 50); game, draw, graphics chip"])
    open(os.path.join(sd, "flags.txt"), "w").write("-TheaterShots={OUT} -TheaterSteps={KIT}/scenes/med-1941.steps.txt\n")
    scenes_json.append({"scene": "map-fleets", "seconds": 148, "exit": 0, "pictures": 4, "note": ""})

    # ---- land-attack (crashed) -----------------------------------------------------------------------------------
    sd = os.path.join(night, "land-attack")
    os.makedirs(sd, exist_ok=True)
    facts = [{"t": 0.6, "k": "run", "scene": "land", "key": "kiev-1941-attack", "window": [1600, 1000]}]
    land_picture(os.path.join(sd, "land-attack-01-opening.png"))
    land_picture(os.path.join(sd, "land-attack-05-the-tanks.png"), men=3)
    for i, n in enumerate(("land-attack-01-opening.png", "land-attack-05-the-tanks.png")):
        facts.append({"t": 4.0 + i * 20, "k": "still", "name": n[12:-4], "file": n, "eye": [0, 0, 0], "rot": [0, 0, 0], "fov": 55})
    for s in range(30):
        facts.append({"t": 5.0 + s, "bt": 5.0 + s, "k": "men", "moving": 40, "run_p10": 3.0, "run_p50": 3.1,
                      "run_p90": 3.2, "rates": 1, "capped": False})
    for i in range(40):
        t = 5.0 + i * 0.5
        facts.append({"t": t, "bt": t, "k": "vehicle", "id": "t34-1", "type": "T-34", "nation": "SU",
                      "heading": 90 + (6 if i % 2 else -6), "speed": 0.1, "turret": 0})
        facts.append({"t": t, "bt": t, "k": "vehicle", "id": "pz3-1", "type": "Pz III", "nation": "DE",
                      "heading": 270 + i * 0.5, "speed": 4.0, "turret": 0})
    facts.append({"t": 2.0, "bt": 2, "k": "look", "look": "ukraine-summer", "month": 7, "year": 1941, "lat": 50.4,
                  "lon": 30.5, "snow": True, "leafless": False, "hour": 9, "weather": "fair"})
    for typ, m in (("T-34", 2), ("Pz III", 3), ("man", 1)):
        facts.append({"t": 3.0, "k": "marks", "kind": "tank" if typ != "man" else "man", "type": typ,
                      "nation": "SU" if typ == "T-34" else "DE", "year": 1941, "month": 7, "marks": m})
    write_facts(sd, facts)
    write_log(sd, ["LogTheater: Land battle: 10 s of battle, wide shot; 37 frames a second, the scene's slowest frame 88.0 ms; a frame's cores 9.10 ms, men 3.20, tanks 1.10, ground 2.00; shots heard 12, takes 4 (dropped 0, on the screen 4), cracks 0, falls 1",
                   "LogTheater: Error: LandMen: no gait clip for 'run-b' on SU rifleman, using 'run-a'",
                   "LogTheater: Error: LandMen: no gait clip for 'run-b' on SU rifleman, using 'run-a'",
                   "LogTheater: Warning: LandScape: look ukraine-summer has snow on in July",
                   "LogOutputDevice: Error: Ensure condition failed: Track.Num() > 0 [File:LandTanks.cpp] [Line: 812]",
                   "LogCore: Error: === Critical error: ===",
                   "LogCore: Error: Fatal error: [File:LandTanks.cpp] [Line: 830] Array index out of bounds: 3 from an array of size 3"])
    open(os.path.join(sd, "flags.txt"), "w").write("-TheaterLandBattle=kiev-1941-attack -TheaterShots={OUT}\n")
    scenes_json.append({"scene": "land-attack", "seconds": 61, "exit": 134, "pictures": 2, "note": "crashed"})

    with open(os.path.join(night, "night.json"), "w") as fh:
        json.dump({"night": NIGHT, "build": "5e151855a", "began": 1764194400, "ended": 1764195000,
                   "stopped": "a key or the mouse was used", "scenes": scenes_json}, fh)
    with open(os.path.join(night, "attempts.log"), "w") as fh:
        fh.write("2026-11-26 21:00:02 not run: the Mac was in use 40s ago\n"
                 "2026-11-26 21:30:01 run begins on build 5e151855a\n"
                 "2026-11-26 21:40:02 stopping before sea-close: a key or the mouse was used\n")

    # ---- shots.yaml ----------------------------------------------------------------------------------------------
    os.makedirs(os.path.join(out, "scenes"), exist_ok=True)
    with open(os.path.join(out, "scenes", "shots.yaml"), "w") as fh:
        fh.write("""# Test shots: per scene its kind, theater code, year, and its pictures (file name or glob) with tags.
scenes:
  air-raid:
    kind: air
    theater: T1
    year: 1940
    month: 8
    about: A German raid on Hampshire, August 1940
    pictures:
      - {file: "*00-flyover-raid*", tags: [air-flyover], about: "the raid's opening flyover, raiders and the guard", look: true}
      - {file: "*05-dogfight*", tags: [air-battle, air-chase], about: "fighters meet over the coast"}
      - {file: "*08-after-card*", tags: [ui-card], about: "the result card"}
  map-fleets:
    kind: map
    theater: T6
    year: 1941
    month: 6
    about: The Mediterranean in June 1941, fleets at sea
    pictures:
      - {file: "step-00[12].png", tags: [map, map-fleet-sailing], about: "fleets sailing"}
      - {file: "step-003.png", tags: [map, map-fleet-rest], about: "a convoy ordered to sail"}
  land-attack:
    kind: land
    theater: T2
    year: 1941
    month: 7
    pictures:
      - {file: "*01-opening*", tags: [land-opening, land-close-men], about: "the opening over the advance"}
  sea-close:
    kind: sea
    theater: T6
    year: 1942
    pictures:
      - {file: "*", tags: [sea-battle, sea-close]}
""")
    return night


def write_facts(sd, facts):
    facts.sort(key=lambda d: d["t"])
    with open(os.path.join(sd, "critic-facts.jsonl"), "w") as fh:
        for d in facts:
            fh.write(json.dumps(d) + "\n")


def write_log(sd, lines):
    with gzip.open(os.path.join(sd, "game.log.gz"), "wt") as fh:
        for i, ln in enumerate(lines):
            fh.write(f"[2026.11.26-21.31.{i:02d}:000][{i:3d}]{ln}\n")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    a = ap.parse_args()
    print(make(a.out))
