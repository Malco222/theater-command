// Only anti-tank weapons take a tank (release 2, a real rule; Brian, 25 November 2026, 15:58Z: "are bullets taking out
// german tanks? cannon fire or bazookas should be the only think taking them out"; the design director's pierce ruling,
// 25 Nov, P1 to P9; LandBattle.pierce, LAND.pierce).
//
// What must hold. With the switch off, every battle is as it was before the rule to the last figure, whatever kit its
// ticket carries, and nothing of the rule runs. With it on: a blow on a tank block is the plain blow times the striker's
// pierce share, a pure function of the two blocks, the gap between them and the ground; a machine-gun team takes
// nothing of a tank's strength; each thing a block on foot carries works to its own reach and not beyond; what a foot
// block or a support team could not put into the tank lands on the nearest enemy block that is not a tank, at the same
// luck (split fire), so the block's output is kept wherever there is anything else to hit; the dice are drawn once a
// blow, in the same order, whatever the share; the overrun's threat follows the kit; and nothing reads an army's name
// or the date.
import { describe, expect, it } from 'vitest';
import { LAND, LandBattle, PRACTICE_AIR, PRACTICE_AIR_DATE, PRACTICE_GROUNDS, SAMPLE_KIT, landTicket, practiceTicket, reachOf, sampleTicket, withTeams, type LandUnit } from '../src/battle/land';
import { GROUNDS, type Ground, type LandPlace } from '../src/battle/land/field';
import { LiveKeeper, livePiece, type LiveDelta } from '../src/battle/land/live';
import { runOut } from '../src/battle/core';
import type { FieldTicket, ForceKit, ForceTicket } from '../src/battle/core/types';
import { hashString } from '../src/core/rng';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { SUPPORT } from '../src/campaign/support';
import { beginHuman, candidates, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** Every test here may fight whole battles on a busy machine. */
const LONG = 300_000;

// (the rule's own methods and the battle's dice are private: the tests reach them by name)
// eslint-disable-next-line @typescript-eslint/no-explicit-any
type Sim = any;
type Ticket = FieldTicket<LandPlace>;
type SampleOpts = NonNullable<Parameters<typeof sampleTicket>[2]>;
interface Blow { dmg: number; nerve: number; front: boolean; aspect: string; ground: number; screen: number; rest?: { roll: number; part: number } }

const P = LAND.pierce;
const FULL_KIT: ForceKit = { atGuns: true, atTd: true, atHand: true };
const seedOf = (s: number): number => s * 7919 + 13;

/** A whole battle, to the last figure: how it ended, every block, and everything it said. */
const sig = (b: Sim): string => JSON.stringify({
  over: b.over, t: b.t,
  units: b.units.map((u: LandUnit) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged ?? null]),
  log: b.log, events: b.events,
});
const digest = (s: string): string => hashString(s).toString(16).padStart(8, '0') + hashString(`#${s}`).toString(16).padStart(8, '0');
const line = (b: Sim): string => `${b.over!.R.toFixed(4)} ${b.over!.seconds} ${digest(sig(b))}`;
/** The same, with no words: the figures alone (the words name the armies). */
const figures = (b: Sim): string => JSON.stringify({
  winner: b.over!.winner, how: b.over!.how, R: b.over!.R, seconds: b.over!.seconds, lost: b.over!.lost, t: b.t,
  units: b.units.map((u: LandUnit) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged ?? null]),
});

/** The sample ticket mirrored: the two make-ups swapped (kit goes with the corps), the attack's forces not dug in. */
const mirror = (t: Ticket): Ticket => ({ ...t,
  att: { ...t.att, forces: t.def.forces.map((f) => ({ ...f, nat: t.att.nat, dug: false })) },
  def: { ...t.def, forces: t.att.forces.map((f) => ({ ...f, nat: t.def.nat })) } });
/** The switches the 3D game ships on, set by hand; and every switch there is but this rule's. */
const ship = (b: Sim): void => { b.hideOn(); b.measured = true; b.cutters = true; b.phones = true; b.patrol = true; b.escort = true; };
const every = (b: Sim): void => { ship(b); b.smoke = true; b.bearers = true; };

function fight(t: Ticket, set?: (b: Sim) => void): Sim {
  const b = new LandBattle(t) as Sim;
  b.ai.att = b.ai.def = true;
  set?.(b);
  runOut(b);
  return b;
}
/** A set piece as the game starts it, the drill removed (looks only), fought to its end. */
function piece(key: string, seed: number, opts: Record<string, unknown> = {}, set?: (b: Sim) => void): Sim {
  const b = livePiece(key, seed, { me: null, platoons: false, ...opts }).sim as Sim;
  b.drill = null;
  set?.(b);
  while (!b.over) b.step();
  return b;
}
const blocks = (b: Sim): LandUnit[] => b.allBlocks();
const forces = (t: Ticket): ForceTicket[] => [...t.att.forces, ...t.def.forces];
const kitOf = (f: ForceTicket): ForceKit | null => f.kit ?? null;

// ---- Off ------------------------------------------------------------------------------------------------------------

describe('with the switch off, every battle is as before the rule', () => {
  // Each battle's result and a digest of its whole signature, taken from the code before this rule (commit a44404f,
  // "wiring: the pierce switch"), where no ticket carried any of the rule's kit. They hold with the kit on every
  // sample ticket and set piece. A later rule that moves these battles on purpose pins them again, knowingly: the
  // failing line prints the new value.
  const s = (ground: Parameters<typeof sampleTicket>[0], odds: number, n: number, o: SampleOpts = {}): Ticket => sampleTicket(ground, odds, { seed: seedOf(n), ...o });
  const BEFORE: [string, string, () => Sim][] = [
    ['bare plains 1.2', '1.1080 254 59c17af6b382d41b', () => fight(s('plains', 1.2, 1))],
    ['bare forest 1.4', '1.6230 342 ac8dec03dcc2197c', () => fight(s('forest', 1.4, 2))],
    ['bare hills 1.0', '0.9560 245 52be49736c4b87e6', () => fight(s('hills', 1.0, 1))],
    ['bare mountain 1.2', '1.4400 333 6df2ae83cf16dff2', () => fight(s('mountain', 1.2, 2))],
    ['bare marsh 1.4', '1.1540 336 daf7d0449829150b', () => fight(s('marsh', 1.4, 1))],
    ['bare urban 1.2', '1.1150 332 84d7693e67c97ffb', () => fight(s('urban', 1.2, 1))],
    ['bare desert 1.0', '1.0660 232 d4fac384c5af95f5', () => fight(s('desert', 1.0, 2))],
    ['with teams, hills', '1.0810 284 25142adc39999405', () => fight(withTeams(s('hills', 1.2, 1)))],
    ['the shipping switches, plains', '1.4400 335 b7af5c59ba684f52', () => fight(withTeams(s('plains', 1.2, 1)), ship)],
    ['the shipping switches, forest mirrored', '1.1900 360 b8ef158f0f7a6320', () => fight(withTeams(mirror(s('forest', 1.2, 2))), ship)],
    ['the shipping switches, urban mirrored', '1.1900 376 3816bcdb1e66ec8e', () => fight(withTeams(mirror(s('urban', 1.2, 1))), ship)],
    ['every switch, plains', '1.4400 359 9ae918b92a6b56ae', () => fight(withTeams(s('plains', 1.2, 3, { nat: { att: 'USA', def: 'GER' } })), every)],
    ['a river', '1.1750 262 97a868a364a0749e', () => fight(s('plains', 1.3, 5, { river: true }))],
    ['fortress works', '1.1560 272 3697a252ce00bbdb', () => fight(s('hills', 1.4, 6, { fort: 2 }))],
    ['dug in', '1.5600 264 281bb60037233ea9', () => fight(s('forest', 1.3, 7, { dug: true }))],
    ['a capital', '1.5600 317 81bd2cdf73ec040c', () => fight(s('urban', 1.3, 8, { town: 'capital' }))],
    ['practice ground 0', '1.5520 254 d8a39610e8e280a1', () => fight(practiceTicket(PRACTICE_GROUNDS[0], { seed: seedOf(9) }))],
    ['practice ground 8', '1.4950 274 38d8f2a9942b08c4', () => fight(practiceTicket(PRACTICE_GROUNDS[8], { seed: seedOf(17) }))],
    ['normandy-1944 seed 11', '1.4400 285 628806926fd48ac1', () => piece('normandy-1944', 11)],
    ['prokhorovka-1943 seed 5', '1.0400 218 c16583e39332f76c', () => piece('prokhorovka-1943', 5)],
    ['normandy-1944 seed 7, infantry with tanks', '0.9850 261 0364036db9f9268e', () => piece('normandy-1944', 7, { escort: true })],
  ];

  it('to the last figure, on a spread of sample tickets, practice grounds and set pieces, bare and with the other switches on', () => {
    const now = BEFORE.map(([name, , make]) => `${name}: ${line(make())}`);
    expect(now).toEqual(BEFORE.map(([name, was]) => `${name}: ${was}`));
  }, LONG);

  it('nothing of the rule runs: no share is reckoned, no fire is split, no kit is asked for the overrun', () => {
    const spied = (b: Sim): void => {
      const calls: string[] = [];
      for (const name of ['pierceShare', 'splitFire', 'kitReach']) b[name] = () => { calls.push(name); throw new Error(`${name} called with the switch off`); };
      const blow = b.blow.bind(b);
      b.blow = (u: LandUnit, t: LandUnit, spill?: unknown): Blow => { const r = blow(u, t, spill); if (spill !== undefined || r.rest) calls.push('a split blow'); return r; };
      b.spied = calls;
    };
    for (const b of [
      fight(withTeams(mirror(sampleTicket('forest', 1.2, { seed: seedOf(2) }))), (x) => { ship(x); spied(x); }),
      fight(withTeams(sampleTicket('plains', 1.2, { seed: seedOf(3), kit: { att: FULL_KIT, def: {} } })), (x) => { every(x); spied(x); }),
      piece('prokhorovka-1943', 17, { escort: true }, spied),
    ]) {
      expect(b.pierce).toBe(false);
      expect(b.spied).toEqual([]);
      expect(b.units.some((u: LandUnit) => u.kind === 'arm' && u.str < u.str0)).toBe(true);
    }
  }, LONG);

  it('the kit on a ticket changes nothing: plain, the sample kit and the full kit fight the same battle, and no kit field is read', () => {
    for (const [ground, n, mir] of [['plains', 1, false], ['forest', 2, true]] as const) {
      const run = (kit: SampleOpts['kit'], reads?: { n: number }): Sim => {
        const base = sampleTicket(ground, 1.2, { seed: seedOf(n), ...(kit ? { kit } : {}) });
        return fight(withTeams(mir ? mirror(base) : base), (b) => { ship(b); if (reads) counted(b, reads); });
      };
      const reads = { n: 0 };
      const sample = run(undefined, reads), plain = run({ att: {}, def: {} }), full = run({ att: FULL_KIT, def: FULL_KIT }), mixed = run({ att: {}, def: FULL_KIT });
      // (the battle with the sample kit read none of its kit; asked first, since the lines below read it themselves)
      expect(reads.n).toBe(0);
      expect(forces(sample.ticket).filter((f) => f.kit?.atGuns).length).toBeGreaterThan(0);
      expect(forces(plain.ticket).every((f) => f.kit === undefined)).toBe(true);
      expect(sig(plain)).toBe(sig(sample));
      expect(sig(full)).toBe(sig(sample));
      expect(sig(mixed)).toBe(sig(sample));
    }
    // (the count bites: with the switch on the same battle reads its kit)
    const reads = { n: 0 };
    fight(withTeams(sampleTicket('plains', 1.2, { seed: seedOf(1) })), (b) => { b.pierce = true; counted(b, reads); });
    expect(reads.n).toBeGreaterThan(0);
    // A set piece's kit, taken off every block before the first tick: the same battle.
    const strip = (b: Sim): void => { for (const u of blocks(b)) if (u.kit) for (const k of ['atGuns', 'atTd', 'atHand'] as const) delete u.kit[k]; };
    for (const [key, seed] of [['normandy-1944', 7], ['prokhorovka-1943', 13]] as const) {
      const as = piece(key, seed, { escort: true }), bare = piece(key, seed, { escort: true }, strip);
      expect(blocks(as).some((u) => u.kit?.atGuns)).toBe(true);
      expect(blocks(bare).some((u) => u.kit?.atGuns || u.kit?.atTd || u.kit?.atHand)).toBe(false);
      expect(sig(bare)).toBe(sig(as));
    }
  }, LONG);
});

/** Counts every read of the rule's three kit fields on a battle's blocks. */
function counted(b: Sim, reads: { n: number }): void {
  const seen = new Set<ForceKit>();
  for (const u of blocks(b)) {
    if (!u.kit || seen.has(u.kit)) continue;
    seen.add(u.kit);
    for (const k of ['atGuns', 'atTd', 'atHand'] as const) {
      const v = u.kit[k];
      Object.defineProperty(u.kit, k, { get: () => { reads.n++; return v; }, enumerable: true, configurable: true });
    }
  }
}

// ---- The share ---------------------------------------------------------------------------------------------------------

interface Stage { b: Sim; foot: LandUnit; gun: LandUnit; mg: LandUnit; mor: LandUnit; own: LandUnit; tank: LandUnit; men: LandUnit[] }

/**
 * A plains field where both sides bring tanks, with the teams: the defence strikes (a block on foot, a field gun, a
 * machine-gun team, a mortar team, a tank block of its own) and the attack is struck (a tank block, and its blocks on
 * foot). Stages made alike stand alike and hold the same dice.
 */
function stage(pierce: boolean, o: SampleOpts = {}): Stage {
  const t = sampleTicket('plains', 1.2, { seed: seedOf(1), ...o });
  const f = t.def.forces;
  f[0].share = 0.4; f[1].share = 0.3; f[2].share = 0.1;
  f.push({ ...t.att.forces[2], corps: 14, nat: t.def.nat, share: 0.2 });
  const b = new LandBattle(withTeams(t)) as Sim;
  b.pierce = pierce;
  const of = (side: string, kind: string, team?: string): LandUnit[] => b.units.filter((u: LandUnit) => u.side === side && u.kind === kind && u.team === team);
  const st = { b, foot: of('def', 'inf')[0], gun: of('def', 'art')[0], mg: of('def', 'art', 'mg')[0], mor: of('def', 'art', 'mor')[0], own: of('def', 'arm')[0], tank: of('att', 'arm')[0], men: of('att', 'inf') };
  for (const u of [st.foot, st.gun, st.mg, st.mor, st.own, st.tank, st.men[0], st.men[1]]) expect(u).toBeDefined();
  return st;
}
const SPOT: [number, number] = [1500, 900];
const edges = (u: LandUnit, t: LandUnit): number => (Math.max(u.w, u.d) + Math.max(t.w, t.d)) / 4;
const paint = (b: Sim, x: number, y: number, g: Ground): void => { const f = b.field; f.ground[Math.floor(y / f.cell) * f.cols + Math.floor(x / f.cell)] = GROUNDS.indexOf(g); };
/** Stands the target at the spot and the striker 'gap' metres off it to the north, edge to edge, each on the ground named (open where not). */
function face(b: Sim, u: LandUnit, t: LandUnit, gap: number, on: { tank?: Ground; striker?: Ground } = {}): void {
  t.x = SPOT[0]; t.y = SPOT[1];
  u.x = SPOT[0]; u.y = SPOT[1] + gap + edges(u, t);
  paint(b, t.x, t.y, on.tank ?? 'open');
  paint(b, u.x, u.y, on.striker ?? 'open');
  expect(b.gap(u, t)).toBeCloseTo(gap, 9);
}
/** Stands a block 'gap' metres south of the striker, edge to edge: on the same bearing as the tank it faces. */
const south = (u: LandUnit, e: LandUnit, gap: number): void => { e.x = u.x; e.y = u.y - gap - edges(u, e); };

describe('the pierce share (P1, P2)', () => {
  it('the numbers stand inside the ranges the ruling gave, and the fixed ones as fixed', () => {
    const inside: [string, number, number, number][] = [
      ['guns.with', P.guns.with, 0.4, 0.6], ['guns.without', P.guns.without, 0.15, 0.35], ['guns.destroyers', P.guns.destroyers, 0, 0.15], ['guns.most', P.guns.most, 0.55, 0.65],
      ['hand.open', P.hand.open, 0.15, 0.25], ['hand.close', P.hand.close, 0.3, 0.4], ['hand.at', P.hand.at, 80, 120],
      ['close.open', P.close.open, 0.1, 0.2], ['close.close', P.close.close, 0.2, 0.3], ['close.at', P.close.at, 25, 40],
      ['eng.open', P.eng.open, 0.25, 0.35], ['eng.close', P.eng.close, 0.4, 0.6], ['teams.mor', P.teams.mor, 0, 0.1], ['fort', P.fort, 0.5, 0.8],
    ];
    for (const [name, v, lo, hi] of inside) { expect(v, name).toBeGreaterThanOrEqual(lo); expect(v, name).toBeLessThanOrEqual(hi); }
    expect(P.most).toBe(1);
    expect(P.teams.mg).toBe(0);
    expect([...P.foot].sort()).toEqual(['cav', 'eng', 'inf', 'mot', 'mtn']);
    expect([...P.ground].sort()).toEqual(['buildings', 'industry', 'rough', 'suburb', 'woods']);
    expect([...P.works].sort()).toEqual(['bunker', 'works']);
    expect(Object.keys(P).sort()).toEqual(['close', 'eng', 'foot', 'fort', 'ground', 'guns', 'hand', 'most', 'teams', 'works']);
    // The numbers the rule must leave alone: off keeps the teams' own figures on armour.
    expect(LAND.teams.mg.armour).toBe(0.15);
    expect(LAND.teams.mor.armour).toBe(0.5);
    expect(SAMPLE_KIT).toEqual({ atGuns: true });
  }, LONG);

  it('a block on foot: its own guns across its reach, launchers to hand.at, close assault to close.at, and none of them beyond', () => {
    const { b, foot, tank } = stage(true);
    const at = (kit: ForceKit | undefined, gap: number, on: { tank?: Ground; striker?: Ground } = {}): number => { foot.kit = kit; face(b, foot, tank, gap, on); return b.pierceShare(foot, tank); };
    const reach = reachOf(foot), far = P.hand.at + 1;
    expect(reach).toBeGreaterThan(far);
    // Its own anti-tank guns, anywhere in its reach: the early guns and anti-tank rifles with the plain kit.
    expect(at(undefined, far)).toBe(P.guns.without);
    expect(at({}, reach)).toBe(P.guns.without);
    expect(at({}, reach + 1)).toBe(0);
    expect(at({ atGuns: true }, far)).toBe(P.guns.with);
    expect(at({ atGuns: true }, reach)).toBe(P.guns.with);
    expect(at({ atGuns: true }, reach + 1)).toBe(0);
    expect(at({ atTd: true }, far)).toBeCloseTo(P.guns.without + P.guns.destroyers, 12);
    expect(at({ atGuns: true, atTd: true }, far)).toBeCloseTo(Math.min(P.guns.most, P.guns.with + P.guns.destroyers), 12);
    expect(at(FULL_KIT, far)).toBeLessThanOrEqual(P.guns.most);
    // (The measure may leave the tank destroyers' number at the bottom of its range, nothing: the field is read all the
    // same, shown here with the number raised for these lines alone, and it too ends at the block's reach.)
    const guns = P.guns as { destroyers: number }, td = guns.destroyers;
    try {
      guns.destroyers = 0.1;
      expect(at({ atTd: true }, far)).toBeCloseTo(P.guns.without + 0.1, 12);
      expect(at({ atTd: true }, reach + 1)).toBe(0);
      expect(at({ atGuns: true, atTd: true }, reach)).toBeCloseTo(Math.min(P.guns.most, P.guns.with + 0.1), 12);
      expect(at({ atGuns: true, atTd: true }, reach + 1)).toBe(0);
      expect(at({ atGuns: true }, far)).toBe(P.guns.with);
      guns.destroyers = 0.4;
      expect(at({ atGuns: true, atTd: true }, far)).toBe(P.guns.most);
    } finally { guns.destroyers = td; }
    expect(P.guns.destroyers).toBe(td);
    // The overrun's own field is not this rule's.
    expect(at({ at: true }, far)).toBe(P.guns.without);
    // Launchers: only where the kit has them, only within hand.at, by the ground.
    expect(at({ atHand: true }, P.hand.at)).toBeCloseTo(P.guns.without + P.hand.open, 12);
    expect(at({ atHand: true }, P.hand.at + 0.5)).toBe(P.guns.without);
    expect(at({ atHand: true }, P.hand.at, { tank: 'woods' })).toBeCloseTo(P.guns.without + P.hand.close, 12);
    expect(at({ atGuns: true }, P.hand.at)).toBe(P.guns.with);
    expect(at({ atGuns: true, atHand: true }, P.close.at + 1)).toBeCloseTo(P.guns.with + P.hand.open, 12);
    // Close assault: every block on foot, only within close.at, by the ground.
    expect(at({}, P.close.at)).toBeCloseTo(P.guns.without + P.close.open, 12);
    expect(at({}, P.close.at + 0.5)).toBe(P.guns.without);
    expect(at({}, 0)).toBeCloseTo(P.guns.without + P.close.open, 12);
    expect(at({}, P.close.at, { tank: 'buildings' })).toBeCloseTo(P.guns.without + P.close.close, 12);
    expect(at({ atGuns: true }, P.close.at)).toBeCloseTo(P.guns.with + P.close.open, 12);
    // Close ground: where the tank stands on woods, rough, buildings, industry or suburb, or the striker in its works.
    for (const g of GROUNDS) {
      const close = (P.ground as readonly string[]).includes(g);
      expect(at({ atHand: true }, 50, { tank: g }), `tank on ${g}`).toBeCloseTo(P.guns.without + (close ? P.hand.close : P.hand.open), 12);
      const works = (P.works as readonly string[]).includes(g);
      expect(at({ atHand: true }, 50, { striker: g }), `striker on ${g}`).toBeCloseTo(P.guns.without + (works ? P.hand.close : P.hand.open), 12);
    }
    // Never above today's weight, and never below nothing.
    expect(at(FULL_KIT, 10, { tank: 'woods' })).toBeCloseTo(Math.min(1, Math.min(P.guns.most, P.guns.with + P.guns.destroyers) + P.hand.close + P.close.close), 12);
    for (const kit of [undefined, {}, { atGuns: true }, { atHand: true }, FULL_KIT]) for (const gap of [0, 20, 30, 31, 80, 100, 101, 150, 200, 201, 400]) for (const g of ['open', 'woods'] as Ground[]) {
      const v = at(kit, gap, { tank: g, striker: g === 'woods' ? 'works' : 'open' });
      expect(v).toBeGreaterThanOrEqual(0);
      expect(v).toBeLessThanOrEqual(1);
    }
    // Every kind on foot, by its own reach; engineers' close assault is their own.
    for (const kind of P.foot) {
      foot.kind = kind;
      const r = reachOf(foot);
      expect(at({ atGuns: true }, r), kind).toBe(P.guns.with);
      expect(at({ atGuns: true }, r + 1), kind).toBe(0);
      const near = kind === 'eng' ? P.eng : P.close;
      expect(at({}, 10), kind).toBeCloseTo(P.guns.without + near.open, 12);
      expect(at({}, 10, { tank: 'rough' }), kind).toBeCloseTo(P.guns.without + near.close, 12);
    }
  }, LONG);

  it('tanks and field guns in full, a machine-gun team nothing, a mortar team and a fort their own; anything not a tank, in full', () => {
    const { b, foot, gun, mg, mor, own, tank, men } = stage(true);
    for (const gap of [10, 100, 150, 600]) {
      face(b, own, tank, gap);
      expect(b.pierceShare(own, tank)).toBe(1);
      face(b, gun, tank, gap);
      expect(b.pierceShare(gun, tank)).toBe(1);
      face(b, mg, tank, gap);
      expect(b.pierceShare(mg, tank)).toBe(0);
      face(b, mor, tank, gap, { tank: 'woods', striker: 'works' });
      expect(b.pierceShare(mor, tank)).toBe(P.teams.mor);
      // Not a tank: everyone's blow is whole.
      for (const u of [foot, gun, mg, mor, own]) { face(b, u, men[0], gap); expect(b.pierceShare(u, men[0])).toBe(1); }
    }
    // A fort's guns are in its works: its own figure across its reach, with no kit read.
    foot.kind = 'fort'; foot.kit = undefined;
    const r = reachOf(foot);
    face(b, foot, tank, r);
    expect(b.pierceShare(foot, tank)).toBe(P.fort);
    foot.kit = FULL_KIT;
    face(b, foot, tank, 20, { tank: 'woods' });
    expect(b.pierceShare(foot, tank)).toBe(P.fort);
    face(b, foot, tank, r + 1);
    expect(b.pierceShare(foot, tank)).toBe(0);
  }, LONG);

  it("every support team has its own share: the rule's table lists the teams LAND.teams lists, and a team it does not know takes nothing off a tank, never a blow that is not a number", () => {
    // (another branch adds a third support team: this fails until that team is given its share here, by a ruling)
    expect(Object.keys(P.teams).sort()).toEqual(Object.keys(LAND.teams).sort());
    for (const k of Object.keys(P.teams) as (keyof typeof P.teams)[]) { expect(P.teams[k], k).toBeGreaterThanOrEqual(0); expect(P.teams[k], k).toBeLessThanOrEqual(1); }
    const teams = LAND.teams as Record<string, unknown>, listed = Object.keys(LAND.teams).sort();
    // A team the rules give a row and this table does not, yet.
    teams.newteam = { ...LAND.teams.mg };
    try {
      const O = stage(false), R = stage(true);
      for (const s of [O, R]) { (s.mg as Sim).team = 'newteam'; face(s.b, s.mg, s.tank, 300); south(s.mg, s.men[0], 420); }
      expect(R.b.pierceShare(R.mg, R.tank)).toBe(0);
      const plain: Blow = O.b.blow(O.mg, O.tank), got: Blow = R.b.blow(R.mg, R.tank);
      expect(R.b.rng.s).toBe(O.b.rng.s);
      expect(Number.isFinite(plain.dmg)).toBe(true);
      expect(got.dmg).toBe(0);
      expect(Number.isFinite(got.nerve)).toBe(true);
      expect(got.rest!.part).toBe(1);
      // All of it goes to the men with her, a whole blow and a number.
      const hurt = new Map<number, { dmg: number; nerve: number; from: [LandUnit, number][]; blind: boolean }>();
      R.b.splitFire(R.mg, got.rest!, hurt, new Map<number, number>(), new Set<number>());
      const h = [...hurt.values()];
      expect(h.length).toBe(1);
      expect(h[0].dmg).toBeGreaterThan(0);
      expect(Number.isFinite(h[0].dmg) && Number.isFinite(h[0].nerve)).toBe(true);
    } finally { delete teams.newteam; }
    expect(Object.keys(LAND.teams).sort()).toEqual(listed);
  }, LONG);

  it('pure: the two blocks, their gap and the ground, and nothing else (no name, no date, no strength, no dice)', () => {
    const { b, foot, tank } = stage(true);
    foot.kit = { atGuns: true, atHand: true };
    face(b, foot, tank, 60, { tank: 'suburb' });
    const was = b.pierceShare(foot, tank), dice = b.rng.s;
    expect(was).toBeCloseTo(P.guns.with + P.hand.close, 12);
    foot.nat = 'GER'; tank.nat = 'SOV'; b.ticket.att.nat = 'SOV'; b.ticket.def.nat = 'GER'; b.ticket.turn = 58; b.ticket.date = 'June 1944';
    foot.str *= 0.3; foot.mor = 0.1; tank.str *= 0.5; foot.xp = 1; foot.state = 'wavering'; b.t = 200;
    expect(b.pierceShare(foot, tank)).toBe(was);
    // The same answer from the places handed to it as from the blocks' own.
    expect(b.pierceShare(foot, tank, 60, [foot.x, foot.y], [tank.x, tank.y])).toBe(was);
    expect(b.pierceShare(foot, tank, P.hand.at + 1, [foot.x, foot.y], [tank.x, tank.y])).toBe(P.guns.with);
    expect(b.rng.s).toBe(dice);
  }, LONG);
});

// ---- The blow -------------------------------------------------------------------------------------------------------

describe('a blow on a tank block (P1)', () => {
  it('is the plain blow times its share, at the same luck and with one draw of the dice, whatever the share', () => {
    const A = stage(false), B = stage(true);
    let draws = 0;
    const next = B.b.rng.next.bind(B.b.rng);
    B.b.rng.next = (): number => { draws++; return next(); };
    const pair = (who: (s: Stage) => [LandUnit, LandUnit], gap: number, on: { tank?: Ground; striker?: Ground } = {}, kit?: ForceKit): { plain: Blow; got: Blow; share: number } => {
      for (const s of [A, B]) { const [u, t] = who(s); if (kit) u.kit = { ...kit }; face(s.b, u, t, gap, on); }
      const [ua, ta] = who(A), [ub, tb] = who(B), d0 = draws;
      const plain: Blow = A.b.blow(ua, ta), got: Blow = B.b.blow(ub, tb);
      // One draw each, so the two fields' dice stay in step.
      expect(draws - d0).toBe(1);
      expect(B.b.rng.s).toBe(A.b.rng.s);
      expect(plain.rest).toBeUndefined();
      expect(plain.dmg).toBeGreaterThan(0);
      expect([got.nerve, got.front, got.aspect, got.ground, got.screen]).toEqual([plain.nerve, plain.front, plain.aspect, plain.ground, plain.screen]);
      return { plain, got, share: B.b.pierceShare(ub, tb) };
    };
    const footOnTank = (s: Stage): [LandUnit, LandUnit] => [s.foot, s.tank];
    // A block on foot, by what it carries, at each reach and just beyond it.
    const cases: [ForceKit, number, { tank?: Ground; striker?: Ground }, number][] = [
      [{}, 150, {}, P.guns.without],
      [{}, P.close.at + 1, {}, P.guns.without],
      [{}, P.close.at, {}, P.guns.without + P.close.open],
      [{}, P.close.at, { tank: 'woods' }, P.guns.without + P.close.close],
      [{ atGuns: true }, 150, {}, P.guns.with],
      [{ atGuns: true, atTd: true }, 150, {}, Math.min(P.guns.most, P.guns.with + P.guns.destroyers)],
      [{ atHand: true }, P.hand.at + 1, {}, P.guns.without],
      [{ atHand: true }, P.hand.at, {}, P.guns.without + P.hand.open],
      [{ atHand: true }, P.hand.at, { striker: 'works' }, P.guns.without + P.hand.close],
      [FULL_KIT, 150, { tank: 'buildings' }, Math.min(P.guns.most, P.guns.with + P.guns.destroyers)],
      [FULL_KIT, 20, { tank: 'buildings' }, Math.min(1, Math.min(P.guns.most, P.guns.with + P.guns.destroyers) + P.hand.close + P.close.close)],
    ];
    for (const [kit, gap, on, want] of cases) {
      const { plain, got, share } = pair(footOnTank, gap, on, kit);
      expect(share).toBeCloseTo(want, 12);
      expect(got.dmg / plain.dmg).toBeCloseTo(share, 12);
      // What it could not put into her is the rest, at the same luck.
      if (share < 1) { expect(got.rest!.part).toBeCloseTo(1 - share, 12); expect(got.rest!.roll).toBeGreaterThanOrEqual(0); expect(got.rest!.roll).toBeLessThan(1); } else expect(got.rest).toBeUndefined();
    }
    // A machine-gun team: nothing of her strength, all of it the rest; the dice drawn all the same (above).
    for (const gap of [40, 300, 700]) {
      const { plain, got } = pair((s) => [s.mg, s.tank], gap);
      expect(plain.dmg).toBeGreaterThan(0);
      expect(got.dmg).toBe(0);
      expect(got.rest!.part).toBe(1);
    }
    // A mortar team: its own small share of the plain blow (off, LAND.teams' armour figure stands in the blow).
    const m = pair((s) => [s.mor, s.tank], 500);
    expect(m.got.dmg / (m.plain.dmg / LAND.teams.mor.armour)).toBeCloseTo(P.teams.mor, 12);
    expect(m.got.rest!.part).toBeCloseTo(1 - P.teams.mor, 12);
    // Tanks, and field guns over open sights or from far back: as ever, and nothing to split.
    for (const [who, gap] of [[(s: Stage) => [s.own, s.tank], 150], [(s: Stage) => [s.gun, s.tank], 100], [(s: Stage) => [s.gun, s.tank], 900]] as [(s: Stage) => [LandUnit, LandUnit], number][]) {
      const { plain, got } = pair(who, gap);
      expect(got.dmg).toBe(plain.dmg);
      expect(got.rest).toBeUndefined();
    }
    // A blow on anything that is not a tank: as ever.
    for (const who of [(s: Stage) => [s.foot, s.men[0]], (s: Stage) => [s.mg, s.men[0]], (s: Stage) => [s.mor, s.men[1]], (s: Stage) => [s.own, s.men[1]]] as ((s: Stage) => [LandUnit, LandUnit])[]) {
      const { plain, got } = pair(who, 120, {}, {});
      expect(got.dmg).toBe(plain.dmg);
      expect(got.rest).toBeUndefined();
    }
  }, LONG);

  it("a tank driving through a foot block is struck by what its men carry as the two stand: their guns and their close assault, never nothing", () => {
    // (the pierce ruling's P1: the share is by "their gap"; the drawing's P9 reads the same gap. Weighed from where she
    // began her drive instead, half the blows of the men she drove through put nothing into her: the fairness checker.)
    const drive = (s: Stage, from: number): void => {
      s.foot.kit = {};
      face(s.b, s.foot, s.tank, from);
      s.tank.ovr = { on: s.foot.id, to: [s.foot.x, s.foot.y + 60], back: [s.tank.x, s.tank.y], leg: 'in' };
      s.tank.x = s.foot.x; s.tank.y = s.foot.y;
      expect(s.b.gap(s.foot, s.tank)).toBe(0);
      expect(s.b.inOverrun(s.foot, s.tank)).toBe(true);
    };
    const among = P.guns.without + P.close.open;
    // From inside the men's reach, and from beyond it but inside her own (a tank's reach is the longer).
    const beyond = reachOf(stage(true).foot) + 10;
    expect(beyond).toBeLessThanOrEqual(reachOf(stage(true).tank));
    for (const from of [150, beyond]) {
      const O = stage(false), R = stage(true);
      for (const s of [O, R]) drive(s, from);
      const plain: Blow = O.b.blow(O.foot, O.tank), driven: Blow = R.b.blow(R.foot, R.tank);
      // One draw each, and the plain blow under the share is the switch-off blow of the same drive (b418: harm as from before it).
      expect(R.b.rng.s).toBe(O.b.rng.s);
      expect(plain.dmg).toBeGreaterThan(0);
      expect(R.b.pierceShare(R.foot, R.tank)).toBeCloseTo(among, 12);
      expect(driven.dmg / plain.dmg).toBeCloseTo(among, 12);
      expect(driven.rest!.part).toBeCloseTo(1 - among, 12);
      expect([driven.nerve, driven.front, driven.aspect, driven.ground, driven.screen]).toEqual([plain.nerve, plain.front, plain.aspect, plain.ground, plain.screen]);
    }
    // Standing off at those gaps, with no drive, the men's guns alone reach her, and beyond their reach they do not strike her at all.
    const S = stage(true);
    S.foot.kit = {};
    face(S.b, S.foot, S.tank, 150);
    expect((S.b.blow(S.foot, S.tank) as Blow).rest!.part).toBeCloseTo(1 - P.guns.without, 12);
    face(S.b, S.foot, S.tank, beyond);
    expect(S.b.pierceShare(S.foot, S.tank)).toBe(0);
  }, LONG);
});

// ---- Split fire -----------------------------------------------------------------------------------------------------

describe('split fire (P1, Q2)', () => {
  /** The spilled blow of the striker's first blow on the tank, through the rule's own split: who took it, and how hard. */
  const split = (s: Stage, u: LandUnit, unseen: number[] = []): { main: Blow; hurt: Map<number, { dmg: number; nerve: number; from: [LandUnit, number][]; blind: boolean }>; kept: Map<number, number> } => {
    const main: Blow = s.b.blow(u, s.tank), hurt = new Map(), kept = new Map<number, number>();
    if (main.rest) s.b.splitFire(u, main.rest, hurt, kept, new Set(unseen));
    return { main, hurt, kept };
  };
  const set = (s: Stage, kit: ForceKit, others: (s: Stage) => void): void => { s.foot.kit = { ...kit }; face(s.b, s.foot, s.tank, 150); others(s); };

  it("keeps the block's output: what it could not put into the tank lands on the men with her, at the same luck and with no draw", () => {
    for (const kit of [{}, { atGuns: true }, FULL_KIT] as ForceKit[]) {
      // Three fields alike: the plain blow on the tank, the plain blow on the men, and the rule's blow split between them.
      const T = stage(false), M = stage(false), R = stage(true);
      for (const s of [T, M, R]) set(s, kit, (x) => south(x.foot, x.men[0], 100));
      const onTank: Blow = T.b.blow(T.foot, T.tank), onMen: Blow = M.b.blow(M.foot, M.men[0]);
      const { main, hurt } = split(R, R.foot);
      // One draw for the whole of it: the three fields' dice are where one blow leaves them.
      expect(R.b.rng.s).toBe(T.b.rng.s);
      expect(R.b.rng.s).toBe(M.b.rng.s);
      const share = R.b.pierceShare(R.foot, R.tank);
      expect(share).toBeGreaterThan(0);
      expect(share).toBeLessThan(1);
      expect([...hurt.keys()]).toEqual([R.men[0].id]);
      const h = hurt.get(R.men[0].id)!;
      expect(main.dmg / onTank.dmg).toBeCloseTo(share, 12);
      expect(h.dmg / onMen.dmg).toBeCloseTo(1 - share, 12);
      expect(main.dmg / onTank.dmg + h.dmg / onMen.dmg).toBeCloseTo(1, 12);
      // A blow on that block like any other: its own nerve, seen, and its harm owed to the striker.
      expect(h.nerve).toBe(onMen.nerve);
      expect(h.blind).toBe(false);
      expect(h.from).toEqual([[R.foot, h.dmg]]);
    }
  }, LONG);

  it('a machine-gun team facing a tank fires all of it at the men with her; with none in reach it does nothing', () => {
    const M = stage(false), R = stage(true), N = stage(true);
    for (const s of [M, R, N]) {
      face(s.b, s.mg, s.tank, 300); south(s.mg, s.men[0], 420);
      for (const e of s.b.units as LandUnit[]) if (e.side === 'att' && e !== s.tank && e !== s.men[0]) e.state = 'gone';
    }
    const onMen: Blow = M.b.blow(M.mg, M.men[0]);
    const { main, hurt } = split(R, R.mg);
    expect(main.dmg).toBe(0);
    expect(hurt.get(R.men[0].id)!.dmg).toBe(onMen.dmg);
    south(N.mg, N.men[0], reachOf(N.mg) + 1);
    const none = split(N, N.mg);
    expect(none.main.dmg).toBe(0);
    expect(none.hurt.size).toBe(0);
  }, LONG);

  it("lands on the nearest enemy block in reach that is not a tank, by the staff's own score; never a tank, a hidden block, one out of reach or one off the field", () => {
    const fresh = (others: (s: Stage) => void, unseen: (s: Stage) => number[] = () => []): number[] => {
      const s = stage(true);
      set(s, { atGuns: true }, (x) => { for (const e of x.b.units as LandUnit[]) if (e.side === 'att' && e !== x.tank && !x.men.slice(0, 2).includes(e)) e.state = 'gone'; others(x); });
      return [...split(s, s.foot, unseen(s)).hurt.keys()].map((id) => (id === s.men[0].id ? 0 : id === s.men[1].id ? 1 : -1));
    };
    const [near, far] = [60, 100];
    // The nearer of two on one bearing.
    expect(fresh((s) => { south(s.foot, s.men[0], far); south(s.foot, s.men[1], near); })).toEqual([1]);
    expect(fresh((s) => { south(s.foot, s.men[0], near); south(s.foot, s.men[1], far); })).toEqual([0]);
    // A routing block counts double its distance.
    expect(fresh((s) => { south(s.foot, s.men[0], far); south(s.foot, s.men[1], near); s.men[1].state = 'routing'; })).toEqual([0]);
    expect(fresh((s) => { south(s.foot, s.men[0], far); south(s.foot, s.men[1], far / 2 - 5); s.men[1].state = 'routing'; })).toEqual([1]);
    // Never a block hidden now, nor one that was hidden as the tick's blows began.
    expect(fresh((s) => { south(s.foot, s.men[0], far); south(s.foot, s.men[1], near); s.men[1].hid = true; })).toEqual([0]);
    expect(fresh((s) => { south(s.foot, s.men[0], far); south(s.foot, s.men[1], near); }, (s) => [s.men[1].id])).toEqual([0]);
    // Never one off the field, nor one beyond the striker's reach: then the rest is lost.
    expect(fresh((s) => { south(s.foot, s.men[0], far); south(s.foot, s.men[1], near); s.men[1].state = 'withdrawn'; })).toEqual([0]);
    expect(fresh((s) => { south(s.foot, s.men[0], reachOf(s.foot) + 1); s.men[1].state = 'gone'; })).toEqual([]);
    expect(fresh((s) => { south(s.foot, s.men[0], reachOf(s.foot)); s.men[1].state = 'gone'; })).toEqual([0]);
    // Never a tank: the tank it faces is nearest of all here, and a second tank block nearer still takes none of it.
    const s = stage(true);
    set(s, { atGuns: true }, (x) => { for (const e of x.b.units as LandUnit[]) if (e.side === 'att' && e.kind !== 'arm') e.state = 'gone'; });
    const second = (s.b.units as LandUnit[]).find((e) => e.side === 'att' && e.kind === 'arm' && e !== s.tank)!;
    south(s.foot, second, 40);
    expect(split(s, s.foot).hurt.size).toBe(0);
  }, LONG);

  it("the air's share is held back from the spilled blow as from any blow", () => {
    const air = { air: PRACTICE_AIR, date: PRACTICE_AIR_DATE };
    const W = stage(true, air), R = stage(true, air);
    for (const s of [W, R]) set(s, { atGuns: true }, (x) => south(x.foot, x.men[0], 100));
    expect(R.b.airLeft.def).toBeGreaterThan(0);
    const share = R.b.airOn.def.share;
    // The spilled blow as the rules weigh it, before anything is held back (the same rest, so no draw).
    const first: Blow = W.b.blow(W.foot, W.tank), whole: Blow = W.b.blow(W.foot, W.men[0], first.rest);
    const before = R.b.airLedger.def.withheld;
    const { hurt, kept } = split(R, R.foot);
    expect(R.b.rng.s).toBe(W.b.rng.s);
    expect(share).toBeGreaterThan(0);
    const raw = (whole.dmg * share) / whole.ground;
    expect(hurt.get(R.men[0].id)!.dmg).toBeCloseTo(whole.dmg * (1 - share), 12);
    expect(kept.get(R.men[0].id)).toBeCloseTo(raw, 12);
    expect(R.b.airBank.def.get(R.men[0].id)).toBeCloseTo(raw, 12);
    expect(R.b.airLedger.def.withheld - before).toBeCloseTo(raw, 12);
  }, LONG);
});

// ---- Whole battles with the switch on ---------------------------------------------------------------------------------

interface Seen {
  b: Sim;
  /** Blows weighed, and how many draws of the battle's dice each made. */
  mains: number; spills: number; mainDraws: number[]; spillDraws: number[];
  /** Blows on tank blocks by who struck: a machine-gun team's (and their weight), a mortar team's, a foot block's by distance. */
  mgOnTank: number; mgWeight: number; morOnTank: number; footOnTank: number; footNear: number; footMid: number; footFar: number; inOverrun: number; assaulted: number;
  /** Tank strength lost, and the part of it in ticks where a machine-gun team was among those it was owed to. */
  tankLost: number; owedToMg: number;
  overruns: number[]; ownAntiTank: number;
  /** What broke a rule, in words: empty is the pass. */
  bad: string[];
}

/** A battle fought with every blow watched: the wrappers call the rules' own methods with the same arguments. */
function watched(t: Ticket, set: (b: Sim) => void): Seen {
  const b = new LandBattle(t) as Sim;
  b.ai.att = b.ai.def = true;
  set(b);
  const seen: Seen = { b, mains: 0, spills: 0, mainDraws: [], spillDraws: [], mgOnTank: 0, mgWeight: 0, morOnTank: 0, footOnTank: 0, footNear: 0, footMid: 0, footFar: 0, inOverrun: 0, assaulted: 0, tankLost: 0, owedToMg: 0, overruns: [], ownAntiTank: 0, bad: [] };
  const bad = (s: string): void => { if (seen.bad.length < 12) seen.bad.push(s); };
  let draws = 0, hidden = new Set<number>();
  const next = b.rng.next.bind(b.rng), step = b.step.bind(b), blow = b.blow.bind(b), suffer = b.suffer.bind(b), own = b.ownAntiTank.bind(b), put = b.overrun.set.bind(b.overrun);
  b.rng.next = (): number => { draws++; return next(); };
  b.step = (): void => { hidden = new Set(b.units.filter((u: LandUnit) => u.hid).map((u: LandUnit) => u.id)); step(); };
  b.ownAntiTank = (u: LandUnit): boolean => { seen.ownAntiTank++; return own(u); };
  b.overrun.set = (k: number, v: number) => { seen.overruns.push(k); return put(k, v); };
  const count = (xs: number[], n: number): void => { if (!xs.includes(n)) xs.push(n); };
  b.blow = (u: LandUnit, t2: LandUnit, spill?: { roll: number; part: number }): Blow => {
    const d0 = draws, r: Blow = blow(u, t2, spill);
    if (spill) {
      seen.spills++; count(seen.spillDraws, draws - d0);
      if (t2.kind === 'arm' || t2.side === u.side || t2.hid || hidden.has(t2.id) || b.gap(u, t2) > reachOf(u) || r.rest) bad(`the rest of ${u.id}'s blow landed on ${t2.id} (${t2.kind})`);
      return r;
    }
    seen.mains++; count(seen.mainDraws, draws - d0);
    if (t2.kind !== 'arm') { if (r.rest) bad(`a blow on ${t2.kind} was split`); return r; }
    const foot = !u.team && (P.foot as readonly string[]).includes(u.kind), pair = b.inOverrun(u, t2);
    if (!b.pierce) { if (r.rest) bad('a blow was split with the switch off'); return r; }
    if (!u.team && !foot) { if (r.rest) bad(`${u.kind}'s blow on a tank was split`); return r; }
    // (a tank driving through the block that strikes her is weighed like any other, as the two stand: counted, and held
    // to the same checks below)
    if (pair) seen.inOverrun++;
    const gap = b.gap(u, t2), share = b.pierceShare(u, t2), rest = r.rest?.part ?? 0;
    if (Math.abs(rest - (1 - share)) > 1e-12) bad(`the rest ${rest} of a blow at share ${share}`);
    if (u.team === 'mg') { seen.mgOnTank++; seen.mgWeight += r.dmg; if (r.dmg !== 0 || rest !== 1) bad(`a machine-gun team's blow of ${r.dmg} on a tank`); }
    else if (u.team === 'mor') { seen.morOnTank++; if (share !== P.teams.mor) bad(`a mortar team's share ${share}`); }
    else {
      seen.footOnTank++;
      const G = Math.min(P.guns.most, (u.kit?.atGuns ? P.guns.with : P.guns.without) + (u.kit?.atTd ? P.guns.destroyers : 0));
      if (gap <= P.close.at) seen.footNear++; else if (gap <= P.hand.at) seen.footMid++; else seen.footFar++;
      // Beyond the launchers' range a block on foot has its own guns' share and no more; beyond close assault, one
      // with no launchers the same.
      if (gap > P.hand.at && share !== G) bad(`foot's share ${share} at ${gap.toFixed(0)} m, its guns ${G}`);
      if (gap > P.close.at && !u.kit?.atHand && share !== G) bad(`foot's share ${share} at ${gap.toFixed(0)} m with no launchers, its guns ${G}`);
      if (share > 1 || share < G - 1e-12) bad(`foot's share ${share}`);
      // Among the men she drives through, their close assault reaches her too: never their guns alone, never nothing.
      if (pair && gap <= P.close.at) { seen.assaulted++; if (share < G + Math.min(P.close.open, P.eng.open) - 1e-12) bad(`foot's share ${share} on a tank driving through it at ${gap.toFixed(0)} m`); }
    }
    return r;
  };
  b.suffer = (t2: LandUnit, dmg: number, nerveMul: number, from: [LandUnit, number][] = [], blind = false, bare = false): void => {
    const before = t2.str;
    suffer(t2, dmg, nerveMul, from, blind, bare);
    if (t2.kind !== 'arm') return;
    seen.tankLost += before - t2.str;
    if (from.some(([u]) => u.team === 'mg')) seen.owedToMg += before - t2.str;
  };
  runOut(b);
  return seen;
}

describe('whole battles with the switch on', () => {
  it('a machine-gun team takes nothing off a tank, and is owed none of her losses; off, its bullets did', () => {
    const t = (): Ticket => withTeams(mirror(sampleTicket('forest', 1.2, { seed: seedOf(2) })));
    const on = watched(t(), (b) => { b.escort = true; b.pierce = true; });
    expect(on.bad).toEqual([]);
    expect(on.mgOnTank).toBeGreaterThan(100);
    expect(on.mgWeight).toBe(0);
    expect(on.owedToMg).toBe(0);
    expect(on.tankLost).toBeGreaterThan(0);
    const off = watched(t(), (b) => { b.escort = true; });
    expect(off.bad).toEqual([]);
    expect(off.spills).toBe(0);
    expect(off.owedToMg).toBeGreaterThan(0);
  }, LONG);

  it('rifles and machine guns take nothing off a tank beyond the ruled reaches: the plain kit, the sample kit and the full kit', () => {
    for (const [ground, n, mir, kit, set] of [
      ['plains', 1, false, { att: {}, def: {} }, (b: Sim) => { b.escort = true; }],
      ['hills', 3, true, { att: {}, def: {} }, (b: Sim) => { b.escort = true; }],
      ['hills', 1, true, undefined, ship],
      ['forest', 1, false, { att: FULL_KIT, def: FULL_KIT }, ship],
    ] as [Parameters<typeof sampleTicket>[0], number, boolean, SampleOpts['kit'], (b: Sim) => void][]) {
      const base = sampleTicket(ground, 1.2, { seed: seedOf(n), ...(kit ? { kit } : {}) });
      const seen = watched(withTeams(mir ? mirror(base) : base), (b) => { set(b); b.pierce = true; });
      expect(seen.bad, `${ground} ${n}`).toEqual([]);
      expect(seen.footOnTank, `${ground} ${n}`).toBeGreaterThan(100);
      expect(seen.spills).toBeGreaterThan(0);
      // The dice: one draw for every blow, none for the rest of one.
      expect(seen.mainDraws).toEqual([1]);
      expect(seen.spillDraws).toEqual([0]);
    }
  }, LONG);

  it('the dice fall in the same order: the first tick a tank is struck draws the very blows of the switch off', () => {
    const rolls = (pierce: boolean): { ticks: string[]; split: number } => {
      const b = new LandBattle(withTeams(mirror(sampleTicket('forest', 1.2, { seed: seedOf(2) })))) as Sim;
      b.ai.att = b.ai.def = true; b.escort = true; b.pierce = pierce;
      let last = NaN, now: [number, number, number][] = [], struck = false, split = 0;
      const next = b.rng.next.bind(b.rng), blow = b.blow.bind(b);
      b.rng.next = (): number => (last = next());
      b.blow = (u: LandUnit, t: LandUnit, spill?: unknown): Blow => {
        const r: Blow = blow(u, t, spill);
        if (r.rest) split++;
        if (!spill) { now.push([u.id, t.id, last]); if (t.kind === 'arm' && !(u.kind === 'arm' || (u.kind === 'art' && !u.team))) struck = true; }
        return r;
      };
      const ticks: string[] = [];
      while (!b.over && !struck) { now = []; b.step(); ticks.push(JSON.stringify(now)); }
      expect(struck).toBe(true);
      return { ticks, split };
    };
    const off = rolls(false), on = rolls(true);
    expect(on.ticks.length).toBeGreaterThan(10);
    expect(on.ticks).toEqual(off.ticks);
    expect(on.split).toBeGreaterThan(0);
    expect(off.split).toBe(0);
  }, LONG);

  it('a battle whose blocks never meet a tank is the same battle, on and off', () => {
    for (const [ground, n] of [['plains', 4], ['urban', 2]] as const) {
      const t = (): Ticket => {
        const x = sampleTicket(ground, 1.2, { seed: seedOf(n), kit: { att: FULL_KIT, def: {} } });
        x.att.forces = x.att.forces.map((f) => (f.kind === 'arm' ? { ...f, kind: 'mot', kit: { ...FULL_KIT } } : f));
        return withTeams(x);
      };
      const off = fight(t(), ship), on = fight(t(), (b) => { ship(b); b.pierce = true; });
      expect(on.pierce).toBe(true);
      expect(blocks(on).some((u) => u.kind === 'arm')).toBe(false);
      expect(sig(on)).toBe(sig(off));
    }
  }, LONG);

  it("nothing reads an army's name or the date: the same kit under other names and another month fights the same battle", () => {
    const run = (nat: { att: string; def: string }, turn: number, date: string, pierce: boolean): Seen => {
      const t = withTeams(sampleTicket('plains', 1.2, { seed: seedOf(1), nat, date, style: { att: 'methodical', def: 'methodical' } }));
      t.turn = turn;
      return watched(t, (b) => { b.escort = true; b.pierce = pierce; });
    };
    const JUNE44 = 58;
    const plain = run({ att: 'RED', def: 'BLU' }, 1, 'September 1939', true);
    for (const [nat, turn, date] of [[{ att: 'GER', def: 'USA' }, JUNE44, 'June 1944'], [{ att: 'USA', def: 'GER' }, JUNE44, 'June 1944'], [{ att: 'SOV', def: 'UK' }, 20, 'May 1941']] as [{ att: string; def: string }, number, string][]) {
      const named = run(nat, turn, date, true);
      expect(figures(named.b)).toBe(figures(plain.b));
      // The overrun's old table by army and month is never opened.
      expect(named.ownAntiTank).toBe(0);
    }
    expect(plain.ownAntiTank).toBe(0);
    // (it bites: with the switch off the overrun asks that table, and the same swap changes the battle)
    const off = run({ att: 'RED', def: 'BLU' }, 1, 'September 1939', false), offNamed = run({ att: 'GER', def: 'USA' }, JUNE44, 'June 1944', false);
    expect(off.ownAntiTank).toBeGreaterThan(0);
    expect(figures(offNamed.b)).not.toBe(figures(off.b));
  }, LONG);

  it('replays the same battle', () => {
    const t = (): Ticket => withTeams(mirror(sampleTicket('hills', 1.3, { seed: seedOf(5) })));
    const a = fight(t(), (b) => { ship(b); b.pierce = true; }), c = fight(t(), (b) => { ship(b); b.pierce = true; });
    expect(sig(c)).toBe(sig(a));
    const p = piece('prokhorovka-1943', 5, { pierce: true, escort: true }), q = piece('prokhorovka-1943', 5, { pierce: true, escort: true });
    expect(p.pierce).toBe(true);
    expect(sig(q)).toBe(sig(p));
    // (and it is a rule: the same piece with the switch off ends otherwise)
    expect(sig(piece('prokhorovka-1943', 5, { escort: true }))).not.toBe(sig(p));
  }, LONG);

  it('gives the same battle however finely the game asks for it, and a battle with the men drawn ends as one without', () => {
    const UPTO = 150;
    const canon = (v: unknown): string =>
      JSON.stringify(v, (_k, x) => (x && typeof x === 'object' && !Array.isArray(x) ? Object.fromEntries(Object.entries(x).sort(([a], [c]) => (a < c ? -1 : 1))) : x));
    const live = (tick: number): { out: LiveDelta[]; b: Sim } => {
      const L = livePiece('normandy-1944', 11, { me: null, pierce: true });
      const out: LiveDelta[] = [];
      while (!L.sim.over && L.sim.t < UPTO - 1e-6) out.push(JSON.parse(JSON.stringify(L.to(Math.min(UPTO, L.sim.t + tick), 100000))));
      return { out, b: L.sim as Sim };
    };
    const fine = live(2.5), coarse = live(UPTO);
    expect(fine.out.length).toBeGreaterThan(coarse.out.length);
    // (the rule has been at work by then: a tank block has been struck)
    expect(fine.b.units.some((u: LandUnit) => u.kind === 'arm' && u.str < u.str0)).toBe(true);
    expect(sig(fine.b)).toBe(sig(coarse.b));
    expect(canon(LiveKeeper.assemble(fine.out))).toBe(canon(LiveKeeper.assemble(coarse.out)));
    expect(canon(fine.out.flatMap((d) => d.rounds))).toBe(canon(coarse.out.flatMap((d) => d.rounds)));
    expect(canon(fine.out.flatMap((d) => d.events))).toBe(canon(coarse.out.flatMap((d) => d.events)));
    // The men drawn (the drill) against the drill removed, to the end.
    const drawn = livePiece('normandy-1944', 11, { me: null, platoons: false, pierce: true }).sim as Sim;
    expect(drawn.drill).toBeTruthy();
    while (!drawn.over) drawn.step();
    const plain = piece('normandy-1944', 11, { pierce: true });
    expect(figures(drawn)).toBe(figures(plain));
  }, LONG);
});

// ---- The overrun's threat (P5) ----------------------------------------------------------------------------------------

describe('the overrun under the switch (P5)', () => {
  it('foot threatens a hull by its kit alone: its guns at its reach, else launchers at hand.at, else not at all', () => {
    const on = stage(true), off = stage(false);
    const reach = (s: Stage, kit: ForceKit | undefined): number | null => { s.foot.kit = kit; return s.b.tankReach(s.foot); };
    const full = reachOf(on.foot);
    expect(reach(on, undefined)).toBeNull();
    expect(reach(on, {})).toBeNull();
    expect(reach(on, { atTd: true })).toBeNull();
    expect(reach(on, { at: true })).toBeNull();
    expect(reach(on, { atGuns: true })).toBe(full);
    expect(reach(on, { atHand: true })).toBe(P.hand.at);
    expect(reach(on, { atGuns: true, atHand: true })).toBe(full);
    expect(reach(on, { at: false, atGuns: true })).toBe(full);
    // Off, the overrun's own test stands: its own field, and never this rule's.
    expect(reach(off, { at: true })).toBe(full);
    expect(reach(off, { at: false, atGuns: true, atHand: true })).toBeNull();
    // Tanks, guns over open sights and teams: as ever, on and off.
    for (const s of [on, off]) {
      expect(s.b.tankReach(s.own)).toBe(reachOf(s.own));
      expect(s.b.tankReach(s.gun)).toBe(LAND.guns.closeAt);
      expect(s.b.tankReach(s.mg)).toBeNull();
      expect(s.b.tankReach(s.mor)).toBeNull();
    }
    // The drill's question, whether a weapon that can pierce a hull reaches a block now, gives the same answer.
    const threat = (kit: ForceKit, gap: number): boolean => { on.foot.kit = kit; face(on.b, on.foot, on.tank, gap); return on.b.hullThreat(on.tank); };
    expect(threat({}, 20)).toBe(false);
    expect(threat({ atHand: true }, P.hand.at)).toBe(true);
    expect(threat({ atHand: true }, P.hand.at + 1)).toBe(false);
    expect(threat({ atGuns: true }, full)).toBe(true);
    expect(threat({ atGuns: true }, full + 1)).toBe(false);
  }, LONG);

  it('tanks drive through foot with the plain kit and never through foot with its own anti-tank guns, whatever the month', () => {
    const run = (kit: SampleOpts['kit'], pierce: boolean): Seen =>
      watched(withTeams(sampleTicket('plains', 1.2, { seed: seedOf(1), ...(kit ? { kit } : {}) })), (b) => { b.escort = true; b.pierce = pierce; });
    const plain = run({ att: {}, def: {} }, true), sample = run(undefined, true), off = run(undefined, false);
    expect(plain.overruns.length).toBeGreaterThan(0);
    for (const id of plain.overruns) { const f = plain.b.unit(id)!; expect((P.foot as readonly string[]).includes(f.kind)).toBe(true); expect(f.kit?.atGuns || f.kit?.atHand).toBeFalsy(); }
    expect(plain.inOverrun).toBeGreaterThan(0);
    // (the men she drives through strike her with their guns and, among them, their close assault: watched() holds every such blow to it)
    expect(plain.assaulted).toBeGreaterThan(0);
    expect(plain.bad).toEqual([]);
    expect(sample.overruns).toEqual([]);
    expect(sample.ownAntiTank + plain.ownAntiTank).toBe(0);
    // (off, this sample's armies have no table of their own, so its foot is driven through)
    expect(off.overruns.length).toBeGreaterThan(0);
    expect(off.ownAntiTank).toBeGreaterThan(0);
  }, LONG);
});

// ---- Where kit comes from (P3) ------------------------------------------------------------------------------------------

describe('where kit comes from (P3)', () => {
  it("a sample ticket gives both sides' foot the same standard kit, by hand where asked; a practice ticket the plain kit", () => {
    const t = withTeams(sampleTicket('plains', 1.2, { seed: 5 }));
    for (const f of forces(t)) expect(kitOf(f), `${f.name} ${f.kind}`).toEqual((P.foot as readonly string[]).includes(f.kind) ? SAMPLE_KIT : null);
    expect(forces(t).filter((f) => f.kit).length).toBe(4);
    // Each force its own copy: one corps' kit is not another's.
    expect(t.att.forces[0].kit).not.toBe(t.att.forces[1].kit);
    expect(t.att.forces[0].kit).not.toBe(SAMPLE_KIT);
    const by = sampleTicket('hills', 1.3, { seed: 5, kit: { att: FULL_KIT, def: {} } });
    expect(by.att.forces.filter((f) => f.kind === 'inf').map(kitOf)).toEqual([FULL_KIT, FULL_KIT]);
    expect(by.att.forces.filter((f) => f.kind !== 'inf').map(kitOf)).toEqual([null]);
    expect(by.def.forces.map(kitOf)).toEqual([null, null, null]);
    expect(sampleTicket('hills', 1.3, { kit: { def: null } }).def.forces.map(kitOf)).toEqual([null, null, null]);
    expect(sampleTicket('hills', 1.3, { kit: { def: null } }).att.forces.map(kitOf)).toEqual([SAMPLE_KIT, SAMPLE_KIT, null]);
    // The mirror takes each corps' kit with it.
    const m = mirror(by);
    expect(m.def.forces.map(kitOf)).toEqual([FULL_KIT, FULL_KIT, null]);
    for (const p of PRACTICE_GROUNDS) expect(forces(practiceTicket(p, { seed: 5 })).every((f) => f.kit === undefined)).toBe(true);
  }, LONG);

  it("a set piece's kit is written by hand in its own data, on its foot alone", () => {
    const kits = (key: string): Record<string, (ForceKit | null)[]> => {
      const t = livePiece(key, 11, { me: null, platoons: false }).sim.ticket as Ticket;
      for (const f of forces(t)) if (!(P.foot as readonly string[]).includes(f.kind)) expect(f.kit, `${key} ${f.name}`).toBeUndefined();
      return { att: t.att.forces.filter((f) => f.kit).map(kitOf), def: t.def.forces.filter((f) => f.kit).map(kitOf) };
    };
    expect(kits('normandy-1944')).toEqual({ att: [FULL_KIT], def: [FULL_KIT] });
    expect(kits('prokhorovka-1943')).toEqual({ att: [{ atGuns: true }], def: [{ atGuns: true, atTd: true }] });
  }, LONG);

  it("a campaign's ticket reads each corps' own nation's research, under the research rules alone, and opens nobody's research", () => {
    const g: Game = startOpen(createGame(3, 'GER'));
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    const r = g.regions.find((x) => x.name === 'Pomorze')!, cs = candidates(g, r);
    expect(cs.some((c) => c.kind === 'arm')).toBe(true);
    expect(cs.some((c) => c.kind === 'inf')).toBe(true);
    const T = SUPPORT.tech, done = (nat: string): string[] => g.nations[nat].tech!.done;
    const keep = { GER: [...done('GER')], POL: [...done('POL')] };
    const having = (ger: string[], pol: string[]): Ticket => {
      g.nations.GER.tech!.done = [...keep.GER.filter((id) => ![T.antiTank, T.tankDestroyers, T.handHeld].includes(id)), ...ger];
      g.nations.POL.tech!.done = [...keep.POL.filter((id) => ![T.antiTank, T.tankDestroyers, T.handHeld].includes(id)), ...pol];
      const before = JSON.stringify(g.nations);
      const t = landTicket(g, cs, r)!;
      expect(JSON.stringify(g.nations)).toBe(before);
      return t;
    };
    const foot = (fs: ForceTicket[]): (ForceKit | null)[] => fs.filter((f) => (P.foot as readonly string[]).includes(f.kind)).map(kitOf);
    const rest = (fs: ForceTicket[]): (ForceKit | null)[] => fs.filter((f) => !(P.foot as readonly string[]).includes(f.kind)).map(kitOf);
    // Nothing researched: the plain kit, no field written.
    let t = having([], []);
    expect(forces(t).every((f) => f.kit === undefined)).toBe(true);
    // Each side by its own record; tanks carry none of it.
    t = having([T.antiTank, T.handHeld], [T.tankDestroyers]);
    expect(foot(t.att.forces).length).toBeGreaterThan(0);
    for (const k of foot(t.att.forces)) expect(k).toEqual({ atGuns: true, atHand: true });
    for (const k of foot(t.def.forces)) expect(k).toEqual({ atTd: true });
    expect(foot(t.def.forces).length).toBeGreaterThan(0);
    for (const k of [...rest(t.att.forces), ...rest(t.def.forces)]) expect(k).toBeNull();
    t = having([T.antiTank, T.tankDestroyers, T.handHeld], [T.antiTank]);
    for (const k of foot(t.att.forces)) expect(k).toEqual(FULL_KIT);
    for (const k of foot(t.def.forces)) expect(k).toEqual({ atGuns: true });
    // The overrun's own field is not written.
    expect(forces(t).every((f) => f.kit?.at === undefined)).toBe(true);
    // The record, not the name: the two nations' research changed over, their corps' kit changes over with it.
    t = having([T.tankDestroyers], [T.antiTank, T.handHeld]);
    for (const k of foot(t.att.forces)) expect(k).toEqual({ atTd: true });
    for (const k of foot(t.def.forces)) expect(k).toEqual({ atGuns: true, atHand: true });
    // Without the research rules: no field, both sides the plain kit.
    g.flags.open = 0;
    t = having([T.antiTank, T.tankDestroyers, T.handHeld], [T.antiTank]);
    expect(forces(t).every((f) => f.kit === undefined)).toBe(true);
  }, LONG);
});

// ---- The code itself ----------------------------------------------------------------------------------------------------

describe("the rule's code", () => {
  it("names no nation and no date, and opens neither the calendar's table nor the overrun's", () => {
    // (each method's own text, as the test runner holds it)
    const text = (name: string): string => { const f = (LandBattle.prototype as Sim)[name]; expect(typeof f, name).toBe('function'); return String(f); };
    const blow = text('blow'), at = blow.search(/if \(this\.pierce && \(spill \|\| t\.kind === ['"]arm['"]\)\)/);
    expect(at).toBeGreaterThan(0);
    const parts = [text('kitReach'), text('pierceShare'), text('splitFire'), blow.slice(at, blow.indexOf('return { dmg, nerve', at))];
    // (a call of turn is the angle between two bearings, geometry's own; a .turn read as a value would be the ticket's)
    const never = /\bnat\b|\bnation|\bticket\b|turnToDate|\.turn\b(?!\)?\()|\bdate\b|\byear\b|\bmonth\b|byYear|overrun\.at|ownAntiTank|commonwealth|reckoning|antiTank\(/;
    for (const p of parts) {
      expect(p.length).toBeGreaterThan(80);
      expect(p.match(never)?.[0] ?? null).toBeNull();
    }
    // (the check bites on the overrun's own test, which does open its table by army and month)
    expect(text('ownAntiTank').match(never)?.[0] ?? null).not.toBeNull();
    // With the switch on, the overrun asks the kit before it could ask the old table, on both its questions.
    const reach = text('tankReach'), kit = reach.search(/if \(this\.pierce\)\s*return this\.kitReach\(e\)/);
    expect(kit).toBeGreaterThan(0);
    expect(kit).toBeLessThan(reach.indexOf('ownAntiTank'));
    expect(text('overruns')).toMatch(/this\.pierce \? this\.kitReach\(f\) !== null : this\.ownAntiTank\(f\)/);
    // Every read of the switch in the rules is one of these places, and the split's own.
    expect(text('step')).toMatch(/const unseen = this\.pierce \?/);
    expect(text('step')).toMatch(/if \(blow\.rest\)\s*this\.splitFire\(/);
  }, LONG);
});
