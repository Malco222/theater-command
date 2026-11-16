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
 */
import { turnToDate } from '../core/ticket';
import { staffRuns, type DeskId } from './depth';
import { majorsOf } from './diplomacy';
import { BUDGET, ledger } from './economy';
import { allied, atWar, theName } from './game';
import { CLASSES, NAVY, shipCount, shipList } from './navy';
import { cityOrders } from './orders';
import { prizeOf, townName, townSize } from './towns';
import type { AirReport, Game, Headline, SeaSide, Ships } from './types';

export type CardTone = 'good' | 'bad' | 'warn' | 'plain';
/** The eight pictures both builds have (art-director/raid-card/targets). */
export type CardPicture = 'troops' | 'fieldworks' | 'town' | 'fleet' | 'factories' | 'rail-yard' | 'harbour' | 'airfield';
export interface CardFigure { n: string; label: string }
/** The battle report a row opens. */
export interface CardReport { type: 'land' | 'sea' | 'air' | 'shore'; id: number }

/** Something on order: a unit or a work, and where it will stand. */
export interface MonthOrder { id: number; what: string; unit: boolean; mark: string; label: string; plural: string; name: string; region: number }

/** What an interface keeps as the player's month opens: where each list stood and who held what; and, as he ends it, what was on order. */
export interface MonthMark {
  turn: number;
  h: number; r: number; s: number; a: number; shore: number;
  /** Who held each province. */
  before: string[];
  /** The countries at war with anyone: a great power not among them that is at war a month later has entered the war. */
  warring: string[];
  orders: MonthOrder[];
}

/** A red band: classes 1 and 2 of the ranking. */
export interface CardBand {
  word: 'WAR' | 'WARNING' | 'FALLEN' | 'FREED';
  /** A warning's own short title ('Coast watch'). */
  title?: string;
  /** The flags, at most two. */
  nats: string[];
  /** The headline, of about eight words, and the whole line of news it was cut from. */
  head: string;
  text: string;
  week: number;
  region?: number;
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
  /** Ruling 64's class: 2 for a red item that found no band, 3 ground, 4 sea and air, 5 goals and honours. */
  cls: number;
  arm: 'land' | 'sea' | 'air' | null;
  picture: CardPicture | null;
  /** The sign in the disc: a key of the order signs ('inf', 'arm', 'art', 'hull', 'cv', 'ss', 'bmr', 'ftr', 'eng', 'para', 'star'), or '' for none. */
  sign: string;
  place: string;
  /** One word, or '': WON, LOST, HELD, HIT, MISSED, SUNK, BLOCKED. The row's edge and its tag share the tone. */
  tag: string;
  tone: CardTone;
  /** The flag on the left and the flag after "against". */
  ours: string | null;
  theirs: string | null;
  /** One sentence. */
  line: string;
  /** One figure, or two for a land battle: ours first, theirs second. */
  figures: CardFigure[];
  /** The place a pin shows on the map; and the battle report the row opens. */
  region?: number;
  report?: CardReport;
  week: number;
  /** For the lead story: the kind word of its tag, a headline of about eight words, one line on what follows from it, and three figures. */
  kind: string;
  head: string;
  follows: string;
  facts: CardFigure[];
}

export interface CardHome { sign: string; count: number; text: string; place: string; region?: number; /** Next month's: drawn in grey. */ next?: boolean }
export interface CardStaff { text: string; desk: string; sign: string; /** True while the staff still runs that desk: the button takes it back from next month. */ mayTake: boolean }
export interface CardElse { nat: string; text: string; region?: number }
export interface CardLetter { text: string; by: string; unit?: number }

export interface MonthCard {
  /** The month reported, as a turn and in words; and whose card it is. */
  turn: number;
  month: string;
  viewer: string;
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
  research: string[];
  also: string[];
  /** What his staff did. It stands: the button takes the desk back from next month. */
  staff: CardStaff[];
  elsewhere: CardElse[];
  letters: CardLetter[];
  /** Lines of the month's news that are not on the card. They are all in the newsreel. */
  more: number;
}

const num = (n: number): string => Math.round(n).toLocaleString('en-US');
const cap = (s: string): string => s.charAt(0).toUpperCase() + s.slice(1);
const plural = (n: number, one: string, many = `${one}s`): string => `${n} ${n === 1 ? one : many}`;
const WORDS = ['No', 'One', 'Two', 'Three', 'Four', 'Five', 'Six', 'Seven', 'Eight', 'Nine', 'Ten'];
const count = (n: number): string => WORDS[n] ?? String(n);
/** The first sentence of a line of news, without its full stop. */
const first = (text: string): string => { const m = /^(.*?[.!?])(\s|$)/.exec(text); return (m ? m[1] : text).replace(/[.!]$/, ''); };
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
/** The room a row has for its sentence, in letters: less beside two figures. */
const ROW_ROOM = { one: 54, two: 44 };
/** "A", "A and B", "A, B and C". */
const listOf = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
const CLASS_WORD = /\b(?:sinks?|torpedoes) (\d+) ([A-Z][a-z]+) (submarines?|destroyers?|cruisers?|battleships?|carriers?|ships?|transports?)/;
/** A fleet's name as it reads in a sentence. */
const the = (name: string): string => (/^the /i.test(name) ? name.replace(/^The /, 'the ') : `the ${name}`);
/** "A Polish", "An Italian". */
const an = (adj: string): string => `${/^[AEIOU]/i.test(adj) ? 'An' : 'A'} ${adj}`;
const SIGNS = new Set(['inf', 'arm', 'mot', 'cav', 'mtn', 'art', 'eng']);
const DESK_SIGN: Record<string, string> = { research: 'star', build: 'inf', diplomacy: 'star', spies: 'star', air: 'ftr', navy: 'hull' };

/** What the player has on order. An interface keeps it in the mark as he ends his month. */
export function monthOrders(g: Game, viewer: string | null = g.human): MonthOrder[] {
  if (!viewer || !g.nations[viewer]) return [];
  return cityOrders(g, viewer).filter((c) => !c.spied).flatMap((c) => c.lines.map((l) => ({ id: l.id, what: l.what, unit: l.unit, mark: l.mark, label: l.label, plural: l.plural, name: l.name, region: l.stands })));
}

/** What an interface keeps as the player's month opens. */
export function monthMark(g: Game, viewer: string | null = g.human): MonthMark {
  return {
    turn: g.turn,
    h: g.headlines.length, r: g.reports.length, s: g.seaReports.length, a: g.airReports.length, shore: g.shoreReports.length,
    before: g.regions.map((r) => r.ctrl),
    warring: Object.values(g.nations).filter((n) => n.alive && n.atWar.length > 0).map((n) => n.id),
    orders: monthOrders(g, viewer),
  };
}

interface Ranked { row: CardRow; sub: number; weight: number; /** A siege laid, and the losses of the two sides in it: told with the town's own fight if that came the same month. */ laid?: { region: number; ours: number; theirs: number; first: CardFigure } }

/** The card for everything that has happened since the mark was taken. */
export function monthCard(g: Game, mark: MonthMark, viewer: string | null = g.human): MonthCard {
  const me = viewer ?? '';
  const N = g.nations[me];
  const card: MonthCard = {
    turn: mark.turn, month: turnToDate(mark.turn).label, viewer: me, bands: [], boxes: [], lead: null, quiet: '', rows: [], show: 7, count: '', foot: '',
    home: [], research: [], also: [], staff: [], elsewhere: [], letters: [], more: 0,
  };
  if (!N) { card.quiet = 'A quiet month.'; return card; }
  const mine = (nat?: string | null): boolean => nat === me;
  // (A line of news may name a country that is not in this game: it is nobody's ally and nobody's enemy.)
  const side = (nat?: string | null): boolean => !!nat && !!g.nations[nat] && (nat === me || allied(g, nat, me));
  const foe = (nat?: string | null): boolean => !!nat && !!g.nations[nat] && atWar(g, nat, me);
  const hs = g.headlines.slice(mark.h);
  const used = new Set<Headline>();
  const lands = g.reports.slice(mark.r);
  const seas = g.seaReports.slice(mark.s);
  const airs = g.airReports.slice(mark.a);
  const shores = g.shoreReports.slice(mark.shore);
  const ranked: Ranked[] = [];
  const blank = (): CardRow => ({ cls: 3, arm: null, picture: null, sign: '', place: '', tag: '', tone: 'plain', ours: null, theirs: null, line: '', figures: [], week: 1, kind: '', head: '', follows: '', facts: [] });

  // ---- 3. Ground: the battles his own troops fought --------------------------------------------------------
  let battlesWon = 0, battlesLost = 0, menOurs = 0, menTheirs = 0;
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
    if (!r.yielded) { if (good) battlesWon++; else battlesLost++; }
    menOurs += oursLost; menTheirs += theirsLost;
    const lead = [...us].filter((u) => !u.guard).sort((a, b) => b.men - a.men)[0];
    const row = blank();
    row.arm = 'land';
    row.place = place;
    row.week = r.week;
    row.region = r.region;
    row.report = { type: 'land', id: r.id };
    row.ours = me;
    row.theirs = att ? r.defNat : r.attNat;
    row.sign = forTown || (r.taken && city) ? 'art' : lead && SIGNS.has(lead.kind) ? lead.kind : 'inf';
    row.picture = forTown || (r.taken && city) ? 'town' : def ? 'fieldworks' : 'troops';
    row.kind = forTown ? 'SIEGE' : 'BATTLE';
    row.figures = [{ n: num(oursLost), label: 'lost' }, { n: num(theirsLost), label: 'theirs' }];
    row.facts = [corps ? { n: plural(corps, 'corps', 'corps'), label: att ? 'went in' : 'held the line' } : { n: 'The guard', label: 'stood alone' }, { n: num(oursLost), label: 'men lost' }, { n: num(theirsLost), label: 'of theirs' }];
    const prize = reg ? prizeOf(g, reg.id, me) : null;
    const worth = (gone: boolean): string => {
      if (!prize) return '';
      if (prize.capitalOf && prize.capitalOf !== me) return gone ? '' : `The seat of ${theName(g.nations[prize.capitalOf])}'s government is in your hands.`;
      if (prize.factories > 0) return gone ? `Its ${plural(prize.factories, 'factory', 'factories')} ${prize.factories === 1 ? 'is' : 'are'} lost to you.` : `Its ${plural(prize.factories, 'factory', 'factories')} now ${prize.factories === 1 ? 'works' : 'work'} for you.`;
      if (prize.oil > 0) return gone ? 'Its oil is lost to you.' : 'Its oil now flows to your depots.';
      if (prize.harbour) return gone ? 'Its harbour is closed to your ships.' : 'Its harbour is open to your ships.';
      return '';
    };
    if (r.yielded) {
      row.tag = att ? 'WON' : 'LOST'; row.tone = att ? 'good' : 'bad';
      row.line = att ? 'Taken without a fight.' : 'Given up under orders.';
      row.head = att ? `${place} is taken without a fight` : `${place} is given up`;
      row.follows = worth(!att) || (att ? 'The enemy fell back before the attack.' : 'Your troops fell back before the attack.');
    } else if (r.siege) {
      row.tag = att ? 'WON' : 'HELD'; row.tone = att ? 'good' : 'warn';
      row.line = att ? 'The field is won. The town is besieged.' : 'The field is lost. The town holds out.';
      row.head = att ? `${place} is under siege` : `${place} is besieged`;
      row.follows = att ? 'The province falls only when the town is taken.' : 'The province is yours for as long as the town holds.';
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
    const lostCity = !good && city && r.taken, wonCity = good && city && r.taken;
    // One story, one row: a siege laid and the town's own fight in the same month are told as the town's fight,
    // with the losses of both.
    const before = forTown && !r.siege ? ranked.findIndex((x) => x.laid?.region === r.region) : -1;
    if (before >= 0) {
      const laid = ranked.splice(before, 1)[0].laid!;
      const o = oursLost + laid.ours, t = theirsLost + laid.theirs;
      row.figures = [{ n: num(o), label: 'lost' }, { n: num(t), label: 'theirs' }];
      row.facts = [corps ? row.facts[0] : laid.first, { n: num(o), label: 'men lost' }, { n: num(t), label: 'of theirs' }];
    }
    ranked.push({ row, sub: lostCity ? 0 : wonCity ? 1 : 2, weight: r.attLost + r.defLost + r.prisoners + (city ? 5e5 : 0), ...(r.siege ? { laid: { region: r.region, ours: oursLost, theirs: theirsLost, first: row.facts[0] } } : {}) });
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
      row.line = ids.length === 1 ? (gain ? 'Occupied without a battle.' : 'Lost without a battle.') : gain ? `${count(ids.length)} provinces occupied without a battle.` : `${count(ids.length)} provinces lost without a battle.`;
      row.figures = [{ n: String(ids.length), label: ids.length === 1 ? 'province' : 'provinces' }];
      row.region = best;
      row.kind = 'GROUND';
      row.head = gain ? `${plural(ids.length, 'province')} occupied${other ? ` from ${theName(other)}` : ''}` : `${plural(ids.length, 'province')} lost${other ? ` to ${theName(other)}` : ''}`;
      row.follows = ids.length > 1 ? `The largest is ${g.regions[best].name}.` : '';
      row.facts = row.figures;
      ranked.push({ row, sub: 3, weight: ids.length });
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
    row.report = { type: 'sea', id: r.id };
    row.ours = me; row.theirs = them[0]?.nat ?? null;
    row.tag = good ? 'WON' : 'LOST'; row.tone = good ? 'good' : 'bad';
    row.line = good ? `${cap(the(ourFleet))} beat ${the(theirFleet)}.` : `${cap(the(theirFleet))} beat ${the(ourFleet)}.`;
    row.figures = good && shipCount(theirSunk) ? [{ n: String(shipCount(theirSunk)), label: `${classWord(theirSunk)} sunk` }] : shipCount(ourSunk) ? [{ n: String(shipCount(ourSunk)), label: `${classWord(ourSunk)} lost` }] : shipCount(theirSunk) ? [{ n: String(shipCount(theirSunk)), label: `${classWord(theirSunk)} sunk` }] : [];
    row.kind = 'FLEET ACTION';
    row.head = good ? `Victory in the ${r.zoneName.replace(/^the /i, '')}` : `Defeat in the ${r.zoneName.replace(/^the /i, '')}`;
    row.follows = shipCount(theirSunk) || shipCount(ourSunk) ? `${cap(shipCount(theirSunk) ? shipList(theirSunk) : 'no enemy ship')} sunk${shipCount(ourSunk) ? ` for the loss of ${shipList(ourSunk)}` : ', and no ship of yours lost'}.` : 'Neither side lost a ship.';
    row.facts = [{ n: String(shipCount(sumShips(us, 'ships'))), label: 'ships engaged' }, { n: String(shipCount(ourSunk)), label: 'lost' }, { n: String(shipCount(theirSunk)), label: 'of theirs sunk' }];
    ranked.push({ row, sub: good ? 1 : 0, weight: 40 * (shipCount(theirSunk) + shipCount(ourSunk)) + 20 });
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
    if (r.region >= 0) row.region = r.region;
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
        : r.mission === 'harbour' ? ['harbour', `${r.place} harbour`, `${what} struck the ships in harbour.`, 'The raid on the harbour was beaten off.']
        : r.mission === 'fleet' ? ['fleet', cap(the(r.ships?.[0]?.fleet ?? r.place)), `${what} struck the fleet at sea.`, 'The strike on the fleet was beaten off.']
        : r.mission === 'strike' ? ['fleet', cap(r.place.replace(/^the /, 'The ')), `${what} struck the shipping there.`, 'The strike on the shipping was beaten off.']
        : r.mission === 'ground' ? ['troops', r.place, `${what} struck the troops there.`, 'The raid on the troops was beaten off.']
        : r.mission === 'town' ? ['town', town, `${what} broke down its defences.`, 'The raid on its defences was beaten off.']
        : ['factories', r.place, `${what} hit its factories${r.mission === 'night' ? ' by night' : ''}.`, 'The bombers were beaten off.'];
      row.picture = pic; row.place = place;
      row.line = r.won ? (att ? hit : hit.replace(/^([\d,]+) (bombers|aircraft)/, 'Their $2')) : miss;
      const low = place.replace(/^The /, 'the ');
      row.head = r.mission === 'fleet' || r.mission === 'strike'
        ? (r.won ? `${r.mission === 'fleet' ? place : `Shipping in ${low}`} is struck from the air` : att ? `The strike on ${r.mission === 'fleet' ? low : `shipping in ${low}`} fails` : `The strike on ${r.mission === 'fleet' ? low : `shipping in ${low}`} is beaten off`)
        : r.won ? `${place} is bombed` : att ? `The raid on ${place} fails` : `${place} beats off the raid`;
    }
    row.follows = r.result;
    const sunkHere = shipsHit ? shipCount(shipsHit) : 0;
    row.figures = sunkHere ? [{ n: String(sunkHere), label: `${classWord(shipsHit!)} ${att ? 'sunk' : 'lost'}` }]
      : att ? (ourLost ? [{ n: num(ourLost), label: r.att.every((u) => !u.lostFtr) ? 'bombers lost' : 'aircraft lost' }] : theirLost ? [{ n: num(theirLost), label: 'shot down' }] : [])
      : theirLost ? [{ n: num(theirLost), label: 'shot down' }] : ourLost ? [{ n: num(ourLost), label: 'fighters lost' }] : [];
    row.facts = [{ n: num(flown), label: att ? 'aircraft flew' : 'aircraft came' }, { n: num(ourLost), label: 'lost' }, { n: num(theirLost), label: 'of theirs shot down' }];
    ranked.push({ row, sub: good ? 1 : 0, weight: flown / 10 + ourLost + theirLost + 60 * sunkHere });
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
      : r.what === 'troops' ? [{ n: String(r.divs), label: r.divs === 1 ? 'division shelled' : 'divisions shelled' }]
      : r.what === 'city' ? [{ n: `${Math.round(r.output * 100)}%`, label: 'of output stopped' }]
      : [{ n: `${Math.round(r.works ?? 0)}%`, label: 'of its works' }];
    row.kind = 'SHELLED FROM THE SEA';
    row.head = `${r.place} is shelled from the sea`;
    row.follows = r.result;
    row.facts = [{ n: String(shipCount(r.ships)), label: 'ships stood in' }, ...row.figures];
    ranked.push({ row, sub: att ? 1 : 0, weight: 15 + 40 * lost });
  }

  // News of fighting that no battle report tells: bridges blown, a landing, a ship torpedoed, an army cut off.
  const involves = (h: Headline): boolean => mine(h.nat) || mine(h.won) || mine(h.lost) || (h.region !== undefined && (g.regions[h.region]?.ctrl === me || mark.before[h.region] === me));
  for (const h of hs) {
    if (!h.arm || h.warn || (h.kind !== 'battle' && h.kind !== 'event')) continue;
    if (!involves(h)) continue;
    // One story, one row: the battle report tells it better.
    const told = h.arm === 'land' ? h.region !== undefined && fought.has(h.region)
      : h.arm === 'air' ? (h.region !== undefined ? airAt.has(h.region) || shelled.has(h.region) : airAtSea.includes(h.nat ?? '') || airs.some((r) => r.zone >= 0))
      : (h.region !== undefined && shelled.has(h.region)) || (h.won !== undefined && h.lost !== undefined && seaPairs.some(([a, b]) => (a === h.won && b === h.lost) || (a === h.lost && b === h.won)));
    if (told) { used.add(h); continue; }
    const good = side(h.won) || (h.won === undefined && foe(h.lost)), bad = !good && (side(h.lost) || (h.lost === undefined && foe(h.won)));
    const bridges = /\bbridges?\b/.test(h.text);
    const row = blank();
    row.cls = h.arm === 'land' ? 3 : 4; row.arm = h.arm;
    row.week = h.week;
    if (h.region !== undefined) row.region = h.region;
    // (News from the sea names its sea: the row's place takes it, and the line is the shorter for it.)
    const where = h.region === undefined ? / in (the [A-Z][A-Za-z' -]*?)(?= and |[.,:]|$)/.exec(h.text) : null;
    row.place = h.region !== undefined ? g.regions[h.region].name : where ? cap(where[1].replace(/^the /, '')) : h.arm === 'sea' ? 'At sea' : 'In the air';
    row.sign = h.arm === 'land' ? (bridges ? 'eng' : /airborne/.test(h.text) ? 'para' : 'inf') : h.arm === 'sea' ? (/submarine|torpedo|convoy|shipping/i.test(h.text) ? 'ss' : 'hull') : 'bmr';
    row.picture = h.arm === 'land' ? (bridges || /\b(town|city|garrison|siege)\b/.test(h.text) ? 'town' : 'troops') : h.arm === 'sea' ? 'fleet' : h.region !== undefined ? 'factories' : 'fleet';
    const sinking = h.arm !== 'land' && /\b(sinks?|sunk|torpedoes|scuttled)\b/.test(h.text);
    // Bridges blown in his path block him; blown by his own men they are his doing.
    const blocked = bridges && /\bblow\b/.test(h.text) && !mine(h.nat);
    row.tag = blocked ? 'BLOCKED' : sinking && (good || bad) ? 'SUNK' : good ? 'WON' : bad ? 'LOST' : '';
    row.tone = blocked ? 'warn' : good ? 'good' : bad ? 'bad' : 'plain';
    const other = [h.won, h.lost, h.nat].find((x) => x !== undefined && x !== me && !!g.nations[x]);
    row.ours = me; row.theirs = other ?? null;
    const said = first(where ? h.text.replace(where[0], '') : h.text);
    const river = bridges ? /bridges over (the [A-Z][A-Za-z' -]*?)(?= at | between | as | before |[.,]|$)/.exec(h.text) : null;
    const ships = sinking ? CLASS_WORD.exec(h.text) : null;
    // A row says it in one short sentence; the whole line of news is in the newsreel.
    row.line = river ? (/\bseize\b/.test(h.text) ? `The bridges over ${river[1]} were taken intact.` : /\bblow\b/.test(h.text) ? `The bridges over ${river[1]} are blown.` : `${said}.`) : `${said}.`;
    row.kind = h.arm === 'land' ? 'ON LAND' : h.arm === 'sea' ? 'AT SEA' : 'IN THE AIR';
    row.head = ships ? `${cap(count(Number(ships[1])).toLowerCase())} ${ships[2]} ${ships[3]} sunk` : brief(said, 64);
    if (ships) row.figures = [{ n: ships[1], label: `${ships[3]} ${good ? 'sunk' : 'lost'}` }];
    // An army cut off: the row carries the men as its figure.
    const ring = /^The ring closes at [^:]+: (.+?), ([\d,]+) men, (?:is|are) cut off/.exec(h.text);
    if (ring) {
      const force = /^(\S+) .*?\b(corps|army|armies|divisions?)$/i.exec(ring[1]);
      const who = force ? `${force[1]} ${force[2]}` : ring[1];
      row.line = `The ring closes: ${who} ${/^(1|one|a|an) /i.test(who) ? 'is' : 'are'} cut off.`;
      row.figures = [{ n: ring[2], label: 'men cut off' }];
    }
    const escorts = /^The escorts of (?:the )?(.+?) sinks? /.exec(h.text);
    if (escorts && ships) row.line = `Sunk by escorts of the ${escorts[1]}.`;
    const arms = /^Cut off and out of supply, (.+?) lays? down (?:its|their) arms(?: at ([^:.]+))?(?:: (?:.+?, )?([\d,]+) men)?\./.exec(h.text);
    if (arms) { row.line = `${cap(arms[1])} lays down its arms${arms[2] ? ` at ${arms[2]}` : ''}.`; if (arms[3]) row.figures = [{ n: arms[3], label: side(h.lost) || mine(h.nat) ? 'men lost' : 'prisoners' }]; }
    used.add(h);
    ranked.push({ row, sub: h.arm === 'land' ? (bad ? 2 : 4) : bad ? 0 : 1, weight: bad ? 30 : 10 });
  }

  // ---- 5. Goals and honours ---------------------------------------------------------------------------------
  for (const h of hs) {
    if (h.kind !== 'goal' && !(h.kind === 'honour' && mine(h.nat))) continue;
    const row = blank();
    row.cls = 5;
    row.sign = 'star';
    row.week = h.week;
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
  const freed = new Set(g.passings.filter((p) => p.turn >= mark.turn && p.kind === 'freed').map((p) => p.nat));
  const passed = (nat?: string) => g.passings.find((p) => p.turn >= mark.turn && p.nat === nat);
  const banded: { band: CardBand; rank: number; h: Headline }[] = [];
  // A great power entering the war: at peace when the month opened and at war now. The first line of news that
  // names it is its band. (One already at war that declares another is war and peace like anyone's: his band only
  // where it touches his own war.)
  const entered = new Set([...majors].filter((m) => !!g.nations[m]?.alive && g.nations[m].atWar.length > 0 && !mark.warring.includes(m)));
  hs.forEach((h) => {
    const warn = !!h.warn && mine(h.nat);
    const enters = h.kind === 'war' ? [...entered].filter((m) => m === h.nat || h.text.includes(g.nations[m].name)) : [];
    for (const m of enters) entered.delete(m);
    const great = enters.length > 0 || (h.kind === 'fall' && !!h.nat && majors.has(h.nat));
    const p = h.kind === 'fall' ? passed(h.nat) : undefined;
    const touches = (h.kind === 'war' || h.kind === 'fall') && (mine(h.nat) || side(h.nat) || foe(h.nat) || (!!p && (side(p.by) || foe(p.by))));
    if (!warn && !great && !touches) return;
    const word: CardBand['word'] = warn ? 'WARNING' : h.kind === 'war' ? 'WAR' : h.nat && freed.has(h.nat) ? 'FREED' : 'FALLEN';
    const nats = warn ? [me] : [...new Set([h.nat, ...enters, p?.by])].filter((x): x is string => !!x && !!g.nations[x] && x !== 'none').slice(0, 2);
    banded.push({ band: { word, title: warn ? h.warn : undefined, nats, head: first(h.text), text: h.text, week: h.week, region: h.region }, rank: great ? 0 : warn ? 1 : 2, h });
    used.add(h);
  });
  // One story, one band. A country's fall is told once, by the line that says the country fell (the capital's own
  // fall is the same news); and a country that declares war on several in a month does so in one band.
  for (const nat of new Set(banded.filter((b) => b.h.kind === 'fall' && b.h.nat).map((b) => b.h.nat!))) {
    const same = banded.filter((b) => b.h.kind === 'fall' && b.h.nat === nat);
    const keep = [...same].reverse().find((b) => b.h.region === undefined) ?? same[same.length - 1];
    for (const b of same) if (b !== keep) banded.splice(banded.indexOf(b), 1);
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
    b.band.head = `${m[1]} declares war on ${listOf(on)}`;
    b.rank = Math.min(b.rank, ...more.map((x) => x.rank));
    b.band.text = [...[b, ...more].map((x) => x.band.text), ...echo.map((h) => h.text)].join(' ');
    b.band.nats = [...new Set([...b.band.nats, ...more.flatMap((x) => x.band.nats)])].slice(0, 2);
    for (const x of more) banded.splice(banded.indexOf(x), 1);
  }
  for (const b of banded) b.band.head = brief(b.band.head, 76);
  banded.sort((a, b) => a.rank - b.rank || Number(mine(b.h.nat)) - Number(mine(a.h.nat)));
  card.bands = banded.slice(0, 2).map((b) => b.band);
  // An item that finds no band stays red and sits above the first battle row.
  for (const b of banded.slice(2)) {
    const row = blank();
    row.cls = 2;
    row.week = b.band.week;
    row.place = b.band.word === 'WARNING' ? b.band.title ?? 'Warning' : g.nations[b.band.nats[0] ?? '']?.name ?? cap(b.band.word.toLowerCase());
    row.tag = b.band.word;
    row.tone = 'bad';
    row.ours = b.band.nats[0] ?? null;
    row.theirs = b.band.nats[1] ?? null;
    row.line = `${b.band.head}.`;
    row.kind = b.band.word;
    row.head = b.band.head;
    if (b.band.region !== undefined) row.region = b.band.region;
    ranked.push({ row, sub: b.rank, weight: 0 });
  }

  // ---- The order, the lead and the fold ---------------------------------------------------------------------
  const badFirst = (t: CardTone): number => (t === 'bad' ? 0 : t === 'warn' ? 1 : t === 'good' ? 2 : 3);
  ranked.sort((a, b) => a.row.cls - b.row.cls || a.sub - b.sub || badFirst(a.row.tone) - badFirst(b.row.tone) || b.weight - a.weight);
  const all = ranked.map((x) => x.row);
  const fighting = all.filter((r) => r.cls === 3 || r.cls === 4);
  card.lead = fighting.find((r) => r.picture !== null) ?? null;
  card.rows = all.filter((r) => r !== card.lead);
  // (Seven rows; six beside the lead story; and a second band takes the room of one more.)
  card.show = (card.lead ? 6 : 7) - (card.bands.length > 1 ? 1 : 0);
  for (const r of card.rows) r.line = sentence(r.line, r.figures.length > 1 ? ROW_ROOM.two : ROW_ROOM.one);
  card.quiet = fighting.length ? '' : card.bands.length || all.some((r) => r.cls === 2) ? 'There was no fighting on your fronts this month.' : 'A quiet month.';
  if (card.rows.length > card.show) card.count = all.every((r) => r.cls === 3 || r.cls === 4) ? `ALL ${all.length} BATTLES` : `ALL ${all.length} ITEMS`;

  // ---- The score --------------------------------------------------------------------------------------------
  const tone = (good: number, bad: number): CardTone => (good > bad ? 'good' : bad > good ? 'bad' : 'plain');
  const words = (n: number, w: string): string => (n ? `${n} ${w}` : `none ${w}`);
  if (provWon || provLost) card.boxes.push({ key: 'provinces', label: 'PROVINCES', figure: `${provWon - provLost > 0 ? '+' : provWon - provLost < 0 ? '−' : ''}${Math.abs(provWon - provLost)}`, sub: `${words(provWon, 'won')} · ${words(provLost, 'lost')}`, tone: tone(provWon, provLost) });
  if (battlesWon || battlesLost) card.boxes.push({ key: 'battles', label: 'BATTLES', figure: `${battlesWon} – ${battlesLost}`, sub: `${battlesWon} won · ${battlesLost} lost`, tone: tone(battlesWon, battlesLost) });
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
    group(done, o.mark, o.unit ? `${o.label}|${o.plural}|ready` : `${o.label}|${o.plural}|finished`, c ? c.reg : o.region, false);
  }
  for (const c of cityOrders(g, me)) {
    if (c.spied) continue;
    for (const l of c.lines) if (l.unit && l.months === 1) group(coming, l.mark, `${l.label}|${l.plural}|next month`, l.stands, true);
  }
  card.home = [...done, ...coming].map((x) => { const [one, many, tail] = x.text.split('|'); return { ...x, text: `${x.count === 1 ? one : many} ${tail}` }; });
  for (const h of hs) {
    if (used.has(h) || !mine(h.nat)) continue;
    if (h.kind === 'letter') continue;
    if (h.desk) { card.staff.push({ text: h.text, desk: h.desk, sign: DESK_SIGN[h.desk] ?? 'star', mayTake: staffRuns(g, me, h.desk as DeskId) }); used.add(h); continue; }
    const m = /^Research complete: ([^.]+)\./.exec(h.text);
    if (m) { card.research.push(m[1]); used.add(h); continue; }
    // The convoys' month is a line under the rows.
    if (h.kind === 'info' && h.arm === 'sea' && !card.foot) { card.foot = h.text; used.add(h); continue; }
    if (h.kind === 'info' && card.also.length < 2) { card.also.push(h.text); used.add(h); }
  }

  // ---- 7. Elsewhere: a flag and one line a war, the greatest event first -------------------------------------
  const weight: Record<Headline['kind'], number> = { war: 0, fall: 1, battle: 2, event: 3, info: 9, goal: 9, honour: 9, letter: 9 };
  const seenNat = new Set<string>();
  // (Of the same kind of news: the war on land before the sea and the air, and a city's news before a field's.)
  const armRank = (h: Headline): number => (h.arm === 'air' ? 2 : h.arm === 'sea' ? 1 : 0);
  const cityRank = (h: Headline): number => { const r = h.region !== undefined ? g.regions[h.region] : undefined; const s = r ? townSize(g, r) : ''; return s === 'capital' ? 0 : s === 'city' ? 1 : 2; };
  for (const h of [...hs].filter((x) => !used.has(x) && !!x.nat && !!g.nations[x.nat] && !mine(x.nat) && weight[x.kind] < 9 && !x.warn).sort((a, b) => weight[a.kind] - weight[b.kind] || armRank(a) - armRank(b) || cityRank(a) - cityRank(b) || Number(foe(b.nat) || side(b.nat)) - Number(foe(a.nat) || side(a.nat)))) {
    if (seenNat.has(h.nat!) || card.elsewhere.length >= 4) continue;
    seenNat.add(h.nat!);
    card.elsewhere.push({ nat: h.nat!, text: `${brief(first(h.text), 96)}.`, ...(h.region !== undefined ? { region: h.region } : {}) });
    used.add(h);
  }

  // ---- 8. Letters -------------------------------------------------------------------------------------------
  for (const h of hs) if (h.kind === 'letter') { card.letters.push({ text: h.text, by: h.by ?? 'A soldier', ...(h.unit !== undefined ? { unit: h.unit } : {}) }); used.add(h); }
  card.more = hs.filter((h) => !used.has(h)).length;
  return card;
}
