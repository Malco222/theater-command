// A raid fought out as an air battle, handed back to the rules (src/battle/air).
//
// When the player fights a raid instead of letting the dice settle it, the battle reports what each air group lost,
// how many downed fighter pilots came back, and how many bombers dropped on the target. fly() then applies those
// numbers in place of its dice, and everything a raid does after that (output stopped, railways, factories, the sky
// won, the news) follows from them exactly as it does for a raid the dice settled.
//
// The rules stay the judge: what the battle reports is held inside the dice's own run round the expected figures
// (the design director's ruling of 2026-11-17 17:55Z): each side's losses within fly()'s luck of the expected, and
// the bombers that bomb within what that luck allows. Absent, fly() rolls its dice exactly as before.

import type { VictoryLevel } from '../core/ticket';
import type { AirFleet, AirReport, Game } from './types';

export interface FoughtAir {
  /** By campaign air group id: fighters and bombers lost, and downed fighter pilots who fly again. */
  lost: Record<number, { ftr: number; bmr: number; back: number }>;
  /** Bombers that dropped on the target. */
  dropped: number;
  /** Bombers that jettisoned their bombs and turned back. */
  jettisoned?: number;
  /**
   * Handed to the staff partway (HAND IT TO THE STAFF): 'lost' and 'dropped' are what the battle had come to at the
   * press, and the rest is the rules' own dice (the design director, log b368): fly() rolls the raid exactly as it
   * does for a raid the dice settle, and books each pool's roll scaled by 'rest', the share of that pool's cost still
   * to come from where each side stood, on top of what was already lost. Pressed at once, rest is 1 and nothing is
   * lost yet, so it books exactly what the campaign would have rolled; it never books fewer than had happened.
   * 'away' are bombers still in the air that had turned back without bombing.
   */
  staff?: { rest: { foe: number; ftr: number; bmr: number }; away: number };
  /** By campaign air group id: the fighters and bombers it sent up into the battle, for the after card's roster. */
  flew?: Record<number, { ftr: number; bmr: number }>;
  /** The raid card's chance of success, for the after card's reckoning line. */
  chance?: number;
}

/** The dice's run either side of the expected figure: fly()'s own luck (AIR.combat.luck). */
const RUN = { luck: 0.15 };

/**
 * What fly() applies for a fought raid: each group's losses, held inside the dice's run. `expected` are fly()'s own
 * figures at even luck (defending fighters, escorting fighters, bombers lost), `groups` the groups each applies to
 * with the share fly() would give each, and `B` the bombers that reached the target.
 */
export function settleFought(
  fought: FoughtAir,
  expected: { foe: number; ftr: number; bmr: number },
  groups: { def: { f: AirFleet; share: number }[]; esc: { f: AirFleet; share: number }[]; bmr: { f: AirFleet; share: number }[] },
  B: number,
  luck = RUN.luck,
): { ftr: (f: AirFleet) => number; bmr: (f: AirFleet) => number; back: (f: AirFleet, k: number) => number; through: (survivors: number) => number } {
  const held = (rows: { f: AirFleet; share: number }[], want: (f: AirFleet) => number, exp: number, cap: (f: AirFleet) => number): Map<number, number> => {
    const got = new Map(rows.map((r) => [r.f.id, Math.min(cap(r.f), Math.max(0, Math.round(want(r.f))))]));
    const total = [...got.values()].reduce((s, v) => s + v, 0);
    const lo = Math.floor(exp * (1 - luck)), hi = Math.ceil(exp * (1 + luck));
    const goal = total < lo ? lo : total > hi ? hi : total;
    if (goal === total || !rows.length) return got;
    // Spread the difference by the shares fly() itself would give, a whole aircraft at a time.
    let left = goal - total;
    const step = Math.sign(left);
    for (let guard = 0; left !== 0 && guard < 100_000; guard++) {
      const pick = [...rows].sort((a, b) => step * (b.share - a.share) || a.f.id - b.f.id)
        .find((r) => (step > 0 ? got.get(r.f.id)! < cap(r.f) : got.get(r.f.id)! > 0));
      if (!pick) break;
      got.set(pick.f.id, got.get(pick.f.id)! + step);
      left -= step;
    }
    return got;
  };
  const row = (f: AirFleet): FoughtAir['lost'][number] => fought.lost[f.id] ?? { ftr: 0, bmr: 0, back: 0 };
  const def = held(groups.def, (f) => row(f).ftr, expected.foe, (f) => f.ftr);
  const esc = held(groups.esc, (f) => row(f).ftr, expected.ftr, (f) => f.ftr);
  const bmr = held(groups.bmr, (f) => row(f).bmr, expected.bmr, (f) => f.bmr);
  const bomberRun = { lo: B - Math.ceil(expected.bmr * (1 + luck)), hi: B - Math.floor(expected.bmr * (1 - luck)) };
  return {
    ftr: (f) => def.get(f.id) ?? esc.get(f.id) ?? 0,
    bmr: (f) => bmr.get(f.id) ?? 0,
    back: (f, k) => Math.min(k, row(f).back),
    // The bombers that bomb: what dropped, but never outside the run the dice give for the bombers that survive.
    through: (survivors) => Math.max(0, Math.min(survivors, Math.max(bomberRun.lo, Math.min(bomberRun.hi, fought.dropped)))),
  };
}

/** A nation's people as the after card names them, from the adjective the cards use: the French, the Germans, the Swiss. */
export function peopleOf(adj: string): string {
  const odd: Record<string, string> = { Luxembourg: 'Luxembourgers', Northern: 'Northern side', Southern: 'Southern side' };
  if (odd[adj]) return odd[adj];
  return /(ish|ch|ese|ss)$/.test(adj) ? adj : `${adj}s`;
}

type Bill = Record<'att' | 'def', { ftr: number; bmr: number }>;

/**
 * The air battle's after card, in the UI director's words (2026-11-17 18:45Z and 18:50Z): the staff's line when he
 * handed it over, whose bombers dropped, one bill (the one the campaign books, his own side first), and, handed over
 * partway, the part of it that came before the press. A kind at 0 is left out; a side that lost nothing lost none.
 * 'bill' null leaves the bill out (the battle itself does not know it when the staff settle the rest).
 */
export function afterWords(w: {
  place: string; sky: boolean; me: 'att' | 'def'; adj: Record<'att' | 'def', string>;
  bombers: number; dropped: number; jettisoned: number; staff: boolean; bill: Bill | null; before: Bill | null;
}): string[] {
  const foe = w.me === 'att' ? 'def' : 'att', them = peopleOf(w.adj[foe]);
  const what = (b: Bill, s: 'att' | 'def'): string => {
    const f = b[s].ftr, k = b[s].bmr;
    return [f ? `${f} ${f === 1 ? 'fighter' : 'fighters'}` : '', k ? `${k} ${k === 1 ? 'bomber' : 'bombers'}` : ''].filter(Boolean).join(' and ');
  };
  const mine0 = w.before ? what(w.before, w.me) || 'none' : '';
  const theirs0 = w.before ? what(w.before, foe) : '';
  return [
    ...(w.staff ? [`The staff settled the rest of the ${w.sky ? 'fight for the sky over' : 'raid on'} ${w.place} from where it stood: what you had won stays won.`] : []),
    // Whose bombers (18:50Z): his own, or the raiders' by their adjective when he defends.
    ...(w.bombers > 0 ? [`${w.dropped} of ${w.me === 'att' ? 'your ' : ''}${w.bombers} ${w.me === 'att' ? '' : `${w.adj.att} `}bombers dropped their bombs on ${w.place}${w.jettisoned ? `, and ${w.jettisoned} jettisoned theirs` : ''}.`] : []),
    ...(w.bill ? [`The ${w.sky ? `fight for the sky over ${w.place}` : 'raid'} cost you ${what(w.bill, w.me) || 'nothing'}, and the ${them} lost ${what(w.bill, foe) || 'none'}.`] : []),
    // Two kinds of his own already carry an 'and', so a comma sets the enemy's part apart.
    // Nothing down on either side yet: the sea card's form (the UI director, air review 2).
    ...(w.before ? [mine0 === 'none' && !theirs0 ? 'Before you handed it to the staff, no aircraft had fallen on either side.'
      : `Before you handed it to the staff, you had lost ${mine0}${mine0.includes(' and ') ? ',' : ''} and the ${them} ${theirs0 || 'none'}.`] : []),
  ];
}

/**
 * How the raid went against the chance the raid card gave it: the land and sea cards' one line under AGAINST THE
 * STAFF'S RECKONING (the UI director, air review 1; the design director, 19:29Z). No dice on the player's page.
 */
export function reckoned(carried: boolean, chance: number): string {
  if (chance >= 0.995) return carried ? 'As the staff expected: they were sure of it.' : 'Worse than the staff expected: they were sure of it.';
  if (chance <= 0.005) return carried ? 'Better than the staff expected: they gave it no chance.' : 'As the staff expected: they gave it no chance.';
  const p = `they gave you about ${Math.round(chance * 100)}% to win it.`;
  if (carried) return chance < 0.5 ? `You did better than the staff expected: ${p}` : `As the staff expected: ${p}`;
  return chance >= 0.5 ? `You did worse than the staff expected: ${p}` : `As the staff feared: ${p}`;
}

/** A roster row: what a group lost of what it sent up, one form for every row (the design director, 19:29Z). */
export function rosterWords(lost: number, flew: number): string {
  // A lone aircraft is spelled (the UI director, 19:36Z): "lost none of the one that flew", "lost the one that flew".
  if (flew === 1) return lost ? 'lost the one that flew' : 'lost none of the one that flew';
  return `lost ${lost === 0 ? 'none' : lost} of the ${flew} that flew`;
}

/** The after card of a raid fought out as an air battle, written from what the campaign booked. */
export interface AfterCard {
  lines: string[];
  /** The verdict as the player reads it (the report's own is the raider's). */
  level: VictoryLevel;
  /** The one line under AGAINST THE STAFF'S RECKONING, when the battle handed back the raid card's chance. */
  reckoning?: string;
  /**
   * The groups' cards at the card's foot: each group's end, as booked. 'flew' are the aircraft it sent up into the
   * battle (what the top bar counted), and 'words' its row: "lost 25 of the 175 that flew".
   */
  groups: { name: string; side: 'att' | 'def'; ftr: number; bmr: number; lostFtr: number; lostBmr: number; flew?: number; words: string }[];
}

const FLIP: Record<VictoryLevel, VictoryLevel> = {
  'Heroic victory': 'Crushing defeat', 'Decisive victory': 'Decisive defeat', 'Close victory': 'Close defeat', 'Pyrrhic victory': 'Close defeat',
  'Close defeat': 'Close victory', 'Decisive defeat': 'Decisive victory', 'Crushing defeat': 'Heroic victory',
};

/**
 * The after card from the raid report the campaign wrote for a fought raid, so the card's bill is the campaign's
 * bill, whether the battle was fought to the end or handed to the staff partway (whose rest the campaign's own dice
 * settled). 'Before you handed it to the staff' is the battle's own part, from what it handed back.
 */
export function afterCard(g: Game, rep: AirReport, fought: FoughtAir): AfterCard {
  const me: 'att' | 'def' = g.human !== null && g.human === rep.defNat && g.human !== rep.attNat ? 'def' : 'att';
  const adj = (n: string): string => g.nations[n]?.adj ?? n;
  const sum = (rows: AirReport['att'], k: 'lostFtr' | 'lostBmr' | 'bmr'): number => rows.reduce((t, x) => t + x[k], 0);
  const had = (rows: AirReport['att'], k: 'ftr' | 'bmr'): number => rows.reduce((t, x) => t + (x.id !== undefined ? fought.lost[x.id]?.[k] ?? 0 : 0), 0);
  const lines = afterWords({
    place: rep.place, sky: rep.mission === 'superiority', me, adj: { att: adj(rep.attNat), def: adj(rep.defNat) },
    bombers: sum(rep.att, 'bmr'), dropped: rep.dropped ?? fought.dropped, jettisoned: fought.jettisoned ?? 0, staff: !!fought.staff,
    bill: { att: { ftr: sum(rep.att, 'lostFtr'), bmr: sum(rep.att, 'lostBmr') }, def: { ftr: sum(rep.def, 'lostFtr'), bmr: sum(rep.def, 'lostBmr') } },
    before: fought.staff ? { att: { ftr: had(rep.att, 'ftr'), bmr: had(rep.att, 'bmr') }, def: { ftr: had(rep.def, 'ftr'), bmr: had(rep.def, 'bmr') } } : null,
  });
  const card = (side: 'att' | 'def') => (x: AirReport['att'][number]) => {
    const up = x.id !== undefined ? fought.flew?.[x.id] : undefined;
    const flew = up ? up.ftr + up.bmr : undefined;
    return { name: x.fleet, side, ftr: x.ftr, bmr: x.bmr, lostFtr: x.lostFtr, lostBmr: x.lostBmr, flew, words: rosterWords(x.lostFtr + x.lostBmr, flew ?? x.ftr + x.bmr) };
  };
  // The raid card's chance was the raider's: a defender reads the other side of it.
  const won = me === 'att' ? rep.won : !rep.won;
  const reckoning = fought.chance !== undefined ? reckoned(won, me === 'att' ? fought.chance : 1 - fought.chance) : undefined;
  return {
    lines, level: me === 'att' ? rep.level : FLIP[rep.level], ...(reckoning ? { reckoning } : {}),
    groups: [...rep.att.map(card('att')), ...rep.def.map(card('def'))],
  };
}
