#!/usr/bin/env python3
"""
Film critic: builds the morning report. One self-contained HTML page (inline CSS, no script, pictures as relative
files in img/ beside it) for the owner, who is not a developer: pictures first, plain words.

Inputs
  night-check.json     from check_night.py
  judge verdicts       the picture judges' answers (judge_prompt.md's JSON), one object per picture, as a JSON list
                       (several --verdicts files are joined)
  verify verdicts      the sceptical judges' answers (verify_prompt.md's JSON); a picture FAIL is shown only when it
                       is CONFIRMED (with --trust-unverified, a FAIL that no second judge saw is shown too)
  history.json         which rule failed in which scene each night (read, then written back with tonight added), so
                       the page says NEW, STILL THERE or FIXED since last night

Outputs (in --out-dir, default the folder of night-check.json)
  index.html           the page (title "Film Critic Report")
  img/                 thumbnails, contact sheets and boxed copies of faulty pictures (the judge's box drawn on it)
  publish-files.json   the page and every file it uses, to publish alongside it
  routing.json         per owner (team), the faults to send them: rule, scene, picture, words

Usage:
  report.py WORK/night-check.json [--verdicts judge.json] [--verify verify.json] [--history history.json]
            [--out-dir WORK] [--night-dir NIGHT_DIR]
"""
import argparse, datetime, html, json, os, re, shutil, sys

from PIL import Image, ImageDraw

BOX_W = 960  # boxed copies are made at most this wide


# ---- reading ---------------------------------------------------------------------------------------------------
def load_json(path, default=None):
    if not path or not os.path.exists(path):
        return default
    with open(path, encoding="utf-8") as fh:
        return json.load(fh)


def load_many(paths, key):
    out = []
    for p in paths or []:
        d = load_json(p, [])
        if isinstance(d, dict):
            d = d.get("results") or d.get(key) or [d]
        out.extend(x for x in d if isinstance(x, dict))
    return out


def norm_verdict(v):
    v = str(v or "").strip().upper().replace("'", "").replace(" ", "_")
    return {"CANT_TELL": "CANT_TELL", "CANNOT_TELL": "CANT_TELL", "PASS": "PASS", "FAIL": "FAIL",
            "CONFIRMED": "CONFIRMED", "REFUTED": "REFUTED"}.get(v, v)


def good_box(b):
    try:
        x0, y0, x1, y1 = [float(v) for v in b]
    except (TypeError, ValueError):
        return None
    x0, x1 = sorted((max(0.0, min(100.0, x0)), max(0.0, min(100.0, x1))))
    y0, y1 = sorted((max(0.0, min(100.0, y0)), max(0.0, min(100.0, y1))))
    if x1 - x0 < 0.5 or y1 - y0 < 0.5:
        return None
    return [x0, y0, x1, y1]


# ---- words -----------------------------------------------------------------------------------------------------
def nice_date(s):
    try:
        d = datetime.date.fromisoformat(str(s)[:10])
        return f"{d.strftime('%a')} {d.day} {d.strftime('%b')}"
    except ValueError:
        return str(s)


def short_date(s):
    try:
        d = datetime.date.fromisoformat(str(s)[:10])
        return f"{d.day} {d.strftime('%b')}"
    except ValueError:
        return str(s)


def plural(n, one, many=None):
    return f"{n} {one if n == 1 else (many or one + 's')}"


def scene_label(sc):
    a = (sc.get("about") or "").strip()
    return a if a else sc["name"].replace("-", " ")


RAN_WORDS = {"ran": "ran", "crashed": "crashed", "timed out": "ran out of time", "stopped": "stopped part way",
             "did not run": "did not run", "ended with an error": "ended with an error"}


def from_brian(rule):
    f = rule.get("from") or {}
    who = str(f.get("who") or "")
    if "brian" in who.lower():
        return short_date(f.get("when")) if f.get("when") else "an earlier day"
    return None


# ---- pictures --------------------------------------------------------------------------------------------------
class Files:
    """Every file the page uses, copied under out_dir when it lives elsewhere."""

    def __init__(self, check_dir, out_dir):
        self.check_dir, self.out_dir, self.used = check_dir, out_dir, []

    def use(self, rel):
        if not rel:
            return None
        src = os.path.join(self.check_dir, rel)
        dst = os.path.join(self.out_dir, rel)
        if not os.path.exists(dst):
            if not os.path.exists(src):
                return None
            os.makedirs(os.path.dirname(dst), exist_ok=True)
            shutil.copyfile(src, dst)
        rel = rel.replace(os.sep, "/")
        if rel not in self.used:
            self.used.append(rel)
        return rel

    def boxed(self, night_dir, picture, thumb, box, tag):
        """A copy of the picture with the judge's box drawn on it. Returns its path relative to out_dir."""
        src = os.path.join(night_dir, picture) if night_dir else None
        if not src or not os.path.exists(src):
            src = os.path.join(self.check_dir, thumb) if thumb else None
        if not src or not os.path.exists(src):
            return None
        rel = f"img/boxed/{safe(picture.replace('/', '-'))}-{safe(tag)}.jpg"
        dst = os.path.join(self.out_dir, rel)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        with Image.open(src) as im:
            im = im.convert("RGB")
            if im.width > BOX_W:
                im = im.resize((BOX_W, round(im.height * BOX_W / im.width)), Image.LANCZOS)
            if box:
                w, h = im.size
                x0, y0, x1, y1 = box[0] * w / 100, box[1] * h / 100, box[2] * w / 100, box[3] * h / 100
                pad = max(4, round(w / 160))
                x0, y0, x1, y1 = max(0, x0 - pad), max(0, y0 - pad), min(w - 1, x1 + pad), min(h - 1, y1 + pad)
                d = ImageDraw.Draw(im)
                lw = max(3, round(w / 240))
                d.rectangle([x0 - lw, y0 - lw, x1 + lw, y1 + lw], outline=(255, 255, 255), width=max(1, lw // 2))
                d.rectangle([x0, y0, x1, y1], outline=(235, 30, 30), width=lw)
            im.save(dst, "JPEG", quality=85, optimize=True)
        if rel not in self.used:
            self.used.append(rel)
        return rel


def safe(s):
    return re.sub(r"[^A-Za-z0-9._-]+", "-", str(s)).strip("-")[:120] or "x"


# ---- merging tonight's verdicts --------------------------------------------------------------------------------
def merge(check, judges, verifies, trust_unverified):
    """Per (rule, scene): status fail / pass / not-measured / not-judged, words, and the evidence."""
    by_pic = {}
    for j in judges:
        by_pic.setdefault(str(j.get("picture")), []).append(j)
    ver = {}
    for v in verifies:
        for c in v.get("checks") or []:
            ver[(str(v.get("picture")), str(c.get("rule")))] = c
    judged_pics = {jl["picture"]: jl for jl in check.get("judge_list", [])}
    results, unconfirmed, looks = {}, [], []
    for sc in check["scenes"]:
        pics = {p["file"]: p for p in sc.get("pictures", [])}
        for r in sc.get("rules", []):
            key = f"{r['rule']}|{sc['name']}"
            entry = {"rule": r["rule"], "scene": sc["name"], "machine": r["status"], "parts": r["parts"],
                     "picture_fails": [], "picture_pass": 0, "picture_cant": 0, "picture_asked": len(r.get("pictures", []))}
            for pf in r.get("pictures", []):
                for j in by_pic.get(pf, []):
                    for a in j.get("answers") or []:
                        if str(a.get("rule")) != r["rule"]:
                            continue
                        v = norm_verdict(a.get("verdict"))
                        if v == "PASS":
                            entry["picture_pass"] += 1
                        elif v == "CANT_TELL":
                            entry["picture_cant"] += 1
                        elif v == "FAIL":
                            c = ver.get((pf, r["rule"]))
                            cv = norm_verdict(c.get("verdict")) if c else None
                            if cv == "CONFIRMED" or (c is None and trust_unverified):
                                entry["picture_fails"].append({
                                    "picture": pf, "thumb": pics.get(pf, {}).get("thumb"),
                                    "box": good_box((c or {}).get("box")) or good_box(a.get("box")),
                                    "where": a.get("where"),
                                    "words": (c or {}).get("words") or a.get("words") or "",
                                    "why": (c or {}).get("why"), "verified": cv == "CONFIRMED"})
                            else:
                                unconfirmed.append({"rule": r["rule"], "scene": sc["name"], "picture": pf,
                                                    "words": a.get("words"), "why": (c or {}).get("why"),
                                                    "second": cv or "not checked"})
            if entry["picture_fails"] or r["status"] == "fail":
                st = "fail"
            elif r["status"] == "pass" or entry["picture_pass"]:
                st = "pass"
            elif r["status"] == "judges" and not (entry["picture_pass"] or entry["picture_cant"]):
                st = "not-judged" if r.get("pictures") else "not-measured"
            else:
                st = "not-measured"
            entry["status"] = st
            results[key] = entry
    for j in judges:
        if j.get("look"):
            looks.append({"picture": j.get("picture"), "look": j["look"], "thumb": judged_pics.get(j.get("picture"), {}).get("thumb")})
    return results, unconfirmed, looks


# ---- history ---------------------------------------------------------------------------------------------------
def compare(history, night, results):
    nights = (history or {}).get("nights", {})
    before = sorted(n for n in nights if n < night)
    prev_name = before[-1] if before else None
    prev = nights.get(prev_name, {}) if prev_name else {}
    prev_fail = prev.get("fail", {})
    for key, e in results.items():
        if e["status"] == "fail":
            if key in prev_fail:
                e["change"] = "still"
                e["since"] = prev_fail[key].get("since") or prev_name
            else:
                e["change"] = "new"
                e["since"] = night
    fixed, unchecked = [], []
    for key, p in prev_fail.items():
        e = results.get(key)
        if e and e["status"] == "pass":
            fixed.append(dict(p, key=key, rule=key.split("|")[0], scene=key.split("|")[1]))
        elif not e or e["status"] in ("not-measured", "not-judged"):
            unchecked.append(dict(p, key=key, rule=key.split("|")[0], scene=key.split("|")[1]))
    return prev_name, fixed, unchecked


def write_history(path, history, night, results, rules, check):
    history = history or {"nights": {}}
    history.setdefault("nights", {})
    history["nights"][night] = {
        "fail": {k: {"title": (rules.get(e["rule"]) or {}).get("title"), "since": e.get("since", night),
                     "words": first_words(e)} for k, e in results.items() if e["status"] == "fail"},
        "pass": sorted(k for k, e in results.items() if e["status"] == "pass"),
        "ran": [s["name"] for s in check["scenes"] if s["ran"]["status"] != "did not run"],
        "build": check.get("build"),
    }
    for old in sorted(history["nights"])[:-60]:
        del history["nights"][old]
    with open(path, "w") as fh:
        json.dump(history, fh, indent=1)


def first_words(e):
    if e["picture_fails"]:
        return e["picture_fails"][0]["words"]
    for p in e["parts"]:
        if p["status"] == "fail":
            return p["words"]
    return ""


# ---- the page --------------------------------------------------------------------------------------------------
CSS = """
:root{--bg:#f6f4ef;--card:#ffffff;--ink:#1d1c1a;--soft:#5d5a53;--line:#e2ded4;--new:#b3261e;--newbg:#fbe9e7;
--still:#8a5a00;--stillbg:#fdf1d8;--fixed:#1e6b3a;--fixedbg:#e3f3e8;--chip:#efece4;--link:#1f5fa8;--shadow:0 1px 2px rgba(0,0,0,.06)}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#16171a;--card:#202226;--ink:#ecebe7;--soft:#a7a49c;
--line:#33363c;--new:#ff8a80;--newbg:#3a1f1d;--still:#f2c46b;--stillbg:#352a14;--fixed:#7dd69a;--fixedbg:#16301f;--chip:#2a2d33;
--link:#8ab8ff;--shadow:none}}
:root[data-theme="dark"]{--bg:#16171a;--card:#202226;--ink:#ecebe7;--soft:#a7a49c;--line:#33363c;--new:#ff8a80;--newbg:#3a1f1d;
--still:#f2c46b;--stillbg:#352a14;--fixed:#7dd69a;--fixedbg:#16301f;--chip:#2a2d33;--link:#8ab8ff;--shadow:none}
*{box-sizing:border-box}
html{-webkit-text-size-adjust:100%}
body{margin:0;background:var(--bg);color:var(--ink);font:16px/1.5 -apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Helvetica,Arial,sans-serif}
main{max-width:920px;margin:0 auto;padding:20px 16px 48px}
a{color:var(--link)}
h1{font-size:15px;font-weight:600;letter-spacing:.04em;text-transform:uppercase;color:var(--soft);margin:0 0 6px}
.lead{font-size:24px;line-height:1.3;font-weight:650;margin:0 0 6px}
.sub{color:var(--soft);font-size:14px;margin:0 0 24px}
h2{font-size:20px;margin:36px 0 12px;padding-top:12px;border-top:1px solid var(--line)}
h2 .n{color:var(--soft);font-weight:500}
.card{background:var(--card);border:1px solid var(--line);border-radius:12px;overflow:hidden;margin:0 0 16px;box-shadow:var(--shadow)}
.card .pic{display:block;background:#111}
.card .pic img{display:block;width:100%;height:auto;max-height:480px;object-fit:contain}
.card .body{padding:12px 16px 14px}
.card h3{font-size:18px;margin:6px 0 4px;line-height:1.3}
.card p{margin:4px 0}
.meta{color:var(--soft);font-size:14px}
.badge{display:inline-block;font-size:12px;font-weight:700;letter-spacing:.05em;text-transform:uppercase;padding:2px 8px;border-radius:999px}
.b-new{color:var(--new);background:var(--newbg)}.b-still{color:var(--still);background:var(--stillbg)}.b-fixed{color:var(--fixed);background:var(--fixedbg)}
.more{margin-top:6px;font-size:14px;color:var(--soft)}
.more summary{cursor:pointer}
.compact .card{display:grid;grid-template-columns:200px 1fr}
.compact .card .pic{height:100%}
.compact .card .pic img{height:100%;object-fit:cover}
@media (max-width:600px){.compact .card{grid-template-columns:1fr}.lead{font-size:21px}}
ul.plain{list-style:none;padding:0;margin:0}
ul.plain li{background:var(--card);border:1px solid var(--line);border-radius:10px;padding:10px 14px;margin:0 0 8px}
.chip{display:inline-block;background:var(--chip);border-radius:6px;padding:0 6px;font-size:13px;margin-right:4px}
table{width:100%;border-collapse:collapse;font-size:14px;background:var(--card);border:1px solid var(--line);border-radius:10px;overflow:hidden}
th,td{text-align:left;padding:8px 10px;border-bottom:1px solid var(--line);vertical-align:top}
th{color:var(--soft);font-weight:600;font-size:13px}
td.num,th.num{text-align:right}
@media (max-width:600px){table{font-size:13px}th,td{padding:7px 6px}}
.bad{color:var(--new);font-weight:700}.ok{color:var(--fixed)}
footer{margin-top:36px;color:var(--soft);font-size:13px}
.empty{color:var(--soft);font-style:italic}
"""


def esc(s):
    return html.escape(str(s if s is not None else ""), quote=True)


def fault_card(e, rule, owners, files, night_dir, compact=False):
    title = rule.get("title") or e["rule"].replace("-", " ").capitalize()
    owner = rule.get("owner")
    team = (owners.get(owner) or {}).get("who") or owner or "no team yet"
    img, caption, words = None, "", []
    if e["picture_fails"]:
        pf = e["picture_fails"][0]
        img = files.boxed(night_dir, pf["picture"], pf["thumb"], pf["box"], e["rule"])
        caption = pf.get("where") or ""
        words.append(pf["words"])
    for p in e["parts"]:
        if p["status"] != "fail":
            continue
        words.append(p["words"])
        if img is None:
            for ev in p.get("evidence") or []:
                if ev.get("focus") or ev.get("sheet"):
                    img = files.use(ev.get("focus") or ev["sheet"])
                    caption = (f"film strip after '{ev.get('strip')}': each row is the frame before, the faulty frame "
                               "(red) and the frame after" if ev.get("focus") else
                               f"film strip after '{ev.get('strip')}': the red frames are where it goes wrong")
                    if ev.get("focus") and ev.get("sheet") and files.use(ev["sheet"]):
                        caption += f' (<a href="{esc(ev["sheet"])}">the whole strip</a>)'
                    break
                if ev.get("thumb"):
                    img = files.use(ev["thumb"])
                    caption = f"the nearest picture to the moment ({ev.get('t', 0):.0f} s in)"
                    break
    words = [w for i, w in enumerate(words) if w and w not in words[:i]]
    badge = {"new": '<span class="badge b-new">New</span>', "still": f'<span class="badge b-still">Still there since {esc(short_date(e.get("since")))}</span>'}.get(e.get("change"), "")
    brian = from_brian(rule)
    meta = [f"in {esc(e['scene_label'])}"]
    if brian:
        meta.append(f"from your find on {esc(brian)}")
    meta.append(f"sent to {esc(team)}")
    pic = (f'<a class="pic" href="{esc(img)}"><img src="{esc(img)}" alt="{esc(caption or title)}" loading="lazy"></a>'
           if img else "")
    extra = ""
    detail = []
    if len(e["picture_fails"]) > 1:
        detail.append(f"Seen in {plural(len(e['picture_fails']), 'picture')} in this scene.")
    if caption:
        detail.append(f"Picture: {caption if '<a ' in caption else esc(caption)}.")
    if detail:
        extra = f'<p class="more">{" ".join(detail)}</p>'
    body = (f'<div class="body">{badge}<h3>{esc(title)}</h3>'
            + "".join(f"<p>{esc(w)}</p>" for w in words[:2])
            + f'<p class="meta">{" · ".join(meta)}</p>{extra}</div>')
    return f'<div class="card">{pic}{body}</div>'


def build_page(check, results, rules, owners, prev_night, fixed, unchecked, unconfirmed, looks, files, night_dir):
    night = check["night"]
    scenes = check["scenes"]
    labels = {s["name"]: scene_label(s) for s in scenes}
    for e in results.values():
        e["scene_label"] = labels.get(e["scene"], e["scene"])
    fails = [e for e in results.values() if e["status"] == "fail"]
    weight = lambda e: (0 if (rules.get(e["rule"]) or {}).get("weight") == "high" else 1, e["scene"], e["rule"])
    new = sorted([e for e in fails if e.get("change") == "new"], key=weight)
    still = sorted([e for e in fails if e.get("change") == "still"], key=weight)
    ran = [s for s in scenes if s["ran"]["status"] != "did not run"]
    trouble = [s for s in scenes if s["ran"]["status"] != "ran"]
    # the top line
    if not check.get("night_ran"):
        lead = f"Nothing ran on the night of {nice_date(night)}."
    else:
        bits = f"{len(ran)} of {plural(len(scenes), 'scene')} ran"
        crashed = [s for s in scenes if s["ran"]["status"] == "crashed"]
        if crashed:
            bits += f" ({len(crashed)} crashed)"
        lead = (f"Last night ({nice_date(night)}) {bits}. I found {plural(len(fails), 'problem')}"
                + (f", {len(new)} of them new" if prev_night else "")
                + (f", and {plural(len(fixed), 'problem')} {'was' if len(fixed) == 1 else 'were'} fixed" if fixed else "")
                + ".")
    out = ["<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">",
           "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">",
           "<title>Film Critic Report</title>",
           f"<style>{CSS}</style></head><body><main>",
           f"<h1>Film critic · {esc(nice_date(night))}</h1>",
           f"<p class=\"lead\">{esc(lead)}</p>",
           f"<p class=\"sub\">Build {esc(check.get('build') or 'unknown')}"
           + (f" · compared with {esc(nice_date(prev_night))}" if prev_night else " · the first night on record")
           + "</p>"]
    # new problems
    out.append(f'<h2>New problems <span class="n">({len(new)})</span></h2>')
    if new:
        out.extend(fault_card(e, rules.get(e["rule"]) or {}, owners, files, night_dir) for e in new)
    else:
        out.append('<p class="empty">Nothing new went wrong.</p>')
    # still there
    out.append(f'<h2>Still there <span class="n">({len(still)})</span></h2>')
    if still:
        out.append('<div class="compact">')
        out.extend(fault_card(e, rules.get(e["rule"]) or {}, owners, files, night_dir, compact=True) for e in still)
        out.append("</div>")
    else:
        out.append('<p class="empty">No problem from last night is still there.</p>' if prev_night else
                   '<p class="empty">This is the first night on record, so every problem above is new.</p>')
    # fixed
    out.append(f'<h2>Fixed since last night <span class="n">({len(fixed)})</span></h2>')
    if fixed:
        out.append('<ul class="plain">')
        for f in fixed:
            r = rules.get(f["rule"]) or {}
            out.append(f'<li><span class="badge b-fixed">Fixed</span> <strong>{esc(r.get("title") or f.get("title") or f["rule"])}</strong>'
                       f' in {esc(labels.get(f["scene"], f["scene"]))}<br><span class="meta">Last night: {esc(f.get("words") or "")}</span></li>')
        out.append("</ul>")
    else:
        out.append('<p class="empty">Nothing that failed last night passes now.</p>' if prev_night else
                   '<p class="empty">No earlier night to compare with yet.</p>')
    # what didn't run and why
    out.append("<h2>What didn't run and why</h2>")
    items = []
    if not check.get("night_ran"):
        tail = [l for l in check.get("attempts_tail", []) if "not run" in l]
        items.append(f"<li>The night run did not start. {esc(tail[-1].split('not run:')[-1].strip().capitalize() if tail else 'No reason was written.')}</li>")
    for s in trouble:
        why = s["ran"].get("why") or ""
        extra = ""
        if s["ran"]["status"] == "crashed" and s.get("log"):
            cr = (s["log"].get("crash") or []) + (s["log"].get("ensure") or [])
            # the most telling line: a fatal error or failed assertion, then any crash line, then a failed check
            cr.sort(key=lambda c: (0 if re.search(r"Fatal error|Assertion failed|Unhandled Exception", c["line"]) else
                                   1 if c in (s["log"].get("crash") or []) and "Critical error" not in c["line"] else 2))
            if cr:
                said = re.sub(r"^Log\w+: (Error|Warning): ", "", cr[0]["line"])
                extra = f'<br><span class="meta">The game said: {esc(said[:220])}</span>'
        items.append(f"<li><strong>{esc(labels[s['name']])}</strong> {esc(RAN_WORDS.get(s['ran']['status'], s['ran']['status']))}"
                     + (f": {esc(why)}" if why else "") + f".{extra}</li>")
    for s in scenes:
        st = s.get("steps") or {}
        nd = st.get("not_did") or []
        if nd:
            lines = []
            for ln in nd[:8]:
                v = ln.get("verdict")
                said = {"DEAD": "nothing happened", "MUTE": "the button was dim and said nothing",
                        "SAID WHY": "the game said why not", "MISSING": "there was no such thing",
                        "ERROR": "something was in the way"}.get(v, (v or "").lower())
                w = f' ("{esc(ln["words"][:120])}")' if ln.get("words") and v == "SAID WHY" else ""
                susp = " (may follow from an earlier step)" if ln.get("suspect") else ""
                lines.append(f"step {esc(ln.get('n'))}, {esc(ln.get('verb'))} {esc(ln.get('target') or '')}: {esc(said)}{w}{susp}")
            more = f"; and {len(nd) - 8} more" if len(nd) > 8 else ""
            items.append(f"<li><strong>{esc(labels[s['name']])}</strong>: {plural(len(nd), 'step')} of {st.get('steps')} did not do what was asked: "
                         + "; ".join(lines) + more + ".</li>")
        lg = s.get("log") or {}
        c = lg.get("counts") or {}
        if (c.get("error") or c.get("ensure")) and s["ran"]["status"] != "crashed":
            first = ((lg.get("ensure") or []) + (lg.get("error") or []))[:1]
            items.append(f"<li><strong>{esc(labels[s['name']])}</strong>: the game logged {plural(c.get('error', 0), 'error')}"
                         + (f" and {plural(c.get('ensure'), 'failed check')}" if c.get("ensure") else "")
                         + (f', first: {esc(first[0]["line"][:200])}' if first else "") + ".</li>")
    nm = {}
    for e in results.values():
        if e["status"] in ("not-measured", "not-judged"):
            nm.setdefault(e["scene"], []).append(e)
    for sc, es in nm.items():
        names = ", ".join("\u201c" + esc((rules.get(e["rule"]) or {}).get("title") or e["rule"]) + "\u201d" for e in es[:6])
        items.append(f"<li><strong>{esc(labels.get(sc, sc))}</strong>: could not check {names}"
                     + (f" and {len(es) - 6} more" if len(es) > 6 else "") + " (the game gave nothing to measure, or no picture showed it).</li>")
    for u in unchecked:
        items.append(f"<li>Last night's problem <strong>{esc((rules.get(u['rule']) or {}).get('title') or u['rule'])}</strong> in "
                     f"{esc(labels.get(u['scene'], u['scene']))} could not be checked tonight, so it is not known to be fixed.</li>")
    if unconfirmed:
        items.append(f"<li>{plural(len(unconfirmed), 'possible problem')} seen by the first picture judge "
                     f"{'was' if len(unconfirmed) == 1 else 'were'} left out because a second judge could not confirm {'it' if len(unconfirmed) == 1 else 'them'}.</li>")
    out.append('<ul class="plain">' + ("".join(items) if items else '<li class="empty">Everything ran and every check could be made.</li>') + "</ul>")
    # every scene
    out.append("<h2>Every scene</h2>")
    out.append('<table><thead><tr><th>Scene</th><th>How it went</th><th class="num">Problems</th><th class="num">Fine</th><th class="num">Not checked</th></tr></thead><tbody>')
    for s in scenes:
        es = [e for e in results.values() if e["scene"] == s["name"]]
        nf = sum(1 for e in es if e["status"] == "fail")
        npass = sum(1 for e in es if e["status"] == "pass")
        nn = sum(1 for e in es if e["status"] in ("not-measured", "not-judged"))
        how = RAN_WORDS.get(s["ran"]["status"], s["ran"]["status"])
        fps = (s.get("speed") or {}).get("fps")
        if fps:
            how += f", {fps:.0f} fps"
        if s["ran"]["status"] == "did not run":
            out.append(f"<tr><td>{esc(labels[s['name']])}</td><td>{esc(how)}</td><td class=\"num\">–</td>"
                       "<td class=\"num\">–</td><td class=\"num\">–</td></tr>")
            continue
        out.append(f"<tr><td>{esc(labels[s['name']])}</td><td>{esc(how)}</td>"
                   f"<td class=\"num {'bad' if nf else ''}\">{nf}</td><td class=\"num ok\">{npass}</td><td class=\"num\">{nn}</td></tr>")
    out.append("</tbody></table>")
    if looks:
        out.append("<h2>Look scores</h2><ul class=\"plain\">")
        for l in looks:
            sc = l["look"]
            lines = ["colour", "light", "lens", "air", "damage", "scale", "readable", "original", "speed"]
            cells = " ".join(f'<span class="chip">{k} {esc(sc.get(k) if sc.get(k) is not None else "–")}</span>' for k in lines)
            notes = sc.get("notes") or {}
            note = "; ".join(f"{esc(k)}: {esc(v)}" for k, v in notes.items())
            out.append(f"<li><strong>{esc(l['picture'])}</strong><br>{cells}" + (f'<br><span class="meta">{note}</span>' if note else "") + "</li>")
        out.append("</ul>")
    out.append(f"<footer>Checked {esc(check.get('checked_at', '')[:16].replace('T', ' '))} UTC from the night's pictures, film strips and "
               "the game's own notes. A problem seen in a picture is shown only when a second judge confirmed it; the others come "
               "from measurements (fps = frames a second).</footer>")
    out.append("</main></body></html>")
    return "\n".join(out), new, still


def routing(results, rules, owners, night):
    out = {}
    for e in sorted(results.values(), key=lambda e: (e["rule"], e["scene"])):
        if e["status"] != "fail":
            continue
        r = rules.get(e["rule"]) or {}
        o = r.get("owner") or "unowned"
        box = out.setdefault(o, {"who": (owners.get(o) or {}).get("who"), "covers": (owners.get(o) or {}).get("covers"),
                                 "night": night, "faults": []})
        pf = e["picture_fails"][0] if e["picture_fails"] else None
        ev = []
        for p in e["parts"]:
            if p["status"] == "fail":
                ev.append({"source": p["source"], "words": p["words"], "measured": p.get("measured"),
                           "evidence": p.get("evidence")})
        box["faults"].append({
            "rule": e["rule"], "title": r.get("title"), "scene": e["scene"], "status": e.get("change", "new"),
            "since": e.get("since"), "weight": r.get("weight"), "from": r.get("from"),
            "picture": pf["picture"] if pf else next((x.get("picture") for p in e["parts"] for x in (p.get("evidence") or []) if x.get("picture")), None),
            "box": pf["box"] if pf else None, "words": first_words(e), "machine": ev,
        })
    return out


def main():
    ap = argparse.ArgumentParser(description="Build the morning report page from night-check.json and the judges' verdicts.")
    ap.add_argument("check")
    ap.add_argument("--verdicts", action="append", default=[], help="picture judges' JSON (list); may repeat")
    ap.add_argument("--verify", action="append", default=[], help="sceptical judges' JSON (list); may repeat")
    ap.add_argument("--history", help="history.json: read, then written back with tonight added")
    ap.add_argument("--no-history-write", action="store_true")
    ap.add_argument("--out-dir")
    ap.add_argument("--night-dir", help="the unpacked night (for full-size pictures to draw boxes on)")
    ap.add_argument("--trust-unverified", action="store_true", help="show a picture FAIL no second judge looked at")
    a = ap.parse_args()
    check = load_json(a.check)
    check_dir = os.path.dirname(os.path.abspath(a.check))
    out_dir = os.path.abspath(a.out_dir or check_dir)
    os.makedirs(out_dir, exist_ok=True)
    night_dir = a.night_dir or check.get("night_dir")
    rules = check.get("rules") or {}
    owners = check.get("owners") or {}
    judges = load_many(a.verdicts, "answers")
    verifies = load_many(a.verify, "checks")
    results, unconfirmed, looks = merge(check, judges, verifies, a.trust_unverified)
    history = load_json(a.history, {"nights": {}}) if a.history else {"nights": {}}
    prev_night, fixed, unchecked = compare(history, check["night"], results)
    files = Files(check_dir, out_dir)
    page, new, still = build_page(check, results, rules, owners, prev_night, fixed, unchecked, unconfirmed, looks, files, night_dir)
    with open(os.path.join(out_dir, "index.html"), "w", encoding="utf-8") as fh:
        fh.write(page)
    with open(os.path.join(out_dir, "routing.json"), "w") as fh:
        json.dump(routing(results, rules, owners, check["night"]), fh, indent=1)
    with open(os.path.join(out_dir, "publish-files.json"), "w") as fh:
        json.dump({"page": "index.html", "files": files.used}, fh, indent=1)
    if a.history and not a.no_history_write:
        write_history(a.history, history, check["night"], results, rules, check)
    print(json.dumps({"page": os.path.join(out_dir, "index.html"), "files": len(files.used), "new": len(new),
                      "still": len(still), "fixed": len(fixed), "unconfirmed": len(unconfirmed),
                      "problems": sum(1 for e in results.values() if e["status"] == "fail")}))


if __name__ == "__main__":
    main()
