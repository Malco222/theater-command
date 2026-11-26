#!/usr/bin/env python3
"""Film critic: splits the judge list into one batch per scene for the picture judges, gathers every FAIL for the
sceptical judges, and merges all answers into the two lists report.py reads.

  judges.py split NIGHT_CHECK.json JUDGES_DIR    -> JUDGES_DIR/<scene>.batch.json (pictures given as full paths)
  judges.py fails JUDGES_DIR                     -> JUDGES_DIR/<scene>.fails.json for every scene with a FAIL
  judges.py merge JUDGES_DIR                     -> JUDGES_DIR/judge.json and JUDGES_DIR/verify.json

A judge writes JUDGES_DIR/<scene>.judge.json: a JSON list of judge_prompt.md answers (one object per picture).
A sceptic writes JUDGES_DIR/<scene>.verify.json: a JSON list of verify_prompt.md answers.
"""
import glob, json, os, sys


def load_list(path):
    with open(path) as fh:
        d = json.load(fh)
    return d if isinstance(d, list) else [d]


def split(check_path, out):
    d = json.load(open(check_path))
    night_dir = d.get("night_dir") or ""
    os.makedirs(out, exist_ok=True)
    by = {}
    for e in d.get("judge_list", []):
        if not e.get("rules") and not e.get("score_look"):
            continue
        e = dict(e)
        e["picture_path"] = os.path.join(night_dir, e["picture"])
        by.setdefault(e["scene"], []).append(e)
    for scene, items in by.items():
        json.dump(items, open(os.path.join(out, f"{scene}.batch.json"), "w"), indent=1)
    print(json.dumps({s: len(v) for s, v in by.items()}))


def fails(out):
    """Gathers each picture's FAILs with the scene and rule wording the sceptic needs (verify_prompt.md)."""
    n = 0
    for f in sorted(glob.glob(os.path.join(out, "*.judge.json"))):
        scene = os.path.basename(f)[:-len(".judge.json")]
        batch = {}
        bp = os.path.join(out, f"{scene}.batch.json")
        if os.path.exists(bp):
            batch = {e["picture"]: e for e in load_list(bp)}
        items = []
        for ans in load_list(f):
            bad = [a for a in ans.get("answers", []) if a.get("verdict") == "FAIL"]
            if not bad:
                continue
            e = batch.get(ans.get("picture"), {})
            rules = {r.get("id"): r for r in e.get("rules", [])}
            item = {k: e.get(k) for k in ("scene", "about", "theater", "year", "tags", "picture_path")}
            item["picture"] = ans.get("picture")
            item["fails"] = [{"rule": a.get("rule"), "title": rules.get(a.get("rule"), {}).get("title"),
                              "question": rules.get(a.get("rule"), {}).get("question"), "where": a.get("where"),
                              "box": a.get("box"), "words": a.get("words")} for a in bad]
            items.append(item)
        if items:
            json.dump(items, open(os.path.join(out, f"{scene}.fails.json"), "w"), indent=1)
            n += sum(len(i["fails"]) for i in items)
    print(json.dumps({"fails": n}))


def merge(out):
    judge, verify = [], []
    for f in sorted(glob.glob(os.path.join(out, "*.judge.json"))):
        judge += load_list(f)
    for f in sorted(glob.glob(os.path.join(out, "*.verify.json"))):
        verify += load_list(f)
    json.dump(judge, open(os.path.join(out, "judge.json"), "w"), indent=1)
    json.dump(verify, open(os.path.join(out, "verify.json"), "w"), indent=1)
    print(json.dumps({"pictures_judged": len(judge), "verified": len(verify)}))


if __name__ == "__main__":
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    cmd = sys.argv[1]
    if cmd == "split":
        split(sys.argv[2], sys.argv[3])
    elif cmd == "fails":
        fails(sys.argv[2])
    elif cmd == "merge":
        merge(sys.argv[2])
    else:
        print(__doc__)
        sys.exit(2)
