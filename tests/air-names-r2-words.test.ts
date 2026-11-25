// The design director's review marks of 05:12Z on the air names lane (design-director/air-names-1125.md, "Review
// marks (05:12Z)"), under the switches the lane already has (airNames for the arms' own names, nicknames for the earned
// names and the Guards title):
//   (a) Guards with a city: allowed ("5th Guards Smolensk Fighter Aviation Division" is the record's manner); the city
//       is the battle of its fight in this war, and it gives no edge;
//   (b) the hard-fight gap: pressed home at 0.8 or less, held at 1.25 or more, both G, in data; near even, ordinary;
//   (c) the block test also catches a real nickname's distinctive word, not only the whole string: no name of ours,
//       generated or listed, holds the distinctive word of a real unit's name ("Rattlers", of "Death Rattlers").
import { afterEach, describe, expect, it } from 'vitest';
import names from '../data/campaign/air-names.json';
import nick from '../data/campaign/nicknames.json';
import { formName, type Rung } from '../src/campaign/air';
import { AIRNAMES, airBlocked, distinctiveIn, type RungR2 } from '../src/campaign/airnames';
import { AIRNICKS, airEarn, airRaidFlown, blockedAir, earnedAirName } from '../src/campaign/airnicks';
import { createGame } from '../src/campaign/game';
import { setNicknames } from '../src/campaign/nicknames';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, AirSide, Game } from '../src/campaign/types';

const LADDERS = (names as unknown as { ladders: Record<string, (Rung & RungR2)[]> }).ladders;
const KINDS = ['ftr', 'bmr'] as const;
const BLOCKED = (names as unknown as { blocked: { names: string[]; common: string[] } }).blocked;
const CORPS_BLOCKED = (nick as unknown as { blocked: string[] }).blocked;

const side = (f: AirFleet, ftr: number, bmr = 0): AirSide => ({ nat: f.nat, fleet: f.name, id: f.id, base: f.base, leader: null, ftr, bmr, lostFtr: 0, lostBmr: 0 });
function nickWar(on = true): Game {
  const g = startOpen(createGame(1, null));
  if (on) setNicknames(g, true);
  return g;
}
afterEach(() => { airEarn.skip = false; });

/** Every name the lane can give: the ladders' every form and number (and the first past each list), the island bases, the arms' plain forms, the earned styles with a place, the Guards forms with and without a city. */
function everyName(place = 'Testplace'): string[] {
  const all: string[] = [];
  for (const rungs of Object.values(LADDERS)) for (const r of rungs) for (const k of KINDS) for (const form of [r.form[k], r.formR2?.[k], r.short?.[k]]) {
    if (!form) continue;
    const list = r.numbers?.[k] ?? [];
    const past = list.length && typeof list[list.length - 1] === 'number' ? [(list[list.length - 1] as number) + 1] : [];
    for (const n of [1, 2, 3, ...list, ...past]) all.push(formName(form, n));
    if (/\{base\}/.test(form)) for (const base of AIRNAMES.navyIslands) all.push(form.replace(/\{base\}/g, base));
  }
  for (const f of Object.values(AIRNAMES.fallback)) for (const k of KINDS) for (const n of [1, 2, 3, ...(f.numbers?.[k] ?? [])]) all.push(formName(f[k], n));
  for (const s of Object.values(AIRNICKS.styles)) {
    for (const n of s.any) all.push(n, s.show.replace('{name}', 'X').replace('{nick}', n));
    for (const t of s.place) all.push(t.replace('{place}', place));
    if (s.guards) for (const row of [s.guards.regiment, s.guards.division, s.guards.corps]) for (const k of KINDS) for (const n of [1, ...row[k].numbers]) all.push(formName(row[k].form, n), formName(row[k].form.replace('Guards', s.guards.city.replace('{city}', place)), n), formName(row[k].short, n));
  }
  return [...new Set(all)];
}

/** The test's own reading of the rule, from the data alone: a blocked name's words but the common ones and numbers. */
const WORD = /[\p{L}\p{N}]+(?:['’][\p{L}\p{N}]+)*/gu;
const words = (s: string): string[] => (s.match(WORD) ?? []).map((w) => w.toLowerCase());
const common = new Set((BLOCKED.common ?? []).map((w) => w.toLowerCase()));
const distinct = new Map<string, string>();
for (const b of [...BLOCKED.names, ...CORPS_BLOCKED]) for (const w of words(b)) if (!common.has(w) && !/^\p{N}+$/u.test(w) && !distinct.has(w)) distinct.set(w, b);

describe('(c) the block test catches a real name\'s distinctive word', () => {
  it('the common words are a list in data, holding the ruling\'s own examples; every other word of a blocked name is distinctive', () => {
    for (const w of ['the', 'black', 'red', 'flying', 'fighting', 'white', 'blue', 'green', 'one']) expect(common.has(w), w).toBe(true);
    // "Rattlers" was struck as too near a real squadron's "Death Rattlers": the list holds that name, and its distinctive word is caught.
    expect(BLOCKED.names).toContain('Death Rattlers');
    expect(distinct.get('rattlers')).toBe('Death Rattlers');
    for (const n of ['Rattlers', 'the Rattlers', 'Prairie Rattlers', 'Rattlers of Testplace']) {
      expect(airBlocked(n), n).toBe(true);
      expect(blockedAir(n), n).toBe(true);
    }
    // And so for any blocked nickname: a name holding its distinctive word alone is refused...
    expect(blockedAir('the Lone Tigers')).toBe(true); // Flying Tigers
    expect(blockedAir('the Ghost Riders')).toBe(true); // Ghost Division, Night Riders
    expect(blockedAir('Iron Swifts')).toBe(true); // Iron Wolf, Iron Brigade ...
    // ...while a common word alone is not ("Flying Tigers" blocks Tigers, not Flying).
    expect(blockedAir('the Flying Kestrels')).toBe(false);
    expect(blockedAir('the Red Kites')).toBe(false);
    // The rule in code is the rule in the data.
    for (const [w] of distinct) expect(distinctiveIn(`the ${w} x`), w).toEqual([w]);
  });

  it('no name the lane can give holds a distinctive word of any real unit\'s name, on either block list', () => {
    const all = everyName();
    expect(all.length).toBeGreaterThan(4000);
    const caught = all.filter((n) => words(n).some((w) => distinct.has(w))).map((n) => `${n} (${words(n).filter((w) => distinct.has(w)).map((w) => `${w}: ${distinct.get(w)}`).join(', ')})`);
    expect(caught).toEqual([]);
    for (const n of all) expect(airBlocked(n), n).toBe(false);
  });

  it('the names it caught are struck, each replaced by an original word in the same manner', () => {
    const S = AIRNICKS.styles;
    const swapped: [string, string, string][] = [
      ['german', 'Iron Swifts', 'Slate Swifts'], ['german', 'Summer Lightning', 'Summer Squall'],
      ['american', 'Sky Wranglers', 'Range Wranglers'], ['american', 'Dust Devils', 'Dust Twisters'], ['american', 'Tin Lizzies', 'Flivvers'],
      ['british', 'the Drizzle Boys', 'the Drizzle Wrens'], ['british', 'the Bell Ringers', 'the Thatchers'],
      ['polish', 'the Bell Founders', 'the Coopers'],
      ['japanese', 'the Sea Swallows', 'the Shore Swallows'], ['japanese', 'the Thunder Kites', 'the Squall Kites'],
      ['soviet', 'the Smiths of the Sky', 'the Cloud Smiths'],
      ['_default', 'the Sky Wardens', 'the Cloud Wardens'], ['_default', 'the Hill Hawks', 'the Fell Hawks'], ['_default', 'the High Larks', 'the Upland Larks'],
    ];
    for (const [k, was, now] of swapped) {
      expect(S[k].any, k).not.toContain(was);
      expect(S[k].any, k).toContain(now);
      expect(new Set(S[k].any).size).toBe(S[k].any.length);
    }
    expect(S.british.place).not.toContain('the Watch over {place}');
    expect(S.british.place).toContain('the Wardens of {place}');
    // The lists keep their length (8 at least, as the lane's own test asks).
    for (const s of Object.values(S)) expect(s.any.length).toBeGreaterThanOrEqual(8);
  });

  it('a place of this war holding a distinctive word is kept out of an earned name and a Guards title', () => {
    const g = nickWar();
    const sov = g.airFleets.find((x) => x.nat === 'SOV' && x.kind === 'ftr' && /Division$/.test(x.name))!;
    // "Tobruk" is the distinctive word of "Rats of Tobruk": no "Tobruk" in the title.
    const got = earnedAirName(g, sov, 'Tobruk');
    expect(got.name).toBeTruthy();
    expect(got.name).not.toContain('Tobruk');
    const ger = g.airFleets.find((x) => x.nat === 'GER' && x.kind === 'ftr')!;
    for (let k = 0; k < 20; k++) { ger.id += 1000; const n = earnedAirName(g, ger, 'Tobruk').nick ?? ''; expect(n).not.toContain('Tobruk'); }
  });
});

describe('(b) the hard-fight gap, in data', () => {
  it('pressed home at 0.8 or less, held at 1.25 or more; between the two an ordinary fight for both sides', () => {
    expect(AIRNICKS.hard.pressed).toBe(0.8);
    expect(AIRNICKS.hard.held).toBe(1.25);
    const count = (odds: number): { att: number; def: number } => {
      const g = nickWar();
      const b = g.airFleets.find((x) => x.nat === 'GER' && x.kind === 'bmr')!, d = g.airFleets.find((x) => x.nat === 'POL')!;
      airRaidFlown(g, 'bomb', { odds, bombers: 100, bombed: 90, E: 50, D: 40, sky: false }, [{ f: b, side: side(b, 0, 100) }], [{ f: d, side: side(d, 40) }], 5);
      return { att: b.done!.n, def: d.done!.n };
    };
    const twice = AIRNICKS.double;
    // Near even: ordinary for both.
    for (const odds of [0.81, 0.9, 1, 1.1, 1.24]) expect(count(odds), `${odds}`).toEqual({ att: 1, def: 1 });
    // Pressed home at 0.8 or less: hard for the raiders, not for the defence.
    for (const odds of [0.8, 0.5]) expect(count(odds), `${odds}`).toEqual({ att: twice, def: 1 });
    // Held at 1.25 or more: hard for the defence, not for the raiders.
    for (const odds of [1.25, 3]) expect(count(odds), `${odds}`).toEqual({ att: 1, def: twice });
  });
});

describe('(a) Guards with the city of its fight in this war', () => {
  it('a Soviet group made Guards after its fight over a city of this war takes the city after "Guards", and nothing else changes', () => {
    const g = nickWar();
    // Any city of this war's map the fight was over, wherever it lies (here a city held by the enemy, not a Soviet one).
    const city = g.regions.find((r) => r.ctrl === 'GER' && r.fac > 0 && !blockedAir(r.name))!;
    const f = g.airFleets.find((x) => x.nat === 'SOV' && x.kind === 'ftr' && /Fighter Aviation Division$/.test(x.name))!;
    // (Everything on the group but its name and the count, record and diary that give it.)
    const rules = (x: AirFleet): string => JSON.stringify(Object.entries(x).filter(([k]) => !['name', 'was', 'guards', 'short', 'roleNamed', 'nick', 'done', 'colours', 'log'].includes(k)));
    const keep = rules(f);
    f.done = { n: AIRNICKS.need - 2, at: 0, x: 0, h: 0 };
    // A fight held at odds of 1.25 (hard: double) brings it to the mark.
    airRaidFlown(g, 'bomb', { odds: AIRNICKS.hard.held, bombers: 100, bombed: 90, E: 80, D: 60, sky: false }, [], [{ f, side: side(f, 80) }], city.id);
    expect(f.done!.h).toBe(1);
    expect(f.guards).toBeDefined();
    expect(f.name).toMatch(new RegExp(`^\\d+(st|nd|rd|th) Guards ${city.name} Fighter Aviation Division$`));
    expect(f.nick).toBeUndefined();
    // No edge: the title is a name and a number; nothing the rules count has moved.
    expect(rules(f)).toBe(keep);
  });

  it('with the nicknames switch off, no title is given and nothing is written', () => {
    const g = nickWar(false);
    const before = saveGame(g);
    const city = g.regions.find((r) => r.ctrl === 'GER' && r.fac > 0)!;
    const f = g.airFleets.find((x) => x.nat === 'SOV' && x.kind === 'ftr')!;
    airRaidFlown(g, 'bomb', { odds: 3, bombers: 100, bombed: 90, E: 80, D: 60, sky: false }, [], [{ f, side: side(f, 80) }], city.id);
    expect(f.guards).toBeUndefined();
    expect(saveGame(g)).toBe(before);
  });
});
