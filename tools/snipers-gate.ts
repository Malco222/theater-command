// The sniper teams' gate (release 2; the design director's ruling of 25 November 2026 16:30Z, "The gate"): computer
// against computer on the test kitchen's land battles, hiding on before the first step, each set on the same tickets
// and seeds in four arms: off, on, and two placebos (off plus one extra throw of the battle's own dice at the clock's
// start, and at 120 s), the chance floor for every mark (gate-chance-floor). One JSON line a battle, resumable: a run
// skips the battles its file already holds.
//
//   node gate.mjs <set> <arm> <out.jsonl>       set F T Y B P C M W; arm off on p0 p120 (W: on20 on80)
//
// Not a test and not shipped: a measuring tool, as tools/history.ts is.
import { diceChance } from '../src/battle/core';
import { LAND, LandBattle, PRACTICE_GROUNDS, practiceTicket, sampleTicket, withSnipers, withTeams } from '../src/battle/land';
import type { FieldTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land';
import { appendFileSync, existsSync, readFileSync } from 'node:fs';

type Terrain = Parameters<typeof sampleTicket>[0];
interface Job { key: string; cell: string; ticket: () => FieldTicket<LandPlace>; plain?: boolean }

const GROUNDS7: Terrain[] = ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'];
const ODDS = [0.8, 1.0, 1.1, 1.2, 1.3, 1.4, 1.6, 1.8];
const seedOf = (s: number): number => s * 7919 + 13;
const FOOT = ['inf', 'mtn', 'mot', 'cav', 'snp'];

function jobs(set: string): Job[] {
  const out: Job[] = [];
  if (set === 'F' || set === 'W' || set === 'T') {
    const gs = set === 'F' ? GROUNDS7 : (['plains', 'forest', 'urban'] as Terrain[]);
    for (const g of gs) for (const o of ODDS) for (let s = 1; s <= 24; s++) {
      out.push({ key: `${g}:${o}:${s}`, cell: `${g}:${o}`, ticket: () => { const t = sampleTicket(g, o, { seed: seedOf(s) }); return set === 'T' ? withTeams(t) : t; } });
    }
  } else if (set === 'Y') {
    for (const g of ['plains', 'forest', 'urban'] as Terrain[]) for (const o of [1.2, 1.4]) for (const a of ['bold', 'methodical', 'mass']) for (const d of ['bold', 'methodical']) for (let s = 1; s <= 24; s++) {
      out.push({ key: `${g}:${o}:${a}-${d}:${s}`, cell: `${g}:${o}:${a}-${d}`, ticket: () => sampleTicket(g, o, { seed: seedOf(s), style: { att: a, def: d } }) });
    }
  } else if (set === 'B') {
    for (const g of ['plains', 'forest', 'urban', 'hills'] as Terrain[]) for (const o of [1.0, 1.2, 1.4]) for (const [a, d] of [['SOV', 'GER'], ['GER', 'SOV'], ['USA', 'GER'], ['GER', 'USA'], ['UK', 'ITA'], ['ITA', 'UK']]) for (let s = 1; s <= 24; s++) {
      out.push({ key: `${g}:${o}:${a}-${d}:${s}`, cell: `${a}-${d}`, ticket: () => sampleTicket(g, o, { seed: seedOf(s), nat: { att: a, def: d } }) });
    }
  } else if (set === 'P') {
    PRACTICE_GROUNDS.forEach((p, i) => { for (let s = 1; s <= 24; s++) out.push({ key: `${i}:${s}`, cell: `practice:${p.terrain}${p.river ? '+river' : ''}${p.fort ? '+fort' : ''}`, ticket: () => practiceTicket(p, { seed: seedOf(s) }) }); });
    for (const town of ['city', 'capital'] as const) for (const o of [1.0, 1.2, 1.4]) for (let s = 1; s <= 24; s++) {
      out.push({ key: `${town}:${o}:${s}`, cell: `town:${town}`, ticket: () => sampleTicket('urban', o, { seed: seedOf(s), town }) });
    }
  } else if (set === 'C') {
    for (const g of GROUNDS7) for (const o of [0.8, 1.0, 1.2, 1.4, 1.8, 2.5]) for (let s = 1; s <= 4; s++) out.push({ key: `${g}:${o}:${s}`, cell: g, ticket: () => sampleTicket(g, o, { seed: seedOf(s) }) });
  } else if (set === 'M') {
    const EVERY: { key: string; ground: Terrain; river?: boolean; fort?: number; dug?: boolean }[] = [
      ...GROUNDS7.map((g) => ({ key: g as string, ground: g })), { key: 'river', ground: 'plains', river: true }, { key: 'fort', ground: 'hills', fort: 2 }, { key: 'dug', ground: 'plains', dug: true },
    ];
    for (const e of EVERY) for (const o of [1.1, 1.2, 1.3, 1.4]) for (let s = 1; s <= 40; s++) {
      out.push({ key: `${e.key}:${o}:${s}`, cell: `${e.key}:${o}`, plain: true, ticket: () => sampleTicket(e.ground, o, { seed: seedOf(s), humanAtt: true, river: e.river, fort: e.fort, dug: e.dug }) });
    }
  }
  return out;
}

/** Select every block, right-click the enemy nearest them, Space (the test's mark 4). */
function plainAttack(b: LandBattle): void {
  const mine = b.units.filter((u) => u.side === 'att' && u.kind !== 'fort');
  const cx = mine.reduce((a, u) => a + u.x, 0) / mine.length, cy = mine.reduce((a, u) => a + u.y, 0) / mine.length;
  const foes = b.units.filter((u) => u.side === 'def');
  const near = foes.reduce((m, f) => (Math.hypot(f.x - cx, f.y - cy) < Math.hypot(m.x - cx, m.y - cy) ? f : m), foes[0]);
  b.command({ do: 'attack', units: mine.map((u) => u.id), target: near.id }, 'att');
}

function fight(job: Job, arm: string): Record<string, unknown> {
  const t0 = job.ticket();
  const on = arm.startsWith('on');
  const b = new LandBattle(on ? withSnipers(t0) : t0);
  if (job.plain) { b.ai.att = false; b.ai.def = true; plainAttack(b); } else b.ai.att = b.ai.def = true;
  b.hideOn();
  const rng = (b as unknown as { rng: { next(): number } }).rng;
  if (arm === 'p0') rng.next();
  const snp = b.units.filter((u) => u.team === 'snp');
  const arm0 = b.units.filter((u) => u.kind === 'arm');
  const S = new Map(snp.map((u) => [u.id, { hid: 0, found: 0, vague: 0, quiet: 0, slip: 0, fired: 0, start: '' }]));
  let closed = 0, threw = false;
  for (let k = 0; k < 1e6 && !b.over; k++) {
    if (arm === 'p120' && !threw && b.t >= 120) { rng.next(); threw = true; }
    b.step();
    if (k === 0) for (const u of snp) S.get(u.id)!.start = u.hid ? 'hid' : b.sniperGround(u.x, u.y) ? 'cover' : 'open';
    for (const u of snp) {
      if (u.state === 'withdrawn' || u.state === 'gone') continue;
      const s = S.get(u.id)!;
      if (u.hid) s.hid++; else if (u.unloc !== undefined && b.t < u.unloc) s.vague++; else s.found++;
      if (u.quietTo !== undefined && b.t < u.quietTo) s.quiet++;
      if (u.slipTo !== undefined) s.slip++;
      if (u.engaged !== undefined) s.fired++;
    }
    for (const u of arm0) if (u.closedTo !== undefined && b.t < u.closedTo) closed++;
  }
  const o = b.over!, k = b.ticket.reckoning;
  // Each side's foot share of its own field losses (a sniper team counted as the foot it came from), and the split
  // weights the campaign's booking would draw from the field's per-corps losses (game.ts splitWeights, men-weighted).
  const footShare = (side: 'att' | 'def'): number => {
    let foot = 0, all = 0;
    for (const u of b.units) if (u.side === side) { const w = u.power0 * (1 - u.str / u.str0); all += w; if (FOOT.includes(u.kind) || u.team === 'snp') foot += w; }
    return all > 1e-9 ? foot / all : 0;
  };
  const split = (side: 'att' | 'def'): Record<number, number> => {
    const fs = t0[side].forces, men = new Map<number, number>();
    for (const f of fs) men.set(f.corps, (men.get(f.corps) ?? 0) + f.men);
    const lost = new Map(o.forces.filter((f) => f.side === side).map((f) => [f.corps, f.lost]));
    let tot = 0, sum = 0;
    for (const [c, m] of men) { tot += m; sum += m * (lost.get(c) ?? 0); }
    const mean = tot > 0 ? sum / tot : 0, w: Record<number, number> = {};
    for (const c of men.keys()) w[c] = mean > 1e-6 && lost.has(c) ? Math.round(Math.min(3, lost.get(c)! / mean) * 1000) / 1000 : 1;
    return w;
  };
  const pins = b.units.filter((u) => u.pinSpent !== undefined).map((u) => u.pinSpent!);
  const lo = k.odds * k.dice[0], hi = k.odds * k.dice[1];
  return {
    key: job.key, cell: job.cell, arm, w: o.winner, how: o.how, sec: o.seconds, R: o.R, odds: k.odds, dice: k.dice,
    chance: diceChance(k.odds, k.dice as [number, number]), out: o.R < lo - 1e-6 || o.R > hi + 1e-6 ? 1 : 0,
    la: Math.round(o.lost.att * 10000) / 10000, ld: Math.round(o.lost.def * 10000) / 10000,
    fa: Math.round(footShare('att') * 1000) / 1000, fd: Math.round(footShare('def') * 1000) / 1000,
    sa: split('att'), sd: split('def'),
    contact: b.contactAt, arrived: b.plan.att?.arrived ?? null, went: b.plan.att?.went ?? null,
    routed: b.units.filter((u) => u.state === 'routing' || u.state === 'gone').length,
    ...(snp.length ? {
      snp: snp.map((u) => ({ side: u.side, nat: u.nat, ...S.get(u.id), slips: u.slips ?? 0, out: u.order?.kind === 'withdraw' || u.state === 'withdrawn' ? 1 : 0, worn: Math.round(((u.power0 * (1 - u.str / u.str0)) / b.units.filter((f) => f.side === u.side).reduce((a, f) => a + f.power0, 0)) * 10000) / 10000 })),
      pin: pins.length ? { n: pins.length, mean: Math.round((pins.reduce((a, p) => a + p, 0) / pins.length) * 10) / 10, most: Math.max(...pins) } : null,
      closed: Math.round(closed / 10),
    } : {}),
  };
}

const [set, arm, file] = process.argv.slice(2);
if (!set || !arm || !file) throw new Error('usage: gate.mjs <set> <arm> <out.jsonl>');
// (SNP_TWEAK='pin.most=20;snipers.bite=0.3': a mend tried before it is made, set on LAND by path)
for (const kv of (process.env.SNP_TWEAK ?? '').split(';').filter(Boolean)) {
  const [path, v] = kv.split('='), keys = path.split('.');
  let o = LAND as unknown as Record<string, unknown>;
  for (const k of keys.slice(0, -1)) o = o[k] as Record<string, unknown>;
  if (!(keys[keys.length - 1] in o)) throw new Error(`no such knob ${path}`);
  o[keys[keys.length - 1]] = Number(v);
}
if (arm === 'on20') (LAND.pin as { most: number }).most = 20;
if (arm === 'on80') (LAND.pin as { most: number }).most = 80;
const done = new Set(existsSync(file) ? readFileSync(file, 'utf8').split('\n').filter(Boolean).map((l) => (JSON.parse(l) as { key: string }).key) : []);
for (const j of jobs(set)) {
  if (done.has(j.key)) continue;
  appendFileSync(file, JSON.stringify(fight(j, arm)) + '\n');
}
appendFileSync(file.replace(/\.jsonl$/, '.done'), `${set} ${arm} ${new Date().toISOString()}\n`);
