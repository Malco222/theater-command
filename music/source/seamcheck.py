"""The loop join as the game will play it: the last two seconds followed by the first two, checked for clicks."""
import sys, numpy as np, soundfile as sf
from scipy import signal
for path in sys.argv[1:]:
    x, sr = sf.read(path, dtype='float64')
    j = np.concatenate([x[-2 * sr:], x[:2 * sr]]).mean(axis=1)
    hp = signal.sosfilt(signal.butter(4, 9000, 'hp', fs=sr, output='sos'), np.concatenate([x[-4 * sr:-2 * sr].mean(axis=1), j]))[2 * sr:]
    env = np.sqrt(np.maximum(signal.convolve(hp ** 2, np.ones(2205) / 2205, mode='same'), 0)) + 1e-6
    ratio = np.abs(hp) / env
    near = slice(2 * sr - int(0.05 * sr), 2 * sr + int(0.05 * sr))
    worst = ratio[near].max()
    body = np.percentile(ratio, 99.99)
    print(f'{path.split("/")[-1]}: at the join the sharpest high-frequency jump is {worst:.1f}x its surroundings '
          f'(the music elsewhere in these 4 s reaches {body:.1f}x; a click scores above 9)')
