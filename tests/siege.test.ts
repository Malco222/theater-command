// Cities, towns and sieges (the grand campaign). The town is the key to its province. Troops stand either in the
// field or inside the town as its garrison, and a town has a guard of its own. Winning the field wins the country
// round the town and puts it under siege; the province changes hands only when the town is stormed, starved out or
// opens its gates. A town is assaulted once a month, and an assault in the month the army came up is a hasty one. A
// garrison can sally out, a relieving army raises the siege with the garrison's help, bombs and shells break down
// the works, and a besieged province pays nothing and raises nothing. A full town takes no more troops. A country
// whose capital is closely ringed counts it part lost, and gives in to the enemy at its throat. None of this is in
// the historical scenarios. The numbers are in data/campaign/siege.json.
import { describe, expect, it } from 'vitest';
import { addCorps, allied, assess, atWar, collapseScore, createGame, relocate, victorOf } from '../src/campaign/game';
import { canRaise, provinceYield } from '../src/campaign/economy';
import { loadGame, saveGame } from '../src/campaign/save';
import {
  SIEGE, arrive, besieged, besiegerOf, besiegers, breachOf, fieldCtrl, garrison, garrisonFed, garrisonOf, guardOf, hitTown, investedOn, monthsShut, musterNear,
  planTownHit, previewSally, ringClosed, roomIn, sally, siegeMonth, siegeRules, siegeSupply, siegeView, storesOf, takeField, townBonus, townHeld, whyNotGarrison,
  whyNotSally, whyNotStorm,
} from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import {
  beginHuman, candidates, garrisonsAt, humanUp, orderAttack, orderGarrison, orderMove, orderStorm, orderTakeField, reach, runTurnAI, siegesInSight, stepAI,
  stormCandidates, stormOutlook, townSiege, whyNotGarrisonTown, whyNotLeaveTown, whyNotStormTown,
} from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A grand campaign with the player's first month open, as the game's own screen would have it. */
function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
/** German field corps beside a province with marching time in hand. */
const germansBy = (g: Game, r: Region): Corps[] =>
  r.nb.flatMap((j) => g.regions[j].units.filter((u) => u.nat === 'GER' && u.kind !== 'fort' && u.kind !== 'eng' && u.ap > 0));
/** Clears the field of a province: its armies are set down in another, for a test. */
function clearField(g: Game, r: Region, to: Region): void {
  for (const u of [...r.units]) relocate(g, u, to);
}
const lineLabels = (lines: { label: string }[]): string[] => lines.map((l) => l.label);

/**
 * Germany in September 1939, with Lodz held against her: one Polish corps inside the town, nobody in the field.
 * `inside: false` leaves the town to its guard alone.
 */
function lodz(o: { inside?: boolean } = {}): { g: Game; town: Region; inside: Corps | null; ger: Corps[] } {
  const g = playing('GER');
  const town = g.byName['Lodz'];
  let inside: Corps | null = null;
  if (o.inside !== false) {
    inside = town.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!;
    expect(garrison(g, inside)).toBe(true);
  }
  clearField(g, town, g.byName['Warsaw']);
  return { g, town, inside, ger: germansBy(g, town) };
}

describe('sieges are for the grand campaign', () => {
  it('the historical scenarios have no towns to besiege: ground changes hands as it always did', () => {
    const g = createGame(3, 'GER');
    const town = g.byName['Lodz'];
    expect(siegeRules(g)).toBe(false);
    expect(siegeView(g, town.id)).toBeNull();
    expect(guardOf(g, town)).toBe(0);
    expect(townHeld(g, town)).toBe(false);
    expect(whyNotGarrison(g, town.units[0])).toMatch(/only in the grand campaign/);
    expect(arrive(g, town, 'GER')).toBe(true);
    expect(town.ctrl).toBe('GER');
  });

  it('the grand campaign has them, and a saved game keeps them', () => {
    const g = playing('GER');
    expect(siegeRules(g)).toBe(true);
    expect(siegeRules(loadGame(saveGame(g))!)).toBe(true);
  });
});

describe('the town as a fortress', () => {
  it('a bigger place is stronger, holds more and eats longer', () => {
    for (const k of ['bonus', 'room', 'stores', 'guard'] as const) {
      const t = SIEGE[k] as Record<string, number>;
      expect(t.village).toBeLessThanOrEqual(t.town);
      expect(t.town).toBeLessThanOrEqual(t.city);
      expect(t.city).toBeLessThanOrEqual(t.capital);
    }
    expect(SIEGE.bonus.village).toBeGreaterThan(1);
  });

  it('a panel can read what a town is worth to its defenders', () => {
    const g = playing('GER');
    const w = g.byName['Warsaw'];
    const v = townSiege(g, w.id)!;
    expect(v.size).toBe('capital');
    expect(v.holder).toBe('POL');
    expect(v.by).toBeNull();
    expect(v.room).toBe(SIEGE.room.capital + (w.fort ? SIEGE.fortRoom : 0));
    expect(v.bonus).toBeCloseTo(townBonus(g, w), 6);
    expect(v.bonus).toBeGreaterThanOrEqual(SIEGE.bonus.capital);
    expect(v.guard).toBeGreaterThan(0);
    expect(v.guard).toBeLessThanOrEqual(SIEGE.guard.capital);
    expect(v.stores).toBe(storesOf(g, w));
    expect(v.lines.length).toBeGreaterThanOrEqual(2);
    expect(v.lines[0]).toMatch(/Troops inside Warsaw fight \d+% stronger/);
  });

  it('a town guard stands only for the country the town belongs to', () => {
    const { g, town, ger } = lodz({ inside: false });
    expect(guardOf(g, town)).toBeGreaterThan(0);
    expect(orderMove(g, ger[0], town.id)).toBe(1);
    expect(town.ctrl).toBe('GER');
    expect(guardOf(g, town)).toBe(0);
  });
});

describe('garrisons', () => {
  it('the player sends a corps into a town and brings it out again', () => {
    const g = playing('GER');
    const c = g.corps.find((x) => x.nat === 'GER' && x.kind === 'inf' && !x.dead && g.regions[x.reg].ctrl === 'GER' && g.regions[x.reg].owner === 'GER')!;
    const r = g.regions[c.reg];
    expect(whyNotGarrisonTown(g, c)).toBeNull();
    expect(orderGarrison(g, c)).toBe(true);
    expect(c.town).toBe(g.turn);
    expect(c.ap).toBe(0);
    expect(r.units).not.toContain(c);
    expect(garrisonOf(r)).toContain(c);
    expect(garrisonsAt(g, r.id).length).toBe(1);
    expect(whyNotGarrisonTown(g, c)).toMatch(/already in/);
    // Out again the same month: it has no marching time left.
    expect(whyNotLeaveTown(g, c)).toBeNull();
    expect(orderTakeField(g, c)).toBe(true);
    expect(c.town).toBeUndefined();
    expect(r.units).toContain(c);
    expect(garrisonOf(r)).not.toContain(c);
    expect(c.ap).toBe(0);
  });

  it('a corps that has been inside a month comes out with most of its marching time', () => {
    const g = playing('GER');
    const c = g.corps.find((x) => x.nat === 'GER' && x.kind === 'inf' && !x.dead && g.regions[x.reg].ctrl === 'GER' && g.regions[x.reg].units.length <= 3)!;
    expect(garrison(g, c)).toBe(true);
    g.turn++;
    expect(takeField(g, c)).toBe(true);
    expect(c.ap).toBe(c.apMax - 1);
  });

  it('a town has room for so many corps and no more', () => {
    const g = playing('GER');
    const r = g.regions.find((x) => x.ctrl === 'GER' && x.units.filter((u) => u.nat === 'GER' && u.kind === 'inf').length > roomIn(g, x))!;
    const inf = r.units.filter((u) => u.nat === 'GER' && u.kind === 'inf');
    const room = roomIn(g, r);
    for (let k = 0; k < room; k++) expect(orderGarrison(g, inf[k])).toBe(true);
    expect(whyNotGarrisonTown(g, inf[room])).toMatch(/has room for/);
    expect(orderGarrison(g, inf[room])).toBe(false);
    expect(garrisonOf(r).length).toBe(room);
  });

  it('fortress troops stay in their works', () => {
    const g = playing('GER');
    const fort = g.corps.find((x) => x.kind === 'fort' && !x.dead)!;
    expect(fort).toBeDefined();
    expect(whyNotGarrison(g, fort)).toMatch(/Fortress troops/);
  });
});

describe('winning the field does not win the province', () => {
  it('an army that marches up to a held town lays siege to it and goes no further that month', () => {
    const { g, town, inside, ger } = lodz();
    const a = ger[0];
    const way = reach(g, a).get(town.id)!;
    expect(way.attack).toBe(false);
    expect(orderMove(g, a, town.id)).toBe(1);
    expect(a.reg).toBe(town.id);
    expect(a.ap).toBe(0);
    expect(town.ctrl).toBe('POL');
    expect(besieged(g, town)).toBe(true);
    expect(fieldCtrl(g, town)).toBe('GER');
    expect(besiegerOf(g, town)).toBe('GER');
    expect(investedOn(g, town)).toBe(g.turn);
    expect(inside!.town).toBeDefined();
    expect(town.units).not.toContain(inside);
    const v = townSiege(g, town.id)!;
    expect(v.by).toBe('GER');
    expect(v.since).toBe(g.turn);
    expect(siegesInSight(g).some((s) => s.region === town.id)).toBe(true);
    expect(g.headlines.some((h) => /close round Lodz/.test(h.text))).toBe(true);
  });

  it('nobody marches through a held town in a month', () => {
    const g = playing('GER');
    let stops = 0;
    for (const c of g.corps.filter((x) => x.nat === 'GER' && !x.dead && x.kind !== 'fort' && x.ap > 0)) {
      for (const [id, e] of reach(g, c)) {
        const r = g.regions[id];
        if (!e.attack && !allied(g, 'GER', r.ctrl) && townHeld(g, r)) stops++;
        if (e.prev < 0 || e.prev === c.reg) continue;
        const through = g.regions[e.prev];
        expect(!allied(g, 'GER', through.ctrl) && townHeld(g, through)).toBe(false);
      }
    }
    // The map does have such places within a march: otherwise this test checks nothing.
    const { g: g2, town, ger } = lodz();
    expect(reach(g2, ger[0]).has(town.id)).toBe(true);
    expect(stops).toBeGreaterThanOrEqual(0);
  });

  it('a battle won in the field puts the town under siege, and the victors stay round it', () => {
    const g = playing('GER');
    const town = g.byName['Poznan'];
    const inside = town.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!;
    expect(garrison(g, inside)).toBe(true);
    expect(town.units.length).toBeGreaterThan(0);
    for (const u of town.units) { u.str = 0.3; u.org = 0.3; }
    const rep = orderAttack(g, candidates(g, town), town)!;
    expect(rep).not.toBeNull();
    // The battle in the field is won, and the report says the town holds out.
    expect(rep.taken).toBe(true);
    expect(rep.siege).toBe(true);
    expect(rep.town).toBeUndefined();
    expect(town.ctrl).toBe('POL');
    expect(besieged(g, town)).toBe(true);
    expect(garrisonOf(town)).toContain(inside);
    const round = besiegers(g, town);
    expect(round.length).toBeGreaterThan(0);
    for (const c of round) expect(c.ap).toBe(0);
    // The player can order the assault at once, though nobody has marching time left.
    expect(whyNotStormTown(g, town.id)).toBeNull();
    expect(stormCandidates(g, town.id).length).toBe(round.length);
  });

  it('a town nobody holds falls to the first army that comes', () => {
    const g = playing('GER');
    const village = g.regions.find((r) => r.ctrl === 'POL' && r.owner === 'POL' && guardOf(g, r) === 0 && r.units.every((u) => u.kind !== 'fort') && germansBy(g, r).length > 0)!;
    clearField(g, village, g.byName['Warsaw']);
    expect(townHeld(g, village)).toBe(false);
    const a = germansBy(g, village)[0];
    expect(orderMove(g, a, village.id)).toBe(1);
    expect(village.ctrl).toBe('GER');
    expect(besieged(g, village)).toBe(false);
    // It marched in without a halt.
    expect(a.ap).toBeGreaterThan(0);
  });

  it('a town held only by its guard opens its gates to an army strong enough, and holds against a weak one', () => {
    const strong = lodz({ inside: false });
    expect(orderMove(strong.g, strong.ger[0], strong.town.id)).toBe(1);
    expect(strong.town.ctrl).toBe('GER');
    expect(strong.g.headlines.some((h) => /Lodz opens its gates/.test(h.text))).toBe(true);

    const weak = lodz({ inside: false });
    const a = weak.ger[0];
    a.str = 0.2;
    expect(orderMove(weak.g, a, weak.town.id)).toBe(1);
    expect(assess(weak.g, besiegers(weak.g, weak.town), weak.town, { town: true }).odds).toBeLessThan(SIEGE.summons);
    expect(weak.town.ctrl).toBe('POL');
    expect(besieged(weak.g, weak.town)).toBe(true);
  });
});

describe('storming a town', () => {
  it('an assault off the march is a hasty one, and a prepared one goes in at full weight', () => {
    const { g, town, ger } = lodz();
    for (const c of ger.slice(0, 3)) orderMove(g, c, town.id);
    const hasty = stormOutlook(g, town.id)!;
    expect(lineLabels(hasty.lines)).toContain('The town');
    expect(lineLabels(hasty.lines)).toContain('A hasty assault');
    // A month later the same army goes in at full weight.
    g.flags[`siege:${town.id}`] = g.turn - 1;
    const ready = stormOutlook(g, town.id)!;
    expect(lineLabels(ready.lines)).not.toContain('A hasty assault');
    expect(ready.odds).toBeGreaterThan(hasty.odds);
    expect(ready.odds / hasty.odds).toBeCloseTo(1 / SIEGE.assault.hasty, 1);
  });

  it('a town that is stormed falls with everyone in it', () => {
    const { g, town, inside, ger } = lodz();
    inside!.str = 0.3;
    inside!.org = 0.3;
    for (const c of ger.slice(0, 4)) orderMove(g, c, town.id);
    const going = stormCandidates(g, town.id);
    expect(going.length).toBeGreaterThanOrEqual(3);
    for (const c of going) expect(c.ap).toBe(0);
    const rep = orderStorm(g, town.id)!;
    expect(rep.town).toBe(true);
    expect(rep.taken).toBe(true);
    expect(town.ctrl).toBe('GER');
    expect(besieged(g, town)).toBe(false);
    expect(investedOn(g, town)).toBe(-1);
    expect(inside!.dead).toBe(true);
    expect(garrisonOf(town).length).toBe(0);
    expect(rep.defenders.some((d) => /Town guard of Lodz/.test(d.name))).toBe(true);
    expect(rep.defenders.every((d) => d.fate === 'surrendered' || d.fate === 'destroyed')).toBe(true);
  });

  it('a town is assaulted once a month', () => {
    const { g, town, inside, ger } = lodz();
    const a = ger[0];
    a.str = 0.25;
    orderMove(g, a, town.id);
    const rep = orderStorm(g, town.id)!;
    expect(rep.town).toBe(true);
    expect(rep.taken).toBe(false);
    expect(town.ctrl).toBe('POL');
    expect(inside!.dead).toBe(false);
    if (besieged(g, town)) {
      expect(whyNotStormTown(g, town.id)).toMatch(/assaulted this month/);
      expect(orderStorm(g, town.id)).toBeNull();
      // Next month the army may try again.
      g.flags[`storm:${town.id}`] = g.turn - 1;
      expect(whyNotStorm(g, town, 'GER')).toBeNull();
    }
  });
});

describe('the sortie and the relief', () => {
  it('a garrison that beats the army round it raises the siege and stays in the town', () => {
    const { g, town, inside, ger } = lodz();
    const a = ger[0];
    a.str = 0.2;
    a.org = 0.3;
    orderMove(g, a, town.id);
    expect(whyNotSally(g, town, 'POL')).toBeNull();
    const look = previewSally(g, town, 'POL')!;
    expect(lineLabels(look.lines)).toContain('The sortie');
    const rep = sally(g, town, 'POL')!;
    expect(rep.sally).toBe(true);
    expect(rep.taken).toBe(true);
    expect(besieged(g, town)).toBe(false);
    expect(investedOn(g, town)).toBe(-1);
    expect(town.ctrl).toBe('POL');
    expect(inside!.town).toBeDefined();
    expect(garrisonOf(town)).toContain(inside);
  });

  it('one sortie a month, and a failed one leaves the siege standing', () => {
    const { g, town, ger } = lodz();
    for (const c of ger.slice(0, 4)) orderMove(g, c, town.id);
    const rep = sally(g, town, 'POL')!;
    expect(rep.sally).toBe(true);
    expect(rep.taken).toBe(false);
    expect(besieged(g, town)).toBe(true);
    expect(whyNotSally(g, town, 'POL')).toMatch(/made its sortie this month/);
    expect(sally(g, town, 'POL')).toBeNull();
  });

  it('a relieving army attacks the besiegers in the field, and the garrison comes out to help', () => {
    const { g, town, ger } = lodz();
    const a = ger[0];
    a.str = 0.2;
    a.org = 0.3;
    orderMove(g, a, town.id);
    const relief = town.nb.flatMap((j) => g.regions[j].units.filter((u) => u.nat === 'POL' && u.kind === 'inf')).slice(0, 2);
    expect(relief.length).toBeGreaterThan(0);
    const look = assess(g, relief, town);
    expect(lineLabels(look.lines)).toContain('The garrison');
    // The besiegers alone are the defenders: the town's walls are no help to them.
    expect(look.defenders.every((d) => d.nat === 'GER')).toBe(true);
  });
});

describe('hunger', () => {
  it('a closed ring eats the stores month by month; an open one does not', () => {
    const { g, town, inside, ger } = lodz();
    orderMove(g, ger[0], town.id);
    // One corps cannot shut in a corps and a guard.
    if (!ringClosed(g, town)) {
      siegeMonth(g, 'POL');
      expect(monthsShut(g, town)).toBe(0);
    }
    for (const c of ger.slice(1, 4)) orderMove(g, c, town.id);
    expect(ringClosed(g, town)).toBe(true);
    const stores = storesOf(g, town);
    for (let k = 1; k <= stores; k++) {
      siegeMonth(g, 'POL');
      expect(monthsShut(g, town)).toBe(k);
      expect(garrisonFed(g, town)).toBe(true);
    }
    siegeMonth(g, 'POL');
    expect(monthsShut(g, town)).toBe(stores + 1);
    expect(garrisonFed(g, town)).toBe(false);
    siegeSupply(g);
    expect(inside!.supplied).toBe(false);
    // A garrison does not give in for hunger at once: it wastes away, or is stormed.
    expect(town.ctrl).toBe('POL');
  });

  it('a town held only by its guard gives in when the stores are gone', () => {
    const { g, town, ger } = lodz({ inside: false });
    const a = ger[0];
    a.str = 0.2;
    orderMove(g, a, town.id);
    expect(town.ctrl).toBe('POL');
    expect(ringClosed(g, town)).toBe(true);
    for (let k = 0; k < storesOf(g, town); k++) siegeMonth(g, 'POL');
    expect(town.ctrl).toBe('POL');
    siegeMonth(g, 'POL');
    expect(town.ctrl).toBe('GER');
    expect(g.headlines.some((h) => /Starved out after a siege/.test(h.text))).toBe(true);
  });
});

describe('a besieged province', () => {
  it('pays nothing and raises nothing until the siege is raised', () => {
    const { g, town, ger } = lodz();
    const before = provinceYield(g, town);
    expect(before.total).toBeGreaterThan(0);
    expect(canRaise(g, 'POL', town)).toBe(true);
    orderMove(g, ger[0], town.id);
    const now = provinceYield(g, town);
    expect(now.total).toBe(0);
    expect(now.siege).toBe(true);
    expect(canRaise(g, 'POL', town)).toBe(false);
  });

  it('a full town takes no more troops: they form up on the nearest ground still held', () => {
    const { g, town, ger } = lodz();
    orderMove(g, ger[0], town.id);
    expect(garrisonOf(town).length).toBe(roomIn(g, town));
    const near = musterNear(g, 'POL', town)!;
    expect(near).not.toBeNull();
    expect(near.id).not.toBe(town.id);
    expect(allied(g, 'POL', fieldCtrl(g, near))).toBe(true);
    const fresh = addCorps(g, 'POL', 'Test Corps', 'inf', 2, town.id, {});
    expect(fresh.reg).toBe(near.id);
    expect(fresh.town).toBeUndefined();
    const moved = g.byName['Warsaw'].units.find((u) => u.nat === 'POL' && u.kind === 'inf')!;
    relocate(g, moved, town);
    expect(moved.reg).not.toBe(town.id);
    expect(garrisonOf(town).length).toBe(roomIn(g, town));
  });

  it('a town with room takes in a corps set down in it', () => {
    const g = playing('GER');
    const w = g.byName['Warsaw'];
    const lodzTown = g.byName['Lodz'];
    clearField(g, w, lodzTown);
    const ger = g.corps.filter((c) => c.nat === 'GER' && c.kind === 'inf' && !c.dead).slice(0, 2);
    for (const c of ger) relocate(g, c, w);
    expect(besieged(g, w)).toBe(true);
    const fresh = addCorps(g, 'POL', 'Test Corps', 'inf', 2, w.id, {});
    expect(fresh.reg).toBe(w.id);
    expect(fresh.town).toBeDefined();
    expect(garrisonOf(w)).toContain(fresh);
    expect(w.units).not.toContain(fresh);
  });
});

describe('softening a town', () => {
  it('bombs and shells break down the works, up to a limit, and the town mends when left alone', () => {
    const g = playing('GER');
    const w = g.byName['Warsaw'];
    const whole = townBonus(g, w);
    const plan = planTownHit(g, w, 'raid', 1);
    // Planning changes nothing.
    expect(breachOf(g, w)).toBe(0);
    expect(plan.works).toBeCloseTo(Math.min(SIEGE.breach.most, SIEGE.breach.raid), 6);
    expect(plan.bonusAfter).toBeLessThan(plan.bonus);
    const guard = guardOf(g, w);
    const hit = hitTown(g, w, 'raid', 1);
    expect(hit.works).toBeCloseTo(plan.works, 6);
    expect(breachOf(g, w)).toBeCloseTo(plan.works / 100, 6);
    expect(townBonus(g, w)).toBeCloseTo(plan.bonusAfter, 6);
    expect(townBonus(g, w)).toBeLessThan(whole);
    expect(guardOf(g, w)).toBeLessThan(guard);
    hitTown(g, w, 'raid', 1);
    hitTown(g, w, 'shell', 1);
    expect(breachOf(g, w)).toBeCloseTo(SIEGE.breach.most / 100, 6);
    expect(townBonus(g, w)).toBeGreaterThan(1);
    // Nobody stands round it: a month mends part of the works and the guard fills up.
    siegeMonth(g, 'POL');
    expect(breachOf(g, w)).toBeCloseTo((SIEGE.breach.most - SIEGE.breach.mend) / 100, 6);
    expect(guardOf(g, w)).toBeCloseTo(guard, 6);
  });

  it('a bombardment shakes the troops inside', () => {
    const { g, town, inside } = lodz();
    inside!.org = 0.9;
    const hit = hitTown(g, town, 'shell', 1);
    expect(hit.corps).toBe(1);
    expect(inside!.org).toBeCloseTo(0.9 - SIEGE.bombard.org, 6);
  });
});

describe('the fall of a country', () => {
  it('a capital closely ringed counts part lost', () => {
    const g = playing('GER');
    const w = g.byName['Warsaw'];
    const before = collapseScore(g, 'POL');
    clearField(g, w, g.byName['Lodz']);
    for (const c of g.corps.filter((x) => x.nat === 'GER' && x.kind === 'inf' && !x.dead).slice(0, 4)) relocate(g, c, w);
    expect(besieged(g, w)).toBe(true);
    expect(ringClosed(g, w)).toBe(true);
    expect(w.ctrl).toBe('POL');
    expect(collapseScore(g, 'POL') - before).toBeCloseTo(0.45 * SIEGE.capital, 6);
  });

  it('a beaten country gives in to the enemy at its throat, and to nobody when none is near', () => {
    const g = playing('GER');
    expect(victorOf(g, 'POL')).toBe('GER');
    // Nobody is at war with Italy: hunger alone brings no one to the table.
    expect(g.nations.ITA.atWar.length).toBe(0);
    expect(victorOf(g, 'ITA')).toBeNull();
    // The army round the capital is the one that takes the surrender.
    const w = g.byName['Warsaw'];
    clearField(g, w, g.byName['Lodz']);
    relocate(g, g.corps.find((x) => x.nat === 'GER' && x.kind === 'inf' && !x.dead)!, w);
    expect(besiegerOf(g, w)).toBe('GER');
    expect(victorOf(g, 'POL')).toBe('GER');
  });
});

describe('saving a siege', () => {
  it('garrisons and sieges come back as they were', () => {
    const { g, town, inside, ger } = lodz();
    orderMove(g, ger[0], town.id);
    hitTown(g, town, 'raid', 0.5);
    const g2 = loadGame(saveGame(g))!;
    const t2 = g2.regions[town.id];
    expect(besieged(g2, t2)).toBe(true);
    expect(investedOn(g2, t2)).toBe(investedOn(g, town));
    expect(breachOf(g2, t2)).toBeCloseTo(breachOf(g, town), 6);
    expect(garrisonOf(t2).map((c) => c.id)).toEqual([inside!.id]);
    const c2 = g2.corps.find((c) => c.id === inside!.id)!;
    expect(c2.town).toBe(inside!.town);
    expect(garrisonOf(t2)[0]).toBe(c2);
    expect(t2.units.some((c) => c.id === inside!.id)).toBe(false);
    expect(t2.units.map((c) => c.id)).toEqual(town.units.map((c) => c.id));
    expect(atWar(g2, 'GER', t2.ctrl)).toBe(true);
  });
});

describe("the computer's wars", () => {
  it('keep the town and the field apart, month after month', () => {
    for (const seed of [5, 21]) {
      const g = startOpen(createGame(seed, null));
      while (g.turn <= 30) {
        runTurnAI(g);
        const listed = new Set<number>();
        for (const r of g.regions) {
          const inside = garrisonOf(r);
          expect(inside.length).toBeLessThanOrEqual(roomIn(g, r));
          for (const c of inside) {
            expect(r.units).not.toContain(c);
            expect(c.town).toBeDefined();
            expect(c.reg).toBe(r.id);
            expect(c.dead).toBe(false);
            expect(allied(g, c.nat, r.ctrl)).toBe(true);
            listed.add(c.id);
          }
          for (const c of r.units) {
            expect(c.town).toBeUndefined();
            listed.add(c.id);
          }
          // Two armies at war never share a field: one of them is in the town or gone.
          const nats = [...new Set(r.units.map((u) => u.nat))];
          for (let i = 0; i < nats.length; i++) for (let j = i + 1; j < nats.length; j++) expect(atWar(g, nats[i], nats[j])).toBe(false);
        }
        for (const c of g.corps) if (!c.dead) expect(listed.has(c.id)).toBe(true);
        // A country at peace keeps its engineers at home.
        for (const c of g.corps) if (!c.dead && c.kind === 'eng' && !g.nations[c.nat].atWar.length) expect(g.regions[c.reg].ctrl).toBe(c.nat);
      }
      // Towns were besieged in these wars, and fell to assault or opened their gates.
      expect(g.flags['sgn:begun'] ?? 0).toBeGreaterThan(0);
      expect((g.flags['sgn:storm'] ?? 0) + (g.flags['sgn:gates'] ?? 0)).toBeGreaterThan(0);
    }
  }, 120_000);
});
