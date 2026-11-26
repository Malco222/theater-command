// A raid on troops, in words (release 2, lane (a)'s switch: flyKinds(['ground']) / TC_FLY=ground / TC.airKinds).
// The design director, 25 November 2026 14:59Z, accepted the card's "more likely than not" list of strafers, the fire
// and the weight staying the exact expectation, and asked the UI director "for words that say the list is the likely
// one, so a strafer who never arrives isn't a broken promise". Her words of 15:04Z, to the letter, no colour on any:
//   the card: LIKELY TO GO DOWN AND STRAFE; under it, once, muted, "The fighter groups most likely to go down and strafe.
//   Which of them do is settled over the target." (as she mended it at 19:17Z: it lost "if the sky is won"); the groups a
//   row each; with none, the line for none of its cause (her marks of 19:10Z, section A, in the order she confirmed at
//   19:17Z: no fighters; else no reach; else the odds; else the fought-out line; muted, the muted line left out above it);
//   the report: "{No. 11 Group} went down and strafed." / "No fighter group went down to strafe."
// The card's list is the ids strafersExpected gives the card's own forecast (the same call); the report's names are the
// groups that really went down, the settled raid's (fly()) or the fought raid's (the battle's). With the switch off
// nothing is shown and nothing is saved (src/campaign/strafers.ts).
//
// The raid is lane (a)'s own (tests/air-troops-forecast-r2.test.ts): France on the German army in the Saar, September
// 1939. Groupement de Chasse 3 flies from Lorraine, within its reach to strafe; Groupement de Chasse 1 is moved to Namur,
// inside its fighter range but beyond its reach to strafe; Groupement de Bombardement 1 flies from Lorraine.
import { vi } from 'vitest';
vi.hoisted(() => { delete (globalThis as { TC_FLY?: string }).TC_FLY; });

import { afterEach, describe, expect, it } from 'vitest';
import { launchRaid, planRaid, raidTargets } from '../src/campaign/airwar';
import { AIR, assessAir, forecastAir, strafeCarry, strafersExpected } from '../src/campaign/air';
import { airPage } from '../src/campaign/airpages';
import { afterCard } from '../src/campaign/airfought';
import { createGame, setWar } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { STRAFE_WORDS, resultFor, strafeOn, strafeWatch, strafedLine } from '../src/campaign/strafers';
import type { Game } from '../src/campaign/types';
import { AirSim, airBattle } from '../src/battle/air/sim';
import { airTicket, flyKinds, foughtAir } from '../src/battle/air/ticket';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;

type Told = { route: 'card' | 'settled' | 'fought'; target: number; nat: string; strafers: number[]; weight?: number; cut?: number };
let heard: Told[] = [];
const WAS = AIR.combat.fighters;
afterEach(() => { flyKinds([]); strafeWatch.tell = undefined; heard = []; AIR.combat.fighters = WAS; });

const HEADING = 'LIKELY TO GO DOWN AND STRAFE';
const NOTE = 'The fighter groups most likely to go down and strafe. Which of them do is settled over the target.';
// The line for none, by its cause (the UI director, 19:10Z section A; the order, 19:17Z).
const NONE = {
  alone: 'No fighters fly with this raid.',
  reach: 'No fighter group has the range to go down and strafe there.',
  odds: 'No fighter group is likely to go down and strafe: the defenders\' fighters are stronger than yours. One you send down before it meets them still may.',
  fought: 'No fighter group is likely to go down and strafe: the air fight is likely to leave them with no fight left. One you send down before it meets the defence still may.',
};
const NOBODY = 'No fighter group went down to strafe.';
// A group that went down with under one fighter's weight on the troops (the UI director, 19:10Z A5; its forms, 19:17Z).
const LITTLE = 'went down to strafe, but had too little fight left to do any harm.';
const FOUND_LITTLE = 'The aircraft found little to hit.';
const GC1 = 9, GC3 = 10, GC2 = 36, BOMBERS = 38;
const NAME: Record<number, string> = { [GC1]: 'Groupement de Chasse 1', [GC2]: 'Groupement de Chasse 2', [GC3]: 'Groupement de Chasse 3' };

/** The Saar raid, k-th luck. 'both': the bombers, GC3 in reach and GC1 at Namur, out of it; 'far': the bombers and GC1 at Namur; 'alone': the bombers alone; 'three': GC1, GC2 and GC3 all at Lorraine with the bombers. */
function saar(k: number, groups: 'both' | 'far' | 'alone' | 'three' = 'both') {
  const g = startOpen(createGame(7, 'FRA'));
  setWar(g, 'FRA', 'GER');
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
  const at = (id: number, n: string): void => { g.airFleets.find((f) => f.id === id)!.base = g.regions.find((r) => r.name === n)!.id; };
  if (groups === 'three') { at(GC1, 'Lorraine'); at(GC2, 'Lorraine'); } else at(GC1, 'Namur');
  const ids = groups === 'both' ? [BOMBERS, GC3, GC1] : groups === 'far' ? [BOMBERS, GC1] : groups === 'alone' ? [BOMBERS] : [BOMBERS, GC1, GC2, GC3];
  return { g, target, ids };
}
const on = (): void => { flyKinds(['ground']); strafeWatch.tell = (x) => { heard.push(JSON.parse(JSON.stringify(x))); }; };
const names = (ids: number[]): string[] => ids.map((id) => NAME[id]);
function fight(s: AirSim): void {
  s.ai.att = true; s.ai.def = true;
  let guard = 0;
  while (!s.over && guard++ < 20_000) s.step();
  expect(s.over).toBeTruthy();
}

describe('the raid card: LIKELY TO GO DOWN AND STRAFE', () => {
  it('to the letter, with a list: the heading, the muted line once, the groups a row each, on the browser\'s plan, the air page and the 3D card', () => {
    on();
    const c = saar(0);
    const p = planRaid(c.g, 'ground', c.target, c.ids, 'FRA');
    expect(p.why).toBeNull();
    expect(p.strafe).toEqual({ heading: HEADING, note: NOTE, names: ['Groupement de Chasse 3'], none: '' });
    expect(STRAFE_WORDS).toEqual({ heading: HEADING, note: NOTE, none: NONE, nobody: NOBODY, little: LITTLE });
    // The air page (the rows the rules build): at the foot of the raid's forces, right under FLYING THE RAID; the muted
    // line is the section's text, each group a row with no nation (no flag, no picture, no colour).
    const page = airPage(c.g, `plan:ground:${c.target}:${p.picked.join('.')}`)!;
    const at = page.sections.findIndex((s) => s.heading === HEADING);
    expect(page.sections[at - 1].heading).toBe('FLYING THE RAID');
    expect(page.sections[at]).toEqual({ heading: HEADING, text: NOTE, rows: [{ nat: '', title: 'Groupement de Chasse 3', lines: [], buttons: [] }] });
    expect(page.sections.filter((s) => s.heading === HEADING).length).toBe(1);
    // The 3D card, through the bridge as the client asks for it. (Changed on the sky gate, the design director 16:04Z,
    // and back on her ruling of 18:14Z: loaded through the bridge the month's other moves are made, and 1. Jagddivision,
    // about 151 fighters able to meet it, faces Groupement de Chasse 3 and Groupement de Chasse 1. At 16:04Z that was
    // short of the 1.65 to 1 that won the sky, and the section was the line for none; read now on the fight the group
    // still carries after its air fight, 1 - D / E of the starting weights, it carries some, so it is named, the muted
    // line once above it, and the settled raid below sends it down.)
    TC.load(saveGame(c.g));
    const card = TC.page(`plan:ground:${c.target}:${p.picked.join('.')}`).card;
    expect(card.strafe).toEqual({ heading: HEADING, note: NOTE, names: ['Groupement de Chasse 3'], none: '' });
    // With every German fighter grounded before the load (none can rise: the sky is won) the 3D card names the group,
    // the muted line once above it: round four's list, still read through the bridge.
    const w = saar(0);
    for (const f of w.g.airFleets) if (f.nat === 'GER') f.ftr = 0;
    TC.load(saveGame(w.g));
    expect(TC.page(`plan:ground:${w.target}:${p.picked.join('.')}`).card.strafe).toEqual({ heading: HEADING, note: NOTE, names: ['Groupement de Chasse 3'], none: '' });
    // Three groups in reach: each a row, in the raid's order.
    const t = saar(0, 'three');
    const q = planRaid(t.g, 'ground', t.target, t.ids, 'FRA');
    expect(q.strafe!.names.length).toBe(3);
    expect([...q.strafe!.names].sort()).toEqual(names([GC1, GC2, GC3]));
  });

  it('with none: the line for none of its cause under the heading, in place of the muted line and the list, on the browser\'s plan, the air page and the 3D card', () => {
    on();
    // Bombers alone: no fighters; Groupement de Chasse 1 at Namur alone: no reach; the escort against 2. Jagddivision at
    // 800 (setup): the odds; the combat rate x60 (setup): the air fight is likely to leave none a fighter.
    // (The 3D card is read through the bridge, where the month's other moves are made on loading: Groupement de Chasse 1
    // goes back to Lille, out of range, so the raid there is the bombers alone, and its card says so; and 2. Jagddivision
    // is the month's again, so the third raid's 3D card is not read here.)
    const cases: [ReturnType<typeof saar>, keyof typeof NONE, keyof typeof NONE | null][] = [];
    cases.push([saar(0, 'alone'), 'alone', 'alone']);
    cases.push([saar(0, 'far'), 'reach', 'alone']);
    const many = saar(0);
    const jd = many.g.airFleets.find((f) => f.id === 3)!;
    jd.ftr = 800; jd.est.ftr = Math.max(jd.est.ftr, 800);
    cases.push([many, 'odds', null]);
    for (const [c, why, why3d] of cases) {
      const p = planRaid(c.g, 'ground', c.target, c.ids, 'FRA');
      expect(p.why).toBeNull();
      expect(p.strafe).toEqual({ heading: HEADING, note: '', names: [], none: NONE[why] });
      const page = airPage(c.g, `plan:ground:${c.target}:${p.picked.join('.')}`)!;
      expect(page.sections.find((s) => s.heading === HEADING)).toEqual({ heading: HEADING, text: NONE[why], rows: [] });
      if (!why3d) continue;
      TC.load(saveGame(c.g));
      expect(TC.page(`plan:ground:${c.target}:${p.picked.join('.')}`).card.strafe).toEqual({ heading: HEADING, note: '', names: [], none: NONE[why3d] });
    }
    // The odds: the fight each group carries is 0 (the defenders' weight above the raid's fighters').
    expect(strafeCarry(assessAir(many.g, many.g.airFleets.filter((f) => many.ids.includes(f.id)), 'ground', many.target))).toBe(0);
    // The fought-out line: Groupement de Chasse 3 has the reach and carries some of its fight, but the air fight the
    // card expects is likely to leave it no fighter.
    AIR.combat.fighters = WAS * 60;
    const hard = saar(0);
    const p = planRaid(hard.g, 'ground', hard.target, hard.ids, 'FRA');
    expect(strafeCarry(assessAir(hard.g, hard.g.airFleets.filter((f) => hard.ids.includes(f.id)), 'ground', hard.target))).toBeGreaterThan(0);
    expect(p.strafe).toEqual({ heading: HEADING, note: '', names: [], none: NONE.fought });
  });

  it('absent with the switch off, on any other kind of raid, and on a raid that is not the player\'s own', () => {
    flyKinds([]);
    expect(strafeOn()).toBe(false);
    const c = saar(0);
    const p = planRaid(c.g, 'ground', c.target, c.ids, 'FRA');
    expect(p.strafe).toBeUndefined();
    expect('strafe' in p).toBe(false);
    const page = airPage(c.g, `plan:ground:${c.target}:${p.picked.join('.')}`)!;
    expect(page.sections.some((s) => s.heading === HEADING)).toBe(false);
    TC.load(saveGame(c.g));
    const card = TC.page(`plan:ground:${c.target}:${p.picked.join('.')}`).card;
    expect(card.why).toBe('');
    expect('strafe' in card).toBe(false);
    // On: a bombing raid carries none.
    on();
    const b = saar(0);
    const t = raidTargets(b.g, 'bomb', 'FRA')[0];
    expect(planRaid(b.g, 'bomb', t.id, undefined, 'FRA').strafe).toBeUndefined();
    // On: a German raid on French troops in Alsace (2. Jagddivision moved to the Saar, with Kampfgeschwader 1) is weighed
    // with its strafers, and its forecast names one, but no card of it carries the section: the player's sight is his own.
    const e = saar(0);
    const foe = raidTargets(e.g, 'ground', 'GER').find((x) => e.g.regions[x.id].name === 'Alsace')!;
    e.g.airFleets.find((f) => f.id === 3)!.base = e.g.regions.find((r) => r.name === 'Saar')!.id;
    heard = [];
    const ep = planRaid(e.g, 'ground', foe.id, [31, 3], 'GER');
    expect(ep.why).toBeNull();
    expect(heard.filter((x) => x.route === 'card' && x.nat === 'GER').map((x) => x.strafers)).toEqual([[3]]);
    expect(ep.strafe).toBeUndefined();
  });

  it('the card\'s list is exactly the ids strafersExpected gave the card\'s own forecast, on a spread of raids and air fights', () => {
    on();
    let named = 0, none = 0;
    for (const mult of [1, 8, 26, 28, 30, 60]) {
      for (const groups of ['both', 'far', 'alone', 'three'] as const) {
        AIR.combat.fighters = WAS * mult;
        const c = saar(0, groups);
        heard = [];
        const p = planRaid(c.g, 'ground', c.target, c.ids, 'FRA');
        expect(p.why).toBeNull();
        const card = heard.filter((x) => x.route === 'card');
        expect(card.length).toBe(1);
        // The same call's ids, named as the card names its groups (the seats' names).
        expect(p.strafe!.names).toEqual(card[0].strafers.map((id) => p.seats.find((s) => s.id === id)!.name));
        // And strafersExpected itself, given the air fight the card expects, says the same.
        const a = assessAir(c.g, c.g.airFleets.filter((f) => c.ids.includes(f.id)), 'ground', c.target);
        const fc = forecastAir(c.g, a, 'ground', c.target);
        expect(strafersExpected(c.g, a, fc.ftrLoss - fc.fire!).ids).toEqual(card[0].strafers);
        // Naming none, the line of the first cause that is true (19:17Z): bombers alone; no reach (Groupement de Chasse 1
        // at Namur alone); the odds (no carry); else the air fight.
        const why = groups === 'alone' ? 'alone' : groups === 'far' ? 'reach' : strafeCarry(a) > 0 ? 'fought' : 'odds';
        expect(p.strafe!.none).toBe(card[0].strafers.length ? '' : NONE[why]);
        expect(p.strafe!.note).toBe(card[0].strafers.length ? NOTE : '');
        if (card[0].strafers.length) named++; else none++;
      }
    }
    expect(named).toBeGreaterThan(5);
    expect(none).toBeGreaterThan(5);
  });
});

describe('the raid report: who went down', () => {
  it('the settled raid: the report\'s line is fly()\'s strafers on 50 lucks, at the edge where the card says likely and the dice differ', () => {
    on();
    // A heavier air fight than the game's (the combat rate x28): the card names Groupement de Chasse 3 as likely, and
    // over 50 lucks it is sometimes shot out of the sky before it can go down.
    AIR.combat.fighters = WAS * 28;
    const c0 = saar(0);
    expect(planRaid(c0.g, 'ground', c0.target, c0.ids, 'FRA').strafe!.names).toEqual(['Groupement de Chasse 3']);
    const seen = new Map<string, number>();
    for (let k = 0; k < 50; k++) {
      const c = saar(k);
      heard = [];
      const rep = launchRaid(c.g, 'ground', c.target, c.ids, 'FRA')!;
      const settled = heard.filter((x) => x.route === 'settled');
      expect(settled.length).toBe(1);
      expect(rep.strafed).toEqual(names(settled[0].strafers));
      const line = strafedLine(rep, 'FRA');
      // (A5, the UI director 19:10Z and 19:17Z: where the air fight left it under one fighter's weight on the troops, its
      // line says it had too little fight left to do any harm.)
      const w = settled[0].weight!;
      expect(line).toBe(!settled[0].strafers.length ? NOBODY : w < 1 ? `Groupement de Chasse 3 ${LITTLE}` : 'Groupement de Chasse 3 went down and strafed.');
      seen.set(line, (seen.get(line) ?? 0) + 1);
      // The report kept it: read back from the save.
      expect(loadGame(saveGame(c.g))!.airReports.at(-1)!.strafed).toEqual(rep.strafed);
      // The defender reading his own troops' raid: her words do not cover it, so nothing.
      expect(strafedLine(rep, 'GER')).toBe('');
    }
    expect(seen.get('Groupement de Chasse 3 went down and strafed.')).toBeGreaterThan(0);
    expect(seen.get(NOBODY)).toBeGreaterThan(0);
  });

  it('several groups are joined as the report joins names; none with the reach: nobody went down', () => {
    on();
    const two = saar(0);
    two.g.airFleets.find((f) => f.id === GC1)!.base = two.g.regions.find((r) => r.name === 'Lorraine')!.id;
    const r2 = launchRaid(two.g, 'ground', two.target, two.ids, 'FRA')!;
    expect(strafedLine(r2, 'FRA')).toBe(`${r2.strafed![0]} and ${r2.strafed![1]} went down and strafed.`);
    expect([...r2.strafed!].sort()).toEqual(names([GC1, GC3]));
    const three = saar(0, 'three');
    const r3 = launchRaid(three.g, 'ground', three.target, three.ids, 'FRA')!;
    expect(r3.strafed!.length).toBe(3);
    expect(strafedLine(r3, 'FRA')).toBe(`${r3.strafed![0]}, ${r3.strafed![1]} and ${r3.strafed![2]} went down and strafed.`);
    for (const groups of ['far', 'alone'] as const) {
      const c = saar(0, groups);
      const rep = launchRaid(c.g, 'ground', c.target, c.ids, 'FRA')!;
      expect(rep.strafed).toEqual([]);
      expect(strafedLine(rep, 'FRA')).toBe(NOBODY);
    }
  });

  it('the fought raid: the line names the battle\'s strafers, on the after card, to the raider only', () => {
    for (const k of [0, 1]) {
      on();
      const f = saar(k);
      const sim = airBattle(airTicket(f.g, 'ground', f.target, f.ids, 'FRA').ticket!) as AirSim;
      fight(sim);
      const o = sim.outcome();
      heard = [];
      const fought = foughtAir(o);
      const rep = launchRaid(f.g, 'ground', f.target, f.ids, 'FRA', fought)!;
      const told = heard.filter((x) => x.route === 'fought');
      expect(told.length).toBe(1);
      expect([...rep.strafed!].sort()).toEqual(names([...(o.strafers ?? [])]).sort());
      const line = rep.strafed!.length ? `${rep.strafed!.join(' and ')} went down and strafed.` : NOBODY;
      expect(strafedLine(rep, 'FRA')).toBe(line);
      // The after card: after what the bombers did and before the bill, to the side that flew it.
      const card = afterCard(f.g, rep, fought);
      expect(card.lines.filter((x) => x === line).length).toBe(1);
      expect(card.lines.indexOf(line)).toBe(1 + (fought.staff ? 1 : 0));
      // The defender reading it: nothing new.
      const def = loadGame(saveGame(f.g))!;
      def.human = 'GER';
      expect(afterCard(def, rep, fought).lines.includes(line)).toBe(false);
      // Switched off, the same battle's card says nothing of it (no report field).
      const bare = { ...rep };
      delete bare.strafed;
      expect(afterCard(f.g, bare, fought).lines.some((x) => x.includes('strafe'))).toBe(false);
    }
  });

  it('through the bridge: the 3D act\'s note carries the line whole after the report\'s own words; off, it is as before', () => {
    on();
    const c = saar(0);
    TC.load(saveGame(c.g));
    const r = TC.act(`raid:ground:${c.target}:${c.ids.join('.')}`);
    expect(r.ok).toBe(true);
    // (Changed on the sky gate, 16:04Z, and back on the ruling of 18:14Z: the group carries some of its fight there, see
    // the card above, so the report says it went down.)
    expect(r.note.endsWith(' Groupement de Chasse 3 went down and strafed.')).toBe(true);
    flyKinds([]);
    TC.load(saveGame(saar(0).g));
    const off = TC.act(`raid:ground:${c.target}:${c.ids.join('.')}`);
    expect(off.ok).toBe(true);
    expect(off.note.includes('strafe')).toBe(false);
  });

  it('nothing shown and nothing saved with the switch off; an older report without the field says nothing', () => {
    flyKinds([]);
    const c = saar(3);
    const rep = launchRaid(c.g, 'ground', c.target, c.ids, 'FRA')!;
    expect('strafed' in rep).toBe(false);
    expect(strafedLine(rep, 'FRA')).toBe('');
    expect(saveGame(c.g).includes('"strafed"')).toBe(false);
    // A report from an older save, read with the switch on: no field, no line.
    on();
    expect(strafedLine(loadGame(saveGame(c.g))!.airReports.at(-1)!, 'FRA')).toBe('');
  });
});

// The UI director, 19:10Z A5 and 19:17Z (her readings 3 and 4): "{A} went down and strafed. {B} went down to strafe, but
// had too little fight left to do any harm."; two "{A} and {B}", three or more "{A}, {B} and {C}"; a group whose harm on
// the troops is under one fighter's weight (the game's own unit, the design director 19:09Z item 3); "The aircraft found
// little to hit." where the raid as a whole did no harm worth a figure; where the only aircraft over the troops were such
// groups, their line alone. The Saar with all three French fighter groups at Lorraine against 2. Jagddivision at a
// strength the setup sets, near the edge where each group carries almost none of its fight.
describe('the raid report: too little fight left to do any harm (A5)', () => {
  function edge(n: number, bombers = true, k = 0) {
    const c = saar(k, 'three');
    const d = c.g.airFleets.find((f) => f.id === 3)!;
    d.ftr = n; d.est.ftr = Math.max(d.est.ftr, n);
    return { ...c, ids: bombers ? c.ids : c.ids.filter((id) => id !== BOMBERS) };
  }

  it('the forms, each sentence opening with a capital', () => {
    const rep = (strafed: string[], little: string[]) => ({ mission: 'ground', attNat: 'FRA', strafed, ...(little.length ? { strafeLittle: { names: little } } : {}) });
    expect(strafedLine(rep(['the 3rd'], ['the 3rd']), 'FRA')).toBe(`The 3rd ${LITTLE}`);
    expect(strafedLine(rep(['A', 'B'], ['B']), 'FRA')).toBe(`A went down and strafed. B ${LITTLE}`);
    expect(strafedLine(rep(['A', 'B', 'C'], ['A', 'B']), 'FRA')).toBe(`C went down and strafed. A and B ${LITTLE}`);
    expect(strafedLine(rep(['A', 'B', 'C'], ['A', 'B', 'C']), 'FRA')).toBe(`A, B and C ${LITTLE}`);
    expect(strafedLine(rep(['the 3rd', 'B'], []), 'FRA')).toBe('The 3rd and B went down and strafed.');
    expect(strafedLine(rep(['A'], ['A']), 'GER')).toBe('');
  });

  it('the settled raid: who had under one fighter\'s weight on the troops, in the report\'s words and kept in the save; the result sentence for no harm', () => {
    on();
    // 646: Groupement de Chasse 3 with a fighter's weight or more, the other two with less.
    const a = edge(646);
    heard = [];
    const r1 = launchRaid(a.g, 'ground', a.target, a.ids, 'FRA')!;
    const settled = heard.find((x) => x.route === 'settled')!;
    expect(r1.strafed!.length).toBe(3);
    expect(r1.strafeLittle).toEqual({ names: names([GC1, GC2]) });
    expect(strafedLine(r1, 'FRA')).toBe(`Groupement de Chasse 3 went down and strafed. Groupement de Chasse 1 and Groupement de Chasse 2 ${LITTLE}`);
    expect(Math.round(settled.cut! * 100)).toBe(0);
    expect(r1.result).toBe(FOUND_LITTLE);
    expect(resultFor(r1, 'FRA')).toBe(FOUND_LITTLE);
    expect(resultFor(r1, 'GER')).toBe(FOUND_LITTLE);
    expect(strafedLine(r1, 'GER')).toBe('');
    expect(loadGame(saveGame(a.g))!.airReports.at(-1)!.strafeLittle).toEqual(r1.strafeLittle);
    // 648: all three with less, the bombers' weight beside them: three names joined, and the result sentence stands.
    const b = edge(648);
    const r2 = launchRaid(b.g, 'ground', b.target, b.ids, 'FRA')!;
    expect(r2.strafeLittle).toEqual({ names: names([GC1, GC2, GC3]) });
    expect(strafedLine(r2, 'FRA')).toBe(`Groupement de Chasse 1, Groupement de Chasse 2 and Groupement de Chasse 3 ${LITTLE}`);
    expect(resultFor(r2, 'FRA')).toBe(FOUND_LITTLE);
    // 648 with no bombers: they were all that reached the troops. The raider reads their line alone; the defender, whom
    // her words do not cover, the report's own sentence.
    const c = edge(648, false);
    const r3 = launchRaid(c.g, 'ground', c.target, c.ids, 'FRA')!;
    expect(r3.strafeLittle).toEqual({ names: names([GC1, GC2, GC3]), alone: true });
    expect(r3.result).toBe(FOUND_LITTLE);
    expect(resultFor(r3, 'FRA')).toBe('');
    expect(resultFor(r3, 'GER')).toBe(r3.result);
    // As built (200 German fighters): every group with a fighter's weight or more, no field written.
    const d = saar(0, 'three');
    const r4 = launchRaid(d.g, 'ground', d.target, d.ids, 'FRA')!;
    expect(r4.strafed!.length).toBe(3);
    expect('strafeLittle' in r4).toBe(false);
    expect(saveGame(d.g).includes('"strafeLittle"')).toBe(false);
    expect(strafedLine(r4, 'FRA')).toBe(`${r4.strafed![0]}, ${r4.strafed![1]} and ${r4.strafed![2]} went down and strafed.`);
  });

  it('the result sentence: "The aircraft found little to hit." exactly where the cut prints as 0%, with the rule on; off, as shipped', () => {
    on();
    for (const [n, mult] of [[200, 1], [0, 1], [646, 1], [200, 30]] as const) {
      AIR.combat.fighters = WAS * mult;
      const c = saar(0, 'three');
      const d = c.g.airFleets.find((f) => f.id === 3)!;
      d.ftr = n;
      heard = [];
      const rep = launchRaid(c.g, 'ground', c.target, c.ids, 'FRA')!;
      const cut = heard.find((x) => x.route === 'settled')!.cut!;
      if (Math.round(cut * 100) > 0) expect(rep.result.startsWith('The raids cut the organisation of')).toBe(true);
      else expect(rep.result).toBe(FOUND_LITTLE);
    }
    AIR.combat.fighters = WAS;
    flyKinds([]);
    const off = saar(0, 'three');
    const r = launchRaid(off.g, 'ground', off.target, off.ids, 'FRA')!;
    expect('strafeLittle' in r).toBe(false);
    expect(resultFor(r, 'FRA')).toBe(r.result);
  });

  it('the fought raid: a group whose passes counted under one fighter on the troops is told so, from the field\'s own count', () => {
    on();
    let little = 0;
    for (const k of [0, 1]) {
      const f = edge(648, true, k);
      const sim = airBattle(airTicket(f.g, 'ground', f.target, f.ids, 'FRA').ticket!) as AirSim;
      fight(sim);
      const o = sim.outcome();
      // The field's count group by group sums to its whole.
      const by = o.strafedBy!;
      expect(Object.values(by).reduce((t, x) => t + x, 0)).toBeCloseTo(o.strafed!, 9);
      const fought = foughtAir(o);
      const rep = launchRaid(f.g, 'ground', f.target, f.ids, 'FRA', fought)!;
      const few = (o.strafers ?? []).filter((id) => (by[id] ?? 0) < 1);
      little += few.length;
      expect(rep.strafeLittle?.names ?? []).toEqual(rep.strafed!.filter((x) => names(few).includes(x)));
      // The after card carries the line to the raider, after what the bombers did.
      const line = strafedLine(rep, 'FRA');
      expect(line.includes(LITTLE)).toBe(few.length > 0);
      expect(afterCard(f.g, rep, fought).lines.filter((x) => x === line).length).toBe(1);
    }
    expect(little).toBeGreaterThan(0);
  });
});

// The UI director, 25 November 2026 21:22Z, g: "A name in its own language is bare ("Groupement de Chasse 3", "JG 26");
// an English one takes "the", "The" at the head of a sentence. That is theGroupName ... The lane's report lines take it:
// A6, A5 (strafers.ts:70, today bare: "2nd Fighter Group went down and strafed."), and j and k". Each line with an English
// name and a name in its own language, at the head of a sentence and inside a list; then from a settled raid with one
// group renamed in English (setup: its name only).
describe('the report lines name their groups by the lane\'s one rule (the UI director, 21:22Z, g)', () => {
  const rep = (strafed: string[], little: string[]) => ({ mission: 'ground', attNat: 'FRA', strafed, ...(little.length ? { strafeLittle: { names: little } } : {}) });
  it('the forms, both kinds of name, at the head of a sentence and inside a list', () => {
    expect(strafedLine(rep(['2nd Fighter Group'], []), 'FRA')).toBe('The 2nd Fighter Group went down and strafed.');
    expect(strafedLine(rep(['JG 26'], []), 'FRA')).toBe('JG 26 went down and strafed.');
    expect(strafedLine(rep(['2nd Fighter Group', 'JG 26'], []), 'FRA')).toBe('The 2nd Fighter Group and JG 26 went down and strafed.');
    expect(strafedLine(rep(['JG 26', '2nd Fighter Group', '3rd Fighter Aviation Division'], []), 'FRA')).toBe('JG 26, the 2nd Fighter Group and the 3rd Fighter Aviation Division went down and strafed.');
    expect(strafedLine(rep(['2nd Fighter Group'], ['2nd Fighter Group']), 'FRA')).toBe(`The 2nd Fighter Group ${LITTLE}`);
    expect(strafedLine(rep(['Groupement de Chasse 3'], ['Groupement de Chasse 3']), 'FRA')).toBe(`Groupement de Chasse 3 ${LITTLE}`);
    expect(strafedLine(rep(['JG 26', '2nd Fighter Group'], ['JG 26', '2nd Fighter Group']), 'FRA')).toBe(`JG 26 and the 2nd Fighter Group ${LITTLE}`);
    expect(strafedLine(rep(['Groupement de Chasse 3', '2nd Fighter Group'], ['2nd Fighter Group']), 'FRA')).toBe(`Groupement de Chasse 3 went down and strafed. The 2nd Fighter Group ${LITTLE}`);
  });
  it('a settled raid: Groupement de Chasse 1 renamed in English, the 646 edge raid (one group with a fighter\'s weight, two with less)', () => {
    on();
    const c = saar(0, 'three');
    const d = c.g.airFleets.find((f) => f.id === 3)!;
    d.ftr = 646; d.est.ftr = Math.max(d.est.ftr, 646);
    c.g.airFleets.find((f) => f.id === GC1)!.name = '1st Fighter Group';
    const r = launchRaid(c.g, 'ground', c.target, c.ids, 'FRA')!;
    expect(r.strafeLittle).toEqual({ names: ['1st Fighter Group', 'Groupement de Chasse 2'] });
    expect(strafedLine(r, 'FRA')).toBe(`Groupement de Chasse 3 went down and strafed. The 1st Fighter Group and Groupement de Chasse 2 ${LITTLE}`);
    // The report keeps the names as the groups have them: the rule is the line's.
    expect(r.strafed).toContain('1st Fighter Group');
  });
});
