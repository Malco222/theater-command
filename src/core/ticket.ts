// The Battle Ticket: the one contract between the campaign map and every battle.
// Everything the campaign knows flows down in a BattleTicket; everything the battle
// changes flows back up in a BattleResult. Played battles and auto-resolve both take
// a ticket and return a result, so the campaign never needs to know which one ran.

export type Domain = 'land' | 'air' | 'sea';

export type VictoryLevel =
  | 'Heroic victory'
  | 'Decisive victory'
  | 'Close victory'
  | 'Pyrrhic victory'
  | 'Close defeat'
  | 'Decisive defeat'
  | 'Crushing defeat';

export interface TicketLine {
  label: string;
  /** What the campaign map says. */
  value: string;
  /** What that does inside the battle. */
  effect?: string;
}

export interface BattleTicketHeader {
  domain: Domain;
  turn: number;
  title: string;
  date: string;
  attacker: string;
  defender: string;
  lines: TicketLine[];
  winConditions: { side: string; text: string }[];
  stakes: string;
}

export interface PilotFate {
  name: string;
  squadron: string;
  side: string;
  fate: 'killed' | 'wounded' | 'captured' | 'missing' | 'rescued' | 'safe';
  kills: number;
  note?: string;
}

export interface SquadronResult {
  name: string;
  side: string;
  aircraft: string;
  started: number;
  lost: number;
  damaged: number;
  kills: number;
  morale: string;
}

export interface BattleResult {
  domain: Domain;
  seed: number;
  durationSec: number;
  winner: string;
  /** From the point of view of each side. */
  levels: Record<string, VictoryLevel>;
  summary: string[];
  squadrons: SquadronResult[];
  pilots: PilotFate[];
  /** Plain numbers the campaign applies: losses, target damage, air superiority swing. */
  flowsBack: { label: string; value: string }[];
  totals: Record<string, { started: number; lost: number; damaged: number; kills: number; pilotsLost: number }>;
  bombsOnTargetPct: number;
}

/** Turn 1 is September 1939. */
export function turnToDate(turn: number): { year: number; month: number; label: string } {
  const idx = 8 + (turn - 1); // months since Jan 1939
  const year = 1939 + Math.floor(idx / 12);
  const month = idx % 12;
  const names = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
  return { year, month, label: `${names[month]} ${year}` };
}
