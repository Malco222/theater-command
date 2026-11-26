// Release 1.5.2: the playtester's air findings 4, 5, 6, 8, 9, 12, 13 and 30 of the 1.5.1 playtest, told in the UI
// director's words (ruled 25 November 2026, 05:10Z, and again at 05:45Z). Words and lists only: no rule reads any of them.
//   4   a bomber group on a standing campaign says why it has no raid of its own, and which button frees it;
//   12  a raid's share that rounds to nothing is "less than 1%", and one where the bombs fell and did nothing says so;
//   9, 34  a carrier wing out of reach has no distance on the raid card: "Out of reach", and why;
//   6   a dated row that lowers a command's establishment is a line, with where the aircraft truly go; several in a
//       month are one line on the month card, summed from their own lines;
//   13  a nation whose aircraft reach no enemy for any kind of raid is told where a field would put the enemy in reach;
//   5   shipping a group across the ocean lists every field, country by country, the first of each naming its country;
//   8, 30  a fight for the sky that met no enemy fighters says so, where it concerns the player.
import { describe, expect, it } from 'vitest';
import { AIR, airPool, airRange, airUpkeep, brokeOff, fly, harbourOf, hasAirfield, km, loweredLine, loweredOf, loweredShort, loweredSum, skyWords } from '../src/campaign/air';
import { airPage } from '../src/campaign/airpages';
import { ARSENAL, offers } from '../src/campaign/arsenal';
import { campaignWhy, groupJobs, groupRow, launchRaid, planRaid, setGroupJob } from '../src/campaign/airwar';
// (a namespace, so that on a build without it only the test that uses it fails)
import * as engineers from '../src/campaign/engineers';
import { addCorps, atWar, createGame, declareWar } from '../src/campaign/game';
import { sail, whyNotShip } from '../src/campaign/navy';
import { wreckPrize } from '../src/campaign/prizes';
import { syncWings, wingsOf } from '../src/campaign/carriers';
import { airTicket, harbourOf as ticketHarbourOf } from '../src/battle/air/ticket';
import { fitFoot, monthCard, monthMark } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { airGroupBases, raidTargetsFor } from '../src/campaign/turn';
import type { AirFleet, AirReport, Game } from '../src/campaign/types';
// (the browser client's source, read as text: its raid report card cannot be drawn here)
import page from '../src/campaign/ui/main.ts?raw';
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
    expect(rep.result).toMatch(/^The bombing stopped less than 1% of the Ruhr's output for a month: less than 0\.1% of German production\./);
  });

  it("a raid on factories whose bombs fell and stopped nothing says it did not stop any of the output", () => {
    const { g, f } = overTheChannel('bmr', 60);
    for (const x of g.airFleets) if (x.nat === 'GER') x.ftr = 0;
    // (guns so heavy that what gets through stops nothing: only the words are under test)
    g.flags[`wk:aa:${regionOf(g, 'Ruhr')}`] = 7;
    const rep = launchRaid(g, 'bomb', regionOf(g, 'Ruhr'), [f.id], 'USA')!;
    expect(rep.att[0].bmr - rep.att[0].lostBmr).toBeGreaterThan(0);
    expect(rep.result).toMatch(/^The bombing did not stop any of the Ruhr's output\./);
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
    // (No. 11 Group's first group, at Kent)
    const near = p.card.seats.find((s: { id: number }) => s.id === 5);
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
    // (Third Air Force, its row lowering both kinds, from 100 fighters and 150 bombers to 60 and 80: its fighter group
    // and its bomber group at their 1942 strength are 40 and 70 over. Each kind is told where its own row lowers it.)
    const make = (pf: number, pb: number): Game => {
      const g = startOpen(createGame(1, 'USA'));
      const taf = g.airFleets.filter((x) => x.nat === 'USA' && (x.parent ?? x.name) === 'Third Air Force');
      expect(taf.map((x) => x.kind).sort()).toEqual(['bmr', 'ftr']);
      g.airFleets = g.airFleets.filter((x) => x.nat !== 'USA' || taf.includes(x));
      g.turn = 40;
      airUpkeep(g, 'USA');
      for (const x of taf) { x.ftr = x.est.ftr; x.bmr = x.est.bmr; }
      const p = airPool(g, 'USA');
      p.ftr = pf; p.bmr = pb; p.pilots = 0;
      g.turn = 41;
      airUpkeep(g, 'USA');
      return g;
    };
    expect(lines(make(0, 0))).toEqual(["The air staff has lowered Third Air Force's establishment to 140: 40 fighters and 70 bombers go back to the pool, from which the other commands draw."]);
    const most = AIR.reserve.USA;
    expect(lines(make(most.ftr, most.bmr))).toEqual(["The air staff has lowered Third Air Force's establishment to 140: the pool, from which the other commands draw, is full, so 40 fighters and 70 bombers are struck off."]);
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

describe("finding 13: a nation whose aircraft reach no enemy at all is told where a field would bring the enemy in reach (USA from 40, seed 11)", () => {
  it("America in January 1943, the playtester's war: the raids pages name the province, by the rules' own reach and field rule, and open fast", () => {
    TC.start({ view: 'grand', human: 'USA', from: 40, seed: 11, world: true });
    const g = copy();
    // (no kind of raid has a target in reach of any of her groups: the only case the line is shown in)
    for (const k of ['bomb', 'night', 'ground', 'line', 'superiority', 'town', 'harbour', 'strike', 'fleet', 'recon'] as const) expect(raidTargetsFor(g, k).length, k).toBe(0);
    const mine = g.airFleets.filter((f) => f.nat === 'USA' && f.ftr + f.bmr > 0 && f.flew !== g.turn && f.mission === 'support');
    expect(mine.length).toBeGreaterThan(0);
    // (she has no engineer corps, and the arsenal offers her one: the UI director's second form, 14:06Z)
    expect(g.corps.some((c) => c.nat === 'USA' && !c.dead && c.kind === 'eng')).toBe(false);
    const said = /^No target is in reach\. Your aircraft could reach the enemy from (.+) if it had an airfield: an engineer corps can lay one there\. You have none: raise one in the ARSENAL\.$/.exec(TC.page('raid:ground').headline);
    expect(said?.[1]).toBeTruthy();
    // The province: his, no field, one an engineer corps could lay out, nearest the enemy, and with the enemy in the
    // longest reach of his aircraft from it.
    const reach = Math.max(...mine.map((f) => Math.max(f.bmr > 0 ? airRange(f, g).bmr : 0, f.ftr > 0 ? airRange(f, g).ftr : 0)));
    const foes = g.regions.filter((r) => atWar(g, 'USA', r.ctrl));
    const near = (id: number): number => Math.min(...foes.map((e) => km(g, id, e.id)));
    const can = g.regions.filter((r) => r.ctrl === 'USA' && !hasAirfield(g, r.id) && engineers.fieldCanBeLaid(g, 'USA', r)).sort((a, b) => near(a.id) - near(b.id));
    const at = can[0];
    expect(at.name).toBe(said![1]);
    expect(near(at.id)).toBeLessThanOrEqual(reach);
    const want = `No target is in reach. Your aircraft could reach the enemy from ${at.name} if it had an airfield: an engineer corps can lay one there. You have none: raise one in the ARSENAL.`;
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

  it('Mexico at war with Guatemala, whose aircraft can strike the merchant shipping in the Gulf of Tehuantepec, is not told it (MEX from 1, seed 9)', () => {
    TC.start({ view: 'grand', human: 'MEX', from: 1, seed: 9, depth: 'supreme', world: true });
    TC.declare('GUA');
    TC.endMonth();
    const g = copy();
    expect(raidTargetsFor(g, 'strike').map((t) => t.name)).toEqual(['Gulf of Tehuantepec']);
    expect(raidTargetsFor(g, 'ground').length).toBe(0);
    expect(TC.page('raid:ground').headline).toBe('No target is in reach');
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

// The sceptical review of these words (25 November 2026): where a line was told that was not true, and the mends.

describe("the review's mends: a lowered establishment is told only where a dated row lowers it", () => {
  /** A nation's war opened at `from`, every one of its groups at its establishment and its pool at `pool`; then the month `to` opened. */
  function opened(nat: string, from: number, to: number, pool: { ftr: number; bmr: number; pilots: number } = { ftr: 0, bmr: 0, pilots: 0 }): { g: Game; mark: ReturnType<typeof monthMark> } {
    const g = startOpen(createGame(1, nat));
    g.turn = from;
    airUpkeep(g, nat);
    for (const x of g.airFleets) if (x.nat === nat && x.ship === undefined) { x.ftr = x.est.ftr; x.bmr = x.est.bmr; }
    Object.assign(airPool(g, nat), pool);
    const mark = monthMark(g);
    g.turn = to;
    airUpkeep(g, nat);
    return { g, mark };
  }
  const told = (g: Game, nat: string): string[] => g.headlines.filter((h) => h.turn === g.turn && h.nat === nat && /establishment/.test(h.text)).map((h) => h.text);
  const foot = (g: Game, mark: ReturnType<typeof monthMark>): string[] => monthCard(g, mark, g.human, { whole: true }).also.filter((x) => /establishment|cut back/.test(x));

  it('Britain, September 1940: No. 11 Group is raised from 220 to 330 and grows a second group; nothing is told, in the news or on the month card', () => {
    for (const pool of [{ ftr: 0, bmr: 0, pilots: 0 }, { ...AIR.reserve.UK }]) {
      const { g, mark } = opened('UK', 8, 9, pool);
      // (the old group is kept at its half of 330, and gives up what it held over that: a share, not a lowering)
      const eleven = g.airFleets.filter((x) => x.nat === 'UK' && x.parent === 'No. 11 Group' && x.kind === 'ftr');
      expect(eleven.length).toBe(2);
      expect(told(g, 'UK')).toEqual([]);
      expect(foot(g, mark)).toEqual([]);
    }
  });

  it('Germany, October 1940: Luftflotten 2 and 3 are raised and tell nothing; Luftflotten 1 and 4 are lowered and are told', () => {
    const { g, mark } = opened('GER', 9, 10);
    const lines = told(g, 'GER');
    expect(lines.map((t) => /^The air staff has lowered (.+)'s establishment/.exec(t)?.[1])).toEqual(['Luftflotte 1', 'Luftflotte 4']);
    expect(lines.every((t) => loweredOf(t))).toBe(true);
    // (two commands: the card's line, where it has room for it, is the sum of two; never a line for Luftflotte 2 or 3)
    expect(loweredSum(lines)!.text).toMatch(/^The establishments of 2 commands were lowered: /);
    expect(foot(g, mark).filter((t) => /Luftflotte [23]|of [3-9] commands/.test(t))).toEqual([]);
  });

  it("a row that raises one kind and lowers the other names both kinds, each at its own establishment (Luftflotte 1, the rows of months 46 and 55)", () => {
    // (air.json: Luftflotte 1 goes from 250 fighters and 450 bombers to 600 and 250 in month 46, and to 1,100 and 150 in
    // month 55; the UI director's sentence of 14:06Z)
    const row = AIR.fleets.find((r) => r.nat === 'GER' && r.name === 'Luftflotte 1')!.est;
    expect(row.filter((e) => e[0] === 20 || e[0] === 46 || e[0] === 55)).toEqual([[20, 250, 450], [46, 600, 250], [55, 1100, 150]]);
    const head46 = "The air staff has lowered Luftflotte 1's bomber establishment to 250 and raised its fighter establishment to 600";
    expect(told(opened('GER', 45, 46).g, 'GER')).toEqual([`${head46}: 200 bombers go back to the pool, from which the other commands draw.`]);
    expect(told(opened('GER', 45, 46, { ...AIR.reserve.GER }).g, 'GER')).toEqual([`${head46}: the pool, from which the other commands draw, is full, so 200 bombers are struck off.`]);
    const head55 = "The air staff has lowered Luftflotte 1's bomber establishment to 150 and raised its fighter establishment to 1,100";
    expect(told(opened('GER', 54, 55).g, 'GER')).toEqual([`${head55}: 100 bombers go back to the pool, from which the other commands draw.`]);
    const part = opened('GER', 54, 55, { ftr: 0, bmr: 200, pilots: 0 });
    expect(told(part.g, 'GER')).toEqual([`${head55}: 50 bombers go back to the pool, from which the other commands draw, and the 50 it has no room for are struck off.`]);
    // (the month card, with one command lowered, gives its own line)
    expect(foot(part.g, part.mark)).toEqual(told(part.g, 'GER'));
  });
});

describe("the review's mends: what a lowering really strikes off, where pilots limit what the other commands can take back (GER, May 1941)", () => {
  /** Germany's war opened in April 1941, every group at its establishment, her pool full but with no pilots; then May opened, its rows lowered or (`lower` false) left as April's. */
  function may1941(lower: boolean): { g: Game; mark: ReturnType<typeof monthMark> } {
    const rows = AIR.fleets.filter((r) => r.nat === 'GER' && r.est.some((e) => e[0] === 20));
    const kept = rows.map((r) => r.est);
    if (!lower) for (const r of rows) r.est = r.est.map((e, i, a) => (e[0] === 20 && i > 0 ? [20, Math.max(e[1], a[i - 1][1]), Math.max(e[2], a[i - 1][2])] : e));
    try {
      const g = startOpen(createGame(1, 'GER'));
      g.turn = 19;
      airUpkeep(g, 'GER');
      for (const x of g.airFleets) if (x.nat === 'GER' && x.ship === undefined) { x.ftr = x.est.ftr; x.bmr = x.est.bmr; }
      Object.assign(airPool(g, 'GER'), { ftr: AIR.reserve.GER.ftr, bmr: AIR.reserve.GER.bmr, pilots: 0 });
      const mark = monthMark(g);
      g.turn = 20;
      airUpkeep(g, 'GER');
      return { g, mark };
    } finally { rows.forEach((r, i) => { r.est = kept[i]; }); }
  }
  const total = (g: Game): { ftr: number; bmr: number } => {
    const p = airPool(g, 'GER');
    return g.airFleets.filter((x) => x.nat === 'GER').reduce((t, x) => ({ ftr: t.ftr + x.ftr, bmr: t.bmr + x.bmr }), { ftr: p.ftr, bmr: p.bmr });
  };
  // The figures read from a line as it is printed (in the test, not by the rules' own reader).
  const num = (s: string, kind: string): number => Number(new RegExp(`([\\d,]+) ${kind}s?`).exec(s)?.[1].replace(/,/g, '') ?? 0);
  const parts = (t: string): { back: string; off: string } => {
    const tail = t.slice(t.indexOf(': ') + 2);
    if (tail.startsWith('the pool, from which the other commands draw, is full, so ')) return { back: '', off: tail };
    const at = tail.indexOf(', and the ');
    return at < 0 ? { back: tail, off: '' } : { back: tail.slice(0, at), off: tail.slice(at) };
  };

  it('the fighters said to be struck off are the aircraft the war truly has fewer of, measured against the same month with the rows not lowered', () => {
    const { g } = may1941(true);
    const lines = g.headlines.filter((h) => h.turn === 20 && h.nat === 'GER' && /^The air staff has lowered/.test(h.text)).map((h) => h.text);
    expect(lines.length).toBe(2);
    const off = { ftr: lines.reduce((n, t) => n + num(parts(t).off, 'fighter'), 0), bmr: lines.reduce((n, t) => n + num(parts(t).off, 'bomber'), 0) };
    const gave = { ftr: lines.reduce((n, t) => n + num(t, 'fighter'), 0), bmr: lines.reduce((n, t) => n + num(t, 'bomber'), 0) };
    const a = total(g), b = total(may1941(false).g);
    expect(off).toEqual({ ftr: b.ftr - a.ftr, bmr: b.bmr - a.bmr });
    // (pilots limit the draw here: the fighters that came back with their pilots let other groups draw, so fewer are
    // lost than came back, though the pool is full)
    expect(off.ftr).toBeGreaterThan(0);
    expect(off.ftr).toBeLessThan(gave.ftr);
  });

  it("the month card's summed line is the sum of the commands' own lines", () => {
    const { g, mark } = may1941(true);
    const lines = g.headlines.filter((h) => h.turn === 20 && h.nat === 'GER' && /^The air staff has lowered/.test(h.text)).map((h) => h.text);
    const back = { ftr: lines.reduce((n, t) => n + num(parts(t).back, 'fighter'), 0), bmr: lines.reduce((n, t) => n + num(parts(t).back, 'bomber'), 0) };
    const off = { ftr: lines.reduce((n, t) => n + num(parts(t).off, 'fighter'), 0), bmr: lines.reduce((n, t) => n + num(parts(t).off, 'bomber'), 0) };
    const words = (c: { ftr: number; bmr: number }): string => [c.ftr ? `${c.ftr.toLocaleString('en-US')} fighters` : '', c.bmr ? `${c.bmr.toLocaleString('en-US')} bombers` : ''].filter(Boolean).join(' and ');
    expect(back.ftr + back.bmr).toBeGreaterThan(0);
    expect(off.ftr + off.bmr).toBeGreaterThan(0);
    const want = `The establishments of ${lines.length} commands were lowered: ${words(back)} go back to the pool, and the ${words(off)} it has no room for are struck off.`;
    expect(monthCard(g, mark, g.human, { whole: true }).also).toContain(want);
  });
});

describe("the review's mends: the pages' lines only where they are true (GER from 17, seed 22)", () => {
  // (Seed 7 on 1.5.2; release 2's war (pass 2, 26 November 2026) gives seed 7's Germany four troop targets in reach.
  // Seeds 1 to 30 scanned: 22 and 27 keep none, with bombing targets, and group 31 in the night campaign over London.)
  it("finding 13's line is not shown where his aircraft reach the enemy for another kind of raid", () => {
    TC.start({ view: 'grand', human: 'GER', from: 17, seed: 22, world: true });
    const g = copy();
    expect(raidTargetsFor(g, 'bomb').length).toBeGreaterThan(0);
    expect(raidTargetsFor(g, 'ground').length).toBe(0);
    expect(TC.page('raid:ground').headline).toBe('No target is in reach');
    expect(TC.page('raid:line').headline).toBe('No target is in reach');
  });

  it("the campaign's way out is not shown on a targets page whose kind would stay empty once the group is freed", () => {
    TC.start({ view: 'grand', human: 'GER', from: 17, seed: 22, world: true });
    const g = copy();
    expect(campaignWhy(g, 31)).toMatch(/^It flies with the night campaign over London\./);
    const ground = TC.page('raid:ground:31');
    expect(ground.sections[0].text).toBe('Move a group to an airfield nearer the enemy, or wait until one that has flown is back.');
    // (where freeing it would give it a target, the line stays)
    expect(TC.page('raid:bomb:31').sections[0].text).toBe(campaignWhy(g, 31));
    // Freed, it has bombing targets and still no troops in reach.
    const free = copy();
    expect(setGroupJob(free, 31, 'patrol')).toBe(true);
    expect(raidTargetsFor(free, 'ground', 31).length).toBe(0);
    expect(raidTargetsFor(free, 'bomb', 31).length).toBeGreaterThan(0);
    // Reading the page changes nothing of the group's orders.
    const after = copy().airFleets.find((x) => x.id === 31)!;
    expect([after.mission, after.target]).toEqual([g.airFleets.find((x) => x.id === 31)!.mission, g.airFleets.find((x) => x.id === 31)!.target]);
  });
});

describe("the review's mends: the 3D month's notes, a raid broken off, and a fraction of a fighter", () => {
  it("each step's note keeps the first two raids as 1.5.1 did, empty or not: another raid's result never moves up into an empty one's place (UK from 10, seed 3)", () => {
    TC.start({ view: 'grand', human: 'UK', from: 10, seed: 3, world: true });
    const r = TC.endMonth();
    const ita = (r.steps as { nat: string; note: string; raids?: unknown[] }[]).find((s) => s.nat === 'ITA' && s.raids?.length)!;
    // (Italy's month: an empty result, then Paris, then Lyon; only the first two take the note's places)
    expect(ita.note).toContain('The bombs broke down 20% of the works of Paris');
    expect(ita.note).not.toContain('works of Lyon');
    // (and no note starts or ends with a space, or has two together, where an empty result was left out)
    for (const s of r.steps as { note: string }[]) expect(s.note).toBe(s.note.trim().replace(/ {2,}/g, ' '));
  });

  it('a fight for the sky broken off before it met anyone keeps its own line, never the empty sweep\'s', () => {
    const { g, f } = overTheChannel('ftr', 80);
    for (const x of g.airFleets) if (x.nat === 'GER') x.ftr = 0;
    const rep = fly(g, 'USA', 'superiority', regionOf(g, 'Calais'), [f], '', { lost: {}, dropped: 0, gaveUp: 'att' })!;
    expect(rep.def).toEqual([]);
    expect(rep.result).toBe('The fighters broke off and turned for home before they met the enemy.');
    expect(brokeOff(rep)).toBe(true);
  });

  it('a plan whose fighters round to none says nothing of "sending 0": it is sending none', () => {
    const { g, f } = overTheChannel('bmr', 50);
    f.ftr = 0.4;
    for (const x of g.airFleets) if (x.nat === 'GER') x.ftr = 0;
    const p = planRaid(g, 'superiority', regionOf(g, 'Calais'), [f.id], 'USA');
    expect(p.why).toBeNull();
    expect(p.fighters).toBeCloseTo(0.4);
    expect(p.tooFew).toBeUndefined();
    expect(JSON.stringify(p)).not.toContain('sending 0');
  });
});

describe("the UI director's nine answers on the air words (25 November 2026, 14:06Z)", () => {
  /** A nation's war opened at `from`, every one of its groups at its establishment, `ready` done to it, its pool at `pool`; then the month `to` opened. */
  function month(nat: string, from: number, to: number, pool: { ftr: number; bmr: number; pilots: number }, ready: (g: Game) => void = () => {}): { g: Game; mark: ReturnType<typeof monthMark> } {
    const g = startOpen(createGame(1, nat));
    g.turn = from;
    airUpkeep(g, nat);
    for (const x of g.airFleets) if (x.nat === nat && x.ship === undefined) { x.ftr = x.est.ftr; x.bmr = x.est.bmr; }
    ready(g);
    Object.assign(airPool(g, nat), pool);
    const mark = monthMark(g);
    g.turn = to;
    airUpkeep(g, nat);
    return { g, mark };
  }
  const none = { ftr: 0, bmr: 0, pilots: 0 };
  const told = (g: Game, nat: string): string[] => g.headlines.filter((h) => h.turn === g.turn && h.nat === nat && /establishment/.test(h.text)).map((h) => h.text);
  const whole = (g: Game, mark: ReturnType<typeof monthMark>): string[] => monthCard(g, mark, g.human, { whole: true }).also;
  /** A command's dated rows set for the test, and put back after. */
  function withRows<T>(nat: string, name: string, est: [number, number, number][], run: () => T): T {
    const r = AIR.fleets.find((x) => x.nat === nat && x.name === name)!;
    const was = r.est;
    r.est = est;
    try { return run(); } finally { r.est = was; }
  }

  it("answer 1, the mirror case built by hand: fighters lowered and bombers raised read the other way round", () => {
    // (Luftflotte 1's row of month 46 set to 200 fighters and 600 bombers: its fighter groups, kept at 250, give up 50)
    const g = withRows('GER', 'Luftflotte 1', [[1, 300, 500], [10, 80, 150], [20, 250, 450], [46, 200, 600]], () => month('GER', 45, 46, none).g);
    expect(told(g, 'GER')).toEqual(["The air staff has lowered Luftflotte 1's fighter establishment to 200 and raised its bomber establishment to 600: 50 fighters go back to the pool, from which the other commands draw."]);
  });

  it('answer 1: where no aircraft leaves the command, nothing is told; a row that lowers one kind and leaves the other prints as before', () => {
    // (its bomber groups already at 200 in all, under the new 250: none leaves; its fighter groups' new group still forms)
    const { g, mark } = month('GER', 45, 46, none, (h) => {
      const bombers = h.airFleets.filter((x) => x.nat === 'GER' && x.parent === 'Luftflotte 1' && x.kind === 'bmr');
      for (const x of bombers) x.bmr = Math.floor(200 / bombers.length);
    });
    expect(told(g, 'GER')).toEqual([]);
    expect(whole(g, mark).filter((t) => /establishment|cut back/.test(t))).toEqual([]);
    expect(g.headlines.some((h) => h.turn === 46 && /forms in .+, a new group of Luftflotte 1\./.test(h.text))).toBe(true);
    // (the fighters held at 250 and the bombers lowered to 250: the command's whole establishment, as before)
    const held = withRows('GER', 'Luftflotte 1', [[1, 300, 500], [10, 80, 150], [20, 250, 450], [46, 250, 250]], () => month('GER', 45, 46, none).g);
    expect(told(held, 'GER')).toEqual(["The air staff has lowered Luftflotte 1's establishment to 500: 200 bombers go back to the pool, from which the other commands draw."]);
  });

  it("answer 1: on the month card such a command is one of the commands lowered, its aircraft in the sums, and the summed line is the sum of the commands' own lines", () => {
    // (Luftflotte 2 given a row for month 46 that lowers it from 400 and 550 to 300 and 400, beside Luftflotte 1's mixed row)
    const { g, mark } = withRows('GER', 'Luftflotte 2', [[1, 250, 450], [10, 650, 750], [20, 400, 550], [46, 300, 400]], () => month('GER', 45, 46, { ftr: 0, bmr: 400, pilots: 0 }));
    const lines = told(g, 'GER');
    expect(lines.length).toBe(2);
    expect(lines[0]).toMatch(/^The air staff has lowered Luftflotte 1's bomber establishment to 250 and raised its fighter establishment to 600: /);
    expect(lines[1]).toMatch(/^The air staff has lowered Luftflotte 2's establishment to 700: /);
    // The figures, read from the lines as printed (here, not by the rules' own reader).
    const num = (s: string, kind: string): number => Number(new RegExp(`([\\d,]+) ${kind}s?`).exec(s)?.[1].replace(/,/g, '') ?? 0);
    const parts = (t: string): { back: string; off: string } => {
      const tail = t.slice(t.indexOf(': ') + 2);
      if (tail.startsWith('the pool, from which the other commands draw, is full, so ')) return { back: '', off: tail };
      const at = tail.indexOf(', and the ');
      return at < 0 ? { back: tail, off: '' } : { back: tail.slice(0, at), off: tail.slice(at) };
    };
    const of = (k: 'back' | 'off', kind: string): number => lines.reduce((n, t) => n + num(parts(t)[k], kind), 0);
    const words = (f: number, b: number): string => [f ? `${f.toLocaleString('en-US')} fighters` : '', b ? `${b.toLocaleString('en-US')} bombers` : ''].filter(Boolean).join(' and ');
    const back = words(of('back', 'fighter'), of('back', 'bomber')), off = words(of('off', 'fighter'), of('off', 'bomber'));
    expect(back).not.toBe('');
    expect(off).not.toBe('');
    const want = `The establishments of 2 commands were lowered: ${back} go back to the pool, and the ${off} it has no room for are struck off.`;
    expect(whole(g, mark)[0]).toBe(want);
  });

  it("answer 2: the month card's foot puts the lowered line first, and four 'forms' lines that do not all fit are one line (Germany, turn 10)", () => {
    const { g, mark } = month('GER', 9, 10, none);
    const lines = told(g, 'GER');
    expect(lines.map((t) => /^The air staff has lowered (.+)'s establishment/.exec(t)?.[1])).toEqual(['Luftflotte 1', 'Luftflotte 4']);
    const forms = g.headlines.filter((h) => h.turn === 10 && h.nat === 'GER' && / forms in /.test(h.text));
    expect(forms.length).toBe(4);
    const also = whole(g, mark);
    expect(also[0]).toBe(loweredSum(lines)!.text);
    expect(also[1]).toBe('4 air commands formed');
    expect(also.filter((t) => / forms in /.test(t))).toEqual([]);
    // (the card whose lines must fit: the summed line or its short form, first, then the one line for the four)
    const card = monthCard(g, mark).also;
    expect([loweredSum(lines)!.text, loweredSum(lines)!.short]).toContain(card[0]);
    expect(card[1]).toBe('4 air commands formed');
  });

  it("answer 2: where the 'forms' lines all fit they stay as they are, after the lowered line (Germany, turn 55)", () => {
    const { g, mark } = month('GER', 54, 55, none);
    expect(whole(g, mark)).toEqual([
      "The air staff has lowered Luftflotte 1's bomber establishment to 150 and raised its fighter establishment to 1,100: 100 bombers go back to the pool, from which the other commands draw.",
      '5. Jagddivision forms in Berlin, a new group of Luftflotte 1.',
      '6. Jagddivision forms in Berlin, a new group of Luftflotte 1.',
    ]);
  });

  it("answer 3: a group's note names the place with its article, as the campaign's line does: 'Ordered against the Midlands' (GER from 17, seed 7)", () => {
    TC.start({ view: 'grand', human: 'GER', from: 17, seed: 7, world: true });
    const g = copy();
    const f = g.airFleets.find((x) => x.id === 31)!;
    f.mission = 'bomb';
    f.target = regionOf(g, 'Midlands');
    expect(groupRow(g, f).doing).toBe('Ordered against the Midlands: flies when the month ends');
    expect(campaignWhy(g, 31)).toMatch(/^It flies with the day campaign over the Midlands\./);
    f.flew = g.turn;
    expect(groupRow(g, f).doing).toBe('Flew against the Midlands this month: back at its airfield next month');
    // (a place with no article is as it was)
    delete f.flew;
    f.target = regionOf(g, 'London');
    expect(groupRow(g, f).doing).toBe('Ordered against London: flies when the month ends');
  });

  it("answer 4: with an engineer corps, the line ends as it did (America, an engineer corps raised from the arsenal, USA from 40, seed 11)", () => {
    TC.start({ view: 'grand', human: 'USA', from: 40, seed: 11, world: true });
    // (Casablanca on 1.5.2; Oran on release 2's war, pass 2 of 26 November 2026: the airfield the line names is the war's.)
    expect(TC.order('corps:eng', -1).ok).toBe(true);
    // (on order and not yet raised: the month it is due, as the arsenal's order list gives it; the UI director, 15:04Z)
    TC.endMonth();
    expect(TC.page('raid:ground').headline).toBe('No target is in reach. Your aircraft could reach the enemy from Oran if it had an airfield: an engineer corps can lay one there. You have one on order in the ARSENAL, due March 1943.');
    TC.endMonth();
    TC.endMonth();
    const g = copy();
    expect(g.corps.filter((c) => c.nat === 'USA' && !c.dead && c.kind === 'eng').length).toBe(1);
    expect(TC.page('raid:ground').headline).toBe('No target is in reach. Your aircraft could reach the enemy from Oran if it had an airfield: an engineer corps can lay one there.');
  });

  it("answer 4: where his nation can never raise an engineer corps, the engineer clause is dropped (the arsenal's list made to hold none, by hand)", () => {
    TC.start({ view: 'grand', human: 'USA', from: 40, seed: 11, world: true });
    const g = copy();
    // (the arsenal offers the engineer corps to every nation in the grand campaign: the case is built by taking it out
    // of the arsenal's list for the test)
    expect(offers(g, 'USA').some((o) => o.key === 'corps:eng')).toBe(true);
    const land = (ARSENAL as unknown as { land: Record<string, unknown> }).land, eng = land.eng;
    delete land.eng;
    try {
      expect(offers(g, 'USA').some((o) => o.key === 'corps:eng')).toBe(false);
      expect(airPage(g, 'raid:ground')!.headline).toBe('No target is in reach. Your aircraft could reach the enemy from Oran if it had an airfield.');
    } finally { land.eng = eng; }
  });

  it("answer 5: the raid report tells a defence that never rose from one that rose, by the groups it keeps", () => {
    // (ui/main.ts airReportCard gives air.ts skyWords: the defenders it lists are those that rose; with none, "the
    // defenders put up no fighters")
    expect(page).toContain('const sky = esc(skyWords(g, r));');
    const sweep = (defenders: number): AirReport => {
      const { g, f } = overTheChannel('ftr', 40);
      let first = true;
      for (const x of g.airFleets) if (x.nat === 'GER') {
        if (first && defenders > 0 && x.kind === 'ftr' && x.ship === undefined) { x.ftr = defenders; x.base = regionOf(g, 'Calais'); x.mission = 'support'; first = false; } else x.ftr = 0;
      }
      Object.assign(airPool(g, 'GER'), { ftr: 0, pilots: 0 });
      return fly(g, 'USA', 'superiority', regionOf(g, 'Calais'), [f], '')!;
    };
    // Nobody rose: no defending group, the reading 99, the sky not won (40 fighters are under the least a sky needs).
    const nobody = sweep(0);
    expect([nobody.def.length, nobody.hold, nobody.sky, brokeOff(nobody)]).toEqual([0, 99, false, false]);
    // Two fighters rose and were worn down: the reading is over 9 too, and the report keeps the group that rose.
    const rose = sweep(2);
    expect(rose.def.length).toBe(1);
    expect(rose.def[0].ftr).toBe(2);
    expect(rose.hold).toBeGreaterThanOrEqual(9);
    expect([rose.sky, brokeOff(rose)]).toEqual([false, false]);
  });

  it("answer 6: reading a campaign group's pages leaves the save byte-identical, a group given escorts and close by hand included (GER from 17, seed 7)", () => {
    TC.start({ view: 'grand', human: 'GER', from: 17, seed: 7, world: true });
    const g = copy();
    // (the two keys set among the group's others, not at its end, as a save that carried them would have them)
    const at = g.airFleets.findIndex((x) => x.id === 31);
    const was = g.airFleets[at] as unknown as Record<string, unknown>;
    const keys = Object.keys(was);
    expect(keys.indexOf('mission')).toBeLessThan(keys.length - 1);
    g.airFleets[at] = Object.fromEntries(keys.flatMap((k) => (k === 'mission' ? [[k, was[k]], ['escorts', 5], ['close', true]] : [[k, was[k]]]))) as unknown as AirFleet;
    expect(campaignWhy(g, 31)).not.toBeNull();
    const before = saveGame(g);
    for (const id of ['group:31', 'raid:bomb:31', 'raid:night:31', 'raid:ground:31', 'raid:harbour:31']) airPage(g, id);
    expect(saveGame(g)).toBe(before);
    // (the page did read the group as freed: its campaign line is on the bombing page)
    expect(airPage(g, 'raid:bomb:31')!.sections[0].text).toBe(campaignWhy(g, 31));
  });
});

describe("the UI director's answers of 15:04Z on the air words", () => {
  /** The 2D month card (the card whose lines must fit) and the whole card of the month a war of Germany's opens at 46, played from `from` on `seed`. */
  function may1943(from: number, seed: number): { g: Game; mark: ReturnType<typeof monthMark>; card: string[]; whole: string[] } {
    TC.start({ view: 'grand', human: 'GER', from, seed, world: true });
    while (copy().turn < 45) TC.endMonth();
    const mark = monthMark(copy());
    TC.endMonth();
    const g = copy();
    expect(g.turn).toBe(46);
    return { g, mark, card: monthCard(g, mark).also, whole: monthCard(g, mark, 'GER', { whole: true }).also };
  }
  const lowered46 = "The air staff has lowered Luftflotte 1's bomber establishment to 250 and raised its fighter establishment to 600: the pool, from which the other commands draw, is full, so 200 bombers are struck off.";

  it("answers 1 and 2: Germany, May 1943 (GER from 40, seed 13): the 2D card gives Luftflotte 1's line in its short form first, and the two 'forms' lines, the second of which it showed cut, are summed", () => {
    const { g, mark, card, whole } = may1943(40, 13);
    expect(whole[0]).toBe(lowered46);
    // (1.5.2 before these answers: ['5. Jagddivision forms in Konigsberg, a new group of Luftflotte 1.', '6. Jagddivision forms in Konigsberg, a new group…'])
    expect(card).toEqual(['Luftflotte 1 cut back: 200 aircraft struck off', '2 air commands formed']);
    // Built from this month by hand: its lowered line taken out of the news. The second 'forms' line would be shown
    // cut; a cut line counts as not fitting, so the two are summed.
    const noLow = copy();
    noLow.headlines = noLow.headlines.filter((h) => !(h.turn === 46 && loweredOf(h.text)));
    expect(monthCard(noLow, mark).also).toEqual(['2 air commands formed', 'LXVIII Panzer Corps has finished forming at Berlin and is ready for orders.']);
    // A single 'forms' line that will not fit is as before: shown cut, never "1 air commands formed" (the lowered line
    // and the first 'forms' line taken out, the second put after the month's other news, by hand).
    const one = copy();
    const forms = one.headlines.filter((h) => h.turn === 46 && / forms in /.test(h.text));
    expect(forms.map((h) => h.text)).toEqual(['5. Jagddivision forms in Konigsberg, a new group of Luftflotte 1.', '6. Jagddivision forms in Konigsberg, a new group of Luftflotte 1.']);
    one.headlines = [...one.headlines.filter((h) => !(h.turn === 46 && (loweredOf(h.text) || / forms in /.test(h.text)))), forms[1]];
    expect(monthCard(one, mark).also).toEqual(['LXVIII Panzer Corps has finished forming at Berlin and is ready for orders.', '6. Jagddivision forms in Konigsberg, a new group…']);
    expect(g.headlines.some((h) => h.turn === 46 && h.text === lowered46)).toBe(true);
  });

  it("answer 1: Germany, May 1943 (GER from 44, seed 7): the short form is the first of the 2D card's lines", () => {
    const { card, whole } = may1943(44, 7);
    expect(whole[0]).toBe(lowered46);
    expect(card).toEqual(['Luftflotte 1 cut back: 200 aircraft struck off', 'Otto Hofmann makes his name at Sumy and is given command of LII Corps.']);
  });

  it("answer 1: Germany, May 1943 (GER from 44, seed 21): the short form is the first of the 2D card's lines", () => {
    const { card, whole } = may1943(44, 21);
    expect(whole[0]).toBe(lowered46);
    expect(card).toEqual(['Luftflotte 1 cut back: 200 aircraft struck off', 'Rudolf Schmidt reaches level 5 and has a skill point to spend.']);
  });

  it('answer 1: the short forms of one command\'s own line, each where it is true, and the singular', () => {
    const none = { ftr: 0, bmr: 0 };
    const est = { low: 'bmr' as const, to: 250, raised: 600 };
    expect(loweredShort(loweredLine('Luftflotte 1', est, none, { ftr: 0, bmr: 200 }))).toBe('Luftflotte 1 cut back: 200 aircraft struck off');
    expect(loweredShort(loweredLine('Luftflotte 1', est, { ftr: 0, bmr: 200 }, none))).toBe('Luftflotte 1 cut back: 200 aircraft back to the pool');
    // (some back, some struck off: the short form gives those struck off, as the summed line's does)
    expect(loweredShort(loweredLine('First Air Force', 160, { ftr: 50, bmr: 0 }, { ftr: 30, bmr: 0 }))).toBe('First Air Force cut back: 30 aircraft struck off');
    expect(loweredShort(loweredLine('First Air Force', 160, { ftr: 40, bmr: 70 }, none))).toBe('First Air Force cut back: 110 aircraft back to the pool');
    expect(loweredShort(loweredLine('No. 11 Group', 1200, none, { ftr: 1, bmr: 0 }))).toBe('No. 11 Group cut back: 1 aircraft struck off');
    expect(loweredShort(loweredLine('No. 11 Group', 1200, { ftr: 1, bmr: 0 }, none))).toBe('No. 11 Group cut back: 1 aircraft back to the pool');
    expect(loweredShort(loweredLine('Luftflotte 1', 1250, { ftr: 0, bmr: 1 }, { ftr: 0, bmr: 99 }))).toBe('Luftflotte 1 cut back: 99 aircraft struck off');
    expect(loweredShort('2. Jagddivision forms in Berlin.')).toBeNull();
    // The summed line's short forms, as built before (two commands).
    const two = [loweredLine('Luftflotte 1', 500, none, { ftr: 0, bmr: 200 }), loweredLine('Luftflotte 4', 400, none, { ftr: 0, bmr: 200 })];
    expect(loweredSum(two)!.short).toBe('2 commands cut back: 400 aircraft struck off');
    expect(loweredSum([loweredLine('Luftflotte 1', 500, { ftr: 0, bmr: 200 }, none), loweredLine('Luftflotte 4', 400, { ftr: 0, bmr: 200 }, none)])!.short).toBe('2 commands cut back: 400 aircraft back to the pool');
  });

  it('answer 2: where a line has a short form, the card never shows it cut', () => {
    const part = 'The establishments of 2 commands were lowered: 281 fighters and 200 bombers go back to the pool, and the 29 fighters and 400 bombers it has no room for are struck off.';
    // (with no short form, fitFoot shows it cut, its point kept)
    expect(fitFoot([part], 35, 3, 3)).toEqual([{ text: 'The establishments of 2 commands were lowered: 281 fighters and 200 bombers go back to the pool…', at: 0 }]);
    // (with a short form, the short form where it fits; where it does not, nothing: never the line cut)
    expect(fitFoot([part], 35, 3, 3, ['2 commands cut back: 429 aircraft struck off'])).toEqual([{ text: '2 commands cut back: 429 aircraft struck off', at: 0 }]);
    expect(fitFoot([part], 35, 3, 3, ['2 commands cut back: 429 aircraft struck off, a short form made by hand too long for the three lines of thirty-five letters this box has'])).toEqual([]);
  });

  /** A sweep of `n` American fighters over Calais met by one German fighter group of `d`, flown with `salt`; `estZero` gives that group no fighter establishment (built by hand). */
  function sweep(n: number, d: number, salt = '', estZero = false): { g: Game; r: AirReport } {
    const { g, f } = overTheChannel('ftr', n);
    let first = true;
    for (const x of g.airFleets) if (x.nat === 'GER') {
      if (first && d > 0 && x.kind === 'ftr' && x.ship === undefined) { x.ftr = d; x.base = regionOf(g, 'Calais'); x.mission = 'support'; if (estZero) x.est = { ...x.est, ftr: 0 }; first = false; } else x.ftr = 0;
    }
    Object.assign(airPool(g, 'GER'), { ftr: 0, pilots: 0 });
    return { g, r: fly(g, 'USA', 'superiority', regionOf(g, 'Calais'), [f], salt)! };
  }

  it('answer 3: the raid report, the sky not won at 9 to 1 or more: "no fighters left to put up" only where every defender that rose was lost', () => {
    // Every defender that rose was lost (one fighter, shot down).
    const lost = sweep(45, 1, ':b');
    expect([lost.r.def.map((x) => [x.ftr, x.lostFtr]), lost.r.sky, lost.r.hold >= 9]).toEqual([[[1, 1]], false, true]);
    expect(skyWords(lost.g, lost.r)).toBe(' The sky was not won: at the end the defenders had no fighters left to put up, but it takes at least 50 fighters still over the target at the end as well, and the attackers had 45 left.');
    // Two rose, both lost.
    const both = sweep(45, 2, ':b');
    expect(both.r.def.map((x) => [x.ftr, x.lostFtr])).toEqual([[2, 2]]);
    expect(skyWords(both.g, both.r)).toBe(' The sky was not won: at the end the defenders had no fighters left to put up, but it takes at least 50 fighters still over the target at the end as well, and the attackers had 45 left.');
    // Nobody rose: as built (14:06Z).
    const nobody = sweep(40, 0);
    expect([nobody.r.def.length, nobody.r.hold, nobody.r.sky]).toEqual([0, 99, false]);
    expect(skyWords(nobody.g, nobody.r)).toBe(' The sky was not won: the defenders put up no fighters, but it takes at least 50 fighters still over the target at the end as well, and the attackers had 40 left.');
  });

  it('answer 3: defenders still flying at 9 to 1 or more: the true figure, as the sentence under 9 to 1 gives it', () => {
    // (1.5.2 before these answers said "at the end the defenders had no fighters left to put up" in each case)
    const one = sweep(45, 1);
    expect([one.r.def.map((x) => [x.ftr, x.lostFtr]), one.r.sky]).toEqual([[[1, 0]], false]);
    expect(one.r.hold).toBeCloseTo(58.44, 2);
    // (the figure from 10 to 1 up a whole number: the UI director's final read, 15:42Z; 15:04Z as built gave 'about 58.44 to 1')
    expect(skyWords(one.g, one.r)).toBe(" The sky was not won: at the end the attackers' fighters stood about 58 to 1 against those the defenders could still put up, which is enough, but it takes at least 50 fighters still over the target at the end as well, and the attackers had 45 left.");
    const three = sweep(45, 3);
    expect(three.r.def.map((x) => [x.ftr, x.lostFtr])).toEqual([[3, 1]]);
    expect(skyWords(three.g, three.r)).toBe(" The sky was not won: at the end the attackers' fighters stood about 19 to 1 against those the defenders could still put up, which is enough, but it takes at least 50 fighters still over the target at the end as well, and the attackers had 45 left.");
    // (under 9 to 1 the sentence is as it was)
    const five = sweep(30, 5);
    expect(five.r.hold).toBeLessThan(9);
    expect(skyWords(five.g, five.r)).toBe(` The sky was not won: at the end the attackers' fighters stood about ${five.r.hold.toFixed(2)} to 1 against those the defenders could still put up, which is enough, but it takes at least 50 fighters still over the target at the end as well, and the attackers had ${five.r.att[0].ftr - five.r.att[0].lostFtr} left.`);
  });

  it('answer 3: at the cap with defenders still flying, "more than 90 to 1" (built by hand: the defending group given no fighter establishment)', () => {
    const cap = sweep(45, 1, '', true);
    expect([cap.r.def.map((x) => [x.ftr, x.lostFtr]), cap.r.hold, cap.r.sky]).toEqual([[[1, 0]], 99, false]);
    expect(skyWords(cap.g, cap.r)).toBe(" The sky was not won: at the end the attackers' fighters stood more than 90 to 1 against those the defenders could still put up, which is enough, but it takes at least 50 fighters still over the target at the end as well, and the attackers had 45 left.");
    // (a sky that WAS won at 9 to 1 or more, defenders still flying: the figure, the UI director's final read of 15:42Z;
    // 15:04Z as built said 'at the end no opposition was left' here)
    const won = sweep(120, 2);
    expect([won.r.sky, won.r.def.map((x) => [x.ftr, x.lostFtr])]).toEqual([true, [[2, 1]]]);
    expect(skyWords(won.g, won.r)).toBe(' The American air force won the sky over Calais: at the end its fighters stood about 78 to 1 against those the defenders could still put up.');
  });

  it("answer 4: an engineer corps on order and not yet raised: 'You have one on order in the ARSENAL, due March 1943.' (USA from 40, seed 11)", () => {
    TC.start({ view: 'grand', human: 'USA', from: 40, seed: 11, world: true });
    const head = 'No target is in reach. Your aircraft could reach the enemy from Oran if it had an airfield: an engineer corps can lay one there.';
    // None living and none on order: as before.
    expect(TC.page('raid:ground').headline).toBe(`${head} You have none: raise one in the ARSENAL.`);
    expect(TC.order('corps:eng', -1).ok).toBe(true);
    const order = copy().nations.USA.queue.filter((b) => b.what === 'corps' && b.kind === 'eng');
    expect(order.map((b) => [b.ordered, b.eta])).toEqual([[40, 43]]);
    expect(TC.page('raid:ground').headline).toBe(`${head} You have one on order in the ARSENAL, due March 1943.`);
  });

  it("answer 5: a harbour order's note names the place with its article: 'Ordered against the harbour of the Highlands'", () => {
    const g = startOpen(createGame(3, 'GER'));
    const f = g.airFleets.find((x) => x.nat === 'GER' && x.bmr > 0 && x.ship === undefined)!;
    const order = (mission: AirFleet['mission'], place: string): string => { f.mission = mission; f.target = regionOf(g, place); delete f.flew; return groupRow(g, f).doing; };
    // (1.5.2 before these answers: 'Ordered against the harbour of Highlands: flies when the month ends')
    expect(order('harbour', 'Highlands')).toBe('Ordered against the harbour of the Highlands: flies when the month ends');
    expect(order('harbour', 'Crimea')).toBe('Ordered against the harbour of the Crimea: flies when the month ends');
    expect(order('harbour', 'London')).toBe('Ordered against the harbour of London: flies when the month ends');
    f.flew = g.turn;
    expect(groupRow(g, f).doing).toBe('Flew against the harbour of London this month: back at its airfield next month');
    // (the town form names the town, as before)
    expect(order('town', 'Highlands')).toBe('Ordered against the defences of Inverness: flies when the month ends');
  });
});

// The UI director's final read of the words list (25 November 2026, 15:42Z), marks (a), (b), (d), (e), (g) and (h).
describe("the UI director's final read (15:42Z)", () => {
  /** A sweep of `n` American fighters over Calais against German fighter groups of `ds`, flown with `salt`. */
  function sweep(n: number, ds: number[], salt = '', estZero = false): { g: Game; r: AirReport } {
    const { g, f } = overTheChannel('ftr', n);
    const left = [...ds];
    for (const x of g.airFleets) if (x.nat === 'GER') {
      if (left.length && x.kind === 'ftr' && x.ship === undefined) { x.ftr = left.shift()!; x.base = regionOf(g, 'Calais'); x.mission = 'support'; if (estZero) x.est = { ...x.est, ftr: 0 }; } else x.ftr = 0;
    }
    Object.assign(airPool(g, 'GER'), { ftr: 0, pilots: 0 });
    return { g, r: fly(g, 'USA', 'superiority', regionOf(g, 'Calais'), [f], salt)! };
  }
  const WON = ' The American air force won the sky over Calais: at the end';
  const NOT = " The sky was not won: at the end the attackers' fighters stood";
  const TAIL = 'against those the defenders could still put up, which is enough, but it takes at least 50 fighters still over the target at the end as well, and the attackers had 45 left.';

  it('(a) the sky won at 9 to 1 or more: "no opposition was left" only where none rose or every one that rose was lost; else the figure', () => {
    // Still flying: 120 against 2, one lost.
    const flying = sweep(120, [2]);
    expect([flying.r.sky, flying.r.def.map((x) => [x.ftr, x.lostFtr]), flying.r.hold >= 9]).toEqual([true, [[2, 1]], true]);
    expect(skyWords(flying.g, flying.r)).toBe(`${WON} its fighters stood about 78 to 1 against those the defenders could still put up.`);
    // Every one that rose was lost (the same report, its one group's fighters all lost, built by hand).
    const lost = { ...flying.r, def: flying.r.def.map((x) => ({ ...x, lostFtr: x.ftr })) };
    expect(skyWords(flying.g, lost)).toBe(`${WON} no opposition was left.`);
    // None rose.
    expect(skyWords(flying.g, { ...flying.r, def: [] })).toBe(`${WON} no opposition was left.`);
    // Under 9 to 1, as 1.5.1 printed it (150 against 30).
    const under = sweep(150, [30]);
    expect([under.r.sky, under.r.hold < 9]).toEqual([true, true]);
    expect(skyWords(under.g, under.r)).toBe(`${WON} its fighters stood about 6.32 to 1 against those the defenders could still put up.`);
  });

  it('(d), (b) the figure: two places under 10 to 1, a whole number from 10 up, "more than 90 to 1" above 90 and at the 99', () => {
    const { g, r } = sweep(45, [1]);
    const flying = { ...r, def: r.def.map((x) => ({ ...x, lostFtr: 0 })), sky: false };
    const not = (hold: number): string => skyWords(g, { ...flying, hold });
    const won = (hold: number): string => skyWords(g, { ...flying, hold, sky: true });
    expect(not(9.5)).toBe(`${NOT} about 9.50 to 1 ${TAIL}`);
    expect(won(9.5)).toBe(`${WON} its fighters stood about 9.50 to 1 against those the defenders could still put up.`);
    // The edge, by the figure as printed: 9.99 keeps its two places, and a reading that would print "10.00" is "10".
    expect(not(9.99)).toBe(`${NOT} about 9.99 to 1 ${TAIL}`);
    expect(not(9.997)).toBe(`${NOT} about 10 to 1 ${TAIL}`);
    expect(won(9.997)).toBe(`${WON} its fighters stood about 10 to 1 against those the defenders could still put up.`);
    expect(not(10)).toBe(`${NOT} about 10 to 1 ${TAIL}`);
    expect(not(58.44)).toBe(`${NOT} about 58 to 1 ${TAIL}`);
    expect(not(58.5)).toBe(`${NOT} about 59 to 1 ${TAIL}`);
    expect(won(19.48)).toBe(`${WON} its fighters stood about 19 to 1 against those the defenders could still put up.`);
    expect(not(90)).toBe(`${NOT} about 90 to 1 ${TAIL}`);
    expect(not(95.3)).toBe(`${NOT} more than 90 to 1 ${TAIL}`);
    expect(not(168.83)).toBe(`${NOT} more than 90 to 1 ${TAIL}`);
    expect(won(168.83)).toBe(`${WON} its fighters stood more than 90 to 1 against those the defenders could still put up.`);
    // The rules' 99 (the defending group given no fighter establishment, built by hand).
    const cap = sweep(45, [1], '', true);
    expect([cap.r.hold, cap.r.sky, cap.r.def.map((x) => [x.ftr, x.lostFtr])]).toEqual([99, false, [[1, 0]]]);
    expect(skyWords(cap.g, cap.r)).toBe(`${NOT} more than 90 to 1 ${TAIL}`);
    // A true reading above 90, flown: 130 against 1, none lost, the sky won.
    const far = sweep(130, [1]);
    expect([far.r.sky, far.r.hold > 90, far.r.hold !== 99]).toEqual([true, true, true]);
    expect(skyWords(far.g, far.r)).toBe(`${WON} its fighters stood more than 90 to 1 against those the defenders could still put up.`);
    // Under 10 the two places stand as 1.5.1 printed them.
    expect(not(7.53)).toBe(`${NOT} about 7.53 to 1 ${TAIL}`);
    expect(skyWords(g, { ...flying, hold: 1.5 })).toBe(`${NOT} about 1.50 to 1 against those the defenders could still put up, and it takes 1.65 to 1.`);
  });

  it('(a), (d) the sky not won at 9 to 1 or more, flown: the whole figure where a defender still flies', () => {
    const two = sweep(46, [2, 1], ':x');
    expect(two.r.def.map((x) => [x.ftr, x.lostFtr])).toEqual([[2, 1], [1, 1]]);
    expect(skyWords(two.g, two.r)).toBe(" The sky was not won: at the end the attackers' fighters stood about 20 to 1 against those the defenders could still put up, which is enough, but it takes at least 50 fighters still over the target at the end as well, and the attackers had 46 left.");
  });

  it("(e) two or more engineer corps on order: 'You have 2 on order in the ARSENAL, the first due March 1943.' (USA from 40, seed 11)", () => {
    TC.start({ view: 'grand', human: 'USA', from: 40, seed: 11, world: true });
    const head = 'No target is in reach. Your aircraft could reach the enemy from Oran if it had an airfield: an engineer corps can lay one there.';
    expect(TC.order('corps:eng', -1).ok).toBe(true);
    expect(TC.page('raid:ground').headline).toBe(`${head} You have one on order in the ARSENAL, due March 1943.`);
    expect(TC.order('corps:eng', -1).ok).toBe(true);
    expect(copy().nations.USA.queue.filter((b) => b.what === 'corps' && b.kind === 'eng').map((b) => b.eta)).toEqual([43, 43]);
    expect(TC.page('raid:ground').headline).toBe(`${head} You have 2 on order in the ARSENAL, the first due March 1943.`);
  });

  it("(g) the harbour with its article in all four forms: the report title, the mission words, the 3D battle ticket's title and the wrecked harbour", () => {
    expect(harbourOf('Highlands')).toBe('the harbour of the Highlands');
    expect(harbourOf('London')).toBe('the harbour of London');
    expect(harbourOf('Pearl Harbor')).toBe('Pearl Harbor');
    expect(ticketHarbourOf('Danzig Corridor')).toBe('the harbour of the Danzig Corridor');
    expect(ticketHarbourOf('Pearl Harbor')).toBe('Pearl Harbor');
    // The browser's report title and mission words (read from its source: the card cannot be drawn here).
    expect(page).toContain("if (r.mission === 'harbour') return `Raid on the harbour of ${placeName(r.place)}`;");
    expect(page).toContain("case 'harbour': return `raiding the harbour of ${placeName(where)}`;");
    // The 3D battle ticket (built by hand: the German Battle Fleet docked in the Danzig Corridor, the Home Fleet's
    // carriers in the Baltic).
    const g = startOpen(createGame(3, 'UK'));
    const port = regionOf(g, 'Danzig Corridor');
    const home = g.fleets.find((x) => x.name === 'Home Fleet')!, foe = g.fleets.find((x) => x.name === 'Battle Fleet')!;
    foe.home = port; foe.base = port; sail(g, foe, port, 'port');
    home.ships.cv = 6; home.zone = g.seas.find((z) => z.name === 'Baltic Sea')!.id; home.stance = 'patrol'; delete home.lane; syncWings(g);
    const strike = wingsOf(g, home.id).find((w) => w.kind === 'bmr')!;
    expect(planRaid(g, 'harbour', port, [strike.id], 'UK').title).toBe('Harbour raid: the harbour of the Danzig Corridor');
    expect(airTicket(g, 'harbour', port, [strike.id], 'UK', { ships: true }).ticket!.title).toBe('The raid on the harbour of the Danzig Corridor');
    // A harbour wrecked (built by hand: Britain's own harbour in the Highlands blown up, a corps at London to ship there).
    const w = startOpen(createGame(3, 'UK'));
    const c = addCorps(w, 'UK', 'Test corps', 'inf', 3, regionOf(w, 'London'), {});
    expect(wreckPrize(w, w.regions[regionOf(w, 'Highlands')], 'harbour', 5)).toBe(true);
    expect(whyNotShip(w, c, w.regions[regionOf(w, 'Highlands')])).toBe('The harbour of the Highlands is wrecked: no ship will use it for 5 months more.');
  });

  it('(h) the seven towns that are their province take the article; every other town reads as before', () => {
    const g = startOpen(createGame(3, 'GER'));
    const f = g.airFleets.find((x) => x.nat === 'GER' && x.bmr > 0 && x.ship === undefined)!;
    const order = (place: string): string => { f.mission = 'town'; f.target = regionOf(g, place); delete f.flew; return groupRow(g, f).doing; };
    for (const [place, town] of [['Ohio Valley', 'the Ohio Valley'], ['Dakotas', 'the Dakotas'], ['Azores', 'the Azores'], ['Canary Islands', 'the Canary Islands'], ['Pampas', 'the Pampas'], ['Valdai', 'the Valdai'], ['Mosquitia', 'the Mosquitia']]) {
      expect(order(place)).toBe(`Ordered against the defences of ${town}: flies when the month ends`);
    }
    expect(planRaid(g, 'town', regionOf(g, 'Ohio Valley'), undefined, 'GER').title).toBe('Raid on a town: the defences of the Ohio Valley');
    // A province on the list whose town has its own name, and one off the list: as before.
    expect(order('Highlands')).toBe('Ordered against the defences of Inverness: flies when the month ends');
    expect(order('London')).toBe('Ordered against the defences of London: flies when the month ends');
  });
});
