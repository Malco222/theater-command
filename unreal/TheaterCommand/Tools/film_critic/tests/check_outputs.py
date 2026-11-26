#!/usr/bin/env python3
"""Asserts that the end-to-end run on the synthetic night found what was planted in it. Usage: check_outputs.py WORK"""
import json, os, sys

work = sys.argv[1]
check = json.load(open(os.path.join(work, "work", "night-check.json")))
routing = json.load(open(os.path.join(work, "report", "routing.json")))
publish = json.load(open(os.path.join(work, "report", "publish-files.json")))
page = open(os.path.join(work, "report", "index.html"), encoding="utf-8").read()
hist = json.load(open(os.path.join(work, "judges", "history.json")))
sc = {s["name"]: s for s in check["scenes"]}
rule = lambda s, r: next(x for x in sc[s]["rules"] if x["rule"] == r)
fails = []


def expect(cond, what):
    print(("ok    " if cond else "FAIL  ") + what)
    if not cond:
        fails.append(what)


expect(sc["air-raid"]["ran"]["status"] == "ran", "air-raid ran")
expect(sc["land-attack"]["ran"]["status"] == "crashed", "land-attack crashed")
expect(sc["sea-close"]["ran"]["status"] == "did not run", "sea-close did not run")
st = {s["of"]: s for s in sc["air-raid"]["strips"]}
expect(st["00-flyover-raid"]["summary"]["counts"]["jump"] == 1, "strip: the injected jump is found once")
expect(st["00-flyover-raid"]["summary"]["counts"]["flicker"] == 1, "strip: the injected flicker is found once")
expect(st["05-dogfight"]["summary"]["counts"]["jump"] == 0 and st["05-dogfight"]["summary"]["counts"]["stutter"] == 0,
       "strip: the smooth strip has no jump or stutter")
cam = sc["air-raid"]["facts"]["camera-smooth"]["measured"]
expect(cam["jumps"] == 1 and cam["zoom_pops"] == 1 and cam["hitches_over_100ms"] == 1 and cam["stutters"] == 0,
       f"facts: camera jump 1, zoom pop 1, hitch 1, stutter 0 (got {cam['jumps']}, {cam['zoom_pops']}, {cam['hitches_over_100ms']}, {cam['stutters']})")
worst = sc["air-raid"]["facts"]["camera-smooth"]["worst"][0]
expect(abs(worst["t"] - 5.0) < 0.05 and worst.get("still"), "facts: the worst camera moment is at 5.0 s with its nearest still")
expect(sc["air-raid"]["facts"]["opening-shows-both-sides"]["status"] == "fail", "facts: the opening with no defenders fails")
expect(sc["air-raid"]["facts"]["marks-on-every-model"]["status"] == "fail", "facts: the bare model fails")
expect(sc["air-raid"]["facts"]["battle-end-not-abrupt"]["status"] == "fail", "facts: the abrupt end fails")
fl = sc["map-fleets"]["facts"]["fleet-bow-on-course"]["measured"]
expect(fl["fleets_off_course"] == 1 and fl["wake_habit"]["fails"], "facts: one fleet off course and the wakes-one-way habit")
expect(sc["map-fleets"]["facts"]["camera-smooth"]["status"] == "pass", "facts: the smooth map camera passes")
expect(sc["land-attack"]["facts"]["men-gait-variety"]["status"] == "fail", "facts: men running alike fail")
expect(sc["land-attack"]["facts"]["vehicles-no-rocking"]["measured"]["rocking"] == 1, "facts: one rocking tank, the moving one passes")
expect(sc["land-attack"]["facts"]["look-matches-place"]["status"] == "fail", "facts: snow in July fails")
expect(sc["land-attack"]["facts"]["battle-end-not-abrupt"]["status"] == "not-measured", "facts: a missing kind is not-measured")
steps = sc["map-fleets"]["steps"]["counts"]
expect(steps["DID"] == 2 and steps["DEAD"] == 1 and steps["MUTE"] == 1 and steps["SAID WHY"] == 1 and steps["MISSING"] == 1,
       "steps: DID 2, DEAD 1, MUTE 1, SAID WHY 1, MISSING 1")
lc = sc["land-attack"]["log"]["counts"]
expect(lc["error"] == 2 and lc["ensure"] == 1 and lc["crash"] >= 1, "log: errors, the ensure and the crash")
pics = {p["name"]: p for p in sc["land-attack"]["pictures"]}
expect(not pics["land-attack-05-the-tanks.png"]["listed"] and "land-advance" in pics["land-attack-05-the-tanks.png"]["tags"],
       "pictures: an unlisted picture gets its scene kind's tags")
expect(all(os.path.exists(os.path.join(work, "work", p["thumb"])) for s in check["scenes"] for p in s["pictures"]),
       "every still has a thumbnail")
jl = {j["picture"]: j for j in check["judge_list"]}
expect([r["id"] for r in jl["air-raid/air-raid-00-flyover-raid.png"]["rules"]] == ["raid-opening-shows-interceptors"],
       "judge list: the flyover is asked only the opening rule")
# the report
expect("<title>Film Critic Report</title>" in page, "page title")
expect("New problems" in page and "Still there" in page and "Fixed since last night" in page, "page sections")
expect("from your find on 26 Nov" in page, "page: Brian's finds are credited")
expect("sent to the camera thread" in page, "page: routed to a team")
night = hist["nights"]
expect("2026-11-26" in night and "2026-11-25" in night, "history: tonight added beside last night")
expect("camera-smooth|map-fleets" not in night["2026-11-26"]["fail"], "history: the map camera is fixed tonight")
expect(night["2026-11-26"]["fail"]["marks-every-year|air-raid"]["since"] == "2026-11-24", "history: a lasting fault keeps its first night")
expect("wake-trails-behind|sea-close" not in night["2026-11-26"]["fail"], "history: an unchecked scene is not carried as failing")
expect(any(f["rule"] == "camera-smooth" for f in routing.get("camera", {}).get("faults", [])), "routing: the camera faults go to camera")
expect(any(f["rule"] == "wake-trails-behind" and f["box"] for f in routing.get("navy", {}).get("faults", [])),
       "routing: the confirmed wake picture goes to navy with its box")
expect(all(os.path.exists(os.path.join(work, "report", f)) for f in publish["files"]), "every file to publish exists")
expect(any(f.startswith("img/boxed/") for f in publish["files"]), "boxed copies are made")
print(f"\n{len(fails)} failed" if fails else "\nall good")
sys.exit(1 if fails else 0)
