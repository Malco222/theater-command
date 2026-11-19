"""Makes the whole score: renders the three pieces written here, checks them, and writes the files the game
and the listening page use. The main theme and The Silent Front are Brian's own recordings: prepare_songs.py
sets their level. (first_theme.py, the earlier main theme, still renders on request.)
Run: python3 make_all.py <out folder> [piece ...]"""
import sys, os, json, subprocess, importlib
sys.path.insert(0, '/home/user/score'); sys.path.insert(0, '/home/user/score/pieces')
out = sys.argv[1] if len(sys.argv) > 1 else '/home/user/score/out'
PIECES = [('map_room', -18.0), ('standing_to', -16.0), ('advance', -15.0), ('first_theme', -16.0)]
only = sys.argv[2:] or [p for p, _ in PIECES[:3]]
from engine import save_loudness
summary = {}
for name, lufs in PIECES:
    if name not in only:
        continue
    mod = importlib.import_module(name)
    P = mod.P
    if P.warnings:
        print(name, 'WARNINGS:\n  ' + '\n  '.join(P.warnings))
    tail = {'first_theme': 7.0, 'map_room': 7.0, 'standing_to': 6.0, 'advance': 5.0}[name]
    path, info = P.render(mod.BARS, out, loop=True, tail=tail, lufs=lufs)
    save_loudness()
    mp3 = os.path.join(out, f'{name}.mp3')
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-i', path, '-codec:a', 'libmp3lame', '-b:a', '192k', mp3], check=True)
    q = subprocess.run(['python3', '/home/user/score/qa.py', path], capture_output=True, text=True).stdout.strip()
    info['qa'] = q
    summary[name] = info
    print(q)
json.dump(summary, open(os.path.join(out, 'summary.json'), 'w'), indent=1)
