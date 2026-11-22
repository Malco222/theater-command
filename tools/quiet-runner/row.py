#!/usr/bin/env python3
"""The quiet test runner's one row: run an engine-free test row of a fetched head alone, N times (3 by default),
and write verdict lines with the median and spread of every number that moved, plus short logs.

It never says CLEAR or FAIL about a head: it reports what each run printed and how it exited; the pre-check decides.

  row.py --head SHA [--branch B] [--bundle FILE] [--patch FILE] --row NAME [--times 3] [--env K=V ...]
         [--cwd unreal/TheaterCommand] [--from THREAD] [--note TEXT] -- COMMAND [ARGS...]

COMMAND runs from --cwd inside a detached worktree of the head (git worktree, read only: nothing is committed or
pushed). Every run gets its own TMPDIR, so the tools' default output folders (${TMPDIR}/land_play and so on) never
collide. One lock serialises every row on this machine, so each timed run is alone.
"""
import argparse, fcntl, hashlib, os, re, shutil, statistics, subprocess, sys, time, datetime

RUNNER = "/home/user/runner"
REPO = "/home/user/theater-command"
SHARED = "/mnt/project-files/production/test-runner-1122/"
LOG_CAP_HEAD, LOG_CAP_TAIL = 80, 220


def sh(cmd, **kw):
    return subprocess.run(cmd, shell=True, check=True, text=True, capture_output=True, **kw).stdout.strip()


def have(sha):
    return subprocess.run(["git", "-C", REPO, "cat-file", "-e", sha + "^{commit}"], capture_output=True).returncode == 0


def prepare(a):
    if a.bundle:
        sh(f"git -C {REPO} bundle verify '{a.bundle}' >/dev/null 2>&1 || true")
        sh(f"git -C {REPO} fetch -q '{a.bundle}' '+refs/*:refs/runner-bundle/{os.path.basename(a.bundle)}/*'")
    if not have(a.head) and a.branch:
        sh(f"git -C {REPO} fetch -q origin '{a.branch}:refs/remotes/origin/{a.branch}'")
    if not have(a.head):
        sys.exit(f"head {a.head} not found (give --branch or --bundle)")
    full = sh(f"git -C {REPO} rev-parse {a.head}^{{commit}}")
    tag = full[:8]
    if a.patch:
        tag += "+" + hashlib.sha1(open(a.patch, "rb").read()).hexdigest()[:6]
    wt = f"{RUNNER}/wt/{tag}"
    if not os.path.isdir(wt):
        sh(f"git -C {REPO} worktree add -q --detach '{wt}' {full}")
        if a.patch:
            sh(f"git -C '{wt}' apply --whitespace=nowarn '{a.patch}'")
    lock = os.path.join(wt, "package-lock.json")
    if os.path.exists(lock) and not os.path.exists(os.path.join(wt, "node_modules")):
        h = hashlib.sha1(open(lock, "rb").read()).hexdigest()[:8]
        nm = f"{RUNNER}/nm/{h}"
        if not os.path.isdir(f"{nm}/node_modules"):
            os.makedirs(nm, exist_ok=True)
            shutil.copy(lock, f"{nm}/package-lock.json")
            shutil.copy(os.path.join(wt, "package.json"), f"{nm}/package.json")
            sh(f"cd '{nm}' && npm ci --no-audit --no-fund --silent")
        os.symlink(f"{nm}/node_modules", os.path.join(wt, "node_modules"))
    return full, tag, wt


def cpu_ticks():
    f = open("/proc/stat").readline().split()[1:]
    v = list(map(int, f))
    return sum(v), v[7] if len(v) > 7 else 0  # total, steal


def others_busy(seconds=1.0):
    """Share of the machine other processes used over a second (0..1), ignoring this runner."""
    me = {os.getpid(), os.getppid()}
    def snap():
        t = {}
        for p in os.listdir("/proc"):
            if not p.isdigit() or int(p) in me:
                continue
            try:
                s = open(f"/proc/{p}/stat").read().rsplit(")", 1)[1].split()
                t[p] = int(s[11]) + int(s[12])
            except Exception:
                pass
        return t
    a = snap(); time.sleep(seconds); b = snap()
    used = sum(b[p] - a.get(p, b[p]) for p in b)
    return used / (os.sysconf("SC_CLK_TCK") * seconds * os.cpu_count())


NUM = re.compile(r"-?\d+(?:\.\d+)?")
OKF = re.compile(r"^(\s*)(ok|FAIL)\b\s*")


def fmt(vals):
    allint = all(float(v).is_integer() and "." not in v for v in vals)
    f = [float(v) for v in vals]
    med = statistics.median(f)
    nd = max((len(v.split(".")[1]) if "." in v else 0) for v in vals)
    p = (lambda x: str(int(round(x)))) if allint else (lambda x: f"{x:.{nd}f}")
    return f"{p(med)} [{p(min(f))}-{p(max(f))}]"


def merge(logs):
    """Lines keyed by their shape (numbers as #) and occurrence; numbers that differ between runs become
    'median [min-max]'. Returns (merged lines, moved lines)."""
    keyed = []
    for log in logs:
        seen, d, order = {}, {}, []
        for line in log:
            shape = NUM.sub("#", OKF.sub(r"\1~ ", line))  # a check line keeps its place whether ok or FAIL
            k = (shape, seen.get(shape, 0)); seen[shape] = k[1] + 1
            d[k] = line; order.append(k)
        keyed.append((d, order))
    merged, moved, done = [], [], set()
    n = len(logs)
    for k in [k for _, o in keyed for k in o]:
        if k in done:
            continue
        done.add(k)
        present = [i for i in range(n) if k in keyed[i][0]]
        lines = [keyed[i][0][k] for i in present]
        nums = [NUM.findall(l) for l in lines]
        parts = NUM.split(lines[0])
        out, moves = parts[0], False
        for j in range(len(nums[0])):
            col = [x[j] for x in nums]
            if len(set(col)) > 1:
                out += fmt(col); moves = True
            else:
                out += col[0]
            out += parts[j + 1]
        marks = [OKF.match(l).group(2) if OKF.match(l) else "" for l in lines]
        if len(set(marks)) > 1:
            out += f"   (FAIL in run {','.join(str(present[i] + 1) for i, m in enumerate(marks) if m == 'FAIL')})"
            moves = True
        if len(present) < n:
            out += f"   (only in run {','.join(str(i + 1) for i in present)})"
            moves = True
        merged.append(out)
        if moves:
            moved.append(out)
    return merged, moved


def cap(lines):
    if len(lines) <= LOG_CAP_HEAD + LOG_CAP_TAIL:
        return lines
    cut = len(lines) - LOG_CAP_HEAD - LOG_CAP_TAIL
    return lines[:LOG_CAP_HEAD] + [f"... {cut} lines cut here (whole log kept in the runner's container) ..."] + lines[-LOG_CAP_TAIL:]


STATUS = re.compile(r"\b(PASS|FAIL\w*|fault\w*|error|errors|warning|over \d+(\.\d+)? ?ms|ASan|AddressSanitizer|UndefinedBehavior|runtime error|Segmentation|Aborted|timed out)\b", re.I)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--head", required=True); ap.add_argument("--branch"); ap.add_argument("--bundle"); ap.add_argument("--patch")
    ap.add_argument("--row", required=True); ap.add_argument("--times", type=int, default=3)
    ap.add_argument("--env", action="append", default=[]); ap.add_argument("--cwd", default="unreal/TheaterCommand")
    ap.add_argument("--from", dest="sender", default=""); ap.add_argument("--note", default="")
    ap.add_argument("--timeout", type=int, default=3600)
    ap.add_argument("cmd", nargs=argparse.REMAINDER)
    a = ap.parse_args()
    cmd = a.cmd[1:] if a.cmd and a.cmd[0] == "--" else a.cmd
    if not cmd:
        sys.exit("no command")
    # A sender's re-point of a queued head ("old new" per line), for rows not yet started.
    rd = f"{RUNNER}/queue/redirect.txt"
    if os.path.exists(rd):
        for line in open(rd):
            p = line.split()
            if len(p) >= 2 and a.head.startswith(p[0]):
                a.note = (a.note + "; " if a.note else "") + f"re-pointed by the sender from {a.head} to {p[1]}"
                a.head = p[1]
    full, tag, wt = prepare(a)
    # land_live's kit.sh and men.sh read what run.sh wrote: point them at this head's run.sh row output.
    if re.search(r"Tools/land_live/(kit|men)\.sh", " ".join(cmd)) and not any(kv.startswith("LAND_LIVE_OUT=") for kv in a.env):
        prev = f"{RUNNER}/out/{tag}-land_live-run/run1/land_live"
        if os.path.isdir(prev):
            a.env.append(f"LAND_LIVE_OUT={prev}")
            a.note = (a.note + "; " if a.note else "") + "reads the land_live run.sh row's output (LAND_LIVE_OUT), as the script expects"
    name = f"{tag}-{a.row}"
    outroot = f"{RUNNER}/out/{name}"
    if os.path.isdir(outroot):
        shutil.rmtree(outroot)
    os.makedirs(outroot)
    env = dict(os.environ)
    for kv in a.env:
        k, v = kv.split("=", 1); env[k] = v
    cxx = {c: (subprocess.run(f"{c} --version 2>/dev/null | head -1", shell=True, capture_output=True, text=True).stdout.strip() or "absent")
           for c in ["g++", "clang++", "clang++-20"]}
    lockf = open(f"{RUNNER}/lock", "w")
    t_wait = time.time()
    fcntl.flock(lockf, fcntl.LOCK_EX)
    waited = time.time() - t_wait
    runs = []
    for i in range(1, a.times + 1):
        # quiet first: wait (up to 10 min) until other processes use under 5% of the machine
        q0 = time.time()
        busy = others_busy()
        while busy > 0.05 and time.time() - q0 < 600:
            time.sleep(5); busy = others_busy()
        d = f"{outroot}/run{i}"; os.makedirs(d)
        e = dict(env); e["TMPDIR"] = d
        load = os.getloadavg()
        tot0, st0 = cpu_ticks(); t0 = time.time()
        try:
            p = subprocess.run(cmd, cwd=os.path.join(wt, a.cwd), env=e, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                               text=True, errors="replace", timeout=a.timeout)
            rc, out = p.returncode, p.stdout
        except subprocess.TimeoutExpired as x:
            rc, out = "timeout", (x.stdout or b"").decode("utf-8", "replace") if isinstance(x.stdout, bytes) else (x.stdout or "")
        wall = time.time() - t0
        tot1, st1 = cpu_ticks()
        steal = 100.0 * (st1 - st0) / max(1, tot1 - tot0)
        out = out.replace(d, "$TMPDIR").replace(wt, "$TREE")
        open(f"{d}.log", "w").write(out)
        runs.append(dict(rc=rc, wall=wall, steal=steal, load=load, busy=busy, lines=out.splitlines()))
    fcntl.flock(lockf, fcntl.LOCK_UN)

    merged, moved = merge([r["lines"] for r in runs])
    status = [l for l in merged if STATUS.search(l)]
    now = datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%d %H:%MZ")
    walls = [f"{r['wall']:.1f}" for r in runs]
    V = []
    V.append(f"TEST RUNNER (cloud) {now}: verdict lines only; the pre-check thread decides CLEAR or FAIL.")
    V.append(f"head {full[:8]}{' + patch ' + os.path.basename(a.patch) if a.patch else ''}{' (' + a.branch + ')' if a.branch else ''}   row {a.row}   asked by {a.sender or '-'}")
    V.append(f"command (from {a.cwd}): {' '.join(a.env + cmd)}")
    V.append(f"machine: cloud x86_64, {os.cpu_count()} vCPU, alone (one row at a time). g++: {cxx['g++']} | clang++: {cxx['clang++']} | clang++-20: {cxx['clang++-20']}")
    V.append(f"runs: {a.times}   exit: {' '.join(str(r['rc']) for r in runs)}   wall s (cloud, includes the script's own compiles): {fmt(walls) if len(set(walls)) > 1 else walls[0]}")
    V.append("per run (cloud): " + "; ".join(f"run {i+1}: exit {r['rc']}, {r['wall']:.1f} s, VM steal {r['steal']:.1f}%, others busy before {100*r['busy']:.0f}%, load {r['load'][0]:.2f}" for i, r in enumerate(runs)))
    def tally(lines):
        ok = sum(1 for l in lines if re.match(r"\s*ok\b", l))
        fail = sum(1 for l in lines if re.search(r"\bFAIL", l))
        pas = sum(1 for l in lines if re.search(r"\bPASS\b", l))
        flt = sum(1 for l in lines if re.search(r"\bfault", l, re.I))
        return f"ok {ok}, PASS {pas}, FAIL {fail}, fault {flt}"
    V.append("line counts per run: " + "; ".join(f"run {i+1}: {tally(r['lines'])}" for i, r in enumerate(runs)))
    if a.note:
        V.append(f"note: {a.note}")
    V.append("")
    V.append("== status lines (PASS / FAIL / fault / error / over N ms), all runs merged; a number that moved is median [min-max] over the runs, cloud ==")
    V += status or ["(none printed)"]
    V.append("")
    V.append("== every line whose numbers moved between runs (timings and the like), median [min-max], cloud ==")
    V += moved or ["(nothing moved: every run printed the same numbers)"]
    V.append("")
    V.append("== last 15 lines, merged ==")
    V += merged[-15:]
    os.makedirs(SHARED + "logs/" + name, exist_ok=True)
    open(f"{SHARED}{name}.txt", "w").write("\n".join(V) + "\n")
    for i, r in enumerate(runs):
        open(f"{SHARED}logs/{name}/run{i+1}.txt", "w").write("\n".join(cap(r["lines"])) + "\n")
    open(f"{SHARED}logs/{name}/merged.txt", "w").write("\n".join(cap(merged)) + "\n")
    open(f"{outroot}/verdict.txt", "w").write("\n".join(V) + "\n")
    print("\n".join(V))
    print(f"\n-> {SHARED}{name}.txt  (logs {SHARED}logs/{name}/)  waited for lock {waited:.0f} s")


if __name__ == "__main__":
    main()
