// Release 1.5.2: the names of new air groups (found in play). Britain's No. 11 Group grows from 220 to 330 fighters in
// September 1940, past what one group may hold (airwar.json groups.most), and a new group forms beside it. Britain's
// ladder counted its groups from 1 whatever their role, so the new Spitfire group was "No. 3 Group", a bomber number.
//   - a name a dated row of air.json will bring later (No. 10 Group, October 1940) is never taken before it comes;
//   - Britain's groups take numbers of their own kind, as the record numbered them (the historian's read row, L):
//     fighters 9, 10, 11, 12, 13, 14, 82, 83, 84, 85, 88; bombers 1, 2, 3, 4, 5, 7, 8, and then 6, which the record
//     gave Canada and which is taken only while no living group of any nation carries "No. 6";
//   - where a kind's list runs out, the lowest whole number above every number in both lists that is free (89, 90,
//     ...): never a number of the other kind, and no counter is kept;
//   - a command cut into groups at once whose own name is one its list gives ("No. 11 Group") keeps it on the piece its
//     leader stays with. Any other command's pieces each take a ladder name, as the shipped tests hold the opening's
//     names (tests/airwar.test.ts line 169, tests/airversion.test.ts line 53).
// Names are saved state: a campaign saved before keeps the names it has.
import { describe, expect, it } from 'vitest';
import { AIR, airUpkeep, splitFleet } from '../src/campaign/air';
import { createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { loadGame } from '../src/campaign/save';
import type { AirFleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;

const FIGHTERS = [9, 10, 11, 12, 13, 14, 82, 83, 84, 85, 88].map((n) => `No. ${n} Group`);
const BOMBERS = [1, 2, 3, 4, 5, 7, 8, 6].map((n) => `No. ${n} Group`);
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
    expect(names).toEqual(['No. 11 Group', 'No. 9 Group']);
    for (const n of names) expect(FIGHTERS).toContain(n);
    expect(names).not.toContain('No. 10 Group');
  });

  it("a war opened in 1941 (UK from 17, seed 6): Keith Park's group is No. 11 Group and the second piece No. 9 Group, the names a war played from 1939 reaches", () => {
    TC.start({ view: 'grand', human: 'UK', from: 17, seed: 6, world: true });
    const g = loadGame(TC.save().text)!;
    expect(groupsOf(g, 'UK', 'No. 11 Group').map((f) => [f.name, f.leader?.name ?? null])).toEqual([['No. 11 Group', 'Keith Park'], ['No. 9 Group', null]]);
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

  it('a name a later dated row brings is never taken before it comes; past the end of the list, 89', () => {
    const g = startOpen(createGame(1, 'UK'));
    g.turn = 9;
    // (No. 12 Group cut into nine: it keeps its name on its first piece, every other free number of the fighter list
    // is given, No. 10 Group never)
    const out = splitFleet(g, command(g, 'UK', 'No. 12 Group', 9 * 230, 0));
    const names = out.map((f) => f.name);
    expect(later('UK')).toContain('No. 10 Group');
    expect(names).not.toContain('No. 10 Group');
    expect(names).toEqual(['No. 12 Group', 'No. 9 Group', 'No. 14 Group', 'No. 82 Group', 'No. 83 Group', 'No. 84 Group', 'No. 85 Group', 'No. 88 Group', 'No. 89 Group']);
    expect(g.flags['agn:UK:ftr:group']).toBeUndefined();
    expect(new Set(g.airFleets.filter((f) => f.nat === 'UK').map((f) => f.name)).size).toBe(g.airFleets.filter((f) => f.nat === 'UK').length);
    // Once it has formed, its name is its own, and no other takes it.
    g.turn = 10;
    airUpkeep(g, 'UK');
    expect(g.airFleets.filter((f) => f.nat === 'UK' && f.name === 'No. 10 Group').length).toBe(1);
  });
});

describe("Britain's group numbers when a list runs out, and a command's number staying with its leader", () => {
  const numberOf = (name: string): number => Number(/^No\. (\d+) Group$/.exec(name)?.[1] ?? NaN);
  const FTR = FIGHTERS.map(numberOf), BMR = BOMBERS.map(numberOf);

  it("a: Britain's eighth bomber group is No. 6 Group while no living group carries 'No. 6'; with one that does, 89 (UK, from 55, seed 31)", () => {
    TC.start({ view: 'grand', human: 'UK', from: 55, seed: 31, world: true });
    const g = loadGame(TC.save().text)!;
    const bombers = g.airFleets.filter((f) => f.nat === 'UK' && f.kind === 'bmr' && f.ship === undefined).map((f) => f.name).sort();
    expect(bombers).toEqual([...BOMBERS].sort());
    // (the same split made again with a group of another nation named "No. 6 Squadron")
    const h = startOpen(createGame(1, 'UK'));
    h.turn = 9;
    h.airFleets.find((f) => f.nat === 'GER')!.name = 'No. 6 Squadron';
    const out = splitFleet(h, command(h, 'UK', 'Bomber Command', 0, 8 * 230));
    expect(out.map((f) => f.name)).toEqual(['No. 1 Group', 'No. 2 Group', 'No. 3 Group', 'No. 4 Group', 'No. 5 Group', 'No. 7 Group', 'No. 8 Group', 'No. 89 Group']);
    // ("No. 60 Squadron" does not hold "No. 6")
    const k = startOpen(createGame(1, 'UK'));
    k.turn = 9;
    k.airFleets.find((f) => f.nat === 'GER')!.name = 'No. 60 Squadron';
    expect(splitFleet(k, command(k, 'UK', 'Bomber Command', 0, 8 * 230)).map((f) => f.name)[7]).toBe('No. 6 Group');
  });

  it('b: a kind whose list is used up takes 89, 90, ..., never a number of the other kind; the counters are not written, and an old save holding them is not read', () => {
    const g = startOpen(createGame(1, 'UK'));
    g.turn = 9;
    // (an old save's counters, at a bomber number and a fighter number)
    g.flags['agn:UK:ftr:group'] = 2;
    g.flags['agn:UK:bmr:group'] = 8;
    const bmr = splitFleet(g, command(g, 'UK', 'Bomber Command', 0, 10 * 230)).map((f) => f.name);
    expect(bmr).toEqual([...BOMBERS, 'No. 89 Group', 'No. 90 Group']);
    const ftr = splitFleet(g, command(g, 'UK', 'No. 12 Group', 11 * 230, 0)).map((f) => f.name);
    // (No. 12 Group keeps its name; 9, 14, 82 to 88 and then 91 and on, 89 and 90 being Bomber Command's now, 10 a dated row's)
    expect(ftr).toEqual(['No. 12 Group', 'No. 9 Group', 'No. 14 Group', 'No. 82 Group', 'No. 83 Group', 'No. 84 Group', 'No. 85 Group', 'No. 88 Group', 'No. 91 Group', 'No. 92 Group', 'No. 93 Group']);
    for (const n of ftr.map(numberOf)) expect(BMR).not.toContain(n);
    expect([g.flags['agn:UK:ftr:group'], g.flags['agn:UK:bmr:group']]).toEqual([2, 8]);
    // A disbanded group's number may be given again: names go by the living groups.
    g.airFleets = g.airFleets.filter((f) => f.name !== 'No. 89 Group');
    expect(splitFleet(g, command(g, 'UK', 'Mediterranean Air Command', 0, 2 * 230)).map((f) => f.name)).toEqual(['No. 89 Group', 'No. 94 Group']);
  });

  it("c: a command whose own name is not one its list gives names every piece from the ladder, as before (Bomber Command; Luftflotte 1)", () => {
    const g = startOpen(createGame(1, 'UK'));
    g.turn = 9;
    expect(splitFleet(g, command(g, 'UK', 'Bomber Command', 0, 460)).map((f) => f.name)).toEqual(['No. 1 Group', 'No. 2 Group']);
    const h = startOpen(createGame(1, 'GER'));
    expect(h.airFleets.filter((f) => f.parent === 'Luftflotte 1').map((f) => f.name)).toEqual(['Jagdgeschwader 1', 'Jagdgeschwader 2', '1. Fliegerdivision', '2. Fliegerdivision']);
  });

  describe('a whole war nobody plays, to month 60 (seed 6): no British group takes a number of the other kind, and every name is its own', () => {
    const g = startOpen(createGame(6, null));
    const seen = new Set<string>();
    const check = (): void => {
      for (const f of g.airFleets.filter((x) => x.nat === 'UK' && x.ship === undefined)) {
        const n = numberOf(f.name);
        seen.add(f.name);
        if (f.kind === 'ftr') expect(BMR, `${g.turn} ${f.name}`).not.toContain(n);
        if (f.kind === 'bmr') expect(FTR, `${g.turn} ${f.name}`).not.toContain(n);
      }
      // (each nation's names its own; and no British group number is any other living group's. Names of other forms
      // are shared across nations as before: "1st Fighter Group", "Atlantic Fleet carrier fighters")
      for (const nat of new Set(g.airFleets.map((f) => f.nat))) {
        const names = g.airFleets.filter((f) => f.nat === nat).map((f) => f.name);
        expect(new Set(names).size, `${g.turn} ${nat}`).toBe(names.length);
      }
      for (const f of g.airFleets.filter((x) => x.nat === 'UK' && /^No\. \d+ Group$/.test(x.name))) expect(g.airFleets.filter((x) => x.name === f.name).length, `${g.turn} ${f.name}`).toBe(1);
    };
    // (fifteen months a test, so that each stays within the half minute every test is given)
    for (const [a, b] of [[1, 15], [16, 30], [31, 45], [46, 60]]) {
      it(`months ${a} to ${b}`, () => {
        for (let m = a; m <= b; m++) { runTurnAI(g); check(); }
        expect(g.turn).toBe(b + 1);
      });
    }
    it('the rung kept no counter, and the lists were reached', () => {
      expect(g.flags['agn:UK:ftr:group']).toBeUndefined();
      expect(g.flags['agn:UK:bmr:group']).toBeUndefined();
      expect([...seen].filter((x) => /^No\. \d+ Group$/.test(x)).length).toBeGreaterThan(10);
    });
  });
});
