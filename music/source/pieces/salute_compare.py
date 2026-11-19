import librosa, numpy as np, sys
def chroma(path, sr=22050):
    y,_ = librosa.load(path, sr=sr, mono=True)
    yh = librosa.effects.harmonic(y)
    C = librosa.feature.chroma_cqt(y=yh, sr=sr, hop_length=512)
    return C, sr/512
Co, fo = chroma('/home/user/lwh/fs/fs.wav')
Cr, fr = chroma(sys.argv[1])
off_o = float(sys.argv[2]) if len(sys.argv) > 2 else 9.11
bar = 2.0; n = int(sys.argv[3]) if len(sys.argv) > 3 else 16
names = ['C','Db','D','Eb','E','F','Gb','G','Ab','A','Bb','B']
seg = lambda C, f, t0, t1: C[:, int(t0*f):int(t1*f)].mean(axis=1)
M = np.zeros((n,n))
for i in range(n):
    o = seg(Co, fo, off_o+i*bar, off_o+(i+1)*bar)
    for j in range(n):
        M[i,j] = np.corrcoef(o, seg(Cr, fr, j*bar, (j+1)*bar))[0,1]
np.set_printoptions(precision=2, suppress=True, linewidth=200)
print('same bar:', np.round(np.diag(M),2), 'mean', round(np.diag(M).mean(),2))
print('other bars mean', round(M[~np.eye(n,dtype=bool)].mean(),2))
for i in range(n):
    o = seg(Co, fo, off_o+i*bar, off_o+(i+1)*bar); r = seg(Cr, fr, i*bar, (i+1)*bar)
    print(i+1, 'Brian:', [names[k] for k in np.argsort(o)[::-1][:4]], ' render:', [names[k] for k in np.argsort(r)[::-1][:4]], round(M[i,i],2))
