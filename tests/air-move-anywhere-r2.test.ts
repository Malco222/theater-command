// Release 2: an air group sent to any airfield of its side's or its allies' with one order (Brian, 24 November 2026
// 22:08Z; the design director's rulings of 21:49Z and 22:10Z; src/campaign/ferry.ts, data/campaign/ferry.json). The
// switch is the game's flag 'ferry', off as shipped. Off, the move page and its orders are today's; on, the page lists
// every friendly field with the turns it takes and the enemy fighters known on the way, the group flies a few hops a
// month out of the fight, a closed sky holds it, enemy fighters in reach of a hop can catch it, and the staffs use it.
import { afterEach, describe, expect, it } from 'vitest';
import { AIR, airAI, whyNotBase } from '../src/campaign/air';
import { airAct, airPage } from '../src/campaign/airpages';
import { groupRow } from '../src/campaign/airwar';
import { allied, createGame } from '../src/campaign/game';
import { FERRY, ferryRoute, ferryRoutes, ferryMonth, legCovered, realKm, setFerry } from '../src/campaign/ferry';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderAirBase, stepAI } from '../src/campaign/turn';
import { skyAt } from '../src/campaign/weather';
import { fieldKey, openFields } from '../src/campaign/fieldstate';
import type { AirFleet, Game } from '../src/campaign/types';

const PER = FERRY.perMonth;
afterEach(() => { (FERRY as { perMonth: number }).perMonth = PER; });

const id = (g: Game, n: string): number => g.byName[n].id;
/** A field laid out by the build-a-field rule's own mark (air.ts seedField), on ground the nation holds. */
function field(g: Game, n: string, nat: string, cls = 1): void {
  const r = g.byName[n];
  g.flags[`af:${r.id}`] = Math.max(cls, g.flags[`af:${r.id}`] ?? 0);
  r.ctrl = nat;
  r.since = -5;
}
/** The British player's month, with the North Atlantic's fields built and held: Gander, Goose Bay, Greenland and Iceland. */
function atlantic(seed = 17, on = true): Game {
  const g = startOpen(createGame(seed, 'UK'));
  if (on) setFerry(g, true);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  for (const n of ['Gander', 'Labrador', 'Greenland', 'Iceland']) field(g, n, 'UK');
  return g;
}
const fighters = (g: Game, nat = 'UK'): AirFleet => g.airFleets.find((f) => f.nat === nat && f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined)!;

describe('the switch off', () => {
  it('leaves the move page and its orders as they were: no section by stages, the old section by sea, and no way written', () => {
    const on = atlantic(17, true), off = atlantic(17, false);
    const f = fighters(off), page = airPage(off, `rebase:${f.id}`)!;
    expect(page.sections.map((s) => s.heading)).not.toContain('FARTHER, BY STAGES');
    // The page is the same page the switch on starts from, but for the new section: the airfields this month are the same rows.
    const pon = airPage(on, `rebase:${fighters(on).id}`)!;
    expect(pon.sections[0]).toEqual(page.sections[0]);
    expect(pon.sections.map((s) => s.heading)).toContain('FARTHER, BY STAGES');
    expect(ferryRoutes(off, f).size).toBe(0);
    expect(airAct(off, `ferry:${f.id}:${id(off, 'Iceland')}`)).toBeNull();
    // A move today's rule allows is made as it always was, and nothing new is written on the group.
    const to = page.sections[0].rows.find((r) => r.buttons[0].ok)!;
    expect(orderAirBase(off, f.id, id(off, to.title))).toBe(true);
    expect(f.way).toBeUndefined();
    expect(JSON.stringify(off.airFleets)).not.toContain('"way"');
  });
});

describe('the search', () => {
  it('measures the fields on the globe, not on the chart', () => {
    const g = atlantic();
    // The chart puts Greenland about 2,000 km from Iceland; the fields stand about 1,240 km apart (historian: 769 miles).
    expect(realKm(g, id(g, 'Greenland'), id(g, 'Iceland'))).toBeGreaterThan(1200);
    expect(realKm(g, id(g, 'Greenland'), id(g, 'Iceland'))).toBeLessThan(1280);
    expect(realKm(g, id(g, 'Labrador'), id(g, 'Greenland'))).toBeLessThan(1300);
  });

  it('finds a chain across the North Atlantic by Greenland and Iceland, and counts its months', () => {
    const g = atlantic();
    const f = fighters(g);
    const t0 = performance.now();
    const all = ferryRoutes(g, f);
    const ms = performance.now() - t0;
    const r = all.get(id(g, 'Nova Scotia'))!;
    expect(r.how).toBe('air');
    const names = r.hops.map((h) => g.regions[h.to].name);
    expect(names).toContain('Greenland');
    expect(names).toContain('Iceland');
    expect(names[names.length - 1]).toBe('Nova Scotia');
    for (const h of r.hops) expect(h.km).toBeLessThanOrEqual(FERRY.range.ftr);
    expect(r.turns).toBe(Math.ceil(r.hops.length / FERRY.perMonth));
    expect(r.words).toMatch(/^2 turns by .*Greenland.*Iceland|^2 turns by .*Iceland.*Greenland/);
    expect(r.words).toContain('No enemy fighters known on the way.');
    // Cheap enough for a page that lists every field (the whole map at once; generous for a loaded machine).
    expect(ms).toBeLessThan(500);
  });

  it('sends a group too short-legged for the ocean crated by sea, and says why', () => {
    const g = atlantic();
    // Iceland's field gone: from Greenland the next field is too far for fighters.
    delete g.flags[`af:${id(g, 'Iceland')}`];
    const f = fighters(g);
    const r = ferryRoute(g, f, id(g, 'Nova Scotia'))!;
    expect(r.how).toBe('sea');
    expect(r.words).toMatch(/^\d turns?: no chain of fields in range, so it goes crated by sea\./);
    expect(r.words).toMatch(/Its fighters fly at most 1,400 km in one hop/);
    // A field on this side of the ocean beyond every chain, with no way by sea, is told plainly why.
    const none = [...ferryRoutes(g, f).values()].find((x) => x.how === 'none');
    if (none) expect(none.why).toBeTruthy();
  });

  it('offers allied fields by the friendly rule only', () => {
    const g = atlantic();
    const f = fighters(g);
    const all = ferryRoutes(g, f);
    expect(all.size).toBeGreaterThan(10);
    for (const r of all.values()) expect(allied(g, 'UK', g.regions[r.to].ctrl)).toBe(true);
    // A neutral's field is never a stop or a destination.
    const dub = id(g, 'Dublin');
    expect(all.has(dub)).toBe(false);
    for (const r of all.values()) for (const h of r.hops) expect(h.to).not.toBe(dub);
    // Canada's fields are its ally's: offered.
    expect(all.get(id(g, 'Montreal'))?.how).toBe('air');
  });

  it('prefers a way with no known enemy fighters at one more turn, and not at two', () => {
    const g = atlantic();
    (FERRY as { perMonth: number }).perMonth = 1; // one hop a turn, so that turns count hops (a fixture, not the data)
    const ger = g.airFleets.filter((x) => x.nat === 'GER' && x.kind === 'ftr' && x.ftr > 0);
    // German fighters on a field at Paris, seen whole from British ground: the quick ways south and east pass in their reach.
    field(g, 'Paris', 'GER');
    ger[0].base = id(g, 'Paris'); ger[0].mission = 'support';
    const f = fighters(g);
    const all = [...ferryRoutes(g, f).values()];
    const taken = all.filter((r) => r.safer?.taken && r.safer.turns === r.safer.quick + 1);
    const left = all.filter((r) => r.safer && !r.safer.taken);
    expect(taken.length).toBeGreaterThan(0);
    expect(left.length).toBeGreaterThan(0);
    for (const r of taken) { expect(r.risky).toEqual([]); expect(r.words).toContain('It goes the safer way, one turn longer than the quickest'); }
    for (const r of left) { expect(r.safer!.turns).toBeGreaterThanOrEqual(r.safer!.quick + 2); expect(r.risky.length).toBeGreaterThan(0); expect(r.words).toMatch(/Risk: enemy fighters seen within reach of the (first|second|third)/); expect(r.words).toMatch(/A way with no enemy fighters known on it takes \d turns/); }
  });
});

describe('the order', () => {
  it('stores the destination, flies the month\'s hops at once, is out of the fight on the way, and arrives', () => {
    const g = atlantic();
    const f = fighters(g);
    const to = id(g, 'Nova Scotia');
    const res = airAct(g, `ferry:${f.id}:${to}`)!;
    expect(res.ok).toBe(true);
    expect(res.note).toMatch(/sets off for Nova Scotia: 2 turns/);
    expect(f.way?.to).toBe(to);
    expect(f.way?.stops.length).toBe(1);
    expect(g.regions[f.base].name).not.toBe('Kent');
    // Out of the fight: its card says so, and no other move takes it while it goes.
    const row = groupRow(g, f);
    expect(row.doing).toMatch(/^On its way to Nova Scotia, 1 hop to go: out of the fight/);
    expect(row.way).toEqual({ to, toName: 'Nova Scotia', left: 1, sea: false });
    expect(whyNotBase(g, f, g.regions[to])).toMatch(/on its way/);
    const page = airPage(g, `group:${f.id}`)!;
    expect(page.top.map((b) => b.label)).toContain('CALL IT OFF');
    // Next month it flies the last hop and is there, its readiness cut once, as for any move.
    g.turn++;
    const ready = f.ready;
    ferryMonth(g, 'UK');
    expect(f.base).toBe(to);
    expect(f.way).toBeUndefined();
    expect(f.ready).toBeCloseTo(Math.max(AIR.ready.floor, ready - AIR.ready.move), 9);
    expect(g.headlines.some((h) => h.text.includes('reaches Nova Scotia'))).toBe(true);
  });

  it('is held by a closed sky over the next field, by the weather rule alone, and goes on when it clears', () => {
    const g = atlantic();
    g.flags.weather = 1;
    const f = fighters(g);
    const ice = id(g, 'Iceland'), gre = id(g, 'Greenland');
    // The group stands at Greenland, bound for Clydeside by Iceland (one weather zone with Greenland). The first month of
    // the seed's own deal with the sky over them closed, and the next with it and Clydeside's open.
    f.base = gre;
    const now = g.turn;
    let t = now;
    for (; t < now + 72; t++) { g.turn = t; if (skyAt(g, ice) === 'closed') break; }
    expect(t).toBeLessThan(now + 72);
    let u = t + 1;
    for (; u < t + 72; u++) { g.turn = u; if (skyAt(g, ice) !== 'closed' && skyAt(g, gre) !== 'closed' && skyAt(g, id(g, 'Clydeside')) !== 'closed') break; }
    g.turn = t;
    f.moved = -1;
    const r = ferryRoute(g, f, id(g, 'Clydeside'))!;
    expect(r.hops.map((h) => h.to)).toEqual([ice, id(g, 'Clydeside')]);
    expect(airAct(g, `ferry:${f.id}:${id(g, 'Clydeside')}`)!.ok).toBe(true);
    expect(f.base).toBe(gre);
    expect(f.way?.stops).toEqual([ice, id(g, 'Clydeside')]);
    expect(g.headlines.some((h) => h.text === `${f.name} is held at Greenland: the sky is closed on the way to Iceland.`)).toBe(true);
    // Under an open sky it goes on, both hops in the month.
    g.turn = u;
    ferryMonth(g, 'UK');
    expect(f.base).toBe(id(g, 'Clydeside'));
    expect(f.way).toBeUndefined();
  });

  it('warns of enemy fighters in reach of a hop and still goes, and they can catch it by the existing rule', () => {
    const g = atlantic();
    const f = fighters(g);
    // Ulster gone, so that the only way to Iceland is by Clydeside, under German fighters put in the Highlands.
    delete g.flags[`af:${id(g, 'Ulster')}`];
    const e = g.airFleets.find((x) => x.nat === 'GER' && x.kind === 'ftr' && x.ftr > 0)!;
    field(g, 'Highlands', 'GER');
    e.base = id(g, 'Highlands'); e.mission = 'support'; delete e.close;
    const r = ferryRoute(g, f, id(g, 'Iceland'))!;
    expect(r.how).toBe('air');
    expect(r.risky.length).toBeGreaterThan(0);
    expect(r.words).toMatch(/Risk: enemy fighters seen within reach of the (first|second)( and second)? hops?\./);
    const page = airPage(g, `rebase:${f.id}`)!;
    const row = page.sections.find((s) => s.heading === 'FARTHER, BY STAGES')?.rows.find((x) => x.title === 'Iceland') ?? page.sections[0].rows.find((x) => x.title === 'Iceland');
    expect(row).toBeTruthy();
    // It still goes, and on the covered leg the battle is fought (the report marks the field it was flying to).
    const hop = r.hops[r.risky[0]];
    expect(legCovered(g, f, hop.from, hop.to)).toBe(true);
    const before = g.airReports.length, had = f.ftr + e.ftr;
    expect(airAct(g, `ferry:${f.id}:${id(g, 'Iceland')}`)!.ok).toBe(true);
    const rep = g.airReports.slice(before).find((x) => x.ferry === id(g, 'Iceland'));
    expect(rep).toBeTruthy();
    expect(rep!.attNat).toBe('UK');
    expect(rep!.def.some((s) => s.id === e.id)).toBe(true);
    expect(rep!.sky).toBe(false);
    expect(f.ftr + e.ftr).toBeLessThanOrEqual(had);
    expect(g.headlines.some((h) => h.text.includes('on its way to Iceland'))).toBe(true);
  });

  it('can be called off, and stays at the field it has reached', () => {
    const g = atlantic();
    const f = fighters(g);
    expect(airAct(g, `ferry:${f.id}:${id(g, 'Nova Scotia')}`)!.ok).toBe(true);
    const at = f.base;
    const res = airAct(g, `ferryoff:${f.id}`)!;
    expect(res.ok).toBe(true);
    expect(f.way).toBeUndefined();
    expect(f.base).toBe(at);
    expect(groupRow(g, f).doing).not.toMatch(/On its way/);
  });

  it('a saved war carries it mid-journey, and it arrives after loading', () => {
    const g = atlantic();
    const f = fighters(g);
    expect(airAct(g, `ferry:${f.id}:${id(g, 'Nova Scotia')}`)!.ok).toBe(true);
    const h = loadGame(saveGame(g))!;
    expect(h).not.toBeNull();
    const f2 = h.airFleets.find((x) => x.id === f.id)!;
    expect(f2.way).toEqual(f.way);
    h.turn++;
    ferryMonth(h, 'UK');
    expect(f2.base).toBe(id(h, 'Nova Scotia'));
  });
});

describe('the staffs', () => {
  it('a computer staff ferries a spare fighter group forward to a field that asks, by stages', () => {
    const make = (on: boolean): Game => {
      const g = startOpen(createGame(17, 'GER'));
      if (on) setFerry(g, true);
      // A British lodgment at Narvik, beside German-held ground, with fields at Trondheim and Narvik beyond a month's flight from England.
      field(g, 'Trondheim', 'UK');
      field(g, 'Narvik', 'UK', 2);
      g.byName.Finnmark.ctrl = 'GER';
      g.byName.Nordland.ctrl = 'GER';
      const c = g.corps.find((x) => x.nat === 'UK' && !x.dead && x.kind !== 'fort')!;
      g.regions[c.reg].units = g.regions[c.reg].units.filter((u) => u !== c);
      c.reg = id(g, 'Narvik');
      g.regions[c.reg].units.push(c);
      return g;
    };
    const on = make(true), off = make(false);
    airAI(on, 'UK');
    airAI(off, 'UK');
    const nar = id(on, 'Narvik');
    const going = on.airFleets.filter((f) => f.nat === 'UK' && (f.way?.to === nar || (f.base === nar && f.moved === on.turn)));
    expect(going.length).toBeGreaterThan(0);
    // Without the switch no group gets there this month, and none is on its way.
    expect(off.airFleets.some((f) => f.nat === 'UK' && f.base === nar)).toBe(false);
    expect(off.airFleets.some((f) => f.way !== undefined)).toBe(false);
  });
});

describe('review: a shut runway', () => {
  it('lets nobody out by stages either: a group on a cratered field is held there until it is repaired', () => {
    const g = atlantic();
    openFields(g);
    const f = fighters(g);
    const home = f.base;
    // Its own runway cratered (airfields as targets): today's move refuses it, and so must the first hop.
    g.flags[fieldKey('fsh', home)] = g.turn;
    expect(whyNotBase(g, f, g.regions[id(g, 'Nova Scotia')], true)).toMatch(/cratered/);
    expect(airAct(g, `ferry:${f.id}:${id(g, 'Nova Scotia')}`)!.ok).toBe(true);
    expect(f.base).toBe(home);
    expect(f.way?.to).toBe(id(g, 'Nova Scotia'));
    // Repaired, it sets off as its next month opens.
    delete g.flags[fieldKey('fsh', home)];
    g.turn++;
    ferryMonth(g, 'UK');
    expect(f.base).not.toBe(home);
  });
});
