// The country in danger (the design director's rulings 117(d) and 133(a)): no campaign ends on the player without a
// red band at least a month before, saying what ends it and what would stop it.
// The Playtester's find on Version 82.3: a player's Germany fell with no band on the card before.
// From the rule that makes a country give in (collapseScore), one reading of his own country as his month opens:
// how near its limit, its three parts in plain words, whether it can give in next month. Four things are said:
// 1. a red band every month it can give in next month: "Germany can fall next month";
// 2. once, earlier, when an enemy army first stands on his home ground beside the capital's province, or the
//    reckoning passes two thirds of its limit: "The enemy is within reach of Berlin";
// 3. where the siege's own clock has carried it over: the LAST MONTH frame, "Germany gives in as October opens
//    unless the siege of Berlin is broken.";
// 4. a besieged capital's lead adds "It is your capital: the country will not long outlast it."
// Display only: the rule is not changed, nothing is written and no dice are thrown.
import { describe, expect, it } from 'vitest';
import { armyWords, CAPITAL_BESIEGED, fallWarning, homeAlarms, IN_DANGER, REACH_AT, reading, standing, withinReach } from '../src/campaign/danger';
import { addCorps, armySize, atWar, collapseScore, createGame, homeLost, setWar } from '../src/campaign/game';
import { markFor, monthCard } from '../src/campaign/monthcard';
import { saveGame } from '../src/campaign/save';
import { besieged, garrisonOf, joinTown, ringClosed } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { BattleReport, Corps, Game, Region } from '../src/campaign/types';

const AUG_44 = 60;
const ME = 'GER';
/** Germany's seat in the grand campaign, at war with the Soviet Union, as August 1944 opens. */
const game = (me: string | null = ME, seed = 7): Game => { const g = startOpen(createGame(seed, me)); if (!atWar(g, 'GER', 'SOV')) setWar(g, 'GER', 'SOV'); g.turn = AUG_44; return g; };
const seatOf = (g: Game): Region => g.regions[g.nations[ME].capital];
/** Home provinces pass into Soviet hands until that share of the home country, by worth, is lost. Berlin is kept, and its neighbours unless `near`. */
const lose = (g: Game, share: number, near = false): void => {
  const seat = seatOf(g);
  for (const r of g.regions) {
    if (homeLost(g, ME) >= share) break;
    if (r.owner !== ME || r.overseas || r.id === seat.id || (!near && seat.nb.includes(r.id))) continue;
    r.ctrl = 'SOV';
  }
};
/** The army stands at this share of its greatest strength. */
const weaken = (g: Game, share: number): void => { g.nations[ME].armyPeak = armySize(g, ME) / share; };
/** Soviet corps put by hand into a province. */
const reds = (g: Game, to: Region, n = 1, divs = 10): Corps[] => Array.from({ length: n }, () => {
  const c = addCorps(g, 'SOV', 'Test Army', 'inf', divs, g.nations.SOV.capital, {});
  const from = g.regions[c.reg];
  from.units.splice(from.units.indexOf(c), 1);
  c.reg = to.id;
  to.units.push(c);
  return c;
});
/** A German home province beside Berlin's. */
const beside = (g: Game): Region => g.regions[seatOf(g).nb.find((j) => g.regions[j].owner === ME && !g.regions[j].overseas)!];
/** Berlin shut in: nobody of his in the field, one corps in the town, and a Soviet army round it strong enough to close the ring. */
const shutIn = (g: Game): void => {
  const seat = seatOf(g);
  for (const c of [...seat.units, ...garrisonOf(seat)]) c.dead = true;
  seat.units = []; seat.garr = [];
  const c = addCorps(g, ME, 'Garrison', 'inf', 1, seat.id, {});
  seat.units.splice(seat.units.indexOf(c), 1);
  joinTown(g, seat, c);
  reds(g, seat, 8, 12);
  expect(besieged(g, seat)).toBe(true);
  expect(ringClosed(g, seat)).toBe(true);
};
const bandOf = (g: Game, mark = markFor(g, ME)) => monthCard(g, mark, ME).bands.find((b) => b.title === IN_DANGER);

describe('the reading of his own country', () => {
  it('is the rule\'s own sum, in three parts', () => {
    const g = game();
    lose(g, 0.4);
    weaken(g, 0.5);
    const s = standing(g, ME);
    expect(s.score).toBeCloseTo(collapseScore(g, ME), 10);
    expect(s).toMatchObject({ capital: 0, army: 0.5 });
    expect(s.home).toBeCloseTo(homeLost(g, ME), 10);
    // And with the capital lost, and under a siege months old.
    seatOf(g).ctrl = 'SOV';
    expect(standing(g, ME).score).toBeCloseTo(collapseScore(g, ME), 10);
    expect(standing(g, ME).capital).toBe(1);
    const k = game();
    lose(k, 0.4);
    shutIn(k);
    k.flags[`siege:${seatOf(k).id}`] = AUG_44 - 5;
    expect(standing(k, ME).score).toBeCloseTo(collapseScore(k, ME), 10);
    expect(standing(k, ME).capital).toBeGreaterThan(0.5);
    // A month ahead the siege is a month older and nothing else has moved.
    expect(standing(k, ME, 1).capital).toBeGreaterThan(standing(k, ME).capital);
    expect(standing(k, ME, 1).home).toBe(standing(k, ME).home);
  });

  it('says the three parts in plain words', () => {
    const g = game();
    lose(g, 0.3);
    weaken(g, 1 / 3);
    const r = reading(g, ME)!;
    expect(r.capital).toBe('Berlin is free');
    expect(r.home).toBe(`${r.lost} of ${r.of} home provinces are lost`);
    expect(r.lost).toBe(g.regions.filter((x) => x.owner === ME && !x.overseas && x.ctrl !== ME).length);
    expect(r.army).toBe('the army is at a third of its strength');
    expect(r.near).toBeCloseTo(collapseScore(g, ME) / g.nations[ME].resilience, 10);
    expect([1, 0.92, 0.74, 0.66, 0.5, 0.3, 0.26, 0.2, 0.11, 0.02].map(armyWords)).toEqual([
      'the army is at full strength', 'the army is at nine tenths of its strength', 'the army is at three quarters of its strength', 'the army is at two thirds of its strength',
      'the army is at half its strength', 'the army is at a third of its strength', 'the army is at a quarter of its strength', 'the army is at a fifth of its strength',
      'the army is at a tenth of its strength', 'the army is all but gone']);
    reds(g, beside(g));
    expect(reading(g, ME)!.capital).toBe('Soviet troops are a march from Berlin');
    reds(g, seatOf(g));
    expect(reading(g, ME)!.capital).toBe('Berlin is besieged');
    seatOf(g).ctrl = 'SOV';
    expect(reading(g, ME)!.capital).toBe('Berlin is lost');
    // One province lost is said in the singular; none is said so.
    const one = game();
    one.regions.find((x) => x.owner === ME && !x.overseas && x.id !== seatOf(one).id)!.ctrl = 'SOV';
    expect(reading(one, ME)!.home).toBe(`1 of ${reading(one, ME)!.of} home provinces is lost`);
    expect(reading(game(), ME)!.home).toBe('no home province is lost');
  });

  it('is his alone, and of a country at war: nobody\'s in a war nobody plays', () => {
    const g = game(null);
    expect(reading(g)).toBeNull();
    expect(fallWarning(g)).toBeNull();
    expect(withinReach(g)).toBe(false);
    const peace = startOpen(createGame(7, 'SUI'));
    expect(reading(peace)).toBeNull();
  });
});

describe('a country that is safe is told nothing', () => {
  it('no band while the enemy is far and the count is low', () => {
    const g = game();
    lose(g, 0.2);
    weaken(g, 0.7);
    const r = reading(g, ME)!;
    expect(r).toMatchObject({ canFall: false, how: null, lastMonth: false, within: false, twoThirds: false, seat: 'free' });
    expect(fallWarning(g)).toBeNull();
    expect(bandOf(g)).toBeUndefined();
  });

  it('an army massed across a frontier is not within reach: only one that stands on his home ground', () => {
    const g = game();
    const seat = seatOf(g);
    const abroad = seat.nb.map((j) => g.regions[j]).find((x) => x.owner !== ME);
    if (abroad) { abroad.ctrl = 'SOV'; reds(g, abroad, 3); expect(reading(g, ME)!.within).toBe(false); expect(fallWarning(g)).toBeNull(); }
    reds(g, beside(g));
    expect(reading(g, ME)!.within).toBe(true);
  });
});

describe('2. the enemy within reach, said once', () => {
  it('the month an enemy army first stands beside the capital\'s province', () => {
    const g = game();
    lose(g, 0.2);
    weaken(g, 0.7);
    const mark = markFor(g, ME);
    expect(mark.reach).toBeUndefined();
    const at = beside(g);
    reds(g, at);
    const r = reading(g, ME)!;
    expect(r).toMatchObject({ canFall: false, within: true, seat: 'reach', reachBy: 'SOV', reachFrom: at.id });
    const w = fallWarning(g)!;
    expect(w).toMatchObject({ title: IN_DANGER, level: 'reach', head: 'The enemy is within reach of Berlin', note: 'Hold Berlin, and win back home ground.', nat: ME, region: seatOf(g).id });
    // (Version 84.2.6, the UI director's words for ruling 211, section 2: the capital's words no longer say again that
    // the enemy is a march from it, and the warning ends with the alarm's help.)
    const help = homeAlarms(g, ME)[0].lines[2];
    expect(w.text).toBe(`The enemy is within reach of Berlin. Soviet troops stand in ${at.name}, beside the capital. ${r.lost} of ${r.of} home provinces are lost and the army is at two thirds of its strength. Germany gives in when the capital, the home provinces and the army lost together pass what the country will bear, and your campaign ends with it: it is ${Math.round(r.near * 100)}% of the way there. Hold Berlin, and win back home ground. ${help}`);
    // On the card: a red band, with what to do where the date stands and the capital to show. Under its headline
    // the band says in full what is lost and that the campaign ends with it: the whole warning is the three together,
    // and from Version 84.2.6 the alarm's help ends both the band's line and the whole warning.
    expect(w.text).toBe(`${w.head}. ${w.sub.replace(` ${help}`, '')} ${w.note} ${help}`);
    expect(w.sub).toMatch(/^Soviet troops stand in .+, beside the capital\. .+ your campaign ends with it: it is \d+% of the way there\. Nearest help: .+\.$/);
    expect(bandOf(g, mark)).toMatchObject({ word: 'WARNING', nats: [ME], head: 'The enemy is within reach of Berlin', text: w.text, sub: w.sub, note: 'Hold Berlin, and win back home ground.', region: seatOf(g).id });
    // The next card, told from the mark taken now, does not say it again.
    const next = markFor(g, ME);
    expect(next.reach).toBe(true);
    expect(fallWarning(g, ME, true)).toBeNull();
    expect(bandOf(g, next)).toBeUndefined();
    // But the card that opens a campaign says it, whatever its mark holds.
    expect(monthCard(g, next, ME, { opening: true }).bands.find((b) => b.title === IN_DANGER)).toMatchObject({ head: 'The enemy is within reach of Berlin' });
  });

  it('or the reckoning passes two thirds of its limit with no enemy near the capital', () => {
    const g = game();
    g.nations[ME].resilience = 0.45;
    lose(g, 0.5);
    weaken(g, 0.3);
    const r = reading(g, ME)!;
    expect(r.score).toBeGreaterThanOrEqual(REACH_AT * 0.45);
    expect(r).toMatchObject({ canFall: false, within: false, twoThirds: true });
    expect(fallWarning(g)).toMatchObject({ level: 'reach', head: 'Germany has lost two thirds of what it will bear', note: 'Win back home ground, and keep the army in being.' });
    expect(fallWarning(g, ME, true)).toBeNull();
  });
});

describe('1. a red band every month it can give in next month', () => {
  it('the enemy a march from the capital, and its loss would end it', () => {
    const g = game();
    lose(g, 0.55);
    weaken(g, 0.2);
    expect(fallWarning(g)).toBeNull();
    reds(g, beside(g));
    const r = reading(g, ME)!;
    expect(r).toMatchObject({ canFall: true, how: 'taken', sure: true, lastMonth: false });
    const w = fallWarning(g)!;
    expect(w).toMatchObject({ level: 'fall', head: 'Germany can fall next month', note: 'Hold Berlin, or win back home ground.' });
    // (Version 84.2.6: the warning ends with the alarm's help, on the band's second line too.)
    const help = homeAlarms(g, ME)[0].lines[2];
    expect(w.text).toBe(`Germany can fall next month. Soviet troops are a march from Berlin, ${r.lost} of ${r.of} home provinces are lost and the army is at a fifth of its strength. If Berlin is taken, Germany gives in and your campaign ends. Hold Berlin, or win back home ground. ${help}`);
    // The band's second line, printed in full: what is lost, and that the campaign ends with it.
    expect(w.sub).toBe(`Soviet troops are a march from Berlin, ${r.lost} of ${r.of} home provinces are lost and the army is at a fifth of its strength. If Berlin is taken, Germany gives in and your campaign ends. ${help}`);
    expect(w.text).toBe(`${w.head}. ${w.sub.replace(` ${help}`, '')} ${w.note} ${help}`);
    // Every month: it does not matter what the last card said.
    expect(fallWarning(g, ME, true)).toMatchObject({ level: 'fall' });
    // It is the first band of the card, above the news of the month.
    const mark = markFor(g, ME);
    g.headlines.push({ turn: g.turn, week: 2, text: 'Hungary declares war on Germany.', kind: 'war', nat: 'HUN' });
    const card = monthCard(g, mark, ME);
    expect(card.bands[0]).toMatchObject({ word: 'WARNING', title: IN_DANGER, nats: [ME], head: 'Germany can fall next month', note: 'Hold Berlin, or win back home ground.', region: seatOf(g).id });
  });

  it('the capital besieged', () => {
    const g = game();
    lose(g, 0.55);
    weaken(g, 0.2);
    reds(g, seatOf(g));
    const r = reading(g, ME)!;
    expect(r).toMatchObject({ canFall: true, how: 'taken', seat: 'besieged' });
    const w = fallWarning(g)!;
    expect(w.text).toContain(`Berlin is besieged, ${r.lost} of ${r.of} home provinces are lost and the army is at a fifth of its strength. If Berlin is taken, Germany gives in and your campaign ends.`);
    expect(w.note).toBe('Break the siege of Berlin, or win back home ground.');
  });

  it('a strong country is not told so because an enemy stands near: the capital\'s loss would not end it', () => {
    const g = game();
    reds(g, beside(g));
    expect(reading(g, ME)).toMatchObject({ canFall: false, within: true });
    expect(fallWarning(g, ME, true)).toBeNull();
  });

  it('shut in, and the months of the siege alone will end it next month', () => {
    const g = game();
    lose(g, 0.85, true);
    weaken(g, 0.1);
    shutIn(g);
    const seat = seatOf(g), limit = g.nations[ME].resilience;
    // The month the count is still under the line and a month more of siege carries it over.
    let since = -1;
    for (let k = 0; k < 14 && since < 0; k++) { g.flags[`siege:${seat.id}`] = AUG_44 - k; if (standing(g, ME).score < limit && standing(g, ME, 1).score >= limit) since = AUG_44 - k; }
    expect(since).toBeGreaterThan(0);
    expect(reading(g, ME)).toMatchObject({ canFall: true, how: 'clock', lastMonth: false });
    const w = fallWarning(g)!;
    expect(w).toMatchObject({ level: 'fall', head: 'Germany can fall next month', note: 'Break the siege of Berlin, or win back home ground.' });
    expect(w.text).toContain('A government shut in its capital loses its hold on the country month by month: as things stand Germany gives in next month, and at once if Berlin is taken. Your campaign ends with it.');
    expect(bandOf(g)).toMatchObject({ head: 'Germany can fall next month' });
  });

  it('a country that fights on without its capital, when a little more lost would end it', () => {
    const g = game();
    seatOf(g).ctrl = 'SOV';
    lose(g, 0.5, true);
    weaken(g, 0.2);
    const r = reading(g, ME)!;
    expect(r.score).toBeLessThan(r.limit);
    expect(r).toMatchObject({ canFall: true, how: 'more', seat: 'lost' });
    const w = fallWarning(g)!;
    expect(w.text).toBe(`Germany can fall next month. Berlin is lost, ${r.lost} of ${r.of} home provinces are lost and the army is at a fifth of its strength. With a little more lost, Germany gives in and your campaign ends. Take back Berlin or other home ground.`);
  });

  it('over the line already, and only a free capital keeps it in the war', () => {
    const g = game();
    g.nations[ME].resilience = 0.3;
    lose(g, 0.5);
    weaken(g, 0.3);
    const r = reading(g, ME)!;
    expect(r.score).toBeGreaterThanOrEqual(0.3);
    expect(r).toMatchObject({ canFall: true, how: 'reached', seat: 'free' });
    const w = fallWarning(g)!;
    expect(w.text).toContain('That is more than the country will bear, and only a free capital keeps it in the war: Germany gives in, and your campaign ends, the month an enemy army stands before Berlin.');
    expect(w.note).toBe('Keep the enemy from Berlin, and win back home ground.');
  });
});

describe('3. the last month of a siege', () => {
  it('leads the card in the LAST MONTH frame, and has no band', () => {
    const g = game();
    lose(g, 0.85, true);
    weaken(g, 0.1);
    shutIn(g);
    const seat = seatOf(g), limit = g.nations[ME].resilience;
    let since = -1;
    for (let k = 0; k < 14 && since < 0; k++) { g.flags[`siege:${seat.id}`] = AUG_44 - k; if (standing(g, ME).score >= limit) since = AUG_44 - k; }
    expect(since).toBeGreaterThan(0);
    const r = reading(g, ME)!;
    expect(r).toMatchObject({ canFall: true, how: 'clock', lastMonth: true });
    const w = fallWarning(g)!;
    expect(w).toMatchObject({ level: 'last', head: 'Germany gives in as September opens unless the siege of Berlin is broken', note: 'Break the siege of Berlin this month.' });
    expect(w.frame).toMatchObject({ head: 'Berlin is shut in', sentence: 'Germany gives in as September opens unless the siege of Berlin is broken. Your campaign ends with it.', month: 'September' });
    expect(w.frame!.sub).toMatch(/^Besieged since \w+ 19\d\d by Soviet troops$/);
    expect(w.frame!.figures).toEqual([{ n: '1', label: 'month left' }, { n: String(r.lost), label: `of ${r.of} home provinces lost` }]);
    const card = monthCard(g, markFor(g, ME), ME);
    expect(card.lead).toMatchObject({ stake: true, tag: 'LAST MONTH', tone: 'bad', ours: ME, kind: 'THE CAPITAL', head: 'Berlin is shut in', follows: w.frame!.sentence, when: 'as September opens', region: seat.id });
    expect(card.bands.filter((b) => b.title === IN_DANGER)).toEqual([]);
  });
});

describe('4. a besieged capital\'s lead', () => {
  it('adds what hangs on it, and a besieged town that is not his capital does not', () => {
    const g = game();
    const mark = markFor(g, ME);
    const seat = seatOf(g);
    shutIn(g);
    const rep = (at: Region): void => {
      const unit = (nat: string) => ({ name: 'I Corps', nat, kind: 'inf', divs: 3, men: 30000, lost: 0, fate: '' });
      g.reports.push({ id: 9900 + at.id, turn: g.turn, week: 2, region: at.id, regionName: at.name, attNat: 'SOV', defNat: ME, attackers: [unit('SOV')], defenders: [unit(ME)],
        lines: [], aPower: 2, dPower: 1, odds: 2, level: 'win', taken: false, siege: true, attLost: 1500, defLost: 900, prisoners: 0 } as unknown as BattleReport);
    };
    rep(seat);
    const card = monthCard(g, mark, ME);
    expect(card.lead).toMatchObject({ tag: 'BESIEGED', head: 'Berlin is besieged' });
    expect(card.lead!.follows.endsWith(` ${CAPITAL_BESIEGED}`)).toBe(true);
    expect(CAPITAL_BESIEGED).toBe('It is your capital: the country will not long outlast it.');
    // Another town of his under siege.
    const k = game();
    const m2 = markFor(k, ME);
    const town = k.regions.find((x) => x.owner === ME && x.ctrl === ME && x.id !== seatOf(k).id && !seatOf(k).nb.includes(x.id) && !x.overseas)!;
    for (const c of [...town.units]) c.dead = true;
    town.units = [];
    reds(k, town, 3);
    k.reports.push({ id: 9990, turn: k.turn, week: 2, region: town.id, regionName: town.name, attNat: 'SOV', defNat: ME, attackers: [{ name: 'I Corps', nat: 'SOV', kind: 'inf', divs: 3, men: 30000, lost: 0, fate: '' }],
      defenders: [{ name: 'I Corps', nat: ME, kind: 'inf', divs: 3, men: 30000, lost: 0, fate: '' }], lines: [], aPower: 2, dPower: 1, odds: 2, level: 'win', taken: false, siege: true, attLost: 1500, defLost: 900, prisoners: 0 } as unknown as BattleReport);
    const other = monthCard(k, m2, ME);
    const row = [other.lead, ...other.rows].find((x) => x?.region === town.id);
    expect(row).toBeDefined();
    expect(row!.follows).not.toContain(CAPITAL_BESIEGED);
  });
});

describe('display only', () => {
  it('reads the position and writes nothing', () => {
    const g = game();
    lose(g, 0.55);
    weaken(g, 0.2);
    reds(g, seatOf(g));
    const before = saveGame(g);
    reading(g, ME); fallWarning(g); withinReach(g); standing(g, ME, 1);
    expect(saveGame(g)).toBe(before);
  });
});
