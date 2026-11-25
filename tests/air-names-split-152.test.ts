// Release 1.5.2: the names of new air groups (found in play). Britain's No. 11 Group grows from 220 to 330 fighters in
// September 1940, past what one group may hold (airwar.json groups.most), and a new group forms beside it. Britain's
// ladder counted its groups from 1 whatever their role, so the new Spitfire group was "No. 3 Group", a bomber number.
//   - a name a dated row of air.json will bring later (No. 10 Group, October 1940) is never taken before it comes;
//   - Britain's groups take numbers of their own kind, as the record numbered them (the historian's read row, L):
//     fighters 9, 10, 11, 12, 13, 14, 82, 83, 84, 85, 88; bombers 1, 2, 3, 4, 5, 7, 8 (No. 6 went to Canada). Where a
//     list runs out, the ladder counts on as it did before.
// A command cut into groups all at once (splitFleet) still names every piece from the ladder, its first too: the shipped
// tests hold the opening's names to that (tests/airwar.test.ts line 169, tests/airversion.test.ts line 53).
// Names are saved state: a campaign saved before keeps the names it has.
import { describe, expect, it } from 'vitest';
import { AIR, airUpkeep, splitFleet } from '../src/campaign/air';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { loadGame } from '../src/campaign/save';
import type { AirFleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;

const FIGHTERS = [9, 10, 11, 12, 13, 14, 82, 83, 84, 85, 88].map((n) => `No. ${n} Group`);
const BOMBERS = [1, 2, 3, 4, 5, 7, 8].map((n) => `No. ${n} Group`);
/** Names the dated rows of air.json bring for a nation after the opening. */
const later = (nat: string): string[] => AIR.fleets.filter((r) => r.nat === nat && (r.from ?? 1) > 1).map((r) => r.name);
const groupsOf = (g: Game, nat: string, parent: string): AirFleet[] => g.airFleets.filter((f) => f.nat === nat && f.ship === undefined && f.parent === parent);

/** A command as history lists it, not yet cut into groups, at `ftr` fighters and `bmr` bombers, on the base of its row. */
function command(g: Game, nat: string, name: string, ftr: number, bmr: number): AirFleet {
  const row = AIR.fleets.find((r) => r.nat === nat && r.name === name)!;
  g.airFleets = g.airFleets.filter((f) => !(f.nat === nat && (f.parent ?? f.name) === name));
  const f: AirFleet = {
    id: g.nextAir++, nat, name, base: g.regions.find((r) => r.name === row.base)!.id, ftr, bmr, est: { ftr, bmr },
    mission: 'support', target: -1, ready: 1, leader: null, moved: 0, lost: { ftr: 0, bmr: 0 },
  };
  g.airFleets.push(f);
  return f;
}

describe('Britain numbers its new groups by their kind, and no name a later row brings is taken early', () => {
  it("Britain, September 1940, in play: the group No. 11 Group grows beside it is No. 9 Group, not a bomber number", () => {
    const g = startOpen(createGame(1, 'UK'));
    g.turn = 8;
    airUpkeep(g, 'UK');
    g.turn = 9;
    airUpkeep(g, 'UK');
    expect(groupsOf(g, 'UK', 'No. 11 Group').map((f) => f.name)).toEqual(['No. 11 Group', 'No. 9 Group']);
    expect(g.headlines.some((h) => h.text === 'No. 9 Group forms in Kent, a new group of No. 11 Group.')).toBe(true);
  });

  it("Britain, September 1940: No. 11 Group cut in two at once is No. 11 Group and a fighter number that no dated row brings", () => {
    const g = startOpen(createGame(1, 'UK'));
    g.turn = 9;
    const out = splitFleet(g, command(g, 'UK', 'No. 11 Group', 330, 0));
    expect(out.length).toBe(2);
    const names = out.map((f) => f.name);
    expect([...names].sort()).toEqual(['No. 11 Group', 'No. 9 Group']);
    for (const n of names) expect(FIGHTERS).toContain(n);
    expect(names).not.toContain('No. 10 Group');
  });

  it('a war opened in 1941 (UK from 17, seed 6): the Spitfires of No. 11 Group are No. 9 Group and No. 11 Group, no longer No. 1 and No. 2 Group', () => {
    TC.start({ view: 'grand', human: 'UK', from: 17, seed: 6, world: true });
    const g = loadGame(TC.save().text)!;
    expect(groupsOf(g, 'UK', 'No. 11 Group').map((f) => f.name)).toEqual(['No. 9 Group', 'No. 11 Group']);
    expect(groupsOf(g, 'UK', 'Bomber Command').map((f) => f.name)).toEqual(['No. 1 Group', 'No. 2 Group']);
  });

  it('a bomber command cut into groups gives them bomber numbers', () => {
    const g = startOpen(createGame(1, 'UK'));
    g.turn = 9;
    const out = splitFleet(g, command(g, 'UK', 'Bomber Command', 0, 700));
    expect(out.length).toBe(3);
    for (const f of out) expect(BOMBERS, f.name).toContain(f.name);
    expect(new Set(g.airFleets.filter((f) => f.nat === 'UK').map((f) => f.name)).size).toBe(g.airFleets.filter((f) => f.nat === 'UK').length);
  });

  it("another nation's groups are named as they were (Germany, September 1939)", () => {
    const g = startOpen(createGame(1, 'GER'));
    // (as the war opened at fca8f91d)
    const before: Record<string, string[]> = {
      'Luftflotte 1': ['Jagdgeschwader 1', 'Jagdgeschwader 2', '1. Fliegerdivision', '2. Fliegerdivision'],
      'Luftflotte 2': ['1. Jagddivision', '3. Fliegerdivision', '4. Fliegerdivision'],
      'Luftflotte 3': ['2. Jagddivision', 'Kampfgeschwader 1', 'Kampfgeschwader 2'],
      'Luftflotte 4': ['Jagdgeschwader 3', '5. Fliegerdivision', '6. Fliegerdivision'],
    };
    for (const [name, was] of Object.entries(before)) expect(groupsOf(g, 'GER', name).map((f) => f.name), name).toEqual(was);
  });

  it('a name a later dated row brings is never taken before it comes; past the end of the list the ladder counts on', () => {
    const g = startOpen(createGame(1, 'UK'));
    g.turn = 9;
    // (No. 12 Group cut into nine: every free number of the fighter list is given, No. 10 Group never)
    const out = splitFleet(g, command(g, 'UK', 'No. 12 Group', 9 * 230, 0));
    const names = out.map((f) => f.name);
    expect(later('UK')).toContain('No. 10 Group');
    expect(names).not.toContain('No. 10 Group');
    expect(names.slice(0, 8)).toEqual(['No. 9 Group', 'No. 12 Group', 'No. 14 Group', 'No. 82 Group', 'No. 83 Group', 'No. 84 Group', 'No. 85 Group', 'No. 88 Group']);
    // (the ninth: the list run out, the rung's old count, which skips the numbers in use)
    expect(names[8]).toMatch(/^No\. \d+ Group$/);
    expect(new Set(g.airFleets.filter((f) => f.nat === 'UK').map((f) => f.name)).size).toBe(g.airFleets.filter((f) => f.nat === 'UK').length);
    // Once it has formed, its name is its own, and no other takes it.
    g.turn = 10;
    airUpkeep(g, 'UK');
    expect(g.airFleets.filter((f) => f.nat === 'UK' && f.name === 'No. 10 Group').length).toBe(1);
  });
});
