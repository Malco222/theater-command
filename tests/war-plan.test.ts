// The war plan (strategy.ts, data/campaign/strategy.json; after release 1). A computer staff that thinks a season
// ahead: it masses for one planned blow where a front has gone quiet, and answers an enemy foothold on its coast
// with a force railed there at once. It ships switched off, and off it leaves no trace in a game; on, it is for
// computer staffs only, at the dial's stops the data names. Its second part: an invasion is reckoned as a campaign
// (the waves that follow the first wave, and the enemy's reply), the staff lays down the troopships for it, and a
// full foothold is widened. The recall of an army that has won its theatre is built and has its own switch, off: in
// the wars measured it put Italy out of the war later, not sooner (strategy.json).

import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { ARSENAL } from '../src/campaign/arsenal';
import { allied } from '../src/campaign/game';
import { campaignOf, idleArmies, landingForce, startOpen } from '../src/campaign/staff';
import { STRATEGY, campaignLog, plansAhead } from '../src/campaign/strategy';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const war = (seed: number, stop: number, months: number, human: string | null = null): Game => {
  const g = startOpen(createGame(seed, human));
  if (stop !== 3) setStop(g, stop, true);
  for (let t = 0; t < months; t++) runTurnAI(g);
  return g;
};
const planFlags = (g: Game): string[] => Object.keys(g.flags).filter((k) => /^st:cam/.test(k));

describe('the war plan', () => {
  const was = { on: STRATEGY.on, staffs: STRATEGY.staffs, recall: STRATEGY.recall!.on };
  afterEach(() => { STRATEGY.on = was.on; STRATEGY.staffs = was.staffs; STRATEGY.recall!.on = was.recall; campaignLog.say = null; });

  it('ships switched off, with its numbers in order', () => {
    expect(STRATEGY.on).toBe(false);
    const C = STRATEGY.campaign, F = STRATEGY.foothold;
    expect(C.prep[0]).toBeGreaterThanOrEqual(1);
    expect(C.prep[1]).toBeGreaterThanOrEqual(C.prep[0]);
    // (The staff's own sums ask more than the blow itself does: a plan is not made at odds it would not strike at.)
    expect(C.strike).toBeGreaterThanOrEqual(C.go);
    expect(F.grown).toBeGreaterThanOrEqual(F.size);
    // Never at the two stops that keep to the book.
    expect(STRATEGY.stops.some((s) => s < 3)).toBe(false);
    // The army of a theatre that is won: left off, a guard always stays, and no crossing is riskier than a landing's own.
    const R = STRATEGY.recall!, W = STRATEGY.landing!.waves!;
    expect(R.on).toBe(false);
    expect(R.least).toBeGreaterThanOrEqual(1);
    expect(R.most).toBeGreaterThanOrEqual(R.least);
    expect(R.quiet).toBeGreaterThanOrEqual(1);
    expect(R.risk).toBeLessThanOrEqual(0.25);
    // The invasion as a campaign: the waves that follow count for no more than they are, and a planning staff is
    // never less wary of the enemy's reply than the test of strength as ruled.
    expect(W.share).toBeGreaterThan(0);
    expect(W.share).toBeLessThanOrEqual(1);
    expect(W.reply).toBeGreaterThanOrEqual(1);
    expect(W.within).toBeGreaterThanOrEqual(1);
    // The ships for it: a staff builds toward more than one harbour's worth, or the rule orders nothing a landing lacks.
    expect(W.ships!).toBeGreaterThan(landingForce());
  });

  it('off, no staff plans and nothing of the plan is written into a game', () => {
    let said = 0;
    campaignLog.say = () => { said++; };
    const g = war(17, 5, 14);
    expect(plansAhead(g, 'GER')).toBe(false);
    expect(planFlags(g)).toEqual([]);
    expect(campaignOf(g, 'GER')).toBeNull();
    // (No army is called home and no foothold widened either: the plan's every event goes through its log.)
    expect(said).toBe(0);
    expect(idleArmies(g, 'UK')).toEqual([]);
  }, 120_000);

  it('on, it is for computer staffs at the named stops, never for the player', () => {
    STRATEGY.on = true;
    const g = startOpen(createGame(17, 'GER'));
    expect(plansAhead(g, 'GER')).toBe(false);
    expect(plansAhead(g, 'SOV')).toBe(STRATEGY.stops.includes(3));
    setStop(g, 1, true);
    expect(plansAhead(g, 'SOV')).toBe(false);
    setStop(g, 5, true);
    expect(plansAhead(g, 'SOV')).toBe(true);
    STRATEGY.staffs = ['UK'];
    expect(plansAhead(g, 'SOV')).toBe(false);
    expect(plansAhead(g, 'UK')).toBe(true);
  });

  it('on, a staff plans a blow, and strikes only where it planned; and lays down the ships for its invasion', () => {
    STRATEGY.on = true;
    const said: { t: number; nat: string; what: string; where: string; lift?: number }[] = [];
    campaignLog.say = (g, nat, what, where, more) => { said.push({ t: g.turn, nat, what, where, lift: more?.lift }); };
    const lift0 = startOpen(createGame(17, null)).nations.GER.sealift;
    const g = war(17, 5, 30);
    expect(said.some((e) => e.what === 'plan')).toBe(true);
    for (const e of said.filter((x) => x.what === 'strike')) {
      const plan = said.filter((x) => x.nat === e.nat && x.what === 'plan' && x.t <= e.t).pop();
      expect(plan?.where).toBe(e.where);
      // (At least the shortest month of massing lies between the plan and the blow.)
      expect(e.t - plan!.t).toBeGreaterThanOrEqual(STRATEGY.campaign.prep[0]);
    }
    expect(planFlags(g).length).toBeGreaterThan(0);
    // Germany, with France fallen and England across the water, lays down troopships: one lot a month, each for one
    // more corps, never past what its yards can take (arsenal.json 'lift') nor past the plan's own number; and the
    // ships come (six months a lot).
    const ships = said.filter((e) => e.what === 'ships');
    const ger = ships.filter((e) => e.nat === 'GER');
    expect(ger.length).toBeGreaterThan(0);
    for (const nat of new Set(ships.map((e) => e.nat))) {
      const mine = ships.filter((e) => e.nat === nat);
      expect(mine.length).toBeLessThanOrEqual(ARSENAL.lift.most);
      expect(new Set(mine.map((e) => e.t)).size).toBe(mine.length);
      for (const e of mine) expect(e.lift!).toBeLessThanOrEqual(STRATEGY.landing!.waves!.ships!);
    }
    expect(g.nations.GER.sealift).toBeGreaterThan(lift0);
    // (The recall is off: nobody is called away.)
    expect(said.some((e) => e.what === 'recall')).toBe(false);
  }, 240_000);

  it('the recall, switched on by its own switch: the army of a theatre that is won goes back into the war, and its guard stays', () => {
    STRATEGY.on = true;
    STRATEGY.recall!.on = true;
    const home = (g: Game, nat: string, id: number): boolean => g.regions[id].owner === nat && !g.regions[id].overseas;
    const said: { t: number; nat: string; where: string; from: number; front: number; cape: boolean }[] = [];
    let checked = 0;
    campaignLog.say = (g, nat, what, where, more) => {
      if (what !== 'recall') return;
      said.push({ t: g.turn, nat, where, from: more!.from as number, front: more!.front as number, cape: !!more!.cape });
    };
    const g = startOpen(createGame(21, null));
    setStop(g, 5, true);
    // In September 1939 the army in Egypt is no idle army: Italy, Germany's ally, stands next door in Libya, at war
    // yet or not. (Before, Britain sent a corps home from Alexandria in the war's second month.) Not even with its
    // garrisons free to go, which makes it more than the landing force that always stays.
    expect(g.nations.UK.atWar).not.toContain('ITA');
    const held = g.corps.filter((c) => c.nat === 'UK' && !c.dead && c.hold && c.kind !== 'fort' && g.regions[c.reg].overseas);
    for (const c of held) c.hold = false;
    expect(g.corps.filter((c) => c.nat === 'UK' && !c.dead && !c.hold && c.kind !== 'fort' && g.regions[c.reg].owner === 'UK' && g.regions[c.reg].overseas).length).toBeGreaterThan(landingForce());
    expect(idleArmies(g, 'UK')).toEqual([]);
    for (const c of held) c.hold = true;
    /** The field corps of a nation on the quiet ground an idle army's harbours stand on. */
    const standing = (ports: number[]): number => {
      const seen = new Set<number>(ports), q = [...ports];
      for (let i = 0; i < q.length; i++) for (const k of g.regions[q[i]].nb) if (!seen.has(k) && !home(g, 'UK', k) && allied(g, 'UK', g.regions[k].ctrl)) { seen.add(k); q.push(k); }
      return g.corps.filter((c) => c.nat === 'UK' && !c.dead && c.kind !== 'fort' && c.kind !== 'eng' && seen.has(c.reg)).length;
    };
    for (let t = 0; t < 32; t++) {
      // (What the staff would send this month, as its month begins: nobody from its own homeland, nobody an order
      // holds in place, and never the whole army of a place: a landing force stays, or all of it if it is smaller.)
      for (const A of idleArmies(g, 'UK')) {
        checked++;
        expect(A.go.length).toBeGreaterThan(0);
        for (const c of A.go) { expect(home(g, 'UK', c.reg)).toBe(false); expect(c.hold).toBeFalsy(); expect(c.kind).not.toBe('fort'); }
        expect(A.ports.length).toBeGreaterThan(0);
        expect(standing(A.ports.map((p) => p.id)) - A.go.length).toBeGreaterThanOrEqual(landingForce());
      }
      runTurnAI(g);
    }
    expect(checked).toBeGreaterThan(0);
    const uk = said.filter((e) => e.nat === 'UK');
    // Britain's desert army starts for home once Africa is quiet, the way it came: round the Cape.
    expect(uk.length).toBeGreaterThan(0);
    expect(uk.some((e) => e.cape)).toBe(true);
    // Nobody is recalled from his own homeland (Ulster is no theatre), and a corps sent home lands on home ground.
    for (const e of said) {
      expect(home(g, e.nat, e.from)).toBe(false);
      if (!e.front) expect(home(g, e.nat, g.byName[e.where].id)).toBe(true);
    }
  }, 240_000);
});
