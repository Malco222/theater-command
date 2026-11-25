// The land battle's paired measure (the design director's pierce ruling, 25 Nov 2026, P7 and P8): what a rule switch
// does to who wins, measured on the same battles with the switch off and on, beside a dice placebo. Written for
// "Only anti-tank weapons take a tank" (LandBattle.pierce, LAND.pierce); any other switch in SWITCH can be put through
// the same sets (its own marks then need its own ruling). Reads the rules, changes nothing: the harm tallies wrap
// methods on the one battle and call the originals with the same arguments.
//
// Arms, each on the same ticket and seed:
//   OFF      the switch off.
//   ON       the switch on.
//   PLACEBO  the switch off, the battle's luck re-dealt by one throwaway draw on its dice before the first tick: how
//            many results flip, and how far the win rate moves, by luck alone on the same battles (the chance floor).
// Mirrors: a sample ticket as it is (the attack has the tanks) and mirrored (the two make-ups swapped, the attack's
// forces not dug in, so the defence has the tanks). Kit goes with the corps. Seeds are s x 7919 + 13, s from 1.
//
// Sets:
//   S1  the standing gate with the switch on: the "fair to the dice" table in the test's own form (7 grounds x 8 odds
//       x 24 seeds), the cost test (7 x 6 x 4) and mark 2's fight times; bare rules, the sample kit, no mirror.
//   S2  the shipping set: plains, forest, hills, urban x odds 1.2 and 1.3 x 30 seeds x 2 mirrors x 3 arms, with
//       machine-gun and mortar teams, with hide, measured armour, cutters, phones, patrol and escort set by hand on a
//       bare LandBattle (smoke and bearers off, as shipped).
//   S3  bare: all 7 grounds x odds 1.2 x 24 seeds x 2 mirrors x 3 arms, no teams.
//   S4  kit lean, the switch on only: plains, forest, hills, urban x 1.2 x 30 seeds x 2 mirrors, with teams; the side
//       without tanks at the plain kit, then at the full kit, the side with tanks at the sample kit.
//       (--placebo adds the plain kit with a throwaway draw, the set's own chance floor.)
//   S5  the set pieces: normandy-1944 and prokhorovka-1943, 24 seeds each, livePiece with the drill removed, x 3 arms.
//   SF  a reading with no bar, not one of the pass rule's sets and not run by 'all' (the fairness checker's, 25 Nov):
//       an army all on foot against tanks. Every set above gives the side without tanks a corps of field guns, which
//       keep their full weight on a tank; here its guns' corps is a third foot corps of the same share. All 7 grounds
//       x odds 1.2 x 30 seeds x off and on, the shipping switches, no teams, both ways round (--mirror attack: the
//       all-foot side defends; defend: it attacks), its foot at the plain kit and then at the sample kit. With the
//       plain kit it is also the only set where tanks drive through foot with the switch on. Whether the pass rule
//       gets such a mark is the design director's to rule.
//
// Marks (P7): M1 the standing gate's bars with the switch on (S1); M2 the lean and M3 the clock (S2 and S3, each
// mirror); M4 the set pieces (S5); M5 the kit lean (S4, each mirror); M6 the purpose (the ON arms: machine-gun teams
// take nothing of tank strength, foot beyond launcher range takes only its guns' share, foot is not helpless).
// A paired mark (M2, M3, M5) that ON fails no worse than PLACEBO fails it on the same set is chance; M1, M4 and M6 are
// absolute. Readings with no bar: field guns beyond 160 m as a share of tank loss, how often tanks overrun, foot's
// share of tank loss by distance.
// --like adds a fourth arm to S2, a reading with no bar: the switch off with every foot block given its own anti-tank
// weapon by the overrun's own field (ForceKit.at), so that tanks drive through nobody in either arm. The sample's
// made-up armies are in no anti-tank table, so with the switch off their foot never stops an overrun, and with it on
// the sample kit always does: ON less this arm is the rule's lean with that difference taken out.
//
//   npx esbuild tools/land-gate/paired.ts --bundle --platform=node --format=esm --outfile=dist/land-gate.mjs --log-level=warning
//   node dist/land-gate.mjs [--set S1,S2,S3,S4,S5,SF | all] [--mirror attack|defend|both] [--switch pierce] [--seeds N]
//                           [--arm on|off] [--placebo] [--like] [--json file]
//   --mirror: one mirror of S2, S3 or S4 only (each mirror is judged on its own, so a long set can be run in two goes).
//   --seeds: fewer seeds a cell, for a quick look (the marks then say so and are no verdict).
//   --arm off: S1 with the switch off, to read the standing gate's own figures beside it.
//   --placebo: S4's own chance floor (S2, S3 and S5 always carry their placebo arm).
//   --like: S2's like for like reading (480 battles more).
//   --json: the figures of each set, appended one line a set.
// About 4,600 battles in all (4,800 with --placebo), 0.3 to 0.5 s each: run one set at a time on a busy machine.
import { appendFileSync } from 'node:fs';
import { LAND, LandBattle, SAMPLE_KIT, sampleTicket, withTeams, type LandUnit } from '../../src/battle/land';
import { livePiece } from '../../src/battle/land/live';
import { PIECES } from '../../src/battle/land/pieces';
import { diceChance, runOut } from '../../src/battle/core';
import type { FieldTicket, ForceKit } from '../../src/battle/core/types';
import type { LandPlace, Terrain } from '../../src/battle/land/field';

// (the harm tally wraps the rules' own private methods by name)
// eslint-disable-next-line @typescript-eslint/no-explicit-any
type Sim = any;
type Arm = 'off' | 'on' | 'placebo' | 'like';
type Ticket = FieldTicket<LandPlace>;

const SWITCH: Record<string, (b: Sim) => void> = {
  none: () => {},
  pierce: (b) => { b.pierce = true; },
  hide: (b) => { b.hideOn(); },
  measured: (b) => { b.measured = true; },
  smoke: (b) => { b.smoke = true; },
  phones: (b) => { b.phones = true; },
  bearers: (b) => { b.bearers = true; },
  escort: (b) => { b.escort = true; },
};
/** The switches the 3D game ships on, set by hand on a bare battle (smoke and bearers held off, escort on). */
const shipping = (b: Sim): void => { b.hideOn(); b.measured = true; b.cutters = true; b.phones = true; b.patrol = true; b.escort = true; };

const args = process.argv.slice(2);
const opt = (name: string, d = ''): string => { const i = args.indexOf(`--${name}`); return i >= 0 ? args[i + 1] ?? '' : d; };
const flag = (name: string): boolean => args.includes(`--${name}`);
const SW = opt('switch', 'pierce');
if (!SWITCH[SW]) throw new Error(`no switch ${SW}`);
const SETS = ((s) => (s === 'all' ? 'S1,S2,S3,S4,S5' : s))(opt('set', 'all')).split(',');
const MIRRORS: boolean[] = ((m) => (m === 'attack' ? [false] : m === 'defend' ? [true] : [false, true]))(opt('mirror', 'both'));
const SEEDS = Number(opt('seeds', '0')) || 0;
const JSON_OUT = opt('json');
const S1_ARM: Arm = opt('arm', 'on') === 'off' ? 'off' : 'on';
const LIKE = flag('like');
const FULL_KIT: ForceKit = { atGuns: true, atTd: true, atHand: true };
const seedOf = (s: number): number => s * 7919 + 13;
const GROUNDS7: Terrain[] = ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'];
const GROUNDS4: Terrain[] = ['plains', 'forest', 'hills', 'urban'];
const P = LAND.pierce;
const FOOT: readonly string[] = P.foot;

/** The sample ticket mirrored: the two make-ups swapped (kit goes with the corps), the attack's forces not dug in. */
const mirror = (t: Ticket): Ticket => ({ ...t,
  att: { ...t.att, forces: t.def.forces.map((f) => ({ ...f, nat: t.att.nat, dug: false })) },
  def: { ...t.def, forces: t.att.forces.map((f) => ({ ...f, nat: t.def.nat })) } });

// ---- Who takes tanks: the harm tally ---------------------------------------------------------------------------------

interface Harm {
  /** Tank strength lost, in power, by who struck it: tanks, guns near and far (beyond 160 m), foot by distance, teams, fort, air and the rest. */
  by: Record<string, number>;
  /** Tank models lost, by the striker each went to (the rules' own owed tally). */
  models: Record<string, number>;
  /** Tank power brought, and lost. */
  brought: number; lost: number;
  /** The attack's and the defence's tanks' loss, and foot's part of each. */
  side: Record<'att' | 'def', { lost: number; foot: number }>;
  /** M6's counts: a machine-gun team's harm on a tank; a foot blow beyond launcher range above its guns' share; a blow whose rest is not 1 less its share. */
  mgHarm: number; beyond: number; odd: number; footBlows: number; maxBeyond: number;
  overruns: number;
}
const harm0 = (): Harm => ({ by: {}, models: {}, brought: 0, lost: 0, side: { att: { lost: 0, foot: 0 }, def: { lost: 0, foot: 0 } }, mgHarm: 0, beyond: 0, odd: 0, footBlows: 0, maxBeyond: 0, overruns: 0 });
const put = (r: Record<string, number>, k: string, v: number): void => { r[k] = (r[k] ?? 0) + v; };
const addHarm = (a: Harm, b: Harm): void => {
  for (const k of Object.keys(b.by)) put(a.by, k, b.by[k]);
  for (const k of Object.keys(b.models)) put(a.models, k, b.models[k]);
  a.brought += b.brought; a.lost += b.lost;
  for (const s of ['att', 'def'] as const) { a.side[s].lost += b.side[s].lost; a.side[s].foot += b.side[s].foot; }
  a.mgHarm += b.mgHarm; a.beyond += b.beyond; a.odd += b.odd; a.footBlows += b.footBlows; a.maxBeyond = Math.max(a.maxBeyond, b.maxBeyond); a.overruns += b.overruns;
};
const who = (u: LandUnit, gap: number): string =>
  u.kind === 'arm' ? 'tanks'
    : u.team === 'mg' ? 'machine-gun teams' : u.team === 'mor' ? 'mortar teams'
      : u.kind === 'art' ? (gap >= LAND.guns.closeAt ? 'field guns beyond 160 m' : 'field guns within 160 m')
        : u.kind === 'fort' ? 'forts'
          : FOOT.includes(u.kind) ? (gap <= P.close.at ? `foot to ${P.close.at} m` : gap <= P.hand.at ? `foot ${P.close.at} to ${P.hand.at} m` : `foot beyond ${P.hand.at} m`) : 'other';
const short = (k: string): string => (k.startsWith('foot') ? 'foot' : k.startsWith('field guns') ? 'field guns' : k);

/** Wraps suffer, blow and the overrun map on this one battle: the originals are called with the same arguments. */
function watch(b: Sim, H: Harm, on: boolean): void {
  const suffer = b.suffer.bind(b), offField = b.offField.bind(b);
  for (const u of b.allBlocks()) if (u.kind === 'arm') H.brought += u.power0;
  b.suffer = (t: LandUnit, dmg: number, nerveMul: number, from: [LandUnit, number][] = [], blind = false, bare = false): void => {
    if (t.kind !== 'arm') { suffer(t, dmg, nerveMul, from, blind, bare); return; }
    const before = t.str, had = t.figures, owed: Record<number, number> = { ...((t as any).owed ?? {}) };
    const gaps = from.map(([u]) => b.gap(u, t));
    suffer(t, dmg, nerveMul, from, blind, bare);
    const lost = ((before - t.str) / t.str0) * t.power0, sum = from.reduce((a, [, d]) => a + d, 0);
    H.lost += lost; H.side[t.side].lost += lost;
    if (lost > 0) {
      if (!from.length || sum <= 0) put(H.by, bare ? 'kept harm landing' : 'air', lost);
      else from.forEach(([u, d], i) => {
        const v = (lost * d) / sum, k = who(u, gaps[i]);
        put(H.by, k, v);
        if (k.startsWith('foot')) H.side[t.side].foot += v;
        if (u.team === 'mg' && d > 0) H.mgHarm += v;
      });
    }
    const model = t.power0 / Math.max(1e-9, t.tray * t.str0);
    for (const [u, d] of from) owed[u.id] = (owed[u.id] ?? 0) + d;
    for (let k = t.figures; k < had; k++) {
      let by: LandUnit | undefined;
      for (const [u] of from) if (!by || owed[u.id] > owed[by.id]) by = u;
      if (!by) { put(H.models, 'air', 1); continue; }
      owed[by.id] -= model;
      put(H.models, short(who(by, 0)), 1);
    }
  };
  b.offField = (s: string, id: number, raw: number): void => {
    const e = b.unit(id), before = e?.str ?? 0;
    offField(s, id, raw);
    if (e && e.kind === 'arm') { const v = ((before - e.str) / e.str0) * e.power0; put(H.by, 'air', v); H.lost += v; H.side[e.side as 'att' | 'def'].lost += v; }
  };
  const set = b.overrun.set.bind(b.overrun);
  b.overrun.set = (k: number, v: number) => { H.overruns++; return set(k, v); };
  if (on && SW === 'pierce') {
    const blow = b.blow.bind(b);
    b.blow = (u: LandUnit, t: LandUnit, spill?: { roll: number; part: number }) => {
      const r = blow(u, t, spill);
      if (!spill && t.kind === 'arm') {
        if (u.team === 'mg' && r.dmg !== 0) H.mgHarm += r.dmg;
        if (FOOT.includes(u.kind) && !u.team) {
          const gap = b.gap(u, t), s = b.pierceShare(u, t, gap);
          const G = Math.min(P.guns.most, (u.kit?.atGuns ? P.guns.with : P.guns.without) + (u.kit?.atTd ? P.guns.destroyers : 0));
          H.footBlows++;
          if (gap > P.hand.at) { H.maxBeyond = Math.max(H.maxBeyond, s); if (s > G + 1e-12 || s > P.guns.most + 1e-12) H.beyond++; }
          if (Math.abs((r.rest?.part ?? 0) - (1 - s)) > 1e-12) H.odd++;
        }
      }
      return r;
    };
  }
}

// ---- One battle -------------------------------------------------------------------------------------------------------

interface Run { won: boolean; R: number; secs: number; la: number; ld: number; clock: boolean; fight: number | null; harm: Harm; halfFought: boolean }

function finish(b: Sim, harm: Harm): Run {
  const o = b.over!;
  let halfFought = true;
  const broke = b.log.find((e: { what: string }) => e.what === 'broken');
  if (broke) {
    const us = b.units.filter((u: LandUnit) => u.side === broke.side);
    const all = us.reduce((a: number, u: LandUnit) => a + u.power0, 0);
    const fought = us.filter((u: LandUnit) => u.met || u.str < u.str0 * 0.97 || u.state === 'gone').reduce((a: number, u: LandUnit) => a + u.power0, 0);
    halfFought = fought / all >= 0.5;
  }
  return { won: o.winner === 'att', R: o.R, secs: o.seconds, la: o.lost.att, ld: o.lost.def, clock: o.how === 'time', fight: b.contactAt === null ? null : b.t - b.contactAt, harm, halfFought };
}

let fought = 0;
function fight(t: Ticket, arm: Arm, ship: boolean): Run {
  const b = new LandBattle(t) as Sim;
  b.ai.att = b.ai.def = true;
  if (ship) shipping(b);
  if (arm === 'on') SWITCH[SW](b);
  if (arm === 'placebo') b.rng.next();
  // (the like for like reading: the switch off, the foot stopping overruns by the overrun's own "Units decide" field)
  if (arm === 'like') for (const u of b.allBlocks()) if ((LAND.alone.foot as readonly string[]).includes(u.kind)) u.kit = { ...(u.kit ?? {}), at: true };
  const H = harm0();
  watch(b, H, arm === 'on');
  runOut(b);
  fought++;
  return finish(b, H);
}

function fightPiece(key: string, seed: number, arm: Arm): Run {
  const L = livePiece(key, seed, { me: null, platoons: false, ...(arm === 'on' && SW === 'pierce' ? { pierce: true } : {}) });
  const b = L.sim as Sim;
  // (looks only: the fight never reads the drill, and a battle drawn ends as one undrawn)
  b.drill = null;
  if (arm === 'on' && SW !== 'pierce') SWITCH[SW](b);
  if (arm === 'placebo') b.rng.next();
  const H = harm0();
  watch(b, H, arm === 'on');
  while (!b.over) b.step();
  fought++;
  return finish(b, H);
}

// ---- Tallies and words ----------------------------------------------------------------------------------------------

interface Cell { n: number; won: number; secs: number; clock: number; la: number; ld: number; harm: Harm }
const cell0 = (): Cell => ({ n: 0, won: 0, secs: 0, clock: 0, la: 0, ld: 0, harm: harm0() });
const addRun = (c: Cell, r: Run): void => { c.n++; c.won += +r.won; c.secs += r.secs; c.clock += +r.clock; c.la += r.la; c.ld += r.ld; addHarm(c.harm, r.harm); };
const addCell = (a: Cell, c: Cell): void => { a.n += c.n; a.won += c.won; a.secs += c.secs; a.clock += c.clock; a.la += c.la; a.ld += c.ld; addHarm(a.harm, c.harm); };
const pc = (x: number, d = 1): string => `${(x * 100).toFixed(d)}%`;
const pts = (x: number): string => `${x >= 0 ? '+' : ''}${(x * 100).toFixed(1)}`;
const say = (s: string): void => { console.log(s); };
const marks: { mark: string; set: string; pass: boolean | null; words: string }[] = [];
const quick = (): string => (SEEDS ? ` (quick look, ${SEEDS} seeds a cell: no verdict)` : '');
function mark(m: string, set: string, pass: boolean | null, words: string): void {
  marks.push({ mark: m, set, pass, words });
  say(`  ${pass === null ? 'READING' : pass ? 'PASS' : 'FAIL'} ${m} (${set}): ${words}${quick()}`);
}
function harmWords(H: Harm): string {
  const tot = Object.values(H.by).reduce((a, v) => a + v, 0) || 1e-9;
  const groups: Record<string, number> = {};
  for (const [k, v] of Object.entries(H.by)) put(groups, short(k), v);
  const g = Object.entries(groups).sort((a, c) => c[1] - a[1]).map(([k, v]) => `${k} ${pc(v / tot)}`).join(', ');
  const foot = Object.entries(H.by).filter(([k]) => k.startsWith('foot')).sort().map(([k, v]) => `${k} ${pc(v / tot)}`).join(', ');
  const far = pc((H.by['field guns beyond 160 m'] ?? 0) / tot);
  const models = Object.values(H.models).reduce((a, v) => a + v, 0);
  return `tank strength lost ${pc(H.lost / Math.max(1e-9, H.brought))} of that brought: ${g} | ${foot || 'foot none'} | field guns beyond 160 m ${far} | tank models lost ${models}: ${Object.entries(H.models).sort((a, c) => c[1] - a[1]).map(([k, v]) => `${k} ${v}`).join(', ') || 'none'}`;
}
const out = (set: string, data: unknown): void => { if (JSON_OUT) appendFileSync(JSON_OUT, JSON.stringify({ set, switch: SW, seeds: SEEDS || 'ruled', at: new Date().toISOString(), data }) + '\n'); };

// ---- S1: the standing gate with the switch on -------------------------------------------------------------------------

function s1(): void {
  const arm = S1_ARM, runs = SEEDS || 24;
  say(`\n== S1: the standing gate, ${SW} ${arm} (bare rules, the sample kit)`);
  const cache = new Map<string, Run>();
  const at = (g: Terrain, odds: number, s: number): Run => {
    const k = `${g} ${odds} ${s}`;
    let r = cache.get(k);
    if (!r) cache.set(k, (r = fight(sampleTicket(g, odds, { seed: seedOf(s) }), arm, false)));
    return r;
  };
  // Fair to the dice (tests/battle-land.test.ts, ruling 217(3)): the mean gap to the dice's chance over the odds.
  const ODDS = [0.8, 1.0, 1.1, 1.2, 1.3, 1.4, 1.6, 1.8];
  const gaps: Record<string, number> = {}, bias: Record<string, number> = {};
  const all = cell0();
  for (const g of GROUNDS7) {
    let gap = 0, lean = 0; const row: string[] = [];
    for (const odds of ODDS) {
      let won = 0;
      for (let s = 1; s <= runs; s++) { const r = at(g, odds, s); won += +r.won; const c = cell0(); addRun(c, r); addCell(all, c); }
      const card = diceChance(odds, [0.8, 1.2]);
      gap += Math.abs(won / runs - card); lean += won / runs - card;
      row.push(`${odds}: ${(100 * won / runs).toFixed(0)} v ${(100 * card).toFixed(0)}`);
    }
    gaps[g] = gap / ODDS.length; bias[g] = lean / ODDS.length;
    say(`  ${g}: gap ${gaps[g].toFixed(3)} bias ${bias[g] >= 0 ? '+' : ''}${bias[g].toFixed(3)} | field v card by odds: ${row.join(', ')}`);
  }
  const mean = Object.values(gaps).reduce((a, v) => a + v, 0) / GROUNDS7.length;
  const worst = Object.entries(gaps).sort((a, c) => c[1] - a[1])[0];
  say(`  ${all.n} battles; mean ${(all.secs / all.n).toFixed(0)} s; to the clock ${all.clock}; lost att ${pc(all.la / all.n)} def ${pc(all.ld / all.n)}`);
  say(`  ${harmWords(all.harm)}`);
  mark('M1 fair to the dice', 'S1', arm === 'on' ? worst[1] <= 0.13 && mean <= 0.1 : null, `worst ground ${worst[0]} ${worst[1].toFixed(3)} (bar 0.13), mean of the seven ${mean.toFixed(3)} (bar 0.10)`);
  // The cost test (b356): mean losses against the bill for the R handed back, winners and losers.
  const clamp = (x: number, a: number, c: number): number => Math.max(a, Math.min(c, x));
  const billAtt = (R: number): number => clamp(0.14 / R, 0.02, 0.2);
  const billDef = (R: number): number => clamp(0.07 + 0.1 * R, 0.06, 0.45) * (R >= 1.2 ? 1 : 0.6);
  const cells = { attWon: { a: 0, d: 0, n: 0 }, defWon: { a: 0, d: 0, n: 0 } };
  let n = 0, inA = 0, inD = 0;
  for (const g of GROUNDS7) for (const odds of [0.8, 1.0, 1.2, 1.4, 1.8, 2.5]) for (let s = 1; s <= Math.min(4, runs); s++) {
    const r = at(g, odds, s), c = cells[r.won ? 'attWon' : 'defWon'];
    c.a += r.la / billAtt(r.R); c.d += r.ld / billDef(r.R); c.n++;
    const lo = odds * 0.8, hi = odds * 1.2, ds = [billDef(lo), billDef(hi), billDef(clamp(1.2, lo, hi))];
    if (r.la >= billAtt(hi) * 0.75 && r.la <= billAtt(lo) * 1.25) inA++;
    if (r.ld >= Math.min(...ds) * 0.75 && r.ld <= Math.max(...ds) * 1.25) inD++;
    n++;
  }
  const ratios = [cells.attWon.a / cells.attWon.n, cells.attWon.d / cells.attWon.n, cells.defWon.a / cells.defWon.n, cells.defWon.d / cells.defWon.n];
  const costOk = ratios.every((x) => x > 0.8 && x < 1.25) && cells.attWon.n > 20 && cells.defWon.n > 20 && inA / n >= 0.7 && inD / n >= 0.7;
  mark('M1 cost', 'S1', arm === 'on' ? costOk : null, `losses over the bill: attack won (${cells.attWon.n}) att ${ratios[0].toFixed(2)} def ${ratios[1].toFixed(2)}; defence won (${cells.defWon.n}) att ${ratios[2].toFixed(2)} def ${ratios[3].toFixed(2)} (bars 0.8 to 1.25); inside the dice's run of bills att ${pc(inA / n, 0)} def ${pc(inD / n, 0)} (bar 70%)`);
  // Mark 2 (ruling 226): fight time from the first contact, in the test's own form (odds 0.8, 1.0, 1.2 and 1.5, 3 seeds).
  const fights: number[] = []; let unfought = 0, unmet = 0;
  for (const g of GROUNDS7) for (const odds of [0.8, 1.0, 1.2, 1.5]) for (let s = 1; s <= Math.min(3, runs); s++) { const r = at(g, odds, s); if (r.fight !== null) fights.push(r.fight); else unmet++; if (!r.halfFought) unfought++; }
  fights.sort((a, c) => a - c);
  const q = (xs: number[], p: number): number => xs[Math.floor(p * (xs.length - 1))];
  mark('M1 mark 2 fight time', 'S1', arm === 'on' ? q(fights, 0.05) >= 90 && q(fights, 0.5) >= 120 && q(fights, 0.5) <= 240 && unfought === 0 && unmet === 0 : null, `the test's own ${fights.length + unmet} battles: 5th percentile ${q(fights, 0.05).toFixed(0)} s (bar 90), median ${q(fights, 0.5).toFixed(0)} s (bar 120 to 240); armies broken with under half their power in the fight ${unfought}; battles with no contact ${unmet}`);
  const allFights = [...cache.values()].map((r) => r.fight).filter((x): x is number => x !== null).sort((a, c) => a - c);
  mark('mark 2 over every S1 battle', 'S1', null, `${allFights.length} battles at all the table's odds (0.8 to 2.5): 5th percentile ${q(allFights, 0.05).toFixed(0)} s, median ${q(allFights, 0.5).toFixed(0)} s`);
  out('S1', { arm, gaps, bias, mean, cost: { cells, inA: inA / n, inD: inD / n }, fights: { p5: q(fights, 0.05), median: q(fights, 0.5), n: fights.length, unfought }, allFights: { p5: q(allFights, 0.05), median: q(allFights, 0.5), n: allFights.length }, battles: cache.size, harm: all.harm });
}

// ---- S2 and S3: the paired sets -----------------------------------------------------------------------------------

interface Pairs { cells: { name: string; off: Cell; on: Cell; pla: Cell; like: Cell; up: number; down: number; pUp: number; pDown: number; lUp: number; lDown: number }[] }

function paired(set: string, grounds: Terrain[], oddsList: number[], seeds: number, teams: boolean, ship: boolean): void {
  for (const mir of MIRRORS) {
    const side = mir ? 'tanks defend' : 'tanks attack';
    say(`\n== ${set} ${side}: ${grounds.join(', ')} x odds ${oddsList.join(', ')} x ${seeds} seeds x 3 arms, ${teams ? 'with teams' : 'no teams'}, ${ship ? 'the shipping switches' : 'bare rules'}; switch ${SW}`);
    const P2: Pairs = { cells: [] };
    for (const g of grounds) for (const odds of oddsList) {
      const c = { name: `${g} @${odds}`, off: cell0(), on: cell0(), pla: cell0(), like: cell0(), up: 0, down: 0, pUp: 0, pDown: 0, lUp: 0, lDown: 0 };
      for (let s = 1; s <= seeds; s++) {
        const base = sampleTicket(g, odds, { seed: seedOf(s) });
        const t = ((x) => (teams ? withTeams(x) : x))(mir ? mirror(base) : base);
        const a = fight(t, 'off', ship), o = fight(t, 'on', ship), p = fight(t, 'placebo', ship);
        addRun(c.off, a); addRun(c.on, o); addRun(c.pla, p);
        if (a.won !== o.won) { if (o.won) c.up++; else c.down++; }
        if (a.won !== p.won) { if (p.won) c.pUp++; else c.pDown++; }
        if (ship && LIKE) { const l = fight(t, 'like', ship); addRun(c.like, l); if (l.won !== o.won) { if (o.won) c.lUp++; else c.lDown++; } }
      }
      P2.cells.push(c);
      say(`  ${c.name} card ${pc(diceChance(odds, [0.8, 1.2]), 0)} | off ${pc(c.off.won / seeds, 0)} on ${pc(c.on.won / seeds, 0)} (${pts((c.on.won - c.off.won) / seeds)}; flips to att ${c.up}, to def ${c.down}) | placebo ${pc(c.pla.won / seeds, 0)} (${pts((c.pla.won - c.off.won) / seeds)}; flips ${c.pUp}, ${c.pDown}) | secs ${(c.off.secs / seeds).toFixed(0)} > ${(c.on.secs / seeds).toFixed(0)} (placebo ${(c.pla.secs / seeds).toFixed(0)}) | clock ${c.off.clock} > ${c.on.clock} (placebo ${c.pla.clock})`);
    }
    const off = cell0(), on = cell0(), pla = cell0();
    let up = 0, down = 0, pUp = 0, pDown = 0, worst = 0, pWorst = 0, worstName = '', pWorstName = '';
    for (const c of P2.cells) {
      addCell(off, c.off); addCell(on, c.on); addCell(pla, c.pla); up += c.up; down += c.down; pUp += c.pUp; pDown += c.pDown;
      const d = (c.on.won - c.off.won) / c.off.n, pd = (c.pla.won - c.off.won) / c.off.n;
      if (Math.abs(d) > Math.abs(worst)) { worst = d; worstName = c.name; }
      if (Math.abs(pd) > Math.abs(pWorst)) { pWorst = pd; pWorstName = c.name; }
    }
    const n = off.n, lean = (on.won - off.won) / n, pLean = (pla.won - off.won) / n, net = Math.abs(up - down) / n, pNet = Math.abs(pUp - pDown) / n;
    say(`  ALL ${n} pairs | off ${pc(off.won / n)} on ${pc(on.won / n)} (${pts(lean)}; flips to att ${up}, to def ${down}) | placebo ${pc(pla.won / n)} (${pts(pLean)}; flips ${pUp}, ${pDown}) | secs ${(off.secs / n).toFixed(1)} > ${(on.secs / n).toFixed(1)} (placebo ${(pla.secs / n).toFixed(1)}) | clock ${off.clock} > ${on.clock} (placebo ${pla.clock}) | lost att ${pc(off.la / n)} > ${pc(on.la / n)}, def ${pc(off.ld / n)} > ${pc(on.ld / n)}`);
    say(`  off: ${harmWords(off.harm)}`);
    say(`  on:  ${harmWords(on.harm)}`);
    // A paired mark failed no worse than the placebo fails it on the same set is chance.
    const judge = (v: number, bar: number, pv: number): { pass: boolean; why: string } =>
      v <= bar + 1e-9 ? { pass: true, why: '' } : v <= pv + 1e-9 ? { pass: true, why: ' (over the bar, but no worse than the placebo: chance)' } : { pass: false, why: '' };
    const j1 = judge(Math.abs(lean), 0.08, Math.abs(pLean)), j2 = judge(net, 0.08, pNet), j3 = judge(Math.abs(worst), 0.15, Math.abs(pWorst));
    mark('M2 lean', `${set} ${side}`, j1.pass, `on less off ${pts(lean)} points (bar 8; placebo ${pts(pLean)})${j1.why}`);
    mark('M2 net flips', `${set} ${side}`, j2.pass, `${up} to the attack, ${down} to the defence: net ${pc(net)} of ${n} pairs (bar 8%; placebo ${pUp} and ${pDown}, net ${pc(pNet)})${j2.why}`);
    mark('M2 worst cell', `${set} ${side}`, j3.pass, `${worstName} ${pts(worst)} points (bar 15; placebo's worst ${pWorstName} ${pts(pWorst)})${j3.why}`);
    const dClock = (on.clock - off.clock) / n, pClock = (pla.clock - off.clock) / n, dSecs = (on.secs - off.secs) / n, pSecs = (pla.secs - off.secs) / n;
    const j4 = judge(dClock, 0.05, pClock), j5 = judge(dSecs, 10, pSecs);
    mark('M3 clock endings', `${set} ${side}`, j4.pass, `${off.clock} off, ${on.clock} on: ${pts(dClock)}% of ${n} battles (bar +5%; placebo ${pla.clock})${j4.why}`);
    mark('M3 battle time', `${set} ${side}`, j5.pass, `${(off.secs / n).toFixed(1)} s off, ${(on.secs / n).toFixed(1)} s on: ${dSecs >= 0 ? '+' : ''}${dSecs.toFixed(1)} s (bar +10 s; placebo ${pSecs >= 0 ? '+' : ''}${pSecs.toFixed(1)} s)${j5.why}`);
    if (SW === 'pierce') {
      const H = on.harm;
      mark('M6 machine-gun teams', `${set} ${side}`, H.mgHarm === 0, `take ${H.mgHarm === 0 ? 'exactly 0' : H.mgHarm} of tank strength with the switch on (off: ${pc((off.harm.by['machine-gun teams'] ?? 0) / Math.max(1e-9, off.harm.lost), 2)} of tank loss)`);
      mark('M6 foot beyond launcher range', `${set} ${side}`, H.beyond === 0 && H.odd === 0, `${H.footBlows} foot blows on tanks: ${H.beyond} beyond ${P.hand.at} m above their guns' share (largest share there ${H.maxBeyond.toFixed(2)}, guns.most ${P.guns.most}); ${H.odd} whose rest was not 1 less the share`);
    }
    const far = (H: Harm): number => (H.by['field guns beyond 160 m'] ?? 0) / Math.max(1e-9, H.lost);
    mark('field guns beyond 160 m, share of tank loss', `${set} ${side}`, null, `off ${pc(far(off.harm))}, on ${pc(far(on.harm))}`);
    if (ship) mark('overruns', `${set} ${side}`, null, `off ${off.harm.overruns}, on ${on.harm.overruns}, placebo ${pla.harm.overruns} in ${n} battles`);
    const like = cell0(); let lUp = 0, lDown = 0, lWorst = 0, lWorstName = '';
    if (ship && LIKE) {
      for (const c of P2.cells) {
        addCell(like, c.like); lUp += c.lUp; lDown += c.lDown;
        const d = (c.on.won - c.like.won) / c.like.n;
        if (Math.abs(d) > Math.abs(lWorst)) { lWorst = d; lWorstName = c.name; }
      }
      say(`  off, the foot stopping overruns: attacker won ${pc(like.won / n)} | secs ${(like.secs / n).toFixed(1)} | clock ${like.clock} | overruns ${like.harm.overruns} | ${harmWords(like.harm)}`);
      mark('like for like, no bar', `${set} ${side}`, null, `against the switch off with its foot stopping overruns (${like.harm.overruns} overruns, ${pc(like.won / n)} to the attack): on less that arm ${pts((on.won - like.won) / n)} points; flips ${lUp} to the attack, ${lDown} to the defence: net ${pc(Math.abs(lUp - lDown) / n)}; worst cell ${lWorstName} ${pts(lWorst)} points; clock ${like.clock} against ${on.clock}: ${pts((on.clock - like.clock) / n)}%; time ${(like.secs / n).toFixed(1)} s against ${(on.secs / n).toFixed(1)} s`);
    }
    out(`${set} ${side}`, { ...(ship && LIKE ? { like, lUp, lDown, lWorst: { cell: lWorstName, d: lWorst } } : {}), n, off, on, pla, up, down, pUp, pDown, lean, pLean, net, pNet, worst: { cell: worstName, d: worst }, pWorst: { cell: pWorstName, d: pWorst }, cells: P2.cells.map((c) => ({ name: c.name, off: c.off.won, on: c.on.won, pla: c.pla.won, n: c.off.n, up: c.up, down: c.down, pUp: c.pUp, pDown: c.pDown, clock: [c.off.clock, c.on.clock, c.pla.clock], secs: [c.off.secs / c.off.n, c.on.secs / c.on.n, c.pla.secs / c.pla.n] })) });
  }
}

// ---- S4: kit lean (the switch on only) -------------------------------------------------------------------------------

function s4(): void {
  const seeds = SEEDS || 30, withPla = flag('placebo');
  for (const mir of MIRRORS) {
    const side = mir ? 'tanks defend' : 'tanks attack';
    say(`\n== S4 ${side}: kit lean, ${SW} on; ${GROUNDS4.join(', ')} x odds 1.2 x ${seeds} seeds, with teams, bare rules; the side without tanks at the plain kit, then the full kit; the side with tanks at the sample kit`);
    const plain = cell0(), full = cell0(), pla = cell0();
    let worst = 0, worstName = '', pWorst = 0, up = 0, down = 0;
    const byGround: Record<string, { plain: Cell; full: Cell }> = {};
    for (const g of GROUNDS4) {
      const cp = cell0(), cf = cell0(), cx = cell0();
      for (let s = 1; s <= seeds; s++) {
        // (the sample's attack has the tanks: the kit under test is its defence's, and goes with those corps into the mirror)
        const make = (kit: ForceKit): Ticket => { const base = sampleTicket(g, 1.2, { seed: seedOf(s), kit: { att: SAMPLE_KIT, def: kit } }); return withTeams(mir ? mirror(base) : base); };
        const a = fight(make({}), 'on', false), f = fight(make(FULL_KIT), 'on', false);
        addRun(cp, a); addRun(cf, f);
        if (a.won !== f.won) { if (f.won) up++; else down++; }
      }
      // (the set's own chance floor: the plain kit again, the switch on, the luck re-dealt by one throwaway draw)
      if (withPla) for (let s = 1; s <= seeds; s++) {
        const base = sampleTicket(g, 1.2, { seed: seedOf(s), kit: { att: SAMPLE_KIT, def: {} } });
        const t = withTeams(mir ? mirror(base) : base);
        const b = new LandBattle(t) as Sim; b.ai.att = b.ai.def = true; SWITCH[SW](b); b.rng.next();
        const H = harm0(); watch(b, H, true); runOut(b); fought++;
        addRun(cx, finish(b, H));
      }
      byGround[g] = { plain: cp, full: cf };
      const d = (cf.won - cp.won) / seeds, pd = withPla ? (cx.won - cp.won) / seeds : 0;
      if (Math.abs(d) > Math.abs(worst)) { worst = d; worstName = g; }
      if (Math.abs(pd) > Math.abs(pWorst)) pWorst = pd;
      addCell(plain, cp); addCell(full, cf); addCell(pla, cx);
      say(`  ${g}: attacker wins plain kit ${pc(cp.won / seeds, 0)}, full kit ${pc(cf.won / seeds, 0)} (${pts(d)})${withPla ? `, plain kit placebo ${pc(cx.won / seeds, 0)} (${pts(pd)})` : ''} | foot's share of the tanks' loss: plain ${pc(cp.harm.side[mir ? 'def' : 'att'].foot / Math.max(1e-9, cp.harm.side[mir ? 'def' : 'att'].lost))}, full ${pc(cf.harm.side[mir ? 'def' : 'att'].foot / Math.max(1e-9, cf.harm.side[mir ? 'def' : 'att'].lost))}`);
    }
    const n = plain.n, lean = (full.won - plain.won) / n, pLean = withPla ? (pla.won - plain.won) / n : NaN;
    say(`  ALL ${n} pairs | plain ${pc(plain.won / n)} full ${pc(full.won / n)} (${pts(lean)}; flips to att ${up}, to def ${down})${withPla ? ` | plain kit placebo ${pc(pla.won / n)} (${pts(pLean)})` : ''} | secs ${(plain.secs / n).toFixed(1)} v ${(full.secs / n).toFixed(1)} | clock ${plain.clock} v ${full.clock}`);
    say(`  plain kit: ${harmWords(plain.harm)}`);
    say(`  full kit:  ${harmWords(full.harm)}`);
    const chance = (v: number, bar: number, pv: number): { pass: boolean; why: string } =>
      v <= bar + 1e-9 ? { pass: true, why: '' } : withPla && v <= pv + 1e-9 ? { pass: true, why: ' (over the bar, but no worse than the placebo: chance)' } : { pass: false, why: withPla ? '' : ' (no placebo run: --placebo gives this set its chance floor)' };
    const j1 = chance(Math.abs(lean), 0.05, Math.abs(pLean)), j2 = chance(Math.abs(worst), 0.12, Math.abs(pWorst));
    mark('M5 kit lean (b418)', `S4 ${side}`, j1.pass, `full kit less plain kit ${pts(lean)} points (bar 5${withPla ? `; placebo ${pts(pLean)}` : ''})${j1.why}`);
    mark('M5 worst ground', `S4 ${side}`, j2.pass, `${worstName} ${pts(worst)} points (bar 12${withPla ? `; placebo's worst ${pts(pWorst)}` : ''})${j2.why}`);
    if (SW === 'pierce') {
      mark('M6 machine-gun teams', `S4 ${side}`, plain.harm.mgHarm === 0 && full.harm.mgHarm === 0, `take ${plain.harm.mgHarm + full.harm.mgHarm === 0 ? 'exactly 0' : plain.harm.mgHarm + full.harm.mgHarm} of tank strength`);
      mark('M6 foot beyond launcher range', `S4 ${side}`, plain.harm.beyond + full.harm.beyond + plain.harm.odd + full.harm.odd === 0, `${plain.harm.footBlows + full.harm.footBlows} foot blows on tanks: ${plain.harm.beyond + full.harm.beyond} beyond ${P.hand.at} m above their guns' share (largest share there: plain ${plain.harm.maxBeyond.toFixed(2)}, full ${full.harm.maxBeyond.toFixed(2)}); ${plain.harm.odd + full.harm.odd} whose rest was not 1 less the share`);
      if (!mir) {
        // Foot is not helpless: on forest and urban, the tanks attacking, the defence's foot's share of the attacking tanks' loss.
        const share = (k: 'plain' | 'full'): number => { let foot = 0, lost = 0; for (const g of ['forest', 'urban']) { const h = byGround[g]?.[k].harm.side.att; if (h) { foot += h.foot; lost += h.lost; } } return foot / Math.max(1e-9, lost); };
        const each = (k: 'plain' | 'full'): string => ['forest', 'urban'].map((g) => { const h = byGround[g][k].harm.side.att; return `${g} ${pc(h.foot / Math.max(1e-9, h.lost))}`; }).join(', ');
        mark('M6 foot not helpless, full kit', 'S4 tanks attack', share('full') >= 0.2, `foot takes ${pc(share('full'))} of the attacking tanks' loss on forest and urban (${each('full')}; bar 20%)`);
        mark('M6 foot not helpless, plain kit', 'S4 tanks attack', share('plain') >= 0.1, `foot takes ${pc(share('plain'))} of the attacking tanks' loss on forest and urban (${each('plain')}; bar 10%)`);
      }
    }
    out(`S4 ${side}`, { n, plain, full, pla: withPla ? pla : null, lean, pLean, worst: { ground: worstName, d: worst }, up, down, byGround: Object.fromEntries(Object.entries(byGround).map(([g, v]) => [g, { plain: v.plain.won, full: v.full.won, n: v.plain.n, footPlain: v.plain.harm.side, footFull: v.full.harm.side }])) });
  }
}

// ---- S5: the set pieces ---------------------------------------------------------------------------------------------

function s5(): void {
  // (a piece with fewer seeds of its own takes the next of Normandy's list, which it has not used)
  const more = [61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107];
  for (const key of ['normandy-1944', 'prokhorovka-1943']) {
    const p = PIECES.find((x) => x.key === key)!;
    const seeds = [...p.seeds, ...more.filter((s) => !p.seeds.includes(s))].slice(0, SEEDS || 24);
    say(`\n== S5 ${key}: ${seeds.length} seeds x 3 arms, as the game starts it (livePiece), the drill removed; switch ${SW}`);
    const off = cell0(), on = cell0(), pla = cell0();
    let up = 0, down = 0, pUp = 0, pDown = 0;
    for (const s of seeds) {
      const a = fightPiece(key, s, 'off'), o = fightPiece(key, s, 'on'), x = fightPiece(key, s, 'placebo');
      addRun(off, a); addRun(on, o); addRun(pla, x);
      if (a.won !== o.won) { if (o.won) up++; else down++; }
      if (a.won !== x.won) { if (x.won) pUp++; else pDown++; }
    }
    const n = off.n, tl = (c: Cell): number => c.harm.lost / Math.max(1e-9, c.harm.brought);
    say(`  attacker won off ${off.won}, on ${on.won}, placebo ${pla.won} of ${n} | flips to att ${up}, to def ${down} (placebo ${pUp}, ${pDown}) | secs ${(off.secs / n).toFixed(0)} > ${(on.secs / n).toFixed(0)} (placebo ${(pla.secs / n).toFixed(0)}) | clock ${off.clock} > ${on.clock} (placebo ${pla.clock})`);
    say(`  off: ${harmWords(off.harm)}`);
    say(`  on:  ${harmWords(on.harm)}`);
    mark('M4 attacker wins', `S5 ${key}`, Math.abs(on.won - off.won) <= 4, `${off.won} off, ${on.won} on of ${n} (bar: within 4; placebo ${pla.won})`);
    mark('M4 flips', `S5 ${key}`, up + down <= 5, `${up + down} of ${n} flipped: ${up} to the attack, ${down} to the defence (bar: at most 5; placebo ${pUp + pDown})`);
    mark('M4 tank strength lost', `S5 ${key}`, tl(on) >= 0.75 * tl(off), `${pc(tl(off))} off, ${pc(tl(on))} on: ${(tl(on) / Math.max(1e-9, tl(off))).toFixed(2)} of off (bar: at least 0.75; placebo ${pc(tl(pla))})`);
    if (SW === 'pierce') {
      mark('M6 machine-gun teams', `S5 ${key}`, on.harm.mgHarm === 0, `take ${on.harm.mgHarm === 0 ? 'exactly 0' : on.harm.mgHarm} of tank strength with the switch on`);
      mark('M6 foot beyond launcher range', `S5 ${key}`, on.harm.beyond === 0 && on.harm.odd === 0, `${on.harm.footBlows} foot blows on tanks: ${on.harm.beyond} beyond ${P.hand.at} m above their guns' share (largest share there ${on.harm.maxBeyond.toFixed(2)}); ${on.harm.odd} whose rest was not 1 less the share`);
    }
    mark('overruns', `S5 ${key}`, null, `off ${off.harm.overruns}, on ${on.harm.overruns} (the game's own start: escort as livePiece opens it)`);
    out(`S5 ${key}`, { n, off, on, pla, up, down, pUp, pDown, tankLost: [tl(off), tl(on), tl(pla)] });
  }
}

// ---- SF: an army all on foot against tanks (a reading, no bar) -------------------------------------------------------

function sf(): void {
  const seeds = SEEDS || 30;
  for (const mir of MIRRORS) for (const [kitName, kit] of [['the plain kit', {}], ['the sample kit', SAMPLE_KIT]] as [string, ForceKit][]) {
    const side = mir ? 'an all-foot attack on tanks defending' : 'an all-foot defence against tanks attacking';
    say(`\n== SF ${side}, its foot at ${kitName}: ${GROUNDS7.join(', ')} x odds 1.2 x ${seeds} seeds x off and on, no teams, the shipping switches; switch ${SW}`);
    const off = cell0(), on = cell0();
    let up = 0, down = 0, worst = 0, worstName = '';
    for (const g of GROUNDS7) {
      const a = cell0(), o = cell0();
      let gu = 0, gd = 0;
      for (let s = 1; s <= seeds; s++) {
        // (the sample's defence is the side without tanks: its guns' corps made a third foot corps of the same share)
        const base = sampleTicket(g, 1.2, { seed: seedOf(s), kit: { att: SAMPLE_KIT, def: kit } });
        const allFoot: Ticket = { ...base, def: { ...base.def, forces: base.def.forces.map((f) => (f.kind === 'art' ? { ...f, kind: 'inf', name: 'XII Corps', divs: 3, men: 3 * 15000 * f.str, ...(Object.keys(kit).length ? { kit: { ...kit } } : {}) } : f)) } };
        const t = mir ? mirror(allFoot) : allFoot;
        const x = fight(t, 'off', true), y = fight(t, 'on', true);
        addRun(a, x); addRun(o, y);
        if (x.won !== y.won) { if (y.won) gu++; else gd++; }
      }
      const d = (o.won - a.won) / seeds;
      if (Math.abs(d) > Math.abs(worst)) { worst = d; worstName = g; }
      addCell(off, a); addCell(on, o); up += gu; down += gd;
      say(`  ${g} card ${pc(diceChance(1.2, [0.8, 1.2]), 0)} | attacker wins off ${a.won}, on ${o.won} of ${seeds} (${pts(d)}; flips to att ${gu}, to def ${gd}) | clock ${a.clock} > ${o.clock} | overruns ${a.harm.overruns} > ${o.harm.overruns}`);
    }
    const n = off.n;
    say(`  off: ${harmWords(off.harm)}`);
    say(`  on:  ${harmWords(on.harm)}`);
    mark('all on foot against tanks, no bar', `SF ${side}, ${kitName}`, null, `attacker wins ${off.won} off, ${on.won} on of ${n} (${pts((on.won - off.won) / n)} points); flips ${up} to the attack, ${down} to the defence: net ${pc(Math.abs(up - down) / n)}; worst ground ${worstName} ${pts(worst)}; clock ${off.clock} off, ${on.clock} on; overruns ${off.harm.overruns} off, ${on.harm.overruns} on; tank strength lost ${pc(off.harm.lost / Math.max(1e-9, off.harm.brought))} off, ${pc(on.harm.lost / Math.max(1e-9, on.harm.brought))} on`);
    if (SW === 'pierce') {
      mark('M6 machine-gun teams', `SF ${side}, ${kitName}`, on.harm.mgHarm === 0, `take ${on.harm.mgHarm === 0 ? 'exactly 0' : on.harm.mgHarm} of tank strength with the switch on`);
      mark('M6 foot beyond launcher range', `SF ${side}, ${kitName}`, on.harm.beyond === 0 && on.harm.odd === 0, `${on.harm.footBlows} foot blows on tanks, those on a tank driving through the block too: ${on.harm.beyond} beyond ${P.hand.at} m above their guns' share (largest share there ${on.harm.maxBeyond.toFixed(2)}); ${on.harm.odd} whose rest was not 1 less the share`);
    }
    out(`SF ${side}, ${kitName}`, { n, off, on, up, down, worst: { ground: worstName, d: worst } });
  }
}

// ---- Run ------------------------------------------------------------------------------------------------------------

const t0 = Date.now();
for (const set of SETS) {
  if (set === 'S1') s1();
  else if (set === 'S2') paired('S2', GROUNDS4, [1.2, 1.3], SEEDS || 30, true, true);
  else if (set === 'S3') paired('S3', GROUNDS7, [1.2], SEEDS || 24, false, false);
  else if (set === 'S4') s4();
  else if (set === 'S5') s5();
  else if (set === 'SF') sf();
  else throw new Error(`no set ${set}`);
}
say(`\n== The marks (${SW}${quick()})`);
for (const m of marks) say(`${m.pass === null ? 'READING' : m.pass ? 'PASS   ' : 'FAIL   '} ${m.mark} | ${m.set} | ${m.words}`);
const judged = marks.filter((m) => m.pass !== null), failed = judged.filter((m) => !m.pass);
say(`${judged.length} marks judged in these sets: ${judged.length - failed.length} pass, ${failed.length} fail. ${fought} battles in ${((Date.now() - t0) / 1000).toFixed(0)} s (${((Date.now() - t0) / Math.max(1, fought)).toFixed(0)} ms a battle).`);
out('marks', marks);
