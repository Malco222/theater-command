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
import { COLLAPSE, allied, armySize, atWar, capitalLost, exhausted, homeLost, theName, victorOf } from './game';
import { standsFreed } from './liberation';
import { willLost } from './resources';
import { SIEGE, besieged, besiegerOf, investedOn, ringClosed, siegeRules } from './siege';
import type { Game } from './types';

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
  // (`sub` is the band's second line; `tail` closes the whole text with what would stop it.)
  const make = (level: FallWarning['level'], head: string, sub: string, tail: string, note: string, more: Partial<FallWarning> = {}): FallWarning =>
    ({ title: IN_DANGER, level, head, text: `${head}. ${sub} ${tail}`, sub, note, nat: r.nat, region: r.region, reading: r, ...more });
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
      return make('reach', `The enemy is within reach of ${city}`, `${where}. ${parts}. ${way}`, note, note);
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
