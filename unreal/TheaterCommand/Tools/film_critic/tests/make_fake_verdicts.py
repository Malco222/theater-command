#!/usr/bin/env python3
"""
Fake judges for the end-to-end test: answers every picture in night-check.json's judge list the way a vision judge
would (judge_prompt.md's JSON), with a few deliberate FAILs, and a sceptical second judge (verify_prompt.md's JSON)
that confirms some and refutes one. Also writes a history.json for the night before, so the report shows NEW,
STILL THERE and FIXED.

Usage: make_fake_verdicts.py WORK/night-check.json OUT_DIR   (writes judge.json, verify.json, history.json)
"""
import argparse, json, os

FAILS = {
    # (picture, rule): (box, where, words)
    ("air-raid/air-raid-05-dogfight.png", "marks-every-year"):
        ([60, 31, 81, 50], "the fighter on the right, its wings and side",
         "The fighter on the right carries no marking on its wings or its side."),
    ("air-raid/air-raid-00-flyover-raid.png", "raid-opening-shows-interceptors"):
        ([25, 20, 62, 36], "the two aircraft near the top left",
         "Only two bombers are in the opening; no fighter is coming up to meet them."),
    ("map-fleets/step-001.png", "wake-trails-behind"):
        ([47, 52, 58, 64], "the ship in the middle and its trail",
         "The trail runs off to the left of the ship rather than behind it."),
    ("map-fleets/step-002.png", "wake-trails-behind"):
        ([40, 50, 74, 80], "the two ships in the middle",
         "Both ships' trails point left although one sails right and one sails down."),
}
REFUTE = {("map-fleets/step-001.png", "wake-trails-behind")}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("check")
    ap.add_argument("out")
    a = ap.parse_args()
    check = json.load(open(a.check))
    os.makedirs(a.out, exist_ok=True)
    judges, verifies = [], []
    for item in check["judge_list"]:
        answers, fails = [], []
        for r in item["rules"]:
            key = (item["picture"], r["id"])
            if key in FAILS:
                box, where, words = FAILS[key]
                answers.append({"rule": r["id"], "verdict": "FAIL", "where": where, "box": box, "words": words, "sure": "high"})
                fails.append(r["id"])
            elif r["id"] == "card-words-readable":
                answers.append({"rule": r["id"], "verdict": "CANT_TELL", "where": None, "box": None,
                                "words": "The card's small print is too small in this picture to read either way.", "sure": "medium"})
            else:
                answers.append({"rule": r["id"], "verdict": "PASS", "where": None, "box": None,
                                "words": "What the rule asks for is there and right.", "sure": "medium"})
        look = None
        if item.get("score_look"):
            look = {"colour": 2, "light": 1, "lens": 2, "air": 0, "damage": 1, "scale": 2, "readable": 2, "original": 2,
                    "speed": None, "notes": {"light": "flat noon light, no shadow to show shape", "air": "no haze at all over the raid"}}
        judges.append({"picture": item["picture"], "judge": "first", "answers": answers, "other": None, "look": look})
        if fails:
            checks = []
            for rid in fails:
                if (item["picture"], rid) in REFUTE:
                    checks.append({"rule": rid, "verdict": "REFUTED", "why": "The pale line at the left is the coast's edge, not a wake.",
                                   "box": None, "words": None})
                else:
                    checks.append({"rule": rid, "verdict": "CONFIRMED", "why": "Plainly visible where the box is.",
                                   "box": None, "words": None})
            verifies.append({"picture": item["picture"], "judge": "verify", "checks": checks})
    json.dump(judges, open(os.path.join(a.out, "judge.json"), "w"), indent=1)
    json.dump(verifies, open(os.path.join(a.out, "verify.json"), "w"), indent=1)
    # the night before: marks failed in the raid (still there tonight), the map camera failed (fixed tonight),
    # the wake rule failed on the map (still there), and the sea scene failed (not checked tonight)
    prev = check["night"][:8] + f"{int(check['night'][8:]) - 1:02d}"
    hist = {"nights": {prev: {
        "fail": {"marks-every-year|air-raid": {"title": "Markings in every year", "since": "2026-11-24",
                                               "words": "The Bf 110 carries no markings."},
                 "camera-smooth|map-fleets": {"title": "The camera moves smoothly", "since": prev,
                                              "words": "The map camera jumped 4 times while panning."},
                 "wake-trails-behind|map-fleets": {"title": "Wakes trail behind ships", "since": prev,
                                                   "words": "Every wake points left."},
                 "wake-trails-behind|sea-close": {"title": "Wakes trail behind ships", "since": prev,
                                                  "words": "The destroyer's wake runs ahead of its bow."}},
        "pass": ["look-fits-place|air-raid"], "ran": ["air-raid", "map-fleets", "sea-close"], "build": "4d0c1aa7e"}}}
    json.dump(hist, open(os.path.join(a.out, "history.json"), "w"), indent=1)
    print(json.dumps({"judged": len(judges), "verified": len(verifies), "history_night": prev}))


if __name__ == "__main__":
    main()
