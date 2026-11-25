// The guns each drawn aircraft carried, by the month it flies (Brian, 2026-11-18 19:35Z: "make the gun fire very
// realistic and the tracer fire too"; the design /mnt/project-files/air-battles/realism/air-combat-realism.md,
// section 3; the historian's review beside it, review-historian.md, questions 1, 2, 3, 9, 10 and 13).
//
// Shown, never counted (ruling 237): nothing here moves a loss. The ticket carries each side's loadout to the 3D game
// (AirPlace.guns), which fires each gun from its socket at its own rate and muzzle speed, through the effects kit's
// AirGun row of the same kind (fx.json air_guns: rifle, hmg, cannon20) and the mix's air.gunsByCalibre.
//
// Sockets are the model files' own (/mnt/project-files/battle-models/air/<Model>.json). A gun with no socket on the
// model (the He 111's beam guns, a French bomber's ventral gun) has 'at' empty: it is heard and its gunner fires, from
// the nearest socket on that side, but it adds no muzzle of its own. Where the sources give a range, the middle is
// used; numbers the historian marked unchecked say so.

import type { When } from './doctrine';

/** The effects kit's air gun row (fx.json air_guns), which also picks the mix's calibre (SKY.cues.calibre). */
export type GunKind = 'rifle' | 'hmg' | 'cannon20';
/** Fixed and firing ahead; in a power turret; or hand-held at a window or hatch. */
export type Mount = 'fixed' | 'turret' | 'hand';

export interface Gun {
  /** The gun's name, as the history books give it. */
  gun: string;
  kind: GunKind;
  mount: Mount;
  /** Its muzzles on the model; one gun to each, or several guns sharing one socket (the guns over the engine). */
  at: string[];
  /** How many guns: at least at.length. */
  n: number;
  /** Rounds a minute, each gun (a gun firing through the airscrew a tenth slower, FG-G2). */
  rate: number;
  /** Muzzle speed, metres a second. */
  mv: number;
  /** Rounds each gun carries; drum: rounds in each drum or pan, where it fed from them. */
  rounds: number;
  drum?: number;
  /**
   * A gunner's arc: yaw in degrees clockwise from the nose seen from above (starboard 90, the tail 180, port 270),
   * running clockwise from the first to the second; pitch from below to above. Null for a power turret whose limits are
   * in the model file.
   */
  arc?: { yaw: [number, number]; pitch: [number, number] } | null;
}

export interface Loadout {
  /** The mark it flies as that month (Bf 109E-1, B-17F...). */
  mark: string;
  guns: Gun[];
  /** Where the wing guns' streams cross, in metres; null with no wing guns (or a gunner's aircraft). */
  conv: number | null;
  /** The model's muzzles that stay dark that month. */
  silent: string[];
  /** A gunner's reach, in metres, by kind (a gunner fires only at an attacker inside his arc and this, 3.3). */
  reach?: Partial<Record<GunKind, number>>;
}

const at = (w: When): number => w.year + w.month / 12;
const MID = (a: number, b: number) => Math.round((a + b) / 2);
/** Synchronised to fire through the airscrew: a tenth slower (FG-G2). */
const SYNC = 0.9;

// ---- The guns (table 3.1 with the historian's settlements, question 2) -----------------------------------------

type Spec = Pick<Gun, 'gun' | 'kind' | 'rate' | 'mv'>;
const G: Record<string, Spec> = {
  mg17: { gun: 'MG 17', kind: 'rifle', rate: 1_150, mv: 850 },
  mg15: { gun: 'MG 15', kind: 'rifle', rate: 1_000, mv: 755 },
  mg81: { gun: 'MG 81', kind: 'rifle', rate: 1_500, mv: 790 },
  mg81z: { gun: 'MG 81Z', kind: 'rifle', rate: 3_000, mv: 790 },
  mg131: { gun: 'MG 131', kind: 'hmg', rate: 900, mv: MID(722, 730) },
  mgff: { gun: 'MG FF', kind: 'cannon20', rate: 520, mv: 600 },
  mgffm: { gun: 'MG FF/M', kind: 'cannon20', rate: 540, mv: 700 },
  // A mixed belt: the middle of armour-piercing 705 and the mine shell 805.
  mg151: { gun: 'MG 151/20', kind: 'cannon20', rate: 750, mv: MID(705, 805) },
  browning303: { gun: 'Browning .303', kind: 'rifle', rate: 1_150, mv: 750 },
  vickersK: { gun: 'Vickers K', kind: 'rifle', rate: 1_000, mv: 745 },
  hispano: { gun: 'Hispano Mk II', kind: 'cannon20', rate: 600, mv: 880 },
  hs404: { gun: 'HS.404', kind: 'cannon20', rate: 700, mv: 880 },
  mac34: { gun: 'MAC 1934', kind: 'rifle', rate: MID(1_200, 1_500), mv: 830 },
  m2: { gun: 'Browning M2 .50', kind: 'hmg', rate: 800, mv: 880 },
  m1919: { gun: 'Browning .30', kind: 'rifle', rate: 1_000, mv: 850 },
  shkas: { gun: 'ShKAS', kind: 'rifle', rate: 1_800, mv: 870 },
  ubs: { gun: 'UBS', kind: 'hmg', rate: 900, mv: 860 },
  shvak: { gun: 'ShVAK', kind: 'cannon20', rate: 800, mv: MID(750, 770) },
  type97: { gun: 'Type 97', kind: 'rifle', rate: 900, mv: 750 },
  type99: { gun: 'Type 99-1', kind: 'cannon20', rate: 520, mv: 600 },
  // Italy's 12.7 mm and 7.7 mm (the historian, question 10: the guns; rates and speeds unchecked).
  breda127: { gun: 'Breda-SAFAT 12.7 mm', kind: 'hmg', rate: 700, mv: 765 },
  breda77: { gun: 'Breda-SAFAT 7.7 mm', kind: 'rifle', rate: 900, mv: 730 },
  // Japan's flexible 7.7 mm, the Lewis pattern on 97-round pans (rate and speed unchecked).
  type92: { gun: 'Type 92', kind: 'rifle', rate: 600, mv: 750 },
  // The plain fighter and bomber of a nation the sources do not cover: a rifle-calibre and a heavy gun of the period.
  rifle: { gun: 'rifle-calibre machine gun', kind: 'rifle', rate: 1_100, mv: 800 },
  heavy: { gun: 'heavy machine gun', kind: 'hmg', rate: 800, mv: 860 },
};

function gun(spec: Spec, mount: Mount, where: string[], rounds: number, more: Partial<Gun> = {}): Gun {
  return { ...spec, mount, at: where, n: Math.max(1, where.length), rounds, ...more };
}
const fixed = (spec: Spec, where: string[], rounds: number, more: Partial<Gun> = {}) => gun(spec, 'fixed', where, rounds, more);
const wings = (k: number) => Array.from({ length: k }, (_, i) => `gun.${i + 1}`);

/** Yards to metres. */
const yd = (x: number) => Math.round(x * 0.9144);

/** The RAF's wing guns: the Dowding spread at about 400 yd until June 1940 (turn 10), then a point at 250 yd (question 3). */
const raf = (w: When) => (at(w) < 1940 + 5 / 12 ? yd(400) : yd(250));

// ---- Fighters (table 3.2, questions 1, 2, 3, 8 and 10) ---------------------------------------------------------

export function fighterGuns(model: string, nat: string, w: When): Loadout {
  const d = at(w);
  switch (model) {
    case 'Bf109': {
      // The E-1 to April 1940, the E-3 from May 1940 (turn 9), the E-4 from July 1940 (turn 11); the F-4 from 1941,
      // its hub cannon and two MG 17s over the engine, no wing guns (question 1; G5, G32).
      if (d >= 1941) return { mark: 'Bf 109F-4', conv: null, silent: ['gun.1', 'gun.2'], guns: [fixed(G.mg151, ['gun.nose'], 200, { n: 1 }), fixed(G.mg17, ['gun.nose'], 500, { n: 2, rate: Math.round(G.mg17.rate * SYNC) })] };
      const nose = fixed(G.mg17, ['gun.nose'], 1_000, { n: 2, rate: 1_000 });
      if (d < 1940 + 4 / 12) return { mark: 'Bf 109E-1', conv: 300, silent: [], guns: [nose, fixed(G.mg17, ['gun.1', 'gun.2'], 500)] };
      if (d < 1940 + 6 / 12) return { mark: 'Bf 109E-3', conv: 300, silent: [], guns: [nose, fixed(G.mgff, ['gun.1', 'gun.2'], 60, { drum: 60 })] };
      return { mark: 'Bf 109E-4', conv: 300, silent: [], guns: [nose, fixed(G.mgffm, ['gun.1', 'gun.2'], 60, { drum: 60 })] };
    }
    case 'Fw190':
      // The A-4: MG 17s over the engine, synchronised MG 151/20s in the wing roots, MG FF/Ms outboard in drums (often
      // removed). The A-8 from 1944: MG 131s over the engine and MG 151/20s inboard and out (question 2; medium-low).
      if (d < 1944) return { mark: 'Fw 190A-4', conv: 300, silent: [], guns: [fixed(G.mg17, ['gun.nose'], 900, { n: 2, rate: 1_000 }), fixed(G.mg151, ['gun.1', 'gun.2'], 200, { rate: MID(600, 650) }), fixed(G.mgffm, ['gun.3', 'gun.4'], 90, { drum: 90 })] };
      return { mark: 'Fw 190A-8', conv: 300, silent: [], guns: [fixed(G.mg131, ['gun.nose'], 450, { n: 2, rate: Math.round(G.mg131.rate * SYNC) }), fixed(G.mg151, ['gun.1', 'gun.2'], 250, { rate: MID(600, 650) }), fixed(G.mg151, ['gun.3', 'gun.4'], 140)] };
    case 'Hurricane':
      return { mark: 'Hurricane I', conv: raf(w), silent: [], guns: [fixed(G.browning303, wings(8), 333)] };
    case 'Spitfire':
      // The model is a Mk I with eight ports. The Mk V's B wing from 1941 (cannon in drums), its C wing from 1942
      // (belts), the IX's E wing from 1944 (two cannon, two .50s). Which port takes which gun is the modeller's
      // question 6 (G33, unchecked): here the cannon inboard, the machine guns in the outer ports.
      if (d < 1941) return { mark: 'Spitfire I', conv: raf(w), silent: [], guns: [fixed(G.browning303, wings(8), 300)] };
      if (d < 1942) return { mark: 'Spitfire VB', conv: yd(250), silent: ['gun.3', 'gun.4'], guns: [fixed(G.hispano, ['gun.1', 'gun.2'], 60, { drum: 60 }), fixed(G.browning303, ['gun.5', 'gun.6', 'gun.7', 'gun.8'], 350)] };
      if (d < 1944) return { mark: 'Spitfire IX (C wing)', conv: yd(250), silent: ['gun.3', 'gun.4'], guns: [fixed(G.hispano, ['gun.1', 'gun.2'], 120), fixed(G.browning303, ['gun.5', 'gun.6', 'gun.7', 'gun.8'], 350)] };
      return { mark: 'Spitfire IX (E wing)', conv: yd(250), silent: ['gun.3', 'gun.4', 'gun.7', 'gun.8'], guns: [fixed(G.hispano, ['gun.1', 'gun.2'], 120), fixed(G.m2, ['gun.5', 'gun.6'], 250)] };
    case 'Warhawk':
      return { mark: 'P-40E', conv: MID(150, 300), silent: [], guns: [fixed(G.m2, wings(6), 281)] };
    case 'Mustang':
      // The P-51B and C: four .50s (350 rounds inboard, 280 outboard); the six-gun P-51D from June 1944 (turn 58).
      if (d < 1944 + 5 / 12) return { mark: 'P-51B', conv: yd(300), silent: ['gun.5', 'gun.6'], guns: [fixed(G.m2, ['gun.1', 'gun.2'], 350), fixed(G.m2, ['gun.3', 'gun.4'], 280)] };
      return { mark: 'P-51D', conv: yd(300), silent: [], guns: [fixed(G.m2, ['gun.1', 'gun.2'], 400), fixed(G.m2, ['gun.3', 'gun.4', 'gun.5', 'gun.6'], 270)] };
    case 'Yak': {
      // The Yak-1 in 1942, the Yak-1B from late 1942, the Yak-9 to mid-1944, the Yak-3 beside it from July 1944: the
      // ShVAK through the hub and the guns over the engine, nothing in the wings (question 8).
      const hub = fixed(G.shvak, ['gun.nose'], 120, { n: 1 });
      const ubs = (k: number) => fixed(G.ubs, ['gun.nose'], k === 2 ? 250 : 200, { n: k, rate: Math.round(G.ubs.rate * SYNC) });
      if (d < 1942 + 9 / 12) return { mark: 'Yak-1', conv: null, silent: [], guns: [hub, fixed(G.shkas, ['gun.nose'], 750, { n: 2, rate: Math.round(G.shkas.rate * SYNC) })] };
      if (d < 1943) return { mark: 'Yak-1B', conv: null, silent: [], guns: [hub, ubs(1)] };
      if (d < 1944 + 6 / 12) return { mark: 'Yak-9', conv: null, silent: [], guns: [hub, ubs(1)] };
      return { mark: 'Yak-3', conv: null, silent: [], guns: [hub, ubs(2)] };
    }
    case 'Zero': {
      // The A6M2's cannon in drums of 60; drums of 100 on the A6M3 and early A6M5 (from mid-1942); belts of 125 from
      // 1944 (question 2: the models are the historian's, the months are ours).
      const c = d < 1942 + 6 / 12 ? 60 : d < 1944 ? 100 : 125;
      return { mark: d < 1942 + 6 / 12 ? 'A6M2' : d < 1944 ? 'A6M3' : 'A6M5', conv: 250, silent: [], guns: [fixed(G.type97, ['gun.nose'], 500, { n: 2, rate: Math.round(G.type97.rate * SYNC) }), fixed(G.type99, ['gun.1', 'gun.2'], c, c < 125 ? { drum: c } : {})] };
    }
    // France's D.520 model (23 Nov) flies the D.520's guns from the first: the MS.406 before it is the plain fighter.
    // The model has a muzzle for each of the four MAC 1934s: gun.1 and gun.3 port, gun.2 and gun.4 starboard.
    case 'D520': {
      const L = plainFighter('FRA', Math.max(d, 1940 + 5 / 12));
      return { ...L, guns: L.guns.map((x) => (x.gun === G.mac34.gun ? { ...x, at: wings(4), n: 4 } : x)) };
    }
  }
  return plainFighter(nat, d);
}

/** The plain fighter, by its nation (question 10): France's, Italy's, the Soviet I-16 and everyone else's. */
function plainFighter(nat: string, d: number): Loadout {
  if (nat === 'FRA') {
    // The MS.406 to May 1940, the D.520 from June 1940: an HS.404 through the hub and MAC 1934s in the wings, at 250 m.
    if (d < 1940 + 5 / 12) return { mark: 'MS.406', conv: 250, silent: [], guns: [fixed(G.hs404, ['gun.nose'], 60, { n: 1, drum: 60 }), fixed(G.mac34, ['gun.1', 'gun.2'], 300)] };
    // Two of the D.520's four MAC 1934s to a wing socket; rounds unchecked.
    return { mark: 'D.520', conv: 250, silent: [], guns: [fixed(G.hs404, ['gun.nose'], 60, { n: 1, drum: 60 }), fixed(G.mac34, ['gun.1', 'gun.2'], 675, { n: 4 })] };
  }
  if (nat === 'ITA') {
    // Two 12.7 mm over the engine and nothing else (CR.42, G.50, MC.200); the MC.202 of late 1941 adds two 7.7 mm in
    // the wings (high). Rounds unchecked.
    const nose = fixed(G.breda127, ['gun.nose'], 370, { n: 2, rate: Math.round(G.breda127.rate * SYNC) });
    if (d < 1941 + 10 / 12) return { mark: 'MC.200', conv: null, silent: ['gun.1', 'gun.2'], guns: [nose] };
    return { mark: 'MC.202', conv: 250, silent: [], guns: [nose, fixed(G.breda77, ['gun.1', 'gun.2'], 500)] };
  }
  if (nat === 'SOV') {
    // The I-16 type 24 to the end of 1941: two ShKAS over the engine, two in the wings, at about 250 m (rounds unchecked).
    return { mark: 'I-16 type 24', conv: 250, silent: [], guns: [fixed(G.shkas, ['gun.nose'], 650, { n: 2, rate: Math.round(G.shkas.rate * SYNC) }), fixed(G.shkas, ['gun.1', 'gun.2'], 900)] };
  }
  // Everyone else: two rifle-calibre guns over the engine and two in the wings to 1941 (the PZL P.11, the Fokker
  // D.XXI, the IAR-80); a heavy machine gun added in each wing from 1942 (low-medium). Rounds are placeholders.
  const base = [fixed(G.rifle, ['gun.nose'], 500, { n: 2, rate: Math.round(G.rifle.rate * SYNC) }), fixed(G.rifle, ['gun.1', 'gun.2'], 500)];
  return { mark: `Fighter:${nat}`, conv: 250, silent: [], guns: d < 1942 ? base : [...base, fixed(G.heavy, ['gun.1', 'gun.2'], 250)] };
}

// ---- Bombers (table 3.3, questions 9, 10 and 13) ---------------------------------------------------------------

/** A gunner's arc: about a facing (nose 0, starboard 90, tail 180, port 270), either side by 'half'. */
const about = (facing: number, half: number, lo: number, hi: number) => ({ yaw: [(facing - half + 360) % 360, (facing + half) % 360] as [number, number], pitch: [lo, hi] as [number, number] });
const from = (a: number, b: number, lo: number, hi: number) => ({ yaw: [a, b] as [number, number], pitch: [lo, hi] as [number, number] });
const hand = (spec: Spec, where: string[], rounds: number, arc: Gun['arc'], more: Partial<Gun> = {}) => gun(spec, 'hand', where, rounds, { arc, ...more });
const turret = (spec: Spec, where: string[], rounds: number, arc: Gun['arc'] = null) => gun(spec, 'turret', where, rounds, { arc });

/** A gunner's reach (3.3): 550 m for the .50 (G18); a placeholder 370 m for rifle calibre (B5); cannon as the .50. */
const REACH: Record<GunKind, number> = { rifle: 370, hmg: 550, cannon20: 550 };


export function bomberGuns(model: string, nat: string, w: When): Loadout {
  const d = at(w);
  const box = (mark: string, guns: Gun[], silent: string[] = []): Loadout => ({ mark, conv: null, silent, guns, reach: REACH });
  switch (model) {
    case 'He111': {
      // Arcs (question 13, all low-medium): the nose ball 35 either side; the dorsal, firing aft, 70 either side of the
      // tail and up to 70; the ventral gondola 40 either side of the tail and down; the ventral forward 40 about the nose.
      const nose = from(325, 35, -20, 30), dorsal = about(180, 70, 0, 70), gondola = about(180, 40, -60, 0), fwd = about(0, 40, -60, 0);
      const drums = (k: number) => ({ drum: 75, rounds: 75 * k });
      // The He 111P of 1939: three MG 15s on 75-round drums (high); rounds a position a placeholder of eight drums.
      if (d < 1940) return box('He 111P', [hand(G.mg15, ['turret.1'], 600, nose, drums(8)), hand(G.mg15, ['turret.2'], 600, dorsal, drums(8)), hand(G.mg15, ['turret.3'], 600, gondola, drums(8))], ['turret.4']);
      // The H-2 to H-6: the ventral forward gun and the beam guns from early 1940, five to seven MG 15s, 8 to 16 drums a
      // position (medium). The beam guns have no socket on the model.
      if (d < 1943) return box('He 111H', [hand(G.mg15, ['turret.1'], 900, nose, drums(12)), hand(G.mg15, ['turret.2'], 900, dorsal, drums(12)), hand(G.mg15, ['turret.3'], 900, gondola, drums(12)), hand(G.mg15, ['turret.4'], 900, fwd, drums(12)), hand(G.mg15, [], 900, about(90, 40, -30, 30), drums(12)), hand(G.mg15, [], 900, about(270, 40, -30, 30), drums(12))]);
      // The H-16 from 1943: an MG FF/M in the nose, an MG 131 above, an MG 81Z below, MG 81s in the waist (G16).
      return box('He 111H-16', [hand(G.mgffm, ['turret.1'], 180, nose, { drum: 60 }), hand(G.mg131, ['turret.2'], 1_000, dorsal), hand(G.mg81z, ['turret.3'], 850, gondola), hand(G.mg81, ['turret.4'], 500, fwd), hand(G.mg81, [], 500, about(90, 40, -30, 30)), hand(G.mg81, [], 500, about(270, 40, -30, 30))]);
    }
    case 'B17': {
      // The model is a G (13 guns), laid out again to the real B-17G on 19 November (the modeller, 01:17Z; Brian's
      // benchmark): chin turret.1, .2; top turret.3, .4; ball turret.5, .6; cheeks turret.7 (starboard), .8; waist
      // turret.9 (starboard), .10; radio hatch turret.11; tail turret.12, .13. About 500 rounds a gun at every position
      // (question 2). Power turrets keep the file's limits; the hand-held arcs are the design's, the tail and the waist
      // as the historian corrected them.
      const tail = hand(G.m2, ['turret.12', 'turret.13'], 500, about(180, 30, -30, 30), { n: 2 });
      const waist = [hand(G.m2, ['turret.9'], 500, from(30, 150, -40, 50)), hand(G.m2, ['turret.10'], 500, from(210, 330, -40, 50))];
      const top = turret(G.m2, ['turret.3', 'turret.4'], 500), ball = turret(G.m2, ['turret.5', 'turret.6'], 500);
      const radio = hand(G.m2, ['turret.11'], 500, about(180, 40, 10, 70));
      const cheeks = [hand(G.m2, ['turret.7'], 500, from(0, 60, -30, 30)), hand(G.m2, ['turret.8'], 500, from(300, 360, -30, 30))];
      // The B-17E: a .30 in the nose (fired from the nose glazing; drawn at the starboard cheek) and no chin turret.
      if (d < 1943) return box('B-17E', [hand(G.m1919, ['turret.7'], 500, from(320, 40, -30, 30)), top, ball, ...waist, radio, tail], ['turret.1', 'turret.2', 'turret.8']);
      // The B-17F of 1943: .50s in both cheeks from early 1943, still no chin turret.
      if (d < 1943 + 8 / 12) return box('B-17F', [...cheeks, top, ball, ...waist, radio, tail], ['turret.1', 'turret.2']);
      // The B-17G at the front from September 1943, with the chin turret; the radio gun often removed from mid-1944.
      const chin = turret(G.m2, ['turret.1', 'turret.2'], 500);
      if (d < 1944 + 5 / 12) return box('B-17G', [chin, ...cheeks, top, ball, ...waist, radio, tail]);
      return box('B-17G', [chin, ...cheeks, top, ball, ...waist, tail], ['turret.11']);
    }
    case 'Lancaster':
      // The FN5 nose and FN50 mid-upper turrets, two .303s each; the FN20 tail turret, four. No belly gun (high): a
      // fighter from below draws no fire. Arcs, the historian's (21:19Z, medium), in the model file since 21:22Z: the
      // nose 95 either side, 45 down to 60 up; the mid-upper all round, level to 70 up (the file has no cut-out, so the
      // 3D game keeps it off her own fins and wings); the tail 95 either side, 60 down to 60 up, the widest field of
      // fire in the aircraft. Rounds unchecked: about 1,000 a gun forward and above, about 2,500 in the tail.
      return box('Lancaster I', [turret(G.browning303, ['turret.1', 'turret.2'], 1_000, about(0, 95, -45, 60)), turret(G.browning303, ['turret.3', 'turret.4'], 1_000, from(0, 360, 0, 70)), turret(G.browning303, ['turret.5', 'turret.6', 'turret.7', 'turret.8'], 2_500, about(180, 95, -60, 60))]);
    // The four models of 23 Nov, laid on their own sockets (the 3D thread's reading of the files, 08:41Z). Guns from the
    // standard references (medium, for the historian's check); rounds unchecked. Arcs as the same turret or position on
    // the Lancaster, the B-17 and the He 111.
    case 'Wellington': {
      // The Mk IC: an FN5 nose turret and an FN10 tail turret, two .303s each, and a hand-held .303 at each beam window
      // (turret.5 starboard, turret.6 port). The Mk III, at the front from mid-1941: the FN20 tail turret's four guns,
      // drawn from the file's two tail muzzles.
      const nose = turret(G.browning303, ['turret.1', 'turret.2'], 1_000, about(0, 95, -45, 60));
      const beams = [hand(G.browning303, ['turret.5'], 500, from(30, 150, -40, 50)), hand(G.browning303, ['turret.6'], 500, from(210, 330, -40, 50))];
      const tail = (n: number) => ({ ...turret(G.browning303, ['turret.3', 'turret.4'], n === 4 ? 2_500 : 1_000, about(180, 95, -60, 60)), n });
      return d < 1941 + 5 / 12 ? box('Wellington IC', [nose, tail(2), ...beams]) : box('Wellington III', [nose, tail(4), ...beams]);
    }
    case 'Pe2': {
      // Two fixed guns in the nose (gun.1, gun.2), the navigator's gun above facing aft (turret.1) and the gunner's below
      // facing aft (turret.2). ShKAS throughout at first; from mid-1942 a UBK beside a ShKAS in the nose and UBTs above
      // and below.
      const above = about(180, 70, 0, 70), below = about(180, 40, -60, 0);
      if (d < 1942 + 6 / 12) return box('Pe-2', [fixed(G.shkas, ['gun.1', 'gun.2'], 750), hand(G.shkas, ['turret.1'], 750, above), hand(G.shkas, ['turret.2'], 750, below)]);
      return box('Pe-2 (1942)', [fixed(G.ubs, ['gun.1'], 150), fixed(G.shkas, ['gun.2'], 450), hand(G.ubs, ['turret.1'], 200, above), hand(G.ubs, ['turret.2'], 200, below)]);
    }
    case 'LeO451':
      // A 20 mm HS.404 above facing aft (turret.1), a MAC 1934 below facing aft (turret.2), and a fixed MAC 1934 in the
      // nose, which the model gives no muzzle (it fires from the nearest socket and adds none).
      return box('LeO 451', [hand(G.hs404, ['turret.1'], 120, about(180, 70, 0, 70), { drum: 60 }), hand(G.mac34, ['turret.2'], 500, about(180, 40, -60, 0)), fixed(G.mac34, [], 300)]);
    case 'SM79':
      // The rebuilt model (23 Nov 08:53Z): a fixed 12.7 mm firing forward from the top of the hump (turret.1), a 12.7 mm
      // at the back of the hump facing aft (turret.2), one in the gondola below facing aft (turret.3), and a 7.7 mm at
      // each beam window (turret.4 starboard, turret.5 port; one gun moved between them on early aircraft).
      return box('SM.79', [fixed(G.breda127, ['turret.1'], 500), hand(G.breda127, ['turret.2'], 350, about(180, 70, 0, 70)), hand(G.breda127, ['turret.3'], 350, about(180, 40, -60, 0)), hand(G.breda77, ['turret.4'], 500, from(30, 150, -40, 50)), hand(G.breda77, ['turret.5'], 500, from(210, 330, -40, 50))]);
    case 'Nell':
      // The G3M2: a 20 mm Type 99 in the rear dorsal position (turret.2), a 7.7 mm in the retractable dorsal turret
      // (turret.1), one in each side blister (turret.3 starboard, turret.4 port), and one in the cockpit without a muzzle.
      return box('G3M2', [hand(G.type99, ['turret.2'], 360, about(180, 70, 0, 70), { drum: 45 }), hand(G.type92, ['turret.1'], 582, from(0, 360, 0, 70), { drum: 97 }), hand(G.type92, ['turret.3'], 582, from(30, 150, -40, 50), { drum: 97 }), hand(G.type92, ['turret.4'], 582, from(210, 330, -40, 50), { drum: 97 }), hand(G.type92, [], 582, from(320, 40, -30, 30), { drum: 97 })]);
    case 'Betty': {
      // The G4M1: a 7.7 mm in the nose (turret.1), one in the dorsal blister (turret.2), one in each side blister
      // (turret.3 starboard, turret.4 port) and a 20 mm Type 99 in the tail (turret.5). The G4M2, at the front from
      // late 1943: 20 mm in the dorsal turret and the side blisters too.
      const nose = hand(G.type92, ['turret.1'], 582, from(320, 40, -30, 30), { drum: 97 });
      const tail = hand(G.type99, ['turret.5'], 360, about(180, 30, -30, 30), { drum: 45 });
      const up = about(180, 80, 0, 80), right = from(30, 150, -40, 50), left = from(210, 330, -40, 50);
      if (d < 1943 + 8 / 12) return box('G4M1', [nose, hand(G.type92, ['turret.2'], 582, up, { drum: 97 }), hand(G.type92, ['turret.3'], 582, right, { drum: 97 }), hand(G.type92, ['turret.4'], 582, left, { drum: 97 }), tail]);
      return box('G4M2', [nose, turret(G.type99, ['turret.2'], 450, up), hand(G.type99, ['turret.3'], 360, right, { drum: 45 }), hand(G.type99, ['turret.4'], 360, left, { drum: 45 }), tail]);
    }
  }
  return plainBomber(nat, d);
}

/** The plain twin: a hand-held gun in the nose and a dorsal turret (the file's limits); France adds a ventral gun. */
function plainBomber(nat: string, d: number): Loadout {
  const nose = from(320, 40, -30, 30), reach = REACH;
  if (nat === 'UK' || nat === 'CAN' || nat === 'AUS' || nat === 'NZL' || nat === 'SAF' || nat === 'IND')
    // Vickers K guns on 100-round pans (about 96 loaded); ten pans a gun, a placeholder.
    return { mark: 'Bomber:UK', conv: null, silent: [], reach, guns: [hand(G.vickersK, ['turret.1'], 960, nose, { drum: 96 }), turret(G.vickersK, ['turret.2'], 960)] };
  if (nat === 'FRA')
    // The Amiot 143 and Bloch MB.210 of 1939: MAC 1934s in the nose, above and below (question 10, medium-high).
    return { mark: 'Bomber:FRA', conv: null, silent: [], reach, guns: [hand(G.mac34, ['turret.1'], 500, nose), turret(G.mac34, ['turret.2'], 500), hand(G.mac34, [], 500, about(180, 40, -60, 0))] };
  return { mark: `Bomber:${nat}`, conv: null, silent: [], reach, guns: [hand(G.rifle, ['turret.1'], 500, nose), turret(d < 1942 ? G.rifle : G.heavy, ['turret.2'], 500)] };
}
