// The history dial (Version 70), the grand campaign only. One setting from 1 (By the book) to 5 (Anything goes) for
// how far the war the computer runs may move from the war that was fought. Stop 3 is the game with nothing added and
// is stored as no setting at all. At stops 1 and 2 the great attacks keep history's calendar while each theatre
// still looks like history, and the book is closed where it no longer fits. At stops 4 and 5 countries with a claim
// on a neighbour's ground press it, with a warning, and may march for it. It never leashes the player.
import { describe, expect, it } from 'vitest';
import historyData from '../data/campaign/history.json';
import openData from '../data/campaign/open.json';
import { bookMonth, claimsMonth, claimsView, dealMonth, historyMonth, moment, readable } from '../src/campaign/claims';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, createGame, removeCorps, setWar } from '../src/campaign/game';
import { FREE, HISTORY, bookDate, bookExpects, bookIf, dialView, leanFor, leash, leagueMul, loosest, mayJoin, moodMul, onBook, setStop, shoreOpen, stopOf, whyNotStop } from '../src/campaign/history';
import { RIVALS, characterOf, decide, startOpen } from '../src/campaign/staff';
import { claimsOf, dialOf, historyStops, orderHistory, runTurnAI, whyNotSetHistory } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const OPS = (openData as unknown as { ops: Record<string, { id: string }[]> }).ops;
const grand = (seed = 3, human: string | null = null, stop = FREE): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== FREE) setStop(g, stop, true);
  return g;
};
const news = (g: Game, re: RegExp): string[] => g.headlines.filter((h) => re.test(h.text)).map((h) => h.text);
const amb = (id: string) => HISTORY.ambitions.find((a) => a.id === id)!;
/** A war in which this claim is alive at stops 4 and 5 (which claims are alive is dealt from the seed). */
const seedWith = (id: string, human: string | null = null): number => {
  for (let s = 1; s < 60; s++) if (readable(grand(s, human, 4), amb(id))) return s;
  throw new Error(`no seed with ${id}`);
};

describe('the dial', () => {
  it('has five stops, and the middle one is the game with nothing added', () => {
    expect(historyStops().map((s) => s.stop)).toEqual([1, 2, 3, 4, 5]);
    expect(FREE).toBe(3);
    const mid = HISTORY.stops[2];
    expect([mid.mood, mid.lean, mid.other, mid.league, mid.join]).toEqual([1, 0, 1, 1, 1]);
    expect(mid.book ?? mid.ambitions ?? mid.deal ?? mid.early).toBeUndefined();
    for (const s of historyStops()) { expect(s.name.length).toBeGreaterThan(3); expect(s.lines.length).toBeGreaterThan(1); }
  });

  it('stores nothing at the middle stop, and remembers the loosest stop a campaign was played at', () => {
    const g = grand(3, 'GER');
    expect(stopOf(g)).toBe(3);
    expect(g.flags.hist).toBeUndefined();
    expect(orderHistory(g, 5)).toBe(true);
    expect([stopOf(g), loosest(g)]).toEqual([5, 5]);
    expect(orderHistory(g, 3)).toBe(true);
    expect(g.flags.hist).toBeUndefined();
    expect([stopOf(g), loosest(g)]).toEqual([3, 5]);
    expect(orderHistory(g, 1)).toBe(true);
    expect([stopOf(g), loosest(g)]).toEqual([1, 5]);
    // A campaign that starts by the book has been played at no other stop.
    const b = grand(3, 'GER', 1);
    expect([stopOf(b), loosest(b)]).toEqual([1, 1]);
    setStop(b, 2);
    expect(loosest(b)).toBe(2);
    setStop(b, 3);
    expect([b.flags.hist, b.flags.histMax]).toEqual([undefined, undefined]);
    const v = dialOf(b)!;
    expect([v.stop, v.name, v.loosest]).toEqual([3, "History's shape", 3]);
  });

  it('belongs to the grand campaign: a scenario is played by its own book, and nobody turns it for a war nobody plays', () => {
    const s = createGame(3, 'GER');
    expect(stopOf(s)).toBe(3);
    expect(whyNotStop(s, 1)).toMatch(/grand campaign/);
    expect(setStop(s, 1)).toBe(false);
    expect(dialView(s)).toBeNull();
    expect(claimsOf(s)).toEqual([]);
    const w = grand(3, null);
    expect(whyNotSetHistory(w, 4)).toMatch(/Nobody/);
    expect(whyNotSetHistory(grand(3, 'UK'), 9)).toMatch(/no such stop/);
    const d = grand(3, 'UK');
    d.flags.dom = 5;
    expect(whyNotSetHistory(d, 4)).toMatch(/last act/);
  });

  it('changes nothing at the middle stop', () => {
    const g = grand(7);
    for (const [nat, list] of Object.entries(OPS)) for (const op of list) expect(leash(g, nat, op.id)).toEqual({ kind: 'free', chance: 1, nerve: 1, early: 0 });
    expect(moodMul(g)).toBe(1);
    expect(leagueMul(g)).toBe(1);
    const w = RIVALS.lean.GER;
    expect(leanFor(g, w)).toBe(w);
    expect(mayJoin(g, 'ESP', 'axis')).toBe(true);
    expect(shoreOpen(g, 'FRA', false)).toBe(true);
    expect(shoreOpen(g, 'UK', false)).toBe(true);
    const before = JSON.stringify(g.flags), told = g.headlines.length;
    g.turn = 30;
    historyMonth(g);
    expect(JSON.stringify(g.flags)).toBe(before);
    expect(g.headlines.length).toBe(told);
    expect(claimsOf(g)).toEqual([]);
  });
});

describe('by the book', () => {
  it('dates the great attacks, lets followers follow, and bars what history never did', () => {
    const g = grand(5, null, 1);
    expect(leash(g, 'GER', 'weser')).toMatchObject({ kind: 'dated', date: 8 });
    expect(leash(g, 'GER', 'west')).toMatchObject({ kind: 'dated', date: 9 });
    expect(leash(g, 'GER', 'east')).toMatchObject({ kind: 'dated', date: 22 });
    expect(leash(g, 'ITA', 'jackal')).toMatchObject({ kind: 'dated', date: 10 });
    expect(leash(g, 'GER', 'west').nerve).toBeLessThan(1);
    expect(leash(g, 'ROM', 'east').kind).toBe('follow');
    expect(leash(g, 'SOV', 'west').kind).toBe('never');
    expect(leash(g, 'SOV', 'vulture').kind).toBe('never');
    // Every dated or theatred choice is a real one.
    for (const key of [...Object.keys(HISTORY.theatre), ...HISTORY.unhistoric]) { const [nat, id] = key.split(':'); expect(OPS[nat]?.some((o) => o.id === id), key).toBe(true); }
    for (const [nat, m] of Object.entries(HISTORY.dates)) for (const id of Object.keys(m)) expect(HISTORY.theatre[`${nat}:${id}`], `${nat}:${id}`).toBeTruthy();
    for (const m of HISTORY.marks) expect(HISTORY.theatres[m.theatre], m.theatre).toBeTruthy();
  });

  it('close to history gives each date a month either way, the same in one war and not the same in every war', () => {
    const seen = new Set<number>();
    for (let s = 1; s <= 30; s++) {
      const g = grand(s, null, 2);
      const d = bookDate(g, 'GER', 'east')!;
      expect(Math.abs(d - 22)).toBeLessThanOrEqual(1);
      expect(bookDate(g, 'GER', 'east')).toBe(d);
      seen.add(d);
    }
    expect(seen.size).toBe(3);
    const g = grand(5, null, 2);
    expect(leash(g, 'SOV', 'west')).toMatchObject({ kind: 'free', chance: 0.3 });
  });

  it('gives each staff the character its real counterpart had, and deals them evenly where anything goes', () => {
    for (let s = 1; s <= 12; s++) {
      const g = grand(s, null, 1);
      expect([characterOf(g, 'GER'), characterOf(g, 'FRA'), characterOf(g, 'ITA'), characterOf(g, 'UK')]).toEqual(['bold', 'cautious', 'opportunist', 'cautious']);
    }
    expect(leanFor(grand(1, null, 5), RIVALS.lean.GER)).toEqual([1, 1, 1]);
    const loose = leanFor(grand(1, null, 4), RIVALS.lean.GER);
    expect(loose[0]).toBeGreaterThan(loose[1]);
    expect(loose[0] / loose[1]).toBeLessThan(RIVALS.lean.GER[0] / RIVALS.lean.GER[1]);
    expect(moodMul(grand(1, null, 1))).toBe(0);
    expect(moodMul(grand(1, null, 5))).toBeGreaterThan(2);
  });

  it('plays the opening of the war on history\'s dates: Norway in April 1940, the west in May, the east in June 1941', () => {
    const g = grand(11, null, 1);
    const first: Record<string, number> = {};
    while (g.turn < 23) {
      const t = g.turn;
      runTurnAI(g);
      for (const [k, a, b] of [['nor', 'GER', 'NOR'], ['west', 'GER', 'BEL'], ['east', 'GER', 'SOV'], ['gre', 'ITA', 'GRE']] as const) if (!first[k] && atWar(g, a, b)) first[k] = t;
    }
    expect(first).toMatchObject({ nor: 8, west: 9, gre: 14, east: 22 });
    expect(onBook(g, 'east')).toBe(true);
    expect(news(g, /History's book is closed/)).toEqual([]);
  }, 60000);

  it('keeps a second front off each shore until history\'s first landing there', () => {
    const g = grand(4, null, 1);
    g.turn = 38;
    expect(shoreOpen(g, 'FRA', true)).toBe(false);
    g.turn = 39;
    expect(shoreOpen(g, 'FRA', true)).toBe(true);
    expect(shoreOpen(g, 'ITA', false)).toBe(false);
    g.turn = 47;
    expect(shoreOpen(g, 'ITA', false)).toBe(true);
    expect(shoreOpen(g, 'FRA', false)).toBe(false);
    g.turn = 58;
    expect(shoreOpen(g, 'FRA', false)).toBe(true);
    expect(shoreOpen(g, 'UK', false)).toBe(false);
    // Once the book is closed in the east the second front is anybody's.
    g.flags['hist:off:second'] = 40;
    expect(shoreOpen(g, 'UK', false)).toBe(true);
  });

  it('lets only the countries that really joined an alliance join it of their own accord', () => {
    const ready = (g: Game, id: string): void => { const n = g.nations[id]; n.align = 'leaning_axis'; n.opinion = n.home = 90; delete g.flags[`step:${id}`]; };
    const book = grand(6, 'UK', 1);
    ready(book, 'ESP'); ready(book, 'HUN');
    monthlyDiplomacy(book);
    expect(book.nations.ESP.align).toBe('leaning_axis');
    expect(book.nations.HUN.align).toBe('axis');
    // A neutral the player has courted is his affair.
    const mine = grand(6, 'GER', 1);
    ready(mine, 'ESP');
    mine.flags['courted:ESP'] = 1;
    monthlyDiplomacy(mine);
    expect(mine.nations.ESP.align).toBe('axis');
    const free = grand(6, 'UK');
    ready(free, 'ESP');
    monthlyDiplomacy(free);
    expect(free.nations.ESP.align).toBe('axis');
  });
});

describe('the book yields when it no longer fits', () => {
  it('is closed in the east, and for the second front, when Germany and the Soviet Union are at war before their time', () => {
    const g = grand(9, 'SOV', 1);
    g.turn = 7;
    expect(bookMonth(g)).toEqual([]);
    setWar(g, 'SOV', 'GER');
    const failed = bookMonth(g);
    expect(failed.map((m) => m.theatre)).toEqual(['east']);
    expect([onBook(g, 'east'), onBook(g, 'second'), onBook(g, 'west')]).toEqual([false, false, true]);
    const told = news(g, /History's book is closed/);
    expect(told.length).toBe(1);
    expect(told[0]).toMatch(/closed in the east: Germany and the Soviet Union are at war before the summer of 1941\. With it the book is closed in the second front\. From here on the staffs there decide for themselves\./);
    // From then on the staffs there decide as at stop 3.
    expect(leash(g, 'GER', 'east')).toEqual({ kind: 'free', chance: 1, nerve: 1, early: 0 });
    expect(leash(g, 'SOV', 'vulture').kind).toBe('free');
    expect(leash(g, 'GER', 'west').kind).toBe('dated');
    expect(dialOf(g)!.closed.map((c) => c.theatre).sort()).toEqual(['east', 'second']);
    // It is said once.
    bookMonth(g);
    expect(news(g, /History's book is closed/).length).toBe(1);
  });

  it('is closed in the west, and everywhere that follows from it, when a German player lets May 1940 go by', () => {
    const g = grand(9, 'GER', 1);
    g.turn = 10;
    expect(bookMonth(g).length).toBe(0);
    g.turn = 11;
    const failed = bookMonth(g);
    expect(failed.some((m) => m.theatre === 'west')).toBe(true);
    for (const t of ['west', 'med', 'balkans']) expect(onBook(g, t), t).toBe(false);
    // The east's book hangs on France's fall by the east's own month (b530): it waits, and with it the second front.
    for (const t of ['east', 'second']) expect(onBook(g, t), t).toBe(true);
    expect(news(g, /closed in the west: Germany has not struck in the west/).length).toBe(1);
    expect(news(g, /the east/)).toEqual([]);
    expect(leash(g, 'ITA', 'jackal').kind).toBe('free');
  });

  it("keeps the east's book when the west's closes, if France is out of the war by May 1941 (b530)", () => {
    const g = grand(9, 'UK', 1);
    g.turn = 11;
    bookMonth(g);
    expect(onBook(g, 'west')).toBe(false);
    expect(g.flags['hist:wait:east']).toBe(11);
    // France falls in the winter: the east's book stands, and Barbarossa keeps its month.
    g.turn = 17;
    g.nations.FRA.alive = false;
    for (const n of Object.values(g.nations)) n.atWar = n.atWar.filter((x) => x !== 'FRA');
    g.nations.FRA.atWar = [];
    bookMonth(g);
    expect(g.flags['hist:wait:east']).toBeUndefined();
    expect(g.flags['hist:kept:east']).toBe(17);
    g.turn = 22;
    bookMonth(g);
    expect([onBook(g, 'east'), onBook(g, 'second')]).toEqual([true, true]);
    expect(leash(g, 'GER', 'east').kind).toBe('dated');
    expect(news(g, /closed in the east/)).toEqual([]);
  });

  it('closes the east, and the second front with it, when France still stands at the end of May 1941 (b530)', () => {
    for (const stop of [1, 2]) {
      const g = grand(9, 'UK', stop);
      g.turn = 12;
      bookMonth(g);
      expect(onBook(g, 'west'), `stop ${stop}`).toBe(false);
      g.turn = 21;
      bookMonth(g);
      expect(onBook(g, 'east'), `stop ${stop}`).toBe(true);
      expect(bookExpects(g).some((x) => x.text === 'The book is closed in the east if France is still in the war when May 1941 is over.'), `stop ${stop}`).toBe(true);
      g.turn = 22;
      bookMonth(g);
      expect([onBook(g, 'east'), onBook(g, 'second')], `stop ${stop}`).toEqual([false, false]);
      const told = news(g, /closed in the east/);
      expect(told, `stop ${stop}`).toEqual(["History's book is closed in the east: France was still in the war when May 1941 ended. With it the book is closed in the second front. From here on the staffs there decide for themselves."]);
      bookMonth(g);
      expect(news(g, /closed in the east/).length).toBe(1);
    }
  });

  it("closes the east at once when the west's book closes after May 1941 (b530)", () => {
    const g = grand(9, 'UK', 1);
    g.turn = 22;
    bookMonth(g);
    expect(onBook(g, 'west')).toBe(false);
    expect([onBook(g, 'east'), onBook(g, 'second')]).toEqual([false, false]);
    expect(g.flags['hist:wait:east']).toBeUndefined();
  });
});

describe('neutrals with ambitions', () => {
  it('names real countries and real ground', () => {
    const g = grand(1);
    for (const a of HISTORY.ambitions) {
      expect(g.nations[a.nat], a.id).toBeTruthy();
      expect(g.nations[a.on], a.id).toBeTruthy();
      for (const nm of a.ground) expect(g.byName[nm], `${a.id}: ${nm}`).toBeTruthy();
      expect(a.why.length).toBeGreaterThan(10);
    }
    expect(new Set(HISTORY.ambitions.map((a) => a.id)).size).toBe(HISTORY.ambitions.length);
    // Brian's example: countries that were never in the war with a claim on Italian ground.
    expect(HISTORY.ambitions.filter((a) => a.on === 'ITA').map((a) => a.nat).sort()).toEqual(['GRE', 'TUR', 'YUG']);
  });

  it('can be read beforehand, are pressed with a warning when the holder is weakened, and are marched for with a reason', () => {
    const seed = seedWith('epirus');
    const a = amb('epirus');
    const g = grand(seed, null, 4);
    expect(claimsOf(g).find((c) => c.id === 'epirus')).toMatchObject({ nat: 'GRE', on: 'ITA', state: 'claims', onYou: false });
    // Italy at peace is nobody's opportunity.
    expect(moment(g, a)).toBeNull();
    claimsMonth(g);
    expect(news(g, /presses its claim/)).toEqual([]);
    // Italy at war with Britain and its army gone from Albania: the moment has come.
    setWar(g, 'ITA', 'UK');
    for (const c of g.corps.filter((c) => c.nat === 'ITA' && g.regions[c.reg].name === 'Albania')) removeCorps(g, c);
    expect(moment(g, a)).toMatch(/Italy/);
    claimsMonth(g);
    const warned = news(g, /Greece presses its claim to Northern Epirus/);
    expect(warned.length).toBe(1);
    expect(warned[0]).toMatch(/is warned that it may march/);
    expect(claimsOf(g).find((c) => c.id === 'epirus')).toMatchObject({ state: 'pressing', since: g.turn });
    expect(atWar(g, 'GRE', 'ITA')).toBe(false);
    // Not in the month of the warning, nor the next.
    g.turn++;
    claimsMonth(g);
    expect(atWar(g, 'GRE', 'ITA')).toBe(false);
    for (let k = 0; k < 24 && !atWar(g, 'GRE', 'ITA'); k++) { g.turn++; claimsMonth(g); }
    expect(atWar(g, 'GRE', 'ITA')).toBe(true);
    expect(news(g, /Greece marches for Northern Epirus: .+\. Greek-speaking country/).length).toBe(1);
    // It fights beside Italy's enemies, and its staff has an offensive on.
    expect(g.nations.GRE.align).toBe('allies_cobelligerent');
    expect(g.flags['st:on:GRE:ITA']).toBe(g.turn);
    expect(claimsOf(g).find((c) => c.id === 'epirus')).toMatchObject({ state: 'marched', since: g.turn });
    expect(news(g, /presses its claim to Northern Epirus/).length).toBe(1);
  });

  it('are never pressed for the player, and a claim on the player\'s ground is listed first', () => {
    const seed = seedWith('epirus');
    const mine = grand(seed, 'GRE', 4);
    expect(readable(mine, amb('epirus'))).toBe(false);
    expect(claimsOf(mine).some((c) => c.id === 'epirus')).toBe(false);
    const onMe = grand(seed, 'ITA', 4);
    const list = claimsOf(onMe);
    expect(list[0].onYou).toBe(true);
    expect(list.filter((c) => c.onYou).every((c) => c.on === 'ITA')).toBe(true);
  });

  it('wait while a great power allied to the holder stands on the claimant\'s border, except where anything goes', () => {
    const seed = seedWith('julian');
    // Italy at war with Britain, and not an Italian soldier left at home.
    const strip = (g: Game): void => { setWar(g, 'ITA', 'UK'); for (const c of g.corps.filter((c) => c.nat === 'ITA' && !g.regions[c.reg].overseas)) removeCorps(g, c); };
    const loose = grand(seed, null, 4);
    strip(loose);
    expect(moment(loose, amb('julian'))).toBeNull();
    const any = grand(seed, null, 5);
    strip(any);
    any.turn = 40;
    expect(readable(any, amb('julian'))).toBe(true);
    expect(moment(any, amb('julian'))).toBeTruthy();
  });

  it('keep a great power out of the great war: the Soviet Union does not march on a country that has taken a side', () => {
    const a = amb('kars');
    let g: Game | null = null;
    for (let s = 1; s < 60 && !g; s++) { const x = grand(s, null, 5); x.turn = 40; if (readable(x, a) && moment(x, a)) g = x; }
    expect(g).not.toBeNull();
    g!.nations.TUR.align = 'allies_cobelligerent';
    expect(moment(g!, a)).toBeNull();
  });

  it('end a war between two countries outside both alliances with a treaty once the claimant holds what it claimed', () => {
    const g = grand(2, null, 5);
    const a = amb('transylvania');
    g.nations.HUN.align = 'independent';
    setWar(g, 'HUN', 'ROM');
    g.flags[`amb:go:${a.id}`] = g.turn;
    g.flags['hist:live'] = 1;
    claimsMonth(g);
    expect(atWar(g, 'HUN', 'ROM')).toBe(true);
    g.byName.Transylvania.ctrl = 'HUN';
    g.turn++;
    claimsMonth(g);
    expect(atWar(g, 'HUN', 'ROM')).toBe(false);
    expect(g.byName.Transylvania.owner).toBe('HUN');
    expect(news(g, /Hungary and Romania sign a treaty: Transylvania is Hungarian, and the two countries are at peace\./).length).toBe(1);
    expect(claimsOf(g).some((c) => c.id === a.id)).toBe(false);
    expect(g.flags['hist:live']).toBeUndefined();
  });

  it('do not exist below stop 4', () => {
    for (const stop of [1, 2, 3]) {
      const g = grand(seedWith('epirus'), null, stop);
      setWar(g, 'ITA', 'UK');
      for (const c of g.corps.filter((c) => c.nat === 'ITA' && g.regions[c.reg].name === 'Albania')) removeCorps(g, c);
      g.turn = 20;
      historyMonth(g);
      expect(claimsOf(g)).toEqual([]);
      expect(news(g, /presses its claim/)).toEqual([]);
    }
  });
});

describe('anything goes', () => {
  it('deals every neutral\'s leaning afresh, once, and says so', () => {
    const g = grand(8, null, 5);
    const before = Object.fromEntries(Object.values(g.nations).map((n) => [n.id, n.home]));
    const turned = dealMonth(g);
    const moved = Object.values(g.nations).filter((n) => n.home !== before[n.id]).map((n) => n.id);
    expect(moved.length).toBeGreaterThan(8);
    for (const id of ['GER', 'ITA', 'UK', 'FRA', 'SOV', 'POL']) expect(moved).not.toContain(id);
    for (const n of Object.values(g.nations)) expect(Math.abs(n.home)).toBeLessThanOrEqual(100);
    expect(news(g, /Nobody has read the history books this time/).length).toBe(1);
    expect(turned.length).toBeGreaterThan(0);
    const after = Object.fromEntries(Object.values(g.nations).map((n) => [n.id, n.home]));
    expect(dealMonth(g)).toEqual([]);
    expect(Object.fromEntries(Object.values(g.nations).map((n) => [n.id, n.home]))).toEqual(after);
    // Not the same deal in every war.
    const other = grand(9, null, 5);
    dealMonth(other);
    expect(Object.values(other.nations).map((n) => n.home)).not.toEqual(Object.values(g.nations).map((n) => n.home));
  });

  it('lets the great attacks come earlier and weighs up what history never did', () => {
    const g = grand(8, null, 5);
    expect(leash(g, 'GER', 'east')).toEqual({ kind: 'free', chance: 1, nerve: 1, early: 6 });
    expect(leash(g, 'SOV', 'west').chance).toBe(2);
    expect(leash(grand(8, null, 4), 'SOV', 'west').chance).toBe(1.5);
    expect((historyData as { stops: { stop: number }[] }).stops.length).toBe(5);
  });
});

// Three mends of Version 72, found by the Playtester on the dial as it shipped.
describe('the book, mended', () => {
  it('has Germany fight the Balkans beside the army it is bringing up for the east, each in its own month', () => {
    // A war at the second stop in which both are dated May 1941: the army for the east starts coming up in February,
    // and the Balkan campaign used to be shut out by it and the book closed there in the summer. (Seed 26 until ruling
    // 229, seed 22 until ruling 139 was held back (b509): in that war the book now closes before the spring. Of the six
    // wars of seeds 1 to 60 with both dates in May 1941, the other five tell this story; seed 26 is the first.)
    const seed = 26;
    const g = grand(seed, null, 2);
    const balkans = bookDate(g, 'GER', 'balkans')!, east = bookDate(g, 'GER', 'east')!;
    expect([balkans, east]).toEqual([21, 21]);
    let yug = 0, sov = 0, prepared = 0;
    while (g.turn <= east) {
      const t = g.turn;
      runTurnAI(g);
      if (!yug && (atWar(g, 'GER', 'YUG') || !g.nations.YUG.alive)) yug = t;
      if (!sov && atWar(g, 'GER', 'SOV')) sov = t;
      if (!prepared && OPS.GER[(g.flags['st:op:GER'] ?? 0) - 1]?.id === 'east') prepared = t;
    }
    // The east was being prepared when the Balkans were struck, the month before the greater blow, and it still fell on its date.
    expect(prepared).toBeGreaterThan(0);
    expect(prepared).toBeLessThan(yug);
    expect(yug).toBe(east - 1);
    expect(sov).toBe(east);
    expect([onBook(g, 'balkans'), onBook(g, 'east')]).toEqual([true, true]);
    expect(news(g, /History's book is closed/)).toEqual([]);
  }, 120000);

  it('gives a dated choice the book\'s own conditions: Italy comes in in her month though France has fallen already', () => {
    expect(bookIf('ITA', 'jackal')).toEqual(['war:GER,UK', 'collapsing:FRA,0.07']);
    expect(bookIf('GER', 'balkans')).toContain('war:ITA,GRE|fallen:GRE');
    expect(bookIf('GER', 'west')).toBeNull();
    const fallen = (stop: number): Game => {
      const g = grand(8, 'GER', stop);
      g.turn = 10;
      const F = g.nations.FRA;
      for (const c of g.corps.filter((c) => c.nat === 'FRA')) removeCorps(g, c);
      F.alive = false;
      F.fellOn = { turn: 9, week: 4, to: 'GER', how: 'beaten' };
      for (const n of Object.values(g.nations)) n.atWar = n.atWar.filter((e) => e !== 'FRA');
      F.atWar = [];
      return g;
    };
    const g = fallen(1);
    expect(leash(g, 'ITA', 'jackal')).toMatchObject({ kind: 'dated', date: 10 });
    expect(atWar(g, 'ITA', 'UK')).toBe(false);
    decide(g, 'ITA');
    expect(g.flags['st:go:ITA']).toBe(10);
    expect(OPS.ITA[(g.flags['st:goOp:ITA'] ?? 0) - 1].id).toBe('jackal');
    // Not before her month.
    const early = fallen(1);
    early.turn = 9;
    decide(early, 'ITA');
    expect(early.flags['st:go:ITA']).toBeUndefined();
    // Off the book the choice keeps its staff's own conditions, and a France already beaten is no reason for it.
    const free = fallen(3);
    decide(free, 'ITA');
    expect(free.flags['st:done:ITA:jackal']).toBeUndefined();
  });

  it('says beforehand what the book expects, soonest first, and drops what has passed or been closed', () => {
    const g = grand(9, 'GER', 1);
    const all = bookExpects(g);
    expect(all.length).toBe(HISTORY.marks.length);
    expect(all.map((x) => x.turn)).toEqual([...all.map((x) => x.turn)].sort((a, b) => a - b));
    expect(dialOf(g)!.expects).toEqual(all);
    expect(all.some((x) => x.text === 'The book is closed in the west if France still stands when December 1940 is over.')).toBe(true);
    expect(all.some((x) => x.text === 'The book is closed in the north if Norway is at war before its time: that is, before April 1940.')).toBe(true);
    for (const x of all) expect(x.text).not.toMatch(/undefined|NaN/);
    // A mark whose month has gone by is no longer expected, and nothing is expected where the book is closed.
    g.turn = 9;
    expect(bookExpects(g).some((x) => /Norway is at war before its time/.test(x.text))).toBe(false);
    g.flags['hist:off:west'] = 9;
    expect(bookExpects(g).some((x) => x.theatre === 'west')).toBe(false);
    // The second stop gives each date its month of grace, and the stops without a book expect nothing.
    const close = grand(9, 'GER', 2);
    expect(bookExpects(close).some((x) => x.text === 'The book is closed in the west if France still stands when January 1941 is over.')).toBe(true);
    expect(bookExpects(grand(9, 'GER', 3))).toEqual([]);
    expect(dialOf(grand(9, 'GER', 4))!.expects).toEqual([]);
  });

  it('tells a player on a card of its own when the book is closed and when a claim is pressed on his ground', () => {
    // The book closing: a warning addressed to the player, where a war nobody plays gets a line of news.
    const g = grand(9, 'SOV', 1);
    g.turn = 7;
    setWar(g, 'SOV', 'GER');
    bookMonth(g);
    const closed = g.headlines.filter((h) => /History's book is closed/.test(h.text));
    expect(closed.length).toBe(1);
    expect(closed[0]).toMatchObject({ kind: 'event', warn: 'History\'s book', nat: 'SOV' });
    const nobody = grand(9, null, 1);
    nobody.turn = 7;
    setWar(nobody, 'SOV', 'GER');
    bookMonth(nobody);
    expect(nobody.headlines.filter((h) => /History's book is closed/.test(h.text)).map((h) => h.warn)).toEqual([undefined]);
    // A claim on the player's own ground: a warning to him, with the ground to look at.
    const seed = seedWith('epirus', 'ITA');
    const mine = grand(seed, 'ITA', 4);
    setWar(mine, 'ITA', 'UK');
    for (const c of mine.corps.filter((c) => c.nat === 'ITA' && mine.regions[c.reg].name === 'Albania')) removeCorps(mine, c);
    claimsMonth(mine);
    const pressed = mine.headlines.filter((h) => /Greece presses its claim to Northern Epirus/.test(h.text));
    expect(pressed.length).toBe(1);
    expect(pressed[0]).toMatchObject({ kind: 'event', warn: 'A claim on your ground', nat: 'ITA', region: mine.byName.Albania.id });
    expect(pressed[0].text).toMatch(/It will not march for \d+ months at least, and what it weighs is the troops you keep there\./);
    // When it marches the declaration is a card as every war is, and it points at the ground.
    for (let k = 0; k < 24 && !atWar(mine, 'GRE', 'ITA'); k++) { mine.turn++; claimsMonth(mine); }
    const marched = mine.headlines.filter((h) => /Greece marches for Northern Epirus/.test(h.text));
    expect(marched.length).toBe(1);
    expect(marched[0]).toMatchObject({ kind: 'war', region: mine.byName.Albania.id });
    // The same claim on a computer's Italy stays a line of news about the claimant.
    const theirs = grand(seedWith('epirus'), null, 4);
    setWar(theirs, 'ITA', 'UK');
    for (const c of theirs.corps.filter((c) => c.nat === 'ITA' && theirs.regions[c.reg].name === 'Albania')) removeCorps(theirs, c);
    claimsMonth(theirs);
    expect(theirs.headlines.filter((h) => /presses its claim to Northern Epirus/.test(h.text)).map((h) => [h.warn, h.nat])).toEqual([[undefined, 'GRE']]);
  });
});
