// Ruling 221 (17 November 2026, 18:39Z) on the Pacific planner's paper 20, what Szechwan costs Japan when history is
// loose. Five switches, each off unless data turns it on:
//   H  a setup row marked 'stay' keeps its hold through a general offensive's release (ai.ts release), every stop;
//   L  the long supply line on the ground of the owners in east.json 'lineOwners', counted from the frontier and from
//      the harbours the invader holds there, with none of the Russian winter, at the stops in 'lineStops';
//   S  a staff reckons that line in its sums of strength as it reckons the winter (staff.ts fronts, 'lineReckon');
//   F  troops lifted off the coast of a country their own is fighting land at the nearest harbour their country holds
//      on that ground, not at home (game.ts refuge, 'nearRefuge'), every stop, the player's army too;
//   R  China among the staffs that will not be ringed (rivals.json, data alone).
// China and Japan are not on main's chart until step 13, so the rules are tried on Europe's ground with Poland and
// Norway standing in, and the data the ruling makes (data/campaign/china-stop3.json) is checked for its shape.
import { afterEach, describe, expect, it } from 'vitest';
import stop3 from '../data/campaign/china-stop3.json';
import { release } from '../src/campaign/ai';
import { EASTERN, addCorps, computeSupply, createGame, declareWar, evacuate, frozen, lineDrag, lineOwner, lineOwners, lineStrength, refuge, removeCorps } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { FAR_STAGE } from '../src/campaign/stage';
import { railheadAt } from '../src/campaign/rails';
import { fronts, startOpen } from '../src/campaign/staff';
import type { Corps, Game, Region } from '../src/campaign/types';
import { STACK } from '../src/campaign/types';

const E = EASTERN as unknown as Record<string, unknown>;
const KEYS = ['lineOwners', 'lineStops', 'lineReckon', 'nearRefuge'] as const;
const kept: Partial<Record<(typeof KEYS)[number], unknown>> = {};
for (const k of KEYS) kept[k] = E[k];
afterEach(() => { for (const k of KEYS) { if (kept[k] === undefined) delete E[k]; else E[k] = kept[k]; } });

const grand = (seed = 2, human: string | null = null, stop = 3): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  return g;
};
const at = (g: Game, name: string): Region => g.byName[name];
/** A German province at home, behind the front. */
const home = (g: Game): string => g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && !r.cap && !r.port && r.nb.every((j) => g.regions[j].ctrl === 'GER'))!.name;
const fresh = (g: Game): void => { g.supplyDirty = true; computeSupply(g); };
/** Poland's ground as the stand-in for China's: the long line on it at the stops without the book. */
const polandLong = (): void => { E.lineOwners = ['POL']; E.lineStops = [3, 4, 5]; };
/** Germany deep in Poland: all of it but the three eastern provinces, taken this month. */
function deepInPoland(g: Game): void {
  for (const r of g.regions) if (r.owner === 'POL' && !['Volhynia', 'Polesie', 'Wilno'].includes(r.name)) { r.ctrl = 'GER'; r.since = g.turn; }
  for (const c of g.corps.filter((c) => c.nat === 'POL' && g.regions[c.reg].ctrl !== 'POL')) removeCorps(g, c);
  fresh(g);
}

describe('221 H: a garrison marked to stay keeps its hold when an offensive releases its ground', () => {
  it('reads the mark from a setup row and keeps it through the release; an unmarked garrison beside it goes', () => {
    const g = grand();
    const base = home(g);
    const marked = addCorps(g, 'GER', 'Marked garrison', 'inf', 1, base, { hold: 1, stay: 1 });
    const plain = addCorps(g, 'GER', 'Plain garrison', 'inf', 1, base, { hold: 1 });
    expect(marked.stay).toBe(true);
    expect(plain.stay).toBeUndefined();
    release(g, 'GER', [base]);
    expect(marked.hold).toBe(true);
    expect(plain.hold).toBe(false);
  });

  it('leaves Europe as it was: no row of the game carries the mark', () => {
    const g = grand();
    const marked = g.corps.filter((c) => c.stay);
    // (With the far side's switch on the ruling's own rows are laid, and the mark is on Japan's garrisons alone.)
    expect(marked.filter((c) => c.nat !== 'JAP')).toEqual([]);
    if (!FAR_STAGE) expect(marked).toEqual([]);
    else expect(marked.length).toBe(stop3._stay.length);
  });
});

describe('221 L: the long line on the ground of the owners named, at the stops named', () => {
  it('is off on main\'s data: the long line is the Soviet Union\'s alone', () => {
    const g = grand();
    // (With the far side's switch on the ruling's data is laid: China's ground is the one owner more.)
    expect(lineOwners(g)).toEqual(FAR_STAGE ? [...EASTERN.deepOwners, ...stop3.east.lineOwners] : EASTERN.deepOwners);
    expect(lineOwner(g, 'POL')).toBe(false);
  });

  it('counts at the stops in lineStops and not at the others, and never takes the Soviet Union\'s away', () => {
    polandLong();
    for (const stop of [1, 2, 3, 4, 5]) {
      const g = grand(2, null, stop);
      expect(lineOwner(g, 'POL'), `stop ${stop}`).toBe(stop >= 3);
      for (const o of EASTERN.deepOwners) expect(lineOwner(g, o)).toBe(true);
    }
    E.lineStops = undefined;
    expect(lineOwner(grand(2, null, 1), 'POL')).toBe(true);
  });

  it('lengthens the invader\'s line deep in the country, counted from the frontier, and from a harbour it holds', () => {
    const off = grand();
    deepInPoland(off);
    expect(off.reach.GER[at(off, 'Lublin').id]).toBe(0);
    polandLong();
    const g = grand();
    deepInPoland(g);
    const reach = (name: string): number => g.reach.GER[at(g, name).id];
    // A step of fresh road for every province of Poland's between the corps and the frontier.
    expect(reach('Polish Silesia')).toBe(EASTERN.stepFresh);
    expect(reach('Lublin')).toBeGreaterThanOrEqual(2 * EASTERN.stepFresh);
    // The Corridor's harbour is supplied from the sea: its line starts there.
    expect(g.supply.GER[at(g, 'Danzig Corridor').id]).toBe(2);
    expect(reach('Danzig Corridor')).toBe(0);
    // An army that far out pays for it: drag on the march, and less strength in the attack and the rest.
    const c = addCorps(g, 'GER', 'Spearhead', 'inf', 2, 'Lublin', {});
    fresh(g);
    expect(lineDrag(g, c, at(g, 'Volhynia'))).toBeGreaterThan(EASTERN.roads);
    expect(lineStrength(g, c).mul).toBeLessThan(1);
    // Its railheads give way to the line, as on Soviet ground.
    expect(railheadAt(g, 'GER', at(g, 'Lublin').id)).toEqual({ road: 0, attack: 1, defence: 1 });
    // The defender is at home on his own railways.
    expect(g.reach.POL?.[at(g, 'Volhynia').id] ?? 0).toBe(0);
  });

  it('brings none of the Russian winter', () => {
    polandLong();
    const g = grand();
    deepInPoland(g);
    g.turn = 5; // January 1940
    expect(frozen(g, 'GER', at(g, 'Lublin'))).toBe(false);
  });

  it('is not read at a stop with the book', () => {
    polandLong();
    const g = grand(2, null, 1);
    deepInPoland(g);
    expect(g.reach.GER[at(g, 'Lublin').id]).toBe(0);
  });
});

describe('221 S: a staff reckons the long line as it reckons the winter', () => {
  const polishFront = (g: Game) => fronts(g, 'GER').find((f) => f.enemies.includes('POL'))!;

  it('counts its attack for less when its corps stand deep in the country, and as before at the frontier', () => {
    polandLong();
    const deep = grand();
    deepInPoland(deep);
    // (Set off by hand first: with the far side's switch on the ruling's data has it on already.)
    E.lineReckon = false;
    const plain = polishFront(deep).atk;
    E.lineReckon = true;
    const reckoned = polishFront(deep).atk;
    expect(reckoned).toBeLessThan(plain);
    expect(reckoned).toBeGreaterThan(0.3 * plain);
    // On the first day the line is a province long: the sums are what they were.
    const first = grand();
    fresh(first);
    E.lineReckon = false;
    const before = polishFront(first).atk;
    E.lineReckon = true;
    expect(polishFront(first).atk).toBeCloseTo(before, 9);
  });

  it('changes nothing where the line is not counted', () => {
    E.lineReckon = true;
    const g = grand();
    deepInPoland(g);
    const on = polishFront(g).atk;
    E.lineReckon = false;
    expect(polishFront(g).atk).toBe(on);
  });
});

describe('221 F: troops lifted off an enemy\'s coast land at the nearest harbour their country holds there', () => {
  // Norway at war with Germany, with Oslo and Bergen in German hands and a German corps lifted off Trondheim.
  function norway(human: string | null = null): { g: Game; c: Corps; from: Region } {
    const g = grand(2, human);
    declareWar(g, 'GER', 'NOR');
    for (const nm of ['Oslo', 'Bergen', 'Trondheim']) { at(g, nm).ctrl = 'GER'; at(g, nm).since = g.turn; }
    for (const c of g.corps.filter((c) => c.nat === 'NOR' && g.regions[c.reg].ctrl === 'GER')) removeCorps(g, c);
    const c = addCorps(g, 'GER', 'Gebirgskorps', 'mtn', 2, 'Trondheim', {});
    return { g, c, from: at(g, 'Trondheim') };
  }

  it('is off on main\'s data: they are taken home', () => {
    const { g, c, from } = norway();
    // (With the far side's switch on the ruling's data has it on, in the grand campaign: the nearest harbour held.)
    expect(refuge(g, c, from).name).toBe(FAR_STAGE ? 'Oslo' : g.regions[g.nations.GER.capital].name);
  });

  it('lands them at the nearest of their own harbours on that ground, then the next when it is full or taken', () => {
    E.nearRefuge = true;
    const { g, c, from } = norway();
    expect(refuge(g, c, from).name).toBe('Oslo');
    for (let k = 0; k < STACK; k++) addCorps(g, 'GER', `Filler ${k}`, 'inf', 1, 'Oslo', {});
    expect(refuge(g, c, from).name).toBe('Bergen');
    at(g, 'Bergen').ctrl = 'NOR';
    expect(refuge(g, c, from).name).toBe(g.regions[g.nations.GER.capital].name);
  });

  it('works through the evacuation itself, and for the player\'s army too', () => {
    E.nearRefuge = true;
    const { g, c } = norway('GER');
    g.nations.GER.seaSupply = true;
    expect(evacuate(g, c)).toBe(true);
    expect(g.regions[c.reg].name).toBe('Oslo');
  });

  it('takes home troops lifted off a friend\'s coast, and lands nobody in an ally\'s harbour', () => {
    E.nearRefuge = true;
    const { g, c, from } = norway();
    at(g, 'Oslo').ctrl = 'ITA';
    at(g, 'Bergen').ctrl = 'ITA';
    expect(refuge(g, c, from).name).toBe(g.regions[g.nations.GER.capital].name);
    // Lifted off an ally's ground the army goes home as before (a theatre's own havens come first, as they always did).
    const u = addCorps(g, 'GER', 'Guest corps', 'inf', 1, home(g), {});
    const friend = g.regions.find((r) => r.owner === 'ITA' && r.port && r.theatre < 0)!;
    expect(refuge(g, u, friend).name).toBe(g.regions[g.nations.GER.capital].name);
  });
});

describe('221: the data for step 13 (data/campaign/china-stop3.json)', () => {
  it('turns on the four switches the code reads, with the long line at the stops without the book', () => {
    expect(Object.keys(stop3.east).sort()).toEqual([...KEYS].sort());
    expect(stop3.east).toEqual({ lineOwners: ['CHN'], lineStops: [3, 4, 5], lineReckon: true, nearRefuge: true });
  });

  it('names China among the staffs that will not be ringed and that rest worn corps (R)', () => {
    expect(stop3.rivals).toEqual({ ring: { staffs: ['CHN'] }, relief: { staffs: ['CHN'] } });
  });

  it('lists the eight Japanese garrison rows of paper 17 to be marked to stay (H), one to a base', () => {
    expect(stop3._stay).toHaveLength(8);
    expect(stop3._stay.every(([nat]) => nat === 'JAP')).toBe(true);
    expect(new Set(stop3._stay.map((r) => r[2])).size).toBe(8);
    expect(stop3._stay.map((r) => r[2]).sort()).toEqual(['Amoy', 'Hainan', 'Nanking', 'Peiping', 'Shanghai', 'Swatow', 'Tatung', 'Tsingtao']);
  });
});
