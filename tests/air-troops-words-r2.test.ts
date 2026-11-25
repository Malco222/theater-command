// A raid on troops, in words (release 2, lane (a)'s switch: flyKinds(['ground']) / TC_FLY=ground / TC.airKinds).
// The design director, 25 November 2026 14:59Z, accepted the card's "more likely than not" list of strafers, the fire
// and the weight staying the exact expectation, and asked the UI director "for words that say the list is the likely
// one, so a strafer who never arrives isn't a broken promise". Her words of 15:04Z, to the letter, no colour on any:
//   the card: LIKELY TO GO DOWN AND STRAFE; under it, once, muted, "The fighter groups most likely to go down and strafe
//   if the sky is won. Which of them do is settled over the target."; the groups a row each; with none, "No fighter
//   group is likely to go down and strafe, though one still may." (her line for none as softened at 16:04Z; muted, the
//   muted line left out above it);
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
import { AIR, assessAir, forecastAir, strafersExpected } from '../src/campaign/air';
import { airPage } from '../src/campaign/airpages';
import { afterCard } from '../src/campaign/airfought';
import { createGame, setWar } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { STRAFE_WORDS, strafeOn, strafeWatch, strafedLine } from '../src/campaign/strafers';
import type { Game } from '../src/campaign/types';
import { AirSim, airBattle } from '../src/battle/air/sim';
import { airTicket, flyKinds, foughtAir } from '../src/battle/air/ticket';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;

type Told = { route: 'card' | 'settled' | 'fought'; target: number; nat: string; strafers: number[] };
let heard: Told[] = [];
const WAS = AIR.combat.fighters;
afterEach(() => { flyKinds([]); strafeWatch.tell = undefined; heard = []; AIR.combat.fighters = WAS; });

const HEADING = 'LIKELY TO GO DOWN AND STRAFE';
const NOTE = 'The fighter groups most likely to go down and strafe if the sky is won. Which of them do is settled over the target.';
const NONE = 'No fighter group is likely to go down and strafe, though one still may.';
const NOBODY = 'No fighter group went down to strafe.';
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
    expect(STRAFE_WORDS).toEqual({ heading: HEADING, note: NOTE, none: NONE, nobody: NOBODY });
    // The air page (the rows the rules build): at the foot of the raid's forces, right under FLYING THE RAID; the muted
    // line is the section's text, each group a row with no nation (no flag, no picture, no colour).
    const page = airPage(c.g, `plan:ground:${c.target}:${p.picked.join('.')}`)!;
    const at = page.sections.findIndex((s) => s.heading === HEADING);
    expect(page.sections[at - 1].heading).toBe('FLYING THE RAID');
    expect(page.sections[at]).toEqual({ heading: HEADING, text: NOTE, rows: [{ nat: '', title: 'Groupement de Chasse 3', lines: [], buttons: [] }] });
    expect(page.sections.filter((s) => s.heading === HEADING).length).toBe(1);
    // The 3D card, through the bridge as the client asks for it.
    TC.load(saveGame(c.g));
    const card = TC.page(`plan:ground:${c.target}:${p.picked.join('.')}`).card;
    expect(card.strafe).toEqual({ heading: HEADING, note: NOTE, names: ['Groupement de Chasse 3'], none: '' });
    // Three groups in reach: each a row, in the raid's order.
    const t = saar(0, 'three');
    const q = planRaid(t.g, 'ground', t.target, t.ids, 'FRA');
    expect(q.strafe!.names.length).toBe(3);
    expect([...q.strafe!.names].sort()).toEqual(names([GC1, GC2, GC3]));
  });

  it('with none: the line for none under the heading, in place of the muted line and the list; a raid of bombers alone says so too', () => {
    on();
    for (const groups of ['far', 'alone'] as const) {
      const c = saar(0, groups);
      const p = planRaid(c.g, 'ground', c.target, c.ids, 'FRA');
      expect(p.why).toBeNull();
      expect(p.strafe).toEqual({ heading: HEADING, note: '', names: [], none: NONE });
      const page = airPage(c.g, `plan:ground:${c.target}:${p.picked.join('.')}`)!;
      expect(page.sections.find((s) => s.heading === HEADING)).toEqual({ heading: HEADING, text: NONE, rows: [] });
      TC.load(saveGame(c.g));
      expect(TC.page(`plan:ground:${c.target}:${p.picked.join('.')}`).card.strafe).toEqual({ heading: HEADING, note: '', names: [], none: NONE });
    }
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
        expect(p.strafe!.none).toBe(card[0].strafers.length ? '' : NONE);
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
      expect(line).toBe(settled[0].strafers.length ? 'Groupement de Chasse 3 went down and strafed.' : NOBODY);
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
    // (Changed on the sky gate, the design director 16:04Z: loaded through the bridge the month's other moves are made,
    // and 1. Jagddivision, about 151 fighters able to meet it, faces Groupement de Chasse 3 alone, short of the 1.65 to 1
    // that wins the sky: the report says nobody went down.)
    expect(r.note.endsWith(` ${NOBODY}`)).toBe(true);
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
