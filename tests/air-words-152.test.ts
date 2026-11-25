// Release 1.5.2: the playtester's air findings 4, 5, 6, 8, 9, 12, 13 and 30 of the 1.5.1 playtest, told in the UI
// director's words (ruled 25 November 2026, 05:10Z, and again at 05:45Z). Words and lists only: no rule reads any of them.
//   4   a bomber group on a standing campaign says why it has no raid of its own, and which button frees it;
//   12  a raid's share that rounds to nothing is "less than 1%", and one where the bombs fell and did nothing says so;
//   9, 34  a carrier wing out of reach has no distance on the raid card: "Out of reach", and why;
//   6   a dated row that lowers a command's establishment is a line, with where the aircraft truly go; several in a
//       month are one line on the month card, summed from their own lines;
//   13  a nation with air groups and nothing in reach is told where a field would put the enemy in reach;
//   5   shipping a group across the ocean lists every field, country by country, the first of each naming its country;
//   8, 30  a fight for the sky that met no enemy fighters says so, where it concerns the player.
import { describe, expect, it } from 'vitest';
import { AIR, airPool, airRange, airUpkeep, fly, hasAirfield, km, loweredOf, loweredSum } from '../src/campaign/air';
import { airPage } from '../src/campaign/airpages';
import { campaignWhy, groupJobs, launchRaid, setGroupJob } from '../src/campaign/airwar';
// (a namespace, so that on a build without it only the test that uses it fails)
import * as engineers from '../src/campaign/engineers';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { fitFoot, monthCard, monthMark } from '../src/campaign/monthcard';
import { loadGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { airGroupBases, raidTargetsFor } from '../src/campaign/turn';
import type { AirFleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const copy = (): Game => loadGame(TC.save().text)!;
const regionOf = (g: Game, name: string): number => g.regions.find((r) => r.name === name)!.id;

/** America at war with Germany at the start of the open campaign, one of her groups of `kind` at London with `n` aircraft, and Calais in German hands. */
function overTheChannel(kind: 'ftr' | 'bmr', n: number): { g: Game; f: AirFleet } {
  const g = startOpen(createGame(1, 'USA'));
  declareWar(g, 'USA', 'GER');
  const f = g.airFleets.find((x) => x.nat === 'USA' && x.kind === kind && x.ship === undefined)!;
  f.base = regionOf(g, 'London');
  if (kind === 'ftr') f.ftr = n; else f.bmr = n;
  g.regions[regionOf(g, 'Calais')].ctrl = 'GER';
  return { g, f };
}
/** A German corps moved to Calais, to be raided there. */
function corpsAtCalais(g: Game): void {
  const at = g.regions[regionOf(g, 'Calais')];
  const c = g.corps.find((x) => x.nat === 'GER' && !x.dead)!;
  const was = g.regions[c.reg];
  was.units = was.units.filter((u) => u !== c);
  c.reg = at.id;
  at.units.push(c);
}

describe('finding 4: a bomber group on a standing campaign says why it has no raid of its own (GER from 17, seed 7)', () => {
  it("1. Fliegerdivision's raid rows name the night campaign over London and the order that frees it", () => {
    TC.start({ view: 'grand', human: 'GER', from: 17, seed: 7, world: true });
    const g = copy();
    const f = g.airFleets.find((x) => x.id === 31)!;
    expect([f.name, f.mission, g.regions[f.target].name]).toEqual(['1. Fliegerdivision', 'night', 'London']);
    const want = "It flies with the night campaign over London. To give it a raid of its own, first press GIVE THIS ORDER on 'Support the armies', which takes it out of the campaign.";
    const p = TC.page('group:31');
    const orders = p.sections.find((s: { heading: string }) => s.heading === 'STANDING ORDERS').rows;
    // (the button named is the one the page shows on that row, and it can be pressed)
    const free = orders.find((r: { title: string }) => r.title === 'Support the armies');
    expect(free.buttons[0]).toMatchObject({ label: 'GIVE THIS ORDER', act: 'job:31:patrol', ok: true });
    const raids = p.sections.find((s: { heading: string }) => s.heading === 'RAIDS').rows;
    const night = raids.find((r: { title: string }) => r.title === 'Bomb by night');
    expect(night.lines[1]).toBe(want);
    expect(night.buttons[0].tip).toBe(want);
    expect(raids.filter((r: { lines: string[] }) => r.lines[1] === 'Nothing in its reach.')).toEqual([]);
    // (its own targets page says the same)
    const own = TC.page('raid:bomb:31');
    expect(own.headline).toBe('No target is in reach');
    expect(own.sections[0].text).toBe(want);
    // Given that order, it has a raid of its own, and the line is gone.
    expect(TC.act('job:31:patrol').ok).toBe(true);
    expect(raidTargetsFor(copy(), 'bomb', 31).length).toBeGreaterThan(0);
    expect(JSON.stringify(TC.page('group:31'))).not.toContain('campaign over');
  });

  it('a day campaign is worded the same way', () => {
    TC.start({ view: 'grand', human: 'GER', from: 17, seed: 7, world: true });
    const g = copy();
    const f = g.airFleets.find((x) => x.id === 31)!;
    f.mission = 'bomb';
    const raids = airPage(g, 'group:31')!.sections.find((s) => s.heading === 'RAIDS')!.rows;
    expect(raids.find((r) => r.title === 'Bomb factories and railways')!.lines[1])
      .toBe("It flies with the day campaign over London. To give it a raid of its own, first press GIVE THIS ORDER on 'Support the armies', which takes it out of the campaign.");
    // (and a group with no campaign keeps the old words)
    expect(setGroupJob(g, 31, 'patrol')).toBe(true);
    expect(JSON.stringify(airPage(g, 'group:31'))).not.toContain('campaign over');
  });

  it("a carrier wing names its own row, 'Stand ready for a strike' (UK from 17, seed 6)", () => {
    TC.start({ view: 'grand', human: 'UK', from: 17, seed: 6, world: true });
    const g = copy();
    // (a wing aboard set on the night campaign over Berlin by hand: only the words are under test)
    const w = g.airFleets.find((x) => x.nat === 'UK' && x.ship !== undefined && x.bmr > 0)!;
    w.mission = 'night';
    w.target = regionOf(g, 'Berlin');
    // (the row it names is the one its page shows, with that button)
    const row = groupJobs(g, w.id).find((j) => j.key === 'patrol')!;
    expect(row.label).toBe('Stand ready for a strike');
    expect(campaignWhy(g, w.id)).toBe("It flies with the night campaign over Berlin. To give it a raid of its own, first press GIVE THIS ORDER on 'Stand ready for a strike', which takes it out of the campaign.");
  });
});

describe('finding 12: a share that rounds to nothing is never "0%"', () => {
  it('a raid on troops that cuts their organisation by less than half a point says "less than 1%"', () => {
    const { g, f } = overTheChannel('bmr', 3);
    corpsAtCalais(g);
    for (const x of g.airFleets) if (x.nat === 'GER') x.ftr = 0;
    const rep = launchRaid(g, 'ground', regionOf(g, 'Calais'), [f.id], 'USA')!;
    expect(rep.result).toMatch(/^The raids cut the organisation of .+ at Calais by less than 1%(, less where they are dug in)?\. They fight the weaker for it until they have rested\.$/);
    expect(rep.result).not.toContain('0%');
  });

  it('a raid on troops that falls on them and cuts nothing says it did not cut their organisation', () => {
    const { g, f } = overTheChannel('bmr', 60);
    corpsAtCalais(g);
    for (const x of g.airFleets) if (x.nat === 'GER') x.ftr = 0;
    const at = g.regions[regionOf(g, 'Calais')];
    for (const c of at.units) if (c.nat === 'GER') c.divs = 0;
    const rep = launchRaid(g, 'ground', at.id, [f.id], 'USA')!;
    const c = at.units.find((u) => u.nat === 'GER')!;
    expect(rep.result).toBe(`The raids did not cut the organisation of ${c.name} at Calais.`);
  });

  it('a raid on factories that stops less than half a point of their output says "less than 1%"', () => {
    const { g, f } = overTheChannel('bmr', 3);
    for (const x of g.airFleets) if (x.nat === 'GER') x.ftr = 0;
    const rep = launchRaid(g, 'bomb', regionOf(g, 'Ruhr'), [f.id], 'USA')!;
    expect(rep.result).toMatch(/^The bombing stopped less than 1% of Ruhr's output for a month: less than 0\.1% of German production\./);
  });

  it("a raid on factories whose bombs fell and stopped nothing says it did not stop any of the output", () => {
    const { g, f } = overTheChannel('bmr', 60);
    for (const x of g.airFleets) if (x.nat === 'GER') x.ftr = 0;
    // (guns so heavy that what gets through stops nothing: only the words are under test)
    g.flags[`wk:aa:${regionOf(g, 'Ruhr')}`] = 7;
    const rep = launchRaid(g, 'bomb', regionOf(g, 'Ruhr'), [f.id], 'USA')!;
    expect(rep.att[0].bmr - rep.att[0].lostBmr).toBeGreaterThan(0);
    expect(rep.result).toMatch(/^The bombing did not stop any of Ruhr's output\./);
  });
});

describe('findings 9 and 34: a carrier wing out of reach has no distance on the raid card (UK from 17, seed 6)', () => {
  it("plan:strike:6:8.39: the Mediterranean Fleet's and Force H's wings are 'Out of reach', with why, and no km", () => {
    TC.start({ view: 'grand', human: 'UK', from: 17, seed: 6, world: true });
    const p = TC.page('plan:strike:6:8.39');
    const far = p.card.seats.filter((s: { base: string }) => s.base === 'Mediterranean Fleet' || s.base === 'Force H');
    expect(far.length).toBe(4);
    for (const s of far) {
      expect(s.km).toBeUndefined();
      expect(s.far).toBe('Out of reach');
      expect(s.why).toBe("Its aircraft cannot reach it from the fleet's station.");
    }
    // (a group in reach keeps its distance, in figures and in words)
    const near = p.card.seats.find((s: { name: string }) => s.name === 'No. 1 Group');
    expect([near.km, near.far]).toEqual([145, '145 km']);
    // Nothing on the card is a number that JSON cannot carry.
    expect(JSON.stringify(p.card)).not.toMatch(/"km":null/);
  });
});

describe("finding 6: a dated row that lowers a command's establishment is a line, with where the aircraft truly go", () => {
  /** America's First Air Force at its 1942 strength in December 1942, her pool at `pool` fighters, and the month of January 1943 opened. */
  function january1943(pool: number, bombers = 0): { g: Game; card: ReturnType<typeof monthCard>; whole: ReturnType<typeof monthCard> } {
    const g = startOpen(createGame(1, 'USA'));
    const faf = g.airFleets.find((x) => x.nat === 'USA' && x.name === 'First Air Force')!;
    g.turn = 40;
    airUpkeep(g, 'USA');
    faf.ftr = 240;
    // (with room in the pool the other groups stand empty, and draw on it; with the pool full they are at their
    // establishment of 1942, and their own rows lower it too)
    for (const x of g.airFleets) if (x.nat === 'USA' && x !== faf) { x.ftr = pool ? x.est.ftr : 0; x.bmr = pool ? x.est.bmr : 0; }
    const p = airPool(g, 'USA');
    p.ftr = pool; p.bmr = bombers; p.pilots = 0;
    const mark = monthMark(g);
    g.turn = 41;
    airUpkeep(g, 'USA');
    return { g, card: monthCard(g, mark), whole: monthCard(g, mark, g.human, { whole: true }) };
  }
  /** First Air Force alone of America's groups, at 240 fighters and `bmr` bombers in December 1942, her pool at `ftr` and `pbmr`; January 1943 opened. */
  function alone(ftr: number, bmr = 0, pbmr = 0): Game {
    const g = startOpen(createGame(1, 'USA'));
    const faf = g.airFleets.find((x) => x.nat === 'USA' && x.name === 'First Air Force')!;
    g.airFleets = g.airFleets.filter((x) => x.nat !== 'USA' || x === faf);
    g.turn = 40;
    airUpkeep(g, 'USA');
    faf.ftr = 240; faf.bmr = bmr;
    const p = airPool(g, 'USA');
    p.ftr = ftr; p.bmr = pbmr; p.pilots = 0;
    g.turn = 41;
    airUpkeep(g, 'USA');
    return g;
  }
  const lines = (g: Game): string[] => g.headlines.filter((h) => h.turn === 41 && h.nat === 'USA' && /establishment/.test(h.text)).map((h) => h.text);
  /** What the pool would hold at the end of January's opening with no ceiling: the same war opened again with the ceiling lifted. */
  function unbounded(make: () => Game): { ftr: number; bmr: number } {
    const was = AIR.reserve.USA;
    AIR.reserve.USA = { ftr: 1e9, bmr: 1e9, pilots: 1e9 };
    try { const p = airPool(make(), 'USA'); return { ftr: p.ftr, bmr: p.bmr }; } finally { AIR.reserve.USA = was; }
  }

  it('with room in the pool: "80 fighters go back to the pool, from which the other commands draw"', () => {
    const { g, card } = january1943(0);
    const want = "The air staff has lowered First Air Force's establishment to 160: 80 fighters go back to the pool, from which the other commands draw.";
    expect(lines(g)).toEqual([want]);
    // (one command lowered: the month card gives its own line)
    expect(card.also).toContain(want);
  });

  it('with the pool full: "the pool, from which the other commands draw, is full, so 80 fighters are struck off"', () => {
    const g = alone(AIR.reserve.USA.ftr);
    expect(airPool(g, 'USA').ftr).toBe(AIR.reserve.USA.ftr);
    expect(lines(g)).toEqual(["The air staff has lowered First Air Force's establishment to 160: the pool, from which the other commands draw, is full, so 80 fighters are struck off."]);
  });

  it('with room for part: "50 fighters go back to the pool ..., and the 30 it has no room for are struck off", and 30 is what the ceiling cut', () => {
    const most = AIR.reserve.USA.ftr;
    // (the pool as January's opening leaves it with nothing in it before: the month's new fighters and First Air Force's 80)
    const after = airPool(alone(0), 'USA').ftr;
    expect(after).toBeLessThan(most);
    const start = most + 30 - after;
    const g = alone(start);
    expect(unbounded(() => alone(start)).ftr - most).toBe(30);
    expect(lines(g)).toEqual(["The air staff has lowered First Air Force's establishment to 160: 50 fighters go back to the pool, from which the other commands draw, and the 30 it has no room for are struck off."]);
  });

  it('both kinds of aircraft: "40 fighters and 70 bombers"', () => {
    // (First Air Force kept at 160 fighters and no bombers, with 200 fighters and 70 bombers: 40 and 70 over)
    const make = (pf: number, pb: number): Game => {
      const g = startOpen(createGame(1, 'USA'));
      const faf = g.airFleets.find((x) => x.nat === 'USA' && x.name === 'First Air Force')!;
      g.airFleets = g.airFleets.filter((x) => x.nat !== 'USA' || x === faf);
      g.turn = 40;
      airUpkeep(g, 'USA');
      faf.ftr = 200; faf.bmr = 70;
      const p = airPool(g, 'USA');
      p.ftr = pf; p.bmr = pb; p.pilots = 0;
      g.turn = 41;
      airUpkeep(g, 'USA');
      return g;
    };
    expect(lines(make(0, 0))).toEqual(["The air staff has lowered First Air Force's establishment to 160: 40 fighters and 70 bombers go back to the pool, from which the other commands draw."]);
    const most = AIR.reserve.USA;
    expect(lines(make(most.ftr, most.bmr))).toEqual(["The air staff has lowered First Air Force's establishment to 160: the pool, from which the other commands draw, is full, so 40 fighters and 70 bombers are struck off."]);
  });

  it('several commands in a month: one summed line on the month card, every command keeping its own line in the news', () => {
    const most = AIR.reserve.USA.ftr;
    const { g, card, whole } = january1943(most, AIR.reserve.USA.bmr);
    const own = lines(g);
    expect(own.length).toBeGreaterThan(1);
    // The figures, read from the commands' own lines as the news gives them.
    const read = (t: string, kind: string): number => Number(new RegExp(`([\\d,]+) ${kind}s?`).exec(t)?.[1].replace(/,/g, '') ?? 0);
    const struck = (t: string): string => /struck off/.test(t) ? t.slice(t.search(/(is full, so|and the) /)) : '';
    const off = { ftr: own.reduce((n, t) => n + read(struck(t), 'fighter'), 0), bmr: own.reduce((n, t) => n + read(struck(t), 'bomber'), 0) };
    // What the pool's ceiling truly cut, of the aircraft the commands gave up: all of them here, the pool being full.
    const gave = { ftr: own.reduce((n, t) => n + read(t, 'fighter'), 0), bmr: own.reduce((n, t) => n + read(t, 'bomber'), 0) };
    const free = unbounded(() => january1943(most, AIR.reserve.USA.bmr).g);
    expect(off).toEqual({ ftr: Math.min(gave.ftr, free.ftr - AIR.reserve.USA.ftr), bmr: Math.min(gave.bmr, free.bmr - AIR.reserve.USA.bmr) });
    expect(off).toEqual(gave);
    const n = own.length;
    const want = `The establishments of ${n} commands were lowered: ${gave.ftr} fighters and ${gave.bmr} bombers are struck off, because the pool is full.`;
    // The card that is read whole (the 3D game's) gives the summed line and none of the commands' own.
    expect(whole.also).toContain(want);
    for (const t of own) expect(whole.also).not.toContain(t);
    // The card whose foot lines have to fit gives it, or its short form where it will not fit.
    const short = `${n} commands cut back: ${gave.ftr + gave.bmr} aircraft struck off`;
    expect(card.also.filter((x) => x === want || x === short).length).toBe(1);
    for (const t of own) expect(card.also).not.toContain(t);
    expect(fitFoot([want], 35, 2, 3, [short])).toEqual([{ text: short, at: 0 }]);
    // The news keeps every command's own line.
    expect(g.headlines.filter((h) => h.turn === 41 && loweredOf(h.text)).length).toBe(n);
  });

  it('the summed line in its three cases, and its short form', () => {
    const line = (name: string, back: string, off: string): string => `The air staff has lowered ${name}'s establishment to 100: ${
      !off ? `${back} go back to the pool, from which the other commands draw.` : !back ? `the pool, from which the other commands draw, is full, so ${off} are struck off.` : `${back} go back to the pool, from which the other commands draw, and the ${off} it has no room for are struck off.`}`;
    const five = (xs: [string, string][]): string[] => xs.map(([b, o], i) => line(`Command ${i}`, b, o));
    expect(loweredSum(five([['', '80 fighters'], ['', '50 bombers'], ['', '40 fighters and 70 bombers'], ['', '100 fighters and 40 bombers'], ['', '50 fighters and 20 bombers']])))
      .toEqual({ text: 'The establishments of 5 commands were lowered: 270 fighters and 180 bombers are struck off, because the pool is full.', short: '5 commands cut back: 450 aircraft struck off' });
    expect(loweredSum(five([['80 fighters', ''], ['50 bombers', ''], ['40 fighters and 70 bombers', ''], ['100 fighters and 40 bombers', ''], ['50 fighters and 20 bombers', '']])))
      .toEqual({ text: 'The establishments of 5 commands were lowered: 270 fighters and 180 bombers go back to the pool.', short: '5 commands cut back: 450 aircraft back to the pool' });
    // (some went back and some were struck off: the short form gives those struck off, the director having ruled no third)
    expect(loweredSum(five([['50 fighters', '30'], ['', '50 bombers'], ['40 fighters', '70 bombers'], ['100 fighters', '40 bombers'], ['10 fighters', '40 fighters and 20 bombers']])))
      .toEqual({ text: 'The establishments of 5 commands were lowered: 200 fighters go back to the pool, and the 70 fighters and 180 bombers it has no room for are struck off.', short: '5 commands cut back: 250 aircraft struck off' });
    // One command is no sum: the card gives its own line.
    expect(loweredSum(five([['80 fighters', '']]))).toBeNull();
  });

  it('a war nobody plays has no such line', () => {
    const g = startOpen(createGame(1, null));
    g.turn = 40;
    airUpkeep(g, 'USA');
    g.airFleets.find((x) => x.nat === 'USA' && x.name === 'First Air Force')!.ftr = 240;
    const h0 = g.headlines.length;
    g.turn = 41;
    airUpkeep(g, 'USA');
    expect(g.headlines.slice(h0).filter((h) => /establishment/.test(h.text))).toEqual([]);
  });
});

describe("finding 13: a nation with air groups and nothing in reach is told where a field would bring the enemy in reach (MEX from 1, seed 9)", () => {
  it("Mexico at war with Guatemala: the raids pages name the province, by the rules' own reach and field rule, and open fast", () => {
    TC.start({ view: 'grand', human: 'MEX', from: 1, seed: 9, depth: 'supreme', world: true });
    TC.declare('GUA');
    TC.endMonth();
    const g = copy();
    const mine = g.airFleets.filter((f) => f.nat === 'MEX' && f.ftr + f.bmr > 0);
    expect(mine.length).toBeGreaterThan(0);
    const said = /^No target is in reach\. Your aircraft could reach the enemy from (.+) if it had an airfield: an engineer corps can lay one there\.$/.exec(TC.page('raid:ground').headline);
    expect(said?.[1]).toBeTruthy();
    // The province: his, no field, one an engineer corps could lay out, nearest the enemy, and with the enemy in the
    // longest reach of his aircraft from it.
    const reach = Math.max(...mine.map((f) => Math.max(f.bmr > 0 ? airRange(f, g).bmr : 0, f.ftr > 0 ? airRange(f, g).ftr : 0)));
    const foes = g.regions.filter((r) => atWar(g, 'MEX', r.ctrl));
    const near = (id: number): number => Math.min(...foes.map((e) => km(g, id, e.id)));
    const can = g.regions.filter((r) => r.ctrl === 'MEX' && !hasAirfield(g, r.id) && engineers.fieldCanBeLaid(g, 'MEX', r)).sort((a, b) => near(a.id) - near(b.id));
    const at = can[0];
    expect(at.name).toBe(said![1]);
    expect(near(at.id)).toBeLessThanOrEqual(reach);
    const want = `No target is in reach. Your aircraft could reach the enemy from ${at.name} if it had an airfield: an engineer corps can lay one there.`;
    for (const kind of ['ground', 'superiority', 'bomb']) {
      const t0 = performance.now();
      const p = TC.page(`raid:${kind}`);
      const ms = performance.now() - t0;
      expect(p.headline, kind).toBe(want);
      expect(ms, `raid:${kind} took ${Math.round(ms)} ms`).toBeLessThan(1500);
    }
    // With no such province, only "No target is in reach".
    for (const r of can) g.flags[`wk:field:${r.id}`] = 99;
    expect(airPage(g, 'raid:ground')!.headline).toBe('No target is in reach');
  });
});

describe('finding 5: shipping a group across the ocean lists every field, country by country (USA from 29, seed 11)', () => {
  it('from the Canal Zone the British fields are offered, East Anglia, Kent and London among them, the shippable first in each country', () => {
    TC.start({ view: 'grand', human: 'USA', from: 29, seed: 11, world: true });
    const g = copy();
    const f = g.airFleets.find((x) => x.nat === 'USA' && x.name === '1st Bombardment Group')!;
    expect(g.regions[f.base].name).toBe('Canal Zone');
    const all = airGroupBases(g, f.id).filter((b) => b.sea);
    const p = TC.page(`rebase:${f.id}`);
    const sea = p.sections.find((s: { heading: string }) => s.heading === 'ACROSS THE OCEAN, BY SEA') as { rows: { title: string; buttons: { ok: boolean }[] }[] };
    expect(sea.rows.length).toBe(all.length);
    expect(sea.rows.length).toBeGreaterThan(8);
    for (const n of ['East Anglia', 'Kent', 'London']) expect(sea.rows.map((r) => r.title)).toContain(n);
    // Country by country (each country's fields together), and in each the fields it can be shipped to now first.
    const key = (t: string): string => { const r = g.regions.find((x) => x.name === t)!; return `${r.owner}/${r.ctrl}`; };
    const order = sea.rows.map((r) => key(r.title));
    const seen: string[] = [];
    for (const k of order) if (seen[seen.length - 1] !== k) { expect(seen, `${k} comes back after another country`).not.toContain(k); seen.push(k); }
    for (const k of seen) {
      const ok = sea.rows.filter((r) => key(r.title) === k).map((r) => r.buttons[0].ok);
      expect(ok, k).toEqual([...ok].sort((x, y) => Number(y) - Number(x)));
    }
    // No heading for each country (still the page's two sections), but the first field of each country's run names
    // the country on its second line, and the fields after it in the run do not (the UI director, 05:45Z).
    expect(p.sections.map((s: { heading: string }) => s.heading)).toEqual(['AIRFIELDS', 'ACROSS THE OCEAN, BY SEA']);
    const rows = sea.rows as unknown as { title: string; lines: string[] }[];
    const at = (t: string): number => rows.findIndex((r) => r.title === t);
    const uk = rows.findIndex((r) => key(r.title) === 'UK/UK');
    expect(rows[uk].lines).toEqual([expect.stringMatching(/^(Landing ground|Airfield|Air base), [\d,]+ km away across the ocean: serves [\d,]+ aircraft, room for [\d,]+ more\./), 'United Kingdom']);
    expect(rows[uk + 1].lines.length).toBe(1);
    for (const t of ['East Anglia', 'Kent', 'London']) expect(rows[at(t)].lines.length).toBe(1);
    const firsts = rows.filter((r, i) => i === 0 || key(r.title) !== key(rows[i - 1].title));
    expect(firsts.map((r) => r.lines[1])).toEqual(seen.map((k) => { const [o, c] = k.split('/'); return o === c ? g.nations[o].name : expect.stringMatching(new RegExp(`^${g.nations[o].name}, held by `)); }));
    expect(rows.filter((r) => r.lines.length > 1).length).toBe(seen.length);
  });
});

describe('findings 8 and 30: a fight for the sky that met no enemy fighters says so, where it concerns the player', () => {
  /** America's land fighter groups at London, `n` fighters each, and Germany's fighters grounded. */
  function sweep(groups: number, n: number): { g: Game; fs: AirFleet[] } {
    const { g } = overTheChannel('ftr', n);
    const fs = g.airFleets.filter((x) => x.nat === 'USA' && x.kind === 'ftr' && x.ship === undefined).slice(0, groups);
    for (const f of fs) { f.base = regionOf(g, 'London'); f.ftr = n; f.bmr = 0; }
    for (const x of g.airFleets) if (x.nat === 'GER') x.ftr = 0;
    return { g, fs };
  }

  it('one group: "First Air Force flew over Calais and met no enemy fighters."', () => {
    const { g, fs } = sweep(1, 80);
    const n0 = g.headlines.length;
    // (flown as his staff flies it: his own order, by an arm too small to hold the sky, has its own line, below)
    const rep = fly(g, 'USA', 'superiority', regionOf(g, 'Calais'), fs, ':staff0')!;
    expect(rep.def).toEqual([]);
    expect(rep.result).toBe(`${fs[0].name} flew over Calais and met no enemy fighters.`);
    // No headline is added for it.
    expect(g.headlines.slice(n0).filter((h) => h.text.includes('met no enemy fighters'))).toEqual([]);
  });

  it('several groups: "First Air Force and 1st Pursuit Squadron flew over Calais and met no enemy fighters."', () => {
    const { g, fs } = sweep(2, 80);
    expect(fs.length).toBe(2);
    const rep = fly(g, 'USA', 'superiority', regionOf(g, 'Calais'), fs, ':staff0')!;
    expect(rep.result).toBe(`${fs[0].name} and ${fs[1].name} flew over Calais and met no enemy fighters.`);
  });

  it('his own sweep by an arm too small to hold the sky keeps the small-arm line in its place', () => {
    const { g, fs } = sweep(1, 20);
    const rep = launchRaid(g, 'superiority', regionOf(g, 'Calais'), [fs[0].id], 'USA')!;
    expect(rep.result).toMatch(new RegExp(`^${fs[0].name} flies over Calais, but no enemy fighters came up to meet it`));
    expect(rep.result).not.toContain('met no enemy fighters');
  });

  it("a sweep over his ground says so too; one between others keeps no line, and stays out of his month's notes", () => {
    const g = startOpen(createGame(1, 'USA'));
    declareWar(g, 'GER', 'UK');
    declareWar(g, 'USA', 'GER');
    const ger = g.airFleets.find((x) => x.nat === 'GER' && x.kind === 'ftr' && x.ship === undefined)!;
    ger.base = regionOf(g, 'Calais'); g.regions[regionOf(g, 'Calais')].ctrl = 'GER'; ger.ftr = 80; ger.bmr = 0;
    for (const x of g.airFleets) if (x.nat === 'UK' || x.nat === 'USA') x.ftr = 0;
    // Over Kent, Britain's: between others.
    const others = fly(g, 'GER', 'superiority', regionOf(g, 'Kent'), [ger], ':staff0')!;
    expect(others.def).toEqual([]);
    expect(others.result).toBe('');
    // Over London made his: against him.
    g.regions[regionOf(g, 'London')].ctrl = 'USA';
    ger.flew = undefined;
    const his = fly(g, 'GER', 'superiority', regionOf(g, 'London'), [ger], ':staff1')!;
    expect(his.result).toBe(`${ger.name} flew over London and met no enemy fighters.`);
  });
});
