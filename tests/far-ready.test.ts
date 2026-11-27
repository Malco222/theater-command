// The readiness test with the far side's switch on (ruling 200(m), rider b586 to ruling 234; the Pacific planner's
// paper 14, section 6; src/campaign/readiness.ts on pacific.json 'ready'): the month Japan strikes where no book
// dates it. The states are a made-up 1941 set by hand, as in far-pacific-south.test.ts; one month of Japan's own is
// played to see the blow fall in the window's last month. No war is played here.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });

import { afterEach, describe, expect, it } from 'vitest';
import { STRENGTH, followUp, reckon, stands } from '../src/campaign/ashore';
import { bookMonth } from '../src/campaign/claims';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { atWar, bloc, capitulate, createGame, declareWar, relocate, startPhase } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { navalUpkeep } from '../src/campaign/navy';
import { PACIFIC, freezeAt, readyRules } from '../src/campaign/pacific';
import { readiness, readinessMonth, readyWatch } from '../src/campaign/readiness';
import { characterOf, monthly, ops, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

const corps = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.name === name && !c.dead);
const at = (g: Game, name: string): string | undefined => { const c = corps(g, name); return c && g.regions[c.reg].name; };
const news = (g: Game, turn: number, part: string): boolean => g.headlines.some((h) => h.turn === turn && h.text.includes(part));
const SOUTH = () => ops('JAP').find((op) => op.id === 'south')!;
const striking = (g: Game) => g.fleets.find((x) => x.nat === 'JAP' && x.name === 'Mobile Fleet')!;

/**
 * A made-up December 1941 (far-pacific-south.test.ts): the Netherlands and France struck down by hand, the months
 * read from June 1941, Japan's dated rows taken in, America's fleet put at Pearl Harbor. Where history is loose the
 * demand comes the thirteenth month after France fell (July 1941) and the freeze the month after it (August, 24).
 */
const december = (stop: number, human: string | null = null): Game => {
  const g = startOpen(createGame(17, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = 9;
  declareWar(g, 'GER', 'NED');
  capitulate(g, 'NED', 'GER');
  g.turn = 10;
  capitulate(g, 'FRA', 'GER');
  for (const t of [22, 23, 24, 25, 27]) { g.turn = t; bookMonth(g); monthlyDiplomacy(g); if (t === 24 || t === 27) startPhase(g, 'JAP'); }
  for (const op of ops('JAP')) if (op.id !== 'south') g.flags[`st:done:JAP:${op.id}`] = 1;
  const sea = g.seas.findIndex((z) => z.key === 'hawaii');
  for (const f of g.fleets.filter((x) => x.nat === 'USA' && (x.name === 'Pacific Fleet' || x.name === 'Pacific Carrier Fleet'))) {
    f.base = g.byName['Pearl Harbor'].id; f.port = 'Pearl Harbor'; f.home = sea; f.zone = sea; f.station = sea; f.stance = 'port';
  }
  g.turn = 28;
  bookMonth(g);
  monthlyDiplomacy(g);
  // (At stops 3 to 5 the strike reads who stands in Japan's way, not history's list: Units decide audit item 45, the
  // design director's ruling of 16:30Z, 25 November 2026. Here America stands beside Britain at war with Germany, so the
  // three are in its way whatever the staff's draw, and the plan is the whole of history's.)
  if (stop >= 3) { g.nations.USA.align = 'allies'; if (!atWar(g, 'USA', 'GER')) declareWar(g, 'USA', 'GER'); }
  navalUpkeep(g, 'USA');
  return g;
};
/** The same war with the freeze put so many months back, and the striking fleet gathered as the staff gathers it. */
const months = (g: Game, m: number, gathered = true): Game => {
  if (gathered) { g.flags['pac:freeze'] = g.turn - 2; readinessMonth(g, 'JAP'); }
  g.flags['pac:freeze'] = g.turn - m;
  return g;
};

afterEach(() => { readyWatch.tell = null; });

describe('the readiness test (ruling 200(m)): whose month it is', () => {
  it('has its figures: the third to the sixth month, one chance in two, the first month by character, the gathering two months after the freeze', () => {
    const R = PACIFIC!.ready!;
    expect(R.choice).toBe('south');
    expect(SOUTH().if).toEqual([`flag:${R.flag}`]);
    expect(SOUTH().sure).toBe(true);
    expect(R.window).toEqual([3, 6]);
    expect(R.chance).toBe(0.5);
    expect(R.first).toEqual({ bold: 3, cautious: 4, opportunist: 4 });
    expect(R.busy).toEqual({ first: 3, of: ['UK', 'SOV'], with: 'GER' });
    expect(R.gather.after).toBe(2);
    expect(R.gather.base).toBe('Etorofu');
    expect(Object.keys(R.leads)).toEqual(['Singapore', 'Manila']);
  });

  it('no freeze, no strike: the staff reads nothing, gathers nothing and raises no flag', () => {
    const g = december(3);
    expect(freezeAt(g)).toBe(24);
    delete g.flags['pac:freeze'];
    expect(readiness(g, 'JAP')).toBeNull();
    readinessMonth(g, 'JAP');
    expect(g.flags['pac:strike']).toBeUndefined();
    expect(g.regions[striking(g).base].name).toBe('Hiroshima');
  });

  it("where the book dates the strike the book's month stands, and a player's Japan is bound by none of it", () => {
    const book = december(1);
    expect(readyRules(book)).not.toBeNull();
    expect(readiness(book, 'JAP')).toBeNull();
    readinessMonth(book, 'JAP');
    expect(book.flags['pac:strike']).toBeUndefined();
    const his = december(3, 'JAP');
    his.flags['pac:freeze'] = his.turn - 6;
    expect(readiness(his, 'JAP')).toBeNull();
    readinessMonth(his, 'JAP');
    expect(his.flags['pac:strike']).toBeUndefined();
    // (And it is Japan's test alone.)
    expect(readiness(december(3), 'USA')).toBeNull();
  });
});

describe('the readiness test: the gathering and the three things', () => {
  it('the gathering waits on the army: with too few of the plan\'s corps on their quays nothing moves; the month they stand on them the carrier striking force goes to Etorofu, and no flag is raised before the window', () => {
    // (At stops 3 to 5 by cause, not by the calendar: Units decide audit item 44, the design director's 16:19Z and 16:30Z.)
    const g = december(3);
    let told = 0;
    readyWatch.tell = () => { told++; };
    const gone = SOUTH().landings!.map((l) => corps(g, l.corps)).filter((c): c is Corps => !!c).slice(0, 6);
    for (const c of gone) c.dead = true;
    for (const m of [1, 2]) {
      g.flags['pac:freeze'] = g.turn - m;
      readinessMonth(g, 'JAP');
      expect(g.regions[striking(g).base].name, `month ${m}`).toBe('Hiroshima');
    }
    for (const c of gone) c.dead = false;
    g.flags['pac:freeze'] = g.turn - 1;
    readinessMonth(g, 'JAP');
    expect(g.regions[striking(g).base].name).toBe('Etorofu');
    expect(g.seas[striking(g).home].key).toBe('kurile');
    expect(news(g, g.turn, 'The carrier striking force gathers at Hitokappu Bay in the Kuriles.')).toBe(true);
    expect(g.flags['pac:strike']).toBeUndefined();
    expect(told).toBe(0);
  });

  it('ready: the fleet in reach of Pearl Harbor, the plan\'s corps on their quays, and the test of strength at Kota Bharu and Lingayen', () => {
    const g = months(december(3), 4);
    const now = readiness(g, 'JAP')!;
    expect(now.month).toBe(4);
    expect(now.fleet).toBe(true);
    // (The record's lift of eleven carries all ten of the plan's landings, rider b630 to ruling 234, so all ten are
    // asked and all ten are on their quays. The 38th Division's row of 1939 is not called in this made-up war.)
    expect(now.asked).toBe(10);
    expect(now.forces).toBeGreaterThanOrEqual(10);
    expect(Object.keys(now.strength)).toEqual(['Singapore', 'Manila']);
    expect(now.strength.Singapore).not.toBeNull();
    expect(now.strength.Manila).not.toBeNull();
    expect(now.ready).toBe(now.strength.Singapore === true && now.strength.Manila === true);
  });

  it("the wave at a beach is every row that sails for it: a division's tank regiments are weighed with it while they stand with it on its quay (rider b631 (i))", () => {
    const g = months(december(3), 4);
    const now = readiness(g, 'JAP')!;
    // The 48th Division's lift for Lingayen is five rows (its two tank regiments, and from the design director's
    // rulings of 22:34Z and 22:42Z of 21 November 2026 the 16th Division's 9th Infantry and the 14th Army's heavy
    // guns), and the two divisions for Kota Bharu sail with the 5th's regiments.
    const NINTH = '16th Division (9th Infantry)', HEAVY = '14th Army artillery (1st and 8th Field Heavy Artillery)';
    expect(now.waves.Manila).toEqual(['14th Army (48th Division)', '4th Tank Regiment', '7th Tank Regiment', NINTH, HEAVY]);
    expect(now.waves.Singapore).toEqual(['25th Army (5th Division)', '1st Tank Regiment', '6th Tank Regiment', '25th Army (18th Division)']);
    // The five rows weigh more at the beach than the division alone does: the test reads what the ships put ashore.
    const beach = g.byName['Lingayen'], also = new Set(['USA', 'PHI', 'UK', 'AUS', 'NED']);
    const wave = now.waves.Manila.map((nm) => corps(g, nm)!);
    const alone = reckon(g, 'JAP', beach, [wave[0]], 0, undefined, also)!, whole = reckon(g, 'JAP', beach, wave, 0, undefined, also)!;
    expect(alone.theirs).toBeGreaterThan(0);
    expect(whole.ours).toBeGreaterThan(alone.ours);
    expect(whole.chance).toBeGreaterThan(alone.chance);
    // A regiment that is not on the division's quay does not sail with it, and one let go is its staff's own.
    relocate(g, corps(g, '4th Tank Regiment')!, g.byName['Tokyo']);
    expect(readiness(g, 'JAP')!.waves.Manila).toEqual(['14th Army (48th Division)', '7th Tank Regiment', NINTH, HEAVY]);
    corps(g, '7th Tank Regiment')!.hold = false;
    expect(readiness(g, 'JAP')!.waves.Manila).toEqual(['14th Army (48th Division)', NINTH, HEAVY]);
    for (const nm of [NINTH, HEAVY]) corps(g, nm)!.hold = false;
    expect(readiness(g, 'JAP')!.waves.Manila).toEqual(['14th Army (48th Division)']);
    // A place whose beaches are all Japan's has no wave: the test is not asked there.
    g.byName['Manila'].ctrl = 'JAP';
    expect(readiness(g, 'JAP')!.waves.Manila).toBeUndefined();
  });

  it("one ground, both sides: the plan's rows for its other beaches count in ours where theirs is counted from (rider b642)", () => {
    const g = months(december(3), 4);
    const now = readiness(g, 'JAP')!;
    const tanaka = 'Tanaka and Kanno Detachments (2nd Formosa Infantry, of the 48th Division)';
    // Luzon's other landings lie on the ground Lingayen's defenders are counted from: Aparri's and Legaspi's rows
    // within the three marches, Davao's beyond them on the same enemy ground. Malaya's test has no other beach.
    // (The 16th Division sails for Legaspi with its Kimura Detachment in its lift: the director's 22:34Z of 21 November 2026.)
    expect(now.beside.Manila).toEqual([tanaka, '14th Army (16th Division)', 'Kimura Detachment (16th Division)', 'Sakaguchi Detachment (56th Mixed Infantry Group)']);
    expect(now.beside.Singapore).toEqual([]);
    const beach = g.byName['Lingayen'], also = new Set(['USA', 'PHI', 'UK', 'AUS', 'NED']);
    const wave = now.waves.Manila.map((nm) => corps(g, nm)!), sixteenth = corps(g, '14th Army (16th Division)')!;
    const weigh = (c: Corps, at: string) => reckon(g, 'JAP', beach, wave, 0, undefined, also, [{ c, at: g.byName[at] }])!;
    const alone = reckon(g, 'JAP', beach, wave, 0, undefined, also)!;
    expect(alone.beside).toBe(0);
    // Within the marches a row counts in full, as it would defend its own beach. Theirs does not move.
    const near = weigh(sixteenth, 'Legaspi');
    expect(near.rows).toEqual(['14th Army (16th Division)']);
    expect(near.beside).toBeCloseTo(stands(g, sixteenth, g.byName['Legaspi']), 9);
    expect(near.ours).toBeCloseTo(alone.ours + near.beside, 9);
    expect(near.theirs).toBe(alone.theirs);
    expect(near.chance).toBeGreaterThan(alone.chance);
    // Beyond the marches, on his ground in one piece with it, at the share his own reserves count for.
    expect(weigh(sixteenth, 'Davao').beside).toBeCloseTo(STRENGTH!.reserve * stands(g, sixteenth, g.byName['Davao']), 9);
    // On other ground it counts for nothing, and a row of the wave is not counted a second time.
    expect(weigh(sixteenth, 'Kota Bharu').beside).toBe(0);
    expect(weigh(wave[0], 'Aparri').rows).toEqual([]);
    // Nor is a row counted beside the wave counted again among those the ships bring in after it.
    const quay = g.regions[wave[0].reg], detachment = corps(g, tanaka)!;
    detachment.str = 1;
    // (Read with the division and its two tank regiments as the wave: the 48th's whole lift of five fills the beach,
    // and the ships bring nobody in behind a beach that is full.)
    const three = wave.slice(0, 3);
    expect(followUp(g, 'JAP', quay, three, beach, undefined, [detachment])).toBeLessThan(followUp(g, 'JAP', quay, three, beach));
    expect(followUp(g, 'JAP', quay, wave, beach)).toBe(0);
    // A row that stands on no quay is not weighed.
    relocate(g, sixteenth, g.regions.find((r) => r.owner === 'JAP' && r.ctrl === 'JAP' && !r.port)!);
    expect(readiness(g, 'JAP')!.beside.Manila).not.toContain('14th Army (16th Division)');
  });

  it('not ready with the fleet unfit or out of reach, or with fewer corps on the quays than the lift carries', () => {
    const far = months(december(3), 4, false);
    expect(readiness(far, 'JAP')!.fleet).toBe(false);
    const unfit = months(december(3), 4);
    striking(unfit).ready = 0.5;
    const a = readiness(unfit, 'JAP')!;
    expect(a.fleet).toBe(false);
    expect(a.ready).toBe(false);
    const few = months(december(3), 4);
    let left = readiness(few, 'JAP')!.forces;
    for (const l of SOUTH().landings!) { const c = corps(few, l.corps); if (c && left > 5) { c.dead = true; left--; } }
    const b = readiness(few, 'JAP')!;
    expect(b.forces).toBe(5);
    expect(b.ready).toBe(false);
  });

  it('the test of strength counts the countries the choice declares on though they are at peace, and is not asked for a place that is already Japan\'s', () => {
    const g = months(december(3), 4);
    const beach = g.byName['Kota Bharu'], wave = [corps(g, '25th Army (5th Division)')!];
    expect(atWar(g, 'JAP', 'UK')).toBe(false);
    expect(reckon(g, 'JAP', beach, wave)!.theirs).toBe(0);
    const K = reckon(g, 'JAP', beach, wave, 0, undefined, new Set(['UK', 'AUS']))!;
    expect(K.theirs).toBeGreaterThan(0);
    expect(K.chance).toBeLessThan(1);
    // A garrison no staff would land against: the test fails there, and the staff is not ready.
    for (const c of g.corps) if (c.nat === 'UK' && !c.dead && ['Kota Bharu', 'Kuantan', 'Ipoh', 'Penang', 'Kuala Lumpur', 'Johore', 'Singapore'].includes(g.regions[c.reg].name)) { c.divs = 9; c.str = 1; }
    const hard = readiness(g, 'JAP')!;
    expect(hard.strength.Singapore).toBe(false);
    expect(hard.ready).toBe(false);
    g.byName['Singapore'].ctrl = 'JAP';
    expect(readiness(g, 'JAP')!.strength.Singapore).toBeNull();
  });
});

describe('the readiness test: the first month, the draw and the last month', () => {
  it("the first month is the staff's character: a bold staff the third, a cautious one the fourth, an opportunist the fourth, or the third while Britain or the Soviet Union fights Germany on land", () => {
    const g = months(december(3), 3);
    const seen = new Set<string>();
    for (let seed = 1; seed <= 200 && seen.size < 3; seed++) {
      g.seed = seed;
      const who = characterOf(g, 'JAP');
      if (seen.has(who)) continue;
      seen.add(who);
      g.nations.UK.lastFight.GER = -99; g.nations.GER.lastFight.UK = -99; g.nations.SOV.lastFight.GER = -99; g.nations.GER.lastFight.SOV = -99;
      expect(readiness(g, 'JAP')!.first, who).toBe(who === 'bold' ? 3 : 4);
      g.nations.SOV.lastFight.GER = g.turn - 2;
      expect(readiness(g, 'JAP')!.first, who).toBe(who === 'cautious' ? 4 : 3);
      g.nations.SOV.lastFight.GER = g.turn - 3;
      expect(readiness(g, 'JAP')!.first, who).toBe(who === 'bold' ? 3 : 4);
    }
    expect(seen.size).toBe(3);
  });

  it('before its first month the staff does not strike; from it, ready, it strikes on the draw of the seed, one chance in two', () => {
    const g = months(december(3), 3);
    const whys: string[] = [];
    readyWatch.tell = (e) => { whys.push(e.why); };
    let both = 0;
    for (let seed = 1; seed <= 60 && both < 7; seed++) {
      g.seed = seed;
      delete g.flags['pac:strike'];
      const now = readiness(g, 'JAP')!;
      if (!now.ready) continue;
      readinessMonth(g, 'JAP');
      if (now.first > 3) { expect(g.flags['pac:strike'], `seed ${seed}`).toBeUndefined(); expect(whys[whys.length - 1]).toBe('before its first month'); both |= 1; continue; }
      const draw = monthly(g, 'JAP', 'south', 'ready', g.turn) < 0.5;
      expect(g.flags['pac:strike'], `seed ${seed}`).toBe(draw ? g.turn : undefined);
      expect(whys[whys.length - 1]).toBe(draw ? 'ready, and the draw for it' : 'ready, and the draw against it');
      both |= draw ? 2 : 4;
    }
    // All three were seen: a staff before its first month, a draw for the strike and a draw against it.
    expect(both).toBe(7);
  });

  it('not ready, it waits; in the sixth month it strikes, ready or not', () => {
    const g = months(december(3), 5);
    const told: { why: string; struck: boolean; ready: boolean }[] = [];
    readyWatch.tell = (e) => { told.push({ why: e.why, struck: e.struck, ready: e.ready }); };
    striking(g).ready = 0.5;
    readinessMonth(g, 'JAP');
    expect(g.flags['pac:strike']).toBeUndefined();
    expect(told[0]).toEqual({ why: 'not ready', struck: false, ready: false });
    g.flags['pac:freeze'] = g.turn - 6;
    readinessMonth(g, 'JAP');
    expect(g.flags['pac:strike']).toBe(g.turn);
    expect(told[1]).toEqual({ why: 'the last month of the window', struck: true, ready: false });
    // Once raised, the test is over.
    expect(readiness(g, 'JAP')).toBeNull();
  });

  it("the flag up, the strike south goes that month with no second draw: Japan declares on the three, the carriers strike Pearl Harbor from Etorofu, the armies land, and Siam lets them pass", () => {
    const g = months(december(3), 6);
    g.phase = g.order.indexOf('JAP');
    runTurnAI(g);
    expect(g.turn).toBe(29);
    expect(g.flags['pac:strike']).toBe(28);
    expect(g.flags['st:done:JAP:south']).toBe(28);
    expect(atWar(g, 'JAP', 'USA') && atWar(g, 'JAP', 'UK') && atWar(g, 'JAP', 'AUS')).toBe(true);
    expect(news(g, 28, 'raid Pearl Harbor without warning')).toBe(true);
    expect(g.regions[striking(g).base].name).toBe('Hiroshima');
    // (Siam stands with Japan the day of the blow at stops 1 to 3, riders b591 and b593 to ruling 234: Singora is the
    // 5th Division's harbour, as the plan's row has it first, and Kota Bharu the 18th's.)
    expect(bloc(g.nations.THA)).toBe('axis');
    expect(at(g, '25th Army (5th Division)')).toBe('Singora');
    const sailed = SOUTH().landings!.filter((l) => l.to.includes(at(g, l.corps) ?? '')).length;
    expect(sailed).toBeGreaterThanOrEqual(5);
  });
});
