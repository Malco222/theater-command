// The raids the pilot tests fly (R1, R2, R5 of the 3D air thread's design, pilot-ai-r2; the design director's ruling of
// 25 November 2026): the flak-hold test's day raids with the guns and the fighters together, the same Kent raid with its
// largest defending group split into two equal groups, the fairness test's fight for the sky (fighters only, no
// bombers), and Kent with the defender's player sending a group at another box once the first contact is made. The
// computer flies both sides.
//
// BASELINE holds what release 2's head a14434e3 gave for each battle, from the very same code below run against that
// head: each side's aircraft lost, the ordered (time, victim) list of every loss, the rules' main stream where the
// battle ended, and the 'air:hit' events with their 'by' and 'gun' taken out. These are shown and credit changes only:
// none of them may move.
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { hashString } from '../src/core/rng';
import type { FieldEvent, FieldTicket } from '../src/battle/core/types';
import { AirSim } from '../src/battle/air/sim';
import { airTicket, raidGroups } from '../src/battle/air/ticket';
import type { AirPlace } from '../src/battle/air/types';

export type Ticket = FieldTicket<AirPlace>;
const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };

/** The flak-hold test's raid: the staff's raid on a named province with every fighter group that can go by day. */
function holdRaid(nat: string, kind: RaidKind, name: string, turn = 0): Ticket {
  const g: Game = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  if (turn) g.turn = turn;
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name)!;
  const day = planRaid(g, kind, t.id, undefined, nat);
  const ids = [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
  return airTicket(g, kind, t.id, ids, nat).ticket!;
}

/** The fairness test's k-th battle of a raid (its play file's September opening). */
function fairRaid(k: number, nat: string, kind: RaidKind, name: string): Ticket {
  const g = startOpen(createGame(7, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name)!;
  return airTicket(g, kind, t.id, raidGroups(g, kind, t.id, nat), nat).ticket!;
}

/** Two equal defending groups: the defence's largest fighter group split into two halves that start together. */
export function twins(tk: Ticket): Ticket {
  const def = tk.def.forces;
  const big = [...def].filter((f) => f.kind !== 'bmr').sort((a, b) => b.men - a.men)[0];
  const half = Math.floor(big.men / 2);
  const corps = Math.max(...[...tk.att.forces, ...def].map((f) => f.corps)) + 1;
  const starts = { ...tk.place.starts, [corps]: { ...tk.place.starts[big.corps] } };
  const forces = def.flatMap((f) => (f === big ? [{ ...f, men: half }, { ...f, corps, name: `${f.name} II`, men: half }] : [f]));
  return { ...tk, def: { ...tk.def, forces }, place: { ...tk.place, starts } };
}

export type Play = 'ai' | 'order';
export interface Run { key: string; ticket: Ticket; play: Play }

/** The battles, five seeds each: [key, ticket, play]. */
export function runs(): Run[] {
  const out: Run[] = [];
  const kent = holdRaid('GER', 'bomb', 'Kent');
  const alsace = holdRaid('GER', 'bomb', 'Alsace');
  for (let k = 0; k < 5; k++) {
    const at = (tk: Ticket): Ticket => ({ ...tk, seed: tk.seed + k });
    out.push({ key: `kent:${k}`, ticket: at(kent), play: 'ai' });
    out.push({ key: `alsace:${k}`, ticket: at(alsace), play: 'ai' });
    out.push({ key: `twins:${k}`, ticket: twins(at(kent)), play: 'ai' });
    out.push({ key: `order:${k}`, ticket: at(kent), play: 'order' });
    out.push({ key: `sky:${k}`, ticket: fairRaid(k, 'GER', 'superiority', 'Alsace'), play: 'ai' });
  }
  return out;
}

/**
 * Flies a battle, the computer on both sides. With 'order', once the first contact is made the defender's player sends
 * his first fighter group still fighting at a bomber box other than its quarry (command 'attack'). 'each' runs after
 * every tick.
 */
export function fly(r: Run, each?: (s: AirSim) => void): AirSim {
  const s = new AirSim(r.ticket, { show: true });
  s.ai.att = true; s.ai.def = true;
  let given = false;
  while (!s.over) {
    if (r.play === 'order' && !given && s.fired) {
      const f = s.units.find((u) => u.side === 'def' && u.air === 'ftr' && u.state !== 'gone' && u.job.do !== 'home');
      const to = s.units.filter((u) => u.side === 'att' && u.air === 'bmr' && u.state !== 'gone').find((b) => !(f && f.job.do === 'intercept' && f.job.on === b.id));
      if (f && to) { s.command({ do: 'attack', units: [f.id], target: to.id }, 'def'); given = true; }
    }
    s.step();
    each?.(s);
  }
  return s;
}

/** An 'air:hit' event without its 'by' and 'gun' (R2). */
export function stripHit(e: FieldEvent): FieldEvent {
  if (e.what !== 'air:hit' || !e.extra) return e;
  const { by: _by, gun: _gun, ...rest } = e.extra as Record<string, unknown>;
  return { ...e, extra: rest };
}

/** What may not move: each side's losses, every loss's (time, victim), the main stream's end, the hits without who. */
export function digest(s: AirSim): string {
  const h = s.history();
  const lost = (side: string, k: string): number => s.units.filter((u) => u.side === side && u.air === k).reduce((t, u) => t + u.n0 - u.n, 0);
  const losses = h.filter((e) => e.what === 'air:lost').map((e) => `${e.t}:${e.unit}`).join(',');
  const hits = JSON.stringify(h.filter((e) => e.what === 'air:hit').map(stripHit));
  const main = (s as unknown as { rng: { state: number } }).rng.state;
  return `${lost('att', 'ftr')}/${lost('att', 'bmr')}/${lost('def', 'ftr')} ${hashString(losses)} ${main} ${hashString(hits)}`;
}

/** Release 2's head a14434e3, flown by runs() and digest() above. */
export const BASELINE: Record<string, string> = {
  'kent:0': '0/261/38 2319427410 1340967069 3510661325',
  'alsace:0': '19/18/29 1826434046 3408063933 1802457500',
  'twins:0': '0/268/30 431133258 2540697212 2643330455',
  'order:0': '0/244/35 3886541873 1340967069 1143825160',
  'sky:0': '31/0/45 3687527599 2199490110 1368465708',
  'kent:1': '0/223/40 2270006118 1340967070 260639464',
  'alsace:1': '22/21/32 1221648462 3408063934 3503774005',
  'twins:1': '0/238/41 886520845 2540697213 3684891373',
  'order:1': '0/228/39 2975366465 1340967070 3467667472',
  'sky:1': '34/0/47 3657701537 960887090 1345823437',
  'kent:2': '0/232/42 379977339 1340967071 2164963944',
  'alsace:2': '21/22/26 1795942530 3408063935 1975224024',
  'twins:2': '0/240/27 520245868 2540697214 2559481704',
  'order:2': '0/239/30 176676299 1340967071 184656339',
  'sky:2': '37/0/46 785332300 1957492596 1653172251',
  'kent:3': '0/267/40 811367203 1340967072 2300237840',
  'alsace:3': '13/16/29 4093758041 3408063936 2408365587',
  'twins:3': '0/238/39 1884505583 2540697215 3674308269',
  'order:3': '0/237/35 31330389 1340967072 1046530882',
  'sky:3': '50/0/46 2052210549 411353920 3475021993',
  'kent:4': '0/266/48 2325410452 1340967073 3953587932',
  'alsace:4': '13/16/31 1372176824 3408063937 196913733',
  'twins:4': '0/250/37 2887988306 2540697216 4012185413',
  'order:4': '0/265/38 2030193382 1340967073 1683863377',
  'sky:4': '41/0/66 1413517410 3333693368 2131518283',
};
