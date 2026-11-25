// WHAT YOU FACE, as the player's staff can state it (the design director's rulings 149(d), 156 and 158(a)).
// The battle works out what the attack faces (support.ts, `Faced`); this says how much of it his reports know.
// Guns are known when the corps is identified. Works are on every staff's map and are told everywhere (ruling 162).
// Mines hide with the guns: on foreign ground they are named only with a report. The grand campaign only.
import { minesAt } from './engineers';
import type { Assessment } from './game';
import { allied } from './game';
import { agentAt, corpsKnown, fogOn, photographedBy } from './intel';
import { aloneUnknown, faceLines, type FaceLine } from './support';
import type { Game, Region } from './types';
import type { TicketLine } from '../core/ticket';

/** The line that closes the list where the defence is not identified, overland and out of view alike: two sentences and no third (ruling 162(c)). */
export const GUNS_UNKNOWN: FaceLine = { sign: 'scouted', text: 'Not scouted: their guns are unknown.', unknown: true };
export const GUNS_AND_MINES_UNKNOWN: FaceLine = { sign: 'scouted', text: 'Not scouted: their guns and mines are unknown.', unknown: true };

/** True where every defender is identified, by a battle, an agent, a flight or a report of one: its guns are known. */
export function gunsKnown(g: Game, a: Assessment, viewer: string | null = g.human): boolean {
  return !fogOn(g, viewer) || a.defenders.every((c) => corpsKnown(g, c, viewer).level !== 'none');
}

/**
 * True where the viewer knows of the mines on this ground (ruling 158(a)): it is his own side's, or the fog is off, or
 * he has a report from it: a corps there is identified, an agent of his works there, or a flight photographed it this month.
 */
export function minesKnown(g: Game, r: Region, viewer: string | null = g.human): boolean {
  if (!fogOn(g, viewer) || viewer === null || allied(g, viewer, r.ctrl)) return true;
  if (agentAt(g, r.id, viewer) || (viewer === g.human && g.intel.scout?.[r.id] === g.turn)) return true;
  // (Fog on raids, airfog.ts: a photograph shows the ground the month it is flown and the next.)
  if (g.flags.rfog === 1 && photographedBy(g, viewer).includes(r.id)) return true;
  return r.units.some((c) => !allied(g, viewer, c.nat) && corpsKnown(g, c, viewer).level !== 'none');
}

/** A small share in words. */
const smallShare = (x: number): string => (x < 0.075 ? 'a twentieth' : x < 0.125 ? 'a tenth' : x < 0.175 ? 'a sixth' : x < 0.225 ? 'a fifth' : 'a quarter');

/**
 * The lines of WHAT YOU FACE for this attack, four at most and one more in italics for what is not scouted. Null outside the grand campaign (a history campaign's card
 * shows none). Where the defence is identified, everything. Where it is not: the works, which are on every staff's
 * map, the snipers the ground itself promises, and one line in italics for what is not known.
 */
export function whatYouFace(g: Game, a: Assessment, target: Region, viewer: string | null = g.human): FaceLine[] | null {
  const f = a.faced;
  if (!f) return null;
  if (gunsKnown(g, a, viewer)) return faceLines(f);
  const out: FaceLine[] = [];
  const at = f.antiTank;
  // Tanks with no infantry beside them: the one thing he can change before he clicks, said first, at the most the guns could take.
  const lone = at && f.shock !== null ? aloneUnknown(at) : null;
  if (lone) out.push(lone);
  const mines = !!at && at.mines > 0 && minesAt(g, target) > 0 && minesKnown(g, target, viewer);
  // The works that stand against tanks: the ditch, the dragon's teeth, and mines with a report.
  if (at && f.shock !== null && (at.works > 0 || mines)) {
    const names = [at.works > 0 ? (at.fort >= 2 ? 'Dragon\'s teeth' : 'A ditch') : '', mines ? (at.works > 0 ? 'mines' : 'Mines') : ''].filter(Boolean);
    const lone = names.length === 1 && names[0] === 'A ditch';
    out.push({ sign: 'anti-tank', text: `${names.join(' and ')}: ${lone ? 'it takes' : 'they take'} ${smallShare(at.works + (mines ? at.mines : 0))} of your tanks' shock, before their guns are counted.` });
  }
  const works = faceLines(f).find((l) => l.sign === 'mortar');
  if (works) out.push(works);
  const snipers = faceLines(f).find((l) => l.sign === 'sniper');
  if (snipers) out.push(snipers);
  // With a report from the ground he knows its mines, or that it has none: only the guns are left unknown.
  // (Four lines at most, the snipers' the first to go; the line in italics stands beside the four.)
  return [...out.slice(0, 4), minesKnown(g, target, viewer) ? GUNS_UNKNOWN : GUNS_AND_MINES_UNKNOWN];
}

/**
 * One cause, one place (ruling 166): on a card where WHAT YOU FACE states what the defence does, the ticket's 'Dug in'
 * row keeps its figure alone, with what this attack takes off the works. Elsewhere the row stands in full.
 */
export function briefTicket(lines: TicketLine[], face: FaceLine[] | null | undefined): TicketLine[] {
  if (!face?.some((l) => !l.unknown)) return lines;
  return lines.map((l) => {
    if (l.label !== 'Dug in' || !l.effect) return l;
    const figure = /^Troops in fieldworks defend \d+% stronger/.exec(l.effect);
    if (!figure) return l;
    const wear = /Won or lost, .*$/.exec(l.effect);
    return { label: l.label, value: figure[0], effect: wear ? wear[0] : undefined };
  });
}
