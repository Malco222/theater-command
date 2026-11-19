"""Checks a finished piece without ears: clicks, the loop seam, silences, level."""
import sys, numpy as np, soundfile as sf
from scipy import signal
import pyloudnorm as pyln
for path in sys.argv[1:]:
    x, sr = sf.read(path, dtype='float32')
    m = x.mean(axis=1)
    # clicks: a sample-to-sample jump far above what the music around it does
    hp = signal.sosfilt(signal.butter(4, 9000, 'hp', fs=sr, output='sos'), m)
    env = np.sqrt(signal.convolve(hp ** 2, np.ones(2205) / 2205, mode='same')) + 1e-6
    ratio = np.abs(hp) / env
    clicks = np.where((ratio > 9) & (np.abs(hp) > 10 ** (-40 / 20)))[0]
    groups = []
    for c in clicks:
        if not groups or c - groups[-1][-1] > sr // 20:
            groups.append([c])
        else:
            groups[-1].append(c)
    # the seam: the last 50 ms against the first 50 ms
    w = int(0.05 * sr)
    end_rms = 20 * np.log10(np.sqrt(np.mean(x[-w:] ** 2)) + 1e-9)
    start_rms = 20 * np.log10(np.sqrt(np.mean(x[:w] ** 2)) + 1e-9)
    jump = np.abs(x[0] - x[-1]).max()
    # silences longer than a second
    blk = int(0.1 * sr)
    r = np.array([np.sqrt(np.mean(m[i:i + blk] ** 2)) for i in range(0, len(m) - blk, blk)])
    quiet = (20 * np.log10(r + 1e-9) < -55)
    longest = 0; run = 0
    for q in quiet:
        run = run + 1 if q else 0; longest = max(longest, run)
    L = pyln.Meter(sr).integrated_loudness(x)
    print(f'{path.split("/")[-1]}: {len(x)/sr:.1f}s  {L:.1f} LUFS  peak {20*np.log10(np.abs(x).max()):.1f} dBFS  '
          f'clicks {len(groups)} {[round(g[0]/sr,2) for g in groups[:8]]}  seam: end {end_rms:.1f} dB, start {start_rms:.1f} dB, '
          f'step {jump:.4f}  longest silence {longest/10:.1f}s  dc {m.mean():.5f}')
