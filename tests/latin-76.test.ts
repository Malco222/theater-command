// Version 76, the grand campaign only.
// 1. The landing card's two amber words (the design director's rulings 36 and 37). The sea behind a beach has
//    three words now: "stays open" when the invader has there at least all the enemy could bring within a month,
//    "contested" when he has at least half of it and less than all, and "can be closed" under half. A landing that
//    takes a whole island where the sea can be closed is told what it is: a force on its own, which holds out for
//    about three months on what it carries. A staff goes on heeding the red warning only.
// 2. Latin America on the chart: Central America, the Caribbean and South America to Cape Horn, drawn after North
//    America's provinces, which keep their numbers. Six countries a player can take (Brazil, Argentina, Chile,
//    Peru, Colombia, Venezuela) and twelve small republics; the Canal on its own ground; colonies that pass into a
//    sea power's keeping when the mother country falls; history's entries into the war; a campaign saved on the
//    smaller chart loads on this one.
// 3. Two faults the Playtester found in Version 75: the American short campaign could be won in the first month,
//    and once the Soviet Union was in the war the list of crossings lost every shore of the continent.
import { describe, expect, it } from 'vitest';
import grandData from '../data/campaign/grand.json';
import { DIPLOMACY, friendsOf, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { income } from '../src/campaign/economy';
import { addCorps, allied, atWar, bloc, capitulate, capture, createGame, monthsUnfed, removeCorps, setWar } from '../src/campaign/game';
import { GOALS, goalState, goalWhen, judgeGoals, openGoals } from '../src/campaign/goals';
import { formOf } from '../src/campaign/government';
import { founders, pathOptions, small } from '../src/campaign/league';
import { NAVY, canPass, crossingsOf, numberWord, sail, seaHold, seaSide, surfaceShips, type CrossingOutlook } from '../src/campaign/navy';
import { onNet, railsAt } from '../src/campaign/rails';
import { crossingOf, riverName } from '../src/campaign/rivers';
import { loadGame, peekSave, saveGame, staleSave } from '../src/campaign/save';
import { heedsCard, startOpen } from '../src/campaign/staff';
import { dealsOf, housesOf } from '../src/campaign/trade';
import { TRAITS, traitsOf } from '../src/campaign/traits';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { FLAGS } from '../src/campaign/ui/flags';
import { WORLD, WORLD_FIRST, WORLD_MAP, WORLD_REGIONS, WORLD_STAGES, armyIdle, beyondEurope } from '../src/campaign/world';

describe('the sea behind the beach has three words (rulings 36 and 37)', () => {
  /** A war the computer plays on every side, as it stands once Poland has fallen, with every enemy fleet of Germany's in harbour. */
  const opened = (): Game => {
    const g = startOpen(createGame(3, null));
    for (let k = 0; k < 8 && (k < 2 || g.nations.POL.alive); k++) runTurnAI(g);
    for (const f of g.fleets) if (atWar(g, f.nat, 'GER')) sail(g, f, f.home, 'port');
    return g;
  };
  const toBritain = (g: Game): CrossingOutlook => crossingsOf(g, 'GER').find((x) => x.owner === 'UK' && x.home)!;
  const word = (h: ReturnType<typeof seaHold>): { value: string; effect: string } => {
    const l = h.lines.find((x) => x.label === 'The sea behind the beach')!;
    return { value: String(l.value), effect: l.effect ?? '' };
  };
  /** A German fleet stood in that sea and grown until Germany has `want` there. */
  const grow = (g: Game, sea: number, want: () => number): void => {
    const f = g.fleets.find((k) => k.nat === 'GER' && surfaceShips(k) > 0)!;
    f.base = -1; f.home = sea; f.zone = sea; f.stance = 'patrol';
    for (let n = 0; n < 2000 && seaSide(g, sea, 'GER', false).total <= want(); n++) f.ships.bb += 2;
  };

  it('says "can be closed" under half of what the enemy could bring', () => {
    const g = opened();
    const x = toBritain(g);
    const h = x.account.hold;
    expect(h.foe).toBeGreaterThanOrEqual(NAVY.closed.min);
    expect(h.own.total).toBeLessThan(h.foe / 2);
    expect(h.open).toBe(false);
    expect(h.contested).toBe(false);
    expect(h.island).toBe(false);
    expect(word(h)).toEqual({ value: 'can be closed', effect: 'the force ashore would be cut off from its supplies' });
  });

  it('says "contested" with at least half and less than all of it, and the sea still counts as open', () => {
    const g = opened();
    const x = toBritain(g);
    const before = x.account.hold;
    grow(g, before.sea, () => before.need * 1.1);
    const h = seaHold(g, 'GER', x.path, x.to);
    expect(h.own.total).toBeGreaterThan(h.foe / 2);
    expect(h.own.total).toBeLessThan(h.foe);
    expect(h.open).toBe(true);
    expect(h.contested).toBe(true);
    expect(word(h)).toEqual({ value: 'contested', effect: 'yours today, but he can bring enough to fight you for it' });
    // The line above it says both figures: what keeps it open today, and what he can bring.
    const mine = h.lines.find((l) => l.label.startsWith('Your ships and aircraft'))!;
    expect(mine.effect).toContain('keep it open today');
    expect(mine.effect).toContain('he can bring');
  });

  it('says "stays open" once the invader has there all the enemy could bring', () => {
    const g = opened();
    const x = toBritain(g);
    const sea = x.account.hold.sea;
    grow(g, sea, () => seaHold(g, 'GER', x.path, x.to).foe * 1.02);
    const h = seaHold(g, 'GER', x.path, x.to);
    expect(h.own.total).toBeGreaterThanOrEqual(h.foe);
    expect(h.open).toBe(true);
    expect(h.contested).toBe(false);
    expect(word(h)).toEqual({ value: 'stays open', effect: 'the supply ships reach the force' });
  });

  it('is never contested where the enemy has too little in reach to close any sea', () => {
    const g = opened();
    // Sweden's coast across the Baltic: no enemy of Germany's has a fleet that could come there.
    const baltic = g.byName['Stockholm'].seas[0];
    const h = seaHold(g, 'GER', [baltic], g.byName['Stockholm']);
    expect(h.foe).toBeLessThan(NAVY.closed.min);
    expect(h.need).toBe(0);
    expect(h.open).toBe(true);
    expect(h.contested).toBe(false);
  });

  it('tells a landing that takes a whole island how long it can hold out', () => {
    const g = opened();
    const malta = g.byName['Malta'];
    // An island: a province with no land frontier.
    expect(malta.nb.length).toBe(0);
    const h = seaHold(g, 'GER', [malta.seas[0]], malta);
    expect(h.island).toBe(true);
    expect(h.months).toBe(3);
    expect(h.foe).toBeGreaterThanOrEqual(NAVY.closed.min);
    expect(h.open).toBe(false);
    expect(word(h)).toEqual({ value: 'can be closed', effect: 'cut off, the force can hold out for about three months on what it carries' });
    // A beach on the mainland is no island.
    expect(seaHold(g, 'GER', toBritain(g).path, toBritain(g).to).island).toBe(false);
  });

  it('counts the months from the rule for a corps cut off from its supplies', () => {
    // A fresh corps loses four tenths of its order each month unfed, and lays down its arms when none is left.
    expect(monthsUnfed()).toBe(3);
    expect(monthsUnfed(0.8)).toBe(2);
    expect(monthsUnfed(0.3)).toBe(1);
    expect(numberWord(3)).toBe('three');
    expect(numberWord(12)).toBe('12');
  });

  it('leaves what a staff heeds as it was: red on the mainland only', () => {
    const g = opened();
    expect(heedsCard(toBritain(g).to)).toBe(true);
    expect(heedsCard(g.byName['Malta'])).toBe(false);
  });
});

const LARGE = ['BRA', 'ARG', 'CHL', 'PER', 'COL', 'VEN'];
const SMALL = ['GUA', 'ELS', 'HON', 'NIC', 'COS', 'PAN', 'HAI', 'DOM', 'ECU', 'BOL', 'PAR', 'URU'];
const LATIN = [...LARGE, ...SMALL];
const wide = (seed = 1, human: string | null = null): Game => startOpen(createGame(seed, human));
const DEC_41 = 28, AUG_42 = 36;

describe('Latin America is on the chart', () => {
  it('is drawn after North America, whose provinces and seas keep their numbers', () => {
    const g = wide();
    // (The cities version lays the Americas' cities after them: the chart's stages are 104 and 201, then its end.)
    expect(WORLD_STAGES).toEqual([104, 201]);
    expect(WORLD_REGIONS).toBe(540);
    expect(g.regions.length).toBe(WORLD_FIRST + 540);
    expect(g.regions[WORLD_FIRST + 103].name).toBe('Southern Baja');
    expect(g.regions[WORLD_FIRST + 104].name).toBe('Guatemala City');
    expect(g.regions[WORLD_FIRST + 200].name).toBe('Rio Grande do Sul');
    // Then the cities, each cut from a province of the 201 (the cities version).
    expect((WORLD_MAP.regions as { of?: number }[]).slice(201).every((r) => r.of !== undefined && r.of >= WORLD_FIRST && r.of < WORLD_FIRST + 201)).toBe(true);
    expect(new Set(g.regions.map((r) => r.name)).size).toBe(g.regions.length);
    // The chart runs down to 56 degrees south at the scale of the rest: the whole height the 3D game was promised.
    expect(WORLD_MAP.frame.y1 - WORLD_MAP.frame.y0).toBe(5096);
    expect(WORLD_MAP.world.height).toBe(5096);
    expect(g.height).toBe(5096);
    // The seas of Versions 74 and 75 keep their places, and the new ones follow them.
    expect(g.seas.length).toBe(67);
    expect(g.seas[43].key).toBe('scotian');
    expect(g.seas.slice(44).map((s) => s.key)).toEqual(['panama', 'guayaquil', 'callao', 'atacama', 'valparaiso', 'fjords', 'eqpacific', 'sepacific', 'guiana', 'amazon', 'narrows',
      'bahia', 'santos', 'plate', 'argentine', 'falklands', 'southatlantic', 'sierra', 'guinea', 'angola', 'cape', 'mozambique', 'aden']);
    // The two Americas are one land mass, joined through the isthmus; the frontiers are the true ones.
    // (Ground to ground: a province with the cities cut from it, since the cities version.)
    const ground = (n: string): number[] => [g.byName[n].id, ...(WORLD_MAP.regions as { of?: number }[]).map((r, i) => (r.of === g.byName[n].id ? WORLD_FIRST + i : -1)).filter((i) => i >= 0)];
    const nb = (a: string, b: string): boolean => ground(a).some((x) => g.regions[x].nb.some((y) => ground(b).includes(y)));
    for (const [a, b] of [['Chiapas', 'Guatemala City'], ['Yucatan', 'British Honduras'], ['Chiriqui', 'Canal Zone'], ['Canal Zone', 'Panama City'], ['Panama City', 'Medellin'],
      ['Caqueta', 'Loreto'], ['Arequipa', 'Tarapaca'], ['Mendoza', 'Santiago'], ['Tacuarembo', 'Rio Grande do Sul'], ['Guayana', 'British Guiana']]) expect(nb(a, b), `${a} and ${b}`).toBe(true);
    // Chile and Brazil do not touch; Ecuador and Brazil do not; Chiriqui reaches Panama City only through the Zone.
    expect(g.regions.some((r) => r.owner === 'CHL' && r.nb.some((j) => g.regions[j].owner === 'BRA'))).toBe(false);
    expect(g.regions.some((r) => r.owner === 'ECU' && r.nb.some((j) => g.regions[j].owner === 'BRA'))).toBe(false);
    expect(nb('Chiriqui', 'Panama City')).toBe(false);
    // Islands are islands, and the Strait of Magellan is a crossing an army can make without a fleet.
    for (const name of ['Trinidad', 'Curacao', 'Martinique', 'Falkland Islands']) expect(g.byName[name].nb, name).toEqual([]);
    expect(nb('Magallanes', 'Tierra del Fuego') && nb('Southern Patagonia', 'Tierra del Fuego')).toBe(true);
    expect(g.links.some(([a, b]) => [a, b].includes(g.byName['Tierra del Fuego'].id))).toBe(true);
    // Every one of the new provinces is beyond Europe, has an owner that is a country, and a coast only on seas that know it.
    for (let i = WORLD_FIRST + 104; i < g.regions.length; i++) {
      const r = g.regions[i];
      expect(beyondEurope(r)).toBe(true);
      expect(g.nations[r.owner], `${r.name} of ${r.owner}`).toBeDefined();
      for (const z of r.seas) expect(g.seas[z].coast, `${r.name} on the ${g.seas[z].name}`).toContain(i);
    }
  });

  it('has the great rivers, and railways only where history had laid them', () => {
    const g = wide();
    const id = (name: string): number => g.byName[name].id;
    // A province with the cities cut from it: since the cities version a river or a line may run from a city.
    const ground = (n: string): number[] => [id(n), ...(WORLD_MAP.regions as { of?: number }[]).map((r, i) => (r.of === id(n) ? WORLD_FIRST + i : -1)).filter((i) => i >= 0)];
    const river = (a: string, b: string): string | null => { for (const x of ground(a)) for (const y of ground(b)) { const c = crossingOf(x, y); if (c) return riverName(c); } return null; };
    expect(river('Amazonas', 'Rio Negro')).toBe('Amazon');
    expect(river('Para', 'Amapa')).toBe('Amazon');
    expect(river('Asuncion', 'Gran Chaco')).toBe('Paraguay');
    expect(river('Asuncion', 'Corrientes')).toBe('Parana');
    expect(river('Tacuarembo', 'Corrientes')).toBe('Uruguay');
    expect(river('Ciudad Bolivar', 'Guayana')).toBe('Orinoco');
    expect(river('Bogota', 'Medellin')).toBe('Magdalena');
    expect(river('Bahia', 'Goias')).toBe('Sao Francisco');
    expect(river('Buenos Aires', 'Pampas')).toBeNull();
    const rail = (a: string, b: string): boolean => ground(a).some((x) => railsAt(x).some((y) => ground(b).includes(y)));
    // The lines up to La Paz, Rio to Sao Paulo, Buenos Aires to Rosario. (The Transandine was cut by the flood of 1934
    // until 1944: the map builder's net of the cities leaves it out.)
    expect(rail('Mendoza', 'Santiago')).toBe(false);
    for (const [a, b] of [['Tarapaca', 'La Paz'], ['Tucuman', 'Potosi'], ['Rio de Janeiro', 'Sao Paulo'], ['Buenos Aires', 'Rosario'], ['Canal Zone', 'Panama City']]) expect(rail(a, b), `${a} to ${b}`).toBe(true);
    // None through the Darien, none down the isthmus, none over the Uruguay, none into the forest.
    for (const [a, b] of [['Panama City', 'Medellin'], ['Managua', 'San Jose'], ['Tacuarembo', 'Corrientes'], ['Santa Cruz', 'Campo Grande']]) expect(rail(a, b), `${a} to ${b}`).toBe(false);
    for (const name of ['Amazonas', 'Acre', 'Loreto', 'Mato Grosso', 'Peten', 'Chaco Boreal', 'Magallanes', 'Tierra del Fuego', 'Falkland Islands']) expect(onNet(id(name)), name).toBe(false);
    for (const name of ['Rio de Janeiro', 'Buenos Aires', 'Santiago', 'Lima', 'Bogota', 'Caracas', 'La Paz', 'Montevideo']) expect(onNet(id(name)), name).toBe(true);
  });

  it('puts eighteen countries on it: six a player can take and twelve small republics', () => {
    const g = wide();
    expect(WORLD.order).toEqual(['CAN', 'MEX', 'CUB', ...LATIN]);
    const list = (grandData as unknown as { nations: { id: string; small?: boolean; years: Record<string, string> }[] }).nations;
    for (const id of LATIN) {
      const n = g.nations[id];
      expect(n?.alive && !n.dormant, id).toBe(true);
      expect(bloc(n), id).toBeNull();
      expect(n.atWar, id).toEqual([]);
      expect(small(id), id).toBe(true);
      expect(g.regions[n.capital].owner, id).toBe(id);
      expect(g.regions[n.capital].cap, id).toBe(true);
      // An army of its own on the first day, at home, and its flag.
      expect(g.corps.some((c) => c.nat === id && g.regions[c.reg].owner === id), id).toBe(true);
      expect(FLAGS[id], id).toBeTruthy();
      expect(n.color, id).toMatch(/^#[0-9a-f]{6}$/);
      expect(DIPLOMACY.home[id], id).toBeDefined();
      expect(DIPLOMACY.bandwagon[id], id).toBe(0.5);
      expect(WORLD.idle[id], id).toBeGreaterThan(0);
      // A staff at peace an ocean from the war raises no army to stand on the quay.
      expect(g.corps.filter((c) => c.nat === id && c.kind !== 'fort').length, id).toBeLessThanOrEqual(WORLD.idle[id]);
    }
    // The six larger ones are on the start screen, each with a line of its own, two abilities or one and a formation,
    // ships to carry an army, and the art director's colour.
    expect(list.filter((n) => LARGE.includes(n.id)).map((n) => n.id)).toEqual(LARGE);
    for (const n of list.filter((x) => LARGE.includes(x.id))) { expect(n.small).toBe(true); expect(n.years['1939'].length).toBeGreaterThan(40); }
    for (const id of LARGE) {
      expect(traitsOf(id).length + (TRAITS.nations[id].unit ? 1 : 0), id).toBe(2);
      expect(g.nations[id].sealift, id).toBe(1);
    }
    expect(LARGE.map((id) => g.nations[id].color)).toEqual(['#f2d516', '#4aa2ec', '#a0600f', '#93479a', '#2f55a6', '#1f9d6b']);
    // (Each of the twelve has a colour of its own since Version 77: the art director's sheet. tests/staff-77.test.ts)
    expect(new Set(SMALL.map((id) => g.nations[id].color)).size).toBe(SMALL.length);
    for (const id of SMALL) { expect(list.some((n) => n.id === id), id).toBe(false); expect(g.nations[id].sealift, id).toBe(0); }
    expect(['CHL', 'COL', 'URU', 'COS'].map(formOf)).toEqual(['democracy', 'democracy', 'democracy', 'democracy']);
    expect(formOf('BRA')).toBe('authoritarian');
    // Four of them keep a squadron in home waters.
    expect(g.fleets.filter((f) => LATIN.includes(f.nat)).map((f) => `${f.nat} ${g.seas[f.zone].key}`)).toEqual(['BRA santos', 'ARG plate', 'CHL valparaiso', 'PER callao']);
    // A neutral selling to both sides does not out-earn a country at war: none of them earns what Canada does.
    for (const id of LATIN) expect(income(g, id), id).toBeLessThan(income(g, 'CAN'));
  });

  it('trades with its neighbours from the first day, and leaves the agreements of Europe as they were', () => {
    const g = wide();
    for (const id of LATIN) {
      const with_ = dealsOf(g, id).map((d) => (d.a === id ? d.b : d.a));
      expect(with_.length, id).toBe(housesOf(g, id));
      for (const o of with_) expect(LATIN, `${id} with ${o}`).toContain(o);
    }
    // Nobody across the ocean has an agreement with any of them: every house in Europe went where it always did.
    for (const d of g.trade) expect(LATIN.includes(d.a)).toBe(LATIN.includes(d.b));
    expect(friendsOf('ARG').map((f) => f.nat).sort()).toEqual(['BRA', 'CHL', 'URU']);
    expect(friendsOf('PAN').map((f) => f.nat)).toEqual(['USA']);
  });

  it('gives the Canal its own ground: whoever holds the Zone holds the Canal', () => {
    const g = wide();
    const zone = g.byName['Canal Zone'];
    const key = (k: string): number => g.seas.findIndex((s) => s.key === k);
    expect([zone.owner, zone.ctrl, zone.overseas, zone.port]).toEqual(['USA', 'USA', true, true]);
    expect(zone.seas.sort((a, b) => a - b)).toEqual([key('caribbean'), key('panama')]);
    // Its garrison stands in it (infantry the staff holds in place since Version 84.2, ruling 176(d)), and is not
    // counted among the corps America keeps at home.
    expect(zone.units.map((c) => [c.nat, c.kind, c.hold])).toEqual([['USA', 'inf', true]]);
    expect(armyIdle(g, 'USA')).toBe(wideIdle());
    // Open to everybody at peace with its holder, shut to his enemies.
    for (const id of ['UK', 'GER', 'BRA', 'CHL']) expect(canPass(g, key('caribbean'), key('panama'), id), id).toBe(true);
    setWar(g, 'USA', 'GER');
    expect(canPass(g, key('caribbean'), key('panama'), 'GER')).toBe(false);
    expect(canPass(g, key('caribbean'), key('panama'), 'CHL')).toBe(true);
    // Taken, it is the taker's to open and shut.
    capture(g, zone, 'GER');
    expect(canPass(g, key('caribbean'), key('panama'), 'GER')).toBe(true);
    expect(canPass(g, key('caribbean'), key('panama'), 'USA')).toBe(false);
    expect(canPass(g, key('caribbean'), key('panama'), 'CHL')).toBe(true);
  });

  it('passes the colonies of a fallen country into a sea power\'s keeping', () => {
    const g = wide();
    expect(['Martinique', 'French Guiana'].map((n) => g.byName[n].owner)).toEqual(['FRA', 'FRA']);
    expect(['Curacao', 'Dutch Guiana'].map((n) => g.byName[n].owner)).toEqual(['NED', 'NED']);
    expect(['British Honduras', 'Trinidad', 'British Guiana', 'Falkland Islands'].map((n) => g.byName[n].owner)).toEqual(['UK', 'UK', 'UK', 'UK']);
    for (const n of ['Martinique', 'French Guiana', 'Curacao', 'Dutch Guiana', 'Trinidad', 'Falkland Islands', 'Canal Zone']) expect(g.byName[n].overseas, n).toBe(true);
    setWar(g, 'NED', 'GER');
    capitulate(g, 'NED', 'GER');
    expect(['Curacao', 'Dutch Guiana'].map((n) => g.byName[n].ctrl)).toEqual(['UK', 'UK']);
    capitulate(g, 'FRA', 'GER');
    expect(['Martinique', 'French Guiana'].map((n) => g.byName[n].ctrl)).toEqual(['UK', 'UK']);
    expect(g.headlines.some((h) => /Martinique, French Guiana and Cayenne into their keeping/.test(h.text))).toBe(true);
  });

  it('brings in the republics of the isthmus when America goes to war, and Brazil in August 1942', () => {
    const g = wide();
    const ISTHMUS = ['PAN', 'COS', 'NIC', 'HON', 'ELS', 'GUA', 'HAI', 'DOM'];
    setWar(g, 'USA', 'GER');
    g.nations.USA.align = 'allies';
    // Not before history's month.
    g.turn = DEC_41 - 1;
    monthlyDiplomacy(g);
    expect(ISTHMUS.some((id) => g.nations[id].atWar.length > 0)).toBe(false);
    g.turn = DEC_41;
    monthlyDiplomacy(g);
    for (const id of ISTHMUS) { expect(bloc(g.nations[id]), id).toBe('allies'); expect(atWar(g, id, 'GER'), id).toBe(true); }
    expect(allied(g, 'PAN', 'USA')).toBe(true);
    for (const id of ['BRA', 'ARG', 'CHL', 'COL', 'ECU', 'URU']) expect(g.nations[id].atWar, id).toEqual([]);
    g.turn = AUG_42;
    monthlyDiplomacy(g);
    expect(bloc(g.nations.BRA)).toBe('allies');
    expect(atWar(g, 'BRA', 'GER')).toBe(true);
    // Argentina, Chile and the rest keep their own counsel.
    for (const id of ['ARG', 'CHL', 'PER', 'COL', 'VEN', 'ECU', 'BOL', 'PAR', 'URU']) expect(g.nations[id].atWar, id).toEqual([]);
    // At war, a small republic's staff still raises no army to stand on the quay.
    expect(armyIdle(g, 'PAN')).toBe(true);
  });

  it('leaves a player\'s country where its player puts it, with every path open to it', () => {
    const g = wide(1, 'BRA');
    const paths = pathOptions(g, 'BRA');
    expect(paths.map((p) => p.id)).toEqual(['axis', 'allies', 'alone', 'found']);
    expect(paths.filter((p) => p.ok).map((p) => p.id)).toEqual(['axis', 'allies', 'found']);
    // The ABC powers would found a league with it.
    expect(founders(g, 'BRA').filter((f) => f.ok && f.friend).map((f) => f.nat).sort()).toEqual(['ARG', 'CHL']);
    // History's declaration does not speak for a player.
    setWar(g, 'USA', 'GER');
    g.turn = AUG_42;
    monthlyDiplomacy(g);
    expect(bloc(g.nations.BRA)).toBeNull();
    expect(g.nations.BRA.atWar).toEqual([]);
  });

  it('loads a campaign saved on the smaller chart of Versions 74 and 75', () => {
    const g = wide(4);
    for (let k = 0; k < 2; k++) runTurnAI(g);
    setWar(g, 'NED', 'GER');
    capitulate(g, 'NED', 'GER');
    // The same campaign as Version 75 would have written it: no province past the 104th, none of the new
    // countries, their armies or squadrons, and no agreement of theirs.
    const d = JSON.parse(saveGame(g));
    const old = WORLD_FIRST + 104;
    const gone = (c: { nat: string; reg: number }): boolean => LATIN.includes(c.nat) || c.reg >= old;
    d.regions = d.regions.slice(0, old);
    d.corps = d.corps.filter((c: { nat: string; reg: number }) => !gone(c));
    d.generals = d.generals.filter((x: { corps?: number }) => x.corps === undefined || d.corps.some((c: { id: number }) => c.id === x.corps));
    d.fleets = d.fleets.filter((f: { nat: string }) => !LATIN.includes(f.nat));
    d.airFleets = (d.airFleets ?? []).filter((f: { nat: string }) => !LATIN.includes(f.nat));
    d.trade = (d.trade ?? []).filter((t: { a: string; b: string }) => !LATIN.includes(t.a) && !LATIN.includes(t.b));
    d.order = d.order.filter((id: string) => !LATIN.includes(id));
    for (const id of LATIN) delete d.nations[id];
    for (const k of Object.keys(d.supply)) { if (LATIN.includes(k)) delete d.supply[k]; else d.supply[k] = d.supply[k].slice(0, old); }
    const text = JSON.stringify(d);
    expect(peekSave(text)).not.toBeNull();
    expect(staleSave(text)).toBe(false);
    const l = loadGame(text)!;
    expect(l.regions.length).toBe(WORLD_FIRST + WORLD_REGIONS);
    expect(l.turn).toBe(g.turn);
    // The new countries take their places in the order of play, each with its first-day army; the squadrons are in port.
    expect(l.order.slice(-LATIN.length)).toEqual(LATIN);
    for (const id of LATIN) expect(l.corps.filter((c) => c.nat === id).length, id).toBe(g.corps.filter((c) => c.nat === id).length);
    expect(l.fleets.filter((f) => LATIN.includes(f.nat)).map((f) => f.nat)).toEqual(['BRA', 'ARG', 'CHL', 'PER']);
    expect(l.byName['Canal Zone'].units.map((c) => c.kind)).toEqual(['inf']);
    expect(new Set(l.corps.map((c) => c.id)).size).toBe(l.corps.length);
    // Ground of a country the save has seen fall goes where a fallen country's colonies go.
    expect(['Curacao', 'Dutch Guiana'].map((n) => l.byName[n].ctrl)).toEqual(['UK', 'UK']);
    expect(l.byName['French Guiana'].ctrl).toBe('FRA');
    // And the campaign plays on.
    for (let k = 0; k < 2; k++) runTurnAI(l);
    expect(l.turn).toBe(g.turn + 2);
    // A save of today's chart is laid over it province for province, and a save of some other size is refused.
    expect(loadGame(saveGame(g))!.regions.length).toBe(g.regions.length);
    d.regions = d.regions.slice(0, old - 1);
    expect(loadGame(JSON.stringify(d))).toBeNull();
    expect(staleSave(JSON.stringify(d))).toBe(true);
  });
});

/** Whether America's staff counts its army at home as enough on the first day, with nothing else changed. */
function wideIdle(): boolean {
  const g = wide();
  g.byName['Canal Zone'].units = [];
  return armyIdle(g, 'USA');
}

describe('two faults of Version 75', () => {
  it('does not hand the American short campaign to a corps shipped to a France that has not been attacked', () => {
    const g = wide(1, 'USA');
    openGoals(g, 'USA', 1);
    const goal = GOALS.USA.short!;
    // The design director's ruling 41: it is settled on its date, as January 1945 opens, like the other goals of
    // endurance, and not in the first month it happens to be true.
    expect([goal.until, goal.by]).toEqual([64, undefined]);
    expect(goalWhen(goal, (t) => `month ${t}`)).toBe('decided when month 65 opens');
    setWar(g, 'USA', 'GER');
    g.nations.USA.align = 'allies';
    const corps = addCorps(g, 'USA', 'I Corps', 'inf', 2, 'Bordeaux', {});
    // Washington, Paris and an army ashore in France all hold in September 1939, and nothing is won: not then, and
    // not in any month before the date.
    for (const turn of [1, 2, 29, 64]) {
      g.turn = turn;
      expect(judgeGoals(g, 'USA'), `month ${turn}`).toEqual([]);
      expect(goalState(g, 'short')).toBe('open');
    }
    // What stands as January 1945 opens decides it: an army that went home before then has opened no second front.
    const later = loadGame(saveGame(g))!;
    g.turn = 65;
    expect(judgeGoals(g, 'USA').map((x) => [x.key, x.won])).toEqual([['short', true]]);
    removeCorps(later, later.corps.find((c) => c.name === corps.name && c.nat === 'USA')!);
    later.turn = 65;
    expect(judgeGoals(later, 'USA').map((x) => [x.key, x.won])).toEqual([['short', false]]);
  });

  it('keeps the shores of the continent on an Allied player\'s list of crossings once the Soviet Union is in the war', () => {
    for (const seat of ['UK', 'USA']) {
      const g = wide(3, seat);
      for (const id of ['UK', 'USA', 'SOV']) { setWar(g, id, 'GER'); g.nations[id].align = 'allies'; }
      // France has fallen on the mainland (Corsica and Syria, over the water whoever lands in Brittany, are left out of
      // it), and the Red Army faces the Germans across the whole of the east.
      const mainland = new Set([g.byName.Paris.id]);
      for (const at of mainland) for (const j of g.regions[at].nb) if (g.regions[j].owner === 'FRA') mainland.add(j);
      for (const r of g.regions) if (mainland.has(r.id) || r.owner === 'POL') { r.ctrl = 'GER'; r.units = r.units.filter((u) => u.nat !== r.owner); }
      if (seat === 'USA') addCorps(g, 'USA', 'V Corps', 'inf', 2, 'West Country', {});
      expect(g.regions.some((r) => r.ctrl === 'SOV' && r.nb.some((j) => g.regions[j].ctrl === 'GER')), seat).toBe(true);
      // The Germans hold Odessa, which the Red Army can march back to.
      const odessa = g.byName.Odessa;
      odessa.ctrl = 'GER';
      odessa.units = [];
      expect(odessa.seas.length).toBeGreaterThan(0);
      const list = crossingsOf(g, seat);
      // The lost ground of a living ally, within a march of his own army, is no crossing of this nation's.
      expect(list.filter((x) => x.owner === 'SOV').map((x) => x.to.name), `${seat}: Soviet ground`).toEqual([]);
      // Germany's own coast and the coast of occupied France are both over the water from Britain.
      expect(list.some((x) => x.owner === 'GER' && x.home), `${seat}: Germany`).toBe(true);
      expect(list.some((x) => x.owner === 'FRA' && x.foe === 'GER'), `${seat}: France`).toBe(true);
      // Once a corps of his own is ashore, the rest of that shore is a march away and needs no ships.
      const brittany = g.byName.Brittany;
      addCorps(g, seat, 'Lodgment', 'inf', 2, 'Brittany', {});
      brittany.ctrl = seat;
      expect(crossingsOf(g, seat).filter((x) => x.owner === 'FRA' && x.foe === 'GER').map((x) => x.to.name), `${seat}: France after landing`).toEqual([]);
    }
  });
});
