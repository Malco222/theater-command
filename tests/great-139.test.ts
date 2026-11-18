// Ruling 139: a staff holds its great cities. A great city is a capital, or a city whose fortress works stand at the
// level siege.json names (ai.great.works) or higher. With the enemy next door, a staff keeps the field round a great
// city held first and puts inside only the corps the field can spare; where the field cannot be held it fills the
// city to its room, infantry first, a lone corps and all (the design director's b512(e)). A garrison put in for a war
// that has ended comes out with the peace (b512(f)). A staff goes to the relief of a great city at a capital's odds,
// the garrison's sortie counted in them. Nothing in the siege's own rules changes, and the historical scenarios have
// no sieges at all.
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { atkPower, defPower, garrisons, reliefBar } from '../src/campaign/ai';
import { atWar, createGame, frontage, friendlyUnits, hostileUnits, relocate } from '../src/campaign/game';
import { GREAT, SIEGE, fieldCtrl, garrisonOf, greatCity, roomIn } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { townSize } from '../src/campaign/towns';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A grand campaign with Germany's first month open: Poland fights as the computer. */
function playing(seed = 3): Game {
  const g = startOpen(createGame(seed, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const field = (r: Region, nat: string): Corps[] => r.units.filter((u) => u.nat === nat && !u.dead);
/** The enemy's strength next door to a town, as the staff weighs it, and the field's against it. */
function weigh(g: Game, r: Region, nat: string): { need: number; held: number } {
  let threat = 0;
  for (const j of r.nb) {
    const e = g.regions[j];
    if (!atWar(g, nat, fieldCtrl(g, e))) continue;
    for (const p of hostileUnits(g, e, nat).filter((u) => u.kind !== 'fort').map((u) => atkPower(g, u)).sort((a, b) => b - a).slice(0, frontage(r))) threat += p;
  }
  return { need: (SIEGE.ai.great?.field ?? 0) * threat, held: friendlyUnits(g, r, nat).reduce((s, u) => s + defPower(g, u, r), 0) };
}
/** Brings the German army next door up to (or down to) a strength: 'strong' the field cannot hold, 'weak' it can. */
function door(g: Game, town: Region, how: 'strong' | 'weak'): void {
  const d = town.nb.map((j) => g.regions[j]).find((x) => x.units.some((u) => u.nat === 'GER'))!;
  if (how === 'weak') { for (const u of d.units.filter((u) => u.nat === 'GER')) u.str = 0.05; return; }
  for (const c of g.corps.filter((c) => c.nat === 'GER' && !c.dead && !c.town && c.kind === 'inf' && c.reg !== d.id).slice(0, 8)) relocate(g, c, d);
}
/** A Polish town (Warsaw unless named) with the given Polish corps alone in its field and a German corps next door. */
function warsaw(g: Game, keep: (c: Corps) => boolean, name = 'Warsaw'): Region {
  const town = g.byName[name];
  const away = town.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'POL' && !r.units.some((u) => u.nat === 'GER'))!;
  for (const c of [...town.units]) if (c.nat !== 'POL' || !keep(c)) relocate(g, c, away);
  for (const c of [...garrisonOf(town)]) c.town = undefined;
  town.garr = [];
  const door = town.nb.map((j) => g.regions[j]).find((r) => r.id !== away.id)!;
  const ger = g.corps.find((c) => c.nat === 'GER' && !c.dead && c.kind === 'inf' && !c.town)!;
  relocate(g, ger, door);
  return town;
}

// (Held back by the design director's b509 until its mend: off by default, turned on here to test the rule as written.)
beforeEach(() => { GREAT.on = true; });
afterEach(() => { GREAT.on = !!SIEGE.ai.great?.on; });

it('waits on its mend: off unless turned on (b509)', () => {
  expect(SIEGE.ai.great?.on).toBe(false);
});

describe('the great cities', () => {
  it('are the capitals and the cities with great works', () => {
    const g = playing();
    const R = (n: string): Region => g.byName[n];
    expect(SIEGE.ai.great?.works).toBe(2);
    for (const n of ['Berlin', 'Warsaw', 'Moscow', 'Paris', 'London', 'Leningrad', 'Kiev']) expect(greatCity(g, R(n))).toBe(true);
    for (const n of ['Minsk', 'Lille', 'Rhineland', 'Lodz']) expect(greatCity(g, R(n))).toBe(false);
    GREAT.on = false;
    expect(greatCity(g, R('Leningrad'))).toBe(false);
  });

  it('are not in the historical scenarios, where there are no sieges', () => {
    const g = createGame(3, 'GER');
    expect(greatCity(g, g.byName['Leningrad'])).toBe(false);
    expect(greatCity(g, g.byName['Berlin'])).toBe(false);
  });
});

describe('the field first, then the garrison (b512(e))', () => {
  it('a lone corps beside an enemy it cannot hold the field against goes into its great city', () => {
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    door(g, town, 'strong');
    expect(field(town, 'POL')).toEqual([one]);
    const { need, held } = weigh(g, town, 'POL');
    expect(held).toBeLessThan(need);
    garrisons(g, 'POL', true);
    expect(garrisonOf(town)).toContain(one);
    expect(field(town, 'POL').length).toBe(0);
  });

  it('a lone corps that holds the field stays in it', () => {
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    door(g, town, 'weak');
    const { need, held } = weigh(g, town, 'POL');
    expect(held).toBeGreaterThanOrEqual(need);
    garrisons(g, 'POL', true);
    expect(garrisonOf(town).length).toBe(0);
    expect(field(town, 'POL')).toEqual([one]);
  });

  it('while the field holds, only the corps it can spare go inside', () => {
    const g = playing();
    const town = warsaw(g, () => false);
    const five = g.corps.filter((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf').slice(0, 5);
    for (const c of five) { relocate(g, c, town); c.joint = undefined; }
    // An enemy next door the five can hold against, but not with every corps they could spare gone inside.
    const d = town.nb.map((j) => g.regions[j]).find((x) => x.units.some((u) => u.nat === 'GER'))!;
    const ger = d.units.find((u) => u.nat === 'GER')!;
    const per = defPower(g, five[0], town);
    ger.str = Math.min(1, ger.str * (3.5 * per) / (SIEGE.ai.great!.field! * atkPower(g, ger)));
    const before = weigh(g, town, 'POL');
    expect(before.held).toBeGreaterThanOrEqual(before.need);
    garrisons(g, 'POL', true);
    const after = weigh(g, town, 'POL');
    // The field still holds, and no corps left in it could have gone in without its failing.
    expect(after.held).toBeGreaterThanOrEqual(after.need);
    const room = roomIn(g, town);
    if (garrisonOf(town).length < room) for (const c of field(town, 'POL')) expect(after.held - defPower(g, c, town)).toBeLessThan(after.need);
    expect(field(town, 'POL').length).toBeGreaterThan(0);
  });

  it('without the ruling a lone corps stays in the field', () => {
    GREAT.on = false;
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    garrisons(g, 'POL', true);
    expect(garrisonOf(town).length).toBe(0);
    expect(field(town, 'POL')).toEqual([one]);
  });

  it('a great city is filled to its room, infantry first', () => {
    const g = playing();
    const town = g.byName['Warsaw'];
    const room = roomIn(g, town);
    // Room for more than the infantry: a mounted or armoured corps goes in after them.
    const inf = g.corps.filter((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf').slice(0, room - 1);
    const other = g.corps.find((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind !== 'inf' && c.kind !== 'fort' && c.kind !== 'eng')!;
    expect(other).toBeDefined();
    const spare = g.corps.filter((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf' && !inf.includes(c)).slice(0, 1);
    const keep = new Set([...inf, other, ...spare]);
    warsaw(g, () => false);
    for (const c of keep) relocate(g, c, town);
    for (const c of keep) c.joint = undefined;
    door(g, town, 'strong');
    garrisons(g, 'POL', true);
    const inside = garrisonOf(town);
    expect(inside.length).toBe(room);
    // Every infantry corps went in before the other kind did.
    for (const c of [...inf, ...spare]) expect(inside).toContain(c);
    expect(inside).not.toContain(other);
    expect(field(town, 'POL')).toEqual([other]);
  });

  it('a city that is not great keeps one corps in the field, as before', () => {
    const g = playing();
    const one = g.corps.find((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf')!;
    const silesia = warsaw(g, () => false, 'Polish Silesia');
    expect(townSize(g, silesia)).toBe('city');
    expect(greatCity(g, silesia)).toBe(false);
    relocate(g, one, silesia);
    garrisons(g, 'POL', true);
    expect(garrisonOf(silesia).length).toBe(0);
    expect(field(silesia, 'POL')).toEqual([one]);
    // With a second corps beside it, one goes in and one stays out.
    const two = g.corps.find((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf' && c !== one)!;
    relocate(g, two, silesia);
    garrisons(g, 'POL', true);
    expect(garrisonOf(silesia).length).toBe(1);
    expect(field(silesia, 'POL').length).toBe(1);
  });
});

describe('an attacker does not shut its attackers in (b519(m))', () => {
  /** Poland as the side that began the war, its staff's offensive against Germany on (as the Red Army's in Karelia, 1939). */
  const attacking = (g: Game): void => {
    delete g.flags['war:GER:POL'];
    g.flags['war:POL:GER'] = g.turn;
    g.flags['st:on:POL:GER'] = g.turn;
  };
  /** The enemy next door attacks the province: a battle there with Germany as the attacker. */
  const struck = (g: Game, town: Region): void => { g.reports.push({ turn: g.turn, region: town.id, attNat: 'GER', defNat: 'POL' } as Game['reports'][number]); };

  it('a lone corps beside an enemy it cannot hold the field against stays out while its staff is on the attack', () => {
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    door(g, town, 'strong');
    attacking(g);
    garrisons(g, 'POL', true);
    expect(garrisonOf(town).length).toBe(0);
    expect(field(town, 'POL')).toEqual([one]);
  });

  it('once the enemy next door has attacked the province, the lone corps goes in', () => {
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    door(g, town, 'strong');
    attacking(g);
    struck(g, town);
    garrisons(g, 'POL', true);
    expect(garrisonOf(town)).toContain(one);
  });

  it('the city is garrisoned as any other city, not filled to its room', () => {
    const g = playing();
    const town = warsaw(g, () => false);
    const five = g.corps.filter((c) => c.nat === 'POL' && !c.dead && !c.town && c.kind === 'inf').slice(0, 5);
    for (const c of five) { relocate(g, c, town); c.joint = undefined; }
    door(g, town, 'strong');
    attacking(g);
    garrisons(g, 'POL', true);
    // (Without the attack the five would fill the city's room: b512(e), the field cannot be held.)
    expect(garrisonOf(town).length).toBe(SIEGE.ai.hold[townSize(g, town)]);
    expect(garrisonOf(town).length).toBeLessThan(roomIn(g, town));
    expect(field(town, 'POL').length).toBeGreaterThan(0);
  });

  it('a war the enemy began is not the attacker\'s: the lone corps goes in as before', () => {
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    door(g, town, 'strong');
    // Germany declared war on Poland; a Polish counter-offensive does not make the war Poland's own.
    g.flags['st:on:POL:GER'] = g.turn;
    garrisons(g, 'POL', true);
    expect(garrisonOf(town)).toContain(one);
  });
});

describe('the peace (b512(f))', () => {
  it('a garrison put in for a war that has ended comes out with it', () => {
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    door(g, town, 'strong');
    garrisons(g, 'POL', true);
    expect(garrisonOf(town)).toContain(one);
    // Poland's war ends (as the Soviet Union's with Finland did in March 1940).
    for (const n of Object.values(g.nations)) n.atWar = n.atWar.filter((x) => x !== 'POL');
    g.nations.POL.atWar = [];
    garrisons(g, 'POL', false);
    expect(garrisonOf(town).length).toBe(0);
    expect(field(town, 'POL')).toContain(one);
  });

  it('without the ruling the garrison stays inside through the peace, as before', () => {
    const g = playing();
    const one = field(g.byName['Warsaw'], 'POL').find((c) => c.kind === 'inf')!;
    const town = warsaw(g, (c) => c === one);
    door(g, town, 'strong');
    garrisons(g, 'POL', true);
    expect(garrisonOf(town)).toContain(one);
    GREAT.on = false;
    for (const n of Object.values(g.nations)) n.atWar = n.atWar.filter((x) => x !== 'POL');
    g.nations.POL.atWar = [];
    garrisons(g, 'POL', false);
    expect(garrisonOf(town)).toContain(one);
  });
});

describe('the relief', () => {
  it("a great city is relieved at its capital's odds, any other town at the usual", () => {
    const g = playing();
    expect(reliefBar(g, g.byName['Leningrad'])).toBe(SIEGE.ai.relief.capital);
    expect(reliefBar(g, g.byName['Warsaw'])).toBe(SIEGE.ai.relief.capital);
    expect(reliefBar(g, g.byName['Minsk'])).toBe(SIEGE.ai.relief.other);
    GREAT.on = false;
    expect(reliefBar(g, g.byName['Leningrad'])).toBe(SIEGE.ai.relief.other);
    expect(reliefBar(g, g.byName['Warsaw'])).toBe(SIEGE.ai.relief.capital);
  });
});
