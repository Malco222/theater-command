// Support teams on the battle card (release 2, behind `teams`, off; src/campaign/teamfire.ts).
//
// With every infantry force's own machine-gun and mortar teams on the field (own-teams.ts), the field leans to the
// defence by about two points at even odds: the teams fire before the lines meet, and the attack is under that fire
// first (the fairness gate, land-battles/teams-gate2-1125.md). The design director ruled that the lean stands and that
// the card carries it by its causes (25 November 2026, 21:40Z and 21:47Z): (a) a team counts at its worth after the
// lines meet, 0.59 of its line in the attack and 0.63 in the defence; (b) fire before the lines meet wears each line,
// by the fire rules' own reach, rate and punch and the seconds of the approach. No fitted figure. Off, the card is
// the card it was; and the field is laid at the power before the term, so it is never taken off twice.
import { describe, expect, it } from 'vitest';
import { diceChance } from '../src/battle/core/parity';
import { OWN_TEAM_SHARE, isOwnTeam, ownTeams } from '../src/battle/land/own-teams';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LAND } from '../src/battle/land/rules';
import { LandBattle } from '../src/battle/land/sim';
import type { FieldTicket, SideTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';
import { FIELD_DICE, addCorps, assess, createGame, relocate } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { TEAM_FIRE, earlySeconds, setTeamFire, teamFire, teamFireLine, teamFireOn } from '../src/campaign/teamfire';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

type Ticket = FieldTicket<LandPlace>;

/** A grand campaign with the player's first month open. */
function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

interface Line { target: Region; from: Region; def: Corps[]; att: Corps[] }
/** A Polish line in the country round Lodz and German corps in front of it, all test corps, in supply. */
function line(g: Game, defKind: Corps['kind'] = 'inf', attKind: Corps['kind'] = 'inf'): Line {
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  const def = [1, 2].map((i) => addCorps(g, 'POL', `Line ${i}`, defKind, 3, target.id, {}));
  const att = [1, 2].map((i) => addCorps(g, 'GER', `Storm ${i}`, attKind, 3, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  g.nations.GER.offStart.POL = g.turn - 6;
  g.nations.GER.lastFight.POL = g.turn - 1;
  return { target, from, def, att };
}

/** A set piece's ticket as a campaign battle's: without the piece's own battalions, its shares whole again. */
function campaign(key: string, seed = 11): Ticket {
  const t = ticketOf(PIECES.find((x) => x.key === key)!, seed, null);
  const side = (s: SideTicket): SideTicket => ({ ...s, forces: s.forces.filter((f) => f.kind !== 'mg' && f.kind !== 'mor').map((f) => ({ ...f, share: f.share / 0.92 })) });
  return { ...t, att: side(t.att), def: side(t.def) };
}

const FOOT: [string, number][] = [['inf', 1]];

describe('the switch', () => {
  it('is off in a new campaign, and off the card is the card it was', () => {
    const g = playing();
    const l = line(g);
    expect(teamFireOn(g)).toBe(false);
    const before = assess(g, l.att, l.target);
    expect(before.teamFire).toBeUndefined();
    expect(before.lines.some((x) => x.label === 'Support teams')).toBe(false);
    setTeamFire(g, true);
    expect(teamFireOn(g)).toBe(true);
    const on = assess(g, l.att, l.target);
    expect(on.teamFire).toBeDefined();
    // its own key: the campaign's `teams` flag is the buildable brigades' switch, and neither moves the other
    expect(g.flags.teams).toBeUndefined();
    setTeamFire(g, false);
    expect(g.flags.teamfire).toBeUndefined();
    g.flags.teams = 1;
    expect(teamFireOn(g)).toBe(false);
    expect(assess(g, l.att, l.target).teamFire).toBeUndefined();
    delete g.flags.teams;
    // on and off again: the same card, to the last figure and the last line
    expect(JSON.stringify(assess(g, l.att, l.target))).toBe(JSON.stringify(before));
  });
});

describe('one row, two readers: the card\'s key is its own, never the brigades\'', () => {
  it('on, the whole save differs by the one key; off again, it is the save it was', () => {
    const g = playing();
    line(g);
    const before = saveGame(g);
    setTeamFire(g, true);
    const on = saveGame(g);
    expect(on).not.toBe(before);
    expect(JSON.parse(on).flags.teamfire).toBe(1);
    // the brigades' switch (the campaign's `teams` flag, the ship list's `teamsrule`) is not touched by it
    expect(JSON.parse(on).flags.teams).toBeUndefined();
    setTeamFire(g, false);
    expect(saveGame(g)).toBe(before);
  });

  it('the brigades\' flag on alone leaves the card\'s term off, and the term on alone leaves the brigades\' flag off', () => {
    const g = playing();
    const l = line(g);
    const plain = JSON.stringify(assess(g, l.att, l.target).A);
    g.flags.teams = 1;
    expect(teamFireOn(g)).toBe(false);
    expect(assess(g, l.att, l.target).teamFire).toBeUndefined();
    expect(JSON.stringify(assess(g, l.att, l.target).A)).toBe(plain);
    delete g.flags.teams;
    setTeamFire(g, true);
    expect(g.flags.teams).toBeUndefined();
    expect(assess(g, l.att, l.target).teamFire).toBeDefined();
  });
});

describe('the card with the switch on', () => {
  it('multiplies each side by its term and nothing else, and the odds by their ratio', () => {
    const g = playing();
    const l = line(g);
    const off = assess(g, l.att, l.target);
    setTeamFire(g, true);
    const on = assess(g, l.att, l.target);
    const k = on.teamFire!;
    expect(k.share).toEqual({ att: 1, def: 1 });
    expect(on.A).toBeCloseTo(off.A * k.att, 9);
    expect(on.D).toBeCloseTo(off.D * k.def, 9);
    expect(on.odds).toBeCloseTo((off.odds * k.att) / k.def, 9);
    // the same term the rules give for these forces at the card's own odds before it
    const by = teamFire({ att: FOOT, def: FOOT, odds: off.odds, terrain: l.target.terrain })!;
    expect(k.att).toBeCloseTo(by.att, 12);
    expect(k.def).toBeCloseTo(by.def, 12);
    // every other part of the card is as it was: the dice, the works, the defenders
    expect(on.dice).toEqual(off.dice);
    expect(on.prepared).toBe(off.prepared);
    expect(on.defenders.map((c) => c.id)).toEqual(off.defenders.map((c) => c.id));
    const line0 = on.lines.find((x) => x.label === 'Support teams')!;
    expect(line0.value).toBe('Machine guns and mortars on both sides fire before the lines meet');
    expect(line0.effect).toBe(`Attack down ${Math.round((1 - k.att) * 100)}%, defence down ${Math.round((1 - k.def) * 100)}%: the attack is under fire first, and for longer`);
    expect(on.lines.filter((x) => x.label !== 'Support teams')).toEqual(off.lines);
  });

  it('prices nothing where the fight is not a field battle: a town stormed, a landing, a sortie', () => {
    const g = playing();
    const l = line(g);
    setTeamFire(g, true);
    expect(assess(g, l.att, l.target).teamFire).toBeDefined();
    expect(assess(g, l.att, l.target, { town: true }).teamFire).toBeUndefined();
    expect(assess(g, l.att, l.target, { sea: 0.5 }).teamFire).toBeUndefined();
    expect(assess(g, l.att, l.target, { sally: true }).teamFire).toBeUndefined();
  });

  it('prices only the corps that bring teams: tanks against tanks is the card as it was', () => {
    const g = playing();
    const l = line(g, 'arm', 'arm');
    const off = assess(g, l.att, l.target);
    setTeamFire(g, true);
    const on = assess(g, l.att, l.target);
    expect(on.teamFire).toBeUndefined();
    expect(JSON.stringify(on)).toBe(JSON.stringify(off));
  });
});

describe('the term by its two causes', () => {
  it('comes to 0.991 of the odds for infantry against infantry at 1.2 to 1 on plains: about two points, as the field leans', () => {
    const k = teamFire({ att: FOOT, def: FOOT, odds: 1.2, terrain: 'plains' })!;
    expect(k.att / k.def).toBeCloseTo(0.991, 3);
    const points = (diceChance(1.2, FIELD_DICE) - diceChance((1.2 * k.att) / k.def, FIELD_DICE)) * 100;
    // (the field's lean on seven campaign grounds: 2.1 points, 48.7% against 50.8%)
    expect(points).toBeGreaterThan(1.5);
    expect(points).toBeLessThan(3);
  });

  it('(a) counts a team at its worth of the line it was taken from, by side', () => {
    const W = TEAM_FIRE.worth;
    expect(W).toEqual({ att: 0.59, def: 0.63 });
    const k = teamFire({ att: FOOT, def: FOOT, odds: 1.2, terrain: 'plains' })!;
    // each side's term is its worth after contact times what the early fire leaves of its bar
    expect(k.att).toBeCloseTo((1 - 2 * OWN_TEAM_SHARE * (1 - W.att)) * (1 - k.early.att / LAND.spent.att), 12);
    expect(k.def).toBeCloseTo((1 - 2 * OWN_TEAM_SHARE * (1 - W.def)) * (1 - k.early.def / LAND.spent.def), 12);
  });

  it('(b) wears each line by the fire rules alone: the enemy teams\' power, rate, punch and seconds, and the line\'s cover', () => {
    const s = earlySeconds(), T = LAND.teams, S = OWN_TEAM_SHARE, odds = 1.2;
    const k = teamFire({ att: FOOT, def: FOOT, odds, terrain: 'plains' })!;
    const onAttack = (S * (T.mg.punch * s.def.mg + T.mor.punch * s.def.mor) * LAND.rate) / (odds * (1 - 2 * S));
    const onDefence = (S * (T.mg.punch * s.att.mg + T.mor.punch * s.att.mor) * LAND.rate * odds * LAND.hold.take.plains) / (1 - 2 * S);
    expect(k.early.att).toBeCloseTo(onAttack, 12);
    expect(k.early.def).toBeCloseTo(onDefence, 12);
    // as the field's trace has it (400 battles, four grounds at 1.2: 0.266% and 0.264% of each line before the lines meet)
    expect(k.early.att * 100).toBeGreaterThan(0.24);
    expect(k.early.att * 100).toBeLessThan(0.3);
    expect(k.early.def * 100).toBeGreaterThan(0.24);
    expect(k.early.def * 100).toBeLessThan(0.3);
  });

  it('counts the seconds from the field\'s own reaches and speeds: a weapon on the march counts none', () => {
    const s = earlySeconds(), G = TEAM_FIRE, pace = LAND.speed.inf * G.pace;
    expect(s.meet).toBeCloseTo((G.gap - LAND.reach.inf) / pace, 9);
    // mortars reach from where they stand, on both sides
    expect(s.att.mor).toBe(s.meet);
    expect(s.def.mor).toBe(s.meet);
    // the defence's machine guns wait for the attack to come into their reach
    expect(s.def.mg).toBeCloseTo(s.meet - (G.gap + G.back - LAND.teams.mg.reach) / pace, 9);
    // the attack's march up to three quarters of their reach and fire only then
    expect(s.att.mg).toBeCloseTo(s.meet - (G.gap + G.back - LAND.teams.mg.reach * G.up.to) / LAND.teams.mg.speed, 9);
    expect(s.att.mg).toBeLessThan(s.def.mg);
    expect(Math.round(s.meet)).toBe(98);
    expect(Math.round(s.def.mg)).toBe(61);
    expect(Math.round(s.att.mg)).toBe(15);
  });

  it('the attack is under fire first: at even power its line is worn more of its bar than the defence\'s', () => {
    const k = teamFire({ att: FOOT, def: FOOT, odds: 1, terrain: 'plains' })!;
    expect(k.early.att / LAND.spent.att).toBeGreaterThan(k.early.def / LAND.spent.def);
    expect(k.att).toBeLessThan(k.def);
  });

  it('follows who brings teams: a side of tanks brings none, and bullets do little to a line of tanks', () => {
    expect(teamFire({ att: [['arm', 1]], def: [['arm', 1], ['art', 0.2]], odds: 1.2, terrain: 'plains' })).toBeNull();
    const mixed = teamFire({ att: [['inf', 1], ['arm', 1]], def: FOOT, odds: 1.2, terrain: 'plains' })!;
    expect(mixed.share).toEqual({ att: 0.5, def: 1 });
    // tanks alone against infantry: only the defence's teams fire, at their rule's share against armour
    const tanks = teamFire({ att: [['arm', 1]], def: FOOT, odds: 1.2, terrain: 'plains' })!;
    const s = earlySeconds(), T = LAND.teams;
    expect(tanks.share).toEqual({ att: 0, def: 1 });
    expect(tanks.early.def).toBe(0);
    expect(tanks.early.att).toBeCloseTo((OWN_TEAM_SHARE * (T.mg.punch * T.mg.armour * s.def.mg + T.mor.punch * T.mor.armour * s.def.mor) * LAND.rate) / 1.2, 12);
    const foot = teamFire({ att: FOOT, def: FOOT, odds: 1.2, terrain: 'plains' })!;
    expect(tanks.early.att).toBeLessThan(foot.early.att / 2);
  });

  it('a town\'s guard beside the defence is a block of its own and brings none', () => {
    const k = teamFire({ att: FOOT, def: [['inf', 3]], guard: 1, odds: 1.2, terrain: 'plains' })!;
    expect(k.share.def).toBeCloseTo(0.75, 12);
  });

  it('reads the defence\'s cover as the field does: its ground and the works that face the attack', () => {
    const at = (terrain: string, fort = 0): number => teamFire({ att: FOOT, def: FOOT, odds: 1.2, terrain, fort })!.early.def;
    expect(at('forest') / at('plains')).toBeCloseTo(LAND.hold.take.forest / LAND.hold.take.plains, 9);
    expect(at('plains', 2) / at('plains')).toBeCloseTo(LAND.hold.works[2], 9);
    expect(at('plains', 9)).toBe(at('plains', 3));
  });

  it('says so on the card in whole points, and says nothing under one', () => {
    const k = teamFire({ att: FOOT, def: FOOT, odds: 1.2, terrain: 'plains' })!;
    expect(teamFireLine(k)!.label).toBe('Support teams');
    expect(teamFireLine({ ...k, att: 0.999, def: 0.998 })).toBeNull();
    expect(teamFireLine({ ...k, share: { att: 0, def: 1 } })!.value).toBe('The defenders\' machine guns and mortars fire as the attack comes on');
    expect(teamFireLine({ ...k, share: { att: 1, def: 0 } })!.value).toBe('The attack\'s machine guns and mortars fire it in');
  });
});

describe('the figures marked G are the field\'s own', () => {
  type B = { side: 'att' | 'def'; kind: string; team?: string; y: number; speed: number };
  const laid = (key: string) => {
    const s = new LandBattle(ownTeams(campaign(key)).ticket as never);
    s.teamsRule = true; s.ai.att = true; s.ai.def = true;
    const us = s.units as unknown as B[];
    const gap = (a: B, b: B): number => (s as unknown as { gap(a: B, b: B): number }).gap(a, b);
    // (the line is the foot in front: tanks stand a hundred metres behind it, guns and teams further back)
    const lineOf = (side: string): B[] => us.filter((u) => u.side === side && !u.team && u.kind === 'inf');
    const near = (u: B): number => Math.min(...lineOf(u.side === 'att' ? 'def' : 'att').map((f) => gap(u, f)));
    const mean = (xs: number[]): number => xs.reduce((a, b) => a + b, 0) / xs.length;
    return { s, us, near, mean, lineOf };
  };

  it('the lines start as far apart, and the teams as far behind their own, as the card counts', () => {
    for (const key of ['normandy-1944', 'prokhorovka-1943']) {
      const { us, near, mean, lineOf } = laid(key);
      const gap = mean(lineOf('att').map(near));
      const teams = us.filter((u) => u.team);
      expect(teams.length).toBeGreaterThan(0);
      expect(Math.abs(gap - TEAM_FIRE.gap)).toBeLessThan(15);
      expect(Math.abs(mean(teams.map(near)) - gap - TEAM_FIRE.back)).toBeLessThan(40);
    }
  });

  it('the attack\'s foot comes on at about the pace the card counts', () => {
    const paces: number[] = [];
    for (const key of ['normandy-1944', 'prokhorovka-1943']) {
      const { s, mean, lineOf } = laid(key);
      const foot = lineOf('att').filter((u) => u.kind === 'inf'), y0 = foot.map((u) => u.y);
      let n = 0;
      while (s.t < 30 && n++ < 100000) s.step();
      const y30 = foot.map((u) => u.y);
      while (s.t < 70 && n++ < 100000) s.step();
      paces.push(mean(foot.map((u, i) => Math.abs(u.y - y30[i]) / (s.t - 30))) / LAND.speed.inf);
      expect(mean(foot.map((u, i) => Math.abs(y30[i] - y0[i])))).toBeGreaterThan(0);
    }
    for (const p of paces) { expect(p).toBeGreaterThan(0.85); expect(p).toBeLessThanOrEqual(1.001); }
    expect(Math.abs(paces.reduce((a, b) => a + b, 0) / paces.length - TEAM_FIRE.pace)).toBeLessThan(0.06);
  });

  it('the staff marches a machine-gun team up into its reach and stands it there; a mortar team in reach stands where it is (ai.ts gunsUp)', () => {
    const G = TEAM_FIRE, T = LAND.teams;
    // the figures the card counts: beyond 0.9 of its reach a team moves up, to 0.75 of it
    expect(G.gap + G.back).toBeGreaterThan(T.mg.reach * G.up.beyond);
    expect(G.gap + G.back).toBeLessThanOrEqual(T.mor.reach * G.up.beyond);
    for (const key of ['normandy-1944', 'prokhorovka-1943']) {
      const { s, us, near } = laid(key);
      const mg = us.find((u) => u.side === 'att' && u.team === 'mg')!, mor = us.find((u) => u.side === 'att' && u.team === 'mor')!;
      const from = { mg: mg.y, mor: mor.y };
      let stood: number | null = null, fired: number | null = null, n = 0;
      while (s.t < 96 && n++ < 100000) {
        s.step();
        if (stood === null && Math.abs(mg.y - from.mg) > 50 && mg.speed < 0.01) stood = s.t;
        // on the march it strikes nothing (sim.ts targetOf): its first target comes when it stands
        if (fired === null && s.targetOf(mg as never)) fired = s.t;
      }
      expect(Math.abs(mor.y - from.mor)).toBeLessThan(1);
      expect(stood).not.toBeNull();
      expect(fired).not.toBeNull();
      expect(fired!).toBeGreaterThanOrEqual(stood! - 0.5);
      // it stands inside its reach, and about when the card counts it (the march the card counts, at the team's own speed)
      expect(near(mg)).toBeLessThanOrEqual(T.mg.reach * G.up.beyond);
      expect(Math.abs(stood! - (G.gap + G.back - T.mg.reach * G.up.to) / T.mg.speed)).toBeLessThan(10);
    }
  });
});

describe('the field is laid at the power before the term', () => {
  it('with the term on the ticket and the teams on the field, the blocks share out the power before it', () => {
    const t = campaign('normandy-1944');
    const k = { att: 0.95, def: 0.96 };
    const priced: Ticket = { ...t, reckoning: { ...t.reckoning, teamFire: k } };
    const plain = new LandBattle(ownTeams(t).ticket as never), on = new LandBattle(ownTeams(priced).ticket as never);
    const power = (s: LandBattle): { att: number; def: number } => (s as unknown as { power: { att: number; def: number } }).power;
    expect(ownTeams(priced).ticket.att.forces.some(isOwnTeam)).toBe(true);
    expect(power(on).att).toBeCloseTo(power(plain).att / k.att, 9);
    expect(power(on).def).toBeCloseTo(power(plain).def / k.def, 9);
    // the card's odds on the ticket are untouched: the verdict and the staff's dice read them
    expect(ownTeams(priced).ticket.reckoning.odds).toBe(t.reckoning.odds);
  });

  it('priced but fought without the teams on the field, it is laid at the card\'s odds as it always was', () => {
    const t = campaign('normandy-1944');
    const priced: Ticket = { ...t, reckoning: { ...t.reckoning, teamFire: { att: 0.95, def: 0.96 } } };
    const power = (s: LandBattle): { att: number; def: number } => (s as unknown as { power: { att: number; def: number } }).power;
    expect(power(new LandBattle(priced as never))).toEqual(power(new LandBattle(t as never)));
  });
});
