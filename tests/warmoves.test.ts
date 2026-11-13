// Version 55, 'the war keeps moving' (the grand campaign). A war nobody is fighting does not stay that way: the
// staffs' monthly chances are real throws, a quiet front makes bolder generals, a nation with ships and no front
// crosses the sea, and the player is told when his war has gone silent. Sieges end: a ring closes with help from
// next door or when every road is cut, a great city never opens its gates at a summons, an assault is more of a
// gamble than a fight in the open, a government shut in its capital loses its hold month by month, and a country
// does not give in while its capital stands free and most of its home is its own. Troops on a neutral's ground
// cannot be attacked until war is declared on it, and news that repeats is folded into one line. None of this is
// in the historical scenarios. The numbers are in data/campaign/open.json, siege.json and production.json.
import { describe, expect, it } from 'vitest';
import east from '../data/campaign/east.json';
import open from '../data/campaign/open.json';
import { PRODUCTION, aiProduction } from '../src/campaign/economy';
import { addCorps, allied, atWar, checkCollapse, collapseScore, createGame, homeLost, relocate, routineNews, foldNews, startPhase } from '../src/campaign/game';
import { SIEGE, arrive, besieged, garrison, garrisonOf, guardOf, ringClosed, siegeEmpties, townHeld } from '../src/campaign/siege';
import { boldness, monthly, probed, probing, quietNews, quietWith, startOpen } from '../src/campaign/staff';
import { townSize } from '../src/campaign/towns';
import { attackOutlook, beginHuman, candidates, humanUp, orderAttack, orderMove, previewAttack, runTurnAI, stepAI, stormOutlook, whyNotAttack } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const I = open.impatience;

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
/** A grand campaign with the player's first month open. */
function playing(human = 'GER', seed = 3): Game {
  const g = free(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const clearField = (g: Game, r: Region, to: Region): void => { for (const u of [...r.units]) relocate(g, u, to); };
const germansBy = (g: Game, r: Region): Corps[] =>
  r.nb.flatMap((j) => g.regions[j].units.filter((u) => u.nat === 'GER' && u.kind !== 'fort' && u.kind !== 'eng' && u.ap > 0));
const quietTold = (g: Game): number => g.headlines.filter((h) => /^No battle has been fought between you and your enemies/.test(h.text)).length;

describe('the staffs\' monthly chances', () => {
  it('are a throw of their own each month, and the same throw for the same war', () => {
    const g = free(1);
    const throws = Array.from({ length: 24 }, (_, m) => monthly(g, 'GER', 'west', m + 1));
    for (const x of throws) { expect(x).toBeGreaterThanOrEqual(0); expect(x).toBeLessThan(1); }
    // Two years of months are not two years of the same answer: some say yes at one chance in three, most say no.
    expect(Math.max(...throws) - Math.min(...throws)).toBeGreaterThan(0.6);
    const yes = throws.filter((x) => x < 1 / 3).length;
    expect(yes).toBeGreaterThanOrEqual(3);
    expect(yes).toBeLessThanOrEqual(15);
    expect(monthly(free(1), 'GER', 'west', 7)).toBe(throws[6]);
    expect(monthly(free(2), 'GER', 'west', 7)).not.toBe(throws[6]);
  });

  it('do not say yes in the same month in every war', () => {
    const first = (seed: number): number => { const g = free(seed); for (let m = 1; m <= 60; m++) if (monthly(g, 'GER', 'west', m) < 0.35) return m; return 0; };
    expect(new Set(Array.from({ length: 12 }, (_, k) => first(k + 1))).size).toBeGreaterThanOrEqual(4);
  });
});

describe('a quiet front', () => {
  it('is counted from the last fight either way round, or from the declaration of war', () => {
    const g = free(2);
    g.turn = 10;
    expect(quietWith(g, 'GER', 'FRA')).toBe(9);
    expect(quietWith(g, 'FRA', 'GER')).toBe(9);
    g.nations.FRA.lastFight.GER = 8;
    expect(quietWith(g, 'GER', 'FRA')).toBe(2);
    g.nations.GER.lastFight.FRA = 10;
    expect(quietWith(g, 'FRA', 'GER')).toBe(0);
  });

  it('makes bolder generals term by term, and a fight makes them careful again', () => {
    const g = free(2);
    const front = { enemies: ['FRA', 'UK'] };
    g.turn = I.after[0];
    expect(boldness(g, 'GER', front)).toBe(0);
    g.turn = I.after[0] + 1;
    expect(boldness(g, 'GER', front)).toBe(1);
    g.turn = I.after[1] + 1;
    expect(boldness(g, 'GER', front)).toBe(2);
    // One fight with either enemy on the front is fighting on the front.
    g.nations.UK.lastFight.GER = g.turn - 1;
    expect(boldness(g, 'GER', front)).toBe(0);
    // The terms shorten the ratio a staff waits for, and never below the floor.
    expect(I.ratio[0]).toBeLessThan(1);
    expect(I.ratio[1]).toBeLessThan(I.ratio[0]);
    expect(I.floor).toBeLessThanOrEqual(I.ratio[1] * 1.3);
  });

  it('is not a truce: a staff on the defensive takes a weak point where it sees one, and keeps at it while the attack is fresh', () => {
    const g = free(2);
    expect(probing(g, 'GER')).toBeNull();
    g.turn = I.after[0] + 1;
    const first = probing(g, 'GER')!;
    expect(first.enemies.has('FRA')).toBe(true);
    expect(first.odds).toBe(I.local[0]);
    expect(first.most).toBe(I.locals);
    expect(first.step).toBe(1);
    g.turn = I.after[1] + 1;
    expect(probing(g, 'GER')!.odds).toBe(I.local[1]);
    expect(I.local[1]).toBeLessThan(I.local[0]);
    // A local attack is a fight: the front is no longer quiet, but the staff keeps at it for a season.
    probed(g, 'GER', 'FRA', 2);
    for (const e of g.nations.GER.atWar) g.nations.GER.lastFight[e] = g.turn;
    g.turn += I.effort;
    expect(probing(g, 'GER')!.step).toBe(2);
    expect([...probing(g, 'GER')!.enemies]).toEqual(['FRA']);
    g.turn += 1;
    expect(probing(g, 'GER')).toBeNull();
  });

  it('gives the player no orders, and history none either', () => {
    const mine = free(2, 'GER');
    mine.turn = I.after[1] + 1;
    expect(probing(mine, 'GER')).toBeNull();
    expect(probing(mine, 'FRA')).not.toBeNull();
    const history = createGame(2, null);
    history.turn = I.after[1] + 1;
    expect(probing(history, 'GER')).toBeNull();
  });

  it('is told to the player once a year while it lasts', () => {
    const g = free(2, 'UK');
    g.turn = 12;
    quietNews(g);
    expect(quietTold(g)).toBe(0);
    g.turn = 13;
    quietNews(g);
    expect(quietTold(g)).toBe(1);
    expect(g.headlines[g.headlines.length - 1].text).toMatch(/for a year\./);
    for (let m = 14; m <= 24; m++) { g.turn = m; quietNews(g); }
    expect(quietTold(g)).toBe(1);
    g.turn = 25;
    quietNews(g);
    expect(quietTold(g)).toBe(2);
    expect(g.headlines[g.headlines.length - 1].text).toMatch(/for 2 years\./);
    // A war that is being fought is not news of this kind, and nor is anything in a war with nobody at the table.
    const busy = free(2, 'UK');
    busy.turn = 20;
    busy.nations.GER.lastFight.UK = 18;
    quietNews(busy);
    expect(quietTold(busy)).toBe(0);
    const nobody = free(2);
    nobody.turn = 20;
    quietNews(nobody);
    expect(quietTold(nobody)).toBe(0);
  });
});

describe('enemy troops on a neutral\'s ground', () => {
  it('cannot be attacked until war is declared on the neutral', () => {
    const g = playing('GER');
    const spot = g.regions.find((r) => !allied(g, 'GER', r.ctrl) && !atWar(g, 'GER', r.ctrl) && !r.units.length && germansBy(g, r).length > 0)!;
    expect(spot).toBeTruthy();
    const pole = g.corps.find((c) => c.nat === 'POL' && !c.dead && c.kind === 'inf' && c.town === undefined)!;
    relocate(g, pole, spot);
    expect(spot.units).toContain(pole);
    const mine = candidates(g, spot);
    expect(mine.length).toBeGreaterThan(0);
    expect(whyNotAttack(g, spot)).toMatch(/you are not at war with it\. Declare war on/);
    expect(previewAttack(g, mine, spot)).toBeNull();
    expect(attackOutlook(g, mine, spot)).toBeNull();
    const fights = g.reports.length;
    expect(orderAttack(g, mine, spot)).toBeNull();
    expect(g.reports.length).toBe(fights);
    expect(pole.dead).toBeFalsy();
    // The same troops on their own country's ground are fair game, and history has no such rule.
    expect(whyNotAttack(g, g.byName['Lodz'])).toBe('');
    expect(whyNotAttack(createGame(3, 'GER'), spot, 'GER')).toBe('');
  });
});

describe('news that repeats', () => {
  const told = (g: Game, text: string): number => g.headlines.filter((h) => h.text === text).length;

  it('is told in full the first time, folded into one line a month after that, and told in full again after a pause', () => {
    const g = free(2, 'UK');
    const full = 'German bombers raid the docks of Hamburg again.';
    routineNews(g, 'raid:GER:7', 'battle', full, 'the docks raided', 'GER', 7);
    expect(told(g, full)).toBe(1);
    g.turn++;
    routineNews(g, 'raid:GER:7', 'battle', full, 'the docks raided', 'GER', 7);
    routineNews(g, 'escort:GER:2', 'battle', 'German escorts sink a submarine.', 'a submarine sunk', 'GER');
    routineNews(g, 'escort:GER:2', 'battle', 'German escorts sink a submarine.', 'another submarine sunk', 'GER');
    expect(told(g, full)).toBe(1);
    const before = g.headlines.length;
    foldNews(g, 'ITA');
    expect(g.headlines.length).toBe(before);
    foldNews(g, 'GER');
    expect(g.headlines.length).toBe(before + 1);
    expect(g.headlines[before].text).toBe('The month in the air and at sea, Germany: the docks raided; another submarine sunk.');
    foldNews(g, 'GER');
    expect(g.headlines.length).toBe(before + 1);
    // Four quiet months later it is news again.
    g.turn += 4;
    routineNews(g, 'raid:GER:7', 'battle', full, 'the docks raided', 'GER', 7);
    expect(told(g, full)).toBe(2);
  });

  it('is never folded when the player ordered it, or in the historical campaigns', () => {
    const g = free(2, 'UK');
    for (let k = 0; k < 3; k++) { routineNews(g, 'raid:UK:9', 'battle', 'British bombers raid the Ruhr.', 'the Ruhr raided', 'UK', 9); g.turn++; }
    expect(told(g, 'British bombers raid the Ruhr.')).toBe(3);
    const history = createGame(2, 'UK');
    for (let k = 0; k < 3; k++) { routineNews(history, 'raid:GER:7', 'battle', 'German bombers raid London.', 'London raided', 'GER', 7); history.turn++; }
    expect(told(history, 'German bombers raid London.')).toBe(3);
  });
});

describe('the computer\'s war economy', () => {
  // The great powers at war from the first day, looked at late enough that every scheduled formation is in.
  const MAJORS = ['GER', 'UK', 'FRA'];
  const forming = (g: Game, nat: string): number => g.nations[nat].queue.filter((b) => (b.what === 'corps' && b.kind !== 'eng') || b.what === 'fort').length;
  const order = (g: Game, nat: string): number => { g.turn = 80; g.nations[nat].queue = []; g.nations[nat].pp = 400; aiProduction(g, nat); return forming(g, nat); };

  it('keeps more than two corps forming where a great industry can pay for them, in the grand campaign only', () => {
    expect(PRODUCTION.aiLine).toBeGreaterThan(0);
    const g = free(2);
    const lines = MAJORS.map((nat) => order(g, nat));
    expect(Math.max(...lines)).toBeGreaterThan(2);
    for (const [k, nat] of MAJORS.entries()) {
      let home = 0;
      for (const r of g.regions) if (r.owner === nat && r.ctrl === nat && !r.overseas) home += r.fac;
      expect(lines[k], nat).toBeLessThanOrEqual(Math.max(2, Math.floor(home / PRODUCTION.aiLine!)));
    }
    const history = createGame(2, null);
    for (const nat of MAJORS) expect(order(history, nat), nat).toBeLessThanOrEqual(2);
  });

  it('raises a standing call-up only as large as the factories it still holds can arm', () => {
    const plan = east.mobilisation.SOV;
    const wave = (lose: number): number => {
      const g = free(2);
      for (const [a, b] of [['GER', 'SOV'], ['SOV', 'GER']]) if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
      g.turn = 30;
      g.flags['mobilised:SOV'] = g.turn - plan.perMonth.length - 2;
      // The factory towns furthest from Moscow are lost first: the ones nearest stay to raise the new corps in.
      const towns = g.regions.filter((r) => r.owner === 'SOV' && !r.overseas && r.fac > 0 && !r.cap).sort((x, y) => y.fac - x.fac);
      let own = 0;
      for (const r of g.regions) if (r.owner === 'SOV' && !r.overseas) own += r.fac;
      let gone = 0;
      for (const r of towns) { if (gone >= lose * own) break; r.ctrl = 'GER'; gone += r.fac; }
      const before = g.corps.filter((c) => c.nat === 'SOV' && /Rifle Corps$/.test(c.name)).length;
      startPhase(g, 'SOV');
      const raised = g.corps.filter((c) => c.nat === 'SOV' && /Rifle Corps$/.test(c.name)).length - before;
      expect(raised).toBeLessThanOrEqual(Math.round(plan.after * (1 - gone / own)));
      return raised;
    };
    const whole = wave(0), half = wave(0.5);
    expect(whole).toBe(plan.after);
    expect(half).toBeLessThan(whole);
  });
});

describe('sieges that end', () => {
  /** Lodz held by a Polish corps inside it, and one weak German corps round it with nobody next door to help. */
  function thinRing(): { g: Game; town: Region; ring: Corps } {
    const g = playing('GER');
    const town = g.byName['Lodz'], berlin = g.byName['Berlin'];
    const inside = town.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!;
    expect(garrison(g, inside)).toBe(true);
    clearField(g, town, g.byName['Warsaw']);
    const ring = germansBy(g, town).find((c) => c.kind === 'inf')!;
    for (const j of town.nb) for (const u of [...g.regions[j].units]) if (u.nat === 'GER' && u !== ring) relocate(g, u, berlin);
    ring.str = 0.3;
    relocate(g, ring, town);
    return { g, town, ring };
  }

  it('the ring is open while a road out is open and the army round the town is too weak to shut it in alone', () => {
    const { g, town } = thinRing();
    expect(besieged(g, town)).toBe(true);
    expect(garrisonOf(town).length).toBe(1);
    expect(ringClosed(g, town)).toBe(false);
  });

  it('the ring closes with the help of troops in the country next door, who count for part of their weight', () => {
    const { g, town } = thinRing();
    const next = g.regions[town.nb.find((j) => g.regions[j].ctrl === 'GER')!];
    expect(SIEGE.ringNear).toBeGreaterThan(0);
    expect(SIEGE.ringNear).toBeLessThan(1);
    for (let k = 0; k < 4; k++) addCorps(g, 'GER', `Test Infantry Corps ${k}`, 'inf', 3, next.id, {});
    expect(ringClosed(g, town)).toBe(true);
  });

  it('the ring closes when every road out runs into ground the enemy holds, whatever stands at the gates', () => {
    const { g, town } = thinRing();
    for (const j of town.nb) { const x = g.regions[j]; if (allied(g, 'POL', x.ctrl)) { clearField(g, x, g.byName['Lwow']); x.ctrl = 'GER'; } }
    expect(ringClosed(g, town)).toBe(true);
  });

  it('a great city never opens its gates at a summons, however many stand outside; a town does', () => {
    expect(SIEGE.summonsNot).toEqual(expect.arrayContaining(['city', 'capital']));
    expect(SIEGE.summonsNot).not.toContain('town');
    const g = playing('GER');
    const city = g.regions.find((r) => r.owner === 'POL' && r.ctrl === 'POL' && townSize(g, r) === 'city' && germansBy(g, r).length > 0)!;
    expect(city).toBeTruthy();
    clearField(g, city, g.byName['Lwow']);
    expect(garrisonOf(city).length).toBe(0);
    expect(guardOf(g, city)).toBeGreaterThan(0);
    for (const c of g.corps.filter((x) => x.nat === 'GER' && x.kind === 'inf' && !x.dead).slice(0, 8)) relocate(g, c, city);
    expect(arrive(g, city, 'GER')).toBe(false);
    expect(city.ctrl).toBe('POL');
    expect(besieged(g, city)).toBe(true);
    // The same army before a town of the middle size is let in.
    const town = g.byName['Lodz'];
    expect(townSize(g, town)).toBe('town');
    clearField(g, town, g.byName['Lwow']);
    for (const c of g.corps.filter((x) => x.nat === 'GER' && x.kind === 'inf' && !x.dead && x.reg !== city.id).slice(0, 5)) relocate(g, c, town);
    expect(arrive(g, town, 'GER')).toBe(true);
    expect(town.ctrl).toBe('GER');
  });

  it('a town that opens its gates after the army outside is beaten says so in the report', () => {
    const g = playing('GER');
    const town = g.byName['Lodz'];
    const out = town.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!;
    for (const u of [...town.units]) if (u !== out) relocate(g, u, g.byName['Warsaw']);
    out.str = 0.15;
    out.org = 0.2;
    const rep = orderAttack(g, candidates(g, town), town)!;
    expect(rep.town).toBeFalsy();
    expect(rep.taken).toBe(true);
    expect(town.ctrl).toBe('GER');
    expect(rep.gates).toBe(true);
    // A city whose field army is beaten is besieged and not taken, and its report carries no such mark.
    const h = playing('GER');
    const city = h.regions.find((r) => r.owner === 'POL' && r.ctrl === 'POL' && townSize(h, r) === 'city' && germansBy(h, r).length > 0 && r.units.some((u) => u.nat === 'POL'))!;
    for (const u of city.units) { u.str = 0.15; u.org = 0.2; }
    const siege = orderAttack(h, candidates(h, city), city)!;
    expect(siege.gates).toBeFalsy();
    expect(city.ctrl).toBe('POL');
    expect(besieged(h, city)).toBe(true);
  });

  it('an assault is more of a gamble than a fight in the open: the even chance stands at longer odds', () => {
    const [lo, hi] = SIEGE.assault.dice;
    expect(lo).toBeLessThan(0.8);
    expect(hi).toBeLessThan(1.2);
    const g = playing('GER');
    const town = g.byName['Lodz'];
    const inside = town.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!;
    garrison(g, inside);
    clearField(g, town, g.byName['Warsaw']);
    for (const c of germansBy(g, town).slice(0, 3)) orderMove(g, c, town.id);
    const storm = stormOutlook(g, town.id)!;
    expect(storm.even).toBeCloseTo(2.4 / (lo + hi), 6);
    expect(storm.even).toBeGreaterThan(1.3);
    const field = g.regions.find((r) => r.ctrl === 'POL' && r.units.some((u) => u.nat === 'POL') && candidates(g, r).length > 0)!;
    expect(attackOutlook(g, candidates(g, field), field)!.even).toBeCloseTo(1.2, 6);
  });

  it('an assault off the march is weaker the bigger the place', () => {
    const h = SIEGE.assault.hasty;
    expect(h.village).toBeLessThanOrEqual(1);
    expect(h.town).toBeLessThan(h.village);
    expect(h.city).toBeLessThan(h.town);
    expect(h.capital).toBeLessThan(h.city);
  });

  it('a town left with nobody in it falls to the army round it when the month closes', () => {
    const { g, town } = thinRing();
    for (const c of [...garrisonOf(town)]) relocate(g, c, g.byName['Warsaw']);
    g.flags[`guard:${town.id}`] = 100;
    expect(townHeld(g, town)).toBe(false);
    expect(town.ctrl).toBe('POL');
    siegeEmpties(g);
    expect(town.ctrl).toBe('GER');
    expect(besieged(g, town)).toBe(false);
    // A town still held is left alone.
    const held = thinRing();
    siegeEmpties(held.g);
    expect(held.town.ctrl).toBe('POL');
  });
});

describe('the fall of a country', () => {
  it('does not come while its capital stands free and most of its home is its own', () => {
    const g = playing('GER');
    const pol = g.nations.POL;
    expect(SIEGE.stand).toBeGreaterThan(0.3);
    expect(homeLost(g, 'POL')).toBeLessThan(SIEGE.stand);
    // An army that has melted away and a will that has gone: by the score alone the country is finished.
    pol.resilience = 0.01;
    pol.armyPeak = Math.max(pol.armyPeak, 100);
    expect(collapseScore(g, 'POL')).toBeGreaterThanOrEqual(pol.resilience);
    checkCollapse(g, 'POL', 'GER');
    expect(pol.alive).toBe(true);
    // With the enemy at the gates of the capital it gives in.
    const w = g.byName['Warsaw'];
    clearField(g, w, g.byName['Lwow']);
    for (const c of g.corps.filter((x) => x.nat === 'GER' && x.kind === 'inf' && !x.dead).slice(0, 4)) relocate(g, c, w);
    expect(besieged(g, w)).toBe(true);
    checkCollapse(g, 'POL', 'GER');
    expect(pol.alive).toBe(false);
  });

  it('comes without a siege once most of the home country is lost', () => {
    const g = playing('GER');
    const pol = g.nations.POL;
    pol.resilience = 0.01;
    for (const r of g.regions) if (r.owner === 'POL' && !r.cap) { clearField(g, r, g.byName['Warsaw']); r.ctrl = 'GER'; }
    expect(homeLost(g, 'POL')).toBeGreaterThanOrEqual(SIEGE.stand);
    checkCollapse(g, 'POL', 'GER');
    expect(pol.alive).toBe(false);
  });
});

describe('a whole war with nobody at the table', () => {
  it('keeps moving: the east opens by the summer of 1942, no siege drags past two years and no corps stands on ground it has no right to', () => {
    const g = free(7);
    const since = new Map<number, number>();
    let longest = 0;
    while (g.turn <= 36) {
      const t = g.turn;
      runTurnAI(g);
      for (const r of g.regions) {
        if (besieged(g, r)) { if (!since.has(r.id)) since.set(r.id, t); longest = Math.max(longest, t - since.get(r.id)!); } else since.delete(r.id);
      }
      for (const c of g.corps) {
        if (c.dead || c.town !== undefined) continue;
        const host = g.regions[c.reg].ctrl;
        expect(allied(g, c.nat, host) || atWar(g, c.nat, host), `${c.name} in ${g.regions[c.reg].name}, month ${t}`).toBe(true);
      }
    }
    const eastOpens = g.flags['war:GER:SOV'] ?? g.flags['war:SOV:GER'] ?? 0;
    expect(eastOpens).toBeGreaterThan(8);
    expect(eastOpens).toBeLessThanOrEqual(34);
    expect(longest).toBeLessThanOrEqual(24);
    // And it was a war: battles in the last year of it, not only in the first.
    expect(g.reports.filter((r) => r.turn > 24).length).toBeGreaterThan(20);
  }, 120_000);
});
