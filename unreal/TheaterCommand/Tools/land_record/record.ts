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
// Platoons (Brian, 22 November: "I want a lot of troops in the ground battles"): with { platoons: true } each rifle
// figure is its whole platoon, its other squads and headquarters numbered on in j after the key man's squad.

import { mkdirSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import type { FieldTicket, ForceTicket, SideTicket } from '../../../../src/battle/core/types';
import { DT } from '../../../../src/battle/core/types';
import { GROUNDS, type LandPlace } from '../../../../src/battle/land/field';
import { LandBattle } from '../../../../src/battle/land/sim';
import { withTeams } from '../../../../src/battle/land/sample';
import { LAND } from '../../../../src/battle/land/rules';
import { calFor } from '../../../../src/battle/land/shells';
import type { LandUnit } from '../../../../src/battle/land/units';
import type { MenOut, MenBlock, Round } from '../../../../src/battle/land/drill';
import { unitOut, planOut } from '../../Rules/battle-bridge';

/** A kept point may be left out where the straight line from the last kept one misses the man by less than this. */
const KEEP_M = 0.06;
const KEEP_F = 0.03;

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
  /** The month's weather for the scene's sky: cloud cover 0..1 and its base in metres (looks only). */
  sky: { cover: number; base: number; kind: 'cumulus' | 'deck' };
  /** The place on the earth, for the sun (latitude, longitude) and the hour of the attack (local, 24 h). */
  latLon: [number, number];
  hour: number;
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
};

const PIECES: SetPiece[] = [NORMANDY];

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
}

/** Angle difference the short way round. */
const turn = (a: number, b: number): number => { let d = b - a; while (d > Math.PI) d -= 2 * Math.PI; while (d < -Math.PI) d += 2 * Math.PI; return d; };

/** A man's name in the keeper: block, spotter or not, figure, and his place in its section. */
const keyOf = (b: number, m: MenOut, fo: boolean): string => `${b}:${fo ? 'fo' : ''}${m.k}${m.j !== undefined ? `.${m.j}` : ''}`;

class TrackKeeper {
  tracks = new Map<string, Track>();
  /** Points seen since the last kept one, for the line test. */
  pending = new Map<string, number[][]>();

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

/** Fights the set piece with one seed and writes it as the scene reads it. */
function fight(p: SetPiece, seed: number): Fought {
  const ticket = ticketOf(p, seed);
  const sim = new LandBattle(ticket);
  // A tank's shells by the model it is drawn with, as battleStart sets them (looks only: nothing in the fight reads cal).
  for (const u of sim.units) if (u.kind === 'arm') { const c = calFor('arm', u.nat, ticket.turn); if (c) u.cal = c; }
  // Full sections: every man of a section, crew and team drawn and falling on his own (looks only, ruling 217).
  const drill = sim.drillOn({ sections: true, platoons: true });
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
  const score = Math.min(tAtt, 4) * 2.5 + Math.min(tDef, 2) * 2 + Math.min(menDown, 60) / 8 - Math.max(0, contact - 100) / 10
    - Math.max(0, 150 - len) / 20 - Math.max(0, len - 330) / 20;
  const why = `seed ${seed}: ${r1(len)} s, contact ${r1(contact)} s, men down ${menDown}, tanks lost ${tAtt} American and ${tDef} German, `
    + `${rounds.size} rounds (${kills.length} kills, ${tankKills} on tanks), ${tracks.length} tracks, winner ${over.R > 1 ? 'attack' : 'defence'} (R ${r2(over.R)})`;

  const out = {
    what: 'RECORDING: a land battle fought by the computer on both sides for the 3D scene to replay (Tools/land_record). Not a booked result: no campaign reads it. Men are tracks (kept points [t, x, y, f] and changes [t, pose, cover, aimX, aimY, hidden, buttoned]), each named by block b, figure k and place in its section j (no j: the key man); rounds (j the shooter, oj the man struck) and events each once; blocks every battle second.',
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
  const [folder = '.', key = 'normandy-1944', seedArg] = process.argv.slice(2);
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
  const path = join(folder, `${p.key}.json`);
  const text = JSON.stringify(best!.out);
  writeFileSync(path, text);
  console.log(`kept ${best!.why}\nwrote ${path} (${(text.length / 1e6).toFixed(2)} MB)`);
}

main();
