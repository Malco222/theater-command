// Every group of ships is called a fleet (Versions 84.2.2 and 84.3; the player's ask of 17 November 2026: "it has atlantic
// squadron for the fleet name. ships should be called fleets"). Names and words only: no rule reads a fleet's name
// but the American staff's own list of the fleet it sends across the ocean, which is renamed with it.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { detach, squadronName } from '../src/campaign/fleets';
import { fleetWords, loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { WORLD } from '../src/campaign/world';
import type { Game } from '../src/campaign/types';
import navy from '../data/campaign/navy.json';
import world from '../data/campaign/world.json';

const wide = (): Game => startOpen(createGame(1, null));

describe('A group of ships is called a fleet', () => {
  it('no fleet on the first day is called a squadron, and America has its Atlantic Fleet', () => {
    const g = wide();
    for (const f of g.fleets) expect(f.name).not.toMatch(/Squadron/);
    for (const row of [...navy.fleets, ...world.fleets]) expect(row.name).not.toMatch(/Squadron/);
    // Version 84.3: every fleet on the first day says what it is, a fleet, or is a whole navy or Force H.
    for (const row of [...navy.fleets, ...world.fleets]) expect(row.name, row.name).toMatch(/ Fleet$| Navy$|^Force H$/);
    for (const nat of new Set(g.fleets.map((f) => f.nat))) { const mine = g.fleets.filter((f) => f.nat === nat).map((f) => f.name); expect(new Set(mine).size, nat).toBe(mine.length); }
    // No two fleets of one name in one sea: Germany's in the Baltic is not a second Baltic Fleet.
    expect(g.fleets.filter((f) => f.name === 'Baltic Fleet').map((f) => f.nat)).toEqual(['SOV']);
    expect(g.fleets.some((f) => f.nat === 'USA' && f.name === 'Atlantic Fleet')).toBe(true);
    expect(g.fleets.some((f) => f.nat === 'FRA' && f.name === 'Atlantic Fleet')).toBe(true);
    // The fleet the American staff sends across the ocean is still on its list.
    expect(WORLD.overseas.USA.fleet).toEqual(['Atlantic Fleet']);
  });

  it('ships split off form a fleet by its kind', () => {
    const g = wide();
    expect(squadronName(g, 'USA', { bb: 1, cv: 0, ca: 2, dd: 5, ss: 0 })).toBe('1st Battle Fleet');
    expect(squadronName(g, 'USA', { bb: 0, cv: 0, ca: 3, dd: 1, ss: 0 })).toBe('1st Cruiser Fleet');
    expect(squadronName(g, 'USA', { bb: 0, cv: 0, ca: 0, dd: 5, ss: 0 })).toBe('1st Escort Fleet');
    expect(squadronName(g, 'USA', { bb: 0, cv: 0, ca: 0, dd: 0, ss: 5 })).toBe('1st Submarine Fleet');
    const f = g.fleets.find((x) => x.nat === 'USA' && x.name === 'Atlantic Fleet')!;
    const part = detach(g, f, { dd: 4 });
    expect(part?.name).toBe('1st Escort Fleet');
  });

  it('a campaign saved under the old names is loaded under the new ones, with its wings, its news and its reports', () => {
    const g = wide();
    const f = g.fleets.find((x) => x.nat === 'USA' && x.name === 'Atlantic Fleet')!;
    // The save as an older build wrote it.
    f.name = 'Atlantic Squadron';
    const wings = g.airFleets.filter((w) => w.nat === 'USA' && w.name.startsWith('Atlantic Fleet'));
    expect(wings.length).toBeGreaterThan(0);
    for (const w of wings) w.name = w.name.replace('Atlantic Fleet', 'Atlantic Squadron');
    const part = detach(g, f, { dd: 4 })!;
    part.name = '1st Escort Group';
    g.headlines.push({ turn: g.turn, week: g.week, text: 'The Atlantic Squadron crosses the ocean to Plymouth.', kind: 'info', nat: 'USA' });
    const before = { ships: { ...f.ships }, base: f.base, zone: f.zone, id: f.id, n: g.fleets.length };
    // An older build wrote no mark of today's words on its save.
    const old = saveGame(g).replace('{"v":1,"words":1,', '{"v":1,');
    expect(old.startsWith('{"v":1,"seed":')).toBe(true);
    const h = loadGame(old)!;
    const now = h.fleets.find((x) => x.id === before.id)!;
    expect(now.name).toBe('Atlantic Fleet');
    expect([now.ships, now.base, now.zone, h.fleets.length]).toEqual([before.ships, before.base, before.zone, before.n]);
    expect(h.fleets.find((x) => x.id === part.id)!.name).toBe('1st Escort Fleet');
    expect(h.fleets.some((x) => /Squadron|Escort Group|Flotilla/.test(x.name))).toBe(false);
    for (const w of wings) expect(h.airFleets.find((x) => x.id === w.id)!.name).toMatch(/^Atlantic Fleet /);
    expect(h.headlines.some((x) => x.text === 'The Atlantic Fleet crosses the ocean to Plymouth.')).toBe(true);
    expect(h.headlines.some((x) => /Atlantic Squadron/.test(x.text))).toBe(false);
    // A save already under the new names comes back as it went in.
    const again = loadGame(saveGame(h))!;
    expect(again.fleets.map((x) => x.name)).toEqual(h.fleets.map((x) => x.name));
  });

  it('keeps a name that is not a squadron, and leaves the air groups alone', () => {
    expect(fleetWords('Home Fleet')).toBe('Home Fleet');
    expect(fleetWords('Force H')).toBe('Force H');
    expect(fleetWords('Norfolk Squadron')).toBe('Norfolk Fleet');
    expect(fleetWords('2nd Submarine Flotilla')).toBe('2nd Submarine Fleet');
    expect(fleetWords('The Mediterranean Squadron defeats the 2nd Squadron in the Tyrrhenian Sea')).toBe('The Mediterranean Fleet defeats the 2nd Fleet in the Tyrrhenian Sea');
    expect(fleetWords('"parent":"1st Air Squadron"')).toBe('"parent":"1st Air Squadron"');
    expect(fleetWords('The U-boat Arm sinks 9 ships of the Western Approaches Command; Dover Command, Baltic Station and Submarine Command are in port. I Bomber Command flies.'))
      .toBe('The U-boat Fleet sinks 9 ships of the Western Approaches Fleet; Dover Fleet, Swinemunde Fleet and Submarine Fleet are in port. I Bomber Command flies.');
    const g = wide();
    g.airFleets[0].name = '1st Air Squadron';
    const h = loadGame(saveGame(g))!;
    expect(h.airFleets.map((a) => a.name)).toEqual(g.airFleets.map((a) => a.name));
    expect(h.fleets.map((f) => f.name)).toEqual(g.fleets.map((f) => f.name));
    // No fleet carries an old word; an air group may be a squadron (ruling 220), and comes back as it went in.
    expect(h.fleets.some((f) => /Squadron|Escort Group|Flotilla/.test(f.name))).toBe(false);
    expect(saveGame(h)).toBe(saveGame(g));
  });
});
