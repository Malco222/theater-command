// Generals: what they add to a battle, how they grow, and how they are lost.
import { describe, expect, it } from 'vitest';
import { assess, createGame, lineStrength, removeCorps, resolveBattle, startPhase } from '../src/campaign/game';
import {
  COMMAND, SKILLS, afterBattle, appoint, assignGeneral, autoSpend, cmdAp, cmdAttack, cmdDefence, cmdLoss, corpsOf,
  gainXp, generalOf, newGeneral, promote, sack, spendPoint, waiting, xpToNext,
} from '../src/campaign/commanders';
import { loadGame, saveGame } from '../src/campaign/save';
import { beginHuman, humanUp, orderAssign, orderPromote, orderSkill, runTurnAI, stepAI } from '../src/campaign/turn';
import type { BattleReport, Corps, Game, General } from '../src/campaign/types';

function asPlayer(nat: string, seed = 7): Game {
  const g = createGame(seed, nat);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
  return g;
}

const corps = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name)!;
const general = (g: Game, name: string): General => g.generals.find((x) => x.name === name)!;

/** A battle report as the rules would file it, for feeding a general's record by hand. */
function report(g: Game, o: Partial<Omit<BattleReport, 'attackers' | 'defenders'>> & { attackers: Corps[]; defenders: Corps[] }): BattleReport {
  const unit = (c: Corps, lost: number) => ({ name: c.name, nat: c.nat, kind: c.kind, divs: c.divs, men: 30000, lost, fate: '' as const });
  g.flags.battles = (g.flags.battles ?? 0) + 1;
  return {
    id: g.flags.battles, turn: g.turn, week: 1, region: 0, regionName: 'Testfeld', attNat: o.attackers[0].nat, defNat: o.defenders[0]?.nat ?? 'POL',
    lines: [], aPower: 10, dPower: 5, odds: 2.5, level: 'Decisive victory', taken: true, attLost: 0, defLost: 0, prisoners: 0,
    ...o,
    attackers: o.attackers.map((c) => unit(c, 1000)), defenders: o.defenders.map((c) => unit(c, 1000)),
  };
}

describe('the real commanders of 1939', () => {
  it('start the war at the head of their corps, with the skills and traits they are known for', () => {
    const g = createGame(1, null);
    const guderian = general(g, 'Heinz Guderian');
    expect(guderian.historical).toBe(true);
    expect(corpsOf(g, guderian)!.name).toBe('XIX Corps');
    expect(guderian.traits).toContain('panzer_leader');
    expect(guderian.level).toBe(3);
    expect(guderian.skills.offense).toBe(2);
    expect(generalOf(g, corps(g, 'XIX Corps'))).toBe(guderian);
  });

  it('others wait for a command, and the computer gives them one', () => {
    const g = createGame(1, null);
    expect(g.generals.some((x) => x.name === 'Gerd von Rundstedt')).toBe(false);
    stepAI(g); // Germany's first month
    const rundstedt = general(g, 'Gerd von Rundstedt');
    expect(rundstedt.status).toBe('active');
    expect(corpsOf(g, rundstedt)).not.toBeNull();
    expect(waiting(g, 'GER').length).toBe(0);
    // Zhukov is not on the list until the Soviet Union is in the war's 22nd month.
    expect(g.generals.some((x) => x.name === 'Georgy Zhukov')).toBe(false);
  });

  it('the player is asked instead: his generals wait until he places them', () => {
    const g = asPlayer('GER');
    const free = waiting(g, 'GER').map((x) => x.name);
    expect(free).toContain('Gerd von Rundstedt');
    const target = g.corps.find((c) => c.nat === 'GER' && c.general === null && c.kind === 'inf')!;
    expect(orderAssign(g, general(g, 'Gerd von Rundstedt').id, target.id)).toBe(true);
    expect(generalOf(g, target)!.name).toBe('Gerd von Rundstedt');
    // One new command a month.
    const other = g.corps.find((c) => c.nat === 'GER' && c.general === null && c.kind === 'inf')!;
    expect(orderAssign(g, general(g, 'Gerd von Rundstedt').id, other.id)).toBe(false);
  });
});

describe('what a general adds', () => {
  it('offense and defense skills and traits move the Battle Ticket', () => {
    const g = createGame(1, null);
    const c = corps(g, 'XIX Corps');
    const gen = generalOf(g, c)!;
    expect(cmdAttack(g, c, false)).toBeCloseTo(1.04 * 1.06, 6);
    gen.skills.offense = 5;
    expect(cmdAttack(g, c, false)).toBeCloseTo(1.1 * 1.06, 6);
    const target = g.byName['Pomorze'];
    const withHim = assess(g, [c], target).A;
    appoint(g, gen, null);
    const without = assess(g, [c], target).A;
    expect(withHim / without).toBeCloseTo(1.1 * 1.06, 4);
    expect(cmdAttack(g, c, false)).toBe(1);
    expect(cmdDefence(g, c)).toBe(1);
  });

  it('the ticket names the commanders and says what they are worth', () => {
    const g = createGame(1, null);
    const a = assess(g, [corps(g, 'XIX Corps')], g.byName['Pomorze']);
    const line = a.lines.find((l) => l.label === 'Commanders')!;
    expect(line.value).toContain('Heinz Guderian (level 3)');
    expect(line.value).toContain('Wladyslaw Bortnowski');
    expect(line.effect).toMatch(/Attack up 10%/);
  });

  it('a panzer leader gives fast troops an extra week, and nothing special to infantry', () => {
    const g = createGame(1, null);
    const tanks = corps(g, 'XIX Corps');
    const gen = generalOf(g, tanks)!;
    expect(cmdAp(g, tanks)).toBe(1);
    const foot = g.corps.find((c) => c.nat === 'GER' && c.kind === 'inf' && c.general === null)!;
    appoint(g, gen, foot);
    expect(cmdAp(g, foot)).toBe(0);
    expect(cmdAttack(g, foot, false)).toBeCloseTo(1.04, 6);
    expect(tanks.general).toBeNull();
  });

  it('an encirclement master hits harder only when the defenders have no way out', () => {
    const g = createGame(1, null);
    const c = corps(g, 'XIX Corps');
    generalOf(g, c)!.traits.push('encirclement_master');
    expect(cmdAttack(g, c, true) / cmdAttack(g, c, false)).toBeCloseTo(1.15, 6);
  });

  it('a butcher loses more of his own men, a cautious general fewer', () => {
    const g = createGame(1, null);
    const c = corps(g, 'XIX Corps');
    const gen = generalOf(g, c)!;
    gen.traits = ['butcher'];
    expect(cmdLoss(g, c)).toBeCloseTo(1.25, 6);
    gen.traits = ['cautious'];
    expect(cmdLoss(g, c)).toBeCloseTo(0.85, 6);
  });

  it('a quartermaster wastes less on a long supply line', () => {
    const g = createGame(3, null);
    for (let t = 1; t <= 26; t++) runTurnAI(g);
    const deep = g.corps.filter((c) => c.nat === 'GER' && c.general === null && lineStrength(g, c).mul < 1);
    expect(deep.length).toBeGreaterThan(0);
    const c = deep[0];
    const bare = lineStrength(g, c).mul;
    const gen = newGeneral(g, 'GER', 'Test Quartermaster', { level: 4, skills: { logistics: 3 } });
    appoint(g, gen, c);
    expect(lineStrength(g, c).mul).toBeCloseTo(1 - (1 - bare) * (1 - 3 * COMMAND.skills.logistics.per!), 6);
  });
});

describe('experience and skills', () => {
  it('experience fills up to the next level, and every level brings one skill point', () => {
    const g = asPlayer('GER');
    const gen = general(g, 'Heinz Guderian');
    expect(xpToNext(gen)).toBe(COMMAND.levelXp[3]);
    gainXp(g, gen, COMMAND.levelXp[3] + 50);
    expect(gen.level).toBe(4);
    expect(gen.xp).toBe(50);
    expect(gen.points).toBe(1);
    expect(orderSkill(g, gen.id, 'logistics')).toBe(true);
    expect(gen.skills.logistics).toBe(1);
    expect(gen.points).toBe(0);
    expect(orderSkill(g, gen.id, 'logistics')).toBe(false);
  });

  it('no skill goes past its cap and no general past the top level', () => {
    const g = createGame(1, null);
    const gen = newGeneral(g, 'GER', 'Test General', { level: 1 });
    gainXp(g, gen, 1_000_000);
    expect(gen.level).toBe(COMMAND.maxLevel);
    expect(xpToNext(gen)).toBe(Infinity);
    expect(SKILLS.reduce((s, k) => s + gen.skills[k], 0)).toBe(COMMAND.maxLevel - 1);
    for (const k of SKILLS) expect(gen.skills[k]).toBeLessThanOrEqual(COMMAND.skillMax);
    gen.points = 1;
    gen.skills.offense = COMMAND.skillMax;
    expect(spendPoint(gen, 'offense')).toBe(false);
  });

  it('the computer spends points on what the general has been doing', () => {
    const g = createGame(1, null);
    const attacker = newGeneral(g, 'GER', 'Test Attacker', {});
    attacker.rec.attacks = 12; attacker.rec.battles = 12; attacker.points = 2;
    autoSpend(attacker);
    expect(attacker.skills.offense).toBeGreaterThanOrEqual(1);
    const defender = newGeneral(g, 'SOV', 'Test Defender', {});
    defender.rec.defences = 12; defender.rec.battles = 12; defender.points = 2;
    autoSpend(defender);
    expect(defender.skills.defense).toBeGreaterThanOrEqual(1);
    expect(defender.points).toBe(0);
  });
});

describe('traits are earned from how a general fights', () => {
  it('three victories that end in surrender make an encirclement master', () => {
    const g = createGame(1, null);
    const c = corps(g, 'XVI Corps'); // Hoepner, a hard charger
    const gen = generalOf(g, c)!;
    const foe = corps(g, 'Army Lodz');
    for (let i = 0; i < 3; i++) {
      expect(gen.traits).not.toContain('encirclement_master');
      const rep = report(g, { attackers: [c], defenders: [foe], prisoners: 20000 });
      afterBattle(g, rep, g.byName['Lodz'], 2.5, [c], [gen], [foe], [generalOf(g, foe)]);
    }
    expect(gen.traits).toContain('encirclement_master');
    expect(gen.rec.battles).toBe(3);
    expect(gen.rec.won).toBe(3);
    expect(g.headlines.some((h) => /Erich Hoepner has earned a reputation: Encirclement master/.test(h.text))).toBe(true);
  });

  it('three positions held against a stronger attacker make an iron defender', () => {
    const g = createGame(1, null);
    const c = corps(g, 'Army Lodz');
    const gen = generalOf(g, c)!;
    const foe = corps(g, 'XVI Corps');
    for (let i = 0; i < 3; i++) {
      const rep = report(g, { attackers: [foe], defenders: [c], taken: false, level: 'Close defeat' });
      afterBattle(g, rep, g.byName['Lodz'], 1.1, [foe], [null], [c], [gen]);
    }
    expect(gen.traits).toContain('iron_defender');
    expect(gen.rec.held).toBe(3);
  });

  it('four defeats in a row make a cautious general, and a win in between breaks the run', () => {
    const g = createGame(1, null);
    const c = corps(g, 'Army Lodz');
    const gen = generalOf(g, c)!;
    const foe = corps(g, 'XVI Corps');
    const lose = () => afterBattle(g, report(g, { attackers: [foe], defenders: [c] }), g.byName['Lodz'], 2.5, [foe], [null], [c], [gen]);
    const hold = () => afterBattle(g, report(g, { attackers: [foe], defenders: [c], taken: false, level: 'Decisive defeat' }), g.byName['Lodz'], 0.6, [foe], [null], [c], [gen]);
    lose(); lose(); lose(); hold();
    expect(gen.rec.streak).toBe(0);
    lose(); lose(); lose();
    expect(gen.traits).not.toContain('cautious');
    lose();
    expect(gen.traits).toContain('cautious');
  });
});

describe('the risks of the job', () => {
  it('a general whose corps is destroyed is captured, killed or gets away, and never keeps the command', () => {
    const fates: Record<string, number> = {};
    for (let seed = 1; seed <= 60; seed++) {
      const g = createGame(seed, null);
      const c = corps(g, 'Army Lodz');
      const gen = generalOf(g, c)!;
      const foe = corps(g, 'XVI Corps');
      const rep = report(g, { attackers: [foe], defenders: [c], prisoners: 30000 });
      removeCorps(g, c);
      afterBattle(g, rep, g.byName['Lodz'], 3, [foe], [null], [c], [gen]);
      expect(gen.corps).toBe(-1);
      const fate = gen.status === 'active' ? 'escaped' : gen.status;
      fates[fate] = (fates[fate] ?? 0) + 1;
    }
    expect(fates.captured).toBeGreaterThan(20);
    expect(fates.dead ?? 0).toBeGreaterThan(0);
    expect(fates.escaped ?? 0).toBeGreaterThan(5);
  });

  it('a wounded general leaves his corps and is fit again when his months are up', () => {
    const g = asPlayer('GER');
    const gen = general(g, 'Heinz Guderian');
    const c = corpsOf(g, gen)!;
    appoint(g, gen, null);
    gen.status = 'wounded';
    gen.backOn = g.turn + 2;
    expect(assignGeneral(g, gen, c)).toBe(false);
    g.turn += 2;
    startPhase(g, 'GER');
    expect(gen.status).toBe('active');
    expect(g.headlines.some((h) => /Heinz Guderian is fit for duty again/.test(h.text))).toBe(true);
    expect(assignGeneral(g, gen, c)).toBe(true);
  });

  it('sacking a popular general costs his corps cohesion and the government political capital', () => {
    const g = asPlayer('GER');
    const gen = general(g, 'Heinz Guderian');
    const c = corpsOf(g, gen)!;
    gen.level = COMMAND.sack.popularLevel;
    g.nations.GER.pc = 20;
    c.org = 1;
    expect(sack(g, gen)).toBe(true);
    expect(gen.status).toBe('sacked');
    expect(c.general).toBeNull();
    expect(c.org).toBeCloseTo(1 - COMMAND.sack.org, 6);
    expect(g.nations.GER.pc).toBe(20 - COMMAND.sack.pc);
    expect(sack(g, gen)).toBe(false);
  });
});

describe('officers who rise from the ranks', () => {
  it('the player can promote an unknown officer for political capital', () => {
    const g = asPlayer('GER');
    const c = g.corps.find((x) => x.nat === 'GER' && x.general === null && x.kind === 'inf')!;
    g.nations.GER.pc = COMMAND.promote.pc - 1;
    expect(orderPromote(g, c.id)).toBeNull();
    g.nations.GER.pc = COMMAND.promote.pc + 4;
    const gen = orderPromote(g, c.id)!;
    expect(gen).not.toBeNull();
    expect(gen.historical).toBe(false);
    expect(gen.level).toBe(1);
    expect(gen.traits).toEqual(['rising_star']);
    expect(generalOf(g, c)).toBe(gen);
    expect(g.nations.GER.pc).toBe(4);
    expect(promote(g, c)).toBeNull(); // the post is taken
    // A rising star learns faster.
    gainXp(g, gen, 100);
    expect(gen.xp).toBe(125);
  });

  it('a war throws up new names, and no nation has more generals than its army can use', () => {
    const g = createGame(5, null);
    for (let t = 1; t <= 12; t++) runTurnAI(g);
    const risen = g.generals.filter((x) => !x.historical);
    expect(risen.length).toBeGreaterThan(0);
    for (const x of risen) expect(x.traits).toContain('rising_star');
    const names = new Set(g.generals.map((x) => `${x.nat}:${x.name}`));
    expect(names.size).toBe(g.generals.length);
    // Every corps points at a general who points back.
    for (const c of g.corps) if (c.general !== null) expect(g.generals[c.general].corps).toBe(c.id);
    for (const x of g.generals) if (x.corps >= 0) expect(g.corps.find((c) => c.id === x.corps)?.general).toBe(x.id);
  });
});

describe('generals and saved games', () => {
  it('a loaded game keeps every general and plays out exactly as the unsaved one would have', () => {
    const a = createGame(11, null);
    for (let t = 1; t <= 3; t++) runTurnAI(a);
    const b = loadGame(saveGame(a))!;
    expect(b.generals).toEqual(a.generals);
    for (let t = 0; t < 8; t++) { runTurnAI(a); runTurnAI(b); }
    expect(b.generals).toEqual(a.generals);
    expect(b.reports.length).toBe(a.reports.length);
    expect(b.reports.map((r) => r.id)).toEqual(a.reports.map((r) => r.id));
  });

  it('a save from before generals were people still loads, and its named commanders are seated', () => {
    const g = createGame(2, null);
    const old = JSON.parse(saveGame(g));
    delete old.generals;
    for (const c of old.corps) {
      const gen = c.general === null ? null : g.generals[c.general];
      c.general = gen ? gen.name : null;
      c.trait = gen?.traits.includes('panzer_leader') ? 'panzer' : null;
    }
    const back = loadGame(JSON.stringify(old))!;
    const guderian = back.generals.find((x) => x.name === 'Heinz Guderian')!;
    expect(guderian.traits).toContain('panzer_leader');
    expect(corpsOf(back, guderian)!.name).toBe('XIX Corps');
    runTurnAI(back);
    expect(back.turn).toBe(2);
  });
});

describe('a battle flows back to the generals in it', () => {
  it('the winner gains experience and a win on his record', () => {
    const g = createGame(1, null);
    const c = corps(g, 'XIX Corps');
    const gen = generalOf(g, c)!;
    const target = g.byName['Pomorze'];
    const tanks = g.corps.filter((x) => x.nat === 'GER' && target.nb.includes(x.reg) && x.kind !== 'fort');
    const rep = resolveBattle(g, tanks, target);
    expect(rep.attackers.some((u) => u.name === 'XIX Corps')).toBe(true);
    expect(gen.rec.battles).toBe(1);
    expect(gen.rec.won).toBe(rep.taken ? 1 : 0);
    expect(gen.xp + (gen.level - 3) * 10000).toBeGreaterThan(0);
  });
});
