// The country in danger (the design director's rulings 117(d) and 133(a); the Playtester's find on Version 82.3: a
// player's Germany fell with no red band on the card before). No campaign ends on the player without a red band at
// least a month before, saying what ends it and what would stop it. A stake that ends a campaign by an event has its
// band already (diplomacy.ts, stakeWarning). This is the other way a campaign ends: defeat in the field.
//
// The rule is older than this file and is not changed by it (game.ts: collapseScore, checkCollapse). A country gives
// in when its capital, its home country and its army lost, weighed together, pass what it will bear. Here the rules
// hand both interfaces one reading of that for the player's own country, made from the position as his month opens:
// how near its limit, its three parts in plain words, and whether it can give in before he next gives orders. Four
// things are said from it (ruling 133(a)):
//   1. a red band every month it can give in next month: "Germany can fall next month";
//   2. once, earlier, when an enemy army first stands beside the capital's province or the reckoning passes two
//      thirds of its limit: "The enemy is within reach of Berlin";
//   3. where the siege's own clock has carried it over: the LAST MONTH frame, "Germany gives in as October opens
//      unless the siege of Berlin is broken.";
//   4. a besieged capital's lead adds "It is your capital: the country will not long outlast it." (monthcard.ts).
// Nothing is written and no dice are thrown.
import { clamp } from '../core/math';
import { turnToDate } from '../core/ticket';
import { armyCard } from './armies';
import { specOf } from './economy';
import { COLLAPSE, allied, armySize, atWar, capitalLost, exhausted, fights, gunsRule, homeLost, railReachFor, theName, victorOf } from './game';
import { armyOf } from './joint';
import { sightOf } from './intel';
import { standsFreed } from './liberation';
import { farGround, planMarch, type GroundOf } from './marches';
import { willLost } from './resources';
import { SIEGE, besieged, besiegerOf, investedOn, ringClosed, siegeRules, siegeView, whyNotStorm } from './siege';
import { townName } from './towns';
import { current, reach, type Reach } from './turn';
import type { Corps, Game, Region } from './types';

/** The title of the warning: the band's own name on the card. */
export const IN_DANGER = 'The country in danger';
/** What the lead of a besieged capital of his own adds (ruling 133(a), point 4). */
export const CAPITAL_BESIEGED = 'It is your capital: the country will not long outlast it.';
/** The share of its limit at which a country is told the enemy is within reach, wherever the enemy stands. */
export const REACH_AT = 2 / 3;
/** What a month's fighting may add to the count besides what is named: a warning is given this far short of the line. */
const NEAR = 0.05;

const pct = (x: number): string => `${Math.round(clamp(x, 0, 1) * 100)}%`;

/**
 * What a nation has lost as its country reckons it: the capital (0 to 1), the home country by worth, the army still
 * standing against its greatest strength, and what hunger and a long war add. `score` is collapseScore's own sum.
 */
export interface Standing { capital: number; home: number; army: number; more: number; score: number }

const weigh = (capital: number, home: number, army: number, more: number): number => COLLAPSE.capital * capital + COLLAPSE.home * home + COLLAPSE.army * (1 - army) + more;

export function standing(g: Game, nat: string, ahead = 0): Standing {
  const n = g.nations[nat];
  const capital = n.capital < 0 ? 0 : capitalLost(g, nat, ahead);
  const home = homeLost(g, nat);
  const army = n.armyPeak > 0 ? clamp(armySize(g, nat) / n.armyPeak, 0, 1) : 1;
  const more = exhausted(g, nat) + willLost(g, nat);
  return { capital, home, army, more, score: weigh(capital, home, army, more) };
}

const SHARES: [number, string][] = [[0.9, 'nine tenths of'], [0.75, 'three quarters of'], [2 / 3, 'two thirds of'], [0.5, 'half'], [1 / 3, 'a third of'], [0.25, 'a quarter of'], [0.2, 'a fifth of'], [0.1, 'a tenth of']];

/** The army against its greatest strength, in plain words: 'the army is at a third of its strength'. */
export function armyWords(share: number): string {
  if (share >= 0.95) return 'the army is at full strength';
  if (share < 0.07) return 'the army is all but gone';
  let best = SHARES[0];
  for (const s of SHARES) if (Math.abs(s[0] - share) < Math.abs(best[0] - share)) best = s;
  return `the army is at ${best[1]} its strength`;
}

/**
 * The enemy army nearest the capital: in its province, or in one next to it. Forts march nowhere and do not count.
 * With `invaded`, only an army that stands on the country's own home ground: one massed across a frontier has
 * not come within reach yet (Polish troops in Poznan are beside Berlin's province in September 1939).
 */
function atTheGates(g: Game, nat: string, seat: number, invaded = false): { by: string; from: number } | null {
  let best: { by: string; from: number; w: number } | null = null;
  for (const id of [seat, ...g.regions[seat].nb]) {
    if (invaded && id !== seat && (g.regions[id].owner !== nat || g.regions[id].overseas)) continue;
    const w: Record<string, number> = {};
    for (const u of g.regions[id].units) if (!u.dead && u.kind !== 'fort' && atWar(g, u.nat, nat)) w[u.nat] = (w[u.nat] ?? 0) + u.divs * u.str;
    for (const by of Object.keys(w).sort()) {
      // (An army already in the capital's own province outweighs any beside it.)
      const weight = w[by] + (id === seat ? 1e6 : 0);
      if (!best || weight > best.w) best = { by, from: id, w: weight };
    }
  }
  return best ? { by: best.by, from: best.from } : null;
}

/** The one reading of the player's own country that both interfaces are handed. */
export interface Reading {
  nat: string;
  /** Its capital: the place SHOW ON MAP shows. */
  region: number;
  /** The reckoning, the limit it is weighed against (the country's resilience), and how near: the one over the other. */
  score: number;
  limit: number;
  near: number;
  parts: Standing;
  /** The capital as it stands: free, an enemy army beside its province, besieged, or lost. */
  seat: 'free' | 'reach' | 'besieged' | 'lost';
  /** The enemy army in the capital's province or beside it, and the province it stands in. */
  by: string | null;
  from: number | null;
  /** The same of an army that stands on the country's own home ground: the enemy within reach. */
  reachBy: string | null;
  reachFrom: number | null;
  /** The home provinces in enemy hands, and of how many. */
  lost: number;
  of: number;
  /** The three parts in plain words: 'Berlin is besieged', '15 of 34 home provinces are lost', 'the army is at a third of its strength'. */
  capital: string;
  home: string;
  army: string;
  /**
   * Whether it can give in before he next gives orders, and by what: 'taken' the capital's loss; 'clock' the months
   * of the siege alone, next month; 'more' a little more ground or army; 'reached' an enemy army before a capital
   * that alone keeps it in the war; 'now' it is over the line already and gives in as his month closes.
   */
  canFall: boolean;
  how: 'taken' | 'clock' | 'more' | 'reached' | 'now' | null;
  /** With 'taken': true when the capital's loss alone ends it as the count stands. */
  sure: boolean;
  /** The siege's own clock has carried it over: it gives in as this month closes unless the siege is broken. */
  lastMonth: boolean;
  /** An enemy army stands in the capital's province, or beside it on the country's own home ground; and the reckoning is past two thirds of its limit. */
  within: boolean;
  twoThirds: boolean;
}

/**
 * The reading of a country as its player's month opens; null for nobody, a country at peace or out of the war, and
 * one that stands by its capital alone (a country freed).
 */
export function reading(g: Game, viewer: string | null = g.human): Reading | null {
  if (!viewer) return null;
  const n = g.nations[viewer];
  if (!n?.alive || n.capital < 0 || standsFreed(g, n)) return null;
  if (!n.atWar.some((e) => g.nations[e]?.alive)) return null;
  const seat = g.regions[n.capital];
  const open = g.flags.open === 1;
  const now = standing(g, viewer);
  const limit = n.resilience;
  const held = allied(g, viewer, seat.ctrl);
  const gates = held ? atTheGates(g, viewer, seat.id) : null;
  const invader = held ? atTheGates(g, viewer, seat.id, true) : null;
  const sieged = held && besieged(g, seat);
  const shut = sieged && siegeRules(g) && ringClosed(g, seat);
  const state: Reading['seat'] = !held ? 'lost' : sieged ? 'besieged' : gates ? 'reach' : 'free';

  // The home country: by province for the words, by worth for the reckoning (as the rule weighs it).
  let of = 0, lost = 0, vp = 0, vpLost = 0, vpSieged = 0, vpFront = 0;
  const foeIn = (id: number): boolean => g.regions[id].units.some((u) => !u.dead && u.kind !== 'fort' && atWar(g, u.nat, viewer));
  for (const r of g.regions) {
    if (r.owner !== viewer || r.overseas) continue;
    of++; vp += r.vp;
    if (!allied(g, viewer, r.ctrl)) { lost++; vpLost += r.vp; }
    else if (r.id !== seat.id) {
      // (Under siege today; and in the front line: an enemy army in the province or in one beside it.)
      if (besieged(g, r)) vpSieged += r.vp;
      if (besieged(g, r) || r.nb.some(foeIn)) vpFront += r.vp;
    }
  }
  const share = (x: number): number => (vp ? x / vp : 0);
  const city = seat.name;
  const byAdj = gates && g.nations[gates.by] ? g.nations[gates.by].adj : 'Enemy';
  const capital = state === 'lost' ? `${city} is lost` : state === 'besieged' ? `${city} is besieged` : state === 'reach' ? `${byAdj} troops are a march from ${city}` : `${city} is free`;
  const home = lost === 0 ? 'no home province is lost' : `${lost} of ${of} home provinces ${lost === 1 ? 'is' : 'are'} lost`;

  // What the month to come can do. With the capital go its province and the troops inside it; the home ground under
  // siege today may go too, and a little besides (NEAR). A country that fights on without its capital, or with most
  // of its home ground gone, can lose its whole front line in a month.
  let how: Reading['how'] = null, sure = false;
  const next = shut ? standing(g, viewer, 1) : now;
  if (shut && (now.score >= limit || next.score >= limit)) { how = 'clock'; sure = true; }
  if (!how && gates) {
    const inside = g.corps.reduce((s, c) => (c.nat === viewer && !c.dead && c.reg === seat.id ? s + c.divs * c.str : s), 0);
    const army = n.armyPeak > 0 ? clamp((armySize(g, viewer) - inside) / n.armyPeak, 0, 1) : 1;
    const own = seat.owner === viewer && !seat.overseas ? seat.vp : 0;
    const alone = weigh(1, share(vpLost + own), army, now.more);
    const withMore = weigh(1, share(vpLost + own + vpSieged), army, now.more) + NEAR;
    if (alone >= limit) { how = 'taken'; sure = true; }
    else if (withMore >= limit) how = 'taken';
  }
  if (!how && held && !sieged && open && siegeRules(g) && now.home < SIEGE.stand) {
    // (The grand campaign: a country whose capital is neither taken nor under siege stands until most of its home
    // ground is gone. Over the line already, only that keeps it in the war.)
    if (now.score >= limit && victorOf(g, viewer)) { how = 'reached'; sure = true; }
  } else if (!how) {
    if (!open && now.score >= limit) { how = 'now'; sure = true; }
    else if (weigh(now.capital, share(vpLost + vpFront), now.army, now.more) + NEAR >= limit && (!open || victorOf(g, viewer))) how = 'more';
  }
  return {
    nat: viewer, region: seat.id, score: now.score, limit, near: limit > 0 ? now.score / limit : 0, parts: now,
    seat: state, by: gates?.by ?? null, from: gates?.from ?? null, reachBy: invader?.by ?? null, reachFrom: invader?.from ?? null, lost, of, capital, home, army: armyWords(now.army),
    canFall: how !== null, how, sure, lastMonth: shut && now.score >= limit,
    within: !!invader, twoThirds: limit > 0 && now.score >= REACH_AT * limit,
  };
}

/** What the card says of the country in danger: a band, or in a siege's last month the lead's frame. */
export interface FallWarning {
  title: string;
  /** 'fall': it can give in next month (a band every such month). 'reach': the enemy is within reach (a band, once). 'last': the siege's clock has run out (the LAST MONTH frame, and no band). */
  level: 'fall' | 'reach' | 'last';
  /** The headline, of about eight words: 'Germany can fall next month'. */
  head: string;
  /** The whole warning: what is lost, what ends the campaign, what would stop it. */
  text: string;
  /**
   * The line under the headline on its band, printed in full (ruling 133(a): the band says what is lost and that the
   * campaign ends with it): 'Berlin is besieged, 15 of 34 home provinces are lost and the army is at a third of its
   * strength. If Berlin is taken, Germany gives in and your campaign ends.' What would stop it is `note`.
   */
  sub: string;
  /** What to do, in one short sentence, for the band's small line: 'Break the siege of Berlin, or win back home ground.' */
  note: string;
  /** The country, and its capital for SHOW ON MAP. */
  nat: string;
  region: number;
  reading: Reading;
  /**
   * With 'last', for the frame: its headline ('Berlin is shut in'), one short line under it, the sentence beneath
   * (the ruling's own: 'Germany gives in as October opens unless the siege of Berlin is broken.'), two figures, and
   * the month it gives in as a name ('October').
   */
  frame?: { head: string; sub: string; sentence: string; figures: { n: string; label: string }[]; month: string };
  /**
   * The first alarm of his home ground (homeAlarms, ruling 211(c)), whose third sentence, the help, ends the warning
   * (words, section 2): SHOW ON MAP does what a press on the tray's red row does.
   */
  alarm?: HomeAlarm;
}

/**
 * The warning to the player's own country as it stands when he reads the card, or null. `told` is whether the enemy
 * was within reach when his last month closed (the card's mark keeps it): 'reach' is said once, the month it becomes
 * true, and on a card that opens a campaign.
 */
export function fallWarning(g: Game, viewer: string | null = g.human, told = false): FallWarning | null {
  const r = reading(g, viewer);
  if (!r) return null;
  const n = g.nations[r.nat];
  const seat = g.regions[r.region];
  const name = theName(n, true), mid = theName(n), city = seat.name;
  const parts = `${r.capital[0].toUpperCase()}${r.capital.slice(1)}, ${r.home} and ${r.army}`;
  // The nearest alarm of his home ground: its help ends the warning (ruling 211(c); words, section 2). An enemy army's
  // only: the empty ground of b385 keeps to its own row, as its "take it back" would not say which ground.
  const alarm = homeAlarms(g, r.nat).find((a) => a.case !== 'empty');
  const help = alarm ? alarm.lines[2] : '';
  // (`sub` is the band's second line; `tail` closes the whole text with what would stop it, and the help comes last.)
  const make = (level: FallWarning['level'], head: string, sub: string, tail: string, note: string, more: Partial<FallWarning> = {}): FallWarning =>
    level !== 'last' && help
      ? { title: IN_DANGER, level, head, text: `${head}. ${sub} ${tail} ${help}`, sub: `${sub} ${help}`, note, nat: r.nat, region: r.region, reading: r, alarm, ...more }
      : { title: IN_DANGER, level, head, text: `${head}. ${sub} ${tail}`, sub, note, nat: r.nat, region: r.region, reading: r, ...more };
  const stop = r.seat === 'besieged' ? `Break the siege of ${city}, or win back home ground.` : r.seat === 'reach' ? `Hold ${city}, or win back home ground.`
    : r.seat === 'lost' ? `Take back ${city} or other home ground.` : 'Win back home ground, and keep the army in being.';

  // 3. The siege's own clock has carried it over: his last month.
  if (r.lastMonth) {
    const month = turnToDate(g.turn + 1).label.split(' ')[0];
    const on = investedOn(g, seat);
    const ring = besiegerOf(g, seat);
    const head = `${name} gives in as ${month} opens unless the siege of ${city} is broken`;
    const sub = `Besieged${on > 0 ? ` since ${turnToDate(on).label}` : ''}${ring && g.nations[ring] ? ` by ${g.nations[ring].adj} troops` : ''}`;
    return make('last', head, `${parts}. A government shut in its capital loses its hold on the country month by month, and the siege has now lasted too long. Your campaign ends with it.`, 'Break the siege this month, or win back home ground.', `Break the siege of ${city} this month.`, {
      frame: { head: `${city} is shut in`, sub, sentence: `${head}. Your campaign ends with it.`, figures: [{ n: '1', label: 'month left' }, { n: String(r.lost), label: `of ${r.of} home provinces lost` }], month },
    });
  }

  // 1. It can give in next month.
  if (r.canFall) {
    const ends = r.how === 'taken' ? (r.sure ? `If ${city} is taken, ${mid} gives in and your campaign ends.` : `If ${city} is taken and the month goes badly besides, ${mid} gives in and your campaign ends.`)
      : r.how === 'clock' ? `A government shut in its capital loses its hold on the country month by month: as things stand ${mid} gives in next month, and at once if ${city} is taken. Your campaign ends with it.`
      : r.how === 'reached' ? `That is more than the country will bear, and only a free capital keeps it in the war: ${mid} gives in, and your campaign ends, the month an enemy army stands before ${city}.`
      : r.how === 'now' ? `That is more than the country will bear: as things stand ${mid} gives in when this month closes, and your campaign ends.`
      : `With a little more lost, ${mid} gives in and your campaign ends.`;
    const note = r.how === 'reached' ? `Keep the enemy from ${city}, and win back home ground.` : r.how === 'now' ? 'Win back home ground this month.' : stop;
    return make('fall', `${name} can fall next month`, `${parts}. ${ends}`, note, note);
  }

  // 2. Once, earlier: the enemy within reach.
  if ((r.within || r.twoThirds) && !told) {
    const way = `${name} gives in when the capital, the home provinces and the army lost together pass what the country will bear, and your campaign ends with it: it is ${pct(r.near)} of the way there.`;
    if (r.within) {
      const adj = r.reachBy && g.nations[r.reachBy] ? g.nations[r.reachBy].adj : 'Enemy';
      const where = r.reachFrom === r.region ? `${adj} troops are at the gates of ${city}` : `${adj} troops stand in ${g.regions[r.reachFrom!].name}, beside the capital`;
      const note = r.seat === 'besieged' ? `Break the siege of ${city}, and win back home ground.` : `Hold ${city}, and win back home ground.`;
      // (Where the capital's own words say again that the enemy is a march from it, they go: the same fact twice running.)
      const rest = r.seat === 'reach' ? `${r.home[0].toUpperCase()}${r.home.slice(1)} and ${r.army}` : parts;
      return make('reach', `The enemy is within reach of ${city}`, `${where}. ${rest}. ${way}`, note, note);
    }
    return make('reach', `${name} has lost two thirds of what it will bear`, `${parts}. ${way}`, stop, stop);
  }
  return null;
}

/** Whether the enemy was within reach as the mark was taken: what `fallWarning` is told, so that it says so once. */
export function withinReach(g: Game, viewer: string | null = g.human): boolean {
  const r = reading(g, viewer);
  return !!r && (r.within || r.twoThirds || r.canFall);
}

// ---- The alarm: an enemy army on his home ground (ruling 211(c)) ----------------------------------------------------
//
// From the Playtester's trace of Brian's own American campaign of 17 November 2026: one Canadian corps of guns walked
// through his empty north-east in December 1940 while the tray said nothing, and one order (the army round Vancouver,
// by rail) would have cured it. One reading that both games draw: every enemy army that stands on his home ground,
// what it can reach next month, and the nearest army of his that can come. Words: the UI director's
// words-ruling-211.md, section 1. Nothing is written and no dice are thrown.

/** One alarm: an enemy army on his home ground, in three sentences for the tray's red row. */
export interface HomeAlarm {
  /**
   * 'stands' on his ground; 'taken' a town of his since he last ended a month; round a town of his that it can
   * 'storm' this month, or whose 'stores' it counts; 'empty', his ground still in enemy hands with no troops of
   * theirs in it (the design director's b385), whose row names the cure and stands until the ground is his again.
   */
  case: 'stands' | 'taken' | 'storm' | 'stores' | 'empty';
  /** The province, and the enemy army's flag (the heaviest there; for 'empty', its holder's). */
  region: number;
  by: string;
  /** His own towns it can reach next month, the capital first; empty where it has no corps that fights. */
  reach: number[];
  /** The nearest army of his that can come: where it stands, the months the game counts to bring it there, by rail or by road, and its corps (what a press picks up). Null where none can. */
  help: { region: number; months: number; rail: boolean; corps: number[]; name: string; to: number } | null;
  /** The months a new corps of infantry takes, for where no army can come. */
  raise: number;
  /** The row's three sentences: the enemy and where, what it can reach next month ('' where nothing), the help. */
  lines: [string, string, string];
  /**
   * What the paper says once a press has picked up the help, '' where none can come: "The army at Chicago is in hand.
   * Right-click Pittsburgh to send it.", "Right-click Cleveland to send it on towards Pittsburgh." where the road
   * passes through his own ground first, or "It can march into Buffalo next month." ("storm" where only a garrison holds
   * the town, "attack" where an enemy army stands) where it already stands beside it.
   */
  hand: string;
}

/** The kinds of corps as the row names them. */
const KIND_WORDS: Record<string, string> = { inf: 'infantry', mot: 'motorized infantry', arm: 'tanks', cav: 'cavalry', mtn: 'mountain troops', art: 'guns', eng: 'engineers' };

const monthsWord = (m: number): string => `${m} ${m === 1 ? 'month' : 'months'}`;

/**
 * The enemy army as far as the fog shows, exactly as the map's hover box has it that month: "A Canadian corps of
 * guns" where its kind is known, "A Canadian corps", "2 Canadian corps", "A Canadian army of 3 corps" by its count,
 * "A Canadian army" where not even that is seen. `many` for a plural verb.
 */
function enemyWords(g: Game, region: number, by: string, viewer: string, town: boolean, seen?: Uint8Array): { who: string; many: boolean } {
  const adj = g.nations[by]?.adj ?? 'Enemy';
  const card = armyCard(g, region, by, viewer, town, seen);
  if (!card) return { who: `A ${adj} army`, many: false };
  const n = card.corpsCount;
  if (n === 1) {
    const r = g.regions[region];
    const c = (town ? r.garr ?? [] : r.units).find((u) => !u.dead && armyOf(u) === by);
    const kind = card.known === 'full' && c ? KIND_WORDS[c.kind] : undefined;
    return { who: kind ? `A ${adj} corps of ${kind}` : `A ${adj} corps`, many: false };
  }
  return n === 2 ? { who: `2 ${adj} corps`, many: true } : { who: `A ${adj} army of ${n} corps`, many: false };
}

/** An army of his that might come: his corps in the field of one province that can march (no forts, none inside a town, none on leave). */
interface Helper { region: number; corps: Corps[]; fighters: Corps[]; hops: number }

function helpers(g: Game, nat: string): Helper[] {
  const by = new Map<number, Corps[]>();
  for (const c of g.corps) {
    if (c.dead || armyOf(c) !== nat || c.kind === 'fort' || c.town !== undefined || c.leave !== undefined) continue;
    if (!g.regions[c.reg]?.units.includes(c)) continue;
    const list = by.get(c.reg) ?? [];
    list.push(c);
    by.set(c.reg, list);
  }
  const out: Helper[] = [];
  for (const [region, corps] of by) {
    const fighters = gunsRule(g) ? corps.filter(fights) : corps;
    if (fighters.length) out.push({ region, corps, fighters, hops: 0 });
  }
  return out;
}

/** How the help is named: "the army round Vancouver" on enemy-held ground, a lone corps by its name, else "the army at New York". */
function helperName(g: Game, nat: string, h: Helper): string {
  const r = g.regions[h.region];
  if (!allied(g, nat, r.ctrl)) return `the army round ${townName(r)}`;
  return h.corps.length === 1 ? `${h.corps[0].name} at ${r.name}` : `the army at ${r.name}`;
}

/**
 * The nearest army of his that can come to a province: one that can attack it this month (0 months); one a train
 * takes this month to his ground beside it (1 month by rail); else the march the game plans to his ground beside it,
 * in its own count of months. Ties go to the train, then to the nearer.
 */
/**
 * Each corps' reach this month and its ground for a kept march (farGround), and what the player can see (sightOf), as
 * the war stands: worked out once while the home ground's alarms are read, since nothing in the war moves meanwhile,
 * and only for what is asked about.
 */
interface Kept { reach: (c: Corps) => () => Map<number, Reach>; ground: (c: Corps) => GroundOf; sight: (viewer: string) => Uint8Array }
function keptNow(g: Game, nat?: string, seen?: Uint8Array): Kept {
  const reaches = new Map<Corps, Map<number, Reach>>(), grounds = new Map<Corps, ReturnType<GroundOf>>(), sights = new Map<string, Uint8Array>();
  if (nat && seen) sights.set(nat, seen);
  return {
    sight: (viewer) => { let m = sights.get(viewer); if (!m) { m = sightOf(g, viewer); sights.set(viewer, m); } return m; },
    reach: (c) => () => { let m = reaches.get(c); if (!m) { m = reach(g, c); reaches.set(c, m); } return m; },
    ground: (c) => () => { let m = grounds.get(c); if (!m) { m = farGround(g, c); grounds.set(c, m); } return m; },
  };
}

function nearestHelp(g: Game, nat: string, target: number, all: Helper[], rails: Map<number, Set<number>>, walkIn = false, kept: Kept = keptNow(g)): HomeAlarm['help'] {
  const t = g.regions[target];
  const mine = (id: number): boolean => allied(g, nat, g.regions[id].ctrl) && !g.regions[id].units.some((u) => !u.dead && atWar(g, u.nat, nat));
  const goals = [...(mine(target) ? [target] : []), ...t.nb.filter(mine)];
  if (!all.length) return null;
  // Hops over the land from the province, for the nearest armies by road. (24 since the cities version: the chart of
  // the Americas with its cities takes about twice the steps over the same ground, and the help from across the
  // continent, the army round Vancouver for Pittsburgh in Brian's December 1940, lies 14 steps off.)
  const hops = new Map<number, number>([[target, 0]]);
  const q = [target];
  for (let i = 0; i < q.length; i++) {
    const d = hops.get(q[i])!;
    if (d >= 24) continue;
    for (const j of g.regions[q[i]].nb) if (!hops.has(j)) { hops.set(j, d + 1); q.push(j); }
  }
  for (const h of all) h.hops = hops.get(h.region) ?? 99;
  const near = [...all].sort((a, b) => a.hops - b.hops || a.region - b.region);
  type Best = { h: Helper; months: number; rail: boolean; to: number };
  let best: Best | null = null;
  const better = (h: Helper, months: number, rail: boolean): boolean =>
    !best || months < best.months || (months === best.months && ((rail && !best.rail) || (rail === best.rail && h.hops < best.h.hops)));
  const turnOf = current(g) === nat;
  for (const h of near) {
    // (Help in reach this month is the best there is, and the nearest first: none further off can be better.)
    if (best?.months === 0) break;
    // This month: a fighting corps beside it with a move in hand (or, onto empty ground, the moves to walk in).
    if (turnOf && h.fighters.some((c) => { const e = kept.reach(c)().get(target); return !!e && (e.attack || (walkIn && c.reg !== target)); })) { if (better(h, 0, false)) best = { h, months: 0, rail: false, to: target }; continue; }
    // By rail, this month, to his ground beside it.
    let r = rails.get(h.region);
    if (!r) { r = railReachFor(g, h.corps); rails.set(h.region, r); }
    const by = goals.find((id) => r!.has(id));
    if (by !== undefined && better(h, 1, true)) best = { h, months: 1, rail: true, to: by };
  }
  // By road: the three nearest armies, each to the nearest of his provinces beside it.
  for (const h of near.slice(0, 3)) {
    if (h.hops >= 99 || best?.months === 0) break;
    let months = Infinity, to = -1, steps = Infinity;
    for (const goal of goals) {
      if (h.region === goal) { months = 1; to = goal; break; }
      let worst = 0, road = 0;
      for (const c of h.corps) {
        const p = planMarch(g, c, goal, { ground: kept.ground(c), reach: kept.reach(c) });
        if (!p.ok) { worst = Infinity; break; }
        worst = Math.max(worst, Math.max(1, p.months));
        road = Math.max(road, p.road.length);
      }
      // (The quickest of his provinces beside it, and of two as quick the one with the shorter road.)
      if (worst < months || (worst === months && road < steps)) { months = worst; to = goal; steps = road; }
    }
    if (Number.isFinite(months) && better(h, months, false)) best = { h, months, rail: false, to };
  }
  if (!best) return null;
  const b: Best = best;
  return { region: b.h.region, months: b.months, rail: b.rail, corps: b.h.corps.map((c) => c.id), name: helperName(g, nat, b.h), to: b.to };
}

/**
 * The paper's line once the help is in hand (the Playtester's and the UI director's 84.3 checks). The right-click goes
 * to the town itself where the help can reach it; to the province of his on the way, named as on the way; and where
 * the help already stands beside the town with no moves left, the line says when it can go in. A lone corps is in
 * hand by its name alone: "First Army is in hand", not "First Army at New York".
 */
function handLine(g: Game, help: NonNullable<HomeAlarm['help']>, r: Region, cure: 'empty' | 'storm' | 'attack'): string {
  const held = help.name.startsWith('the army ') ? `The${help.name.slice(3)}` : g.corps.find((c) => c.id === help.corps[0])?.name ?? help.name;
  if (help.to === r.id) return `${held} is in hand. Right-click ${r.name} to ${cure === 'empty' ? 'march in' : 'send it'}.`;
  // (One verb a case, as the hint line has them: the UI director.)
  if (help.to === help.region) return `${held} is in hand. It can ${cure === 'empty' ? 'march into' : cure} ${r.name} next month.`;
  return `${held} is in hand. Right-click ${g.regions[help.to].name} to send it on towards ${r.name}.`;
}

/**
 * The alarms of a nation's home ground (its own provinces, not those overseas): every province of it where an enemy
 * army stands, nearest the capital first; then every province of it still in enemy hands with no troops of theirs in
 * it, nearest the capital first (b385: such a row stands until the ground is his again, and names the cure). Empty
 * for a nation at peace. 'seen' is the nation's sightOf where the caller has it already (the 3D table works it out
 * once a picture); the same answer either way.
 */
export function homeAlarms(g: Game, nat: string | null = g.human, seen?: Uint8Array): HomeAlarm[] {
  if (!nat) return [];
  const n = g.nations[nat];
  if (!n?.alive || !n.atWar.length) return [];
  const raise = specOf(g, 'inf')?.months ?? 5;
  const myIndex = g.order.indexOf(nat);
  const cap = n.capital;
  const dist = g.capDist?.[nat];
  const far = (id: number): number => (dist ? dist[id] : 0);
  const foe = (u: Corps): boolean => !u.dead && u.kind !== 'fort' && atWar(g, u.nat, nat);
  let all: Helper[] | null = null;
  const rails = new Map<number, Set<number>>();
  const kept = keptNow(g, nat, seen);
  const out: HomeAlarm[] = [], empty: Region[] = [];
  for (const r of g.regions) {
    if (r.owner !== nat || r.overseas) continue;
    const held = allied(g, nat, r.ctrl);
    let foes = r.units.filter(foe), inTown = false;
    if (!foes.length && !held) { foes = (r.garr ?? []).filter(foe); inTown = foes.length > 0; }
    if (!foes.length) {
      // (Still in the hands of an enemy, with nobody of theirs in it: its row comes after every enemy army's.)
      if (!held && atWar(g, r.ctrl, nat)) empty.push(r);
      continue;
    }
    // The heaviest army there.
    const w = new Map<string, number>();
    for (const u of foes) w.set(armyOf(u), (w.get(armyOf(u)) ?? 0) + u.divs * u.str);
    const by = [...w.keys()].sort((a, b) => w.get(b)! - w.get(a)! || (a < b ? -1 : 1))[0];
    // Fell since he last ended a month (the design director's ruling b340, by the order of play): this month, before
    // his turn or as it opened; or last month in a turn after his own (Buffalo, stormed in Canada's November after his
    // November had ended). The turn it fell in is kept as it falls; a save from before has the holder's place in the order.
    const fellIn = g.flags[`fellP:${r.id}`] ?? g.order.indexOf(r.ctrl);
    const taken = !held && (r.since === g.turn || (r.since === g.turn - 1 && fellIn > myIndex));
    const ring = held && besieged(g, r);
    const kase: HomeAlarm['case'] = !held ? (taken ? 'taken' : 'stands') : ring ? (whyNotStorm(g, r, by) === null ? 'storm' : 'stores') : 'stands';
    // His towns it can reach next month: his own ground beside it, held by his side; none for guns alone.
    const fighting = foes.some((u) => armyOf(u) === by && (!gunsRule(g) || fights(u)));
    const reachIds = fighting ? r.nb.filter((j) => { const x = g.regions[j]; return x.owner === nat && !x.overseas && allied(g, nat, x.ctrl); }) : [];
    reachIds.sort((a, b) => Number(b === cap) - Number(a === cap) || far(a) - far(b) || a - b);
    if (!all) all = helpers(g, nat);
    const help = nearestHelp(g, nat, r.id, all, rails, false, kept);
    // The words.
    const { who, many } = enemyWords(g, r.id, by, nat, inTown, kept.sight(nat));
    const town = townName(r);
    let s1: string;
    if (kase === 'stands') s1 = `${who} ${many ? 'stand' : 'stands'} at ${r.name}, on your home ground.`;
    else if (kase === 'taken') s1 = `${who} ${many ? 'have' : 'has'} taken ${town}.`;
    else if (kase === 'storm') s1 = `${who} ${many ? 'are' : 'is'} round ${town} and can storm it this month.`;
    else {
      const v = siegeView(g, r.id);
      const left = v?.left ?? 0;
      s1 = `${who} ${many ? 'are' : 'is'} round ${town}, ${v?.bySea ? 'which ships still feed' : v?.byBook ? 'which is fed across the lake' : left > 0 ? `which has stores for ${monthsWord(left)}` : 'whose stores are gone'}.`;
    }
    const named = reachIds.map((id) => (id === cap ? `${g.regions[id].name}, your capital` : g.regions[id].name));
    const s2 = !named.length ? '' : named.length === 1 ? `Next month ${many ? 'they' : 'it'} can reach ${named[0]}.`
      : named.length === 2 ? `Next month ${many ? 'they' : 'it'} can reach ${named[0]} or ${named[1]}.`
      : `Next month ${many ? 'they' : 'it'} can reach ${named[0]}${named[0].includes(',') ? ',' : ''} and ${named.length - 1} more towns of yours.`;
    const s3 = !help ? `No army of yours can come. A new corps takes ${monthsWord(raise)}.`
      : help.months === 0 ? `Nearest help: ${help.name}, in reach this month.`
      : `Nearest help: ${help.name}, ${monthsWord(help.months)} by ${help.rail ? 'rail' : 'road'}.`;
    const hand = help ? handLine(g, help, r, inTown ? 'storm' : 'attack') : '';
    out.push({ case: kase, region: r.id, by, reach: reachIds, help, raise, lines: [s1, s2, s3], hand });
  }
  out.sort((a, b) => far(a.region) - far(b.region) || a.region - b.region);
  // Home ground still in enemy hands with no troops in it (b385; the UI director's words, section 1): two sentences,
  // the first bold, and the cure: "Buffalo, on your home ground, is still in Canadian hands, with no troops in it.
  // First Army at Pittsburgh can march in and take it back this month."
  empty.sort((a, b) => far(a.id) - far(b.id) || a.id - b.id);
  for (const r of empty) {
    if (!all) all = helpers(g, nat);
    const help = nearestHelp(g, nat, r.id, all, rails, true, kept);
    const adj = g.nations[r.ctrl]?.adj ?? 'enemy';
    const s1 = `${r.name}, on your home ground, is still in ${adj} hands, with no troops in it.`;
    const who = help ? `${help.name[0].toUpperCase()}${help.name.slice(1)}` : '';
    const s3 = !help ? `No army of yours can reach it. A new corps takes ${monthsWord(raise)}.`
      : help.months === 0 ? `${who} can march in and take it back this month.`
      : `${who} can march in and take it back in ${monthsWord(help.months)}, by ${help.rail ? 'rail' : 'road'}.`;
    const hand = help ? handLine(g, help, r, 'empty') : '';
    out.push({ case: 'empty', region: r.id, by: r.ctrl, reach: [], help, raise, lines: [s1, '', s3], hand });
  }
  return out;
}

/** One town of an enemy's where his own guns and engineers are left alone round it (ruling 211(a); the tray's row, 84.2.6). */
export interface GunsAlone {
  region: number;
  /** 'guns', 'engineers' or 'guns and engineers'. */
  what: string;
  /** Whether infantry or tanks of his can reach them this month: the row is red only then. */
  red: boolean;
  /** The fighting corps of his that can reach them this month, the nearest army's first (what a press picks up), and where they stand. */
  help: number[];
  from: number;
  line: string;
}

/**
 * The towns of an enemy's round which his own corps are guns and engineers alone: the siege is raised at the
 * holder's next month unless a fighting corps joins them. The row is red while infantry or tanks of his can reach
 * them this month, and plain where none can (the design director's ruling of 17 November 2026; the UI director's words).
 */
export function gunsAlone(g: Game, nat: string | null = g.human): GunsAlone[] {
  if (!nat || !gunsRule(g) || !siegeRules(g)) return [];
  const out: GunsAlone[] = [];
  let fighters: Corps[] | null = null;
  for (const r of g.regions) {
    if (allied(g, nat, r.ctrl) || !besieged(g, r)) continue;
    const round = r.units.filter((c) => !c.dead && c.kind !== 'fort' && allied(g, c.nat, nat));
    if (!round.length || round.some(fights) || !round.some((c) => armyOf(c) === nat)) continue;
    const what = round.every((c) => c.kind === 'eng') ? 'engineers' : round.some((c) => c.kind === 'eng') ? 'guns and engineers' : 'guns';
    if (!fighters) fighters = g.corps.filter((c) => !c.dead && armyOf(c) === nat && fights(c) && !c.hold && c.town === undefined && c.leave === undefined && c.ap > 0);
    const can = current(g) === nat ? fighters.filter((c) => reach(g, c).has(r.id)) : [];
    const red = can.length > 0;
    // (A press picks up the corps of one army: the one that stands nearest, and of two as near the larger.)
    const at = can.length ? [...new Set(can.map((c) => c.reg))].sort((a, b) => Number(!r.nb.includes(a)) - Number(!r.nb.includes(b)) || can.filter((c) => c.reg === b).length - can.filter((c) => c.reg === a).length || a - b)[0] : -1;
    const town = townName(r);
    out.push({ region: r.id, what, red, help: can.filter((c) => c.reg === at).map((c) => c.id), from: at, line: red ? `Only ${what} are left round ${town}: send infantry or tanks this month, or the siege is raised.`
      : `Only ${what} are left round ${town}. No infantry or tanks of yours can reach them this month: the siege will be raised.` });
  }
  return out;
}
