// Ruling 232: beat the army, take the city. On Brian's words of 2026-11-18, playing America against Panama: "if i
// drive the army out of a city then i should conquer it. if the army that loses retreats than they should be out in
// the country and i can attack them next turn". Behind the switch `driveOut` (siege.json), which this file turns on:
// (a) a field army beaten in a province never goes into its town, not even its own capital: it falls back to open
// country next door; (b) with no ground of its side to go to and no ship to lift it, it lays down its arms; (c) where
// no garrison stands in the town, its guard fights in the same battle, behind its walls, on the attack card and in the
// odds, and when the field is won the town falls with the province in that one order; (d) a garrison ordered in
// beforehand still holds the town after the field is lost. History campaigns are fought as they always were.
import { beforeEach, describe, expect, it, vi } from 'vitest';
vi.hoisted(() => { (globalThis as { TC_DRIVE?: unknown }).TC_DRIVE = 1; });
import { addCorps, createGame, men, relocate, setWar, sovietEvent } from '../src/campaign/game';
import { DRIVE, besieged, driveOut, garrison, garrisonOf, guardInField, guardMen, guardOf, guardWorth, opensTo, siegeView } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { attackOutlook, beginHuman, candidates, humanUp, orderAttack, previewAttack, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

function seat(nat: string, seed = 3): Game {
  const g = startOpen(createGame(seed, nat));
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const field = (g: Game, r: Region, nat: string): Corps[] => r.units.filter((u) => u.nat === nat && !u.dead);
const fateOf = (rep: { defenders: { name: string; fate: string }[] }, name: string): string | undefined => rep.defenders.find((u) => u.name === name)?.fate;

beforeEach(() => { DRIVE.on = true; });

/** America at war with Panama, with an army in the Canal Zone strong enough to win the field for certain. */
function panama(): { g: Game; city: Region; police: Corps } {
  const g = seat('USA');
  setWar(g, 'USA', 'PAN');
  const city = g.byName['Panama City'];
  const police = field(g, city, 'PAN')[0];
  expect(police.name).toBe('National Police of Panama');
  for (let k = 0; k < 2; k++) addCorps(g, 'USA', `Test Corps ${k}`, 'inf', 3, 'Canal Zone', {});
  return { g, city, police };
}

describe("ruling 232: beat the army, take the city", () => {
  it('Panama: the police, beaten with nowhere of their own to go, lay down their arms, and Panama City is American that month', () => {
    const { g, city, police } = panama();
    expect(driveOut(g)).toBe(true);
    expect(garrisonOf(city)).toHaveLength(0);
    // (The computer's months before his first have already taken towns with the field: Poland's, in September.)
    const fell = g.flags['sgn:field'] ?? 0;
    const a = previewAttack(g, candidates(g, city), city)!;
    expect(a.odds * a.dice[0]).toBeGreaterThanOrEqual(1.2);
    // A small country's guard is part of a division, never "0 divisions' worth".
    expect(a.guard!).toBeLessThan(0.95);
    expect(a.lines.find((l) => l.label === 'The town')!.value).toBe("Panama City is held by its own guard: part of a division's worth, behind its walls");
    const rep = orderAttack(g, candidates(g, city), city)!;
    expect(rep.taken).toBe(true);
    // Its only ground beside the city is America's Canal Zone and Colombia, not in its war, and it has no ships.
    expect(fateOf(rep, 'National Police of Panama')).toBe('surrendered');
    expect(rep.defenders.find((u) => u.name === 'National Police of Panama')!.cut).toBe(true);
    expect(rep.defenders.find((u) => u.guard)!.divs).toBeGreaterThan(0);
    expect(police.dead).toBe(true);
    expect(rep.defenders.some((u) => u.fate === 'sheltered')).toBe(false);
    expect(city.ctrl).toBe('USA');
    expect(besieged(g, city)).toBe(false);
    expect(rep.siege).toBeFalsy();
    expect(g.flags['sgn:field']).toBe(fell + 1);
  });

  it('the switch off: the same police go into the town and Panama City is besieged, as before the ruling', () => {
    DRIVE.on = false;
    const { g, city } = panama();
    expect(driveOut(g)).toBe(false);
    const rep = orderAttack(g, candidates(g, city), city)!;
    expect(fateOf(rep, 'National Police of Panama')).toBe('sheltered');
    expect(city.ctrl).toBe('PAN');
    expect(besieged(g, city)).toBe(true);
  });

  it('a capital with a way out: its army falls back to open country, and the town falls with its guard', () => {
    const g = seat('GER');
    const warsaw = g.byName['Warsaw'], masuria = g.byName['Masuria'];
    expect(masuria.ctrl).toBe('GER');
    // (Warsaw opens with its Defence Command inside (rider b566). Sent away to Lublin, it leaves the city to its guard.)
    expect(garrisonOf(warsaw).map((c) => c.name)).toEqual(['Warsaw Defence Command']);
    for (const c of [...garrisonOf(warsaw)]) relocate(g, c, g.byName['Lublin']);
    expect(garrisonOf(warsaw)).toHaveLength(0);
    const poles = field(g, warsaw, 'POL');
    expect(poles.length).toBeGreaterThan(0);
    for (const c of poles) { c.str = 0.6; c.org = 0.2; }
    // Most of the guard is already out of action: the battle is the field army and the rest of it.
    g.flags[`guard:${warsaw.id}`] = 80;
    const guard = guardOf(g, warsaw), guardSaid = guardMen(g, warsaw);
    expect(guard).toBeGreaterThan(0.5);
    for (let k = 0; k < 4; k++) addCorps(g, 'GER', `Test Corps ${k}`, 'inf', 3, 'Masuria', {});
    const a = previewAttack(g, candidates(g, warsaw), warsaw)!;
    expect(a.guard).toBeCloseTo(guard, 6);
    expect(a.odds * a.dice[0]).toBeGreaterThanOrEqual(1.2);
    const rep = orderAttack(g, candidates(g, warsaw), warsaw)!;
    expect(rep.taken).toBe(true);
    expect(rep.guarded).toBe(true);
    // The field army is out in the country next door, with its month spent, where it can be caught next month.
    for (const c of poles) {
      expect(c.dead).toBe(false);
      expect(c.town).toBeUndefined();
      expect(c.reg).not.toBe(warsaw.id);
      expect(g.regions[c.reg].ctrl).toBe('POL');
      expect(c.ap).toBe(0);
      expect(fateOf(rep, c.name)).toBe('retreated');
      // The report keeps where it went, for the words "The Polish army falls back to ...".
      expect(rep.defenders.find((u) => u.name === c.name)!.to).toBe(c.reg);
    }
    // The guard is taken with the field, and the capital is German that month.
    const row = rep.defenders.find((u) => u.guard)!;
    expect(row).toMatchObject({ name: 'Town guard of Warsaw', nat: 'POL', fate: 'surrendered', men: guardSaid, lost: guardSaid });
    expect(rep.defenders.indexOf(row)).toBe(rep.defenders.length - 1);
    expect(rep.prisoners).toBeGreaterThanOrEqual(guardSaid);
    expect(warsaw.ctrl).toBe('GER');
    expect(besieged(g, warsaw)).toBe(false);
    expect(g.headlines.some((h) => /Warsaw falls to German forces/.test(h.text))).toBe(true);
  });

  it('a garrisoned town: the field is won, the garrison holds, and the town is besieged', () => {
    const g = seat('GER');
    const lodz = g.byName['Lodz'];
    const [inside, outside] = [addCorps(g, 'POL', 'Inside Corps', 'inf', 1, 'Lodz', {}), field(g, lodz, 'POL')[0]];
    expect(garrison(g, inside)).toBe(true);
    expect(garrisonOf(lodz)).toEqual([inside]);
    for (const c of field(g, lodz, 'POL')) { c.str = 0.6; c.org = 0.2; }
    for (let k = 0; k < 4; k++) addCorps(g, 'GER', `Test Corps ${k}`, 'inf', 3, 'Upper Silesia', {});
    const a = previewAttack(g, candidates(g, lodz), lodz)!;
    // A garrison is a separate fight: the guard does not join the field battle, and the card says what follows.
    expect(a.guard).toBeUndefined();
    expect(a.lines.find((l) => l.label === 'The town')!.effect).toBe('Winning here wins the country round it. The garrison holds the town after: it must then be stormed or starved out');
    expect(a.odds * a.dice[0]).toBeGreaterThanOrEqual(1.2);
    const rep = orderAttack(g, candidates(g, lodz), lodz)!;
    expect(rep.taken).toBe(true);
    expect(rep.siege).toBe(true);
    expect(rep.guarded).toBeFalsy();
    expect(fateOf(rep, outside.name)).toBe('retreated');
    expect(garrisonOf(lodz)).toEqual([inside]);
    expect(lodz.ctrl).toBe('POL');
    expect(besieged(g, lodz)).toBe(true);
  });

  it('shows the guard on the attack card as a defender, in the odds and the ticket, known or not', () => {
    // (A city: its guard never opens its gates at a summons, so it always stands in the field, b540.)
    const g = seat('GER');
    const krakow = g.byName['Krakow'];
    expect(garrisonOf(krakow)).toHaveLength(0);
    const going = candidates(g, krakow);
    const on = previewAttack(g, going, krakow)!;
    expect(on.guard).toBeCloseTo(guardOf(g, krakow), 6);
    const said = guardMen(g, krakow).toLocaleString('en-US');
    expect(on.lines.find((l) => l.label === 'Forces')!.value).toMatch(new RegExp(`, and the town guard of about ${said}$`));
    expect(on.lines.find((l) => l.label === 'The town')).toMatchObject({
      value: "Krakow is held by its own guard: 2.0 divisions' worth, behind its walls",
      effect: 'The guard fights beside the army, 70% stronger behind the walls. Win, and the town falls with the country round it',
    });
    const look = attackOutlook(g, going, krakow)!;
    expect(look.lines.find((l) => l.label === 'Forces')!.value).toMatch(new RegExp(`, and the town guard of about ${said}$`));
    // The same attack with the rule off faces the field army alone.
    DRIVE.on = false;
    const off = previewAttack(g, going, krakow)!;
    expect(off.guard).toBeUndefined();
    expect(on.D).toBeGreaterThan(off.D);
    expect(on.odds).toBeLessThan(off.odds);
    expect(on.prepared).toBeCloseTo(off.prepared, 9);
  });

  it('a town whose guard would open its gates to this army sends no guard to the field, and opens at the summons after the battle (b540)', () => {
    const g = seat('GER');
    const lodz = g.byName['Lodz'];
    expect(garrisonOf(lodz)).toHaveLength(0);
    const going = candidates(g, lodz);
    expect(opensTo(g, lodz, going)).toBe(true);
    expect(guardInField(g, lodz, 'GER', going)).toBe(0);
    expect(guardInField(g, lodz, 'GER')).toBeCloseTo(guardOf(g, lodz), 9);
    const a = previewAttack(g, going, lodz)!;
    expect(a.guard).toBeUndefined();
    DRIVE.on = false;
    const off = previewAttack(g, going, lodz)!;
    DRIVE.on = true;
    expect(a.D).toBeCloseTo(off.D, 9);
    const rep = orderAttack(g, going, lodz)!;
    expect(rep.guarded).toBeFalsy();
    expect(rep.taken).toBe(true);
    expect(rep.gates).toBe(true);
    expect(lodz.ctrl).toBe('GER');
    expect(besieged(g, lodz)).toBe(false);
  });

  it('a field attack thrown back: the guard pays its share, as a storm thrown back does, and the town stands', () => {
    const g = seat('GER');
    const lodz = g.byName['Lodz'];
    const before = guardOf(g, lodz);
    const one = g.byName['Upper Silesia'].units.find((u) => u.nat === 'GER' && u.kind === 'inf' && !u.dead) ?? addCorps(g, 'GER', 'Lone Corps', 'inf', 1, 'Upper Silesia', {});
    one.str = 0.2;
    const a = previewAttack(g, [one], lodz)!;
    expect(a.odds * a.dice[1]).toBeLessThan(1.2);
    const rep = orderAttack(g, [one], lodz)!;
    expect(rep.taken).toBe(false);
    expect(rep.guarded).toBe(true);
    const row = rep.defenders.find((u) => u.guard)!;
    expect(row.fate).toBe('');
    expect(row.lost).toBeGreaterThan(0);
    expect(guardOf(g, lodz)).toBeLessThan(before);
    expect(lodz.ctrl).toBe('POL');
  });

  it('the province card says what the guard and a garrison do', () => {
    const g = seat('GER');
    const lodz = g.byName['Lodz'];
    expect(siegeView(g, lodz.id)!.lines.join(' ')).toContain('With no garrison inside, it fights beside the army in the field, and the town falls with the field.');
    const c = field(g, lodz, 'POL')[0];
    expect(garrison(g, addCorps(g, 'POL', 'Inside Corps', 'inf', 1, 'Lodz', {}))).toBe(true);
    expect(c.dead).toBe(false);
    expect(siegeView(g, lodz.id)!.lines).toContain('The garrison will hold the town after the field is lost: it must then be stormed or starved out.');
  });

  it("says a guard's size in words, never 0", () => {
    expect(guardWorth(0.024)).toBe("part of a division's worth");
    expect(guardWorth(1)).toBe("1 division's worth");
    expect(guardWorth(2.37)).toBe("2.4 divisions' worth");
  });

  it('a guard-only town already besieged: its guard does not fight beside the besiegers, and the card says it holds the town', () => {
    const g = seat('GER');
    setWar(g, 'GER', 'POL');
    const lodz = g.byName['Lodz'];
    for (const u of [...lodz.units]) relocate(g, u, g.byName['Warsaw']);
    expect(garrisonOf(lodz)).toHaveLength(0);
    expect(guardInField(g, lodz, 'GER')).toBeGreaterThan(0);
    const ger = addCorps(g, 'GER', 'Ring Corps', 'inf', 2, 'Upper Silesia', {});
    relocate(g, ger, lodz);
    expect(besieged(g, lodz)).toBe(true);
    // A third power at war with both, attacking the besiegers, fights them alone, and takes the siege over if it wins.
    setWar(g, 'SOV', 'POL');
    expect(guardInField(g, lodz, 'SOV')).toBe(0);
    const lines = siegeView(g, lodz.id)!.lines.join(' ');
    expect(lines).toContain('It holds the town while no army does.');
    expect(lines).not.toContain('fights beside the army in the field');
  });

  it('leaves a history campaign as it always was: no town rules, no guard in the field', () => {
    const g = createGame(3, 'GER');
    expect(driveOut(g)).toBe(false);
    const lodz = g.byName['Lodz'];
    const going = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind === 'inf' && lodz.nb.includes(c.reg));
    if (going.length) expect(previewAttack(g, going, lodz)?.guard).toBeUndefined();
    expect(men(field(g, lodz, 'POL')[0])).toBeGreaterThan(0);
  });

  it("Warsaw holds out with its Defence Command, and the Red Army still comes into its zone: what Germany overran is handed over (riders b566, b571)", () => {
    const g = startOpen(createGame(3, null));
    const warsaw = g.byName['Warsaw'];
    expect(garrisonOf(warsaw).map((c) => c.name)).toEqual(['Warsaw Defence Command']);
    // (The Red Army is held back for a month, to see the zone as Germany's first month leaves it.)
    g.flags.sovietMove = 1;
    runTurnAI(g);
    delete g.flags.sovietMove;
    // Germany's first month takes the field everywhere, the Soviet zone too; Warsaw stands, so Poland does.
    expect(g.nations.POL.alive).toBe(true);
    expect(warsaw.ctrl).toBe('POL');
    const overrun = g.regions.filter((r) => r.sov && r.owner === 'POL' && r.ctrl === 'GER');
    expect(overrun.length).toBeGreaterThan(0);
    const sovietsBefore = g.corps.filter((c) => c.nat === 'SOV' && !c.dead && g.regions[c.reg].sov).length;
    // A player's Germany keeps what it took: the pact moves no ground and no corps of his.
    const h = structuredClone(g);
    h.human = 'GER';
    const theirs = h.regions.filter((r) => r.sov && r.owner === 'POL' && r.ctrl === 'GER').map((r) => r.id);
    sovietEvent(h);
    for (const id of theirs) expect(h.regions[id].ctrl, h.regions[id].name).toBe('GER');
    // The computer's Germany draws back to the line, and the Red Army takes up its places.
    sovietEvent(g);
    for (const r of overrun) {
      expect(r.ctrl, r.name).toBe('SOV');
      expect([...r.units, ...garrisonOf(r)].some((c) => c.nat !== 'SOV'), r.name).toBe(false);
    }
    for (const name of ['Wilno', 'Brest', 'Lwow', 'Volhynia', 'Bialystok', 'Polesie']) expect(g.byName[name].ctrl, name).toBe('SOV');
    expect(g.corps.filter((c) => c.nat === 'SOV' && !c.dead && g.regions[c.reg].sov).length).toBeGreaterThan(sovietsBefore);
    expect(g.corps.filter((c) => c.nat === 'GER' && !c.dead).every((c) => !g.regions[c.reg].sov || g.regions[c.reg].ctrl !== 'SOV')).toBe(true);
    expect(warsaw.ctrl).toBe('POL');
  });
});
