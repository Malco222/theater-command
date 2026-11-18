"""Brian's own songs into the game: set the level, keep the notes untouched.

Each song is decoded, turned down (or up) as a whole to the score's loudness, checked for true peak
(four times oversampled) and written as the game's WAV (44.1 kHz, 16-bit) and a listening MP3.
Nothing else is changed: no cut, no EQ, no new ending. Run: python3 prepare_songs.py <originals> <out>"""
import sys, os, subprocess, json
import numpy as np, soundfile as sf, pyloudnorm as pyln
from scipy import signal

SONGS = [  # (original file, game name, loudness, where it plays)
    ('The_Final_Salute.mp3', 'theme', -16.0, 'opening screen: the main theme'),
    ('The_Long_Walk_Home.mp3', 'long_walk', -18.0, 'when a war ends, then the campaign map at peace with The Map Room and A Vigil at the Ridge'),
    ('The_Silent_Front.mp3', 'silent_front', -16.0, 'campaign map at war, taking turns with Standing To'),
    ('A_Vigil_at_the_Ridge.mp3', 'vigil', -18.0, 'campaign map at peace, taking turns with The Map Room and The Long Walk Home'),
    ('Trench_Line_At_Twilight.mp3', 'trench_line', -15.0, 'battle, taking turns with The Advance'),
]
src, out = sys.argv[1], sys.argv[2]
os.makedirs(out, exist_ok=True)
report = {}
for fname, name, target, where in SONGS:
    tmp = os.path.join(out, f'_{name}_decoded.wav')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', os.path.join(src, fname), '-ar', '44100', '-c:a', 'pcm_f32le', tmp], check=True)
    x, sr = sf.read(tmp, dtype='float64'); os.remove(tmp)
    meter = pyln.Meter(sr)
    before = meter.integrated_loudness(x)
    gain_db = target - before
    y = x * 10 ** (gain_db / 20)
    tp = 20 * np.log10(np.abs(signal.resample_poly(y, 4, 1, axis=0)).max())
    if tp > -1.0:                       # only if needed: a plain turn-down, never a limiter
        y *= 10 ** ((-1.0 - tp) / 20); gain_db += -1.0 - tp
        tp = -1.0
    wav = os.path.join(out, f'{name}.wav')
    sf.write(wav, y, sr, subtype='PCM_16')
    subprocess.run(['ffmpeg', '-v', 'error', '-y', '-i', wav, '-codec:a', 'libmp3lame', '-b:a', '320k',
                    os.path.join(out, f'{name}.mp3')], check=True)
    after = meter.integrated_loudness(sf.read(wav)[0])
    report[name] = {'from': fname, 'plays': where, 'seconds': round(len(y) / sr, 1), 'loudness_before': round(before, 1),
                    'gain_db': round(gain_db, 2), 'loudness_after': round(after, 1), 'true_peak_db': round(tp, 1)}
    print(name, report[name])
json.dump(report, open(os.path.join(out, 'brian_songs.json'), 'w'), indent=1)
