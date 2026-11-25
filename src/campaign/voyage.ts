// The voyage log (release 2; Brian, 24 November 2026 19:47Z, "yes to all", of the idea "each fleet keeps a log of where
// it sailed and what it fought, month by month, on its page"). Behind its own switch (stage.json 'voyageLog', the flag
// 'voyageLog'): off, nothing is written or shown, and every war is the war without it. A record only: it draws no dice
// and no rule reads it, like a ship's logbook kept in the chart room, read by whoever opens it.
//
// At every month's end each fleet writes one line: where it was (the sea and what it did there, or the harbour) and the
// fleet actions it fought that month (navy.ts fleetAction notes them as they are fought). The newest VOYAGE.keep lines
// are kept. On the fleet's page the lines are read newest first, months alike run together ("September to December
// 1939: on patrol in the North Sea").

import data from '../../data/campaign/voyage.json';
import { turnToDate } from '../core/ticket';
import { seaWhere } from './sea-words';
import type { FleetStance, Fleet, Game } from './types';

export const VOYAGE = data as unknown as { keep: number; show: number };

/** One line of a fleet's log: the month, what it was doing at the month's end and where, and the actions it fought in it. */
export interface VoyageEntry { t: number; s: FleetStance; z: number; p: string; f?: string[] }

/** True where fleets keep a voyage log: the grand campaign, with the switch on. */
export const voyageOn = (g: Game): boolean => g.flags.open === 1 && g.flags.voyageLog === 1;

/** Turns the voyage log on or off in a campaign under way. The lines already written are kept, and shown again when it is back on. */
export function setVoyageLog(g: Game, on: boolean): void {
  if (on) g.flags.voyageLog = 1;
  else delete g.flags.voyageLog;
}

/** The fleet's line for this month, begun now if it has none yet. */
function lineNow(g: Game, f: Fleet): VoyageEntry {
  const log = (f.voyage ??= []);
  const last = log[log.length - 1];
  if (last && last.t === g.turn) return last;
  const e: VoyageEntry = { t: g.turn, s: f.stance, z: f.zone, p: f.port };
  log.push(e);
  if (log.length > VOYAGE.keep) log.splice(0, log.length - VOYAGE.keep);
  return e;
}

/** A fleet action this fleet fought this month, in a sentence, for its log (navy.ts fleetAction). */
export function noteFight(g: Game, f: Fleet, text: string): void {
  if (!voyageOn(g)) return;
  (lineNow(g, f).f ??= []).push(text);
}

/** The month's end: every fleet with ships writes where it was. */
export function logVoyages(g: Game): void {
  if (!voyageOn(g)) return;
  for (const f of g.fleets) {
    if (!f.ships || Object.values(f.ships).every((n) => !n)) continue;
    const e = lineNow(g, f);
    e.s = f.stance; e.z = f.zone; e.p = f.port;
  }
}

/** What a line says the fleet was doing: "on patrol in the North Sea", "in harbour at Scapa Flow". */
function doing(g: Game, e: VoyageEntry): string {
  const sea = seaWhere(g.seas[e.z]?.name ?? 'sea');
  switch (e.s) {
    case 'port': return `in harbour at ${e.p}`;
    case 'raid': return `raiding shipping ${sea}`;
    case 'hunt': return `hunting submarines ${sea}`;
    case 'escort': return `escorting convoys ${sea}`;
    // (Release 2, the far chart: on its way to a new base, the port it was making for.)
    case 'passage': return `in passage to ${e.p}, ${sea}`;
    default: return `on patrol ${sea}`;
  }
}

/** The month or months a run of lines covers: "March 1940", "September to December 1939", "December 1939 to February 1940". */
function months(a: number, b: number): string {
  const x = turnToDate(a), y = turnToDate(b);
  if (a === b) return x.label;
  return x.year === y.year ? `${x.label.split(' ')[0]} to ${y.label}` : `${x.label} to ${y.label}`;
}

/** The fleet's log for its page, newest first, months alike run together: at most VOYAGE.show lines. Empty while off. */
export function voyageLines(g: Game, f: Fleet): string[] {
  if (!voyageOn(g) || !f.voyage?.length) return [];
  const out: string[] = [];
  const log = f.voyage;
  let i = log.length - 1;
  while (i >= 0 && out.length < VOYAGE.show) {
    const e = log[i];
    const what = doing(g, e);
    let j = i;
    // Quiet months doing the same thing in the same place run together.
    if (!e.f?.length) while (j > 0 && !log[j - 1].f?.length && log[j - 1].t === log[j].t - 1 && doing(g, log[j - 1]) === what) j--;
    const said = `${months(log[j].t, e.t)}: ${what}.`;
    out.push(e.f?.length ? `${said} ${e.f.join(' ')}` : said);
    i = j - 1;
  }
  return out;
}
