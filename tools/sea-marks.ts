// The fleet action's marks, measured: the fairness bars (217(3), b415, b407) and the tactics panel's behaviour marks
// (tactics-panel/briefs/sea.md, ruling 239), with the computer on both sides. Read only: it fights battles and counts.
//
//   npx esbuild tools/sea-marks.ts --bundle --platform=node --format=esm --outfile=dist/sea-marks.mjs --log-level=warning
//   node dist/sea-marks.mjs --sets=parity,marks,b415,b407 --jobs=4 --out=marks.json [--samples=a.json,b.json]
//     [--set='{"torp":{"bank":180}}'] [--styles=bold:methodical] [--grounds=heavy,light] [--luck=7919]
//
// Sets: 'parity' is battle-sea-parity.test.ts's eight grounds at odds 0.9, 1 and 1.15, 40 even-luck seeds each (the
// win gap and the totals each side dealt); 'marks' is the same grounds at even odds over 100 seeds (the behaviour
// marks); 'b415' is the calibration grounds at 100 seeds; 'b407' is the campaign's own actions by tenth of size at
// telling odds, from ticket samples (--samples). '--set' lays numbers over data/battle/sea.json for tuning; '--styles'
// sets each side's style where the ticket takes one.
import { fork } from 'node:child_process';
import { readFileSync, writeFileSync, unlinkSync } from 'node:fs';
import { runOut } from '../src/battle/core/clock';
import type { Side } from '../src/battle/core/types';
import { SEA } from '../src/battle/sea/numbers';
import { PRACTICE } from '../src/battle/sea/practice';
import { seaChance } from '../src/battle/sea/result';
import { SeaSim } from '../src/battle/sea/sim';
import { plainTicket, type PlainFleet } from '../src/battle/sea/ticket';
import type { SeaTicket } from '../src/battle/sea/types';
import { evenLuck } from '../tests/sea-luck';

const arg = (k: string, d = ''): string => process.argv.find((a) => a.startsWith(`--${k}=`))?.slice(k.length + 3) ?? d;
/** Where the seeds' search starts (evenLuck's base): another base draws another set of battles, to tell a mark's noise from its lean. */
const LUCK = Number(arg('luck', '7919'));

const GUNS_A: PlainFleet = { name: 'Home Fleet', nat: 'UK', ships: { bb: 3, ca: 4, dd: 8 } };
const GUNS_B: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 4, dd: 12 } };
const CARRIERS: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 2, dd: 8 } };
const BATTLE_LINE: PlainFleet = { name: 'Battle Force', nat: 'USA', ships: { bb: 2, ca: 3, dd: 8 } };
const HEAVY_LINE: PlainFleet = { name: 'Atlantic Fleet', nat: 'USA', ships: { bb: 4, ca: 4, dd: 8 } };
const LIGHT_FLEET: PlainFleet = { name: '2nd Fleet', nat: 'ITA', ships: { ca: 7, dd: 25 } };
const OA: PlainFleet = { name: 'Home Fleet', nat: 'UK', ships: { bb: 2, ca: 4, dd: 10 } };
const OB: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 3, ca: 3, dd: 10 } };
const USCV: PlainFleet = { name: 'Atlantic Fleet', nat: 'USA', ships: { bb: 4, cv: 1, ca: 4, dd: 20 } };
const CV_A: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { cv: 2, ca: 2, dd: 8 } };
const CV_B: PlainFleet = { name: 'Task Force', nat: 'USA', ships: { cv: 2, ca: 3, dd: 6 } };

interface Ground { key: string; att: PlainFleet[]; def: PlainFleet[]; air?: Partial<Record<Side, number>>; odds?: number[] }
const T = [0.9, 1, 1.15], E = [1];
/** battle-sea-parity.test.ts's eight grounds, and the carriers' ground (item 3). */
const PARITY: Ground[] = [
  { key: 'heavy', att: [GUNS_A], def: [GUNS_B] }, { key: 'light', att: [GUNS_B], def: [GUNS_A] },
  { key: 'cvatt', att: [CARRIERS], def: [BATTLE_LINE] }, { key: 'lineatt', att: [BATTLE_LINE], def: [CARRIERS] },
  { key: 'bbl', att: [HEAVY_LINE], def: [LIGHT_FLEET] }, { key: 'bblr', att: [LIGHT_FLEET], def: [HEAVY_LINE] },
  { key: 'airatt', att: [GUNS_A], def: [GUNS_B], air: { att: 10 } }, { key: 'airdef', att: [GUNS_A], def: [GUNS_B], air: { def: 15 } },
  { key: 'cvcv', att: [CV_A], def: [CV_B] }, { key: 'cvcvr', att: [CV_B], def: [CV_A] },
];
/** b415's calibration grounds (naval-battles/b415-b407-numbers.md, b458-numbers.md). */
const B415: Ground[] = [
  { key: 'mirror', att: [GUNS_A], def: [{ ...GUNS_A, nat: 'GER' }], odds: E },
  { key: 'heavy', att: [GUNS_A], def: [GUNS_B], odds: E }, { key: 'light', att: [GUNS_B], def: [GUNS_A], odds: E },
  { key: 'outgun', att: [OA], def: [OB], odds: E }, { key: 'outgunr', att: [OB], def: [OA], odds: E },
  { key: 'bbl', att: [HEAVY_LINE], def: [LIGHT_FLEET], odds: E }, { key: 'bblr', att: [LIGHT_FLEET], def: [HEAVY_LINE], odds: E },
  { key: 'uscv', att: [USCV], def: [LIGHT_FLEET], odds: E }, { key: 'uscvr', att: [LIGHT_FLEET], def: [USCV], odds: E },
  { key: 'p1even', att: PRACTICE[1].att, def: PRACTICE[1].def, odds: E }, { key: 'p1rev', att: PRACTICE[1].def, def: PRACTICE[1].att, odds: E },
  { key: 'smallair', att: [{ name: 'Home Fleet', nat: 'UK', ships: { bb: 3, ca: 3, dd: 10 } }], def: [{ name: '2nd Fleet', nat: 'ITA', ships: { dd: 8 } }], air: { def: 30 }, odds: E },
  { key: 'smallairr', att: [{ name: '2nd Fleet', nat: 'ITA', ships: { dd: 8 } }], def: [{ name: 'Home Fleet', nat: 'UK', ships: { bb: 3, ca: 3, dd: 10 } }], air: { att: 30 }, odds: E },
  { key: 'cvatt', att: [CARRIERS], def: [BATTLE_LINE], odds: E }, { key: 'lineatt', att: [BATTLE_LINE], def: [CARRIERS], odds: E },
  { key: 'airatt', att: [GUNS_A], def: [GUNS_B], air: { att: 10 }, odds: E }, { key: 'airdef', att: [GUNS_A], def: [GUNS_B], air: { def: 15 }, odds: E },
  { key: 'cvcv', att: [CV_A], def: [CV_B], odds: E }, { key: 'cvcvr', att: [CV_B], def: [CV_A], odds: E },
  { key: 'big', att: [{ name: 'A', nat: 'UK', ships: { bb: 6, ca: 12, dd: 30 } }, { name: 'B', nat: 'UK', ships: { ca: 5, dd: 18 } }], def: [{ name: 'C', nat: 'GER', ships: { bb: 3, ca: 9, dd: 24 } }], odds: T },
  { key: 'bigr', att: [{ name: 'C', nat: 'GER', ships: { bb: 3, ca: 9, dd: 24 } }], def: [{ name: 'A', nat: 'UK', ships: { bb: 6, ca: 12, dd: 30 } }, { name: 'B', nat: 'UK', ships: { ca: 5, dd: 18 } }], odds: T },
  ...PRACTICE.map((p, i) => ({ key: `p${i}`, att: p.att, def: p.def, air: p.air })),
];

/** One battle to fight: a ticket built from a ground, or a campaign's own. */
interface Job { set: string; key: string; odds: number | null; seed: number; ground?: Ground; ticket?: SeaTicket }

function jobs(): Job[] {
  const want = arg('sets', 'parity,marks,b415').split(','), only = arg('grounds') ? arg('grounds').split(',') : null;
  const out: Job[] = [];
  const add = (set: string, gs: Ground[], odds: number[] | null, n: number): void => {
    for (const g of gs) {
      if (only && !only.includes(g.key)) continue;
      for (const o of g.odds ?? odds ?? [null as unknown as number]) for (const seed of evenLuck(n, LUCK)) out.push({ set, key: g.key, odds: o ?? null, seed, ground: g });
    }
  };
  if (want.includes('parity')) add('parity', PARITY, T, 40);
  if (want.includes('marks')) add('marks', PARITY.map((g) => ({ ...g, odds: E })), E, 100);
  if (want.includes('b415')) add('b415', B415, null, 100);
  if (want.includes('b407')) {
    const ships = (t: SeaTicket): number => t.place.fleets.reduce((n, f) => n + f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd, 0);
    const all: SeaTicket[] = arg('samples').split(',').filter(Boolean).flatMap((f) => JSON.parse(readFileSync(f, 'utf8')).map((x: { ticket: SeaTicket }) => x.ticket));
    const seen = new Set<string>();
    const uniq = all.filter((t) => { const k = JSON.stringify([t.place.fleets.map((f) => [f.side, f.ships, f.mul.toFixed(2), f.deck.toFixed(2)]), t.reckoning.odds.toFixed(2), t.place.air]); if (seen.has(k)) return false; seen.add(k); return true; });
    const Ts = uniq.filter((t) => ships(t) > 0 && t.place.fleets.some((f) => f.side === 'def' && f.ships.bb + f.ships.cv + f.ships.ca + f.ships.dd > 0)).sort((a, b) => ships(a) - ships(b));
    const n = Ts.length, tenth = Math.max(1, Math.round(n / 10));
    const groups: [string, SeaTicket[]][] = [['smallest', Ts.slice(0, tenth)], ['middle', Ts.slice(Math.floor(n / 2 - tenth / 2), Math.floor(n / 2 - tenth / 2) + tenth)], ['largest', Ts.slice(n - tenth)]];
    for (const [name, ts] of groups) for (const t of ts) for (const o of T) {
      const c: SeaTicket = JSON.parse(JSON.stringify(t)), k = t.reckoning.odds / o;
      for (const f of c.place.fleets) if (f.side === 'def') f.mul *= k;
      c.place.air.def *= k; c.reckoning.D *= k; c.reckoning.odds = o;
      for (const seed of evenLuck(20, LUCK)) out.push({ set: 'b407', key: name, odds: o, seed, ticket: c });
    }
  }
  return out;
}

/** What one battle leaves for the marks. */
interface Rec { set: string; key: string; odds: number | null; seed: number; won: number; card: number; how: string; seconds: number; tally: Record<string, number> }

function fight(j: Job): Rec {
  let t: SeaTicket;
  if (j.ticket) { t = JSON.parse(JSON.stringify(j.ticket)); t.seed = j.seed; t.att.human = false; t.def.human = false; }
  else {
    const g = j.ground!;
    const styles = arg('styles') ? arg('styles').split(':') : null;
    const o = { att: g.att, def: g.def, air: g.air, seed: j.seed, odds: j.odds ?? undefined, human: null, ...(styles ? { style: { att: styles[0], def: styles[1] } } : {}) };
    t = plainTicket(o as Parameters<typeof plainTicket>[0]);
  }
  const sim = new SeaSim(t);
  runOut(sim);
  const r = sim.over!;
  return { set: j.set, key: j.key, odds: j.odds, seed: j.seed, won: r.winner === 'att' ? 1 : 0, card: seaChance(t.reckoning.odds), how: r.how, seconds: r.seconds, tally: sim.tally };
}

function lay(o: Record<string, unknown>, into: Record<string, unknown>): void {
  for (const [k, v] of Object.entries(o)) {
    if (v && typeof v === 'object' && !Array.isArray(v)) { into[k] ??= {}; lay(v as Record<string, unknown>, into[k] as Record<string, unknown>); } else into[k] = v;
  }
}

const sum = (rs: Rec[], f: (r: Rec) => number): number => rs.reduce((n, r) => n + f(r), 0);
const k = (r: Rec, key: string): number => r.tally[key] ?? 0;
const SIDES: Side[] = ['att', 'def'];
const dealt = (r: Rec, s: Side): number => ['bb', 'ca', 'dd', 'cv', 'air'].reduce((n, c) => n + k(r, `${s}:${c}`), 0);
const ratio = (a: number, b: number): number | null => (b > 0 ? a / b : null);

/** The marks of a group of battles. */
function marksOf(rs: Rec[]): Record<string, number | null> {
  const n = rs.length, out: Record<string, number | null> = { n, field: sum(rs, (r) => r.won) / n, card: sum(rs, (r) => r.card) / n };
  out.gap = out.field! - out.card!;
  out.time = rs.filter((r) => r.how === 'time').length / n;
  out.seconds = sum(rs, (r) => r.seconds) / n;
  for (const s of SIDES) {
    // Dealt plus paid: a blow that pays for hull its side put out of the line (R2's debt) takes its own weight out of the
    // fight though it lands none (the design director, 03:49Z). Before R2 nothing is paid, so the count is as it was.
    out[`${s}.total`] = sum(rs, (r) => dealt(r, s) + k(r, `${s}:debtPaid`)) / n;
    out[`${s}.B1`] = ratio(sum(rs, (r) => k(r, `${s}:bb@big>big:s`)), sum(rs, (r) => k(r, `${s}:bb@big:s`)));
    out[`${s}.B2bb`] = ratio(sum(rs, (r) => k(r, `${s}:bb:untroubled:s`)), sum(rs, (r) => k(r, `${s}:bb:exposed:s`)));
    out[`${s}.B2ca`] = ratio(sum(rs, (r) => k(r, `${s}:ca:untroubled:s`)), sum(rs, (r) => k(r, `${s}:ca:exposed:s`)));
    out[`${s}.B3`] = sum(rs, (r) => k(r, `${s}:dd>bb`) + k(r, `${s}:dd>cv`)) / n;
    for (const c of ['bb', 'ca', 'dd']) {
      const all = sum(rs, (r) => ['bb', 'ca', 'dd', 'cv'].reduce((m, t) => m + k(r, `${s}:${c}>${t}`), 0));
      for (const t of ['bb', 'ca', 'dd', 'cv']) out[`${s}.${c}>${t}`] = ratio(sum(rs, (r) => k(r, `${s}:${c}>${t}`)), all);
    }
    // Item 2 and 3's later marks, where the sim counts them.
    for (const c of ['bb', 'ca', 'dd', 'cv', 'air']) out[`${s}.${c}`] = sum(rs, (r) => k(r, `${s}:${c}`)) / n;
    out[`${s}.torp`] = sum(rs, (r) => ['bb', 'ca', 'dd', 'cv'].reduce((m, t) => m + k(r, `${s}:torp>${t}`), 0)) / n;
    out[`${s}.B4`] = ratio(sum(rs, (r) => k(r, `${s}:runBattles`)), sum(rs, (r) => k(r, `${s}:runEligible`)));
    out[`${s}.B5far`] = ratio(sum(rs, (r) => k(r, `${s}:launchFar`)), sum(rs, (r) => k(r, `${s}:runs`)));
    out[`${s}.B5late`] = ratio(sum(rs, (r) => k(r, `${s}:turnLate`)), sum(rs, (r) => k(r, `${s}:runs`)));
    out[`${s}.B6`] = ratio(sum(rs, (r) => k(r, `${s}:runsShotByScreen`)), sum(rs, (r) => k(r, `${s}:runsNearScreen`)));
    // Item 4's marks: C1, a carrier's first strike at a carrier where the enemy has one (else at a battleship where it has
    // one); C2, each class's share of the coasts' raids against its expected share (strike.aim times ships afloat).
    out[`${s}.C1cv`] = ratio(sum(rs, (r) => k(r, `${s}:cvFirstAtCv`)), sum(rs, (r) => k(r, `${s}:cvFirstVsCv`)));
    out[`${s}.C1bb`] = ratio(sum(rs, (r) => k(r, `${s}:cvFirstAtBb`)), sum(rs, (r) => k(r, `${s}:cvFirstVsBb`)));
    const raids = sum(rs, (r) => ['bb', 'ca', 'dd', 'cv'].reduce((m, c) => m + k(r, `${s}:raid>${c}`), 0));
    if (raids) {
      out[`${s}.raids`] = raids;
      for (const c of ['bb', 'ca', 'dd', 'cv']) out[`${s}.C2${c}`] = (sum(rs, (r) => k(r, `${s}:raid>${c}`)) - sum(rs, (r) => k(r, `${s}:raidExp>${c}`))) / raids;
    }
    // Step 5's marks: C3, strikes on a side with carriers that met its fighters; C4, weight denied less weight paid, a
    // battle (what the guns still owed at the end is beside it); C5, battles in which a side's carrier came within the
    // guns' reach of an enemy battleship, of those where it faced one.
    out[`${s}.C3`] = ratio(sum(rs, (r) => k(r, `${s}:strikesMet`)), sum(rs, (r) => k(r, `${s}:strikesIn`)));
    const denied = sum(rs, (r) => k(r, `${s}:denied`));
    if (denied) { out[`${s}.C4`] = (denied - sum(rs, (r) => k(r, `${s}:paid`))) / n; out[`${s}.deniedPer`] = denied / n; }
    out[`${s}.C5`] = ratio(sum(rs, (r) => k(r, `${s}:cvNearBb`)), sum(rs, (r) => k(r, `${s}:cvVsBb`)));
    for (const m of ['runs', 'runEligible', 'runBattles', 'runsBroken', 'launchFar', 'turnLate', 'screenLaunch', 'runsNearScreen', 'runsShotByScreen', 'cvFirst', 'cvFirstAtCv', 'cvFirstAtBb', 'strikesMet', 'strikesBelow', 'strikesIn', 'denied', 'paid', 'owed', 'cvNearBb', 'bankLeft', 'ddBig:s', 'ddBigOff:s', 'ddBigFar:s', 'ddGun:s', 'screenFreed', 'owedLeft', 'coverLeft', 'cvVsBb', 'settled', 'coverFlown', 'aloft', 'salvoDrawn', 'salvoPaid', 'salvoUnpaid', 'loadedLeft', 'salvoLanded', 'salvoOnTarget', 'salvoInWater', 'salvoBeforeBreak', 'divisions', 'salvoEarnedRun', 'guardOn',
      // R6's marks (the ship plan's R6; the design director, 05:17Z): combs and comb-seconds, by class, for splitting a miss.
      'combs', 'combs>bb', 'combs>ca', 'combs>cv', 'combWater', 'comb:s', 'comb>bb:s', 'comb>ca:s', 'comb>cv:s', 'combInLine:s', 'combBeforeBreak',
      'combSwitch', 'combSwitchWide', 'combGun:s', 'combSilent:s', 'combEndOut', 'combLost', 'combPaid', 'combLeft', 'combTie', 'combResume', 'combApart:s',
      // R6 item (2)'s (the design director, 19:03Z): seconds a line ship that combed spent working back to her place, and those back.
      'regain:s', 'regained']) {
      const v = sum(rs, (r) => k(r, `${s}:${m}`));
      if (v) out[`${s}.${m}`] = v;
    }
  }
  return out;
}

function report(rs: Rec[]): string {
  const lines: string[] = [];
  const f = (v: number | null | undefined, d = 2): string => (v === null || v === undefined ? '  -  ' : v.toFixed(d));
  const sg = (v: number): string => `${v >= 0 ? '+' : ''}${v.toFixed(3)}`;
  for (const set of [...new Set(rs.map((r) => r.set))]) {
    lines.push(`== ${set}`);
    const of = rs.filter((r) => r.set === set);
    for (const key of [...new Set(of.map((r) => r.key))]) {
      const g = of.filter((r) => r.key === key), m = marksOf(g);
      const byOdds = [...new Set(g.map((r) => r.odds))].map((o) => { const x = marksOf(g.filter((r) => r.odds === o)); return `${o ?? 'own'}: ${f(x.field)} v ${f(x.card)}`; }).join('; ');
      lines.push(`${key.padEnd(9)} gap ${sg(m.gap!)} (${byOdds}) time ${f(m.time)} | dealt att ${f(m['att.total'], 3)} def ${f(m['def.total'], 3)} | B1 ${f(m['att.B1'])}/${f(m['def.B1'])} B2bb ${f(m['att.B2bb'])}/${f(m['def.B2bb'])} B2ca ${f(m['att.B2ca'])}/${f(m['def.B2ca'])} B3 ${f(m['att.B3'], 4)}/${f(m['def.B3'], 4)}`
        + (m['att.runs'] || m['def.runs'] ? ` | torp ${f(m['att.torp'])}/${f(m['def.torp'])} runs ${f(m['att.runs']! / m.n!, 1)}/${f(m['def.runs']! / m.n!, 1)} B4 ${f(m['att.B4'])}/${f(m['def.B4'])} B5 far ${f(m['att.B5far'])}/${f(m['def.B5far'])} late ${f(m['att.B5late'])}/${f(m['def.B5late'])} B6 ${f(m['att.B6'])}/${f(m['def.B6'])}` : ''));
    }
  }
  return lines.join('\n');
}

async function main(): Promise<void> {
  if (arg('set')) lay(JSON.parse(arg('set')), SEA as unknown as Record<string, unknown>);
  const all = jobs(), shard = arg('shard'), out = arg('out', 'sea-marks.json');
  if (shard) {
    const [i, n] = shard.split('/').map(Number);
    writeFileSync(out, JSON.stringify(all.filter((_, x) => x % n === i).map(fight)));
    return;
  }
  const n = Math.max(1, Number(arg('jobs', '1')));
  const parts = Array.from({ length: n }, (_, i) => `${out}.part${i}`);
  await Promise.all(parts.map((p, i) => new Promise<void>((ok, no) => {
    const c = fork(process.argv[1], [...process.argv.slice(2).filter((a) => !a.startsWith('--out=') && !a.startsWith('--jobs=')), `--shard=${i}/${n}`, `--out=${p}`]);
    c.on('exit', (code) => (code === 0 ? ok() : no(new Error(`shard ${i} exited ${code}`))));
  })));
  const rs: Rec[] = parts.flatMap((p) => { const r = JSON.parse(readFileSync(p, 'utf8')) as Rec[]; unlinkSync(p); return r; });
  const sets = [...new Set(rs.map((r) => r.set))];
  const summary: Record<string, Record<string, Record<string, number | null>>> = {};
  for (const set of sets) {
    summary[set] = {};
    const of = rs.filter((r) => r.set === set);
    for (const key of [...new Set(of.map((r) => r.key))]) {
      const g = of.filter((r) => r.key === key);
      summary[set][key] = marksOf(g);
      for (const o of [...new Set(g.map((r) => r.odds))]) summary[set][`${key}@${o ?? 'own'}`] = marksOf(g.filter((r) => r.odds === o));
    }
  }
  writeFileSync(out, JSON.stringify({ args: process.argv.slice(2), summary }, null, 1));
  console.log(report(rs));
}

void main();
