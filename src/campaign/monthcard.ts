/**
 * The month's report on one card (Version 82). Display only: no rule reads it, and it changes no game, no save
 * and no history hash. The design director's rulings 64 and 65 set what the card holds and in what order; the art
 * director's brief (art-director/turn-report) sets its look. Three calls serve both interfaces:
 *
 *   monthMark(g)         what an interface keeps as the player's month opens, before he gives an order
 *   monthOrders(g)       what he has on order, kept again in the mark as he ends his month (mark.orders)
 *   monthCard(g, mark)   the card, when his next month opens, for everything that has happened since the mark
 *
 * So the card counts his own attacks of the month with everything the others did after it, and what was delivered
 * is told from what he had on order when he handed the month over.
 *
 * The order (ruling 64): what needs his decision first, his own war before anyone else's, bad news before good of
 * the same weight. 1 warnings that need a decision; 2 war and peace that involve him; 3 ground, cities lost, then
 * cities won, then provinces as one count a front; 4 sea and air, losses first; 5 goals and honours; 6 home;
 * 7 elsewhere; 8 letters. Within a class the larger first. Classes 1 and 2 are the red bands at the top (at most
 * two; a great power entering the war or falling always takes the first), and the pictured lead story is the first
 * row from class 3 down.
 *
 * "The larger first" (rulings 84 and 95): every item has a weight, and what is stamped LOST ranks at twice its
 * weight, so bad news leads good news until the good news is twice its size. HELD and WON count once.
 *   On the ground the weight is his side's men, lost or cut off, and the place adds by its rank on the map: a town
 *   half a corps, a city a corps and a half, a capital more than any ordinary battle of the month, open country
 *   nothing. A ring that closes weighs all the men cut off. Battles, sieges, rings and surrenders are one list; the
 *   provinces lost or won without a battle come after it, one count a front.
 *   At sea and in the air the weight is what both sides lost, ships by class and aircraft by number, and a fleet
 *   meeting a fleet is never ranked below a single ship lost alone.
 * One measure runs across the three: the cost to build what was lost, counted in men (an infantry corps is its
 * men; a ship or an aircraft is the men its cost would have raised). The pictured lead story is the heaviest item
 * of ground, sea and air together, and a light ground item (a count, or a battle that cost less than a corps) never
 * takes the picture from a fleet action or a great ship sunk. The rows keep land first, then sea and air.
 */
import airNames from '../../data/campaign/air-names.json';
import { turnToDate } from '../core/ticket';
import { staffRuns, type DeskId } from './depth';
import { CAPITAL_BESIEGED, fallWarning, withinReach } from './danger';
import { NO_LAST_MONTH, ROME_HOLDS, majorsOf, stakeDue, stakeWarning } from './diplomacy';
import { ARSENAL, workLevel, yardLevel } from './arsenal';
import { BUDGET, PRODUCTION, ledger } from './economy';
import { allied, atWar, men, theName } from './game';
import { CLASSES, NAVY, shipCount, shipList } from './navy';
import { cityOrders } from './orders';
import { besieged, garrisonOf, guardMen } from './siege';
import { prizeOf, townName, townSize } from './towns';
import { skyView, type SkyView } from './weather';
import { menRules, raisable } from './manpower';
import { bureauCost } from './research';
import { MEN_PER_DIV, type AirReport, type Game, type Headline, type SeaSide, type ShipClass, type Ships } from './types';
import { seaSaid, seaWhere } from './sea-words';
import { rowsWaitingOnRows, struckOnCard } from './arrivalwaits';

export type CardTone = 'good' | 'bad' | 'warn' | 'plain';
/** The eight pictures both builds have (art-director/raid-card/targets). */
export type CardPicture = 'troops' | 'fieldworks' | 'town' | 'fleet' | 'factories' | 'rail-yard' | 'harbour' | 'airfield';
export interface CardFigure { n: string; label: string }
/** The battle report a row opens. */
export interface CardReport { type: 'land' | 'sea' | 'air' | 'shore'; id: number }

/** Something on order: a unit or a work, and where it will stand. */
export interface MonthOrder { id: number; what: string; unit: boolean; mark: string; label: string; plural: string; name: string; region: number; /** The work's sign ('field', 'aa'...): a field is told by it, not its label, which the airfield switch renames. */ sign?: string }

/** What an interface keeps as the player's month opens: where each list stood and who held what; and, as he ends it, what was on order. */
export interface MonthMark {
  turn: number;
  h: number; r: number; s: number; a: number; shore: number;
  /** Who held each province. */
  before: string[];
  /** The countries at war with anyone: a great power not among them that is at war a month later has entered the war. */
  warring: string[];
  orders: MonthOrder[];
  /** The enemy was within reach of his capital, or his country could give in, as the mark was taken (danger.ts): so that the card says it once. */
  reach?: boolean;
}

/**
 * A band: class 1 of the ranking, the dated warning, red; class 2 (WAR, FALLEN, FREED) brass on the dark ground, since red
 * is the warning's alone (ruling 160); and NEWS, brass too, for a line of news with a title of its own that is no war
 * and no fall (Britain stays out of Canada's war: ruling 211(b)).
 */
export interface CardBand {
  word: 'WAR' | 'WARNING' | 'FALLEN' | 'FREED' | 'NEWS';
  /** A warning's own short title ('Coast watch'). */
  title?: string;
  /** The flags, at most two. */
  nats: string[];
  /** The headline, of about eight words, and the whole line of news it was cut from. */
  head: string;
  text: string;
  week: number;
  /** The month the news is dated in, where that is not the card's own ('July'): the days after the month turned, before his own month opened. */
  month?: string;
  /** Its date as the card prints it: 'week 3', or 'as July opens' for news of the month that has just turned (see dateWords). */
  when: string;
  /** A place on the map to show: the news's own, or the capital of the country it names. */
  region?: number;
  /** FREED by his own side: the band's field and left edge take the rows' good-news green (the art director, 82.1). */
  good?: boolean;
  /**
   * A country in danger whose warning ends with the help of his home ground's first alarm (ruling 211(c)): SHOW ON MAP
   * does what a press on the tray's red row does, picking up that army and centring the town (the UI director's words, section 2).
   */
  alarm?: boolean;
  /**
   * The month of grace is spent (ruling 120): the standing warning of the months after a LAST MONTH lead, while the
   * campaign goes on. Its head is the warning's own title ('Rome holds, for now'), the label WARNING sits on a cream
   * plate in red letters and the band's bright edge turns cream (the art director's turn-report brief), so it cannot
   * be taken for the band of the months before.
   */
  spent?: boolean;
  /**
   * The sentence that says what to do, printed small where the date stands: with `spent`, 'There will be no last
   * month: hold all six.'; on the band of a country in danger (danger.ts), 'Hold Berlin, or win back home ground.'
   */
  note?: string;
  /**
   * A second line under the headline, printed in full and never cut: a band that must be read whole (ruling 133(a),
   * the country in danger: what is lost, and that the campaign ends with it). The band is as tall as the line needs.
   */
  sub?: string;
}

/** One of the six score boxes. A box with nothing to count is left out. */
export interface CardBox {
  key: 'provinces' | 'battles' | 'men' | 'sea' | 'air' | 'treasury';
  label: string;
  figure: string;
  sub: string;
  tone: CardTone;
}

/** A row of the player's fronts. The lead story is a row too, drawn large. */
export interface CardRow {
  /** Ruling 64's class: 2 for a band's item that found no band (red for a warning, else brass), 3 ground, 4 sea and air, 5 goals and honours. */
  cls: number;
  arm: 'land' | 'sea' | 'air' | null;
  picture: CardPicture | null;
  /** The sign in the disc: a key of the order signs ('inf', 'arm', 'art', 'hull', 'cv', 'ss', 'bmr', 'ftr', 'eng', 'para', 'star'), or '' for none. */
  sign: string;
  place: string;
  /** One word, or '': WON, LOST, HELD, BESIEGED, HIT, MISSED, SUNK, BLOCKED. The row's edge and its tag share the tone. */
  tag: string;
  /**
   * The word that goes with the tag and the other side's name in words, beside its flag, so that the row reads whole
   * with no picture (the UI director's words for ruling 211, section 5): "Monterrey WON from Mexico", "Guadalajara
   * LOST to Mexico", "Seattle HELD against Canada", "Pittsburgh BESIEGED by Canada", "Toronto WON against Canada".
   */
  join?: { word: string; name: string };
  tone: CardTone;
  /** The flag on the left and the flag after "against". */
  ours: string | null;
  theirs: string | null;
  /** One sentence. */
  line: string;
  /** One figure, or two for a land battle: ours first, theirs second. */
  figures: CardFigure[];
  /** The place a pin shows on the map, a province or a sea of the chart; and the battle report the row opens. */
  region?: number;
  sea?: number;
  report?: CardReport;
  week: number;
  /** The month the item is dated in, where that is not the card's own ('July'). */
  month?: string;
  /** Its date as the card prints it: 'week 3', or 'as July opens' (see dateWords). */
  when: string;
  /** For the lead story: the kind word of its tag, a headline of about eight words, one line on what follows from it, and three figures. */
  kind: string;
  head: string;
  follows: string;
  facts: CardFigure[];
  /**
   * The lead of the month a campaign's stake falls due on him (ruling 118(f); see stakeDue): no picture, the frame in
   * the warning band's red, `ours` the flag drawn large, the tag LAST MONTH on a cream plate, `line` the short line
   * under the headline in place of the two flags, `follows` the sentence under the frame, two figures, and SHOW ON
   * MAP alone. That month the card has no warning band of the same title.
   */
  stake?: boolean;
}

export interface CardHome {
  sign: string; count: number; text: string; place: string; region?: number;
  /** Next month's: drawn in grey. */
  next?: boolean;
  /** Works that are not counted (ruling 133(g)): 'Anti-aircraft guns ready', with the place and the level under it and no number before it. */
  bare?: boolean;
}
export interface CardStaff { /** The whole line of news, and its short form for the card: the desk, then what it did ('Research: 2 teams set to work'). */ text: string; short: string; desk: string; sign: string; /** True while the staff still runs that desk: the button takes it back from next month. */ mayTake: boolean }
export interface CardElse { nat: string; text: string; /** The whole line of news. */ full?: string; region?: number }
export interface CardLetter { text: string; by: string; unit?: number }

export interface MonthCard {
  /** The month reported, as a turn and in words; and whose card it is. */
  turn: number;
  month: string;
  viewer: string;
  /**
   * The card that opens a campaign (ruling 123(b)): it reports no month of his, so it is headed by the moment and
   * not by a month ('As July 1943 opens': see cardHead), and its fronts line never calls the month quiet.
   */
  opening?: boolean;
  bands: CardBand[];
  boxes: CardBox[];
  lead: CardRow | null;
  /** In the rows' place when nothing was fought: 'A quiet month.', or under a band that there was no fighting on his fronts. Empty otherwise. */
  quiet: string;
  /** Every row but the lead, in order. `show` of them show at first; `count` is the fold's label ('ALL 9 BATTLES'). */
  rows: CardRow[];
  show: number;
  count: string;
  /** A line under the rows: the convoys' month. */
  foot: string;
  /** At home: what was delivered, what was researched, anything else of his own, and next month's deliveries. */
  home: CardHome[];
  /**
   * The line when men run out (ruling 133(d)(1)): the reserve cannot fill a corps of one division and the treasury
   * holds more than three months' income. The first line AT HOME, with OPEN THE ARSENAL beside it; the first month
   * and then every third while it lasts (menOutLine).
   */
  menOut?: string;
  research: string[];
  /** Other news of home, each cut to a line or two of whole words; and the same lines whole, for a hover. */
  also: string[];
  alsoFull: string[];
  /** What his staff did. It stands: the button takes the desk back from next month. */
  staff: CardStaff[];
  elsewhere: CardElse[];
  letters: CardLetter[];
  /** Lines of the month's news that are not on the card. They are all in the newsreel. */
  more: number;
  /**
   * THE SKY (weather, rulings 127 and 143(b)): after the rows of news and before AT HOME, never a warning. Closed zones
   * where he flies or fights first, a line each; cloud on one line; then one line for the clear sky, always. Only in a
   * campaign with weather.
   */
  sky?: SkyView;
  /**
   * The telling (arrivalwaits.ts): a muted line for each of his dated arrivals that waits only for another arrival to
   * form, every month it waits; first, in the month a told row held for its place is struck, the struck line that ends
   * its muted line (`struckOnCard`, the UI director's section 11 (a)), on the strike month's own card only (her
   * section 12, A1). Absent where there is none.
   */
  waits?: string[];
}

const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);
const plural = (n: number, one: string, many = `${one}s`): string => `${n} ${n === 1 ? one : many}`;
const WORDS = ['No', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten'];
const count = (n: number): string => WORDS[n] ?? String(n);
/** The first sentence of a line of news, without its full stop. */
// (A full stop after an abbreviation or an initial ends no sentence: "No. 1 Bomber Group bombs Hamburg." The Playtester, Version 82.1.
// Nor does a German ordinal at the head of a name, "2. Fliegerdivision bombs Rotterdam": the third playtester, 84.3.1. It is
// taken for one at the head of the line, after "the" or in a bracket, or wherever the word after it is one that follows
// a number in the air forces' names (ruling 220's ladders: Fliegerdivision, Jagddivision, Dywizjon...), so "3. Fliegerdivision
// and 1. Jagddivision strike..." holds together and "reduced to 2. The army..." still ends there.)
const ABBR = /(?:^|[\s(])(?:Nos?|St|Mt|Ft|Gen|Lt|Col|Maj|Capt|Adm|Dr|Mr|Mrs)\.$|(?:^|[\s.(])[A-Z]\.$|(?:^|\b[Tt]he |\()\d{1,2}\.$/;
const reEsc = (s: string): string => s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
const ORDINAL_WORDS = [...new Set(Object.values((airNames as unknown as { ladders: Record<string, { form: { ftr: string; bmr: string } }[]> }).ladders)
  .flat().flatMap((r) => [r.form.ftr, r.form.bmr]).map((f) => /^\{n\}\. (\S+)/.exec(f)?.[1]).filter((w): w is string => !!w))];
const ORDINAL_NEXT = new RegExp(`^ (?:${ORDINAL_WORDS.map(reEsc).join('|')})(?=[\\s,.;:)]|$)`);
const first = (text: string): string => {
  const stop = /[.!?](?=\s|$)/g;
  for (let m = stop.exec(text); m; m = stop.exec(text)) {
    const head = text.slice(0, m.index + 1);
    const ordinal = /\b\d{1,2}\.$/.test(head) && ORDINAL_NEXT.test(text.slice(m.index + 1));
    if (m[0] !== '.' || !(ABBR.test(head) || ordinal)) return head.replace(/[.!]$/, '');
  }
  return text.replace(/[.!]$/, '');
};
/** A line cut to a length a row can carry: at a colon or a comma if one falls in the last third, else at a word. */
const brief = (text: string, max: number): string => {
  if (text.length <= max) return text;
  const cut = text.slice(0, max + 1);
  const stop = Math.max(cut.lastIndexOf(': '), cut.lastIndexOf(', '), cut.lastIndexOf('; '));
  if (stop >= max * 0.6) return cut.slice(0, stop);
  return `${cut.slice(0, Math.max(cut.lastIndexOf(' '), max * 0.6))}…`;
};
/** A row's one sentence, kept to the room the row has: whole where it fits, else cut at a clause or a word. */
const sentence = (text: string, max: number): string => {
  if (text.length <= max) return text;
  const cut = brief(text.replace(/\.$/, ''), max - 1);
  return /[….]$/.test(cut) ? cut : `${cut}.`;
};
/**
 * A line for one of the foot boxes, kept to the room its lines have (the boxes are set in a typewriter face, so a
 * line's room is a count of letters). No word is cut and no name is cut in half: the whole sentence where it
 * wraps into the lines it has, else whole words, backing off from the middle of a name and from a word that
 * leads nowhere. A cut shows itself: it ends after a whole word with "…", never with a full stop where the
 * sentence did not end (the art director's rider of 2026-11-16).
 */
const linesAt = (text: string, per: number): number => { let n = 1, at = 0; for (const w of text.split(' ')) { if (at && at + 1 + w.length > per) { n++; at = w.length; } else at += (at ? 1 : 0) + w.length; } return n; };
const LITTLE = /^(a|an|the|of|at|in|on|to|with|for|from|by|and|or|but|its|their|his|is|are|was|were|be|been|has|have|had|can|could|will|would|may|must|not|no|it|they|he|we|that|which|as|if|so|than|then|when|while|before|after|until|because|about|out|up|off|into|over|under|near|round|against|between|who|whose)$/i;
export const wholeWords = (text: string, per: number, lines = 2): string => {
  const t = text.replace(/\s+/g, ' ').trim().replace(/[.]$/, '');
  const fits = (s: string): boolean => linesAt(s, per) <= lines;
  if (fits(`${t}.`)) return `${t}.`;
  const all = t.split(' ');
  const kept = [...all];
  const capital = (w: string | undefined): boolean => !!w && /^[A-Z0-9]/.test(w);
  const said = (): string => `${kept.join(' ').replace(/[,;:]$/, '')}…`;
  const back = (): void => { while (kept.length > 1 && (!fits(said()) || LITTLE.test(kept[kept.length - 1]) || (capital(kept[kept.length - 1]) && capital(all[kept.length])))) kept.pop(); };
  back();
  // ("Between" needs both its places: a line that has room for one of them ends before the word.)
  const pair = kept.lastIndexOf('between');
  if (pair > 0 && !kept.slice(pair).includes('and')) { kept.length = pair; back(); }
  return said();
};
/**
 * A cut that still says what the line had to say. A line that leads in with a colon has its point after it: the
 * cut keeps the whole of the first clause after the last colon, or the line is not cut. Any other keeps most of
 * its sentence, or it is not shown cut.
 */
const keepsPoint = (whole: string, cut: string): boolean => {
  const kept = cut.replace(/…$/, '');
  const colon = whole.lastIndexOf(': ');
  if (colon >= 0) { const end = whole.slice(colon + 2).search(/[,;] /); return end >= 0 && kept.length >= colon + 2 + end; }
  const all = whole.split(' ').length, n = kept.split(' ').length;
  return n >= 5 && n >= Math.ceil(all * 0.6);
};
/**
 * Fits lines of news into a foot box (the art director's rider of 2026-11-16, in place of "two lines, then cut").
 * Each is given whole while the box's lines last. One that will not fit is cut, the last first, and the box ends
 * there; but a line that loses its point when cut is not cut: it stays in the newsreel and the next is tried in
 * its place. Returns what is shown, each with its place in the list it came from.
 */
export function fitFoot(items: string[], per: number, lines: number, most: number): { text: string; at: number }[] {
  const out: { text: string; at: number }[] = [];
  let left = lines;
  for (let i = 0; i < items.length && out.length < most && left > 0; i++) {
    const whole = `${items[i].replace(/\s+/g, ' ').trim().replace(/[.]$/, '')}.`;
    const need = linesAt(whole, per);
    if (need <= left) { out.push({ text: whole, at: i }); left -= need; continue; }
    const cut = wholeWords(whole, per, left);
    if (keepsPoint(whole, cut)) { out.push({ text: cut, at: i }); left = 0; }
  }
  return out;
}
/** The room the foot boxes have, in letters to a line: at home beside a sign and from the box's edge, elsewhere, and what a staff line may run to in all. */
const FOOT_ROOM = { home: 30, homeWide: 35, elsewhere: 36, staff: 44 };
const DESK_WORD: Record<string, string> = { research: 'Research', build: 'Build', towns: 'Towns', diplomacy: 'Diplomacy', spies: 'Spies', air: 'Air', navy: 'Navy' };
/** "Hamburg's", "Calais'". */
const of = (place: string): string => (/s$/i.test(place) ? `${place}'` : `${place}'s`);
/**
 * What a desk did, as the card says it: the desk, then the deed, in about forty letters and never a cut name. `all`
 * is the letters the line may run to in all; Infinity for a card whose lines wrap (the 3D game's), where the first
 * way of saying it is always taken and nothing is cut.
 */
export function staffShort(desk: string, text: string, all: number = FOOT_ROOM.staff): string {
  const word = DESK_WORD[desk] ?? `${desk.charAt(0).toUpperCase()}${desk.slice(1)}`;
  const room = all - word.length - 2;
  const said = first(text).replace(/^Your (advisers|staff|air staff|admirals|envoys|ministers|secret service|engineers|generals) /, '');
  // The first way of saying it that fits; the last is always short.
  const fit = (...tries: string[]): string => { const t = tries.find((x) => x.length <= room) ?? tries[tries.length - 1]; return `${word}: ${t.length <= room ? t : wholeWords(t, room, 1).replace(/\.$/, '')}`; };
  const teams = /^put (a|\d+) research teams? to work on (.+)$/.exec(said);
  if (teams) { const n = teams[1] === 'a' ? 1 : Number(teams[1]); return n === 1 ? fit(`1 team on ${teams[2].replace(/ \(\d+ months?\)/g, '')}`, '1 team set to work') : fit(`${n} teams set to work`); }
  const ordered = /^ordered (.+)$/.exec(said);
  if (ordered) {
    // ("X at P, Y at Q and Z at R": a piece without its place belongs to the one before.)
    const items = ordered[1].split(/, | and /).reduce<string[]>((xs, p) => { if (xs.length && !/ at /.test(p)) xs[xs.length - 1] += ` and ${p}`; else xs.push(p); return xs; }, []);
    const at = items[0].lastIndexOf(' at ');
    const what = (at < 0 ? items[0] : items[0].slice(0, at)).replace(/^the /, ''), where = at < 0 ? '' : items[0].slice(at + 4);
    const more = items.length - 1;
    return more ? fit(`${what}, ${where} and ${more} more`, `${what} and ${more} more`, `${items.length} orders placed`) : fit(where ? `${what}, ${where}` : what, what, '1 order placed');
  }
  const raids = /^flew (a|\d+) raids?: (.+)$/.exec(said);
  if (raids) {
    const n = raids[1] === 'a' ? 1 : Number(raids[1]);
    const cost = Number(/It cost (\d+) aircraft/.exec(text)?.[1] ?? 0);
    if (n > 1) return cost ? fit(`${n} raids, ${cost} aircraft lost`, `${n} raids flown`) : fit(`${n} raids flown`);
    const on = /^the (.+?) at (.+)$/.exec(raids[2]);
    const thing = on ? (/^ships/.test(on[1]) ? 'harbour' : on[1]) : '';
    return on ? fit(`a raid on ${of(on[2])} ${thing}`, `a raid on ${on[2]}`, 'a raid flown') : fit(`a raid on ${raids[2]}`, 'a raid flown');
  }
  const nearer = /^courted (.+?), which has moved a step towards us$/.exec(said);
  if (nearer) return fit(`${nearer[1]} moves a step towards us`, `${nearer[1]} moves a step nearer`, `courted ${nearer[1]}`, 'a country moves a step nearer');
  // Anything else: the first thing it did, and how many more. (A fleet sent to a sea is said the short way.)
  const deeds = said.split(/ and (?=[a-z])/).map((d) => d.replace(/^sent (the .+?) into (the .+)$/, '$1 to $2'));
  return deeds.length > 1 ? fit(`${deeds[0]} and ${deeds.length - 1} more`, deeds[0]) : fit(deeds[0]);
}
/** Other names a line of news gives a country. */
const ALSO_CALLED: Record<string, RegExp> = { UK: /\bBritain\b/, USA: /\bAmerica\b/, SOV: /\bRussia\b/ };

/** The room a band's head has, in letters. */
const BAND_ROOM = 76;
/** The room a row has for its sentence, in letters: less beside two figures. */
const ROW_ROOM = { one: 54, two: 44 };
/** "A", "A and B", "A, B and C". */
const listOf = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
/** A list cut to the room a band's sentence leaves it, before the verb is added: "A, B and 3 more", never a sentence with its verb cut off. */
const listIn = (xs: string[], room: number): string => {
  if (listOf(xs).length <= room) return listOf(xs);
  for (let k = xs.length - 1; k > 1; k--) { const s = `${xs.slice(0, k).join(', ')} and ${xs.length - k} more`; if (s.length <= room) return s; }
  return `${xs[0]} and ${xs.length - 1} more`;
};
/** A country's name as it reads after the first word of a sentence: "The Dominican Republic" becomes "the Dominican Republic". */
const midName = (s: string): string => s.replace(/^The /, 'the ');
const CLASS_WORD = /\b(?:sinks?|torpedoes) (\d+) ([A-Z][a-z]+) (submarines?|destroyers?|cruisers?|battleships?|carriers?|ships?|transports?)/;
/** A fleet's name as it reads in a sentence. */
const the = (name: string): string => (/^the /i.test(name) ? name.replace(/^The /, 'the ') : `the ${name}`);
/** "A Polish", "An Italian". */
const an = (adj: string): string => `${/^[AEIOU]/i.test(adj) ? 'An' : 'A'} ${adj}`;
const SIGNS = new Set(['inf', 'arm', 'mot', 'cav', 'mtn', 'art', 'eng']);
const DESK_SIGN: Record<string, string> = { research: 'star', build: 'inf', diplomacy: 'star', spies: 'star', air: 'ftr', navy: 'hull' };
/**
 * The sign of a build order in the staff's box (the art director's review of 82.1): the soldier is for infantry corps
 * only, so an order that opens with guns, a factory, a fort or another arm takes the star.
 */
function buildSign(text: string): string {
  const what = /ordered (.+?)(?: at |, | and |\.|$)/.exec(text)?.[1] ?? '';
  if (/anti-aircraft/i.test(what)) return 'aa';
  // (City defences: searchlights have no sign of their own yet, and take the star as any other work.)
  if (/searchlights/i.test(what)) return 'star';
  return /corps/i.test(what) && !/armou?red|panzer|tank|artillery|engineer|mountain|airborne|parachute|marine|cavalry|mechani[sz]ed|motori[sz]ed/i.test(what) ? 'inf' : 'star';
}

/** What the player has on order. An interface keeps it in the mark as he ends his month. */
export function monthOrders(g: Game, viewer: string | null = g.human): MonthOrder[] {
  if (!viewer || !g.nations[viewer]) return [];
  return cityOrders(g, viewer).filter((c) => !c.spied).flatMap((c) => c.lines.map((l) => ({ id: l.id, what: l.what, unit: l.unit, mark: l.mark, label: l.label, plural: l.plural, name: l.name, region: l.stands, sign: l.sign })));
}

/** What an interface keeps as the player's month opens. */
export function monthMark(g: Game, viewer: string | null = g.human): MonthMark {
  return {
    turn: g.turn,
    h: g.headlines.length, r: g.reports.length, s: g.seaReports.length, a: g.airReports.length, shore: g.shoreReports.length,
    before: g.regions.map((r) => r.ctrl),
    warring: Object.values(g.nations).filter((n) => n.alive && n.atWar.length > 0).map((n) => n.id),
    orders: monthOrders(g, viewer),
    ...(withinReach(g, viewer) ? { reach: true } : {}),
  };
}

/**
 * The mark for the first month of a war played from its opening. Everything since the game began is that month's
 * news, and the world was at peace before it: so the card for September 1939 carries the declarations that opened
 * the war, though they were made before the player's first month came round. Both interfaces take this mark while
 * the game is in its first month, and monthMark from the second.
 */
export function openingMark(g: Game, viewer: string | null = g.human): MonthMark {
  return { ...monthMark(g, viewer), h: 0, r: 0, s: 0, a: 0, shore: 0, warring: [] };
}
/** The mark as a month opens, whichever month it is. */
export const markFor = (g: Game, viewer: string | null = g.human): MonthMark => (g.turn === 1 ? openingMark(g, viewer) : monthMark(g, viewer));

/** The one measure across land, sea and air (ruling 95): the cost to build what was lost, counted in men. */
const CORPS_MEN = PRODUCTION.builds.inf.divs * MEN_PER_DIV;
const MEN_A_COST = CORPS_MEN / PRODUCTION.builds.inf.cost;
const shipMen = (c: ShipClass): number => (ARSENAL.ships[c].cost / ARSENAL.ships[c].count) * MEN_A_COST;
const shipsWeight = (s: Ships): number => CLASSES.reduce((t, c) => t + shipMen(c) * s[c], 0);
const greatShips = (s: Ships): boolean => s.bb + s.cv > 0;
/** The same from a line of news, which names the class in a word. */
const wordClass = (word: string): ShipClass => (/^battleship/.test(word) ? 'bb' : /^carrier/.test(word) ? 'cv' : /^cruiser/.test(word) ? 'ca' : /^submarine/.test(word) ? 'ss' : 'dd');
const PLANE_MEN = ((ARSENAL.air.ftr.cost / ARSENAL.air.ftr.count + ARSENAL.air.bmr.cost / ARSENAL.air.bmr.count) / 2) * MEN_A_COST;
/** What a place adds to the fight for it, by its rank on the map. A capital adds more than any ordinary battle of the month, and never less than this. */
const PLACE_MEN: Record<string, number> = { capital: 3 * CORPS_MEN, city: 1.5 * CORPS_MEN, town: 0.5 * CORPS_MEN };
/** A besieged town holds "at a cost" where the fight outside it cost half a corps, or more men than are shut in. */
const HEAVY_MEN = 0.5 * CORPS_MEN;
/** A province lost or won without a battle, in a front's count; and news that names no men and no ship. */
const PROVINCE_MEN = 1000, NEWS_MEN = 500, SHELLED_MEN = 2000;
/** The ground's three parts: the fighting in one list, the provinces counted a front, other news. */
const GROUND_FIGHTING = 0, GROUND_PROVINCES = 1, GROUND_NEWS = 2;

interface Ranked {
  row: CardRow; sub: number; weight: number;
  /** A fight for a place: its rank on the map, added to the weight when the month's battles are all known. */
  place?: string;
  /** A fleet meeting a fleet, a single ship lost alone, and whether a battleship or a carrier went down in it. */
  fleet?: boolean; alone?: boolean; great?: boolean;
  /** A ring that closes on a corps or more of his side's: never under the fold (ruling 106). */
  ring?: boolean;
  /** A siege laid, and the losses of the two sides in it: told with the town's own fight if that came the same month. */ laid?: { region: number; ours: number; theirs: number; first: CardFigure } }

/**
 * An item's date in the card's words (the design director's ruling 102, on a June card that said "July, week 4").
 * In the card's own month it is the week. News of the month that has just turned is another country's move, made
 * before the player's own month opens: its week counts that country's month, not his calendar, so the card says only
 * "as July opens". Both builds print this and nothing else for a date.
 */
export const dateWords = (x: { week: number; month?: string }): string => (x.month ? `as ${x.month} opens` : `week ${x.week}`);

/**
 * Whether a card told from this mark is the one that opens a campaign: the mark was taken in the month now opening,
 * so no month of the player's has passed. (A war played from its first month has no such card: its first card
 * tells September 1939.) Both interfaces ask this and pass the answer to monthCard.
 */
export const opensCampaign = (g: Game, mark: MonthMark): boolean => g.turn > 1 && mark.turn === g.turn;

/** What stands on an opening card where another would say the month was quiet: there is no month yet to be quiet. */
export const NOT_YET_FOUGHT = 'Nothing has been fought on your fronts yet.';

/**
 * A card's head in three parts. Every card is headed by the month it reports ('July 1943'); the card that opens a
 * campaign by the moment ('As July 1943 opens'), the month in the title's own colour and the two small words dimmer.
 */
export const cardHead = (card: { month: string; opening?: boolean }): { before: string; month: string; after: string } =>
  (card.opening ? { before: 'As', month: card.month, after: 'opens' } : { before: '', month: card.month, after: '' });

/**
 * The card for everything that has happened since the mark was taken. `opening` marks the card that opens a campaign
 * (see opensCampaign). `whole` is for a card whose rows and boxes wrap and grow (the 3D game's: its UI director's
 * review of build 1628, "no sentence is cut with three dots"): a row's sentence, a desk's line and a foot box's lines
 * are given whole, and a foot box holds as many lines of news as it has lines.
 */
export function monthCard(g: Game, mark: MonthMark, viewer: string | null = g.human, opts: { opening?: boolean; whole?: boolean } = {}): MonthCard {
  const me = viewer ?? '';
  const N = g.nations[me];
  const card: MonthCard = {
    turn: mark.turn, month: turnToDate(mark.turn).label, viewer: me, ...(opts.opening ? { opening: true } : {}), bands: [], boxes: [], lead: null, quiet: '', rows: [], show: 7, count: '', foot: '',
    home: [], research: [], also: [], alsoFull: [], staff: [], elsewhere: [], letters: [], more: 0,
  };
  if (!N) { card.quiet = opts.opening ? NOT_YET_FOUGHT : 'A quiet month.'; return card; }
  const mine = (nat?: string | null): boolean => nat === me;
  // (A line of news may name a country that is not in this game: it is nobody's ally and nobody's enemy.)
  const side = (nat?: string | null): boolean => !!nat && !!g.nations[nat] && (nat === me || allied(g, nat, me));
  const foe = (nat?: string | null): boolean => !!nat && !!g.nations[nat] && atWar(g, nat, me);
  const hs = g.headlines.slice(mark.h);
  const used = new Set<Headline>();
  // (The telling's struck lines, arrivalwaits.ts `struckOnCard`: the card's own from the start, first among the home
  // lines with the waiting lines (section 6), so that no other part of the card takes one and none is counted in
  // "more". The UI director's section 11 (a), 06:56Z, 27 November 2026. Only the strike month's own card shows one; an
  // older struck line, on a card that counts from the start of its month, is claimed without a word: her section 12, A1.)
  const { own: struck, older: struckBefore } = struckOnCard(g, hs, me, mark.turn);
  for (const h of [...struck, ...struckBefore]) used.add(h);
  // The card tells everything since the mark, so its last items are dated in the month that has just opened: the
  // moves of those who go before him. Such an item names its month, or "week 1" under June's title reads as June's.
  const dated = (turn: number): { month?: string } => (turn !== mark.turn ? { month: turnToDate(turn).label.split(' ')[0] } : {});
  // The roll's own entries since the mark. A fall's is the one that says who took the country; and the line that
  // tells a liberation is the roll's line for it: the same week, at the country's capital.
  const roll = g.passings.filter((p) => p.turn >= mark.turn);
  const fellOf = (h: Headline) => { const ps = roll.filter((p) => p.kind === 'fell' && p.nat === h.nat); return ps.find((p) => p.turn === h.turn && p.week === h.week) ?? ps[ps.length - 1]; };
  const freedOf = (h: Headline) => (h.kind === 'event' && h.nat ? roll.find((p) => p.kind === 'freed' && p.nat === h.nat && p.turn === h.turn && p.week === h.week && p.capital === h.region) : undefined);
  /** A country freed that touches his war: its line is a band's, never a row's. */
  const freedBand = (h: Headline) => { const p = freedOf(h); return p && (mine(h.nat) || side(h.nat) || foe(h.nat) || side(p.by) || foe(p.by)) ? p : undefined; };
  const lands = g.reports.slice(mark.r);
  const seas = g.seaReports.slice(mark.s);
  const airs = g.airReports.slice(mark.a);
  const shores = g.shoreReports.slice(mark.shore);
  const ranked: Ranked[] = [];
  const blank = (): CardRow => ({ cls: 3, arm: null, picture: null, sign: '', place: '', tag: '', tone: 'plain', ours: null, theirs: null, line: '', figures: [], week: 1, when: '', kind: '', head: '', follows: '', facts: [] });

  // ---- 3. Ground: the battles his own troops fought --------------------------------------------------------
  let battlesWon = 0, battlesLost = 0, menOurs = 0, menTheirs = 0;
  /**
   * The men shut in a town of his side's that is besieged and holds, as the card is made (rulings 104 and 105): the
   * corps in the town and its guard, which is what a storm would take. They are what he stands to lose, and are in no
   * count of the lost. Nothing where no siege stands there.
   */
  const shutIn = (reg: Game['regions'][number] | undefined): { men: number; corps: number } => {
    if (!reg || !side(reg.ctrl) || !besieged(g, reg)) return { men: 0, corps: 0 };
    const inside = garrisonOf(reg);
    return { men: inside.reduce((n, c) => n + men(c), 0) + guardMen(g, reg), corps: inside.length };
  };
  /** The line beneath the picture for a town of his that is besieged and holds: the price first where it was heavy. */
  const shutWords = (place: string, lost: number, shut: { men: number; corps: number }): string => {
    const corps = shut.corps ? `${plural(shut.corps, 'corps', 'corps')} ${shut.corps === 1 ? 'is' : 'are'} shut in` : '';
    if (lost >= HEAVY_MEN || lost > shut.men) return `${place} holds, at a cost of ${num(lost)} men. ${corps ? `${corps}.` : 'Only its guard is inside.'}`;
    // (The art director's words of 16 November for a town with no corps inside.)
    return `${corps ? `${corps} at ${place}.` : `${place} is held by its guard alone.`} The province is yours while the town holds.`;
  };
  const besiegedTowns = new Set<number>();
  const fought = new Set<number>();
  for (const r of lands) {
    const att = mine(r.attNat) || r.attackers.some((u) => mine(u.nat));
    const def = !att && (mine(r.defNat) || r.defenders.some((u) => mine(u.nat)));
    if (!att && !def) continue;
    fought.add(r.region);
    const reg = g.regions[r.region];
    const size = reg ? townSize(g, reg) : 'village';
    const city = size === 'capital' || size === 'city';
    const forTown = !!(r.town || r.siege || r.gates || r.relief || r.sally);
    const place = forTown && reg ? townName(reg) : r.regionName;
    const us = att ? r.attackers : r.defenders;
    // (The defender's loss is his dead and wounded and the men taken prisoner.)
    const oursLost = att ? r.attLost : r.defLost + r.prisoners, theirsLost = att ? r.defLost + r.prisoners : r.attLost;
    const corps = us.filter((u) => !u.guard).length;
    // A siege laid is the field won and the town still to take: good news for the besieger, a warning to the besieged.
    const good = r.siege ? att : att ? r.taken : !r.taken;
    // A siege laid on his town that holds is neither a battle won nor a battle lost (ruling 104): the score's small
    // line says a town is besieged, while the siege stands.
    // (A town that fell after the siege was laid, with no fight of its own, did not hold: its guard was broken from
    // the air or it opened its gates, and the province has changed hands as the card is made. That row is LOST, or
    // WON for the besieger, and the field battle is counted.)
    const fellSince = !!r.siege && !r.yielded && !!reg && (def ? !side(reg.ctrl) : side(reg.ctrl));
    const laidOnHim = !!r.siege && def && !r.yielded && !fellSince;
    const shut = laidOnHim ? shutIn(reg) : { men: 0, corps: 0 };
    if (laidOnHim && shut.men > 0) besiegedTowns.add(r.region);
    if (!r.yielded && !laidOnHim) { if (good) battlesWon++; else battlesLost++; }
    menOurs += oursLost; menTheirs += theirsLost;
    const lead = [...us].filter((u) => !u.guard).sort((a, b) => b.men - a.men)[0];
    const row = blank();
    row.arm = 'land';
    row.place = place;
    row.week = r.week;
    Object.assign(row, dated(r.turn));
    row.region = r.region;
    row.report = { type: 'land', id: r.id };
    row.ours = me;
    row.theirs = att ? r.defNat : r.attNat;
    row.sign = forTown || (r.taken && city) ? 'art' : lead && SIGNS.has(lead.kind) ? lead.kind : 'inf';
    row.picture = forTown || (r.taken && city) ? 'town' : def ? 'fieldworks' : 'troops';
    row.kind = forTown ? 'SIEGE' : 'BATTLE';
    row.figures = [{ n: num(oursLost), label: 'lost' }, { n: num(theirsLost), label: 'theirs' }];
    row.facts = [corps ? { n: plural(corps, 'corps', 'corps'), label: att ? 'went in' : good ? 'held the line' : 'fought' } : { n: 'The guard', label: 'stood alone' }, { n: num(oursLost), label: 'men lost' }, { n: num(theirsLost), label: 'of theirs' }];
    const prize = reg ? prizeOf(g, reg.id, me) : null;
    const worth = (gone: boolean): string => {
      if (!prize) return '';
      if (prize.capitalOf && prize.capitalOf !== me) return gone ? '' : `The seat of ${theName(g.nations[prize.capitalOf])}'s government is in your hands.`;
      if (prize.factories > 0) return gone ? `Its ${plural(prize.factories, 'factory', 'factories')} ${prize.factories === 1 ? 'is' : 'are'} lost to you.` : `Its ${plural(prize.factories, 'factory', 'factories')} now ${prize.factories === 1 ? 'works' : 'work'} for you.`;
      // (A field on colony ground gives nothing at home, to owner or conqueror: resources.ts fieldOverseas.)
      if (prize.oil > 0 && !reg!.overseas) return gone ? 'Its oil is lost to you.' : 'Its oil now flows to your depots.';
      if (prize.harbour) return gone ? 'Its harbour is closed to your ships.' : 'Its harbour is open to your ships.';
      return '';
    };
    if (r.yielded) {
      row.tag = att ? 'WON' : 'LOST'; row.tone = att ? 'good' : 'bad';
      row.line = att ? 'Taken without a fight.' : 'Given up under orders.';
      row.head = att ? `${place} is taken without a fight` : `${place} is given up`;
      row.follows = worth(!att) || (att ? 'The enemy fell back before the attack.' : 'Your troops fell back before the attack.');
    } else if (r.siege && fellSince) {
      row.tag = att ? 'WON' : 'LOST'; row.tone = att ? 'good' : 'bad';
      row.line = att ? 'The field was won, and the town fell after it.' : 'The field was lost, and the town fell after it.';
      row.head = att ? `${place} is taken` : `${place} is lost`;
      // (Beneath the lead's picture the truth of it comes first, then what the town was worth: the art director's review of 82.1.)
      row.follows = `${row.line} ${worth(!att) || (att ? 'The front moves forward.' : 'The front falls back.')}`;
      if (!att && corps) row.facts[0] = { n: plural(corps, 'corps', 'corps'), label: 'fought outside' };
    } else if (r.siege) {
      // (His town besieged and holding: BESIEGED, amber, "Pittsburgh BESIEGED by Canada. Canadian troops close round the town.")
      row.tag = att ? 'WON' : 'BESIEGED'; row.tone = att ? 'good' : 'warn';
      row.line = att ? 'The field is won. The town is besieged.' : shut.men > 0 ? `${g.nations[r.attNat]?.adj ?? 'Enemy'} troops close round the town.` : 'The field is lost. The town holds out.';
      row.head = att ? `${place} is under siege` : `${place} is besieged`;
      row.follows = att ? 'The province falls only when the town is taken.' : shut.men > 0 ? shutWords(place, oursLost, shut) : 'The province is yours for as long as the town holds.';
      // The words are true to what they count: the corps that lost the field outside did not hold the line, and the
      // men shut in are not lost. Three boxes: the men shut in, the men lost (shown even at none), and theirs.
      if (!att) {
        row.facts = [shut.men > 0 ? { n: num(shut.men), label: 'men shut in' } : { n: plural(corps, 'corps', 'corps'), label: 'fought outside' }, { n: num(oursLost), label: 'men lost' }, { n: num(theirsLost), label: 'of theirs' }];
        if (shut.men > 0) row.figures = [{ n: num(shut.men), label: 'shut in' }];
      }
    } else if (r.relief || r.sally) {
      row.tag = good ? 'WON' : 'LOST'; row.tone = good ? 'good' : 'bad';
      row.line = r.relief ? (r.taken ? (att ? 'The siege is raised.' : 'Their relief broke your siege.') : (att ? 'The relief was thrown back.' : 'Their relief was thrown back.')) : (r.taken ? (att ? 'The garrison broke the ring.' : 'The garrison broke your ring.') : (att ? 'The sally was thrown back.' : 'Their sally was thrown back.'));
      row.head = r.taken ? `The siege of ${place} is raised` : `The siege of ${place} holds`;
      row.follows = r.taken ? (att ? 'The town is free of its besiegers.' : 'The ring round the town is broken.') : (att ? 'The town is still cut off.' : 'The ring round the town stands.');
    } else if (att) {
      row.tag = r.taken ? 'WON' : 'LOST'; row.tone = r.taken ? 'good' : 'bad';
      row.line = r.taken
        ? (r.gates ? 'The town opened its gates.' : forTown ? `${count(corps)} corps stormed the town.` : r.prisoners >= 1000 ? `${count(corps)} corps took it, with ${num(r.prisoners)} prisoners.` : `${count(corps)} corps took the province.`)
        : (forTown ? 'The assault on the town was thrown back.' : 'The attack was thrown back.');
      row.head = r.taken ? `${place} is taken` : `The attack on ${place} is thrown back`;
      row.follows = r.taken ? worth(false) || (r.prisoners > 0 ? `${num(r.prisoners)} prisoners were taken.` : 'The front moves forward.') : `The attack gained no ground${r.works ? `, and their works still stand` : ''}.`;
    } else {
      row.tag = r.taken ? 'LOST' : 'HELD'; row.tone = r.taken ? 'bad' : 'good';
      row.line = r.taken
        ? (r.gates ? 'The town opened its gates to them.' : forTown ? 'The town fell to assault.' : r.prisoners >= 1000 ? `The line broke: ${num(r.prisoners)} men taken prisoner.` : 'The line broke and the province is lost.')
        : (forTown ? 'An assault on the town was thrown back.' : `${an(g.nations[r.attNat]?.adj ?? 'enemy')} attack was thrown back.`);
      row.head = r.taken ? `${place} is lost` : `${place} holds`;
      row.follows = r.taken ? worth(true) || 'The front falls back.' : 'The line holds where it stood.';
    }
    // What it weighs: his side's men, and the place by its rank (a capital is a capital in the month it falls).
    // (A siege laid on his town weighs its men shut in too, once, in the month it is laid: ruling 104(c).)
    let ourMen = oursLost + shut.men;
    // One story, one row: a siege laid and the town's own fight in the same month are told as the town's fight,
    // with the losses of both.
    const before = forTown && !r.siege ? ranked.findIndex((x) => x.laid?.region === r.region) : -1;
    if (before >= 0) {
      const laid = ranked.splice(before, 1)[0].laid!;
      const o = oursLost + laid.ours, t = theirsLost + laid.theirs;
      // (Besieged and still holding after an assault: the men shut in weigh here, once, and take the first box.)
      const still = def && !r.taken ? shutIn(reg) : { men: 0, corps: 0 };
      ourMen += laid.ours + still.men;
      row.figures = [{ n: num(o), label: 'lost' }, { n: num(t), label: 'theirs' }];
      row.facts = [still.men > 0 ? { n: num(still.men), label: 'men shut in' } : corps ? row.facts[0] : laid.first, { n: num(o), label: 'men lost' }, { n: num(t), label: 'of theirs' }];
      if (still.men > 0) row.follows = shutWords(place, o, still);
    }
    // His own capital besieged and holding: the sentence under the picture says what hangs on it (ruling 133(a), point 4).
    if (reg && reg.id === N?.capital && mine(reg.ctrl) && besieged(g, reg) && row.follows && !row.follows.includes(CAPITAL_BESIEGED)) row.follows = `${row.follows} ${CAPITAL_BESIEGED}`;
    // The word after the tag: by for a siege on him, from and to where no battle was fought, against for a battle.
    if (row.theirs && g.nations[row.theirs]) row.join = { word: row.tag === 'BESIEGED' ? 'by' : r.yielded ? (att ? 'from' : 'to') : 'against', name: theName(g.nations[row.theirs]) };
    ranked.push({ row, sub: GROUND_FIGHTING, weight: ourMen, place: reg?.cap ? 'capital' : size, ...(r.siege ? { laid: { region: r.region, ours: oursLost, theirs: theirsLost, first: row.facts[0] } } : {}) });
  }

  // Provinces that changed hands without a battle of his: one count a front.
  const wonFrom: Record<string, number[]> = {}, lostTo: Record<string, number[]> = {};
  let provWon = 0, provLost = 0;
  g.regions.forEach((r, i) => {
    const was = mark.before[i];
    if (was === undefined || was === r.ctrl) return;
    if (r.ctrl === me) { provWon++; if (!fought.has(i) && !allied(g, was, me)) (wonFrom[was] ??= []).push(i); }
    else if (was === me) { provLost++; if (!fought.has(i) && !allied(g, r.ctrl, me)) (lostTo[r.ctrl] ??= []).push(i); }
  });
  for (const [gain, by] of [[true, wonFrom], [false, lostTo]] as const) {
    for (const [nat, ids] of Object.entries(by)) {
      const other = g.nations[nat];
      const best = [...ids].sort((a, b) => g.regions[b].vp - g.regions[a].vp || a - b)[0];
      const row = blank();
      row.arm = 'land';
      row.sign = 'inf';
      row.picture = 'troops';
      row.place = ids.length === 1 ? g.regions[ids[0]].name : `The ${other?.adj ?? 'open'} front`;
      row.tag = gain ? 'WON' : 'LOST'; row.tone = gain ? 'good' : 'bad';
      row.ours = me; row.theirs = other ? nat : null;
      // ("Lost without a battle." after LOST says lost twice: both ways the line is "No battle was fought.", and the count is the figure.)
      row.line = 'No battle was fought.';
      if (other) row.join = { word: gain ? 'from' : 'to', name: theName(other) };
      row.figures = [{ n: String(ids.length), label: ids.length === 1 ? 'province' : 'provinces' }];
      row.region = best;
      row.kind = 'GROUND';
      row.head = gain ? `${plural(ids.length, 'province')} occupied${other ? ` from ${theName(other)}` : ''}` : `${plural(ids.length, 'province')} lost${other ? ` to ${theName(other)}` : ''}`;
      row.follows = ids.length > 1 ? `The largest is ${g.regions[best].name}.` : '';
      row.facts = row.figures;
      ranked.push({ row, sub: GROUND_PROVINCES, weight: ids.length * PROVINCE_MEN });
    }
  }

  // ---- 4. Sea and air ---------------------------------------------------------------------------------------
  let sunk = 0, shipsLost = 0, shotDown = 0, planesLost = 0;
  const sumShips = (sides: SeaSide[], key: 'ships' | 'sunk'): Ships => { const t: Ships = { bb: 0, cv: 0, ca: 0, dd: 0, ss: 0 }; for (const s of sides) for (const c of CLASSES) t[c] += s[key][c]; return t; };
  const classWord = (s: Ships): string => { const kinds = CLASSES.filter((c) => s[c] > 0); const n = shipCount(s); return kinds.length === 1 ? (n === 1 ? NAVY.classes[kinds[0]].one : NAVY.classes[kinds[0]].many) : n === 1 ? 'ship' : 'ships'; };
  const seaPairs: [string, string][] = [];
  for (const r of seas) {
    const att = r.att.some((u) => mine(u.nat)), def = !att && r.def.some((u) => mine(u.nat));
    seaPairs.push([r.attNat, r.defNat]);
    if (!att && !def) continue;
    const us = att ? r.att : r.def, them = att ? r.def : r.att;
    const good = att ? r.won : !r.won;
    const theirSunk = sumShips(them, 'sunk'), ourSunk = sumShips(us, 'sunk');
    sunk += shipCount(theirSunk); shipsLost += shipCount(ourSunk);
    const carriers = [...us, ...them].some((s) => s.ships.cv > 0);
    const ourFleet = us.find((u) => mine(u.nat))?.fleet ?? us[0].fleet, theirFleet = them[0]?.fleet ?? 'enemy fleet';
    const row = blank();
    row.cls = 4; row.arm = 'sea';
    row.sign = carriers ? 'cv' : 'hull';
    row.picture = 'fleet';
    row.place = r.zoneName;
    if (g.seas[r.zone]) row.sea = r.zone;
    row.report = { type: 'sea', id: r.id };
    row.ours = me; row.theirs = them[0]?.nat ?? null;
    row.tag = good ? 'WON' : 'LOST'; row.tone = good ? 'good' : 'bad';
    if (row.theirs && g.nations[row.theirs]) row.join = { word: 'against', name: theName(g.nations[row.theirs]) };
    row.line = good ? `${cap(the(ourFleet))} beat ${the(theirFleet)}.` : `${cap(the(theirFleet))} beat ${the(ourFleet)}.`;
    row.figures = good && shipCount(theirSunk) ? [{ n: String(shipCount(theirSunk)), label: `${classWord(theirSunk)} sunk` }] : shipCount(ourSunk) ? [{ n: String(shipCount(ourSunk)), label: `${classWord(ourSunk)} lost` }] : shipCount(theirSunk) ? [{ n: String(shipCount(theirSunk)), label: `${classWord(theirSunk)} sunk` }] : [];
    row.kind = 'FLEET ACTION';
    row.head = `${good ? 'Victory' : 'Defeat'} ${seaWhere(r.zoneName.replace(/^the /i, ''))}`;
    row.follows = shipCount(theirSunk) || shipCount(ourSunk) ? `${cap(shipCount(theirSunk) ? shipList(theirSunk) : 'no enemy ship')} sunk${shipCount(ourSunk) ? ` for the loss of ${shipList(ourSunk)}` : ', and no ship of yours lost'}.` : 'Neither side lost a ship.';
    row.facts = [{ n: String(shipCount(sumShips(us, 'ships'))), label: 'ships engaged' }, { n: String(shipCount(ourSunk)), label: 'lost' }, { n: String(shipCount(theirSunk)), label: 'of theirs sunk' }];
    // (A fleet action is never a light thing: it starts at a corps, and the ships sunk add their cost.)
    ranked.push({ row, sub: 0, weight: CORPS_MEN + shipsWeight(theirSunk) + shipsWeight(ourSunk), fleet: true, great: true });
  }
  const airAt = new Set<number>();
  const airAtSea: string[] = [];
  for (const r of airs) {
    if (r.region >= 0) airAt.add(r.region); else airAtSea.push(r.attNat);
    if (r.mission === 'support' || r.mission === 'rest') continue;
    const att = mine(r.attNat) || r.att.some((u) => mine(u.nat));
    const def = !att && (mine(r.defNat) || r.def.some((u) => mine(u.nat)) || (r.region >= 0 && g.regions[r.region]?.ctrl === me));
    if (!att && !def) continue;
    const lostOf = (xs: AirReport['att']): number => xs.reduce((t, u) => t + u.lostFtr + u.lostBmr, 0);
    const attLost = lostOf(r.att), defLost = lostOf(r.def);
    const ourLost = att ? attLost : defLost, theirLost = att ? defLost : attLost;
    shotDown += theirLost; planesLost += ourLost;
    const bombers = r.att.reduce((t, u) => t + u.bmr, 0), flown = r.att.reduce((t, u) => t + u.bmr + u.ftr, 0);
    const shipsHit = r.ships ? sumShips(r.ships, 'sunk') : null;
    if (shipsHit && shipCount(shipsHit)) { if (att) sunk += shipCount(shipsHit); else shipsLost += shipCount(shipsHit); }
    const row = blank();
    row.cls = 4; row.arm = 'air';
    row.report = { type: 'air', id: r.id };
    if (r.region >= 0) row.region = r.region; else if (g.seas[r.zone]) row.sea = r.zone;
    row.ours = me; row.theirs = att ? r.defNat || null : r.attNat;
    const sky = r.mission === 'superiority';
    const good = att ? r.won : !r.won;
    row.tone = good ? 'good' : 'bad';
    row.kind = sky ? 'THE SKY' : 'AIR RAID';
    const town = r.region >= 0 && g.regions[r.region] ? townName(g.regions[r.region]) : r.place;
    const what = bombers ? `${num(bombers)} bombers` : `${num(flown)} aircraft`;
    if (sky) {
      row.sign = 'ftr'; row.picture = 'airfield';
      row.place = `The sky over ${r.place}`;
      row.tag = good ? 'WON' : 'LOST';
      row.line = good ? 'Your fighters hold the sky there.' : 'Their fighters hold the sky there.';
      row.head = good ? `The sky over ${r.place} is yours` : `The sky over ${r.place} is lost`;
    } else {
      row.sign = 'bmr';
      row.tag = att ? (r.won ? 'HIT' : 'MISSED') : r.won ? 'HIT' : 'HELD';
      const [pic, place, hit, miss]: [CardPicture, string, string, string] =
        r.mission === 'line' ? ['rail-yard', `${town} rail yard`, `${what} cut the railway there.`, 'The bombs fell wide. The line still runs.']
        : r.mission === 'harbour' ? ['harbour', /\bHarbou?r$/.test(r.place) ? r.place : `${r.place} harbour`, `${what} struck the ships in harbour.`, 'The raid on the harbour was beaten off.']
        : r.mission === 'fleet' ? ['fleet', cap(the(r.ships?.[0]?.fleet ?? r.place)), `${what} struck the fleet at sea.`, 'The strike on the fleet was beaten off.']
        : r.mission === 'strike' ? ['fleet', cap(r.place.replace(/^the /, 'The ')), `${what} struck the shipping there.`, 'The strike on the shipping was beaten off.']
        : r.mission === 'ground' ? ['troops', r.place, `${what} struck the troops there.`, 'The raid on the troops was beaten off.']
        : r.mission === 'town' ? ['town', town, `${what} broke down its defences.`, 'The raid on its defences was beaten off.']
        : ['factories', r.place, `${what} hit its factories${r.mission === 'night' ? ' by night' : ''}.`, 'The bombers were beaten off.'];
      row.picture = pic; row.place = place;
      row.line = r.won ? (att ? hit : hit.replace(/^([\d,]+) (bombers|aircraft)/, 'Their $2')) : miss;
      const low = place.replace(/^The /, 'the ');
      // Shipping is where the sea is, by the sea words: "Shipping off the Azores", "the strike on shipping in the North Sea".
      const shipAt = g.seas[r.zone] ? seaWhere(g.seas[r.zone].name) : `in ${low}`;
      row.head = r.mission === 'fleet' || r.mission === 'strike'
        ? (r.won ? `${r.mission === 'fleet' ? place : `Shipping ${shipAt}`} is struck from the air` : att ? `The strike on ${r.mission === 'fleet' ? low : `shipping ${shipAt}`} fails` : `The strike on ${r.mission === 'fleet' ? low : `shipping ${shipAt}`} is beaten off`)
        : r.won ? `${place} is bombed` : att ? `The raid on ${place} fails` : `${place} beats off the raid`;
    }
    row.follows = r.result;
    const sunkHere = shipsHit ? shipCount(shipsHit) : 0;
    row.figures = sunkHere ? [{ n: String(sunkHere), label: `${classWord(shipsHit!)} ${att ? 'sunk' : 'lost'}` }]
      : att ? (ourLost ? [{ n: num(ourLost), label: r.att.every((u) => !u.lostFtr) ? 'bombers lost' : 'aircraft lost' }] : theirLost ? [{ n: num(theirLost), label: 'shot down' }] : [])
      : theirLost ? [{ n: num(theirLost), label: 'shot down' }] : ourLost ? [{ n: num(ourLost), label: 'fighters lost' }] : [];
    row.facts = [{ n: num(flown), label: att ? 'aircraft flew' : 'aircraft came' }, { n: num(ourLost), label: 'lost' }, { n: num(theirLost), label: 'of theirs shot down' }];
    ranked.push({ row, sub: 0, weight: (flown / 20 + ourLost + theirLost) * PLANE_MEN + (shipsHit ? shipsWeight(shipsHit) : 0), great: !!shipsHit && greatShips(shipsHit) });
  }
  const shelled = new Set<number>();
  for (const r of shores) {
    shelled.add(r.region);
    const att = mine(r.nat), def = !att && (mine(r.defNat) || g.regions[r.region]?.ctrl === me);
    if (!att && !def) continue;
    const lost = shipCount(r.sunk);
    if (att) shipsLost += lost; else sunk += lost;
    const row = blank();
    row.cls = 4; row.arm = 'sea';
    row.sign = 'hull'; row.picture = 'harbour';
    row.place = r.place;
    row.region = r.region;
    row.report = { type: 'shore', id: r.id };
    row.ours = me; row.theirs = att ? r.defNat || null : r.nat;
    row.tag = 'HIT'; row.tone = att ? 'good' : 'bad';
    const target = r.what === 'troops' ? 'the troops ashore' : r.what === 'city' ? 'the city' : 'the defences of the town';
    row.line = `${cap(the(r.fleet))} shelled ${target}.`;
    row.figures = lost ? [{ n: String(lost), label: `${classWord(r.sunk)} ${att ? 'lost' : 'sunk'}` }]
      : r.what === 'troops' ? [{ n: String(r.divs), label: r.divs === 1 ? 'division hit' : 'divisions hit' }]
      : r.what === 'city' ? [{ n: `${Math.round(r.output * 100)}%`, label: 'output stopped' }]
      : [{ n: `${Math.round(r.works ?? 0)}%`, label: 'of its works' }];
    row.kind = 'SHELLED FROM THE SEA';
    row.head = `${r.place} is shelled from the sea`;
    row.follows = r.result;
    row.facts = [{ n: String(shipCount(r.ships)), label: 'ships stood in' }, ...row.figures];
    ranked.push({ row, sub: 0, weight: SHELLED_MEN + shipsWeight(r.sunk), great: greatShips(r.sunk) });
  }

  // News of fighting that no battle report tells: bridges blown, a landing, a ship torpedoed, an army cut off.
  // (His ring that closes on the enemy is his news too: an enemy army cut off on ground that touches his own.)
  const hisRing = (h: Headline): boolean => h.region !== undefined && foe(h.lost) && /^The ring closes at /.test(h.text) && (g.regions[h.region]?.nb ?? []).some((n) => g.regions[n]?.ctrl === me);
  const involves = (h: Headline): boolean => mine(h.nat) || mine(h.won) || mine(h.lost) || (h.region !== undefined && (g.regions[h.region]?.ctrl === me || mark.before[h.region] === me)) || hisRing(h);
  // A siege of his side's town raised with no battle (its besiegers gone, or guns left alone falling back): one row,
  // HELD against the besieger, whose line is the head alone (ruling 211(f); the UI director's words of 17 November).
  const RAISED = /^The siege of .+? is raised\b|\. The siege is raised\.$/;
  const raisedNews = (h: Headline): boolean => h.arm === 'land' && h.region !== undefined && side(g.regions[h.region]?.ctrl) && RAISED.test(h.text);
  const raisedBy = new Map<number, string>();
  for (const h of hs) if (raisedNews(h) && foe(h.lost) && !raisedBy.has(h.region!)) raisedBy.set(h.region!, h.lost!);
  const raisedAt = new Set<number>();
  for (const h of hs) {
    if (!h.arm || h.warn || (h.kind !== 'battle' && h.kind !== 'event')) continue;
    if (!involves(h) || freedBand(h)) continue;
    const raised = raisedNews(h);
    if (raised && raisedAt.has(h.region!)) { used.add(h); continue; }
    // One story, one row: the battle report tells it better.
    const told = h.arm === 'land' ? h.region !== undefined && fought.has(h.region)
      : h.arm === 'air' ? (h.region !== undefined ? airAt.has(h.region) || shelled.has(h.region) : airAtSea.includes(h.nat ?? '') || airs.some((r) => r.zone >= 0))
      // (At sea only the fleet action's own line: a submarine sunk by escorts the same month is another story.)
      : (h.region !== undefined && shelled.has(h.region)) || (h.won !== undefined && h.lost !== undefined && /\bdefeats?\b/.test(h.text) && seaPairs.some(([a, b]) => (a === h.won && b === h.lost) || (a === h.lost && b === h.won)));
    if (told) { used.add(h); continue; }
    const good = side(h.won) || (h.won === undefined && foe(h.lost)), bad = !good && (side(h.lost) || (h.lost === undefined && foe(h.won)));
    const bridges = /\bbridges?\b/.test(h.text);
    const row = blank();
    row.cls = h.arm === 'land' ? 3 : 4; row.arm = h.arm;
    row.week = h.week;
    Object.assign(row, dated(h.turn));
    if (h.region !== undefined) row.region = h.region;
    // (News from the sea names its sea, as the sea words say it: "in the North Sea", "off the Azores", "in Hudson Bay".
    // The row's place takes the sea's map name, and the line is the shorter for it. Anything else "in the X" as before.)
    const seaAt = h.region === undefined ? seaSaid(h.text, g.seas.map((z) => z.name)) : null;
    const where = h.region === undefined && !seaAt ? / in (the [A-Z][A-Za-z' -]*?)(?= and |[.,:]|$)/.exec(h.text) : null;
    row.place = h.region !== undefined ? g.regions[h.region].name : seaAt ? g.seas[seaAt.i].name : where ? cap(where[1].replace(/^the /, '')) : h.arm === 'sea' ? 'At sea' : 'In the air';
    // (A sea has a place on the chart: the row's pin goes there.)
    const zone = seaAt ? seaAt.i : where ? g.seas.findIndex((z) => z.name.replace(/^the /i, '') === where[1].replace(/^the /i, '')) : -1;
    if (zone >= 0) row.sea = zone;
    row.sign = h.arm === 'land' ? (bridges ? 'eng' : /airborne/.test(h.text) ? 'para' : 'inf') : h.arm === 'sea' ? (/submarine|torpedo|convoy|shipping/i.test(h.text) ? 'ss' : 'hull') : 'bmr';
    row.picture = h.arm === 'land' ? (bridges || /\b(town|city|garrison|siege)\b/.test(h.text) ? 'town' : 'troops') : h.arm === 'sea' ? 'fleet' : h.region !== undefined ? 'factories' : 'fleet';
    const sinking = h.arm !== 'land' && /\b(sinks?|sunk|torpedoes|scuttled)\b/.test(h.text);
    // Bridges blown in his path block him; blown by his own men they are his doing.
    const blocked = bridges && /\bblow\b/.test(h.text) && !mine(h.nat);
    row.tag = blocked ? 'BLOCKED' : sinking && (good || bad) ? 'SUNK' : good ? 'WON' : bad ? 'LOST' : '';
    row.tone = blocked ? 'warn' : good ? 'good' : bad ? 'bad' : 'plain';
    const other = [h.won, h.lost, h.nat].find((x) => x !== undefined && x !== me && !!g.nations[x]);
    row.ours = me; row.theirs = other ?? null;
    const said = first(seaAt ? h.text.replace(seaAt.said, '') : where ? h.text.replace(where[0], '') : h.text);
    const river = bridges ? /bridges over (the [A-Z][A-Za-z' -]*?)(?= at | between | as | before |[.,]|$)/.exec(h.text) : null;
    const ships = sinking ? CLASS_WORD.exec(h.text) : null;
    // A heavy ship torpedoed out of its fleet: one ship, named by its class.
    const torpedoed = sinking && !ships ? /\btorpedoes and sinks an? (battleship|carrier|cruiser|destroyer|submarine) of (the [^:.]+?) (?:in|off) [^:.]+:/.exec(h.text) : null;
    // A row says it in one short sentence; the whole line of news is in the newsreel.
    row.line = river ? (/\bseize\b/.test(h.text) ? `The bridges over ${river[1]} were taken intact.` : /\bblow\b/.test(h.text) ? `The bridges over ${river[1]} are blown.` : `${said}.`) : `${said}.`;
    row.kind = h.arm === 'land' ? 'ON LAND' : h.arm === 'sea' ? 'AT SEA' : 'IN THE AIR';
    row.head = ships ? `${cap(count(Number(ships[1])).toLowerCase())} ${ships[2]} ${ships[3]} sunk` : brief(said, 64);
    if (ships) row.figures = [{ n: ships[1], label: `${ships[3]} ${good ? 'sunk' : 'lost'}` }];
    if (torpedoed) { row.line = `Torpedoed: a ${torpedoed[1]} of ${torpedoed[2]}.`; row.head = `A ${torpedoed[1]} is torpedoed and sunk`; row.figures = [{ n: '1', label: `${torpedoed[1]} ${good ? 'sunk' : 'lost'}` }]; }
    // An army cut off: the row carries the men as its figure.
    const ring = /^The ring closes at [^:]+: (.+?), ([\d,]+) men, (?:is|are) cut off/.exec(h.text);
    if (ring) {
      const force = /^(\S+) .*?\b(corps|army|armies|divisions?)$/i.exec(ring[1]);
      const who = force ? `${force[1]} ${force[2]}` : ring[1];
      row.line = `The ring closes: ${who} ${/^(1|one|a|an) /i.test(who) ? 'is' : 'are'} cut off.`;
      row.figures = [{ n: ring[2], label: 'men cut off' }];
      // (As the lead story it is told whole: where, who, and how many.)
      row.head = `The ring closes at ${row.place}`;
      row.follows = `${cap(ring[1])} ${/^(1|one|a|an) /i.test(ring[1]) ? 'is' : 'are'} cut off from all supply.`;
      row.facts = row.figures;
    }
    if (raised && !fought.has(h.region!)) {
      raisedAt.add(h.region!);
      const town = townName(g.regions[h.region!]), by = raisedBy.get(h.region!);
      Object.assign(row, { tag: 'HELD', tone: 'good', picture: 'town', line: `The siege of ${town} is raised.`, head: `The siege of ${town} is raised`, theirs: by ?? null });
      if (by) row.join = { word: 'against', name: theName(g.nations[by]) };
    }
    // A siege laid on a town of his side's with no battle in the field (the Playtester's 84.2.6 find): BESIEGED by
    // the besieger, amber, as the battle's own row says it ("Cleveland BESIEGED by Canada. Canadian troops close round
    // the town."), never a nameless LOST while the town holds.
    const laid = h.arm === 'land' && h.region !== undefined && / troops close round [^:]+: .+ is under siege\b/.test(h.text);
    if (laid && bad && h.won && g.nations[h.won]) {
      Object.assign(row, { tag: 'BESIEGED', tone: 'warn', picture: 'town', line: `${g.nations[h.won].adj} troops close round the town.`, head: `${row.place} is besieged`, theirs: h.won });
    }
    // (A row on land that names the other side says it by name, with the tag's own word.)
    if (h.arm === 'land' && !row.join && row.theirs && g.nations[row.theirs] && (row.tag === 'WON' || row.tag === 'LOST' || row.tag === 'BESIEGED')) {
      row.join = { word: row.tag === 'BESIEGED' ? 'by' : 'against', name: theName(g.nations[row.theirs]) };
    }
    const escorts = /^The escorts of (?:the )?(.+?) sinks? /.exec(h.text);
    if (escorts && ships) row.line = `Sunk by escorts of the ${escorts[1]}.`;
    const arms = /^Cut off and out of supply, (.+?) lays? down (?:its|their) arms(?: at ([^:.]+))?(?:: (?:.+?, )?([\d,]+) men)?\./.exec(h.text);
    if (arms) { row.line = `${cap(arms[1])} lays down its arms${arms[2] ? ` at ${arms[2]}` : ''}.`; if (arms[3]) row.figures = [{ n: arms[3], label: side(h.lost) || mine(h.nat) ? 'men lost' : 'prisoners' }]; }
    used.add(h);
    // What it weighs: the men of a ring or of an army that lays down its arms; the ships a line of news names.
    const menNamed = Number((ring?.[2] ?? arms?.[3] ?? '0').replace(/,/g, ''));
    const sunkWord = ships?.[3] ?? torpedoed?.[1] ?? '', sunkCount = ships ? Number(ships[1]) : torpedoed ? 1 : 0;
    // (The score at sea counts what the rows say: a ship a line of news names is in it too.)
    if (good) sunk += sunkCount; else if (bad) shipsLost += sunkCount;
    const weight = h.arm === 'land' ? menNamed || (bad ? 0 : NEWS_MEN) : sunkCount ? sunkCount * shipMen(wordClass(sunkWord)) : NEWS_MEN;
    ranked.push({ row, sub: h.arm === 'land' ? (menNamed || bad ? GROUND_FIGHTING : GROUND_NEWS) : 0, weight, alone: sunkCount > 0, great: /^(battleship|carrier)/.test(sunkWord), ...(ring && bad && menNamed >= CORPS_MEN ? { ring: true } : {}) });
  }

  // ---- 5. Goals and honours ---------------------------------------------------------------------------------
  for (const h of hs) {
    if (h.kind !== 'goal' && !(h.kind === 'honour' && mine(h.nat))) continue;
    const row = blank();
    row.cls = 5;
    row.sign = 'star';
    row.week = h.week;
    Object.assign(row, dated(h.turn));
    row.place = h.kind === 'goal' ? 'Campaign goal' : 'Honour';
    row.tone = 'good';
    row.ours = me;
    row.line = `${first(h.text)}.`;
    row.kind = h.kind === 'goal' ? 'CAMPAIGN GOAL' : 'HONOUR';
    row.head = first(h.text);
    if (h.region !== undefined) row.region = h.region;
    used.add(h);
    ranked.push({ row, sub: h.kind === 'goal' ? 0 : 1, weight: 0 });
  }

  // ---- 1 and 2. The red bands -------------------------------------------------------------------------------
  const majors = new Set(majorsOf(g));
  const banded: { band: CardBand; rank: number; h: Headline; onHim?: boolean; good?: boolean }[] = [];
  /** Whether a line names a country, by its name or by another the news gives it; and the countries a line names, in its order. */
  const nameAt = (id: string, text: string): number => {
    const n = g.nations[id];
    if (!n) return -1;
    const a = text.search(new RegExp(`\\b${n.name.replace(/[.*+?^${}()|[\]\\]/g, '\\$&')}\\b`)), b = ALSO_CALLED[id] ? text.search(ALSO_CALLED[id]) : -1;
    return a < 0 ? b : b < 0 ? a : Math.min(a, b);
  };
  const namedIn = (text: string): string[] => Object.keys(g.nations).map((id) => ({ id, at: nameAt(id, text) })).filter((x) => x.at >= 0).sort((a, b) => a.at - b.at).map((x) => x.id);
  // A great power entering the war: at peace when the month opened and at war now. The first line of news that
  // names it is its band. (One already at war that declares another is war and peace like anyone's: his band only
  // where it touches his own war.)
  const entered = new Set([...majors].filter((m) => !!g.nations[m]?.alive && g.nations[m].atWar.length > 0 && !mark.warring.includes(m)));
  hs.forEach((h) => {
    const warn = !!h.warn && mine(h.nat);
    const enters = h.kind === 'war' ? [...entered].filter((m) => m === h.nat || nameAt(m, h.text) >= 0) : [];
    for (const m of enters) entered.delete(m);
    const great = enters.length > 0 || (h.kind === 'fall' && !!h.nat && majors.has(h.nat));
    // (A fall is FALLEN whatever else the month did to the country, under the flag of whoever took it; FREED is the
    // liberation's own line.)
    const lib = warn ? undefined : freedBand(h);
    const p = h.kind === 'fall' ? fellOf(h) : lib;
    const touches = !!lib || ((h.kind === 'war' || h.kind === 'fall') && (mine(h.nat) || side(h.nat) || foe(h.nat) || (!!p && (side(p.by) || foe(p.by)))));
    // (News with a title of its own that is no war and no fall is a band where it names his country or is news of
    // a country in his war: the brass band, below the red ones.)
    const news = !warn && !lib && !!h.head && h.kind !== 'war' && h.kind !== 'fall' && (mine(h.nat) || side(h.nat) || foe(h.nat) || nameAt(me, h.text) >= 0);
    if (!warn && !great && !touches && !news) return;
    const word: CardBand['word'] = warn ? 'WARNING' : news ? 'NEWS' : h.kind === 'war' ? 'WAR' : lib ? 'FREED' : 'FALLEN';
    // For him the news is the declaration on him: where a line of news says more than one thing, the sentence
    // that names war on his country is the headline, under the flags of those who declare it.
    const said = h.kind === 'war' && !warn ? (h.text.match(/[^.!?]+/g) ?? []).map((s) => s.trim()).find((s) => { const at = s.search(/\bwar on\b/); return at >= 0 && nameAt(me, s.slice(at)) >= 0; }) : undefined;
    const declarers = said ? namedIn(said.slice(0, said.search(/\bdeclares? war\b|\bwar on\b/))).filter((x) => x !== me) : [];
    // (The flags are those of the headline: who did it, and a great power the headline names as entering the war.)
    // (News with a title of its own is headed by it: "Britain stands by Canada", ruling 211(b).)
    const head = h.head ?? said ?? first(h.text);
    // (A warning flies the flag of the country its news is about, Italy's for "Rome wavers": the art director's rule.)
    const nats = warn ? [h.about && g.nations[h.about] ? h.about : me] : declarers.length ? declarers.slice(0, 2) : [...new Set([h.nat, ...enters.filter((m) => nameAt(m, head) >= 0), p?.by])].filter((x): x is string => !!x && !!g.nations[x] && x !== 'none').slice(0, 2);
    banded.push({ band: { word, title: warn ? h.warn : undefined, nats, head, text: h.text, week: h.week, when: '', ...dated(h.turn), region: h.region, ...(lib && (mine(h.nat) || side(lib.by)) ? { good: true } : {}) }, rank: great ? 0 : warn ? 1 : news ? 3 : 2, h, onHim: !!said, good: !!lib && (mine(h.nat) || side(lib.by)) });
    used.add(h);
  });
  // A seat's stake speaks for itself (rulings 121(c) and 123(a)). Its band is made from the stake as it stands when
  // he reads the card, never from a month's news: so the card that opens a campaign carries it before any month's
  // events have run, and a place lost since the news was written is on the band. Its small line names the next
  // moment the test is made ('as September opens' on the card read as August opens), not the date of the news.
  const stake = stakeWarning(g, me);
  if (stake) {
    for (const b of banded.filter((x) => x.band.word === 'WARNING' && (x.band.title === 'Rome wavers' || x.band.title === ROME_HOLDS))) banded.splice(banded.indexOf(b), 1);
    const h: Headline = { turn: g.turn, week: 1, text: stake.text, kind: 'event', nat: stake.nat, region: stake.region, warn: stake.title, about: stake.about };
    banded.push({ band: { word: 'WARNING', title: stake.title, nats: [g.nations[stake.about] ? stake.about : me], head: first(stake.text), text: stake.text, week: 1, when: '', month: stake.tested, region: stake.region }, rank: 1, h });
  }
  // The country in danger (rulings 117(d) and 133(a)): no campaign ends on him without a red band at least a month
  // before. His own country able to give in next month is the first band of the card, whatever else the month
  // brought; the enemy within reach of his capital is a band once, the month it becomes true (the mark keeps whether
  // it was so when his last month opened). Like a stake's band it is made from the position as he reads the card
  // (danger.ts), and its small line says what would stop it. In the last month of a siege it is the lead instead.
  const fall = fallWarning(g, me, !opts.opening && !!mark.reach);
  const due = stakeDue(g, me);
  if (fall && (fall.level !== 'last' || due)) {
    const h: Headline = { turn: g.turn, week: 1, text: fall.text, kind: 'event', nat: fall.nat, region: fall.region, warn: fall.title, about: fall.nat };
    banded.push({ band: { word: 'WARNING', title: fall.title, nats: [fall.nat], head: fall.head, text: fall.text, sub: fall.sub, week: 1, when: '', region: fall.region, note: fall.note, ...(fall.alarm?.help && fall.nat === me ? { alarm: true } : {}) }, rank: fall.level === 'reach' ? 1 : -1, h });
  }
  // One story, one band. A country's fall is told once, by the line that says the country fell (the capital's own
  // fall is the same news); and a country that declares war on several in a month does so in one band.
  for (const nat of new Set(banded.filter((b) => b.h.kind === 'fall' && b.h.nat).map((b) => b.h.nat!))) {
    const same = banded.filter((b) => b.h.kind === 'fall' && b.h.nat === nat);
    const keep = [...same].reverse().find((b) => b.h.region === undefined) ?? same[same.length - 1];
    for (const b of same) if (b !== keep) banded.splice(banded.indexOf(b), 1);
  }
  // A country freed and fallen in the same month is one story too: the later news stands.
  for (const nat of new Set(banded.filter((b) => b.band.word === 'FREED' && b.h.nat).map((b) => b.h.nat!))) {
    const both = banded.filter((b) => (b.band.word === 'FREED' || b.band.word === 'FALLEN') && b.h.nat === nat);
    const keep = both.reduce((a, b) => (hs.indexOf(b.h) > hs.indexOf(a.h) ? b : a));
    for (const b of both) if (b !== keep) banded.splice(banded.indexOf(b), 1);
  }
  const DECLARES = /^(.+?) declares war on (.+)$/;
  for (const b of [...banded]) {
    const m = b.h.kind === 'war' ? DECLARES.exec(b.band.head) : null;
    if (!m || !banded.includes(b)) continue;
    const more = banded.filter((x) => x !== b && x.h.kind === 'war' && x.h.nat === b.h.nat && DECLARES.exec(x.band.head)?.[1] === m[1]);
    // (Its other declarations of the month belong to the same band, though they would have had none of their own.)
    const echo = hs.filter((h) => !used.has(h) && h.kind === 'war' && h.nat === b.h.nat && DECLARES.exec(first(h.text))?.[1] === m[1]);
    if (!more.length && !echo.length) continue;
    for (const h of echo) used.add(h);
    // (His own country is named first, and the band takes the highest place any of them had.)
    const on = [...new Set([m[2], ...more.map((x) => DECLARES.exec(x.band.head)![2]), ...echo.map((h) => DECLARES.exec(first(h.text))![2])])];
    const own = on.findIndex((x) => x.includes(N.name));
    if (own > 0) on.unshift(...on.splice(own, 1));
    b.band.head = `${m[1]} declares war on ${listIn(on, BAND_ROOM - `${m[1]} declares war on `.length)}`;
    b.rank = Math.min(b.rank, ...more.map((x) => x.rank));
    b.band.text = [...[b, ...more].map((x) => x.band.text), ...echo.map((h) => h.text)].join(' ');
    b.band.nats = [...new Set([...b.band.nats, ...more.flatMap((x) => x.band.nats)])].slice(0, 2);
    for (const x of more) banded.splice(banded.indexOf(x), 1);
  }
  // Several countries that declare war on him in a month do so in one band, under their flags.
  const onHim = banded.filter((b) => b.h.kind === 'war' && !mine(b.h.nat) && DECLARES.exec(b.band.head)?.[2].replace(/^the /, '') === N.name);
  if (onHim.length > 1) {
    const [keep, ...rest] = onHim;
    const at = ` declare war on ${theName(N)}`;
    keep.band.head = `${listIn(onHim.map((b, i) => (i ? midName(DECLARES.exec(b.band.head)![1]) : DECLARES.exec(b.band.head)![1])), BAND_ROOM - at.length)}${at}`;
    keep.band.nats = onHim.map((b) => b.h.nat!).slice(0, 2);
    keep.band.text = onHim.map((b) => b.band.text).join(' ');
    keep.rank = Math.min(...onHim.map((b) => b.rank));
    for (const b of rest) banded.splice(banded.indexOf(b), 1);
  }
  for (const b of banded) {
    b.band.head = brief(b.band.head, BAND_ROOM);
    // A declaration or a fall has a place to show too: the capital of the first country it names that is not his
    // own; and of an invasion, the country invaded.
    if (b.band.region !== undefined || b.band.word === 'WARNING') continue;
    const struck = /\b(?:invades?|attacks?) /.exec(b.band.head);
    const who = [...(struck ? namedIn(b.band.head.slice(struck.index)) : []), ...namedIn(b.band.head), ...b.band.nats].find((x) => x !== me && !!g.nations[x]);
    const seat = who ? g.nations[who].capital : -1;
    if (g.regions[seat]) b.band.region = seat;
  }
  // The warning of the months after a stake's last month is headed by its own title, and says on its face what to do.
  for (const b of banded) {
    if (b.band.word !== 'WARNING' || b.band.title !== ROME_HOLDS) continue;
    const short = b.band.text.split(/(?<=\.) /).find((s) => s.startsWith(NO_LAST_MONTH));
    Object.assign(b.band, { head: ROME_HOLDS, spent: true }, short ? { note: short } : {});
  }
  // The month a stake falls due the warning is the lead story, and is not said twice (ruling 118(f)).
  if (due) for (const b of banded.filter((x) => x.band.word === 'WARNING' && x.band.title === due.title)) banded.splice(banded.indexOf(b), 1);
  // (Of the same rank: war declared on him, then his own doing, then the rest.)
  const nearness = (b: (typeof banded)[number]): number => (b.onHim || (!mine(b.h.nat) && nameAt(me, b.band.head) >= 0) ? 0 : mine(b.h.nat) ? 1 : 2);
  banded.sort((a, b) => a.rank - b.rank || nearness(a) - nearness(b));
  card.bands = banded.slice(0, 2).map((b) => b.band);
  // An item that finds no band keeps its class's colour (red for a warning, brass for the rest) and sits above the first
  // battle row.
  for (const b of banded.slice(2)) {
    const row = blank();
    row.cls = 2;
    row.week = b.band.week;
    row.place = b.band.word === 'WARNING' ? b.band.title ?? 'Warning' : g.nations[b.band.nats[0] ?? '']?.name ?? cap(b.band.word.toLowerCase());
    row.tag = b.band.word;
    row.tone = b.good ? 'good' : b.band.word === 'NEWS' ? 'plain' : 'bad';
    if (b.band.month) row.month = b.band.month;
    row.ours = b.band.nats[0] ?? null;
    row.theirs = b.band.nats[1] ?? null;
    row.line = `${b.band.head}.`;
    row.kind = b.band.word;
    row.head = b.band.head;
    if (b.band.region !== undefined) row.region = b.band.region;
    ranked.push({ row, sub: b.rank, weight: 0 });
  }

  // ---- The order, the lead and the fold ---------------------------------------------------------------------
  // The place adds by its rank, and a capital more than any ordinary battle of the month.
  const placed = ranked.filter((x) => x.place !== undefined);
  const ordinary = Math.max(0, ...placed.filter((x) => x.place !== 'capital').map((x) => x.weight + (PLACE_MEN[x.place!] ?? 0)));
  for (const x of placed) x.weight += x.place === 'capital' ? Math.max(PLACE_MEN.capital, ordinary + 1) : PLACE_MEN[x.place!] ?? 0;
  // (Rulings 84 and 95: what is stamped LOST ranks at twice its weight, the larger rank first, and at equal ranks the loss first.)
  const lost = (x: Ranked): boolean => x.row.tag === 'LOST' || (x.row.cls === 4 && x.row.tone === 'bad');
  const rank = (x: Ranked): number => x.weight * (lost(x) ? 2 : 1);
  // A fleet meeting a fleet is never ranked below a single ship lost alone.
  const alone = Math.max(0, ...ranked.filter((x) => x.alone).map(rank));
  const order = (x: Ranked): number => (x.fleet ? Math.max(rank(x), alone + 1) : rank(x));
  ranked.sort((a, b) => a.row.cls - b.row.cls || a.sub - b.sub || order(b) - order(a) || Number(lost(b)) - Number(lost(a)));
  const all = ranked.map((x) => x.row);
  const fighting = all.filter((r) => r.cls === 3 || r.cls === 4);
  // The lead story is the heaviest of ground, sea and air together; a light ground item never takes the picture
  // from a fleet action or a great ship sunk.
  const topG = ranked.find((x) => x.row.cls === 3 && x.row.picture !== null), topS = ranked.find((x) => x.row.cls === 4 && x.row.picture !== null);
  const light = (x: Ranked): boolean => x.sub !== GROUND_FIGHTING || x.weight < CORPS_MEN;
  card.lead = (topG && topS ? ((topS.great && light(topG)) || order(topS) > order(topG) ? topS : topG) : topG ?? topS)?.row ?? null;
  // The month a stake falls due it leads, whatever was fought (ruling 118(f)), and the battle that would have led is
  // the first of the rows.
  if (due) {
    card.lead = { ...blank(), cls: 2, stake: true, tag: 'LAST MONTH', tone: 'bad', ours: due.nat, place: g.nations[due.nat]?.name ?? '', kind: 'THE STAKE', month: due.month,
      head: due.head, line: due.sub, follows: due.sentence, facts: due.figures, ...(due.region !== undefined ? { region: due.region } : {}) };
  } else if (fall?.level === 'last' && fall.frame) {
    // The month the siege of his capital runs out leads in the same frame (ruling 133(a), point 3).
    card.lead = { ...blank(), cls: 2, stake: true, tag: 'LAST MONTH', tone: 'bad', ours: fall.nat, place: g.nations[fall.nat]?.name ?? '', kind: 'THE CAPITAL', month: fall.frame.month,
      head: fall.frame.head, line: fall.frame.sub, follows: fall.frame.sentence, facts: fall.frame.figures, region: fall.region };
  }
  card.rows = all.filter((r) => r !== card.lead);
  // (Seven rows; six beside the lead story; and a second band takes the room of one more.)
  card.show = (card.lead ? 6 : 7) - (card.bands.length > 1 ? 1 : 0);
  // A ring that closes on a corps or more of his is never under the fold (ruling 106): it takes the last row that
  // shows, and the lightest item that is no such ring becomes the first under the fold. That may be a row of war and
  // peace that found no band (the ring is the weightier news: b444, on seed 31's French May 1940 under ruling 227),
  // so the rows keep their classes' order above the fold and below it. Never a warning: a dated warning needs his
  // decision (ruling 64's first class), and a ring that finds nothing else to take stays where it falls.
  const rings = new Set(ranked.filter((x) => x.ring).map((x) => x.row));
  for (let i = card.show; i < card.rows.length; i++) {
    if (!rings.has(card.rows[i])) continue;
    let out = -1;
    for (let j = card.show - 1; j >= 0 && out < 0; j--) if (!rings.has(card.rows[j]) && card.rows[j].tag !== 'WARNING') out = j;
    if (out < 0) break;
    const ring = card.rows.splice(i, 1)[0], gone = card.rows.splice(out, 1)[0];
    card.rows.splice(card.show - 1, 0, ring);
    card.rows.splice(card.show, 0, gone);
  }
  // (A long word under the figure takes its width from the sentence beside it.)
  if (!opts.whole) for (const r of card.rows) r.line = sentence(r.line, r.figures.length > 1 ? ROW_ROOM.two : ROW_ROOM.one - Math.max(0, (r.figures[0]?.label.length ?? 0) - 14));
  card.quiet = fighting.length ? '' : opts.opening ? NOT_YET_FOUGHT : card.bands.length || all.some((r) => r.cls === 2) ? 'There was no fighting on your fronts this month.' : 'A quiet month.';
  if (card.rows.length > card.show) card.count = `ALL ${all.length} REPORTS`;

  // ---- The score --------------------------------------------------------------------------------------------
  const tone = (good: number, bad: number): CardTone => (good > bad ? 'good' : bad > good ? 'bad' : 'plain');
  const words = (n: number, w: string): string => (n ? `${n} ${w}` : `none ${w}`);
  if (provWon || provLost) card.boxes.push({ key: 'provinces', label: 'PROVINCES', figure: `${provWon - provLost > 0 ? '+' : provWon - provLost < 0 ? '−' : ''}${Math.abs(provWon - provLost)}`, sub: `${words(provWon, 'won')} · ${words(provLost, 'lost')}`, tone: tone(provWon, provLost) });
  // (A town of his besieged and holding is in neither number: the small line adds it. The art director's words.)
  const sieged = besiegedTowns.size ? ` · ${plural(besiegedTowns.size, 'town')} besieged` : '';
  if (battlesWon || battlesLost || sieged) card.boxes.push({ key: 'battles', label: 'BATTLES', figure: `${battlesWon} – ${battlesLost}`, sub: `${battlesWon || battlesLost ? `${battlesWon} won · ${battlesLost} lost` : 'none fought'}${sieged}`, tone: tone(battlesWon, battlesLost) });
  if (menOurs || menTheirs) card.boxes.push({ key: 'men', label: 'MEN LOST', figure: num(menOurs), sub: `theirs: ${num(menTheirs)}`, tone: menOurs > menTheirs ? 'bad' : 'plain' });
  if (sunk || shipsLost) card.boxes.push({ key: 'sea', label: 'AT SEA', figure: `${sunk} – ${shipsLost}`, sub: `${sunk} sunk · ${shipsLost} lost`, tone: tone(sunk, shipsLost) });
  if (shotDown || planesLost) card.boxes.push({ key: 'air', label: 'IN THE AIR', figure: `${num(shotDown)} – ${num(planesLost)}`, sub: `${num(shotDown)} shot down · ${num(planesLost)} lost`, tone: tone(shotDown, planesLost) });
  {
    // What the month leaves him after upkeep, as his books show it, and what is in hand. (Not the change in the
    // treasury: what he spent on his own orders is no loss.)
    const L = ledger(g, me);
    const net = Math.round(L.net * BUDGET.scale), hand = L.treasury < 0 ? Math.round(-L.treasury * BUDGET.scale) : Math.floor(L.treasury * BUDGET.scale + 1e-6);
    card.boxes.push({ key: 'treasury', label: 'TREASURY', figure: `${net > 0 ? '+' : net < 0 ? '−' : ''}${Math.abs(net).toLocaleString('en-US')}`, sub: L.treasury < 0 ? `${hand.toLocaleString('en-US')} in debt` : `${hand.toLocaleString('en-US')} in hand`, tone: net > 0 ? 'good' : net < 0 ? 'bad' : 'plain' });
  }

  // ---- 6. Home ----------------------------------------------------------------------------------------------
  const still = new Set(N.queue.map((b) => b.id));
  const group = (list: CardHome[], sign: string, text: string, region: number, next: boolean): void => {
    const place = g.regions[region] ? townName(g.regions[region]) : '';
    const at = list.find((x) => x.sign === sign && x.text === text && x.place === place && !!x.next === next);
    if (at) at.count++; else list.push({ sign, count: 1, text, place, region, ...(next ? { next: true } : {}) });
  };
  const done: CardHome[] = [], coming: CardHome[] = [];
  for (const o of mark.orders) {
    if (still.has(o.id)) continue;
    // A corps stands where it formed, which is the capital if its own place was full or lost; a work is lost with its province.
    const c = o.what === 'corps' ? g.corps.filter((x) => x.nat === me && !x.dead && x.name === o.name).sort((x, y) => y.id - x.id)[0] : undefined;
    if (o.what === 'corps' ? !c : g.regions[o.region]?.ctrl !== me) continue;
    // Guns, airfields and works are not counted: one order is a level of them at a place (ruling 133(g): never
    // '1 anti-aircraft guns finished'). The line says what is ready, and the place and its level stand under it.
    const item = o.what === 'work' ? (o.label === 'anti-aircraft guns' ? 'aa' : o.label === 'airfields' || o.sign === 'field' ? 'field' : o.sign === 'lights' ? 'lights' : null) : null;
    if (!o.unit && (item || o.label === 'fortifications')) {
      const level = item ? workLevel(g, item, o.region) : 0;
      const town = g.regions[o.region] ? townName(g.regions[o.region]) : '';
      const text = `${cap(o.label)} ${item === 'aa' || item === 'lights' ? 'ready' : 'finished'}`, place = level > 0 ? `${town}, level ${level}` : town;
      if (!done.some((x) => x.bare && x.text === text && x.place === place)) done.push({ sign: o.mark, count: 1, text, place, region: o.region, bare: true });
      continue;
    }
    group(done, o.mark, o.unit ? `${o.label}|${o.plural}|ready` : `${o.label}|${o.plural}|finished`, c ? c.reg : o.region, false);
  }
  for (const c of cityOrders(g, me)) {
    if (c.spied) continue;
    for (const l of c.lines) if (l.unit && l.months === 1) group(coming, l.mark, `${l.label}|${l.plural}|next month`, l.stands, true);
  }
  card.home = [...done, ...coming].map((x) => { if (x.bare) return x; const [one, many, tail] = x.text.split('|'); return { ...x, text: `${x.count === 1 ? one : many} ${tail}` }; });
  // Weather (ruling 143(b)): THE SKY, this month's and next month's chances, and last month's grounded groups.
  const sky = skyView(g, me);
  if (sky) card.sky = sky;
  // The line when men run out (ruling 133(d)(1)).
  const menOut = menOutLine(g, me);
  if (menOut && menOutDue(g, me)) card.menOut = menOut;
  // (A struck line first, in the place of the muted line it ends; the rows still waiting after it.)
  const waits = [...struck.map((h) => h.text), ...rowsWaitingOnRows(g, me)];
  if (waits.length) card.waits = waits;
  const news: Headline[] = [];
  for (const h of hs) {
    if (used.has(h) || !mine(h.nat)) continue;
    if (h.kind === 'letter') continue;
    if (h.desk) { card.staff.push({ text: h.text, short: staffShort(h.desk, h.text, opts.whole ? Infinity : FOOT_ROOM.staff), desk: h.desk, sign: h.desk === 'build' || h.desk === 'towns' ? buildSign(h.text) : DESK_SIGN[h.desk] ?? 'star', mayTake: staffRuns(g, me, h.desk as DeskId) }); used.add(h); continue; }
    const m = /^Research complete: ([^.]+)\./.exec(h.text);
    if (m) { card.research.push(m[1]); used.add(h); continue; }
    // The convoys' month is a line under the rows.
    if (h.kind === 'info' && h.arm === 'sea' && !card.foot) { card.foot = h.text; used.add(h); continue; }
    // (A work already said among the deliveries, 'Anti-aircraft guns ready' with its place and level, is not said a
    // second time as a line of news.)
    const built = h.kind === 'info' ? /^The (anti-aircraft guns|airfields|searchlights) at (.+) are (?:in place|ready): level \d+\.$/.exec(h.text) : null;
    // (The headline gives a place its article, "at the Ruhr": the delivery row has the bare name.)
    if (built && card.home.some((x) => x.bare && x.region !== undefined && g.regions[x.region]?.name === built[2].replace(/^the /, '') && x.text.toLowerCase().startsWith(built[1]))) { used.add(h); continue; }
    if (h.kind === 'info') news.push(h);
  }
  // The other news of home, in the lines the box has left. The three foot boxes share one height: three items of
  // two lines beside a lead story, five on a quieter card. What was delivered comes first and takes its lines;
  // each line of news is then given whole while lines last (fitFoot).
  const slots = card.lead ? 3 : 5;
  const goodLines = (x: { count: number; text: string; place?: string }): number => (x.text.length + (x.place ? 0.86 * x.place.length + 1 : 0) > 28 ? 2 : 1);
  const signed = card.home.length + card.research.length;
  const taken = card.home.reduce((n, x) => n + goodLines(x), 0) + card.research.reduce((n, t) => n + linesAt(`Research complete: ${t}.`, FOOT_ROOM.home), 0);
  // A march that halts wants his orders and comes first; armies that arrive at one place are one line; and where
  // the town held, the staff speaks of the battle there and not of its loss.
  const held = new Set(ranked.filter((x) => x.row.tag === 'HELD' || x.row.tag === 'BESIEGED').map((x) => x.row.place));
  const said: { text: string; full: string; hs: Headline[] }[] = [];
  for (const h of news.filter((x) => / has halted at .+ on its march to /.test(x.text))) said.push({ text: first(h.text), full: h.text, hs: [h] });
  const arrivals = new Map<string, Headline[]>();
  for (const h of news) { const m = /^(.+) has arrived at (.+)\.$/.exec(h.text); if (m) arrivals.set(m[2], [...(arrivals.get(m[2]) ?? []), h]); }
  for (const h of news) {
    if (said.some((s) => s.hs.includes(h))) continue;
    const m = /^(.+) has arrived at (.+)\.$/.exec(h.text);
    const same = m ? arrivals.get(m[2])! : [];
    if (m && same.length > 1) { said.push({ text: `${m[1]} and ${same.length - 1} more corps have arrived at ${m[2]}`, full: same.map((x) => x.text).join(' '), hs: same }); continue; }
    const lost = /^Your staff on the loss of ([^:]+):/.exec(h.text);
    const fixed = lost && held.has(lost[1].replace(/^the /, '')) ? h.text.replace(`the loss of ${lost[1]}`, `the battle at ${lost[1]}`) : h.text;
    // (A directive is said as its chip says it: who set it, and its title.)
    const dir = /^A directive from ([^:]+): /.exec(h.text), title = dir ? N.gov?.dir?.title : undefined;
    said.push({ text: dir && title ? `A directive from ${dir[1]}: ${title}` : first(fixed), full: fixed, hs: [h] });
  }
  if (signed < slots) for (const f of fitFoot(said.map((s) => s.text), opts.whole ? Infinity : signed ? FOOT_ROOM.home : FOOT_ROOM.homeWide, slots * 2 - taken, Math.min(3, slots - signed))) {
    card.also.push(f.text); card.alsoFull.push(said[f.at].full);
    for (const h of said[f.at].hs) used.add(h);
  }

  // ---- 7. Elsewhere: a flag and one line a war, the greatest event first -------------------------------------
  const weight: Record<Headline['kind'], number> = { war: 0, fall: 1, battle: 2, event: 3, info: 9, goal: 9, honour: 9, letter: 9 };
  // (News with a line of its own for this card stands before a battle's line for its country: sheet 25, box 5.)
  const newsRank = (h: Headline): number => (h.card ? weight.fall + 0.5 : weight[h.kind]);
  const seenNat = new Set<string>();
  const abroad: Headline[] = [];
  // (Of the same kind of news: the war on land before the sea and the air, and a city's news before a field's.)
  const armRank = (h: Headline): number => (h.arm === 'air' ? 2 : h.arm === 'sea' ? 1 : 0);
  const cityRank = (h: Headline): number => { const r = h.region !== undefined ? g.regions[h.region] : undefined; const s = r ? townSize(g, r) : ''; return s === 'capital' ? 0 : s === 'city' ? 1 : 2; };
  for (const h of [...hs].filter((x) => !used.has(x) && !!x.nat && !!g.nations[x.nat] && !mine(x.nat) && weight[x.kind] < 9 && !x.warn).sort((a, b) => newsRank(a) - newsRank(b) || armRank(a) - armRank(b) || cityRank(a) - cityRank(b) || Number(foe(b.nat) || side(b.nat)) - Number(foe(a.nat) || side(a.nat)))) {
    if (seenNat.has(h.nat!)) continue;
    seenNat.add(h.nat!);
    abroad.push(h);
  }
  // (Each whole while the box's lines last; one that will not fit is cut, or stays in the newsreel: fitFoot.)
  for (const f of fitFoot(abroad.map((h) => h.card ?? first(h.text)), opts.whole ? Infinity : FOOT_ROOM.elsewhere, slots * 2, slots)) {
    const h = abroad[f.at];
    card.elsewhere.push({ nat: h.nat!, text: f.text, full: h.text, ...(h.region !== undefined ? { region: h.region } : {}) });
    used.add(h);
  }

  // ---- 8. Letters -------------------------------------------------------------------------------------------
  for (const h of hs) if (h.kind === 'letter') { card.letters.push({ text: h.text, by: h.by ?? 'A soldier', ...(h.unit !== undefined ? { unit: h.unit } : {}) }); used.add(h); }
  card.more = hs.filter((h) => !used.has(h)).length;
  for (const x of [...card.bands, ...card.rows, ...(card.lead ? [card.lead] : [])]) x.when = dateWords(x);
  return card;
}

// ---- The line when men run out (ruling 133(d)(1), `fl-men-out`) -------------------------------------------------

/** What money alone still buys, by the rules: everything in the arsenal that takes no men from the reserve. */
export function moneyBuys(g: Game, nat: string): string[] {
  const n = g.nations[nat];
  const out = ['anti-aircraft guns', 'fortifications', 'factories', 'airfields', 'aircraft'];
  if (g.regions.some((r) => r.owner === nat && r.ctrl === nat && yardLevel(g, r) > 0)) out.push('warships');
  if (n && bureauCost(n) !== null) out.push('design teams');
  return out;
}

/**
 * The line when men run out (ruling 133(d)(1)): "The reserve of men is empty and 153,343 lies unspent. Money alone
 * still buys: anti-aircraft guns, fortifications, factories, airfields, aircraft, warships and design teams." Null
 * while the reserve can still fill a corps of one division, or the treasury holds no more than three months' income.
 * The grand campaign only. Words: it changes nothing.
 */
export function menOutLine(g: Game, nat: string): string | null {
  const n = g.nations[nat];
  if (!n?.alive || !menRules(g) || raisable(g, nat, 1) >= 1) return null;
  const L = ledger(g, nat);
  if (L.gross <= 0 || L.treasury <= 3 * L.gross) return null;
  const buys = moneyBuys(g, nat);
  const list = buys.length > 1 ? `${buys.slice(0, -1).join(', ')} and ${buys[buys.length - 1]}` : buys[0];
  return `The reserve of men is empty and ${Math.floor(L.treasury * BUDGET.scale + 1e-6).toLocaleString('en-US')} lies unspent. Money alone still buys: ${list}.`;
}

/** The line is said the first month the men are out and then every third (the month card marks it seen: cardSeen). */
const menOutDue = (g: Game, nat: string): boolean => { const from = g.flags[`menout:${nat}`]; return from === undefined || (g.turn - from) % 3 === 0; };

/**
 * The page has shown the player this month's card: marks what is said once or in turn, so that both games say it
 * alike. THE SKY's tip (ruling 143(g)) and the line when men run out (133(d)(1)). Never called in a war nobody plays.
 */
export function cardSeen(g: Game, card: MonthCard): void {
  if (card.sky?.tip) g.flags['wx:tip'] = g.turn;
  const me = card.viewer;
  if (!me) return;
  const key = `menout:${me}`;
  if (!menOutLine(g, me)) delete g.flags[key];
  else if (card.menOut && g.flags[key] === undefined) g.flags[key] = g.turn;
}
