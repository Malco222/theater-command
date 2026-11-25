// The wider world, part one (Version 74). A grand campaign opens on a wider chart: the map of Europe carried on
// westward, on the same scale, to the Pacific coast of North America, with Canada, Mexico and Cuba as countries and
// the United States at home on its own soil, neutral until December 1941. Europe's provinces and seas keep their
// numbers, and history's scenarios never see any of it. And two mends to the landing card, from the Playtester:
// the forecast of the sea behind the beach follows the rule the enemy's admirals follow, wherever their fleets lie.
import { describe, expect, it } from 'vitest';
import { allied, atWar, capitulate, createGame, setWar } from '../src/campaign/game';
import { majorsOf } from '../src/campaign/diplomacy';
import { NAVY, airOver, atSea, canPass, fleetPower, fleetReach, fleetsAnswer, fleetsInReach, navalAI, sail, seaHold, seaSide, surfaceShips } from '../src/campaign/navy';
import { RAIL_LINKS, WORLD_RAIL_LINKS, onNet, railLinks, railsAt } from '../src/campaign/rails';
import { CROSSINGS, RIVERS, WORLD_CROSSINGS, crossingOf, crossingsOf, riverName, riversOf } from '../src/campaign/rivers';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { WORLD, WORLD_FIRST, WORLD_MAP, WORLD_REGIONS, armyIdle, attachWorld, beyondEurope, lakesOf, oilAtHome, peaceFooting, protectorOf, worldMonth, worldOn } from '../src/campaign/world';
import type { Fleet, Game } from '../src/campaign/types';
import goals from '../data/campaign/goals.json';
import grand from '../data/campaign/grand.json';

const DEC_40 = 16, JAN_42 = 29;
/** A grand campaign on the wider world as it opens. */
const wide = (seed = 1): Game => startOpen(createGame(seed, null));
const names = (g: Game, ids: number[]): string[] => ids.map((i) => g.regions[i].name);

/** One war on the wider world with nobody playing, remembered as these months open. */
const war: Record<number, string> = {};
{
  const g = wide(2);
  while (g.turn < JAN_42) { runTurnAI(g); if (g.turn === DEC_40 || g.turn === JAN_42) war[g.turn] = saveGame(g); }
}

describe('the wider world', () => {
  it('carries the map of Europe on westward and leaves Europe as it was', () => {
    const e = createGame(1, null), g = wide();
    expect(worldOn(g)).toBe(true);
    expect(WORLD_FIRST).toBe(e.regions.length);
    expect(g.regions.length).toBe(WORLD_FIRST + WORLD_REGIONS);
    expect(g.seas.length).toBeGreaterThan(e.seas.length);
    // Europe's provinces and seas keep their numbers, names, owners and places.
    for (const r of e.regions) {
      const w = g.regions[r.id];
      expect([w.id, w.name, w.owner, w.x, w.y]).toEqual([r.id, r.name, r.owner, r.x, r.y]);
    }
    for (const s of e.seas) expect([g.seas[s.id].id, g.seas[s.id].key, g.seas[s.id].name]).toEqual([s.id, s.key, s.name]);
    // The frame runs west of the old chart's edge, on the same scale.
    expect(g.x0).toBe(WORLD_MAP.frame.x0);
    expect(g.x0).toBeLessThan(e.x0 ?? 0);
    expect(g.width).toBe(WORLD_MAP.frame.x1 - WORLD_MAP.frame.x0);
    expect(g.height).toBeGreaterThanOrEqual(e.height);
    // Every new province is numbered on from Europe's, in its place in the list, and found by its name.
    for (let i = WORLD_FIRST; i < g.regions.length; i++) {
      expect(g.regions[i].id).toBe(i);
      expect(g.byName[g.regions[i].name]).toBe(g.regions[i]);
      expect(beyondEurope(g.regions[i])).toBe(true);
    }
    expect(beyondEurope(g.byName.London)).toBe(false);
    // Attaching it twice changes nothing.
    attachWorld(g);
    expect(g.regions.length).toBe(WORLD_FIRST + WORLD_REGIONS);
  });

  it('is never seen by history\'s scenarios, nor by a grand campaign kept to the map of Europe', () => {
    const h = createGame(1, null);
    for (let k = 0; k < 3; k++) runTurnAI(h);
    expect(worldOn(h)).toBe(false);
    expect(h.regions.length).toBe(WORLD_FIRST);
    expect(h.nations.CAN).toBeUndefined();
    expect(h.nations.USA.dormant).toBe(true);
    expect(lakesOf(h)).toEqual([]);
    expect(riversOf(h)).toBe(RIVERS);
    expect(crossingsOf(h)).toBe(CROSSINGS);
    expect(railLinks(h)).toBe(RAIL_LINKS);
    expect(majorsOf(h)).not.toContain('USA');
    expect(peaceFooting(h, 'USA')).toBeNull();
    expect(oilAtHome(h, 'USA')).toBeNull();
    expect(protectorOf(h, 'GER')).toBeNull();
    expect(armyIdle(h, 'USA')).toBe(false);
    const e = startOpen(createGame(1, null), false);
    expect(e.flags.open).toBe(1);
    expect(worldOn(e)).toBe(false);
    expect(e.regions.length).toBe(WORLD_FIRST);
    expect(e.nations.MEX).toBeUndefined();
  });

  it('has a sound chart: neighbours both ways, coasts on seas that know them, and every sea joined to the rest', () => {
    const g = wide();
    for (let i = WORLD_FIRST; i < g.regions.length; i++) {
      const r = g.regions[i];
      for (const j of r.nb) {
        expect(g.regions[j].nb, `${r.name} and ${g.regions[j].name}`).toContain(i);
        // No land joins the new world to the old.
        expect(beyondEurope(j), `${r.name} and ${g.regions[j].name}`).toBe(true);
      }
      for (const z of r.seas) expect(g.seas[z].coast, `${r.name} on the ${g.seas[z].name}`).toContain(i);
    }
    for (const s of g.seas) for (const j of s.adj) expect(g.seas[j].adj, `${s.name} and ${g.seas[j].name}`).toContain(s.id);
    const seen = new Set([0]);
    const q = [0];
    for (let k = 0; k < q.length; k++) for (const j of g.seas[q[k]].adj) if (!seen.has(j)) { seen.add(j); q.push(j); }
    expect(seen.size).toBe(g.seas.length);
    // The Pacific coast is joined to the Caribbean by the Panama Canal, which is America's to open and shut. Since
    // Version 76 it stands on its own ground: the Canal Zone, between the Caribbean and the Gulf of Panama.
    const key = (k: string): number => g.seas.findIndex((s) => s.key === k);
    const canal = g.gates.find((gt) => gt.name === 'the Panama Canal')!;
    expect([canal.a, canal.b].sort((a, b) => a - b)).toEqual([key('caribbean'), key('panama')]);
    expect(g.regions[canal.region].name).toBe('Canal Zone');
    expect(g.byName['Canal Zone'].ctrl).toBe('USA');
    expect(canPass(g, key('caribbean'), key('panama'), 'UK')).toBe(true);
    expect(canPass(g, key('caribbean'), key('panama'), 'GER')).toBe(true);
    setWar(g, 'USA', 'GER');
    expect(canPass(g, key('caribbean'), key('panama'), 'GER')).toBe(false);
    expect(canPass(g, key('caribbean'), key('panama'), 'UK')).toBe(true);
    expect(canPass(g, key('caribbean'), key('tehuantepec'), 'USA')).toBe(true);
    // The islands of the Atlantic stand alone on their seas.
    // (An island's only neighbours are the cities cut from it: St John's, Gander and Corner Brook on Newfoundland.)
    const ofIt = (name: string): number[] => (WORLD_MAP.regions as { of?: number }[]).map((r, i) => (r.of === g.byName[name].id ? WORLD_FIRST + i : -1)).filter((i) => i >= 0);
    for (const name of ['Newfoundland', 'Iceland', 'Greenland', 'Azores', 'Puerto Rico', 'Jamaica', 'Bahamas', 'Canary Islands']) {
      expect(g.byName[name].nb.filter((j) => !ofIt(name).includes(j)), name).toEqual([]);
      expect(g.byName[name].seas.length, name).toBeGreaterThan(0);
    }
    // The frontiers run where history drew them: the lakes and the 49th parallel, the Rio Grande, the Gulf of California.
    // (Ground to ground: a province with the cities cut from it, since the cities version.)
    const ground = (n: string): number[] => [g.byName[n].id, ...ofIt(n)];
    const borders = (a: string, b: string): boolean => ground(a).some((x) => g.regions[x].nb.some((y) => ground(b).includes(y)));
    expect(borders('Detroit', 'Toronto')).toBe(true);
    expect(borders('Buffalo', 'Toronto')).toBe(true);
    expect(borders('Puget Sound', 'Vancouver')).toBe(true);
    expect(borders('Rio Grande', 'Monterrey')).toBe(true);
    expect(borders('San Diego', 'Baja California')).toBe(true);
    expect(borders('Southern Baja', 'Sinaloa')).toBe(false);
    expect(lakesOf(g).map((l) => l.name)).toEqual(expect.arrayContaining(['Lake Superior', 'Lake Michigan', 'Lake Erie']));
    // Each lake carries what whoever draws the world in relief needs: its surface above the sea and its shore.
    expect(Object.fromEntries(lakesOf(g).map((l) => [l.name.replace('Lake ', ''), l.level]))).toMatchObject({ Superior: 183, Michigan: 176, Huron: 176, Erie: 174, Ontario: 75, Winnipeg: 217 });
    // The cities version brings the map builder's lakes of the Americas: 25 in all, Titicaca the highest.
    expect(lakesOf(g).length).toBe(25);
    expect(lakesOf(g).find((l) => l.name === 'Lake Titicaca')?.level).toBe(3812);
    for (const l of lakesOf(g)) {
      // (A small lake of the cities version, Okeechobee, is drawn in ten points.)
      expect(l.outline.length, l.name).toBeGreaterThanOrEqual(8);
      const xs = l.outline.map((p) => p[0]), ys = l.outline.map((p) => p[1]);
      expect(l.x > Math.min(...xs) && l.x < Math.max(...xs) && l.y > Math.min(...ys) && l.y < Math.max(...ys), l.name).toBe(true);
    }
    // The file says how its own coordinates sit in the whole world's.
    expect(WORLD_MAP.world).toMatchObject({ width: 9256.14, height: 5096, shift: 500, seam: -30.45 });
  });

  it('puts the United States at home, neutral, and Canada, Mexico and Cuba on the map', () => {
    const g = wide();
    const n = g.nations;
    expect(n.USA.dormant).toBe(false);
    expect(n.USA.alive).toBe(true);
    expect(n.USA.offmap).toBeNull();
    expect(n.USA.align).toBe('leaning_allies');
    expect(n.USA.atWar).toEqual([]);
    expect(g.regions[n.USA.capital].name).toBe('Washington');
    expect(g.regions[n.CAN.capital].name).toBe('Ottawa');
    expect(g.regions[n.MEX.capital].name).toBe('Mexico City');
    expect(g.regions[n.CUB.capital].name).toBe('Havana');
    // They follow the countries of the old chart in the order of play, and the countries of later slices follow them.
    expect(g.order.slice(g.order.indexOf('CAN'), g.order.indexOf('CAN') + 3)).toEqual(['CAN', 'MEX', 'CUB']);
    expect(g.order.indexOf('CAN')).toBe(createGame(1, null).order.length);
    expect(n.MEX.align).toBe('independent');
    expect(n.MEX.atWar).toEqual([]);
    // A dominion is in its mother country's wars from the day they began.
    expect(n.CAN.align).toBe('allies');
    expect(n.CAN.atWar).toEqual(n.UK.atWar);
    expect(atWar(g, 'CAN', 'GER')).toBe(true);
    // The armies at home are cadres, and the squadrons lie in their home ports.
    for (const [nat, name, , divs, at, o] of WORLD.corps) {
      const c = g.corps.find((x) => x.nat === nat && x.name === name)!;
      expect(c, name).toBeDefined();
      expect([c.divs, g.regions[c.reg].name, c.str], name).toEqual([divs, at, o.str ?? 1]);
    }
    const sq = g.fleets.find((f) => f.nat === 'USA' && f.name === 'Atlantic Fleet')!;
    expect(g.regions[sq.base].name).toBe('Norfolk');
    expect(g.seas[sq.zone].key).toBe('seaboard');
    expect(g.fleets.some((f) => f.nat === 'CAN' && g.seas[f.zone].key === 'scotian')).toBe(true);
    // America counts among the great powers on the wider world.
    expect(majorsOf(g)).toContain('USA');
    expect(majorsOf(g)).not.toContain('CAN');
    // Every country at home stands in supply on its own ground on the first day.
    for (const c of g.corps) if (beyondEurope(c.reg)) expect(c.supplied, c.name).toBe(true);
  });

  it('keeps America\'s factories at a peacetime pace, and puts its oil in fields that can be taken', () => {
    const g = wide();
    expect(peaceFooting(g, 'GER')).toBeNull();
    const rows = WORLD.peace.USA;
    for (const [from, share] of rows) { g.turn = from; expect(peaceFooting(g, 'USA')).toBe(share); }
    g.turn = rows[1][0] - 1;
    expect(peaceFooting(g, 'USA')).toBe(rows[0][1]);
    expect(rows.map((r) => r[1])).toEqual([...rows.map((r) => r[1])].sort((a, b) => a - b));
    expect(oilAtHome(g, 'USA')).toBe(WORLD.oilHome.USA);
    expect(oilAtHome(g, 'GER')).toBeNull();
    for (const name of ['East Texas', 'Oklahoma', 'West Texas', 'San Joaquin']) expect(g.byName[name].owner, name).toBe('USA');
    expect(g.byName.Tampico.owner).toBe('MEX');
  });

  it('raises no more armies to stand on the quay once a computer\'s homeland across the ocean has enough', () => {
    const g = wide();
    const cap = WORLD.idle.USA;
    // The army at home: a corps in an overseas possession is its garrison (the Canal's division, Version 84.2), and is not counted.
    const home = (): number => g.corps.filter((c) => c.nat === 'USA' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && beyondEurope(c.reg) && !g.regions[c.reg].overseas).length;
    expect(g.corps.some((c) => c.nat === 'USA' && !c.dead && c.kind === 'inf' && g.regions[c.reg].overseas)).toBe(true);
    expect(home()).toBeLessThan(cap);
    expect(armyIdle(g, 'USA')).toBe(false);
    // Corps on order count as if they had formed.
    while (home() + g.nations.USA.queue.length < cap) g.nations.USA.queue.push({ what: 'corps', kind: 'inf', left: 3 } as never);
    expect(armyIdle(g, 'USA')).toBe(true);
    // A player's own staff is not bound by it, and neither is a country of Europe.
    g.human = 'USA';
    expect(armyIdle(g, 'USA')).toBe(false);
    g.human = null;
    expect(armyIdle(g, 'GER')).toBe(false);
    // An enemy ashore on its side of the water ends it.
    setWar(g, 'USA', 'GER');
    expect(armyIdle(g, 'USA')).toBe(true);
    g.byName['Nova Scotia'].ctrl = 'GER';
    expect(armyIdle(g, 'USA')).toBe(false);
  });

  it('takes a dominion into its mother country\'s wars, unless a player leads it', () => {
    const g = wide();
    setWar(g, 'UK', 'ITA');
    worldMonth(g);
    expect(atWar(g, 'CAN', 'ITA')).toBe(true);
    expect(g.headlines[g.headlines.length - 1].text).toMatch(/Canada is at war with Italy/);
    const p = wide();
    p.human = 'CAN';
    setWar(p, 'UK', 'ITA');
    worldMonth(p);
    expect(atWar(p, 'CAN', 'ITA')).toBe(false);
  });

  it('passes a fallen country\'s islands across the ocean into the keeping of a sea power at war with the victor', () => {
    const g = wide();
    expect(protectorOf(g, 'GER')).toBe('UK');
    expect(protectorOf(g, 'UK')).toBeNull();
    expect([g.byName.Iceland.owner, g.byName.Greenland.owner]).toEqual(['DEN', 'DEN']);
    capitulate(g, 'DEN', 'GER');
    expect(g.byName.Jutland.ctrl).toBe('GER');
    expect(g.byName.Iceland.ctrl).toBe('UK');
    expect(g.byName.Greenland.ctrl).toBe('UK');
    expect(g.headlines.some((h) => /Iceland and Greenland into their keeping/.test(h.text))).toBe(true);
  });

  it('lays the railways and rivers of the new world beside Europe\'s, and Europe\'s as they were', () => {
    const g = wide();
    expect(railLinks(g).length).toBe(RAIL_LINKS.length + WORLD_RAIL_LINKS.length);
    expect(railLinks(g).slice(0, RAIL_LINKS.length)).toEqual(RAIL_LINKS);
    expect(crossingsOf(g).length).toBe(CROSSINGS.length + WORLD_CROSSINGS.length);
    expect(crossingsOf(g).slice(0, CROSSINGS.length)).toEqual(CROSSINGS);
    expect(riversOf(g).slice(0, RIVERS.length)).toEqual(RIVERS);
    // The great rivers first, then the cities version's 150 more (the Yukon, the Arkansas, the Snake...).
    expect(riversOf(g).slice(RIVERS.length, RIVERS.length + 15).map((r) => r.name)).toEqual(['Mississippi', 'Missouri', 'Ohio', 'St Lawrence', 'Rio Grande', 'Columbia', 'Colorado',
      'Amazon', 'Madeira', 'Parana', 'Paraguay', 'Uruguay', 'Orinoco', 'Magdalena', 'Sao Francisco']);
    expect(riversOf(g).length - RIVERS.length).toBe(165);
    const id = (name: string): number => g.byName[name].id;
    // A province with the cities cut from it: since the cities version a river or a line may run from a city.
    const ground = (n: string): number[] => [id(n), ...(WORLD_MAP.regions as { of?: number }[]).map((r, i) => (r.of === id(n) ? WORLD_FIRST + i : -1)).filter((i) => i >= 0)];
    const river = (a: string, b: string): string | null => { for (const x of ground(a)) for (const y of ground(b)) { const c = crossingOf(x, y); if (c) return riverName(c); } return null; };
    expect(river('Illinois', 'Missouri')).toBe('Mississippi');
    expect(river('Rio Grande', 'Monterrey')).toBe('Rio Grande');
    expect(river('Kennebec', 'Quebec')).toBe('St Lawrence');
    expect(river('Indiana', 'Kentucky')).toBe('Ohio');
    expect(river('New York', 'Philadelphia')).toBeNull();
    // Every crossing lies between two neighbours beyond Europe.
    for (const c of WORLD_CROSSINGS) {
      expect(beyondEurope(c.a) && beyondEurope(c.b)).toBe(true);
      expect(g.regions[c.a].nb).toContain(c.b);
    }
    // The transcontinental lines history built, and none where there was none.
    const rail = (a: string, b: string): boolean => ground(a).some((x) => railsAt(x).some((y) => ground(b).includes(y)));
    for (const [a, b] of [['Iowa', 'Nebraska'], ['Wyoming', 'Utah'], ['Kansas', 'New Mexico'], ['Northern Ontario', 'Winnipeg'], ['Sonora', 'Arizona'], ['Sonora', 'Sinaloa'], ['Guadalajara', 'Sinaloa'], ['New York', 'Philadelphia']]) expect(rail(a, b), `${a} to ${b}`).toBe(true);
    for (const [a, b] of [['Sonora', 'Baja California'], ['San Diego', 'Sonora'], ['Chihuahua', 'Sinaloa']]) expect(rail(a, b), `${a} to ${b}`).toBe(false);
    for (const name of ['Yukon', 'Mackenzie', 'Keewatin', 'Ungava', 'Labrador', 'Southern Baja', 'Iceland', 'Greenland']) expect(onNet(id(name)), name).toBe(false);
    for (const name of ['Washington', 'Chicago', 'San Francisco', 'Ottawa', 'Vancouver', 'Mexico City', 'Havana']) expect(onNet(id(name)), name).toBe(true);
    for (const l of WORLD_RAIL_LINKS) expect(g.regions[l.a].nb, names(g, [l.a, l.b]).join(' to ')).toContain(l.b);
  });

  it('saves and loads on the wider world, and loads a campaign of the old chart on the old chart', () => {
    const g = wide();
    const a = saveGame(g);
    const h = loadGame(a)!;
    expect(worldOn(h)).toBe(true);
    expect(h.regions.length).toBe(g.regions.length);
    expect(saveGame(h)).toBe(a);
    // A war under way comes back the same, and plays on.
    const m = loadGame(war[JAN_42])!;
    expect(saveGame(m)).toBe(war[JAN_42]);
    expect(m.turn).toBe(JAN_42);
    runTurnAI(m);
    expect(m.turn).toBe(JAN_42 + 1);
    // A grand campaign begun on the map of Europe stays on it.
    const e = startOpen(createGame(1, null), false);
    const b = saveGame(e);
    const k = loadGame(b)!;
    expect(worldOn(k)).toBe(false);
    expect(k.regions.length).toBe(WORLD_FIRST);
    expect(saveGame(k)).toBe(b);
  });

  it('keeps America out of the war until December 1941, and brings it in then', () => {
    const early = loadGame(war[DEC_40])!;
    expect(early.nations.USA.alive).toBe(true);
    expect(early.nations.USA.atWar).toEqual([]);
    expect(early.regions[early.nations.USA.capital].name).toBe('Washington');
    const late = loadGame(war[JAN_42])!;
    expect(atWar(late, 'USA', 'GER')).toBe(true);
    expect(allied(late, 'USA', 'UK')).toBe(true);
    // Nobody has set foot on its soil, and its factories are its own.
    for (let i = WORLD_FIRST; i < late.regions.length; i++) if (late.regions[i].owner === 'USA') expect(late.regions[i].ctrl, late.regions[i].name).toBe('USA');
  });

  it('attaches to a campaign taken over later, with America already in the war', () => {
    const g = createGame(2, null);
    while (g.turn < JAN_42) runTurnAI(g);
    expect(g.nations.USA.dormant).toBe(false);
    startOpen(g);
    expect(worldOn(g)).toBe(true);
    expect(g.regions[g.nations.USA.capital].name).toBe('Washington');
    expect(g.nations.USA.offmap).toBeNull();
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    // Canada joins every war Britain is in on that day.
    for (const e of g.nations.UK.atWar) if (g.nations[e].alive) expect(atWar(g, 'CAN', e), e).toBe(true);
    const a = saveGame(g);
    expect(saveGame(loadGame(a)!)).toBe(a);
  });

  it('offers the United States, Canada and Mexico on the start screen, each with its words, and America its goals', () => {
    const G = grand as unknown as { years: { year: number }[]; nations: { id: string; small?: boolean; years: Record<string, string> }[] };
    const tile = (id: string) => G.nations.find((n) => n.id === id);
    // America stands among the great powers, with words for every year a campaign can begin.
    expect(tile('USA')!.small).toBeFalsy();
    for (const y of G.years) expect(tile('USA')!.years[String(y.year)]?.length ?? 0, String(y.year)).toBeGreaterThan(40);
    expect(tile('CAN')!.small).toBe(true);
    expect(tile('MEX')!.small).toBe(true);
    expect(tile('CUB')).toBeUndefined();
    const us = (goals as unknown as { nations: Record<string, { short: { hold: string[]; by: number }; long: { hold: string[]; beaten?: string[]; by: number } }> }).nations.USA;
    expect(us.short.hold).toEqual(['Washington', 'Paris']);
    expect(us.long.beaten).toEqual(['GER']);
    const g = wide();
    for (const name of [...us.short.hold, ...us.long.hold]) expect(g.byName[name], name).toBeDefined();
    // The new countries and their flags' owners all have a name and a colour.
    for (const id of WORLD.order) { expect(g.nations[id].name.length).toBeGreaterThan(3); expect(g.nations[id].color).toMatch(/^#[0-9a-f]{6}$/); }
  });
});

describe('the forecast of the sea behind a beach follows the enemy\'s admirals', () => {
  /** A war the computer plays on every side, as it stands once Poland has fallen, with every enemy fleet of Germany's in harbour. */
  const opened = (): Game => {
    const g = startOpen(createGame(3, null), false);
    for (let k = 0; k < 8 && (k < 2 || g.nations.POL.alive); k++) runTurnAI(g);
    for (const f of g.fleets) if (atWar(g, f.nat, 'GER')) sail(g, f, f.home, 'port');
    return g;
  };
  const sea = (g: Game, key: string): number => g.seas.findIndex((s) => s.key === key);
  const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
  const called = (fs: Fleet[]): string[] => fs.map((f) => `${f.nat} ${f.name}`).sort();
  const power = (g: Game, fs: Fleet[]): number => fs.reduce((s, f) => s + fleetPower(g, f), 0);
  /** Puts a German battle fleet of at least this strength to sea in that water. */
  const germansIn = (g: Game, z: number, strength: number): void => {
    const f = g.fleets.find((k) => k.nat === 'GER' && surfaceShips(k) > 0)!;
    f.base = -1; f.home = z; f.zone = z; f.stance = 'patrol';
    for (let n = 0; n < 400 && seaSide(g, z, 'GER', false).total < strength; n++) f.ships.bb += 2;
  };

  it('counts a fleet a month\'s sail away, not only those next door', () => {
    const g = opened();
    const biscay = sea(g, 'biscay');
    const home = fleet(g, 'UK', 'Home Fleet');
    // The Home Fleet lies in the far north: no sea off its base is the Bay of Biscay or next to it.
    const off = g.regions[home.base].seas;
    expect(off.includes(biscay) || off.some((z) => g.seas[z].adj.includes(biscay))).toBe(false);
    expect(fleetReach(g, home)).toContain(biscay);
    const a = fleetsAnswer(g, 'GER', [biscay], g.byName.Brittany);
    expect(called(a.come)).toContain('UK Home Fleet');
    expect(fleetsInReach(g, 'GER', [biscay], g.byName.Brittany)).toEqual(a.come);
  });

  it('leaves out a fleet no strait lets through, one tied to its convoys elsewhere, and a raider', () => {
    const g = opened();
    const black = sea(g, 'black'), aegean = sea(g, 'aegean'), channel = sea(g, 'channel');
    // The Mediterranean Fleet at sea next door to the Black Sea, with the straits shut to it.
    const med = fleet(g, 'UK', 'Mediterranean Fleet');
    expect(sail(g, med, aegean, 'patrol')).toBe(true);
    expect(g.seas[aegean].adj).toContain(black);
    expect(fleetReach(g, med)).not.toContain(black);
    const b = fleetsAnswer(g, 'GER', [black], g.byName.Odessa);
    expect([...b.come, ...b.kept, ...b.shy]).not.toContain(med);
    // Dover Fleet keeps the Channel and Western Approaches Fleet its convoys: only the first answers in the Channel.
    const dover = fleet(g, 'UK', 'Dover Fleet'), wac = fleet(g, 'UK', 'Western Approaches Fleet');
    expect([dover.tied, wac.tied, dover.station === channel, wac.station === channel]).toEqual([true, true, true, false]);
    expect(fleetReach(g, wac)).toContain(channel);
    const c = fleetsAnswer(g, 'GER', [channel], g.byName.Kent);
    expect(c.come).toContain(dover);
    expect([...c.come, ...c.kept, ...c.shy]).not.toContain(wac);
    // A fleet sent raiding keeps to its raiding.
    const home = fleet(g, 'UK', 'Home Fleet');
    expect(c.come).toContain(home);
    home.duty = 'raid';
    const d = fleetsAnswer(g, 'GER', [channel], g.byName.Kent);
    expect([...d.come, ...d.kept, ...d.shy]).not.toContain(home);
  });

  it('keeps his fleets out of a sea the invader\'s aircraft command, unless their own homeland is invaded across it', () => {
    const g = opened();
    const skag = sea(g, 'skag'), north = sea(g, 'nsea');
    // The Skagerrak lies under the German air force.
    expect(airOver(g, skag, 'UK', true) - airOver(g, skag, 'UK', false)).toBeGreaterThan(NAVY.air.avoid);
    const a = fleetsAnswer(g, 'GER', [skag], g.byName.Jutland);
    expect(a.come).toEqual([]);
    expect(called(a.kept)).toContain('UK Home Fleet');
    const h = seaHold(g, 'GER', [skag], g.byName.Jutland);
    expect(h.come).toBe(0);
    expect(h.kept).toBeCloseTo(power(g, a.kept), 9);
    expect(h.open).toBe(true);
    expect(h.lines.some((l) => l.label === 'Enemy ships your aircraft keep away' && l.value === Math.round(h.kept).toLocaleString('en-US'))).toBe(true);
    expect(h.lines.some((l) => l.label === 'The sea behind the beach' && l.value === 'stays open')).toBe(true);
    // The North Sea under the same command: a beach in Norway keeps them all away, a beach in Yorkshire brings the
    // British fleet out and keeps the French one away still.
    g.nations.GER.air *= 12;
    expect(airOver(g, north, 'UK', true) - airOver(g, north, 'UK', false)).toBeGreaterThan(NAVY.air.avoid);
    const n = fleetsAnswer(g, 'GER', [north], g.byName.Bergen);
    expect(n.come).toEqual([]);
    expect(called(n.kept)).toEqual(expect.arrayContaining(['FRA Atlantic Fleet', 'UK Home Fleet']));
    const y = fleetsAnswer(g, 'GER', [north], g.byName.Yorkshire);
    expect(called(y.come)).toContain('UK Home Fleet');
    expect(called(y.kept)).toContain('FRA Atlantic Fleet');
    expect(called(y.kept)).not.toContain('UK Home Fleet');
  });

  it('sends none in alone against more than it can face: the invader\'s whole strength there, or half of it off the homeland', () => {
    const g = opened();
    const aegean = sea(g, 'aegean'), channel = sea(g, 'channel');
    const med = fleet(g, 'UK', 'Mediterranean Fleet');
    expect(fleetsAnswer(g, 'GER', [aegean], g.byName.Crete).come).toEqual([med]);
    germansIn(g, aegean, fleetPower(g, med) + 5);
    const a = fleetsAnswer(g, 'GER', [aegean], g.byName.Crete);
    expect(a.come).toEqual([]);
    expect(a.shy).toEqual([med]);
    const h = seaHold(g, 'GER', [aegean], g.byName.Crete);
    expect(h.open).toBe(true);
    expect(h.shy).toBeCloseTo(fleetPower(g, med), 9);
    expect(h.lines.some((l) => l.label === 'Enemy ships that will not risk it')).toBe(true);
    // Off Kent the Home Fleet comes out against odds it would not face off a foreign beach.
    const w = opened();
    const home = fleet(w, 'UK', 'Home Fleet');
    const mine = (): number => seaSide(w, channel, 'GER', false).total;
    const theirs = seaSide(w, channel, 'GER', true).total;
    germansIn(w, channel, (theirs + fleetPower(w, home)) * 1.5);
    expect(theirs + fleetPower(w, home)).toBeLessThan(mine());
    expect(theirs + fleetPower(w, home)).toBeGreaterThan(0.5 * mine());
    expect(fleetsAnswer(w, 'GER', [channel], w.byName.Kent).come).toContain(home);
    const f = fleetsAnswer(w, 'GER', [channel], w.byName.Normandy ?? w.byName.Cotentin);
    expect(f.come).not.toContain(home);
    expect(f.shy).toContain(home);
  });

  it('counts whole the fleets of a player who orders his own navy', () => {
    const g = opened();
    const skag = sea(g, 'skag'), channel = sea(g, 'channel');
    g.human = 'UK';
    const a = fleetsAnswer(g, 'GER', [skag], g.byName.Jutland);
    expect(called(a.come)).toContain('UK Home Fleet');
    expect(called(a.kept)).not.toContain('UK Home Fleet');
    expect(called(fleetsAnswer(g, 'GER', [channel], g.byName.Kent).come)).toContain('UK Western Approaches Fleet');
  });

  it('says what the admirals then do', () => {
    // A landing in Kent: every fleet the forecast names puts to sea in the Channel, and no other.
    const g = opened();
    const channel = sea(g, 'channel');
    const kent = g.byName.Kent;
    const a = fleetsAnswer(g, 'GER', [channel], kent);
    expect(a.come.length).toBeGreaterThan(1);
    g.flags[`landed:${channel}:GER`] = g.turn;
    kent.ctrl = 'GER';
    kent.since = g.turn;
    navalAI(g, 'UK');
    navalAI(g, 'FRA');
    const there = g.fleets.filter((f) => atWar(g, f.nat, 'GER') && surfaceShips(f) > 0 && atSea(f) && f.zone === channel);
    expect(called(there)).toEqual(called(a.come));
    // A landing in Jutland under the German air force: none comes into the Skagerrak.
    const d = opened();
    const skag = sea(d, 'skag');
    expect(fleetsAnswer(d, 'GER', [skag], d.byName.Jutland).come).toEqual([]);
    d.flags[`landed:${skag}:GER`] = d.turn;
    navalAI(d, 'UK');
    navalAI(d, 'FRA');
    expect(d.fleets.filter((f) => atWar(d, f.nat, 'GER') && surfaceShips(f) > 0 && atSea(f) && f.zone === skag)).toEqual([]);
    // A landing in Crete with a stronger German fleet off it: the Mediterranean Fleet stays away.
    const c = opened();
    const aegean = sea(c, 'aegean');
    const med = fleet(c, 'UK', 'Mediterranean Fleet');
    germansIn(c, aegean, fleetPower(c, med) + 5);
    c.flags[`landed:${aegean}:GER`] = c.turn;
    navalAI(c, 'UK');
    expect(atSea(med) && med.zone === aegean).toBe(false);
  });
});
