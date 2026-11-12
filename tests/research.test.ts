// Research and doctrine: design teams, history's clock, designs that arrive early or late,
// what a lead is worth on land, in the air, at sea and in the factories, and saved games.
import { describe, expect, it } from 'vitest';
import { addCorps, assess, corpsBase, createGame } from '../src/campaign/game';
import { income } from '../src/campaign/economy';
import { assessAir, airFleetsOf, forecastAir } from '../src/campaign/air';
import { fleetPower } from '../src/campaign/navy';
import {
  FIELDS, MAX_TEAMS, RESEARCH, arrival, bureauCost, doctrineOf, foundBureau, inService, lead, monthsAhead, nextStep, researches, setTeams,
  stepsOf, teamsFree, teamsTotal, techMul,
} from '../src/campaign/research';
import { loadGame, saveGame } from '../src/campaign/save';
import { beginHuman, endHuman, humanUp, orderBureau, orderTeams, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Field, Game } from '../src/campaign/types';

const MAJORS = ['GER', 'UK', 'FRA', 'ITA', 'SOV'];
const line = (lines: { label: string; value: string; effect?: string }[], label: string) => lines.find((l) => l.label === label);

function asPlayer(nat: string, seed = 7): Game {
  const g = createGame(seed, nat);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
  return g;
}

function nextMonth(g: Game): void {
  endHuman(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
}

/** History to the end of June 1940, then the player takes Germany into July with France beaten. */
function july1940(seed = 3): Game {
  const g = createGame(seed, null);
  for (let t = 1; t <= 10; t++) runTurnAI(g);
  g.human = 'GER';
  beginHuman(g);
  return g;
}

/** Puts a branch this many months ahead of (or behind) where history stands for the nation. */
function shift(g: Game, nat: string, f: Field, months: number): void {
  const lab = g.nations[nat].lab!;
  lab.clock[f] = lab.months + months;
}

describe('design teams', () => {
  it('the five great powers have design teams, one to a branch, and the pre-war designs in service', () => {
    const g = createGame(1, null);
    for (const nat of MAJORS) {
      const lab = g.nations[nat].lab!;
      expect(lab, nat).not.toBeNull();
      expect(teamsTotal(lab)).toBe(FIELDS.length);
      expect(teamsFree(lab)).toBe(0);
      for (const f of FIELDS) {
        expect(lab.teams[f]).toBe(1);
        expect(stepsOf(nat, f)[0].at, `${nat} ${f}`).toBeLessThanOrEqual(0);
        expect(inService(nat, lab, f)).toEqual(stepsOf(nat, f)[0]);
      }
      expect(doctrineOf(nat)).not.toBe('');
    }
    expect(inService('GER', g.nations.GER.lab!, 'armour')!.name).toBe('Panzer III and IV with short guns');
    expect(inService('UK', g.nations.UK.lab!, 'fighters')!.name).toBe('Hurricane and Spitfire Mk I');
    expect(doctrineOf('GER')).toBe('Blitzkrieg');
    expect(doctrineOf('SOV')).toBe('Deep Battle');
  });

  it('a small nation buys its weapons abroad', () => {
    const g = createGame(1, null);
    expect(researches('POL')).toBe(false);
    expect(g.nations.POL.lab).toBeNull();
    expect(lead(g, 'POL', 'armour')).toBe(0);
    expect(techMul(g, 'POL', 'armour')).toBe(1);
    expect(setTeams(g, 'POL', 'armour', 2)).toBe(false);
    expect(foundBureau(g, 'POL')).toBe(false);
  });

  it('every design list is in date order and the dates are the ones history knows', () => {
    for (const nat of MAJORS) for (const f of FIELDS) {
      const steps = stepsOf(nat, f);
      for (let i = 1; i < steps.length; i++) expect(steps[i].at, `${nat} ${f}`).toBeGreaterThan(steps[i - 1].at);
    }
    const at = (nat: string, f: Field, name: string): number => stepsOf(nat, f).find((s) => s.name.includes(name))!.at;
    expect(at('GER', 'armour', 'Panther')).toBe(47); // July 1943, at Kursk
    expect(at('UK', 'fighters', 'Spitfire Mk V')).toBe(18); // February 1941
    expect(at('SOV', 'armour', 'T-34/85')).toBe(55); // March 1944
    expect(at('UK', 'bombers', 'Lancaster')).toBe(31); // March 1942
  });
});

describe("history's clock", () => {
  it('left alone, every power gets every design in the month history got it, and fights exactly as it did', () => {
    const g = createGame(3, null);
    for (let t = 1; t <= 20; t++) {
      runTurnAI(g);
      for (const nat of MAJORS) {
        const lab = g.nations[nat].lab!;
        for (const f of FIELDS) {
          expect(lab.clock[f], `${nat} ${f} at turn ${t}`).toBe(lab.months);
          expect(lead(g, nat, f)).toBe(0);
          expect(techMul(g, nat, f)).toBe(1);
        }
      }
    }
    // Britain is still in the war and its teams have worked every month of it.
    expect(g.nations.UK.lab!.months).toBe(20);
    expect(inService('UK', g.nations.UK.lab!, 'fighters')!.name).toBe('Spitfire Mk V');
    expect(inService('UK', g.nations.UK.lab!, 'industry')!.name).toBe('Lend-Lease');
  });

  it('a design arrives in its own month, not the month before', () => {
    const g = createGame(3, null);
    while (g.turn < 11) runTurnAI(g);
    const lab = g.nations.GER.lab!;
    expect(inService('GER', lab, 'armour')!.name).toBe('Panzer III and IV with short guns');
    expect(nextStep('GER', lab, 'armour')!.name).toBe('Panzer III with the 50 mm gun');
    expect(arrival(g, 'GER', 'armour')).toBe(11);
    stepAI(g); // Germany's July 1940
    expect(inService('GER', lab, 'armour')!.name).toBe('Panzer III with the 50 mm gun');
  });

  it('the computer never moves a team or founds one', () => {
    const g = createGame(5, null);
    for (let t = 1; t <= 24; t++) runTurnAI(g);
    for (const nat of MAJORS) {
      const lab = g.nations[nat].lab!;
      expect(lab.founded).toBe(0);
      for (const f of FIELDS) expect(lab.teams[f]).toBe(1);
    }
  });
});

describe('the player\'s teams', () => {
  it('a team can only go where one is free, and no branch can use more than three', () => {
    const g = asPlayer('GER');
    expect(orderTeams(g, 'fighters', 2)).toBe(false); // all eight are busy
    expect(orderTeams(g, 'navy', 0)).toBe(true);
    expect(teamsFree(g.nations.GER.lab!)).toBe(1);
    expect(orderTeams(g, 'fighters', 2)).toBe(true);
    expect(orderTeams(g, 'bombers', 0)).toBe(true);
    expect(orderTeams(g, 'fighters', MAX_TEAMS)).toBe(true);
    expect(orderTeams(g, 'radar', 0)).toBe(true);
    expect(orderTeams(g, 'fighters', MAX_TEAMS + 1)).toBe(false);
    expect(orderTeams(g, 'fighters', -1)).toBe(false);
    expect(orderTeams(g, 'fighters', 1.5)).toBe(false);
    expect(orderTeams(g, 'tanks' as Field, 1)).toBe(false);
    expect(g.nations.GER.lab!.teams).toMatchObject({ fighters: 3, navy: 0, bombers: 0, radar: 0, armour: 1 });
  });

  it('orders are only taken in the player\'s own month, for the player\'s own nation', () => {
    const g = asPlayer('GER');
    endHuman(g);
    expect(orderTeams(g, 'navy', 0)).toBe(false);
    expect(orderBureau(g)).toBe(false);
    expect(g.nations.GER.lab!.teams.navy).toBe(1);
  });

  it('two teams work half as fast again, three nearly twice as fast, and none not at all', () => {
    const g = asPlayer('GER');
    orderTeams(g, 'navy', 0); orderTeams(g, 'bombers', 0); orderTeams(g, 'radar', 0);
    orderTeams(g, 'fighters', 3); orderTeams(g, 'armour', 2);
    const lab = g.nations.GER.lab!;
    const start = { ...lab.clock };
    for (let i = 0; i < 4; i++) nextMonth(g);
    expect(lab.months).toBe(5);
    expect(lab.clock.fighters - start.fighters).toBeCloseTo(4 * RESEARCH.speed[3], 9);
    expect(lab.clock.armour - start.armour).toBeCloseTo(4 * RESEARCH.speed[2], 9);
    expect(lab.clock.navy).toBe(start.navy);
    expect(lab.clock.infantry).toBe(lab.months);
    expect(monthsAhead(lab, 'fighters')).toBeCloseTo(4 * (RESEARCH.speed[3] - 1), 9);
    expect(monthsAhead(lab, 'navy')).toBe(-4);
  });

  it('the forecast names the month the next design really arrives', () => {
    const g = asPlayer('GER');
    orderTeams(g, 'navy', 0); orderTeams(g, 'bombers', 0); orderTeams(g, 'fighters', 3);
    const lab = g.nations.GER.lab!;
    const due = arrival(g, 'GER', 'fighters')!;
    expect(due).toBeLessThan(stepsOf('GER', 'fighters')[1].at - 6); // well ahead of March 1941
    expect(arrival(g, 'GER', 'navy')).toBeNull();
    while (g.turn < due - 1) nextMonth(g);
    expect(inService('GER', lab, 'fighters')!.name).toBe('Messerschmitt Bf 109E');
    nextMonth(g);
    expect(g.turn).toBe(due);
    expect(inService('GER', lab, 'fighters')!.name).toBe('Messerschmitt Bf 109F');
    expect(lead(g, 'GER', 'fighters')).toBe(1);
    const news = g.headlines.filter((h) => h.nat === 'GER').map((h) => h.text);
    expect(news.some((t) => /A new fighter enters service: Messerschmitt Bf 109F, \d+ months ahead of history\./.test(t))).toBe(true);
    expect(news).toContain('Bombers falling behind: by now history had Junkers Ju 88.');
  });

  it('a lead lasts until history catches up, and a neglected branch pays when history moves on', () => {
    const g = asPlayer('GER');
    orderTeams(g, 'navy', 0); orderTeams(g, 'fighters', 2);
    const f109 = stepsOf('GER', 'fighters')[1].at, wolf = stepsOf('GER', 'navy')[1].at;
    let first = 0;
    while (g.turn < f109 + 1) {
      if (!first && lead(g, 'GER', 'fighters') === 1) first = g.turn;
      if (g.turn < wolf) expect(lead(g, 'GER', 'navy')).toBe(0);
      else expect(lead(g, 'GER', 'navy')).toBe(-1);
      nextMonth(g);
    }
    expect(first).toBeGreaterThan(0);
    expect(first).toBeLessThan(f109);
  });

  it('a new design team costs production points, and three more is the limit', () => {
    const g = asPlayer('GER');
    const n = g.nations.GER, lab = n.lab!;
    n.pp = RESEARCH.bureau.cost[0] - 1;
    expect(bureauCost(n)).toBe(RESEARCH.bureau.cost[0]);
    expect(orderBureau(g)).toBe(false);
    n.pp = 1000;
    for (let i = 0; i < RESEARCH.bureau.cost.length; i++) {
      const before = n.pp, cost = bureauCost(n)!;
      expect(orderBureau(g)).toBe(true);
      expect(n.pp).toBe(before - cost);
      expect(teamsTotal(lab)).toBe(FIELDS.length + i + 1);
    }
    expect(teamsFree(lab)).toBe(3);
    expect(bureauCost(n)).toBeNull();
    expect(orderBureau(g)).toBe(false);
    expect(orderTeams(g, 'armour', 3)).toBe(true);
    expect(orderTeams(g, 'fighters', 2)).toBe(true);
    expect(orderTeams(g, 'navy', 2)).toBe(false);
  });
});

describe('what a lead is worth', () => {
  it('counts designs, not months, and never more than three either way', () => {
    const g = asPlayer('GER');
    shift(g, 'GER', 'armour', 5); // nothing new within five months of September 1939
    expect(lead(g, 'GER', 'armour')).toBe(0);
    shift(g, 'GER', 'armour', 10); // the 50 mm Panzer III
    expect(lead(g, 'GER', 'armour')).toBe(1);
    expect(techMul(g, 'GER', 'armour')).toBeCloseTo(1 + RESEARCH.fields.armour.per, 9);
    shift(g, 'GER', 'armour', 200);
    expect(lead(g, 'GER', 'armour')).toBe(RESEARCH.maxLead);
    const lab = g.nations.GER.lab!;
    lab.months = 60; // late 1944, and the tank designers never got past 1939
    lab.clock.armour = 0;
    expect(lead(g, 'GER', 'armour')).toBe(-RESEARCH.maxLead);
    expect(techMul(g, 'GER', 'armour')).toBeCloseTo(1 - RESEARCH.maxLead * RESEARCH.fields.armour.per, 9);
  });

  it('better tanks make armoured and motorized corps stronger, better guns the infantry', () => {
    const g = asPlayer('GER');
    const arm = g.corps.find((c) => c.nat === 'GER' && c.kind === 'arm')!, inf = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf')!;
    const a0 = corpsBase(g, arm), i0 = corpsBase(g, inf);
    shift(g, 'GER', 'armour', 10);
    expect(corpsBase(g, arm)).toBeCloseTo(a0 * (1 + RESEARCH.fields.armour.per), 9);
    expect(corpsBase(g, inf)).toBe(i0);
    shift(g, 'GER', 'armour', 0);
    const lab = g.nations.GER.lab!;
    lab.months = 20; lab.clock.infantry = 0; lab.clock.armour = 20; // April 1941 without the Pak 38
    expect(lead(g, 'GER', 'infantry')).toBe(-1);
    expect(corpsBase(g, inf)).toBeCloseTo(i0 * (1 - RESEARCH.fields.infantry.per), 9);
    expect(corpsBase(g, arm)).toBeCloseTo(a0, 9);
  });

  it('the battle ticket says whose equipment is ahead of its time, and only then', () => {
    const g = asPlayer('GER');
    const target = g.byName['Poznan'];
    const from = target.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'GER' && r.units.some((c) => c.nat === 'GER'))!;
    const att = from.units.filter((c) => c.nat === 'GER');
    const plain = assess(g, att, target);
    expect(line(plain.lines, 'Equipment')).toBeUndefined();
    shift(g, 'GER', 'infantry', 20); shift(g, 'GER', 'armour', 10); shift(g, 'GER', 'doctrine', 9);
    const better = assess(g, att, target);
    const eq = line(better.lines, 'Equipment')!;
    expect(eq.value).toContain('German infantry weapons a generation ahead of history');
    expect(eq.value).toContain('German army doctrine a generation ahead of history');
    expect(eq.effect).toContain('Attack up 6%');
    expect(eq.effect).toContain('Attack up 5%');
    expect(better.A).toBeGreaterThan(plain.A * 1.1);
    expect(better.D).toBeCloseTo(plain.D, 9);
  });

  it('doctrine helps the attack, not the defence', () => {
    const g = asPlayer('GER');
    const target = g.byName['Poznan'];
    const from = target.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'GER' && r.units.some((c) => c.nat === 'GER'))!;
    const att = from.units.filter((c) => c.nat === 'GER');
    const plain = assess(g, att, target);
    shift(g, 'GER', 'doctrine', 9);
    expect(assess(g, att, target).A).toBeCloseTo(plain.A * (1 + RESEARCH.fields.doctrine.per), 6);
    shift(g, 'GER', 'doctrine', 0);
    // A defender whose doctrine is ahead holds no better for it; one whose guns are behind holds worse.
    g.nations.SOV.lab!.months = 30; g.nations.SOV.lab!.clock.doctrine = 60;
    const sov = addCorps(g, 'SOV', 'Test Corps', 'inf', 3, target.id, {});
    const withSov = assess(g, att, target);
    g.nations.SOV.lab!.clock.doctrine = 30;
    expect(assess(g, att, target).D).toBeCloseTo(withSov.D, 9);
    expect(sov.nat).toBe('SOV');
  });

  it('better production methods mean more production', () => {
    const g = asPlayer('GER');
    const base = income(g, 'GER');
    shift(g, 'GER', 'industry', 6); // Todt's ministry, six months early
    expect(lead(g, 'GER', 'industry')).toBe(1);
    expect(income(g, 'GER')).toBeCloseTo(base * (1 + RESEARCH.fields.industry.per), 9);
  });

  it('better fighters, radar and bombers all tell in the air', () => {
    const g = july1940();
    const fleets = airFleetsOf(g, 'GER').filter((f) => f.ftr > 0 && f.bmr > 0).slice(0, 2);
    const kent = g.byName['Kent'].id;
    const plain = assessAir(g, fleets, 'superiority', kent);
    expect(plain.why).toBeNull();
    expect(line(plain.lines, 'Equipment')).toBeUndefined();
    const cut0 = forecastAir(g, plain, 'superiority', kent).cut;

    shift(g, 'GER', 'fighters', 9); // the Bf 109F in July 1940
    const ftr = assessAir(g, fleets, 'superiority', kent);
    expect(ftr.Epow).toBeCloseTo(plain.Epow * (1 + RESEARCH.fields.fighters.per), 6);
    expect(ftr.Dpow).toBeCloseTo(plain.Dpow, 9);
    expect(line(ftr.lines, 'Equipment')!.value).toContain('German fighters a generation ahead of history');
    shift(g, 'GER', 'fighters', 0);

    shift(g, 'UK', 'radar', 7); // ground-controlled interception, seven months early
    const rad = assessAir(g, fleets, 'superiority', kent);
    expect(rad.Dpow).toBeCloseTo(plain.Dpow * (1 + RESEARCH.fields.radar.per), 6);
    expect(rad.bomberLoss).toBeGreaterThanOrEqual(plain.bomberLoss);
    expect(line(rad.lines, 'Equipment')!.effect).toContain('Defender up 6%');
    shift(g, 'UK', 'radar', 0);

    g.nations.GER.lab!.clock.bombers = 0; // still without the Ju 88
    expect(lead(g, 'GER', 'bombers')).toBe(-1);
    const bmr = assessAir(g, fleets, 'superiority', kent);
    expect(forecastAir(g, bmr, 'superiority', kent).cut).toBeCloseTo(cut0 * (1 - RESEARCH.fields.bombers.per), 6);
  });

  it('better ships make every fleet stronger', () => {
    const g = asPlayer('UK');
    const f = g.fleets.find((x) => x.nat === 'UK')!;
    const p0 = fleetPower(g, f);
    shift(g, 'UK', 'navy', 8); // the corvettes, early
    expect(fleetPower(g, f)).toBeCloseTo(p0 * (1 + RESEARCH.fields.navy.per), 9);
  });
});

describe('saved games', () => {
  it('a saved game keeps its design teams and their progress', () => {
    const a = asPlayer('GER');
    orderTeams(a, 'navy', 0); orderTeams(a, 'fighters', 2);
    a.nations.GER.pp = 500; orderBureau(a);
    for (let i = 0; i < 3; i++) nextMonth(a);
    const b = loadGame(saveGame(a))!;
    expect(b.nations.GER.lab).toEqual(a.nations.GER.lab);
    expect(b.nations.UK.lab).toEqual(a.nations.UK.lab);
    for (let i = 0; i < 3; i++) { nextMonth(a); nextMonth(b); }
    expect(saveGame(b)).toBe(saveGame(a));
  });

  it('a save from before research picks up on history\'s schedule', () => {
    const a = asPlayer('GER');
    for (let i = 0; i < 12; i++) nextMonth(a);
    const d = JSON.parse(saveGame(a));
    for (const n of Object.values(d.nations) as any[]) delete n.lab;
    const b = loadGame(JSON.stringify(d))!;
    for (const nat of MAJORS) {
      const lab = b.nations[nat].lab!;
      expect(lab.months, nat).toBe(a.nations[nat].lab!.months);
      for (const f of FIELDS) expect(lab.clock[f]).toBe(lab.months);
    }
    expect(inService('GER', b.nations.GER.lab!, 'armour')!.name).toBe('Panzer III with the 50 mm gun');
    expect(b.nations.POL.lab).toBeNull();
  });
});
