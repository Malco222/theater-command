// Recorded land battles for the 3D game's ground battle scene, before the land rules reach the Unreal rules bundle
// (Brian, 21 November 2026 16:22Z: "the same intense feeling as the air battle", and 16:27Z: "a normandy one with
// shermans vs tiger tanks and soldiers and artillery ... a dry run of a land battle just like the air"). Each is a set
// piece fought out by the computer on both sides with the squad brain on (TC.battleMen), written as the bridge hands it
// to the 3D game: the field once, the blocks every battle second, every event and every round once, and every man's
// track. The scene replays it ("-TheaterLandBattle=normandy-1944"). Nothing is booked: no campaign reads it.
//
// It lives on the scene's branch (claude/project-thread-uhhxrn, based on unreal) but runs against the land rules, which
// are on the rules branches: bundle it from a worktree of claude/r2-support-teams (3671f29 or later), with this file at
// the same path there:
//
//   npx esbuild unreal/TheaterCommand/Tools/land_record/record.ts --bundle --platform=node --format=esm \
//     --outfile=dist/land-record.mjs --log-level=warning
//   node dist/land-record.mjs <out folder> [key] [seed]    (writes <key>.json; key normandy-1944 when left out)
//
// Marks (release 2, the production lead's re-record for the camera, 22 November): with the tactics rules
// (claude/project-thread-ljtuk8) a round carries g on a tank's machine-gun round ('coax', 'hull', 'roof'), cm: 1 where it
// struck a commander up in his hatch, and fate on a kill on a tank ('out', 'burn', 'blow'); a man's track carries ld, the
// battle seconds [from, to] he is one of the squad leading his side's assault (the platoon cam's mark), and cl, his radio
// calls' stamps (the signaller's call before a shoot). All absent where empty. A piece may ask the drill for platoons or
// the drill book (SetPiece.drill); with neither, sections as before. A fourth argument names the file written.
//
// The men are written as tracks, not as snapshots: a man's place and facing are kept where a straight line between the
// kept points would miss him by more than KEEP_M (or his facing by KEEP_F), and his pose, cover, aim, hidden and
// buttoned flags whenever they change, at the step's own time (0.1 s). So the scene lerps between kept points and
// sees every change of pose on its step. The drill's rules hold (battle-bridge.ts, "The men"): no man moves more than
// 0.9 m a step, so a lerp between kept points is never more than KEEP_M from where the drill had him.
//
// Full sections (Brian, 21 November 2026 18:29Z: "the ground battle needs a lot more soldiers in it"): the drill runs
// with { sections: true }, so each rules figure is a whole section, gun crew or team. A man is known by his block (b),
// his figure (k) and, for every man but the figure's key man, his place in its section (j, from 1). A track and a round
// carry j as the drill gives it: a round's j is the shooter's, its oj the man it struck. A man with no j is the key man.

import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import type { FieldTicket, ForceTicket, SideTicket } from '../../../../src/battle/core/types';
import { DT } from '../../../../src/battle/core/types';
import { GROUNDS, groundAt, type LandField, type LandPlace } from '../../../../src/battle/land/field';
import { LandBattle } from '../../../../src/battle/land/sim';
import { withTeams } from '../../../../src/battle/land/sample';
import { LAND } from '../../../../src/battle/land/rules';
import { calFor } from '../../../../src/battle/land/shells';
import type { LandUnit } from '../../../../src/battle/land/units';
import type { MenOut, MenBlock, Round } from '../../../../src/battle/land/drill';
import { unitOut, planOut } from '../../Rules/battle-bridge';

/** A kept point may be left out where the straight line from the last kept one misses the man by less than this. */
const KEEP_M = 0.10;
const KEEP_F = 0.05;

interface Army {
  nat: string;
  adj: string;
  color: string;
  /** The soldier file the scene draws this side's men with (battle-models/land/soldier/soldier_<army>.json). */
  army: string;
  /** Until the army's own soldier file is made: the file drawn in its stead, painted as this army. */
  standIn?: string;
  /** The tank model each armoured block is drawn with, in order of the side's armoured blocks; the last repeats. */
  tanks: string[];
  /** The gun model of each gun block (a support team draws its own team). */
  gun: string;
  general: string;
  style: 'bold' | 'methodical' | 'mass';
}

interface SetPiece {
  key: string;
  title: string;
  date: string;
  turn: number;
  place: LandPlace;
  att: Army;
  def: Army;
  /** [name, kind, divisions, share of its side, first division's number, dug in] */
  attForces: [string, string, number, number, number, boolean?][];
  defForces: [string, string, number, number, number, boolean?][];
  odds: number;
  /** The seeds tried, best first by the pick rule (pick() below), when no seed is given. */
  seeds: number[];
  /** Strength and experience of each side (0..1). */
  str: [number, number];
  xp: [number, number];
  /**
   * The month's weather for the scene's sky: cloud cover 0..1 and its base in metres (looks only). Where given, the
   * wind the smoke and dust lean with (metres a second toward east and north) and the battle camera's grade (the colour
   * kept in the darks and the middle tones, red and blue: the art director's four numbers, LandBattle's Grade).
   */
  sky: { cover: number; base: number; kind: 'cumulus' | 'deck'; wind?: [number, number]; grade?: [number, number, number, number] };
  /** The place on the earth, for the sun (latitude, longitude) and the hour of the attack (local, 24 h). */
  latLon: [number, number];
  hour: number;
  /**
   * The ground made over after the rules lay it (looks and ground both: the fight reads what is laid). 'steppe': the open
   * farmland of the Kursk salient (openSteppe below), for a battle seen across the whole field.
   */
  open?: 'steppe';
  /**
   * What the drill draws beyond full sections (looks only, ruling 217): platoons (the drill book comes with them), or the
   * book alone on sections, so the men drawn stay as many as before.
   */
  drill?: { platoons?: boolean; book?: boolean };
  /** The pick's caps when the piece is bigger than Normandy: tanks lost on each side and men down that still count. */
  caps?: { att: number; def: number; men: number; len: [number, number] };
}

/**
 * Normandy, July 1944: an American attack into the hedgerows on a German line with a platoon of Tigers behind it (Brian's
 * pick, 21 November 16:27Z). A sandbox pairing: nearly every Tiger in Normandy fought the British and Canadians round
 * Caen (the 101st and 102nd SS and the 503rd heavy tank battalions), and the Americans in the bocage met mostly Panzer
 * IVs, StuGs and Panthers; so the German armour is drawn as one block of Tigers beside Panzer IVs, a realistic few. The
 * Americans: two infantry divisions, two armoured blocks of Shermans, their 105 mm howitzers, and the support teams'
 * machine guns and mortars. The Germans: two infantry divisions dug into the hedgerows, the armour, their leFH 18s,
 * machine guns and mortars.
 */
const NORMANDY: SetPiece = {
  key: 'normandy-1944',
  title: 'The hedgerows before Le Mesnil',
  date: 'July 1944',
  turn: 59,
  place: { region: -1, name: 'Normandy', terrain: 'plains', river: null, bridgeDown: false, town: 'Le Mesnil', townSize: 'village', fort: 0, mines: 0 },
  att: { nat: 'USA', adj: 'American', color: '#6b7a4a', army: 'USA', standIn: 'POL', tanks: ['Sherman'], gun: 'Howitzer105', general: 'General Hale', style: 'methodical' },
  def: { nat: 'GER', adj: 'German', color: '#5f6a6e', army: 'GER', tanks: ['Tiger', 'PanzerIV'], gun: 'leFH18', general: 'General Vane', style: 'bold' },
  attForces: [
    ['VII Corps', 'inf', 3, 0.5, 4],
    ['3rd Armored Group', 'arm', 2, 0.32, 3],
    ['Corps Artillery', 'art', 1, 0.18, 1],
  ],
  defForces: [
    ['LXXXIV Corps', 'inf', 3, 0.6, 352, true],
    ['Heavy Panzer Group', 'arm', 2, 0.25, 101],
    ['Artillery Group', 'art', 1, 0.15, 1],
  ],
  odds: 1.2,
  seeds: [11, 3, 7, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107],
  str: [0.92, 0.85],
  xp: [0.45, 0.6],
  sky: { cover: 0.45, base: 900, kind: 'cumulus' },
  latLon: [49.12, -1.09],
  hour: 9.5,
  // Platoons, as the Mac's gate recording (normandy-1944-platoons-seed11) draws them; the drill book comes with them.
  drill: { platoons: true },
};

/**
 * Prokhorovka, July 1943: the Soviet tank army's counter-attack across the open farmland and steppe south-west of the
 * station, into a German panzer corps on the ground it had taken (Brian, 22 November 00:12Z: "a battle in an open field
 * so we can see them battle more clearly", and "an epic battle with artillery, tanks, infantry"). The ground is the
 * point: big open fields of wheat and rye, a gully or two, a railway embankment and a state farm, so the armour, the
 * men, the guns and the support teams are all in sight of one another. The Soviets: three corps' worth of T-34s with T-70
 * light tanks among them, rifle divisions riding in behind, their ZiS-3s, machine guns and mortars, attacking in the
 * mass at speed, as the tank army did, to close the range on the German guns. The Germans: two divisions' worth of
 * Panzer IVs and StuGs with a few Tigers, panzergrenadiers dug in on the line they had taken, leFH 18s, machine guns
 * and mortars. No unit names or insignia of the real corps are drawn (neutral names, the project's rule).
 */
const PROKHOROVKA: SetPiece = {
  key: 'prokhorovka-1943',
  title: 'The fields before Prokhorovka',
  date: 'July 1943',
  turn: 47,
  place: { region: -1, name: 'Kursk salient', terrain: 'plains', river: null, bridgeDown: false, town: 'Oktyabrsky', townSize: 'village', fort: 0, mines: 0 },
  att: { nat: 'SOV', adj: 'Soviet', color: '#7b7748', army: 'SOV', tanks: ['T34', 'T34', 'LightTank', 'T34', 'T34', 'LightTank'], gun: 'ZiS3', general: 'General Orlov', style: 'mass' },
  def: { nat: 'GER', adj: 'German', color: '#5f6a6e', army: 'GER', tanks: ['PanzerIV', 'Tiger', 'StuG3', 'PanzerIV'], gun: 'leFH18', general: 'General Vane', style: 'methodical' },
  attForces: [
    ['Guards Tank Army', 'arm', 6, 0.5, 18],
    ['Guards Rifle Corps', 'inf', 4, 0.36, 9],
    ['Army Artillery', 'art', 2, 0.14, 1],
  ],
  defForces: [
    ['Panzer Corps', 'arm', 4, 0.42, 1],
    ['Panzergrenadier Group', 'inf', 3, 0.43, 1, true],
    ['Corps Artillery', 'art', 2, 0.15, 1],
  ],
  odds: 1.3,
  seeds: [5, 13, 17, 11, 3, 7, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59],
  str: [0.95, 0.8],
  xp: [0.35, 0.7],
  // (a close, hazy July morning on the steppe after the night's rain, cumulus building; a light westerly; the grade a
  // little warmer than Normandy's for the ripe rye and the dust: the art director's to check)
  sky: { cover: 0.5, base: 1200, kind: 'cumulus', wind: [2.2, 0.7], grade: [0.45, 0.5, 1.02, 0.99] },
  latLon: [51.03, 36.72],
  hour: 8.5,
  open: 'steppe',
  caps: { att: 10, def: 5, men: 180, len: [180, 420] },
  // Sections, as the gate's recording draws them (no more men until MB10), with the drill book's tactics on them.
  drill: { book: true },
};

const PIECES: SetPiece[] = [NORMANDY, PROKHOROVKA];

function ticketOf(p: SetPiece, seed: number): FieldTicket<LandPlace> {
  const side = (a: Army, forces: SetPiece['attForces'], human: boolean, base: number, str: number, xp: number): SideTicket => ({
    nat: a.nat, adj: a.adj, color: a.color, human, general: { name: a.general, level: 2 }, style: a.style,
    forces: forces.map(([name, kind, divs, share, first, dug], i): ForceTicket => ({
      corps: base + i, name, nat: a.nat, kind, divs, men: divs * 15000 * str, str, org: 0.85, xp, mor: 0.7, share, dug: !!dug, first, battles: 4,
    })),
  });
  const A = 100;
  const t: FieldTicket<LandPlace> = {
    domain: 'land', kind: 'field', seed, turn: p.turn, date: p.date, title: p.title, place: p.place,
    att: side(p.att, p.attForces, false, 1, p.str[0], p.xp[0]),
    def: side(p.def, p.defForces, false, 11, p.str[1], p.xp[1]),
    reckoning: { A, D: A / p.odds, odds: p.odds, dice: [0.8, 1.2], even: 1.2 },
    lines: [], limit: LAND.limit,
  };
  return withTeams(t);
}

/** Rounded to tenths (metres) or thousandths (radians), as the bridge rounds. */
const r1 = (x: number): number => Math.round(x * 10) / 10;
const r2 = (x: number): number => Math.round(x * 100) / 100;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

/** One man's track, as written. */
interface Track {
  /** Block, figure, and his place in the figure's section (absent for the key man). */
  b: number; k: number; j?: number; r: string;
  /** Kept points: [t, x, y, f]. */
  at: number[][];
  /** Changes: [t, pose, cover, aimX|null, aimY|null, hidden 0|1, buttoned 0|1]. */
  ch: (number | string | null)[][];
  /** Shot stamps (battle time), each once. */
  s: number[];
  /** Battle time he fell. */
  d?: number;
  /** Scars, as the last view gave them (each has its own from-time). */
  mk?: unknown[];
  /** Battle time he left the list (his block ran off the field or was withdrawn): fade him out there. */
  out?: number;
  /** A spotter (fo pair), not on the block's tray. */
  fo?: 1;
  /** The battle seconds [from, to] he carries the platoon cam's mark (the squad leading his side's assault). */
  ld?: number[][];
  /** His radio calls' stamps (battle time), each once. */
  cl?: number[];
}

/** Angle difference the short way round. */
const turn = (a: number, b: number): number => { let d = b - a; while (d > Math.PI) d -= 2 * Math.PI; while (d < -Math.PI) d += 2 * Math.PI; return d; };

/** A man's name in the keeper: block, spotter or not, figure, and his place in its section. */
const keyOf = (b: number, m: MenOut, fo: boolean): string => `${b}:${fo ? 'fo' : ''}${m.k}${m.j !== undefined ? `.${m.j}` : ''}`;

class TrackKeeper {
  tracks = new Map<string, Track>();
  /** Points seen since the last kept one, for the line test. */
  pending = new Map<string, number[][]>();
  /** Whether he carried the platoon cam's mark at the step before: his open ld span is extended while he does. */
  private ldOn = new Map<string, boolean>();

  see(t: number, b: number, m: MenOut, fo: boolean): void {
    const key = keyOf(b, m, fo);
    let tr = this.tracks.get(key);
    const p = [r2(t), r1(m.x), r1(m.y), r3(m.f)];
    const ch = [r2(t), m.p, m.c, m.a ? r1(m.a[0]) : null, m.a ? r1(m.a[1]) : null, m.h ? 1 : 0, m.b ? 1 : 0];
    if (!tr) {
      tr = { b, k: m.k, ...(m.j !== undefined ? { j: m.j } : {}), r: m.r, at: [p], ch: [ch], s: [], ...(fo ? { fo: 1 as const } : {}) };
      this.tracks.set(key, tr);
      this.pending.set(key, []);
    } else {
      const last = tr.ch[tr.ch.length - 1];
      if (last.slice(1).some((v, i) => v !== ch[i + 1])) tr.ch.push(ch);
      this.keep(key, tr, p);
    }
    if (m.s !== undefined && (tr.s.length === 0 || tr.s[tr.s.length - 1] !== m.s)) tr.s.push(m.s);
    if (m.ld) {
      const spans = (tr.ld ??= []);
      if (this.ldOn.get(key) && spans.length) spans[spans.length - 1][1] = r2(t); else spans.push([r2(t), r2(t)]);
    }
    this.ldOn.set(key, !!m.ld);
    if (m.cl !== undefined) { const c = r2(m.cl); if (!tr.cl || tr.cl[tr.cl.length - 1] !== c) (tr.cl ??= []).push(c); }
    if (m.d !== undefined) tr.d = m.d;
    if (m.mk) tr.mk = m.mk;
    delete tr.out;
  }

  /** The line test: keep the previous point if the line from the last kept point to this one misses any point between. */
  private keep(key: string, tr: Track, p: number[]): void {
    const pend = this.pending.get(key)!;
    const a = tr.at[tr.at.length - 1];
    const off = (q: number[]): boolean => {
      const span = p[0] - a[0];
      const u = span > 0 ? (q[0] - a[0]) / span : 0;
      const x = a[1] + (p[1] - a[1]) * u, y = a[2] + (p[2] - a[2]) * u, f = a[3] + turn(a[3], p[3]) * u;
      return Math.hypot(q[1] - x, q[2] - y) > KEEP_M || Math.abs(turn(f, q[3])) > KEEP_F;
    };
    if (pend.some(off)) {
      tr.at.push(pend[pend.length - 1]);
      pend.length = 0;
    }
    pend.push(p);
  }

  /** Ends every track: its last point kept. */
  close(): void {
    for (const [key, tr] of this.tracks) {
      const pend = this.pending.get(key)!;
      if (pend.length) tr.at.push(pend[pend.length - 1]);
      pend.length = 0;
    }
  }

  /** Men not seen this step: they left with their block. */
  gone(t: number, seen: Set<string>): void {
    for (const [key, tr] of this.tracks) if (!seen.has(key) && tr.out === undefined) {
      this.ldOn.set(key, false);
      const pend = this.pending.get(key)!;
      if (pend.length) { tr.at.push(pend[pend.length - 1]); pend.length = 0; }
      tr.out = r2(t);
    }
  }
}

interface Fought {
  seed: number;
  out: Record<string, unknown>;
  score: number;
  why: string;
}


/** A small seeded random number source (mulberry32): the steppe's own, so the rules' draws are untouched. */
function rand(seed: number): () => number {
  let a = seed >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

/** The steppe's crops, by the digit each cell carries in Steppe.crop. */
const CROPS = ['none', 'wheat', 'rye', 'pasture', 'fallow'] as const;

/** What the steppe laid, for the scene to draw (looks only: the ground codes it set are what the fight reads). */
interface Steppe {
  /** Each cell's crop as one digit, row by row like ground: an index into crops (0: not a crop: road, farm, gully, wood). */
  crops: readonly string[];
  crop: string;
  /**
   * The farms' fields as laid, out to 6 km round the field (so the scene's country beyond is the same farms): a point
   * (x, y) is turned by -turn about the field's middle (cx, cy), u = cx + (x - cx) cos turn + (y - cy) sin turn and
   * v = cy - (x - cx) sin turn + (y - cy) cos turn; its field is the column of xs that u falls between and the row of
   * ys that v - shift[column] falls between (each column's fields moved north by its own shift, so the fields of one
   * column do not line up with the next's), and patch holds each field's crop digit, row by row (ys), xs.length - 1 to
   * a row.
   */
  fields: { turn: number; cx: number; cy: number; xs: number[]; ys: number[]; shift: number[]; patch: string };
  /** The gully's middle line in metres, from its mouth at the field's edge to its head, and how deep it is at the mouth. */
  balka: { line: [number, number][]; deep: number };
  /** The railway's line on its bank, south to north, in metres, and the bank's height. */
  rail: { line: [number, number][]; bank: number };
  /** The rise the defence holds, its top and how far its slopes run. */
  hill: { x: number; y: number; r: number; rise: number };
}

/**
 * The open steppe of the Kursk salient (Prokhorovka, July 1943), laid over the rules' plains field: the woods and
 * hedged fields of the plains recipe made into the collective farms' great fields of wheat and rye, ripe and standing
 * waist to chest high, with pasture and fallow between, half a kilometre and more on a side; the roll of the ground
 * eased; a broad rise in the defence's half (the hill the panzer corps held before the state farm); a balka (a dry,
 * steep-sided gully, scrub in its floor) running in from the west flank across the ground between the armies, dying out
 * in the middle; and the railway on its bank along the east flank. Roads, the farm's buildings, water and any works are
 * kept as the rules laid them. Every unit's cover is read again afterwards. The fight reads the new ground as any
 * other: wheat and rye are 'field', pasture and fallow 'open', the gully's floor and the bank 'rough', its scrub 'woods'.
 */
function openSteppe(f: LandField, seed: number): Steppe {
  const rnd = rand(seed ^ 0x57e99e);
  const { cols, rows, cell, w, h } = f;
  const at = (c: number, r: number): number => r * cols + c;
  const mid = (i: number): [number, number] => [((i % cols) + 0.5) * cell, (Math.floor(i / cols) + 0.5) * cell];
  const code = (g: string): number => (GROUNDS as readonly string[]).indexOf(g);
  const OPEN = code('open'), FIELD_ = code('field'), WOODS = code('woods'), ROUGH = code('rough'), WORKS = code('works');
  const made = new Set(['open', 'field', 'woods', 'rough', 'marsh', 'rock'].map(code));
  const town = f.town;
  const nearTown = (x: number, y: number): boolean => !!town && Math.hypot(x - town.x, y - town.y) < town.r * 1.25;
  const crop = new Array<number>(cols * rows).fill(0);

  // The roll of the ground eased: the steppe rolls long and low.
  const base = f.height.slice();
  for (let i = 0; i < base.length; i++) f.height[i] = base[i] * 0.55;

  // The farms' great fields: a grid of fields 450 to 800 m wide and 300 to 550 m deep, turned a little off the field's
  // square, each one crop, and no two side by side alike where it can be helped.
  const turn = (rnd() - 0.5) * 0.24, cs = Math.cos(turn), sn = Math.sin(turn);
  const edges = (len: number, lo: number, hi: number): number[] => {
    const e = [-6000];
    while (e[e.length - 1] < len + 6000) e.push(e[e.length - 1] + lo + rnd() * (hi - lo));
    return e;
  };
  const xs = edges(w, 450, 800), ys = edges(h, 300, 550);
  // (each column's fields moved north by up to half a field, by a draw of their own so the rest of the lay is as it was)
  const rshift = rand(seed ^ 0x5b1f7);
  const shift = xs.slice(1).map(() => (rshift() - 0.5) * 420);
  const slot = (v: number, e: number[]): number => { let k = 0; while (k < e.length - 2 && e[k + 1] <= v) k++; return k; };
  const patch = new Map<string, number>();
  const draw = (): number => { const v = rnd(); return v < 0.38 ? 1 : v < 0.72 ? 2 : v < 0.88 ? 3 : 4; };
  const cropOf = (a: number, b: number): number => {
    const key = `${a},${b}`;
    let k = patch.get(key);
    if (k === undefined) {
      const west = patch.get(`${a - 1},${b}`), south = patch.get(`${a},${b - 1}`);
      k = draw();
      for (let tries = 0; tries < 4 && (k === west || k === south); tries++) k = draw();
      patch.set(key, k);
    }
    return k;
  };
  // Filled in order from the south-west, so the neighbours are drawn before.
  let patchOut = '';
  for (let b = 0; b + 1 < ys.length; b++) for (let a = 0; a + 1 < xs.length; a++) patchOut += String(cropOf(a, b));
  for (let i = 0; i < crop.length; i++) {
    const [x, y] = mid(i);
    const u = (x - w / 2) * cs + (y - h / 2) * sn + w / 2, v = -(x - w / 2) * sn + (y - h / 2) * cs + h / 2;
    const a = slot(u, xs), k = cropOf(a, slot(v - shift[a], ys));
    if (!made.has(f.ground[i])) continue;
    if (nearTown(x, y)) continue; // the farm's own gardens, orchards and yards as the rules laid them
    crop[i] = k;
    f.ground[i] = k <= 2 ? FIELD_ : OPEN;
  }

  // The rise the defence holds, in its half, on the attacker's side of the farm and a little west of it.
  const hx = town ? town.x - 200 - rnd() * 120 : w * (0.36 + rnd() * 0.14), hy = town ? town.y - 150 - rnd() * 60 : h * (0.7 + rnd() * 0.06);
  const hill = { x: Math.max(w * 0.2, Math.min(w * 0.8, hx)), y: Math.max(h * 0.6, Math.min(h * 0.8, hy)), r: 430, rise: 11 };
  for (let i = 0; i < f.height.length; i++) {
    const [x, y] = mid(i);
    f.height[i] += hill.rise * Math.exp(-((x - hill.x) ** 2 + (y - hill.y) ** 2) / (2 * hill.r * hill.r));
  }

  // The balka: in from the west edge across the ground between the armies, wandering, deepest at its mouth.
  const line: [number, number][] = [];
  let bx = 0, by = h * (0.34 + rnd() * 0.08), head = 0.25 + rnd() * 0.25;
  const reach = w * (0.42 + rnd() * 0.08);
  while (bx < reach) {
    line.push([bx, by]);
    head += (rnd() - 0.5) * 0.5;
    head = Math.max(-0.15, Math.min(0.6, head));
    bx += 60 * Math.cos(head); by += 60 * Math.sin(head);
    by = Math.min(h * 0.55, by);
  }
  line.push([bx, by]);
  const deep = 7.5;
  const along = (x: number, y: number): { d: number; s: number } => {
    let best = Infinity, s = 0, run = 0, total = 0;
    for (let k = 1; k < line.length; k++) total += Math.hypot(line[k][0] - line[k - 1][0], line[k][1] - line[k - 1][1]);
    for (let k = 1; k < line.length; k++) {
      const [ax, ay] = line[k - 1], [cx, cy] = line[k], dx = cx - ax, dy = cy - ay, L = Math.hypot(dx, dy);
      const t = Math.max(0, Math.min(1, ((x - ax) * dx + (y - ay) * dy) / (L * L)));
      const d = Math.hypot(x - (ax + t * dx), y - (ay + t * dy));
      if (d < best) { best = d; s = (run + t * L) / total; }
      run += L;
    }
    return { d: best, s };
  };
  for (let i = 0; i < f.height.length; i++) {
    const [x, y] = mid(i);
    const { d, s } = along(x, y);
    const wide = 95 * (1 - 0.55 * s), dig = deep * (1 - s * s);
    if (d >= wide) continue;
    f.height[i] -= dig * (1 - (d / wide) ** 2) ** 1.5;
    if (d < 20 && s < 0.92 && made.has(f.ground[i]) && !nearTown(x, y)) {
      // The floor and its steep sides: rough, scrub grown in it here and there.
      const scrub = Math.sin(x * 0.013 + y * 0.021 + seed) * 0.5 + 0.5 < 0.38 && s < 0.75;
      f.ground[i] = scrub ? WOODS : ROUGH;
      crop[i] = 0;
    }
  }

  // The railway on its bank along the east flank, south to north, and on past the field's ends. The bank is the
  // scene's to raise (a crisp 3 m bank on a 25 m cell would smear into a swell): here its cells are rough, the cover a
  // bank gives a man lying on it.
  const rx0 = w * (0.925 + rnd() * 0.02), rx1 = w * (0.945 + rnd() * 0.02), bank = 3;
  const railX = (y: number): number => { const t = y / h; return (1 - t) * (1 - t) * rx0 + 2 * t * (1 - t) * ((rx0 + rx1) / 2 + 12) + t * t * rx1; };
  const rail: [number, number][] = [];
  // (beyond the field's ends, straight on as it leaves)
  const onLine = (y: number): number => y < 0 ? railX(0) + y * (railX(50) - railX(0)) / 50 : y > h ? railX(h) + (y - h) * (railX(h) - railX(h - 50)) / 50 : railX(y);
  for (let y = -1000; y <= h + 1000; y += 100) rail.push([onLine(y), y]);
  for (let r = 0; r < rows; r++) {
    const y = (r + 0.5) * cell, rx = railX(y);
    for (let c = 0; c < cols; c++) {
      const x = (c + 0.5) * cell, d = Math.abs(x - rx), i = at(c, r);
      if (d < 12.5 && made.has(f.ground[i])) { f.ground[i] = ROUGH; crop[i] = 0; }
    }
  }

  // A copse or two out on the flanks, the steppe's only trees away from the farm.
  for (let k = 0; k < 2; k++) {
    const cx = rnd() < 0.5 ? w * (0.04 + rnd() * 0.1) : w * (0.74 + rnd() * 0.12), cy = h * (0.15 + rnd() * 0.7), rr = 35 + rnd() * 30;
    for (let i = 0; i < f.ground.length; i++) {
      const [x, y] = mid(i);
      if (Math.hypot(x - cx, y - cy) < rr && made.has(f.ground[i]) && crop[i] !== 0) { f.ground[i] = WOODS; crop[i] = 0; }
    }
  }

  // Works dug into ground that is no longer there are dug into what is.
  if (f.dugInto) for (const k of Object.keys(f.dugInto)) {
    const i = Number(k);
    if (f.ground[i] === WORKS && made.has(f.dugInto[i])) f.dugInto[i] = crop[i] ? (crop[i] <= 2 ? FIELD_ : OPEN) : f.dugInto[i];
  }
  return { crops: CROPS, crop: crop.join(''), fields: { turn: r3(turn), cx: w / 2, cy: h / 2, xs: xs.map(r1), ys: ys.map(r1), shift: shift.map(r1), patch: patchOut }, balka: { line: line.map(([x, y]) => [r1(x), r1(y)]), deep }, rail: { line: rail.map(([x, y]) => [r1(x), r1(y)]), bank }, hill: { ...hill, x: r1(hill.x), y: r1(hill.y) } };
}

/** Fights the set piece with one seed and writes it as the scene reads it. */
function fight(p: SetPiece, seed: number): Fought {
  const ticket = ticketOf(p, seed);
  const sim = new LandBattle(ticket);
  // The steppe laid over the rules' field before a shot is fired, and every block's cover read again from it.
  const steppe = p.open === 'steppe' ? openSteppe(sim.field, seed) : null;
  if (steppe) for (const u of sim.units) u.cover = groundAt(sim.field, u.x, u.y);
  // A tank's shells by the model it is drawn with, as battleStart sets them (looks only: nothing in the fight reads cal).
  for (const u of sim.units) if (u.kind === 'arm') { const c = calFor('arm', u.nat, ticket.turn); if (c) u.cal = c; }
  // Full sections: every man of a section, crew and team drawn and falling on his own (looks only, ruling 217).
  const drill = sim.drillOn({ sections: true, ...(p.drill ?? {}) });
  const f = sim.field;

  // What the scene draws each block with (looks only): its soldiers' file, its tank or gun model.
  const tanksSeen: Record<string, number> = { att: 0, def: 0 };
  const blocks = sim.units.map((u: LandUnit) => {
    const a = u.side === 'att' ? p.att : p.def;
    let model: string | null = null;
    if (u.kind === 'arm') model = a.tanks[Math.min(a.tanks.length - 1, tanksSeen[u.side]++)];
    else if (u.kind === 'art' && !u.team) model = a.gun;
    return {
      id: u.id, side: u.side, nat: u.nat, kind: u.kind, name: u.name, short: u.short, parent: u.parent, corps: u.corps,
      figures0: u.figures0, tray: u.tray, w: u.w, d: u.d, ...(u.cal ? { cal: u.cal } : {}), ...(u.team ? { team: u.team } : {}),
      ...(u.crew ? { crew: u.crew } : {}), model, army: a.army, ...(a.standIn ? { standIn: a.standIn } : {}),
    };
  });

  const views: Record<string, unknown>[] = [];
  const events: unknown[] = [];
  const rounds = new Map<number, Round & { n: number }>();
  const keeper = new TrackKeeper();
  const phases = new Map<number, [number, string, number | null, number | null][]>();
  const fos = new Map<number, number>();
  let lastView = -1;

  const look = (): void => {
    const t = sim.t;
    const men = drill.view() as MenBlock[];
    const seen = new Set<string>();
    for (const b of men) {
      for (const m of b.men) { keeper.see(t, b.id, m, false); seen.add(keyOf(b.id, m, false)); }
      for (const m of b.fo ?? []) { keeper.see(t, b.id, m, true); seen.add(keyOf(b.id, m, true)); fos.set(b.id, 1); }
      const ph = phases.get(b.id) ?? [];
      const last = ph[ph.length - 1];
      const now: [number, string, number | null, number | null] = [r2(t), b.ph, b.foWith ?? null, b.foOn ?? null];
      if (!last || last[1] !== now[1] || last[2] !== now[2] || last[3] !== now[3]) ph.push(now);
      phases.set(b.id, ph);
    }
    keeper.gone(t, seen);
    for (const r of drill.rounds()) if (!rounds.has(r.n)) rounds.set(r.n, r);
    const v = sim.view();
    for (const e of v.events) events.push(e);
    // The blocks every battle second (and the last view), for the screen's words, bars and the camera's choice.
    if (Math.floor(t) !== lastView || v.over) {
      lastView = Math.floor(t);
      views.push({
        t: r2(t), units: v.units.map((u) => unitOut(u, v.t)), balance: v.balance, zones: v.zones,
        ...(v.breaking ? { breaking: { att: r3(v.breaking.att), def: r3(v.breaking.def) } } : {}),
        ...(v.contact !== undefined ? { contact: v.contact } : {}),
        ...(v.plan ? { plan: planOut(v.plan) } : {}),
        ...(v.over ? { over: v.over } : {}),
      });
    }
  };

  look();
  let guard = 0;
  while (!sim.over && guard++ < 20000) { sim.step(); look(); }
  keeper.close();

  const over = sim.over!;
  const tracks = [...keeper.tracks.values()];
  const kills = [...rounds.values()].filter((r) => r.r === 'kill');
  const tankKills = kills.filter((r) => blocks.find((b) => b.id === r.on)?.kind === 'arm').length;
  const menDown = tracks.filter((tr) => tr.d !== undefined && tr.r !== 'tank' && tr.r !== 'gun' && tr.r !== 'lorry').length;
  const contact = sim.contactAt ?? 999;
  // The pick: a fight with tanks lost on both sides, men falling, contact inside a minute, and not too short or too long.
  const tanksLostBy = (side: string): number => tracks.filter((tr) => tr.r === 'tank' && tr.d !== undefined && blocks.find((b) => b.id === tr.b)?.side === side).length;
  const tAtt = tanksLostBy('att'), tDef = tanksLostBy('def');
  const len = sim.t;
  const cap = p.caps ?? { att: 4, def: 2, men: 60, len: [150, 330] };
  const score = Math.min(tAtt, cap.att) * 2.5 + Math.min(tDef, cap.def) * 2 + Math.min(menDown, cap.men) / 8 - Math.max(0, contact - 100) / 10
    - Math.max(0, cap.len[0] - len) / 20 - Math.max(0, len - cap.len[1]) / 20;
  const why = `seed ${seed}: ${r1(len)} s, contact ${r1(contact)} s, men down ${menDown}, tanks lost ${tAtt} ${p.att.adj} and ${tDef} ${p.def.adj}, `
    + `${rounds.size} rounds (${kills.length} kills, ${tankKills} on tanks), ${tracks.length} tracks, winner ${over.R > 1 ? 'attack' : 'defence'} (R ${r2(over.R)})`;

  const out = {
    what: 'RECORDING: a land battle fought by the computer on both sides for the 3D scene to replay (Tools/land_record). Not a booked result: no campaign reads it. Men are tracks (kept points [t, x, y, f] and changes [t, pose, cover, aimX, aimY, hidden, buttoned]; ld: [from, to] spans he carries the platoon cam mark; cl: his radio-call stamps), each named by block b, figure k and place in its section j (no j: the key man); rounds (j the shooter, oj the man struck; g a tank machine gun, cm: 1 a commander struck in his hatch, fate a tank kill) and events each once; blocks every battle second.',
    recording: true, key: p.key, seed, dt: DT, title: p.title, date: p.date, turn: p.turn,
    sky: { ...p.sky, latLon: p.latLon, hour: p.hour },
    sides: {
      att: { nat: p.att.nat, adj: p.att.adj, color: p.att.color, army: p.att.army, ...(p.att.standIn ? { standIn: p.att.standIn } : {}) },
      def: { nat: p.def.nat, adj: p.def.adj, color: p.def.color, army: p.def.army, ...(p.def.standIn ? { standIn: p.def.standIn } : {}) },
    },
    field: {
      w: f.w, h: f.h, cell: f.cell, cols: f.cols, rows: f.rows, grounds: GROUNDS, ground: f.ground, height: f.height.map((x: number) => r1(x)),
      zones: f.zones, town: f.town, place: ticket.place,
      ...(f.storeys ? { storeys: f.storeys } : {}), ...(f.landmark ? { landmark: f.landmark } : {}), ...(f.stream ? { stream: f.stream } : {}),
      ...(steppe ? { steppe } : {}),
    },
    limit: LAND.limit, contact: sim.contactAt, end: r2(sim.t),
    blocks,
    phases: [...phases.entries()].map(([b, ph]) => ({ b, ph })),
    views, events,
    rounds: [...rounds.values()].sort((a, b) => a.n - b.n),
    men: tracks,
    over,
  };
  return { seed, out, score, why };
}

function main(): void {
  const [folder = '.', key = 'normandy-1944', seedArg, name] = process.argv.slice(2);
  const p = PIECES.find((x) => x.key === key);
  if (!p) throw new Error(`no set piece ${key}`);
  mkdirSync(folder, { recursive: true });
  const seeds = seedArg ? [Number(seedArg)] : p.seeds;
  let best: Fought | null = null;
  for (const s of seeds) {
    const f = fight(p, s);
    console.log(`${f.why}; score ${r1(f.score)}`);
    if (!best || f.score > best.score) best = f;
  }
  const path = join(folder, `${name ?? p.key}.json`);
  const text = JSON.stringify(best!.out);
  writeFileSync(path, text);
  console.log(`kept ${best!.why}\nwrote ${path} (${(text.length / 1e6).toFixed(2)} MB)`);
}

main();
