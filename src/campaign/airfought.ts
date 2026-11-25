// A raid fought out as an air battle, handed back to the rules (src/battle/air).
//
// When the player fights a raid instead of letting the dice settle it, the battle reports what each air group lost,
// how many downed fighter pilots came back, and how many bombers dropped on the target. fly() then applies those
// numbers in place of its dice, and everything a raid does after that (output stopped, railways, factories, the sky
// won, the news) follows from them exactly as it does for a raid the dice settled.
//
// The field is the judge (the design director, b397): fly() books the very losses the field saw, so the after card
// never says other than the field (ruling 226, mark 6), and the bombers that bomb are those that reached the release.
// Whether the bombs cut the railway or wrecked a factory is decided on the field as the bombs fall, by the campaign's
// own law (BombLaw), with one throw for each drawn when the battle starts; fly() takes the same throws, so the line the
// field reports cut is the line the campaign cuts. Absent, fly() rolls its dice exactly as before.
//
// Release 1.6 (release 2's file on release 1.5's campaign): day raids and the fight for the sky only, so a strike on
// ships is never fought here and the after card has no ships-kept-afloat line (ruling 240 is release 2's).

import type { VictoryLevel } from '../core/ticket';
import type { AirFleet, AirReport, BomberRow, BomberShot, Game, ShipClass } from './types';

export interface FoughtAir {
  /** By campaign air group id: fighters and bombers lost, and downed fighter pilots who fly again. */
  lost: Record<number, { ftr: number; bmr: number; back: number }>;
  /** Bombers that dropped on the target. */
  dropped: number;
  /** Bombers that jettisoned their bombs and turned back. */
  jettisoned?: number;
  /** Bombers lost on the field before they reached the release, and after it (a bomber lost after bombing still bombed). */
  lostBefore?: number;
  lostAfter?: number;
  /** A side that broke off everything (BREAK OFF ALL): it gave the battle up, and the verdict goes against it. */
  gaveUp?: 'att' | 'def';
  /**
   * Handed to the staff partway (HAND IT TO THE STAFF): 'lost' and 'dropped' are what the battle had come to at the
   * press, and the rest is the rules' own dice (the design director, log b368): fly() rolls the raid exactly as it
   * does for a raid the dice settle, and books each pool's roll scaled by 'rest', the share of that pool's cost still
   * to come from where each side stood, on top of what was already lost. Pressed at once, rest is 1 and nothing is
   * lost yet, so it books exactly what the campaign would have rolled; it never books fewer than had happened.
   * 'away' are bombers still in the air that had turned back without bombing.
   */
  staff?: {
    rest: { foe: number; ftr: number; bmr: number }; away: number;
    /** The raider's chance as the staff weighed it at the press (AirSim.staffChance), and whether any group had met an enemy yet. */
    chance?: number; fired?: boolean;
  };
  /** By campaign air group id: the fighters and bombers it sent up into the battle, for the after card's roster. */
  flew?: Record<number, { ftr: number; bmr: number }>;
  /** The raid card's chance of success, for the after card's reckoning line. */
  chance?: number;
  /**
   * The field's throws for the railway and for a factory (b397): each is cut or wrecked when its chance, from the bombers
   * that dropped, comes above the throw. fly() uses them in place of its own, so it books what the field showed.
   */
  rolls?: { rail?: number; ruin?: number };
  /**
   * A strike on ships (ruling 238): by fleet id, the ships of each class the field sank as the bombers dropped, drawn by
   * the campaign's own law (shipHits). fly() books them in place of its own roll; handed to the staff, they stand and
   * the staff's dice may add to them.
   */
  sunk?: Record<number, Partial<Record<ShipClass, number>>>;
  /**
   * Ruling 240 (C3; the design director, 13:38Z on step 9): where damage control is in play, by fleet id, the ships of
   * each class it kept afloat on the field: on the same throws, the plain law's sinkings less the eased law's (ShipBook
   * kept()). fly() tells them where the field's sinkings stand. Absent where no fleet's damage control is other than 1.
   */
  kept?: Record<number, Partial<Record<ShipClass, number>>>;
  /**
   * By campaign air group id, for a group whose bombers the battle knew by name (bombers.ts seatsOf): the bombers that
   * fell, each as the row the battle was handed, and what the battle saw each bomber's gunners shoot down. fly() makes
   * the group's roster up as the ticket did before it books the losses, and the stories strike off these very bombers
   * (stories.ts bombersFlew). Never a count: 'lost' is the count.
   */
  fell?: Record<number, BomberRow[]>;
  shot?: Record<number, BomberShot[]>;
  /**
   * By campaign air group id: the enemy fighters its bombers' gunners shot down, every loss the field booked to its
   * boxes (ruling 227), for the group's record (stories.ts bombersFlew). Never a count: the losses are in 'lost'.
   */
  guns?: Record<number, number>;
}

/**
 * How a raid's bombs are weighed, from the bombers that drop: fly()'s own law for a raid on a province (b397), so the
 * field can say as the bombs fall what the campaign will make of them.
 */
export interface BombLaw {
  /** What each bomber that drops counts for on the target: by night fewer find it, and better bombers count for more. */
  hit: number;
  /** The share of the province's output stopped, min(most, hit x bombers / scale) x keep, worth 'worth' aircraft whole (0 where there is none). */
  out: { scale: number; most: number; keep: number; worth: number };
  /** The railway: cut with the chance min(most, hit x bombers / scale) x (1 - stop); null where it cannot be cut. */
  rail: { scale: number; most: number; stop: number } | null;
  /** A factory wrecked outright: the chance min(cap, stopped x chance) once the output stopped passes 'over'; null where none can be. */
  ruin: { over: number; chance: number; cap: number } | null;
}
/** The share of output the bombs of n bombers stop. */
export const bombStopped = (L: BombLaw, n: number): number => (n > 0 ? Math.min(L.out.most, (n * L.hit) / L.out.scale) * L.out.keep : 0);
/** What the bombs of n bombers are worth on the campaign's scale (aircraft), before a cut railway or a wrecked factory. */
export const bombWorth = (L: BombLaw, n: number): number => L.out.worth * bombStopped(L, n);
/** The chance that the bombs of n bombers cut the railway. */
export const railChanceOf = (L: BombLaw, n: number): number => (L.rail && n > 0 ? Math.min(L.rail.most, (n * L.hit) / L.rail.scale) * (1 - L.rail.stop) : 0);
/** The chance that the bombs of n bombers wreck a factory outright. */
export const ruinChanceOf = (L: BombLaw, n: number): number => {
  if (!L.ruin) return 0;
  const s = bombStopped(L, n);
  return s <= L.ruin.over ? 0 : Math.min(L.ruin.cap, s * L.ruin.chance);
};

/**
 * What fly() applies for a fought raid: each group's losses as the field saw them (the design director, b397: the card
 * takes the field's losses, so the tray, the card and the field never disagree), never past a group's own strength,
 * and the bombers that reached the release. `B` are the bombers that flew.
 */
export function settleFought(
  fought: FoughtAir,
  B: number,
): { ftr: (f: AirFleet) => number; bmr: (f: AirFleet) => number; back: (f: AirFleet, k: number) => number; through: () => number } {
  const row = (f: AirFleet): FoughtAir['lost'][number] => fought.lost[f.id] ?? { ftr: 0, bmr: 0, back: 0 };
  const whole = (n: number, most: number): number => Math.min(most, Math.max(0, Math.round(n)));
  return {
    ftr: (f) => whole(row(f).ftr, f.ftr),
    bmr: (f) => whole(row(f).bmr, f.bmr),
    back: (f, k) => Math.min(k, row(f).back),
    // The bombers that bomb: those that reached the release on the field, including any lost after it (b384(a)).
    through: () => Math.max(0, Math.min(B, fought.dropped)),
  };
}

/** A nation's people as the after card names them, from the adjective the cards use: the French, the Germans, the Swiss. */
export function peopleOf(adj: string): string {
  const odd: Record<string, string> = { Luxembourg: 'Luxembourgers', Northern: 'Northern side', Southern: 'Southern side' };
  if (odd[adj]) return odd[adj];
  return /(ish|ch|ese|ss)$/.test(adj) ? adj : `${adj}s`;
}

type Bill = Record<'att' | 'def', { ftr: number; bmr: number }>;

/** How the staff took it over: before any shot, with the fight moved the player's way from where the card put it, or not. */
export type StaffHow = 'whole' | 'won' | 'rest';

/** The staff's line on the after card, in its three forms (the UI director's ruling 226 words, passed 19:48Z). */
function staffLine(w: { place: string; sky: boolean; how: StaffHow }): string {
  const what = w.sky ? 'fight for the sky over' : 'raid on';
  if (w.how === 'whole') return `The staff settled the whole ${what} ${w.place}: no shot had been fired.`;
  return `The staff settled the rest of the ${what} ${w.place} from where it stood${w.how === 'won' ? ': what you had won stays won' : ''}.`;
}

/** How far the fight must have moved his way before "what you had won stays won" is said (the design director, 20:51Z): 5 points. */
export const WON_BY = 0.05;

/** Which form: no group had met an enemy; or the staff's chance at the press, his side of it, 5 points or more above the raid card's. */
export function staffHow(me: 'att' | 'def', staff: { chance?: number; fired?: boolean }, card?: number): StaffHow {
  if (staff.fired === false) return 'whole';
  if (staff.chance === undefined || card === undefined) return 'rest';
  const mine = (p: number): number => (me === 'att' ? p : 1 - p);
  return mine(staff.chance) - mine(card) >= WON_BY - 1e-9 ? 'won' : 'rest';
}

/**
 * The air battle's after card, in the UI director's words (2026-11-17 18:45Z and 18:50Z): the staff's line when he
 * handed it over, whose bombers dropped, one bill (the one the campaign books, his own side first), and, handed over
 * partway, the part of it that came before the press. A kind at 0 is left out; a side that lost nothing lost none.
 * 'bill' null leaves the bill out (the battle itself does not know it when the staff settle the rest).
 */
export function afterWords(w: {
  place: string; sky: boolean; me: 'att' | 'def'; adj: Record<'att' | 'def', string>;
  /** What the bombers dropped on, where it is not the place: a fleet at sea ("the Mobile Force"). The bombers line only. */
  target?: string;
  bombers: number; dropped: number; jettisoned: number; staff: boolean; bill: Bill | null; before: Bill | null;
  /** Bombers lost before the target. */
  lostBefore?: number;
  /** Handed to the staff: which of the staff line's three forms. */
  how?: StaffHow;
  /** A strike on ships, with damage control in play: the ships it kept afloat, told after the bombers line (keptLines). */
  kept?: string[];
}): string[] {
  const foe = w.me === 'att' ? 'def' : 'att', them = peopleOf(w.adj[foe]);
  const what = (b: Bill, s: 'att' | 'def'): string => {
    const f = b[s].ftr, k = b[s].bmr;
    return [f ? `${f} ${f === 1 ? 'fighter' : 'fighters'}` : '', k ? `${k} ${k === 1 ? 'bomber' : 'bombers'}` : ''].filter(Boolean).join(' and ');
  };
  const mine0 = w.before ? what(w.before, w.me) || 'none' : '';
  const theirs0 = w.before ? what(w.before, foe) : '';
  return [
    // "What you had won stays won" only when something was won before the press; pressed before anything happened,
    // the staff settled the whole of it (ruling 226, mark 5).
    ...(w.staff ? [staffLine({ place: w.place, sky: w.sky, how: w.how ?? 'rest' })] : []),
    ...(w.bombers > 0 ? [bombersLine(w)] : []),
    ...(w.kept ?? []),
    ...(w.bill ? [`The ${w.sky ? `fight for the sky over ${w.place}` : 'raid'} cost you ${what(w.bill, w.me) || 'nothing'}, and the ${them} lost ${what(w.bill, foe) || 'none'}.`] : []),
    // Two kinds of his own already carry an 'and', so a comma sets the enemy's part apart.
    // Nothing down on either side yet: the sea card's form (the UI director, air review 2).
    ...(w.before ? [mine0 === 'none' && !theirs0 ? 'Before you handed it to the staff, no aircraft had fallen on either side.'
      : `Before you handed it to the staff, you had lost ${mine0}${mine0.includes(' and ') ? ',' : ''} and the ${them} ${theirs0 || 'none'}.`] : []),
  ];
}

/**
 * Whose bombers dropped, and what became of the rest, each bomber counted once (the UI director's newcomer review 5;
 * the design director, b384(a)): "36 of your 48 bombers dropped their bombs on Bremen; 8 jettisoned theirs, and 4 were
 * lost before the target." / "None of your 48 bombers dropped their bombs on Bremen: 41 jettisoned theirs, and 7 were
 * lost before the target." A bomber shot down after bombing bombed; a kind at 0 is left out. When he defends, the
 * raiders' bombers by their adjective (18:50Z).
 */
function bombersLine(w: { place: string; target?: string; me: 'att' | 'def'; adj: Record<'att' | 'def', string>; bombers: number; dropped: number; jettisoned: number; lostBefore?: number }): string {
  const B = w.bombers, dropped = Math.max(0, Math.min(B, w.dropped)), on = w.target ?? w.place;
  const lost = Math.max(0, Math.min(B - dropped, w.lostBefore ?? 0));
  const jett = Math.max(0, Math.min(B - dropped - lost, w.jettisoned));
  const whose = w.me === 'att' ? `your ${B}` : `${B} ${w.adj.att}`;
  const rest = [jett ? `${jett} jettisoned ${jett === 1 ? 'its bombs' : 'theirs'}` : '', lost ? `${lost} ${lost === 1 ? 'was' : 'were'} lost before the target` : ''].filter(Boolean).join(', and ');
  if (!dropped) return `None of ${w.me === 'att' ? whose : `the ${whose}`} bombers dropped their bombs on ${on}${rest ? `: ${rest}` : ''}.`;
  return `${dropped} of ${whose} bombers dropped their bombs on ${on}${rest ? `; ${rest}` : ''}.`;
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

/** Each side's aircraft lost on an after card, fighters and bombers apart. */
export type AfterLost = Record<'att' | 'def', { ftr: number; bmr: number }>;

/**
 * The totals of an after card's rows, each side's fighters and bombers lost: the one place they are summed, so the
 * totals the 3D game reads (AirFeed.cpp ReadAfter, after.lost) are always the rows' own sums (the lead, 09:12Z).
 */
export function afterLost(card: Pick<AfterCard, 'groups'>): AfterLost {
  const out: AfterLost = { att: { ftr: 0, bmr: 0 }, def: { ftr: 0, bmr: 0 } };
  for (const x of card.groups) { out[x.side].ftr += x.lostFtr; out[x.side].bmr += x.lostBmr; }
  return out;
}

/** A verdict as the other side reads it. */
export const FLIP: Record<VictoryLevel, VictoryLevel> = {
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
  // Each bomber once: those that bombed (the report's count), those lost before the target (as booked, and fought to
  // the end no more than fell on the way in on the field), and the rest turned back.
  const B = sum(rep.att, 'bmr'), dropped = rep.dropped ?? fought.dropped, lostB = sum(rep.att, 'lostBmr');
  const lostBefore = fought.staff ? Math.max(0, lostB - (fought.lostAfter ?? 0)) : Math.min(lostB, fought.lostBefore ?? lostB);
  const lines = afterWords({
    place: rep.place, sky: rep.mission === 'superiority', me, adj: { att: adj(rep.attNat), def: adj(rep.defNat) },
    bombers: B, dropped, jettisoned: Math.max(0, B - dropped - lostBefore), lostBefore, staff: !!fought.staff,
    ...(fought.staff ? { how: staffHow(me, fought.staff, fought.chance) } : {}),
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
