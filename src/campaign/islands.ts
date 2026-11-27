// The Pacific's islands as the historian's table sets them out (rulings 173 and 179; the design director, 25 November
// 2026, form B3; counted in men at 14:28Z). An island has room for so many divisions' worth of men and no more (an
// atoll for one), and a garrison cut off there withers to a floor, a quarter on an atoll and a half on an island large
// enough to feed itself (173(e), rider (o)).
// The rows are data/campaign/far-islands.json, written from data/campaign/tables/islands-draft.csv by
// tools/make-far-islands.mjs. They are read on the whole-world chart alone: with the far side off, and for ground
// that is no island of the table, nothing here answers, and every rule that asks reads as it always did.
// (This file and game.ts read each other's functions, as world.ts and game.ts do: nothing of either runs as they load.)
import data from '../../data/campaign/far-islands.json';
import { FAR_STAGE } from './stage';
import { allied, men, placed, roomless } from './game';
import { whyNotBreakUp } from './breakup';
import { MEN_PER_DIV, STACK, type Corps, type Game, type Region } from './types';
import { farOn } from './world';

/** An island's row: its room in divisions' worth of men, the floor a cut-off garrison withers to, and whether it is an islet. */
export interface Island { room: number; floor: number; islet: boolean }

// (the 4th element is the look of the island's field in the 3D ground battle, read by islandLook below and by no rule)
type Row = [number, 'quarter' | 'half', 0 | 1, 'island' | 'jungle' | ''];
const ROWS: Record<string, Row> | null = FAR_STAGE ? (data as unknown as { islands: Record<string, Row> }).islands : null;

/** The island a province is, by the table. Null with the far side off, and for ground that is no island of the table. */
export function islandOf(g: Game, r: Region | undefined | null): Island | null {
  if (!ROWS || !r || !farOn(g)) return null;
  const row = ROWS[r.name];
  return row ? { room: row[0], floor: row[1] === 'quarter' ? 0.25 : 0.5, islet: row[2] === 1 } : null;
}

/**
 * The corps of a side an island holds are counted in men, its room in divisions' worth of men (the design director,
 * 25 November 2026, 14:28Z, (a): "An island's room is its table figure in divisions' worth of men; a side's corps fit
 * while their men together are within it"; ruling 173(b), 179 rider (s)). A division's worth is the game's own
 * division, 15,000 men at full strength (MEN_PER_DIV), for every nation, and a corps' men are the corps card's own
 * measure, game.ts `men` (divisions x strength x 15,000; breakup.ts `menOf` is the same function). (The design
 * director, 21:37Z, 26 November 2026, 7: "the room and its sentence read a corps' men through the same function the
 * corps card shows (menOf), 15,000 a division's worth, never a second count", China's corps too; and the UI director,
 * 21:37Z, 26 November 2026, 1: a corps' men as its own card shows them. China's corps are counted at 15,000 a division
 * here, as their corps cards count them, not at the 67,000 of China's reserve sentences.)
 *
 * What counts is as ruled at 13:58Z: the corps of the side on the island's ground and in its town, for their own side
 * only; engineers apart, still in corps, up to `engRoom`; fortress crews apart by their own kind, whatever the fortress
 * rows' switch says (the design director, 00:20Z, 27 November 2026, D4: `counted`); a retreat onto
 * an island at room plus two, now two divisions' worth (`fits` with `slack` 2). Men are read as they stand: a worn
 * corps takes less room than a full one, and drafts that raise the men on an island over its room move nobody (the
 * design director, 21:37Z, 26 November 2026, 4: "drafts are never refused for room, because a refilled corps is the
 * same corps. The island refuses only new corps"; an island over its room refuses only new corps of that side).
 * Nobody is ever moved or struck off to keep it.
 *
 * (b) A corps larger than the island's whole room does not fit, for every seat: it falls out of `fits` with nothing
 * more, since its men alone exceed the room on an empty island. It may split to fit where the split rules allow
 * (breakup.ts), which no computer nation does (the design director, 21:37Z, 26 November 2026, 2: "a wave is the corps
 * that fit, in plan order; if none fits, the shore is full and the staff goes to the plan's next shore"; no computer
 * splits). One already there at the start is the record's hand and
 * stays: nothing here moves anybody.
 *
 * The record's own hand: at stops 1 and 2 of the dial (the stops that keep the book) a dated row is put down at the
 * record's place counted against five as before, and the book's stations taken (game.ts roomNear and the row offer,
 * stations.ts). At stops 3 to 5 the record's place reads `fits` like every other placing (the design director,
 * 21:37Z, 26 November 2026, 1: "at stops 3 to 5 the record's place reads the room in men like every other placing;
 * stops 1 and 2 as built"; her 13:58Z rested on a retirement of dated rows not on this base, and none is built here).
 * The other reading, the record's hand reading
 * the islands' room at stops 1 and 2 as well, was built and measured on 25 November 2026 with room counted in corps and
 * Singapore at room 2, and left out: by the book the British divisions of Malaya could not step back onto Singapore
 * Island, and the guard rows of Kwajalein, Yap and Chichi Jima waited two months for their landing forces.
 *
 * With the far side off, and for ground that is no island of the table, nothing here answers in men: a province holds
 * five corps (STACK) and every reader's own count is compared with five exactly as it always was (`rows`).
 */

/** The men of a corps, as its card shows them (game.ts `men`). */
const menOfCorps = (c: Corps): number => men(c);

/**
 * The men of nat's side standing on an island, in the field and in the town, engineers and fortress crews left out,
 * and the corps in `going` left out (they are counted as going, not as there).
 */
function menOn(g: Game, r: Region, nat: string, going: readonly Corps[]): number {
  let n = 0;
  for (const u of placed(g, r, nat)) if (counted(u) && !going.includes(u)) n += menOfCorps(u);
  for (const u of r.garr ?? []) if (allied(g, nat, u.nat) && !roomless(g, u) && counted(u) && !going.includes(u)) n += menOfCorps(u);
  return n;
}

/**
 * Whether a corps takes an island's room: not an engineer corps (counted apart, in corps), and not a corps of the
 * fortress kind, told by its own kind whatever the fortress rows' switch says (the design director, 00:20Z, 27
 * November 2026, D4: "YES, crews apart whatever the fortress-rows switch says. Room in men asks whether a placing can
 * go there, and a fortress crew is never placed and never moves, so it is told by its own kind and counted once there.
 * One switch never reads another's state to decide its own count. This widens my 13:58Z."). Read on an island alone:
 * nothing else that reads `roomless` or `fortRow` changes.
 */
const counted = (u: Corps): boolean => u.kind !== 'eng' && u.kind !== 'fort';

/** Whether only nat's own corps (none of an ally's) stand on an island, field and town, `going` left out. */
function onlyMine(g: Game, r: Region, nat: string, going: readonly Corps[]): boolean {
  const there = [...placed(g, r, nat), ...(r.garr ?? []).filter((u) => allied(g, nat, u.nat) && !roomless(g, u))];
  return there.every((u) => going.includes(u) || !counted(u) || u.nat === nat);
}

/** Engineer corps of nat's side in the field of r, `going` left out. */
const engThere = (g: Game, r: Region, nat: string, going: readonly Corps[]): number =>
  placed(g, r, nat).filter((u) => u.kind === 'eng' && !going.includes(u)).length;

/**
 * Whether the side's fighting corps on an island's ground, with those going, number no more than any ground holds:
 * five, seven on a retreat (STACK + slack). Engineers and fortress crews apart, as for men (`counted`). The design
 * director, 00:20Z, 27 November 2026, D5: "YES, both: men, and no more corps than any ground holds (five, seven on a
 * retreat)." The field's corps, as the five are counted on any ground (`placed`); the town's are its own.
 */
function fewEnough(g: Game, r: Region, nat: string, gone: readonly Corps[], plan: readonly Corps[], line: number, slack: number): boolean {
  const here = placed(g, r, nat).filter((u) => counted(u) && !gone.includes(u)).length;
  return here + plan.filter(counted).length + line <= STACK + slack;
}

/** How a fit is asked. */
export interface FitAsk {
  /**
   * The reader's own count of corps there, on ground that is no island (and with the far side off): the comparison is
   * then `rows + going.length <= STACK + slack`, the one the reader always made. Left out, the shared count is used:
   * the side's corps in the field, engineers apart (`roomForAll`'s). An island never reads it: there the men decide.
   */
  rows?: number;
  /** With `rows`: the engineers there by the reader's count, engineers then counted apart (`hasRoom`'s). */
  eng?: number;
  /** Room over the island's (in divisions' worth) or the five (in corps): 2 for a retreat. */
  slack?: number;
  /**
   * Corps a plan counts as there already though they stand elsewhere (a staff's copies, weighed and not moved). On an
   * island they are added to the side's men there; elsewhere the reader's own `rows` count them already.
   */
  with?: readonly Corps[];
  /**
   * Corps of the side there that leave as these come (a row made from them, which strikes them as it forms; the corps a
   * swap sends out): on an island they are left out of those there. Elsewhere the reader's own count stands.
   */
  out?: readonly Corps[];
}

/**
 * THE answer to "do these corps of one side fit at r, beside those of their side already there": every rule of room
 * reads it, a march, a train, a ship, a landing, a new corps, the winners of a fight, a retreat, the record's rows at
 * stops 3 to 5 and the staffs' own placing. On an island of the table: the side's men there (`menOn`) and the men of
 * the fighting corps going must be within (room + slack) x 15,000, and engineers going must be within `engRoom` in
 * corps. On all other ground, and with the far side off: the count of corps against five, as it always was.
 */
export function fits(g: Game, r: Region | undefined | null, nat: string, going: readonly Corps[], o: FitAsk = {}): boolean {
  if (!r) return false;
  const slack = o.slack ?? 0;
  const riding = going.filter((c) => c.kind === 'eng').length, line = going.length - riding;
  const isle = islandOf(g, r);
  if (isle) {
    const plan = o.with ?? [], gone = [...going, ...(o.out ?? [])];
    // (A retreat's two places more are for every beaten corps, engineers among them: the design director, 00:20Z, 27
    // November 2026, D2: "YES: room plus two is the retreat's allowance for every beaten corps on any ground, engineers
    // included (the engineer places plus two)". Elsewhere a retreating engineer is one of the seven already, `rows`.)
    if (riding && engThere(g, r, nat, gone) + plan.filter((c) => c.kind === 'eng').length + riding > engRoom(g, r) + slack) return false;
    if (!line) return true;
    let sending = 0;
    for (const c of [...plan, ...going]) if (c.kind !== 'eng') sending += menOfCorps(c);
    return menOn(g, r, nat, gone) + sending <= (isle.room + slack) * MEN_PER_DIV && fewEnough(g, r, nat, gone, plan, line, slack);
  }
  if (o.rows === undefined) {
    const here = placed(g, r, nat).filter((u) => !going.includes(u));
    const eng = here.filter((u) => u.kind === 'eng').length;
    // (Engineers apart: two. Engineers already there over that, the record's or a beaten army's, bar more engineers.)
    return (riding === 0 ? eng <= Math.max(2, engRoom(g, r)) : eng + riding <= engRoom(g, r)) && here.length - eng + line <= STACK + slack;
  }
  if (o.eng !== undefined) return riding ? o.eng + riding <= engRoom(g, r) : o.rows + line <= STACK + slack;
  return o.rows + going.length <= STACK + slack;
}

/**
 * THE room left at r for nat's side: on an island of the table, in divisions' worth of men (men / 15,000, not
 * rounded, never below none); on all other ground and with the far side off, five less the reader's own count
 * (`rows`, or the side's corps in the field, engineers apart). "Full" is `roomLeft(...) < 1` where no corps is at hand.
 * `without`: corps of the side left out of those there on an island (a staff's own corps, when it asks what room its
 * allies and the town leave it); the reader's `rows` leave them out elsewhere.
 */
export function roomLeft(g: Game, r: Region | undefined | null, nat: string, rows?: number, without: readonly Corps[] = []): number {
  if (!r) return 0;
  const isle = islandOf(g, r);
  if (isle) return Math.max(0, isle.room * MEN_PER_DIV - menOn(g, r, nat, without)) / MEN_PER_DIV;
  return STACK - (rows ?? placed(g, r, nat).filter((u) => u.kind !== 'eng').length);
}

/**
 * The corps of `list`, in plan order, that still go to r: each that fits beside those of its side there and those of
 * the list taken before it; a corps too large for what is left (or for the whole room) is passed over and the next
 * one asked (the design director, 21:37Z, 26 November 2026, 2). A helper over `fits`, not a second count. On ground
 * that is no island, the
 * whole list, as a landing was always counted there (callers cap it by their own lift and five).
 */
export function fitWave<C extends Corps>(g: Game, r: Region | undefined | null, nat: string, list: readonly C[]): C[] {
  if (!r || !islandOf(g, r)) return [...list];
  const out: C[] = [];
  for (const c of list) if (fits(g, r, nat, [...out, c])) out.push(c);
  return out;
}

/** How many of `list` still go to r (`fitWave`'s length); Infinity on ground that is no island, as `shoreRoom` was. */
export const fitCount = (g: Game, r: Region | undefined | null, nat: string, list: readonly Corps[]): number =>
  r && islandOf(g, r) ? fitWave(g, r, nat, list).length : Infinity;

/**
 * The too-large line (b), said before anything of how full the island is today (the UI director, 15:02Z, section 3):
 * the first corps of the order larger than the island's whole room, at its card's own figure, with "split it and send
 * a part" where the split rules let him split it; where more than one is too large, a sentence counts the rest so he
 * is not refused again for the next (the UI director's answer, 21:37Z, 26 November 2026, 3). Null where none is too
 * large.
 */
function tooLarge(g: Game, r: Region, line: readonly Corps[]): string | null {
  const isle = islandOf(g, r);
  if (!isle) return null;
  const all = line.filter((c) => menOfCorps(c) > isle.room * MEN_PER_DIV);
  if (!all.length) return null;
  const big = all[0], head = `${r.name} has ${roomWords(isle.room)}. ${big.name} has ${cardMen(big)}`;
  const first = whyNotBreakUp(g, big, [0]) === null ? `${head}, too many to go there whole: split it and send a part.` : `${head}: too many for the island.`;
  const more = all.length - 1;
  return more ? `${first} ${Count(more)} more of the corps you are sending ${more === 1 ? 'is' : 'are'} too large for ${r.name} as well.` : first;
}

/**
 * What the swap card says where two corps cannot change places onto an island for want of room (the UI director's
 * answer of 21:37Z 26 November 2026 on the mapper's Q6): the too-large line first; else the section 3 sentence with
 * "there" counting only the men who stay after the swap. Null where the swap fits, to the last man.
 */
export function swapRefusal(g: Game, r: Region, c: Corps, out: Corps): string | null {
  const isle = islandOf(g, r);
  if (!isle || swapFits(g, r, c, out)) return null;
  const big = tooLarge(g, r, [c]);
  if (big) return big;
  const room = isle.room * MEN_PER_DIV, stay = menOn(g, r, c.nat, [c, out]);
  const yours = onlyMine(g, r, c.nat, [c, out]) ? ' of yours' : '';
  // (Fewer than 1,000 more fit after the swap: section 3's "full" line with the swap's one change, so the line never
  // promises room that is not there; the words sceptic's lean of 26 November 2026, laid until the UI director rules.)
  if (room - stay < 1000) return `${r.name} is full: it has room for about ${roomMen(room)} men and ${roomMen(stay, 'up')}${yours} stay there after the swap.`;
  return `${r.name} has ${roomWords(isle.room)}, and ${roomMen(stay, 'up')}${yours} stay there after the swap: only ${roomMen(room - stay, 'down')} more can go, and ${c.name} has ${cardMen(c)}.`;
}

/**
 * Whether corps `c` may change places with `out` onto island r (the swap, game.ts whyNoSwap): ruled by the design
 * director, 21:37Z, 26 November 2026, 5: "allowed while the side's men there after it fit or are no more than before;
 * a corps larger than the whole room never swaps on". A corps larger than the island's whole room never swaps
 * on; otherwise the swap stands while the side's men there after it fit (the corps going out left out), or are no more
 * than before. Engineers change places only with engineers, so their count does not change. True on all other ground.
 */
export function swapFits(g: Game, r: Region, c: Corps, out: Corps): boolean {
  const isle = islandOf(g, r);
  if (!isle || c.kind === 'eng') return true;
  if (menOfCorps(c) > isle.room * MEN_PER_DIV) return false;
  return menOfCorps(c) <= menOfCorps(out) || fits(g, r, c.nat, [c], { out: [out] });
}

/** The island's room said once: "room for 4 divisions, about 60,000 men", "room for one division, about 15,000 men". */
const roomWords = (room: number): string =>
  `room for ${room === 1 ? 'one division' : `${room} divisions`}, about ${(room * MEN_PER_DIV).toLocaleString('en-US')} men`;

/**
 * A figure of men in the room's sentences (the UI director's marks of 15:02Z 25 November 2026, section 1, and her
 * answer, 21:37Z, 26 November 2026, 2): thousands with the comma, under 1,000 the figure whole.
 * 'down' for "more can go", worked from the true figures and rounded DOWN, so the line never promises room that is not
 * there; 'up' for the men there, so that there and more add up to the room the line shows; 'near' (the nearest
 * thousand) for what he is sending and for a shortfall.
 */
export function roomMen(n: number, how: 'down' | 'up' | 'near' = 'near'): string {
  const m = Math.max(0, Math.round(n));
  return roomFigure(m, how).toLocaleString('en-US');
}
const roomFigure = (m: number, how: 'down' | 'up' | 'near'): number =>
  (m < 1000 ? m : (how === 'down' ? Math.floor(m / 1000) : how === 'up' ? Math.ceil(m / 1000) : Math.round(m / 1000)) * 1000);
/** A corps' men as its card shows them: the exact figure (ui/main.ts `num(men(c))`). */
const cardMen = (c: Corps): string => menOfCorps(c).toLocaleString('en-US');
/** "Two", "One": a count opening a sentence. */
const Count = (k: number): string => { const w = ['One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten'][k - 1]; return w ?? `${k}`; };

/**
 * The head of the swap card on an island (the UI director, 15:02Z, section 2): "Okinawa has room for 4 divisions, about
 * 60,000 men." Null for ground that is no island of the table (and with the far side off), where each rule keeps the
 * sentence it had.
 */
export function roomSays(g: Game, r: Region | undefined | null): string | null {
  const isle = islandOf(g, r);
  return isle && r ? `${r.name} has ${roomWords(isle.room)}.` : null;
}

/**
 * How many engineer corps a province takes apart from its fighting corps: two, or on an island of the table the
 * lesser of two and the island's room (the design director, 13:58Z 25 November 2026: "Wake, a room of one, takes one
 * engineer corps apart; a room of two or more takes two, as anywhere"). Two with the far side off. Counted in corps.
 */
export const engRoom = (g: Game, r: Region | undefined | null): number => Math.min(2, islandOf(g, r)?.room ?? 2);

/**
 * What an island tells the player whose order it has no room for, in the UI director's words of 15:02Z 25 November
 * 2026 (section 3): the room, what is there, how much more fits and what he is sending, in that order, in men. A corps
 * larger than the island's whole room is told first (the UI director's marks, 15:02Z, section 3): the first such corps
 * of the order, with "split it and send a part" where the split rules let him split it, and where more than one is too
 * large one sentence counts the rest (the UI director, 21:37Z, 26 November 2026, 3). `corps` are the corps ordered there;
 * those already on the island are not counted as there, and the corps in its town are. "Of yours" is dropped where an
 * ally's corps stand there too. Engineers are told their own count in corps (section 4). Null for ground that is no
 * island of the table, with the far side off, and where room is not what stops the order.
 */
export function roomRefusal(g: Game, r: Region | undefined | null, corps: Corps[]): string | null {
  const isle = islandOf(g, r);
  if (!isle || !r || !corps.length) return null;
  const nat = corps[0].nat, room = isle.room * MEN_PER_DIV, line = corps.filter((c) => c.kind !== 'eng');
  const big = tooLarge(g, r, line);
  if (big) return big;
  if (line.length && !fits(g, r, nat, line)) {
    // (The men fit and the count does not: the sentence the game says on any ground that holds five (game.ts
    // whyNoTrainFor and the march's), the design director, 00:20Z, 27 November 2026, D5: "use the sentence the game
    // already says on any ground that holds five".)
    if (!fewEnough(g, r, nat, corps, [], line.length, 0) && menOn(g, r, nat, corps) + line.reduce((n, c) => n + menOfCorps(c), 0) <= room) return `${r.name} has no room for ${line.length === 1 ? 'another corps' : `${line.length} more corps`}.`;
    const there = menOn(g, r, nat, corps), sending = line.reduce((n, c) => n + menOfCorps(c), 0), left = room - there;
    const yours = onlyMine(g, r, nat, corps) ? ' of yours' : '';
    // (What he sends would read at or under what can go once rounded: say the shortfall instead, the UI director's
    // answer of 21:37Z 26 November 2026, as the rifles' fifth read.)
    const short = roomFigure(Math.round(sending), 'near') <= roomFigure(Math.max(0, Math.round(left)), there === 0 ? 'near' : 'down');
    const sends = short ? `you are sending ${roomMen(sending - left)} men more than fit` : `you are sending ${roomMen(sending)}`;
    if (there === 0) return `${r.name} has ${roomWords(isle.room)}: ${sends}.`;
    if (left < 1000) return `${r.name} is full: it has room for about ${roomMen(room)} men and ${roomMen(there, 'up')}${yours} are there.`;
    if (short) return `${r.name} has ${roomWords(isle.room)}, and ${roomMen(there, 'up')}${yours} are there: ${sends}.`;
    return `${r.name} has ${roomWords(isle.room)}, and ${roomMen(there, 'up')}${yours} are there: only ${roomMen(left, 'down')} more can go, and ${sends}.`;
  }
  const riding = corps.length - line.length;
  if (!riding) return null;
  const most = engRoom(g, r), here = placed(g, r, nat).filter((u) => u.kind === 'eng' && !corps.includes(u));
  const n = here.length, left = Math.max(0, most - n);
  if (riding <= left) return null;
  const count = (k: number): string => (k === 1 ? 'one' : `${k}`);
  const has = `room for ${count(most)} engineer corps as well as its troops`, mine = here.every((u) => u.nat === nat);
  if (n === 0) return `${r.name} has ${has}: only ${count(most)} can go.`;
  const are = n === 1 ? (mine ? 'one of yours is there' : 'one is there') : `${n}${mine ? ' of yours' : ''} are there`;
  if (left > 0) return `${r.name} has ${has} and ${are}: only ${count(left)} more can go.`;
  return `${r.name} can take no more engineers: it has ${has}, and ${most === 1 && n === 1 && mine ? 'yours is there' : are}.`;
}

/** Every island of the table, by its province's name (for the lists an interface or a test draws). */
export const islandNames = (): string[] => (ROWS ? Object.keys(ROWS) : []);

/**
 * The look an island's field is dressed in when its battle is fought in the 3D game (the design director, 25 November
 * 2026, 12:39Z, B(3), narrowed at 13:00Z, D(2)): "jungle" where the table's jungle column says 1 (a big island's fight
 * was inland), "island" (beach, reef and palms) where it is an islet, as the table's own columns give it
 * (tools/make-far-islands.mjs, lookOf). No word ('') for every other island, which the 3D game dresses by its own
 * climate from the place's latitude and longitude; for Attu, Kiska, Singapore and Iwo Jima until the art director has
 * scored stills of them; for ground that is no island of the table; and with the far side off. Looks only: the battle's
 * ticket carries it for the 3D game (battle-bridge.ts, battleLook), and no rule of the campaign or of the fight reads it.
 */
export function islandLook(g: Game, r: Region | undefined | null): 'island' | 'jungle' | '' {
  if (!ROWS || !r || !farOn(g)) return '';
  return ROWS[r.name]?.[3] ?? '';
}
