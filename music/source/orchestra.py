"""The orchestra: every desk, which recordings it plays, and where it sits on the stage."""
import os, glob, re
from engine import Articulation, Region, Track, resolve, LIB


class Hits:
    """A drum or cymbal: recordings sorted by how hard they were struck. The key is ignored; the
    velocity chooses the recording, and takes alternate between the recordings at that strength."""

    def __init__(self, name, layers, gain_db=0.0, release=None, lead=0.0):
        # layers: list of (max velocity, [relative paths])
        self.name = name
        self.layers = [(v, [resolve(p) for p in ps]) for v, ps in layers]
        self.release = release
        self.attack = None
        self.lead = lead
        self.gain_db = gain_db
        self.target = -23.0
        self.range = (0, 127)
        self.counter = {}

    def pick(self, key, vel, rng):
        for vmax, paths in self.layers:
            if vel <= vmax:
                break
        n = self.counter.get(vmax, rng.randrange(len(paths)))
        self.counter[vmax] = n + 1
        r = Region()
        r.path = paths[n % len(paths)]
        r.lokey, r.hikey, r.kc = 0, 127, key
        r.lovel, r.hivel = 0, 127
        r.tune = 0.0
        r.seqlen, r.seqpos, r.lorand, r.hirand = 1, 1, 0, 1
        r.release, r.attack = 2.0, 0.001
        r.vol = 0.0
        return r


def files(pattern):
    base = os.path.join(LIB, '')
    out = sorted(os.path.relpath(p, LIB) for p in glob.glob(os.path.join(LIB, pattern)))
    if not out:
        raise FileNotFoundError(pattern)
    return out


def strings_arts(prefix, long_rel=0.6):
    return {
        'sus': Articulation(f'{prefix} sus', f'{prefix}EnsSusVib.sfz' if prefix != 'Contrabass' else 'ContrabassSusVB.sfz',
                            release=long_rel, attack=0.06, lead=0.05),
        'spic': Articulation(f'{prefix} spic', f'{prefix}EnsSpic.sfz' if prefix != 'Contrabass' else 'ContrabassSpic.sfz', release=0.25, lead=0.0),
        'trem': Articulation(f'{prefix} trem', f'{prefix}EnsTrem.sfz' if prefix != 'Contrabass' else 'ContrabassTrem.sfz', release=0.6, attack=0.08, lead=0.03),
        'pizz': Articulation(f'{prefix} pizz', f'{prefix}EnsPizz.sfz' if prefix != 'Contrabass' else 'ContrabassPizz.sfz', release=0.8),
    }


def build():
    """The desks, seated in the usual concert layout: violins to the left, cellos and basses to the right,
    woodwinds in the middle, brass and drums at the back."""
    A = Articulation
    T = {}
    vln = strings_arts('Violin')
    T['vln1'] = Track('vln1', vln, pan=-0.55, width=0.8, send=0.22, depth=0.0)
    T['vln2'] = Track('vln2', strings_arts('Violin'), pan=-0.25, width=0.8, send=0.22, depth=0.05)
    T['vla'] = Track('vla', strings_arts('Viola'), pan=0.15, width=0.7, send=0.24, depth=0.1)
    T['vc'] = Track('vc', strings_arts('Cello'), pan=0.45, width=0.7, send=0.22, depth=0.05)
    T['cb'] = Track('cb', strings_arts('Contrabass', 0.7), pan=0.6, width=0.5, send=0.25, depth=0.2,
                    players=3, spread_cents=7, spread_ms=18)
    T['hn'] = Track('hn', {
        'sus': A('horn sus', 'FHornSus.sfz', release=0.7, attack=0.04, lead=0.04),
        'stac': A('horn stac', 'FHornStac.sfz', release=0.3),
    }, pan=-0.3, width=0.6, send=0.38, depth=0.6, players=3, spread_cents=6, spread_ms=16)
    T['tpt'] = Track('tpt', {
        'sus': A('trumpet sus', 'TrumpetSus.sfz', release=0.5, attack=0.02, lead=0.02),
        'vib': A('trumpet vib', 'TrumpetSusVib.sfz', release=0.5, attack=0.02, lead=0.02),
        'stac': A('trumpet stac', 'TrumpetStac.sfz', release=0.3),
        'mute': A('trumpet mute', 'TrumpetStraightMuteSus.sfz', release=0.4, attack=0.02, lead=0.02),
    }, pan=0.15, width=0.5, send=0.34, depth=0.7, players=2, spread_cents=5, spread_ms=12)
    T['tbn'] = Track('tbn', {
        'sus': A('trombone sus', 'TromboneSus.sfz', release=0.6, attack=0.03, lead=0.03),
        'stac': A('trombone stac', 'TromboneStac.sfz', release=0.3),
    }, pan=0.35, width=0.5, send=0.34, depth=0.75, players=2, spread_cents=6, spread_ms=14)
    T['tba'] = Track('tba', {
        'sus': A('tuba sus', 'TubaSus.sfz', release=0.6, attack=0.03, lead=0.03),
        'stac': A('tuba stac', 'TubaStac.sfz', release=0.3),
    }, pan=0.5, width=0.4, send=0.3, depth=0.75)
    T['fl'] = Track('fl', {'sus': A('flute', 'FluteSusVib.sfz', release=0.4, attack=0.03, lead=0.03),
                           'stac': A('flute stac', 'FluteStac.sfz', release=0.25)},
                    pan=-0.1, width=0.4, send=0.3, depth=0.35)
    T['ob'] = Track('ob', {'sus': A('oboe', 'OboeSusVib.sfz', release=0.35, attack=0.02, lead=0.02),
                           'stac': A('oboe stac', 'OboeStac.sfz', release=0.25)},
                    pan=0.05, width=0.4, send=0.3, depth=0.35)
    T['cl'] = Track('cl', {'sus': A('clarinet', 'ClarinetSus.sfz', release=0.35, attack=0.03, lead=0.03),
                           'stac': A('clarinet stac', 'ClarinetStac.sfz', release=0.25)},
                    pan=-0.15, width=0.4, send=0.3, depth=0.4)
    T['bsn'] = Track('bsn', {'sus': A('bassoon', 'BassoonSus.sfz', release=0.35, attack=0.03, lead=0.03),
                            'stac': A('bassoon stac', 'BassoonStac.sfz', release=0.25)},
                     pan=0.15, width=0.4, send=0.3, depth=0.4)
    T['harp'] = Track('harp', {'pluck': A('harp', 'Harp.sfz', release=2.5)}, pan=-0.65, width=0.6, send=0.3, depth=0.3)
    T['timp'] = Track('timp', {
        'hit': A('timpani', 'Timpani.sfz', release=3.0),
        'roll': A('timpani roll', 'TimpaniRolls.sfz', release=1.5, attack=0.3),
    }, pan=0.0, width=0.6, send=0.4, depth=0.9)
    T['snare'] = Track('snare', {
        'hit': Hits('snare', [
            (30, files('VSCO 1 Percussion/drums/snare/drum3_marching/snare3_ppp_*.wav')),
            (50, files('VSCO 1 Percussion/drums/snare/drum3_marching/snare3_pp_*.wav')),
            (75, files('VSCO 1 Percussion/drums/snare/drum3_marching/snare3_p_*.wav')),
            (95, files('VSCO 1 Percussion/drums/snare/drum3_marching/snare3_mp_*.wav')),
            (115, files('VSCO 1 Percussion/drums/snare/drum3_marching/snare3_f_*.wav')),
            (127, files('VSCO 1 Percussion/drums/snare/drum3_marching/snare3_fff_*.wav')),
        ], release=0.6),
        'roll': Hits('snare roll', [
            (60, files('Percussion/Snare2-rollSN_v1_*.wav')),
            (95, files('Percussion/Snare2-rollSN_v3_*.wav')),
            (127, files('Percussion/Snare2-rollSN_v5_*.wav')),
        ], release=0.4),
    }, pan=0.1, width=0.5, send=0.35, depth=0.85)
    T['bd'] = Track('bd', {'hit': Hits('bass drum', [
        (40, files('VSCO 1 Percussion/drums/bass/bdrum_ppp_*.wav') + files('VSCO 1 Percussion/drums/bass/bdrum_pp_*.wav')),
        (70, files('VSCO 1 Percussion/drums/bass/bdrum_p_*.wav') + files('VSCO 1 Percussion/drums/bass/bdrum_mp_*.wav')),
        (100, files('VSCO 1 Percussion/drums/bass/bdrum_f_*.wav')),
        (127, files('VSCO 1 Percussion/drums/bass/bdrum_ff_*.wav') + files('VSCO 1 Percussion/drums/bass/bdrum_fff_*.wav')),
    ], release=3.0),
        'roll': Hits('bass drum roll', [(127, files('VSCO 1 Percussion/drums/bass/bdrum_roll_long1.wav'))], release=1.0)},
        pan=-0.05, width=0.5, send=0.45, depth=0.95)
    T['cym'] = Track('cym', {
        'crash': Hits('crash', [
            (45, files('Percussion/cymbal-crash1_pp_*.wav')),
            (75, files('Percussion/cymbal-crash1_mp_*.wav')),
            (100, files('Percussion/cymbal-crash1_mf_*.wav')),
            (127, files('Percussion/cymbal-crash1_ff_*.wav')),
        ], release=4.0),
        'swell': Hits('cymbal swell', [
            (60, files('Percussion/susCymb1-cresc-Short_v1.wav')),
            (90, files('Percussion/susCymb1-cresc-Median_v1.wav')),
            (127, files('Percussion/susCymb1-cresc-Long_v1.wav')),
        ], release=1.5),
        'sus': Hits('suspended cymbal', [
            (50, files('Percussion/susCymb1-hit_pp_*.wav')),
            (80, files('Percussion/susCymb1-hit_mp_*.wav')),
            (105, files('Percussion/susCymb1-hit_f_*.wav')),
            (127, files('Percussion/susCymb1-hit_fff_*.wav')),
        ], release=4.0),
        'gong': Hits('tam-tam', [
            (50, files('Percussion/gongHit_p.wav')),
            (80, files('Percussion/gongHit_mf.wav')),
            (105, files('Percussion/gongHit_f.wav')),
            (127, files('Percussion/gongHit_fff.wav')),
        ], release=6.0),
    }, pan=0.2, width=0.8, send=0.45, depth=0.9)
    T['pno'] = Track('pno', {'hit': A('piano', 'UprightPiano.sfz', release=0.8)}, pan=-0.2, width=0.8, send=0.28, depth=0.3)
    level(T)
    return T


# How loud one mf note of each desk should be (LUFS), before the music asks for any balance of its own:
# brass carry more than strings, strings more than woodwinds, as in a hall.
TARGET = {
    'vln1.sus': -22, 'vln1.spic': -23, 'vln1.trem': -24, 'vln1.pizz': -25,
    'vla.sus': -23, 'vla.spic': -24, 'vla.trem': -25, 'vla.pizz': -26,
    'vc.sus': -22, 'vc.spic': -23, 'vc.trem': -24, 'vc.pizz': -25,
    'cb.sus': -22, 'cb.spic': -23, 'cb.trem': -24, 'cb.pizz': -25,
    'hn.sus': -20, 'hn.stac': -21, 'tpt.sus': -20, 'tpt.vib': -20, 'tpt.stac': -21, 'tpt.mute': -23,
    'tbn.sus': -20, 'tbn.stac': -21, 'tba.sus': -21, 'tba.stac': -22,
    'fl.sus': -24, 'fl.stac': -25, 'ob.sus': -24, 'ob.stac': -25, 'cl.sus': -24, 'cl.stac': -25,
    'bsn.sus': -24, 'bsn.stac': -25, 'harp.pluck': -25, 'pno.hit': -25,
    'timp.hit': -23, 'timp.roll': -25, 'snare.hit': -26, 'snare.roll': -29, 'bd.hit': -25, 'bd.roll': -28,
    'cym.crash': -24, 'cym.swell': -25, 'cym.sus': -25, 'cym.gong': -26,
}


def level(T):
    for desk, tr in T.items():
        for a, art in tr.arts.items():
            key = f'{desk}.{a}'
            alt = f'vln1.{a}' if desk == 'vln2' else (f'tpt.{a}' if desk.startswith('tpt') else key)
            art.target = TARGET.get(key, TARGET.get(alt, -23.0))
