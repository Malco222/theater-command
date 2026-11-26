#!/usr/bin/env python3
"""
Film critic: the morning check of one night. Reads an unpacked night folder (one subfolder per scene, as night.sh
leaves it), rules.yaml and scenes/shots.yaml, and writes night-check.json with, per scene:

  ran        whether it ran and how it ended (night.json and the folder)
  steps      the step player's verdicts: DID / SAID WHY / DEAD / MUTE / MISSING / ERROR counts and every non-DID line
  speed      frame rate and long frames from steps.jsonl and the game log's Speed / battle lines
  log        LogTheater errors and warnings, ensures and crash lines in game.log.gz (each distinct line once, counted)
  strips     each film strip (frames grouped by the still or step they follow) measured by measure_clip, judged
             against every rule with `clip:` limits that fits its tags; a contact sheet image for each
  facts      check_facts.py's checks named by the rules that fit the scene
  rules      per rule that fits the scene: the machine verdict from its clip and facts parts
  pictures   every still, with its tags, a thumbnail (<= 640 px wide) and the picture rules that apply

and the JUDGE LIST: for every picture, the rules whose picture question a vision judge must answer for it.

Usage:
  check_night.py NIGHT_DIR [--rules rules.yaml] [--shots scenes/shots.yaml] [--out WORK/night-check.json]
                 [--img-dir WORK/img] [--look-scores] [--only scene,scene]
Needs numpy, Pillow, PyYAML. Run with python3 -I: the night folder came from the Mac as zips.
"""
import argparse, datetime, fnmatch, glob, gzip, json, math, os, re, sys

import yaml
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import measure_clip  # noqa: E402
import check_facts  # noqa: E402

VERDICTS = ["DID", "SAID WHY", "DEAD", "MUTE", "MISSING", "ERROR"]
# Tags a picture gets from its scene's kind when shots.yaml does not list it (and every battle picture gets
# every-battle). Rules name the finer tags (air-flyover, land-close-men, map-fleet-sailing ...): an unlisted picture is
# judged only against the broad rules, never against a rule for a shot it may not be.
KIND_TAGS = {"map": ["map"], "air": ["air-battle"], "land": ["land-advance"], "sea": ["sea-battle"]}
BATTLE_KINDS = ("air", "land", "sea")
PICTURE_EXT = (".png", ".jpg", ".jpeg")
THUMB_W = 640


# ---- small readers ---------------------------------------------------------------------------------------------
def read_yaml(path):
    if not path or not os.path.exists(path):
        return None
    with open(path, encoding="utf-8") as fh:
        return yaml.safe_load(fh)


def read_night_json(path):
    """night.json as night.sh writes it. A note with a stray quote can break the JSON: then the scene entries are
    picked out one by one."""
    if not os.path.exists(path):
        return None
    txt = open(path, encoding="utf-8", errors="replace").read()
    try:
        return json.loads(txt)
    except ValueError:
        pass
    out = {"broken": True, "scenes": []}
    for key in ("night", "build", "stopped"):
        m = re.search(r'"%s":"([^"]*)"' % key, txt)
        if m:
            out[key] = m.group(1)
    for m in re.finditer(r'\{"scene":"([^"]+)","seconds":(\d+),"exit":(\d+),"pictures":(\d+),"note":"(.*?)"\}(?=,|\s*\])', txt, re.S):
        out["scenes"].append({"scene": m.group(1), "seconds": int(m.group(2)), "exit": int(m.group(3)),
                              "pictures": int(m.group(4)), "note": m.group(5)})
    return out


def read_jsonl(path):
    rows, bad = [], 0
    if not os.path.exists(path):
        return None, 0
    with open(path, encoding="utf-8", errors="replace") as fh:
        for raw in fh:
            raw = raw.strip()
            if not raw:
                continue
            try:
                rows.append(json.loads(raw))
            except ValueError:
                bad += 1
    return rows, bad


def read_log(scene_dir):
    for name in ("game.log.gz", "game.log"):
        p = os.path.join(scene_dir, name)
        if os.path.exists(p):
            try:
                if name.endswith(".gz"):
                    with gzip.open(p, "rt", encoding="utf-8", errors="replace") as fh:
                        return fh.read().splitlines()
                return open(p, encoding="utf-8", errors="replace").read().splitlines()
            except (OSError, EOFError) as e:
                return [f"(the game log could not be read: {e})"]
    return None


def median(xs):
    xs = sorted(x for x in xs if x is not None)
    if not xs:
        return None
    m = len(xs) // 2
    return xs[m] if len(xs) % 2 else (xs[m - 1] + xs[m]) / 2


# ---- shots.yaml ------------------------------------------------------------------------------------------------
def shots_table(doc):
    """{scene: {kind, theater, year, month, pictures: [{match, tags, about, look}]}} from a tolerant shots.yaml."""
    out = {}
    if not doc:
        return out
    scenes = doc.get("scenes", doc) if isinstance(doc, dict) else doc
    items = scenes.items() if isinstance(scenes, dict) else [((s or {}).get("name") or (s or {}).get("scene"), s) for s in scenes or []]
    for name, s in items:
        if not name or not isinstance(s, dict):
            continue
        pics = []
        for p in s.get("pictures") or []:
            if isinstance(p, str):
                p = {"file": p}
            match = p.get("file") or p.get("glob") or p.get("name")
            if not match:
                continue
            tags = p.get("tags") or []
            if isinstance(tags, str):
                tags = [t.strip() for t in tags.split(",") if t.strip()]
            pics.append({"match": str(match), "tags": list(tags),
                         "about": p.get("about") or p.get("description") or p.get("desc") or "",
                         "look": bool(p.get("look", False))})
        out[str(name)] = {"kind": s.get("kind"), "theater": s.get("theater") or s.get("theatre") or s.get("code"),
                          "year": s.get("year"), "month": s.get("month"), "about": s.get("about") or "",
                          "tags": list(s.get("tags") or []), "pictures": pics}
    return out


def picture_tags(scene_shots, kind, fname):
    """(tags, about, listed, look) for a picture file name."""
    stem = os.path.splitext(fname)[0]
    always = {"every-picture"} | ({"every-battle"} if kind in BATTLE_KINDS else set())
    for p in (scene_shots or {}).get("pictures", []):
        m = p["match"]
        if fnmatch.fnmatch(fname, m) or fnmatch.fnmatch(stem, m) or fname == m or stem == os.path.splitext(m)[0]:
            return sorted(set(p["tags"]) | always), p["about"], True, p["look"]
    base = list(KIND_TAGS.get(kind or "", []))
    base += (scene_shots or {}).get("tags", [])
    return sorted(set(base) | always), "", False, False


def scene_kind(name, shots, facts_run, flags):
    if shots and shots.get("kind"):
        return str(shots["kind"])
    if facts_run and facts_run.get("scene"):
        return str(facts_run["scene"])
    f = flags or ""
    for key, k in (("-TheaterAirBattle", "air"), ("-TheaterLandBattle", "land"), ("-TheaterLandScript", "land"),
                   ("-TheaterSeaBattle", "sea"), ("-TheaterSteps", "map")):
        if key in f:
            return k
    for k in ("air", "land", "sea", "map"):
        if k in name.lower():
            return k
    return None


# ---- rules -----------------------------------------------------------------------------------------------------
def load_rules(path):
    doc = read_yaml(path) or {}
    rules = []
    for r in doc.get("rules") or []:
        if not isinstance(r, dict) or not r.get("id"):
            continue
        tags = r.get("tags") or []
        if isinstance(tags, str):
            tags = [tags]
        rules.append(dict(r, tags=list(tags)))
    return rules, doc.get("owners") or {}


def applies(rule, tags):
    rt = set(rule["tags"])
    return "every-picture" in rt or bool(rt & set(tags))


# ---- steps -----------------------------------------------------------------------------------------------------
def read_steps(path):
    rows, bad = read_jsonl(path)
    if rows is None:
        return None
    counts = {v: 0 for v in VERDICTS}
    other = []
    lines = []
    suspect_from = None
    for r in rows:
        v = str(r.get("verdict") or "").upper().replace("_", " ")
        if v in counts:
            counts[v] += 1
        else:
            counts.setdefault(v or "(none)", 0)
            counts[v or "(none)"] += 1
        if v == "MISSING" and str(r.get("verb", "")).split(":")[0] in ("start", "select-unit", "press") and suspect_from is None:
            suspect_from = r.get("n")
        if v != "DID" or r.get("pass") is False:
            lines.append({k: r.get(k) for k in ("n", "line", "verb", "target", "expect", "verdict", "said", "pass",
                                                "note", "picture") if r.get(k) is not None}
                         | {"words": str(r.get("words") or "")[:300]})
    for ln in lines:
        if suspect_from is not None and (ln.get("n") or 0) > suspect_from:
            ln["suspect"] = True
    fps_low = [r.get("fps_low") for r in rows if isinstance(r.get("fps_low"), (int, float))]
    fps_mid = [r.get("fps_mid") for r in rows if isinstance(r.get("fps_mid"), (int, float))]
    return {"steps": len(rows), "bad_lines": bad, "counts": counts, "not_did": lines,
            "first_missing_key_step": suspect_from,
            "fps_low_min": min(fps_low) if fps_low else None, "fps_mid_median": median(fps_mid),
            "game_ms_median": median([r.get("game_ms") for r in rows if isinstance(r.get("game_ms"), (int, float))]),
            "draw_ms_median": median([r.get("draw_ms") for r in rows if isinstance(r.get("draw_ms"), (int, float))]),
            "chip_ms_median": median([r.get("chip_ms") for r in rows if isinstance(r.get("chip_ms"), (int, float))])}


# ---- the game log ----------------------------------------------------------------------------------------------
PREFIX = re.compile(r"^\[[^\]]*\]\[\s*\d+\]")
RX_SPEED = re.compile(r"Speed: ([\d.]+) s, ([\d.]+) frames a second \(worst frame ([\d.]+) ms; (\d+) of (\d+) over 33 ms")
RX_LONG = re.compile(r"Speed: a long frame, ([\d.]+) ms \(game ([\d.]+), draw ([\d.]+), graphics chip ([\d.]+) ms\)")
RX_LAND = re.compile(r"Land battle: ([\d.]+) s of battle, .*?; ([\d.]+) frames a second, the scene's slowest frame ([\d.]+) ms")
RX_PHOTO = re.compile(r"Photographer: .* at ([\d.]+) frames a second")
RX_SEA = re.compile(r"Sea battle: .*frame.*?worst ([\d.]+)")
CRASH_WORDS = ("=== Critical error", "Fatal error", "Assertion failed", "Unhandled Exception", "SIGSEGV", "SIGABRT",
               "SIGBUS", "appError called", "LowLevelFatalError", "Crash in runnable thread", "EXCEPTION_ACCESS_VIOLATION")


def read_game_log(lines):
    if lines is None:
        return None, None
    found = {"error": {}, "warning": {}, "ensure": {}, "crash": {}, "other_errors": 0}
    speed = {"windows": [], "long_frames": [], "land": [], "photographer": []}
    for ln in lines:
        msg = PREFIX.sub("", ln).strip()
        kind = None
        if "LogTheater: Error:" in msg:
            kind = "error"
        elif "LogTheater: Warning:" in msg:
            kind = "warning"
        elif "Ensure condition failed" in msg or "ensure(" in msg.lower() and "Error" in msg:
            kind = "ensure"
        elif any(w in msg for w in CRASH_WORDS):
            kind = "crash"
        elif re.search(r"Log\w+: Error:", msg):
            found["other_errors"] += 1
        if kind:
            key = re.sub(r"\s+", " ", msg)[:400]
            found[kind][key] = found[kind].get(key, 0) + 1
        m = RX_SPEED.search(msg)
        if m:
            speed["windows"].append({"s": float(m.group(1)), "fps": float(m.group(2)), "worst_ms": float(m.group(3)),
                                     "over33": int(m.group(4)), "frames": int(m.group(5))})
        m = RX_LONG.search(msg)
        if m:
            speed["long_frames"].append({"ms": float(m.group(1)), "game": float(m.group(2)), "draw": float(m.group(3)),
                                         "chip": float(m.group(4))})
        m = RX_LAND.search(msg)
        if m:
            speed["land"].append({"bt": float(m.group(1)), "fps": float(m.group(2)), "slowest_ms": float(m.group(3))})
        m = RX_PHOTO.search(msg)
        if m:
            speed["photographer"].append(float(m.group(1)))
    log = {k: [{"line": l, "times": n} for l, n in sorted(v.items(), key=lambda kv: -kv[1])[:40]]
           for k, v in found.items() if isinstance(v, dict)}
    log["counts"] = {k: sum(v.values()) for k, v in found.items() if isinstance(v, dict)}
    log["counts"]["other_errors"] = found["other_errors"]
    log["lines"] = len(lines)
    return log, speed


def speed_summary(steps, logspeed, cam):
    s = {}
    if steps:
        s.update(steps_fps_low_min=steps.get("fps_low_min"), steps_fps_mid_median=steps.get("fps_mid_median"),
                 game_ms=steps.get("game_ms_median"), draw_ms=steps.get("draw_ms_median"), chip_ms=steps.get("chip_ms_median"))
    if logspeed:
        fps = [w["fps"] for w in logspeed["windows"]] + [w["fps"] for w in logspeed["land"]] + logspeed["photographer"]
        if fps:
            s["log_fps_min"] = round(min(fps), 1)
            s["log_fps_median"] = round(median(fps), 1)
        worst = [w["worst_ms"] for w in logspeed["windows"]] + [w["slowest_ms"] for w in logspeed["land"]] + \
                [w["ms"] for w in logspeed["long_frames"]]
        if worst:
            s["log_worst_frame_ms"] = round(max(worst), 1)
        lf = logspeed["long_frames"]
        s["log_long_frames"] = len(lf)
        s["log_long_over_100ms"] = sum(1 for w in lf if w["ms"] > 100)
        if lf:
            w = max(lf, key=lambda w: w["ms"])
            s["log_longest"] = w
            parts = {"game": w["game"], "draw": w["draw"], "graphics chip": w["chip"]}
            s["log_longest_bound_by"] = max(parts, key=parts.get)
    if cam and cam.get("status") != "not-measured":
        m = cam.get("measured", {})
        s.update(facts_frame_ms_median=m.get("frame_ms_median"), facts_frame_ms_worst=m.get("frame_ms_worst"),
                 hitches_over_50ms=m.get("hitches_over_50ms"), hitches_over_100ms=m.get("hitches_over_100ms"))
    fps_vals = [v for v in (s.get("log_fps_median"), s.get("steps_fps_mid_median")) if v]
    s["fps"] = round(min(fps_vals), 1) if fps_vals else None
    return s


# ---- strips ----------------------------------------------------------------------------------------------------
RX_STRIP = re.compile(r"^strip-(.+)-(\d{3,})\.(jpe?g|png)$", re.I)


def strip_groups(scene_dir, facts_lines):
    files = sorted(f for f in os.listdir(scene_dir) if f.lower().startswith("strip-") and f.lower().endswith(PICTURE_EXT))
    info = {}
    for d in facts_lines or []:
        if d.get("k") == "strip" and d.get("file"):
            info[os.path.basename(str(d["file"]))] = (check_facts.num(d.get("t")), d.get("of"))
    groups = {}
    for f in files:
        m = RX_STRIP.match(f)
        of = info.get(f, (None, None))[1] or (m.group(1) if m else "strip")
        frame = int(m.group(2)) if m else len(groups.get(of, []))
        groups.setdefault(str(of), []).append((frame, f, info.get(f, (None, None))[0]))
    for of in groups:
        groups[of].sort()
    return groups


def strip_fps(flags):
    m = re.search(r"-FilmCriticStrip=([\d.]+),([\d.]+)", flags or "")
    return float(m.group(1)) if m else None


# ---- images ----------------------------------------------------------------------------------------------------
def thumbnail(src, dst, width=THUMB_W):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    try:
        with Image.open(src) as im:
            im = im.convert("RGB")
            if im.width > width:
                im = im.resize((width, round(im.height * width / im.width)), Image.LANCZOS)
            im.save(dst, "JPEG", quality=82, optimize=True)
            return [im.width, im.height]
    except (OSError, ValueError) as e:
        return {"error": str(e)}


def safe(name):
    return re.sub(r"[^A-Za-z0-9._-]+", "-", name).strip("-") or "x"


# ---- one scene -------------------------------------------------------------------------------------------------
def check_scene(name, night_dir, entry, shots, rules, img_dir, out_dir, look_all, stopped_night):
    sdir = os.path.join(night_dir, name)
    exists = os.path.isdir(sdir)
    flags = open(os.path.join(sdir, "flags.txt"), encoding="utf-8", errors="replace").read().strip() \
        if exists and os.path.exists(os.path.join(sdir, "flags.txt")) else None
    ran = {"folder": exists}
    if entry:
        ran.update({k: entry.get(k) for k in ("seconds", "exit", "pictures", "note")})
    note = (entry or {}).get("note") or ""
    if not entry and not exists:
        ran["status"] = "did not run"
        ran["why"] = f"the night stopped before it ({stopped_night})" if stopped_night else "it was not played last night"
    elif "crashed" in note:
        ran["status"] = "crashed"
    elif "timed out" in note:
        ran["status"] = "timed out"
    elif note.startswith("stopped"):
        ran["status"] = "stopped"
        ran["why"] = note
    elif entry and entry.get("exit") not in (0, None):
        ran["status"] = "ended with an error"
        ran["why"] = f"the game ended with code {entry.get('exit')}"
    else:
        ran["status"] = "ran"
    scene = {"name": name, "ran": ran, "flags": flags}
    sh = shots.get(name) or {}
    scene.update(theater=sh.get("theater"), year=sh.get("year"), month=sh.get("month"), about=sh.get("about"),
                 listed_in_shots=bool(sh))
    if not exists:
        scene.update(kind=sh.get("kind"), pictures=[], strips=[], facts={}, rules=[], steps=None, speed={}, log=None)
        return scene, []
    facts_path = os.path.join(sdir, "critic-facts.jsonl")
    facts_lines = check_facts.load_facts(facts_path)[0] if os.path.exists(facts_path) else None
    run_line = next((d for d in facts_lines or [] if d.get("k") == "run"), None)
    kind = scene_kind(name, sh, run_line, flags)
    scene["kind"] = kind
    # steps, log, speed
    steps = read_steps(os.path.join(sdir, "steps.jsonl"))
    scene["steps"] = steps
    log, logspeed = read_game_log(read_log(sdir))
    scene["log"] = log
    if log and log["counts"].get("crash") and ran["status"] == "ran":
        ran["status"] = "crashed"
    # pictures and their tags
    files = sorted(f for f in os.listdir(sdir) if f.lower().endswith(PICTURE_EXT) and not f.lower().startswith("strip-"))
    pictures, scene_tags = [], set(KIND_TAGS.get(kind or "", [])) | {"every-picture"} | set(sh.get("tags", []))
    if kind in BATTLE_KINDS:
        scene_tags.add("every-battle")
    tag_by_stem = {}
    for f in files:
        tags, about, listed, look = picture_tags(sh, kind, f)
        scene_tags |= set(tags)
        thumb_rel = os.path.join("img", safe(name), safe(os.path.splitext(f)[0]) + ".jpg")
        size = thumbnail(os.path.join(sdir, f), os.path.join(img_dir, safe(name), safe(os.path.splitext(f)[0]) + ".jpg"))
        pictures.append({"file": f"{name}/{f}", "name": f, "thumb": rel_img(img_dir, out_dir, thumb_rel), "thumb_size": size,
                         "tags": tags, "about": about, "listed": listed, "look": look or look_all})
        tag_by_stem[os.path.splitext(f)[0]] = tags
    scene["pictures"] = pictures
    # strips
    groups = strip_groups(sdir, facts_lines)
    if groups:
        scene_tags.add("strip")
    if facts_lines and any(d.get("k") == "cam" for d in facts_lines):
        scene_tags.add("strip")
    fps = strip_fps(flags)
    strips = []
    for of, frames in groups.items():
        paths = [os.path.join(sdir, f) for _, f, _ in frames]
        times = [t for _, _, t in frames]
        if any(t is None for t in times):
            times = None
        stags = {"strip"} | set(next((v for k, v in tag_by_stem.items() if of in k or k.endswith(of)), []))
        res = measure_clip.measure(paths, fps=fps, expect_motion=True, times=times, merge_repeats=False)
        sheet_rel = os.path.join("img", safe(name), f"strip-{safe(of)}-sheet.jpg")
        try:
            measure_clip.sheet(paths, res["faults"], os.path.join(img_dir, safe(name), f"strip-{safe(of)}-sheet.jpg"),
                               label=f"{name}: strip after {of} ({len(paths)} frames)")
        except (OSError, ValueError) as e:
            sheet_rel = None
        focus_rel = os.path.join("img", safe(name), f"strip-{safe(of)}-faults.jpg")
        try:
            if not measure_clip.focus_sheet(paths, res["faults"], os.path.join(img_dir, safe(name), f"strip-{safe(of)}-faults.jpg"),
                                            label=f"Film strip after {of}"):
                focus_rel = None
        except (OSError, ValueError):
            focus_rel = None
        verdicts = []
        for r in rules:
            if r.get("clip") and applies(r, stags):
                br = measure_clip.judge(res, r["clip"])
                verdicts.append({"rule": r["id"], "status": "fail" if br else "pass", "breaches": br})
        worst = sorted(res["faults"], key=lambda f: {"jump": 0, "zoom": 1, "stutter": 2, "flicker": 3, "pop": 4,
                                                     "freeze": 5, "blank": 6}.get(f["kind"], 9))[:10]
        for w in worst:
            if 0 <= w["frame"] < len(frames):
                w["file"] = frames[w["frame"]][1]
                if times:
                    w["t"] = times[w["frame"]]
        strips.append({"of": of, "frames": len(paths), "tags": sorted(stags), "summary": res["summary"],
                       "worst": worst, "sheet": rel_img(img_dir, out_dir, sheet_rel) if sheet_rel else None,
                       "focus": rel_img(img_dir, out_dir, focus_rel) if focus_rel else None,
                       "first_file": frames[0][1], "verdicts": verdicts})
    scene["strips"] = strips
    # facts
    fitting = [r for r in rules if applies(r, scene_tags)]
    names = sorted({r["facts"] for r in fitting if r.get("facts")})
    ctx = {"theater": sh.get("theater"), "year": sh.get("year"), "month": sh.get("month")}
    facts = check_facts.run_checks(facts_path if os.path.exists(facts_path) else None, names, ctx) if names else {}
    file_info = facts.pop("_file", None)
    scene["facts"] = facts
    scene["facts_file"] = file_info
    scene["speed"] = speed_summary(steps, logspeed, facts.get("camera-smooth"))
    scene["tags"] = sorted(scene_tags)
    # the machine verdict per rule (clip and facts parts; the picture part is the judges')
    pic_by_name = {p["name"]: p for p in pictures}
    rule_out = []
    for r in fitting:
        parts = []
        if r.get("clip"):
            sv = [(s, v) for s in strips for v in s["verdicts"] if v["rule"] == r["id"]]
            if sv:
                bad = [(s, v) for s, v in sv if v["status"] == "fail"]
                parts.append({"source": "clip", "status": "fail" if bad else "pass",
                              "words": clip_words(bad) if bad else f"{len(sv)} film strip{'s' if len(sv) != 1 else ''} measured smooth.",
                              "evidence": [{"strip": s["of"], "sheet": s["sheet"], "focus": s.get("focus"), "breaches": v["breaches"],
                                            "worst": [w for w in s["worst"] if w["kind"] in {b["measure"] for b in v["breaches"]}][:3]}
                                           for s, v in bad]})
            else:
                parts.append({"source": "clip", "status": "not-measured", "words": "No film strip was saved for this scene."})
        if r.get("facts"):
            fr = facts.get(r["facts"]) or check_facts.result("not-measured", {}, [], "not run")
            ev = []
            for w in fr.get("worst", [])[:3]:
                still = w.get("still")
                # the nearest still stands for the moment only when it was taken within 15 s of it
                near = still and abs(w.get("still_off_s") or 0) <= 15
                p = pic_by_name.get(os.path.basename(str(still))) if near else None
                ev.append(dict(w, thumb=p["thumb"] if p else None, picture=p["file"] if p else None))
            parts.append({"source": "facts", "check": r["facts"], "status": fr["status"], "words": fr["words"],
                          "measured": fr.get("measured"), "evidence": ev})
        has_pic = bool(r.get("picture"))
        statuses = [p["status"] for p in parts]
        status = "fail" if "fail" in statuses else ("pass" if "pass" in statuses else "not-measured")
        if not parts:
            status = "judges"  # picture-only rule: the judges decide
        rule_out.append({"rule": r["id"], "status": status, "parts": parts, "picture_question": has_pic,
                         "pictures": [p["file"] for p in pictures if applies(r, p["tags"])] if has_pic else []})
    scene["rules"] = rule_out
    # the judge list
    judge = []
    for p in pictures:
        rs = [{"id": r["id"], "title": r.get("title"), "question": " ".join(str(r["picture"]).split())}
              for r in rules if r.get("picture") and applies(r, p["tags"])]
        if rs or p["look"]:
            judge.append({"scene": name, "picture": p["file"], "thumb": p["thumb"], "tags": p["tags"],
                          "about": p["about"] or sh.get("about") or "", "theater": sh.get("theater"),
                          "year": sh.get("year"), "rules": rs, "score_look": bool(p["look"])})
    return scene, judge


def clip_words(bad):
    names = {"jump": "jumps", "stutter": "moves unevenly", "zoom": "zooms in one frame", "flicker": "flickers",
             "freeze": "freezes", "pop": "has things popping", "blank": "goes blank"}
    parts = []
    for s, v in bad[:3]:
        what = ", ".join(f"{names.get(b['measure'], b['measure'])} {b['got']} time{'s' if b['got'] != 1 else ''}"
                         + (" a minute" if b["per_minute"] else "") for b in v["breaches"])
        parts.append(f"after '{s['of']}' the picture {what}")
    more = f" (and {len(bad) - 3} more strips)" if len(bad) > 3 else ""
    return "In the film strips, " + "; ".join(parts) + more + "."


def rel_img(img_dir, out_dir, rel):
    """Paths in night-check.json are relative to the folder night-check.json is in."""
    if rel is None:
        return None
    full = os.path.join(img_dir, os.path.relpath(rel, "img"))
    return os.path.relpath(full, out_dir).replace(os.sep, "/")


# ---- the night -------------------------------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="Check one unpacked night folder and write night-check.json.")
    ap.add_argument("night_dir")
    ap.add_argument("--rules", default=os.path.join(HERE, "rules.yaml"))
    ap.add_argument("--shots", default=os.path.join(HERE, "scenes", "shots.yaml"))
    ap.add_argument("--out", help="night-check.json path (default: NIGHT_DIR/../<night>-check/night-check.json)")
    ap.add_argument("--img-dir", help="thumbnails and contact sheets (default: next to --out, in img/)")
    ap.add_argument("--look-scores", action="store_true", help="ask the judges for the Look Bible score of every picture")
    ap.add_argument("--only", help="comma-separated scene names")
    a = ap.parse_args()
    night_dir = os.path.abspath(a.night_dir)
    nj = read_night_json(os.path.join(night_dir, "night.json"))
    night = (nj or {}).get("night") or os.path.basename(night_dir.rstrip("/"))
    out = os.path.abspath(a.out or os.path.join(os.path.dirname(night_dir), f"{night}-check", "night-check.json"))
    out_dir = os.path.dirname(out)
    img_dir = os.path.abspath(a.img_dir or os.path.join(out_dir, "img"))
    os.makedirs(img_dir, exist_ok=True)
    rules, owners = load_rules(a.rules)
    shots = shots_table(read_yaml(a.shots))
    entries = {e.get("scene"): e for e in (nj or {}).get("scenes", []) if isinstance(e, dict)}
    folders = [d for d in sorted(os.listdir(night_dir)) if os.path.isdir(os.path.join(night_dir, d)) and not d.startswith(".")]
    names = list(dict.fromkeys(list(entries) + folders + list(shots)))
    if a.only:
        keep = set(a.only.split(","))
        names = [n for n in names if n in keep]
    attempts = []
    ap_path = os.path.join(night_dir, "attempts.log")
    if os.path.exists(ap_path):
        attempts = open(ap_path, encoding="utf-8", errors="replace").read().splitlines()[-20:]
    stopped = (nj or {}).get("stopped") or ""
    scenes, judge = [], []
    for n in names:
        sc, jl = check_scene(n, night_dir, entries.get(n), shots, rules, img_dir, out_dir, a.look_scores, stopped)
        scenes.append(sc)
        judge.extend(jl)
        print(f"{n}: {sc['ran']['status']}, {len(sc.get('pictures', []))} pictures, {len(sc.get('strips', []))} strips, "
              f"{sum(1 for r in sc.get('rules', []) if r['status'] == 'fail')} machine faults", file=sys.stderr)
    result = {
        "night": night, "build": (nj or {}).get("build"), "checked_at": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
        "night_dir": night_dir, "night_json": nj, "night_ran": nj is not None, "stopped": stopped,
        "done": os.path.exists(os.path.join(night_dir, "DONE")), "attempts_tail": attempts,
        "rules": {r["id"]: {k: r.get(k) for k in ("title", "words", "from", "tags", "owner", "weight", "known")}
                  | {"has": [p for p in ("picture", "clip", "facts") if r.get(p)]} for r in rules},
        "owners": owners, "scenes": scenes, "judge_list": judge,
    }
    os.makedirs(out_dir, exist_ok=True)
    with open(out, "w") as fh:
        json.dump(result, fh, indent=1, default=str)
    print(json.dumps({"out": out, "scenes": len(scenes), "pictures_to_judge": len(judge),
                      "machine_faults": sum(1 for s in scenes for r in s.get("rules", []) if r["status"] == "fail")}))


if __name__ == "__main__":
    main()
