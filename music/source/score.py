"""Writing music down: bars, lines of notes, chords, the harmony each bar stands on, and a check that the
notes written agree with that harmony (the composer cannot hear, so the paper is checked instead)."""
import re, os, json, sys
import numpy as np
import soundfile as sf
from engine import (Clock, render_track, mix, make_hall, master, fold_loop, note_number, SR)
from orchestra import build

DYN = {'ppp': 22, 'pp': 34, 'p': 48, 'mp': 62, 'mf': 78, 'f': 96, 'ff': 112, 'fff': 124}
PC = {'C': 0, 'D': 2, 'E': 4, 'F': 5, 'G': 7, 'A': 9, 'B': 11}


def chord_pcs(sym):
    """'Cm7' -> pitch classes {0, 3, 7, 10}. Bass after a slash is added."""
    m = re.match(r'^([A-G])(b|#)?(.*?)(?:/([A-G])(b|#)?)?$', sym)
    root = (PC[m.group(1)] + {'b': -1, '#': 1, None: 0}[m.group(2)]) % 12
    q = m.group(3)
    table = {
        '': [0, 4, 7], 'm': [0, 3, 7], '7': [0, 4, 7, 10], 'm7': [0, 3, 7, 10], 'maj7': [0, 4, 7, 11],
        'sus4': [0, 5, 7], 'sus2': [0, 2, 7], '7sus4': [0, 5, 7, 10], 'dim': [0, 3, 6], 'dim7': [0, 3, 6, 9],
        'm7b5': [0, 3, 6, 10], '6': [0, 4, 7, 9], 'm6': [0, 3, 7, 9], 'add9': [0, 2, 4, 7], 'madd9': [0, 2, 3, 7],
        '5': [0, 7], 'm9': [0, 3, 7, 10, 2], 'maj9': [0, 4, 7, 11, 2], '9': [0, 4, 7, 10, 2], 'aug': [0, 4, 8],
        'm(maj7)': [0, 3, 7, 11], '7b9': [0, 4, 7, 10, 1], 'msus4': [0, 5, 7],
    }
    if q not in table:
        raise ValueError(f'chord quality {q!r} in {sym}')
    pcs = {(root + i) % 12 for i in table[q]}
    if m.group(4):
        pcs.add((PC[m.group(4)] + {'b': -1, '#': 1, None: 0}[m.group(5)]) % 12)
    return pcs


def chord_tones(sym, low=36, count=5):
    """The notes of a chord laid out from its bass upward: the bass first, then the chord's notes above it."""
    m = re.match(r'^([A-G])(b|#)?(.*?)(?:/([A-G])(b|#)?)?$', sym)
    root = (PC[m.group(1)] + {'b': -1, '#': 1, None: 0}[m.group(2)]) % 12
    bass_pc = (PC[m.group(4)] + {'b': -1, '#': 1, None: 0}[m.group(5)]) % 12 if m.group(4) else root
    pcs = chord_pcs(sym)
    bass = next(n for n in range(low, low + 12) if n % 12 == bass_pc)
    up = [n for n in range(bass + 6, bass + 40) if n % 12 in pcs][:count - 1]
    return [bass] + up


class Piece:
    def __init__(self, name, bpm, bpb=4, tempo=None, seed=1):
        self.name = name
        self.bpb = bpb
        self.clock = Clock(bpm, [((b - 1) * bpb + beat, t, g) for (b, beat, t, g) in (tempo or [])])
        self.T = build()
        self.harm = []       # (beat, symbol)
        self.seed = seed
        self.warnings = []
        self.sections = []   # (bar, label) for the listening page

    # ------------------------------------------------------------ time
    def at(self, bar, beat=0.0):
        """Bars count from 1; beats inside a bar from 0."""
        return (bar - 1) * self.bpb + beat

    def section(self, bar, label):
        self.sections.append((bar, label))

    # ------------------------------------------------------------ harmony
    def harmony(self, bar, text):
        """'Bb . . F | Gm' : one symbol per beat-group; '.' holds the chord. Each '|' starts the next bar.
        Inside a bar the symbols share the bar evenly."""
        for i, chunk in enumerate(text.split('|')):
            syms = chunk.split()
            if not syms:
                continue
            step = self.bpb / len(syms)
            for j, s in enumerate(syms):
                if s != '.':
                    chord_pcs(s)
                    self.harm.append((self.at(bar + i, j * step), s))
        self.harm.sort()

    def chord_at(self, beat):
        cur = None
        for b, s in self.harm:
            if b <= beat + 1e-6:
                cur = s
            else:
                break
        return cur

    # ------------------------------------------------------------ notes
    def line(self, track, bar, text, vel='mf', art='sus', legato=0.0, beat=0.0, check=True, octave=0, **kw):
        """A line of notes. Tokens: 'Bb4:2' (note, beats), 'r:1' (rest), 'Bb4:2.s' (short), '<f>' (dynamic),
        'Bb4:2>' (this note swells), 'Bb4:2\\' (this note fades), '|' (a bar line, checked), '[Bb3,D4]:4' (chord)."""
        tr = self.T[track]
        pos = self.at(bar, beat)
        v = DYN[vel] if isinstance(vel, str) else vel
        tokens = text.split()
        notes = []
        for tok in tokens:
            if tok == '|':
                rel = pos - self.at(bar)
                if abs(rel / self.bpb - round(rel / self.bpb)) > 1e-6:
                    raise ValueError(f'{self.name} {track} bar {bar}: a bar line falls on beat {rel} of the line')
                continue
            m = re.match(r'^<(\w+)>$', tok)
            if m:
                v = DYN[m.group(1)]
                continue
            m = re.match(r'^(\[[^\]]+\]|[A-Ga-g][#b]?-?\d|r):([\d.]+(?:/\d+)?)((?:\.s|\.t|>|\\|@\d+|!)*)$', tok)
            if not m:
                raise ValueError(f'{self.name} {track}: cannot read {tok!r}')
            pitch, d, flags = m.groups()
            dur = eval(d)
            if pitch != 'r':
                keys = [k.strip() for k in pitch.strip('[]').split(',')]
                a = art
                vv = v
                curve = kw.get('curve')
                if '.s' in flags:
                    a = 'stac' if 'stac' in tr.arts else ('spic' if 'spic' in tr.arts else art)
                if '.t' in flags:
                    a = 'trem'
                mv = re.search(r'@(\d+)', flags)
                if mv:
                    vv = int(mv.group(1))
                if '!' in flags:
                    vv = min(127, vv + 14)
                if '>' in flags:
                    curve = [(0, -9), (0.85, 0), (1, 0)]
                if '\\' in flags:
                    curve = [(0, 0), (0.3, -1), (1, -12)]
                for k in keys:
                    key = note_number(k) + 12 * octave
                    extra = {kk: vv2 for kk, vv2 in kw.items() if kk not in ('curve',)}
                    hold = dur + (legato if a in ('sus', 'trem') else 0)
                    if a in ('stac', 'spic'):
                        hold = min(dur, 0.5)
                    tr.add(pos, hold, key, vv, a, curve=curve, **extra)
                    notes.append((pos, dur, key, k))
                    if check:
                        self._check(track, pos, dur, key, k)
            pos += dur
        return pos

    def chords(self, track, bar, seq, vel='mf', art='sus', curve=None, beat=0.0, **kw):
        """seq: list of (beats, 'Bb3 D4 F4'). Every note of a chord is held for its beats."""
        pos = self.at(bar, beat)
        v = DYN[vel] if isinstance(vel, str) else vel
        for item in seq:
            beats, notes = item[0], item[1]
            vv = item[2] if len(item) > 2 else v
            cv = item[3] if len(item) > 3 else curve
            if notes not in ('r', ''):
                for k in notes.split():
                    tr = self.T[track]
                    tr.add(pos, beats + kw.get('overlap', 0.0), note_number(k), DYN[vv] if isinstance(vv, str) else vv, art, curve=cv,
                           **{a: b for a, b in kw.items() if a != 'overlap'})
                    self._check(track, pos, beats, note_number(k), k)
            pos += beats
        return pos

    def arp(self, track, bar, bars, step=0.5, pattern=(0, 1, 2, 3, 4, 3, 2, 1), vel='p', low=36, count=5,
            art='pluck', accent=8, ring=1.0):
        """Spreads whatever chord the harmony names at each moment across the instrument, note by note:
        a harp's ripple. pattern indexes the chord's notes from the bass up."""
        v = DYN[vel] if isinstance(vel, str) else vel
        pos = self.at(bar)
        end = self.at(bar + bars)
        i = 0
        while pos < end - 1e-9:
            sym = self.chord_at(pos)
            notes = chord_tones(sym, low, count)
            k = notes[pattern[i % len(pattern)] % len(notes)]
            vv = v + (accent if abs(pos % self.bpb) < 1e-6 else 0)
            self.T[track].add(pos, step * ring, k, vv, art)
            pos += step
            i += 1

    def ostinato(self, track, bar, bars, pattern=('R', 'R', '5', 'R', 'R', '5', '3', '2'), step=0.5, vel='p',
                 accents=(0, 3, 6), accent=14, low=38, art='spic', octave=0):
        """A driving figure that follows the harmony: R the bass, 5 the chord's fifth above it, 3 its third,
        2 a step above the bass, 8 the bass an octave up."""
        v = DYN[vel] if isinstance(vel, str) else vel
        pos = self.at(bar)
        end = self.at(bar + bars)
        i = 0
        n = len(pattern)
        while pos < end - 1e-9:
            sym = self.chord_at(pos)
            m = re.match(r'^([A-G])(b|#)?(.*?)(?:/([A-G])(b|#)?)?$', sym)
            root = (PC[m.group(1)] + {'b': -1, '#': 1, None: 0}[m.group(2)]) % 12
            bass_pc = (PC[m.group(4)] + {'b': -1, '#': 1, None: 0}[m.group(5)]) % 12 if m.group(4) else root
            pcs = chord_pcs(sym)
            bass = next(k for k in range(low, low + 12) if k % 12 == bass_pc)
            fifth_pc = (root + 7) % 12 if (root + 7) % 12 in pcs else (root + 6) % 12
            third_pc = (root + 3) % 12 if (root + 3) % 12 in pcs else (root + 4) % 12
            above = lambda pc: next(k for k in range(bass + 1, bass + 13) if k % 12 == pc)
            tok = pattern[i % n]
            k = {'R': bass, '5': above(fifth_pc), '3': above(third_pc), '2': bass + 2, '8': bass + 12,
                 'b': bass - 1}[tok] + 12 * octave
            vv = v + (accent if (i % n) in accents else 0)
            self.T[track].add(pos, step, k, vv, art)
            pos += step
            i += 1

    def hit(self, track, bar, beat, key, vel, art='hit', **kw):
        v = DYN[vel] if isinstance(vel, str) else vel
        self.T[track].add(self.at(bar, beat), kw.pop('dur', 1.0), note_number(key) if isinstance(key, str) else key, v, art, **kw)

    def drums(self, track, bar, pattern, accent=96, normal=70, ghost=40, roll_vel=None, bars=1):
        """A drum part on a grid of sixteenths: 'X' accent, 'x' a stroke, 'g' a ghost stroke, 'R' a roll lasting
        through the '-' after it, '.' silence. Spaces are ignored. Repeats for `bars` bars."""
        p = pattern.replace(' ', '')
        step = self.bpb / len(p)
        for b in range(bars):
            i = 0
            while i < len(p):
                c = p[i]
                pos = self.at(bar + b, i * step)
                if c in 'Xxg':
                    v = {'X': accent, 'x': normal, 'g': ghost}[c]
                    self.T[track].add(pos, 0.5, 60, v, 'hit')
                    i += 1
                elif c == 'R':
                    j = i + 1
                    while j < len(p) and p[j] == '-':
                        j += 1
                    rv = roll_vel or normal
                    self.T[track].add(pos, (j - i) * step, 60, rv, 'roll', release=0.08)
                    i = j
                else:
                    i += 1

    # ------------------------------------------------------------ the check
    def _check(self, track, pos, dur, key, name):
        art_ranges = [a.range for a in self.T[track].arts.values()]
        lo = min(r[0] for r in art_ranges); hi = max(r[1] for r in art_ranges)
        if not (lo <= key <= hi) and track not in ('snare', 'bd', 'cym'):
            self.warnings.append(f'RANGE {track} {name} ({key}) outside {lo}-{hi} at bar {pos // self.bpb + 1:.0f}')
        if track in ('snare', 'bd', 'cym', 'timp'):
            return
        sym = self.chord_at(pos)
        if not sym:
            return
        on_strong = abs((pos % self.bpb) % (self.bpb / 2)) < 1e-6
        if on_strong and dur >= 1 and (key % 12) not in chord_pcs(sym):
            bar = int(pos // self.bpb) + 1
            beat = pos % self.bpb
            self.warnings.append(f'HARM {track} {name} on {sym} at bar {bar} beat {beat:g} ({dur:g} beats)')

    # ------------------------------------------------------------ sound
    def render(self, bars, outdir, loop=True, tail=6.0, hall=None, wet_db=-8.0, lufs=-16.0, stems_only=None):
        end_beat = self.at(bars + 1)
        n_loop = int(round(self.clock.sec(end_beat) * SR))
        total = n_loop / SR + tail
        stems = {}
        used = [t for t in self.T.values() if t.notes]
        for tr in used:
            stems[tr.name] = render_track(tr, self.clock, total, seed=self.seed)
        hall = hall if hall is not None else make_hall()
        y, dry, wet = mix(stems, used, hall, wet_db=wet_db)
        if loop:
            y = fold_loop(y, n_loop)
        y, L, tp = master(y, target_lufs=lufs)
        os.makedirs(outdir, exist_ok=True)
        path = os.path.join(outdir, f'{self.name}.wav')
        sf.write(path, y, SR, subtype='PCM_16')
        info = {
            'name': self.name, 'seconds': round(len(y) / SR, 2), 'lufs': round(float(L), 1), 'true_peak_db': round(float(tp), 2),
            'bars': bars, 'loop': loop, 'sections': [{'bar': b, 'at': round(self.clock.sec(self.at(b)), 2), 'label': l} for b, l in self.sections],
            'desks': {tr.name: len(tr.notes) for tr in used},
            'stem_lufs': {},
            'warnings': self.warnings,
        }
        import pyloudnorm as pyln
        meter = pyln.Meter(SR)
        for k, s in stems.items():
            try:
                info['stem_lufs'][k] = round(float(meter.integrated_loudness(s[:n_loop])), 1)
            except Exception:
                pass
        # the balance, desk by desk and section by section, as a table the composer can read
        bounds = [self.clock.sec(self.at(b)) for b, _ in self.sections] + [n_loop / SR]
        rows = []
        head = 'desk      ' + ' '.join(f'{b:>6d}' for b, _ in self.sections)
        rows.append(head)
        for k, s_ in stems.items():
            vals = []
            for i in range(len(self.sections)):
                a, b = int(bounds[i] * SR), int(bounds[i + 1] * SR)
                try:
                    v = meter.integrated_loudness(s_[a:b].astype(np.float64))
                    vals.append(f'{v:6.1f}' if np.isfinite(v) and v > -60 else '     .')
                except Exception:
                    vals.append('     .')
            rows.append(f'{k:9s} ' + ' '.join(vals))
        vals = []
        for i in range(len(self.sections)):
            a, b = int(bounds[i] * SR), int(bounds[i + 1] * SR)
            vals.append(f'{meter.integrated_loudness(y[a:b].astype(np.float64)):6.1f}')
        rows.append('MIX       ' + ' '.join(vals))
        open(os.path.join(outdir, f'{self.name}_balance.txt'), 'w').write('\n'.join(rows) + '\n')
        info['balance'] = rows
        self.picture(y, os.path.join(outdir, f'{self.name}_picture.png'), bounds)
        import engine
        info['limiter_db'] = round(getattr(engine.master, 'last_reduction', 0.0), 2)
        json.dump(info, open(os.path.join(outdir, f'{self.name}.json'), 'w'), indent=1)
        return path, info

    def picture(self, y, path, bounds):
        """The piece drawn: pitch upward on a musical (log) scale, time across, loudness as brightness."""
        import matplotlib
        matplotlib.use('Agg')
        import matplotlib.pyplot as plt
        m = y.mean(axis=1)
        nfft, hop = 8192, 2048
        win = np.hanning(nfft)
        frames = [np.abs(np.fft.rfft(m[i:i + nfft] * win)) for i in range(0, len(m) - nfft, hop)]
        S = 20 * np.log10(np.array(frames).T + 1e-9)
        fr = np.fft.rfftfreq(nfft, 1 / SR)
        t = np.arange(S.shape[1]) * hop / SR
        fig, ax = plt.subplots(2, 1, figsize=(18, 8), gridspec_kw={'height_ratios': [3, 1]})
        sel = (fr > 30) & (fr < 6000)
        ax[0].pcolormesh(t, fr[sel], S[sel], shading='auto', vmin=S.max() - 80, vmax=S.max(), cmap='magma')
        ax[0].set_yscale('log'); ax[0].set_ylabel('Hz')
        for b, (bar, lab) in zip(bounds, self.sections):
            ax[0].axvline(b, color='cyan', lw=0.6); ax[0].text(b, 4500, f'{bar} {lab}', color='cyan', fontsize=8)
        w = int(0.4 * SR)
        rms = [20 * np.log10(np.sqrt(np.mean(m[i:i + w] ** 2)) + 1e-9) for i in range(0, len(m) - w, w // 2)]
        ax[1].plot(np.arange(len(rms)) * w / 2 / SR, rms); ax[1].set_ylim(-60, 0); ax[1].grid(alpha=0.3)
        ax[1].set_ylabel('dB')
        fig.tight_layout(); fig.savefig(path, dpi=70); plt.close(fig)
