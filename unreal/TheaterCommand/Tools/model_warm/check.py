#!/usr/bin/env python3
"""Checks the model-warm mend (tc.Speed.ModelWarm) from the logs of step-player runs. Reads logs only; no engine.

    check.py LOG[:STEPS.JSONL] LOG[:STEPS.JSONL] ... [--bar 4.0] [--over 250] [--late-ms 5] [--allow-missing] [--no-ref-check]

One or more logs. With two or more, the first is the reference (the run as it was: tc.Speed.ModelWarm=0 or the old build)
and the others are the runs to be judged; with one log there is no reference, section 1 is skipped and that log is judged.
The reference is printed in 2, 3 and 4 as information only (it is expected to break the bar) and is not part of the verdict;
instead a check 5 requires that it does break the bar (some end-month fps_low under --bar), unless --no-ref-check. Where no steps.jsonl is named, it is looked for as <log stem>/steps.jsonl, then steps.jsonl beside the log.
Run the step file month-end-bar-ger-1939.steps.txt (release-1-5/mac-steps) with -TheaterSpeedLog.

Reports, each with PASS or FAIL:
  1. SAME LOOK   every 'Sculpted model K read: P points, T triangles' and 'Sculpted model K hash H' of the reference is in every
                 other log with equal P, T and H (a hash is compared only where both logs have one: logs from before the mend
                 have none). A key missing from a run FAILS unless --allow-missing; keys only a later run has are listed.
  2. LATE READS  every 'Speed: model K was read late' line of --late-ms (5) or more, and whether it fell in a month-end step's
                 window (FAILS if it did). Lines under --late-ms are listed as small and ignored.
  3. LONG FRAMES every 'Speed: a long frame, N ms' over --over (250) not tagged "beside the step player's look" (FAILS if any).
  4. THE BAR     every end-month line of steps.jsonl: fps_low >= --bar (4.0, a longest frame of 0.25 s) (FAILS if any is under).
  5. REFERENCE   the reference itself breaks the bar (so the comparison means something).
A log with no speed lines (run without -TheaterSpeedLog) is said to have none: 2 and 3 then pass on nothing, and say so.
Exit status 0 if every check passed on every log, 1 if any failed, 2 on a usage error.
"""
import json
import os
import re
import sys
from datetime import datetime

STAMP = re.compile(r'^\[(\d{4})\.(\d\d)\.(\d\d)-(\d\d)\.(\d\d)\.(\d\d):(\d+)\]\[\s*(\d+)\]')
READ = re.compile(r'Sculpted model (\S+) read: (-?\d+) points, (-?\d+) triangles')
HASH = re.compile(r'Sculpted model (\S+) hash ([0-9a-fA-F]+)')
LATE = re.compile(r'Speed: model (\S+) was read late, in the frame that asked, (\d+) ms')
ASIDE = re.compile(r'Speed: model (\S+) read aside in (\d+) ms')
LONG = re.compile(r'Speed: a long frame, (\d+) ms')
STEP = re.compile(r'Steps: (\d+) \(line (\d+)\) (\S+)')
BESIDE = "beside the step player's look"


def stamp(line):
    """(seconds, frame) of a log line, or (None, None)."""
    m = STAMP.match(line)
    if not m:
        return None, None
    y, mo, d, h, mi, s, ms, frame = (int(g) for g in m.groups())
    return datetime(y, mo, d, h, mi, s).timestamp() + ms / 1000.0, frame


class Run:
    def __init__(self, log, steps):
        self.log, self.steps_path = log, steps
        self.reads, self.hashes = {}, {}          # key -> (P, T), key -> H
        self.late, self.longs, self.aside = [], [], []   # (when, frame, key or ms, ms)
        self.step_times = []                      # (n, verb, when)
        self.lines = 0
        with open(log, encoding='utf-8-sig', errors='replace') as f:
            for text in f:
                self.lines += 1
                when, frame = stamp(text)
                m = READ.search(text)
                if m:
                    self.reads[m.group(1)] = (int(m.group(2)), int(m.group(3)))
                m = HASH.search(text)
                if m:
                    self.hashes[m.group(1)] = m.group(2).lower().lstrip('0') or '0'
                m = LATE.search(text)
                if m:
                    self.late.append((when, frame, m.group(1), int(m.group(2))))
                m = ASIDE.search(text)
                if m:
                    self.aside.append((when, frame, m.group(1), int(m.group(2))))
                m = LONG.search(text)
                if m:
                    self.longs.append((when, frame, int(m.group(1)), BESIDE in text, text.strip()))
                m = STEP.search(text)
                if m and when is not None:
                    self.step_times.append((int(m.group(1)), m.group(3), when))
        self.steps = []
        if steps and os.path.exists(steps):
            with open(steps, encoding='utf-8-sig', errors='replace') as f:
                for text in f:
                    text = text.strip()
                    if text:
                        try:
                            self.steps.append(json.loads(text))
                        except ValueError:
                            pass

    def window_of(self, when):
        """The step (n, verb) whose window (the one before it logged, up to its own log line) holds this time, or None."""
        before = None
        for n, verb, at in self.step_times:
            if when is not None and (before is None or when > before) and when <= at:
                return n, verb
            before = at
        return None

    def name(self):
        return os.path.basename(self.log)


def find_steps(log):
    stem = os.path.splitext(log)[0]
    for candidate in (os.path.join(stem, 'steps.jsonl'), os.path.join(os.path.dirname(log) or '.', 'steps.jsonl')):
        if os.path.exists(candidate):
            return candidate
    return None


def main(argv):
    bar, over, late_ms, allow_missing, ref_check, paths = 4.0, 250, 5, False, True, []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == '--bar':
            i += 1; bar = float(argv[i])
        elif a == '--over':
            i += 1; over = int(argv[i])
        elif a == '--late-ms':
            i += 1; late_ms = int(argv[i])
        elif a == '--no-ref-check':
            ref_check = False
        elif a == '--allow-missing':
            allow_missing = True
        elif a in ('-h', '--help'):
            print(__doc__); return 0
        else:
            paths.append(a)
        i += 1
    if len(paths) < 1:
        print(__doc__); return 2
    runs = []
    for p in paths:
        log, _, steps = p.partition(':')
        if not os.path.exists(log):
            print('no such log: ' + log); return 2
        runs.append(Run(log, steps or find_steps(log)))
    failed = False

    def verdict(ok):
        nonlocal failed
        if not ok:
            failed = True
        return 'PASS' if ok else 'FAIL'

    ref = runs[0] if len(runs) > 1 else None
    judged = runs[1:] if ref else runs
    print('Runs: ' + ', '.join('%s (%d lines, %d models read, %d hashes, %d steps lines%s)' % (
        r.name(), r.lines, len(r.reads), len(r.hashes), len(r.steps), '' if r.steps_path else ', no steps.jsonl') for r in runs))
    print('Reference: ' + (ref.name() if ref else 'none (one log)'))

    # 1. the same look
    print('\n1. SAME LOOK (points, triangles, hash of every model read, against ' + (ref.name() if ref else 'no reference') + ')')
    if not ref:
        print('  skipped: no reference log')
    for r in (judged if ref else []):
        bad, missing, extra, nohash = [], [], sorted(set(r.reads) - set(ref.reads)), 0
        for key, pt in sorted(ref.reads.items()):
            if key not in r.reads:
                missing.append(key)
                continue
            if r.reads[key] != pt:
                bad.append('%s: %d points, %d triangles here %d, %d' % ((key,) + pt + r.reads[key]))
            if key in ref.hashes and key in r.hashes:
                if ref.hashes[key] != r.hashes[key]:
                    bad.append('%s: hash %s here %s' % (key, ref.hashes[key], r.hashes[key]))
            else:
                nohash += 1
        ok = not bad and (allow_missing or not missing)
        print('  %s vs %s: %s  (%d keys in the reference, %d equal in points and triangles, %d differ, %d missing%s, %d only here; %d compared without a hash)' % (
            ref.name(), r.name(), verdict(ok), len(ref.reads), len(ref.reads) - len(bad) - len(missing), len(bad), len(missing),
            ' (allowed)' if allow_missing and missing else '', len(extra), nohash))
        for line in bad:
            print('    DIFFERS ' + line)
        for key in missing:
            print('    MISSING ' + key)
        for key in extra[:20]:
            print('    only here ' + key)
        if len(extra) > 20:
            print('    ... and %d more only here' % (len(extra) - 20))
    if not any(r.hashes for r in runs):
        print('  (no hash lines in any log: the format before the mend; the points and triangles are what was compared)')

    # 2. late reads
    print('\n2. LATE READS (a model read in the frame that asked, and whether in a month-end step)')
    for r in runs:
        info = r is ref
        in_end = []
        for when, frame, key, ms in r.late:
            if ms < late_ms:
                print('  %s [%s] %s %d ms  small, ignored (under %d ms)' % (r.name(), frame, key, ms, late_ms))
                continue
            win = r.window_of(when)
            endish = win is not None and any(n == win[0] and verb == 'end-month' for n, verb, _ in r.step_times)
            print('  %s [%s] %s %d ms%s' % (r.name(), frame, key, ms, '  IN a month-end window (step %d)' % win[0] if endish else ''))
            if endish:
                in_end.append(key)
        have = bool(r.late or r.aside or r.longs)
        print('  %s: %d late reads, %d in a month-end window, %d read aside: %s%s' % (
            r.name(), len(r.late), len(in_end), len(r.aside), 'reference, information only' if info else verdict(not in_end), '' if have else '  (no speed lines in this log: run with -TheaterSpeedLog; nothing was checked)'))

    # 3. long frames
    print('\n3. LONG FRAMES over %d ms not beside the step player\'s look' % over)
    for r in runs:
        bad = [x for x in r.longs if x[2] > over and not x[3]]
        for when, frame, ms, beside, text in bad:
            win = r.window_of(when)
            print('  %s [%s] %d ms%s' % (r.name(), frame, ms, ' (step %d %s)' % win if win else ''))
        print('  %s: %d long frames logged, %d over %d ms and not beside the look: %s%s' % (
            r.name(), len(r.longs), len(bad), over, 'reference, information only' if r is ref else verdict(not bad), '' if r.longs else '  (none logged: run with -TheaterSpeedLog; nothing was checked)'))

    # 4. the bar
    print('\n4. THE BAR (every end-month step: fps_low >= %.1f, its longest frame at most %.2f s)' % (bar, 1.0 / bar))
    for r in runs:
        ends = [s for s in r.steps if s.get('verb') == 'end-month']
        under = [s for s in ends if s.get('fps_low') is None or s['fps_low'] < bar]
        for s in ends:
            after = s.get('after') or {}
            print('  %s step %3d %-16s fps_low %5.1f  longest %.2f s  %s%s' % (
                r.name(), s.get('n', 0), after.get('month', ''), s.get('fps_low') or 0.0,
                (1.0 / s['fps_low']) if s.get('fps_low') else 0.0, 'ok' if s not in under else 'UNDER',
                '  [expect %s: pass %s]' % (s.get('expect'), s.get('pass')) if str(s.get('expect', '')).startswith('frames-under') else ''))
        lowest = min((s.get('fps_low') or 0.0 for s in ends), default=0.0)
        print('  %s: %d end-month lines, lowest fps_low %.1f, %d under %.1f: %s%s' % (
            r.name(), len(ends), lowest, len(under), bar, 'reference, expected to break the bar' if r is ref else verdict(bool(ends) and not under), '' if ends else '  (no end-month lines: no steps.jsonl)'))

    # 5. the reference breaks the bar
    if ref and ref_check:
        ref_ends = [s for s in ref.steps if s.get('verb') == 'end-month']
        broke = [s for s in ref_ends if s.get('fps_low') is not None and s['fps_low'] < bar]
        print('\n5. REFERENCE %s breaks the bar: %d of %d end-month lines under %.1f: %s' % (
            ref.name(), len(broke), len(ref_ends), bar, verdict(bool(broke))))
    print('\nOVERALL: ' + ('FAIL' if failed else 'PASS'))
    return 1 if failed else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
