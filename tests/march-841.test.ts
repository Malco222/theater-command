// Marching in is the attack (Version 84.1, ruling 175(c); the grand campaign). Ordering an army onto enemy ground
// where no enemy corps stands is the order to take the place. A village, or a town that opens its gates to the
// force, falls with the move. Where only the town's guard holds out and will fight, the player sees the guard as
// the defender and the forecast before he commits: the assault as it will stand on arrival (a hasty one) and the
// same assault made ready next month. Then one order marches in and storms. No rule of battle or siege changes:
// the forecast is read off a copy of the campaign on which the march has been made.
import { describe, expect, it } from 'vitest';
import { createGame, relocate } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { SIEGE, besieged, garrison, garrisonOf, guardMen, guardOf, investedOn, townHeld } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townSize } from '../src/campaign/towns';
import { fogOn } from '../src/campaign/intel';
import { FAVOURED, beginHuman, favoured, humanUp, marchOutlook, orderMove, orderStorm, orderTake, reach, stepAI, stormChoice, stormOutlook, townSiege, verdictAt, verdictOf, whyNotStormTown } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const germansBy = (g: Game, r: Region): Corps[] =>
  r.nb.flatMap((j) => g.regions[j].units.filter((u) => u.nat === 'GER' && u.kind !== 'fort' && u.kind !== 'eng' && u.ap > 0));
function clearField(g: Game, r: Region, to: Region): void {
  for (const u of [...r.units]) relocate(g, u, to);
}
const labels = (lines: { label: string }[]): string[] => lines.map((l) => l.label);
/** A Polish city or capital, which never opens its gates to a summons, with German corps beside it and nobody in its field. */
function city(): { g: Game; town: Region; ger: Corps[] } {
  const g = playing('GER');
  const town = g.regions.find((r) => r.ctrl === 'POL' && SIEGE.summonsNot.includes(townSize(g, r)) && !garrisonOf(r).length && germansBy(g, r).length >= 3)!;
  expect(town).toBeDefined();
  clearField(g, town, g.regions.find((r) => r.ctrl === 'POL' && r.id !== town.id && !town.nb.includes(r.id))!);
  return { g, town, ger: germansBy(g, town).filter((c) => !!reach(g, c).get(town.id)) };
}
/** Germany in September 1939 with nobody in the field at Lodz: its guard alone, or one Polish corps inside. */
function lodz(inside = false): { g: Game; town: Region; ger: Corps[] } {
  const g = playing('GER');
  const town = g.byName['Lodz'];
  if (inside) expect(garrison(g, town.units.find((u) => u.nat === 'POL' && u.kind === 'inf')!)).toBe(true);
  clearField(g, town, g.byName['Warsaw']);
  return { g, town, ger: germansBy(g, town) };
}

describe('marching in is the attack', () => {
  it('where only the guard will fight, the player sees it and both forecasts before anything is ordered', () => {
    const { g, town, ger } = city();
    expect(guardOf(g, town)).toBeGreaterThan(0);
    expect(ger.length).toBeGreaterThanOrEqual(3);
    const before = saveGame(g);
    const m = marchOutlook(g, ger.slice(0, 3), town.id)!;
    // Nothing was ordered: the campaign is as it was, to the letter.
    expect(saveGame(g)).toBe(before);
    expect(m.meets).toBe('guard');
    expect(m.guardMen).toBe(guardMen(g, town));
    expect(m.guardMen).toBeGreaterThan(0);
    expect(m.going.length).toBe(3);
    expect(m.line).toMatch(/^Only its town guard, about [\d,]+ men, holds .+, and it will fight/);
    expect(labels(m.outlook!.lines)).toContain('A hasty assault');
    expect(labels(m.outlook!.lines)).toContain('The town');
    expect(labels(m.prepared!.lines)).not.toContain('A hasty assault');
    expect(m.prepared!.odds).toBeGreaterThan(m.outlook!.odds);
    expect(m.prepared!.odds / m.outlook!.odds).toBeCloseTo(1 / SIEGE.assault.hasty[townSize(g, town)], 1);
  });

  it('the forecast is the assault that is then fought: one order marches in and storms', () => {
    const { g, town, ger } = city();
    const force = ger.slice(0, 4);
    const m = marchOutlook(g, force, town.id)!;
    expect(m.meets).toBe('guard');
    const res = orderTake(g, force, town.id);
    expect(res.moved).toBe(m.going.length);
    expect(res.report).not.toBeNull();
    expect(res.report!.town).toBe(true);
    // (The day itself moves the odds a hair from any assault's card, as it always has: the forecast is the storm card's own, to the last figure, in the test below.)
    expect(Math.abs(res.report!.odds / m.outlook!.odds - 1)).toBeLessThan(0.05);
    expect(res.report!.lines.map((l) => l.label)).toContain('A hasty assault');
    expect(res.report!.defenders.some((d) => /^Town guard of /.test(d.name))).toBe(true);
    expect(res.fell).toBe(res.report!.taken);
    expect(town.ctrl).toBe(res.fell ? 'GER' : 'POL');
    expect(res.stands).toBe(res.fell ? null : 'guard');
  });

  it('the army may march in and lay the siege instead: the assault it then has in hand is the one the card showed', () => {
    const { g, town, ger } = city();
    const force = ger.slice(0, 3);
    const m = marchOutlook(g, force, town.id)!;
    const res = orderTake(g, force, town.id, false);
    expect(res).toEqual({ moved: 3, fell: false, report: null, stands: 'guard' });
    expect(besieged(g, town)).toBe(true);
    expect(investedOn(g, town)).toBe(g.turn);
    expect(stormOutlook(g, town.id)!.odds).toBeCloseTo(m.outlook!.odds, 9);
    // A month on, with nothing else changed, it is the prepared assault of the card.
    g.flags[`siege:${town.id}`] = g.turn - 1;
    expect(stormOutlook(g, town.id)!.odds).toBeCloseTo(m.prepared!.odds, 9);
    // And an army already round the town only joins the siege: there is no new card.
    expect(marchOutlook(g, germansBy(g, town), town.id)).toBeNull();
  });

  it('corps inside the town are not the guard: the march closes round it and a siege begins', () => {
    const { g, town, ger } = lodz(true);
    const m = marchOutlook(g, ger.slice(0, 2), town.id)!;
    expect(m.meets).toBe('garrison');
    expect(m.outlook).toBeNull();
    expect(m.line).toMatch(/A corps holds Lodz: your army closes round it and the siege begins/);
    const res = orderTake(g, ger.slice(0, 2), town.id);
    expect(res.report).toBeNull();
    expect(res.stands).toBe('garrison');
    expect(besieged(g, town)).toBe(true);
    expect(garrisonOf(town).length).toBe(1);
  });

  it('a place nothing holds, and a town that opens its gates, fall with the move', () => {
    const g = playing('GER');
    const park = g.byName['Warsaw'];
    let open = 0, gates = 0;
    for (const r of g.regions) {
      if (r.ctrl !== 'POL' || r.id === park.id || garrisonOf(r).length) continue;
      const ger = germansBy(g, r).filter((c) => { const e = reach(g, c).get(r.id); return !!e; });
      if (!ger.length) continue;
      clearField(g, r, park);
      const m = marchOutlook(g, ger, r.id);
      if (!m || (m.meets !== 'open' && m.meets !== 'gates')) continue;
      if (m.meets === 'open') { expect(townHeld(g, r)).toBe(false); expect(m.line).toMatch(/^Nothing holds /); open++; }
      else { expect(guardOf(g, r)).toBeGreaterThan(0); expect(m.line).toMatch(/opens its gates/); gates++; }
      expect(m.outlook).toBeNull();
      const res = orderTake(g, ger, r.id);
      expect(res.fell).toBe(true);
      expect(res.report).toBeNull();
      expect(res.stands).toBeNull();
      expect(r.ctrl).toBe('GER');
      if (open && gates) break;
    }
    expect(open + gates).toBeGreaterThan(0);
  });

  it('it is no such order where an enemy army stands in the field, on a friend\'s ground, or outside the grand campaign', () => {
    const g = playing('GER');
    const town = g.byName['Lodz'];
    const ger = germansBy(g, town);
    expect(town.units.some((u) => u.nat === 'POL')).toBe(true);
    expect(marchOutlook(g, ger, town.id)).toBeNull();
    const home = g.byName['Berlin'];
    expect(marchOutlook(g, germansBy(g, home), home.id)).toBeNull();
    const old = createGame(3, 'GER');
    expect(marchOutlook(old, old.corps.filter((c) => c.nat === 'GER').slice(0, 2), old.byName['Lodz'].id)).toBeNull();
  });

  it('the province card says so: a town with no army in it is held by its guard only, with the men', () => {
    const { g, town } = lodz();
    const v = townSiege(g, town.id)!;
    expect(v.inside).toBe(0);
    expect(v.lines[0]).toMatch(/^No army holds Lodz: only its town guard, about [\d,]+ men\./);
    const held = lodz(true);
    expect(townSiege(held.g, held.town.id)!.lines[0]).not.toMatch(/^No army holds/);
    // The assault the storm card would show is still there to be ordered the old way.
    orderMove(g, germansBy(g, town)[0], town.id);
    expect(orderStorm(g, town.id)).not.toBeNull();
  });

  it('the card lights the order its own forecast favours, and says what each order comes to (rulings 180, 182)', () => {
    const { g, town, ger } = city();
    // With fog on, the province next to your own troops is in full view: the march has its card.
    expect(fogOn(g, 'GER')).toBe(true);
    const m = marchOutlook(g, ger.slice(0, 3), town.id)!;
    expect(m.meets).toBe('guard');
    expect(m.favours).toBe(favoured(m.outlook!) ? 'storm' : 'siege');
    expect(m.ifStorm).toMatch(/^If the storm fails, your corps pay more dearly/);
    expect(m.ifSiege).toMatch(/^Nothing is risked this month\. The storm is then yours to order from the siege/);
    // A lone corps against a city's guard is not favoured: the siege is lit. (Whatever the figure, the light is the verdict's.)
    const one = marchOutlook(g, ger.slice(0, 1), town.id)!;
    expect(one.favours).toBe(verdictOf(one.outlook!, 'storm').band === 'certain' || verdictOf(one.outlook!, 'storm').band === 'favoured' ? 'storm' : 'siege');
    // Laying siege first changes nothing by itself: the storm is the player's to order from the siege, and until
    // he does the town stands. On arrival the same choice is put, with the made-ready forecast beside the wait.
    expect(orderTake(g, ger.slice(0, 3), town.id, false).stands).toBe('guard');
    expect(whyNotStormTown(g, town.id)).toBeNull();
    // (Any forecast first brings the supply map up to date after the march, as it always has: that is not an order.)
    stormOutlook(g, town.id);
    const before = saveGame(g);
    const c = stormChoice(g, town.id)!;
    expect(saveGame(g)).toBe(before);
    expect(c.hasty).toBe(true);
    expect(c.outlook).toEqual(stormOutlook(g, town.id));
    expect(c.outlook.odds).toBeCloseTo(m.outlook!.odds, 9);
    expect(c.prepared!.odds).toBeCloseTo(m.prepared!.odds, 9);
    expect(c.favours).toBe(m.favours);
    expect(c.ifWait).toMatch(/^Made ready, a month later: [\d.]+ to 1, about \d+% to carry it, if nothing else changes\.$/);
    expect(besieged(g, town)).toBe(true);
    expect(town.ctrl).toBe('POL');
    // Next month the storm is no longer hasty: the wait is then what the siege is doing, in the siege's own sentence.
    g.flags[`siege:${town.id}`] = g.turn - 1;
    const later = stormChoice(g, town.id)!;
    expect(later.hasty).toBe(false);
    expect(later.prepared).toBeNull();
    expect(later.outlook.odds).toBeCloseTo(c.prepared!.odds, 9);
    expect(townSiege(g, town.id)!.lines).toContain(later.ifWait);
    // No storm to order, no choice.
    expect(stormChoice(g, g.byName['Berlin'].id)).toBeNull();
  });

  it('the verdict is one line of the rules, the same for every card', () => {
    expect(FAVOURED).toBe(0.6);
    expect(verdictAt(0.97)).toEqual({ band: 'certain', word: 'Victory is all but certain', tone: 'win' });
    expect(verdictAt(0.97, 'sortie').word).toBe('Breaking out is all but certain');
    expect(verdictAt(0.6)).toEqual({ band: 'favoured', word: 'The odds favour you', tone: 'win' });
    expect(verdictAt(0.59)).toEqual({ band: 'even', word: 'It could go either way', tone: '' });
    expect(verdictAt(0.29)).toEqual({ band: 'long', word: 'A long shot', tone: 'loss' });
    expect(verdictAt(0)).toEqual({ band: 'none', word: 'This attack will fail', tone: 'loss' });
    expect(verdictAt(0, 'storm').word).toBe('This will fail');
    const { g, town, ger } = city();
    const look = marchOutlook(g, ger.slice(0, 3), town.id)!.outlook!;
    // Where the defence is partly a guess the verdict is the staff's, of the band.
    const band = { ...look, exact: false, chance: 0.7, chanceLo: 0.5, chanceHi: 0.9 };
    expect(verdictOf(band).word).toBe('The staff thinks the odds favour you');
    expect(favoured(band)).toBe(true);
    expect(verdictOf({ ...band, chance: 0.4, chanceLo: 0.2, chanceHi: 0.6 }).word).toBe('The staff cannot call it');
    expect(verdictOf({ ...band, chance: 0.1, chanceLo: 0, chanceHi: 0.2 }).word).toBe('The staff thinks it a long shot');
    expect(verdictOf({ ...band, chance: 0.98, chanceLo: 0.96, chanceHi: 1 }, 'storm').word).toBe('Victory looks certain');
    expect(verdictOf({ ...band, chance: 0, chanceLo: 0, chanceHi: 0 }).word).toBe('This attack looks hopeless');
    expect(favoured({ ...band, chance: 0, chanceLo: 0, chanceHi: 0 })).toBe(false);
    expect(verdictOf({ ...look, exact: true, chance: 0.61 }).band).toBe('favoured');
  });
});
