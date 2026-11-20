// Junior partners (the historian's section 7, ruled in by the design director 20 November 2026 07:40Z; sandbox stops
// 4 and 5 only): any nation fighting beside a far stronger ally may lose heart from its own causes; a computer's may
// then lose its government with an enemy on its soil, and sign an armistice. The record's dated loss of heart, fall
// of Mussolini and Italian armistice are not read there. Stops 1 to 3 are as they were.
import { describe, expect, it } from 'vitest';
import { leavings, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { createGame, setWar } from '../src/campaign/game';
import { heartCause, heartMonth, noteArmyLost, seniorOf } from '../src/campaign/heart';
import { setStop } from '../src/campaign/history';
import { reformsMonth } from '../src/campaign/reforms';
import { startOpen } from '../src/campaign/staff';
import type { BattleReport, Game } from '../src/campaign/types';

function grand(stop: number, seed = 3): Game {
  const g = startOpen(createGame(seed, null));
  setStop(g, stop, true);
  return g;
}
/** Lays 'n' corps of 'nat' on the memorial wall, lost to 'by' in month 'turn'. */
function fall(g: Game, nat: string, by: string, n: number, turn: number): void {
  for (let i = 0; i < n; i++) g.stories.fallen.push({ id: -1, nat, name: `x${i}`, kind: 'inf', divs: 1, turn, reg: 0, how: 'destroyed', by, born: 1, home: 0, xp: 0 } as never);
}
/** Italy and Germany at war with Britain, Italy with 18 months of land battles and a peak of 30 corps to Germany's 150. */
function axis(stop = 5): Game {
  const g = grand(stop);
  setWar(g, 'GER', 'UK');
  setWar(g, 'ITA', 'UK');
  g.turn = 40;
  monthlyDiplomacy(g);
  g.flags['peak:ITA'] = 30;
  g.flags['peak:GER'] = 150;
  g.flags['cm:ITA'] = 18;
  return g;
}
const heart = (g: Game, nat: string): number | undefined => g.flags[`event:reform:heart:${nat}`];
/** An enemy army takes one of the nation's home provinces. */
function landIn(g: Game, place: string, by: string): void {
  const r = g.byName[place];
  r.ctrl = by;
}

describe('stops 1 to 3 keep the record', () => {
  it('stop 3: Italy loses heart on its month once Tunis is lost, as release 1 has it, and nothing is weighed', () => {
    const g = grand(3);
    setWar(g, 'GER', 'UK');
    setWar(g, 'ITA', 'UK');
    g.byName.Tunis.ctrl = 'UK';
    g.turn = 47;
    monthlyDiplomacy(g);
    expect(g.flags['event:italy-loses-heart']).toBe(47);
    expect(g.nations.ITA.quality).toBe(0.45);
    expect(Object.keys(g.flags).some((k) => k.startsWith('event:reform:heart:') || k.startsWith('q0:'))).toBe(false);
  });
});

describe('a junior partner loses heart (stop 5)', () => {
  it('never by date: the record\'s three Italian events are not read', () => {
    const g = grand(5);
    setWar(g, 'GER', 'UK');
    setWar(g, 'ITA', 'UK');
    g.byName.Tunis.ctrl = 'UK';
    g.byName.Sicily.ctrl = 'UK';
    for (const t of [1, 47, 48, 49, 50]) { g.turn = t; monthlyDiplomacy(g); }
    for (const id of ['italy-loses-heart', 'mussolini-falls', 'italian-armistice']) expect(g.flags[`event:${id}`]).toBeUndefined();
    expect(g.nations.ITA.alive).toBe(true);
    expect(g.nations.ITA.quality).toBe(0.75);
  });

  it('Italy beside Germany: 18 months of land battles and 40% of its peak lost; quality to 0.6 of its first rating', () => {
    const g = axis();
    expect(seniorOf(g, 'ITA')).toBe('GER');
    fall(g, 'ITA', 'UK', 11, 39);
    heartMonth(g);
    expect(heart(g, 'ITA')).toBeUndefined();
    fall(g, 'ITA', 'UK', 1, 40);
    expect(heartCause(g, 'ITA')).toBe('losses');
    heartMonth(g);
    expect(heart(g, 'ITA')).toBe(40);
    expect(g.nations.ITA.quality).toBe(0.45);
  });

  it('not before 18 months of land battles against a great power, whatever it has lost', () => {
    const g = axis();
    g.flags['cm:ITA'] = 17;
    fall(g, 'ITA', 'UK', 20, 39);
    heartMonth(g);
    expect(heart(g, 'ITA')).toBeUndefined();
  });

  it('the loss of all its overseas ground', () => {
    const g = axis();
    const over = g.regions.filter((r) => (r.first ?? r.owner) === 'ITA' && r.overseas);
    expect(over.length).toBeGreaterThan(0);
    for (const r of over.slice(1)) r.ctrl = 'UK';
    expect(heartCause(g, 'ITA')).toBeNull();
    over[0].ctrl = 'UK';
    expect(heartCause(g, 'ITA')).toBe('overseas');
  });

  it('a whole army lost in one battle at its senior partner\'s side', () => {
    const g = axis();
    const unit = (nat: string, divs: number, fate: string) => ({ name: 'x', nat, kind: 'inf', divs, men: 0, lost: 0, fate });
    const rep = { attackers: [unit('UK', 3, 'advanced')], defenders: [unit('ITA', 3, 'destroyed'), unit('ITA', 2, 'surrendered'), unit('GER', 2, 'retreated')] } as unknown as BattleReport;
    noteArmyLost(g, rep);
    expect(g.flags['ha:ITA']).toBeUndefined();
    rep.defenders.push(unit('ITA', 1, 'destroyed') as never);
    noteArmyLost(g, rep);
    expect(g.flags['ha:ITA']).toBe(40);
    expect(heartCause(g, 'ITA')).toBe('army');
    // (Without its senior partner in the battle, a lost army is only a lost battle.)
    const h = axis();
    noteArmyLost(h, { attackers: [unit('UK', 3, 'advanced')], defenders: [unit('ITA', 6, 'destroyed')] } as unknown as BattleReport);
    expect(h.flags['ha:ITA']).toBeUndefined();
  });

  it('nor a partner with half its senior\'s factories or more on its first day: it fights its own war with its own arms', () => {
    const g = axis();
    expect(g.flags['f0:ITA']).toBe(8);
    expect(g.flags['f0:GER']).toBe(37);
    g.flags['f0:ITA'] = 19;
    expect(seniorOf(g, 'ITA')).toBeUndefined();
    fall(g, 'ITA', 'UK', 12, 39);
    heartMonth(g);
    expect(heart(g, 'ITA')).toBeUndefined();
    g.flags['f0:ITA'] = 18;
    expect(seniorOf(g, 'ITA')).toBe('GER');
  });

  it('the senior partner never loses heart, and nobody fighting alone does', () => {
    const g = axis();
    g.flags['cm:GER'] = 40;
    fall(g, 'GER', 'UK', 100, 39);
    heartMonth(g);
    expect(seniorOf(g, 'GER')).toBeUndefined();
    expect(heart(g, 'GER')).toBeUndefined();
    const h = grand(5);
    setWar(h, 'ITA', 'UK');
    h.turn = 40;
    h.flags['peak:ITA'] = 30;
    h.flags['cm:ITA'] = 18;
    fall(h, 'ITA', 'UK', 20, 39);
    heartMonth(h);
    expect(heart(h, 'ITA')).toBeUndefined();
  });

  it('any junior partner: Romania beside Germany against the Soviet Union, from 0.7 to 0.42', () => {
    const g = grand(5);
    setWar(g, 'GER', 'SOV');
    setWar(g, 'ROM', 'SOV');
    g.turn = 45;
    monthlyDiplomacy(g);
    g.flags['peak:ROM'] = 10;
    g.flags['peak:GER'] = 150;
    g.flags['cm:ROM'] = 18;
    fall(g, 'ROM', 'SOV', 4, 44);
    heartMonth(g);
    expect(heart(g, 'ROM')).toBe(45);
    expect(g.nations.ROM.quality).toBe(0.42);
  });

  it('a nation that has lost heart never comes of age', () => {
    const g = axis();
    fall(g, 'ITA', 'UK', 12, 39);
    heartMonth(g);
    g.flags['cm:ITA'] = 30;
    fall(g, 'GER', 'ITA', 20, 39);
    reformsMonth(g);
    expect(g.flags['event:reform:ofAge:ITA']).toBeUndefined();
    expect(g.nations.ITA.quality).toBe(0.45);
  });
});

describe('then its government falls, and it signs (a computer\'s only)', () => {
  it('the month after an enemy first holds a home province; the armistice the month after that; its ground to whoever stands on it', () => {
    const g = axis();
    fall(g, 'ITA', 'UK', 12, 39);
    heartMonth(g);
    expect(leavings(g).find((L) => L.nation === 'ITA')?.grounds).toContain('Sicily');
    g.turn = 41;
    heartMonth(g);
    expect(g.flags['event:regime:ITA']).toBeUndefined();
    landIn(g, 'Sicily', 'UK');
    g.turn = 42;
    heartMonth(g);
    expect(g.flags['event:regime:ITA']).toBeUndefined();
    g.turn = 43;
    heartMonth(g);
    expect(g.flags['event:regime:ITA']).toBe(43);
    expect(g.nations.ITA.alive).toBe(true);
    g.turn = 44;
    heartMonth(g);
    expect(g.flags['event:armistice:ITA']).toBe(44);
    expect(g.nations.ITA.alive).toBe(false);
    expect(g.corps.some((c) => c.nat === 'ITA' && !c.dead)).toBe(false);
    expect(g.byName.Sicily.ctrl).toBe('UK');
    expect(g.regions.some((r) => r.ctrl === 'ITA')).toBe(false);
  });

  it('a foothold given back before the month is out topples nobody', () => {
    const g = axis();
    fall(g, 'ITA', 'UK', 12, 39);
    heartMonth(g);
    landIn(g, 'Sicily', 'UK');
    g.turn = 41;
    heartMonth(g);
    landIn(g, 'Sicily', 'ITA');
    g.turn = 42;
    heartMonth(g);
    expect(g.flags['event:regime:ITA']).toBeUndefined();
    expect(g.flags['hh:ITA']).toBeUndefined();
  });

  it('without the loss of heart, an enemy on its soil topples nobody', () => {
    const g = axis();
    landIn(g, 'Sicily', 'UK');
    for (const t of [41, 42, 43, 44]) { g.turn = t; heartMonth(g); }
    expect(g.flags['event:regime:ITA']).toBeUndefined();
    expect(g.nations.ITA.alive).toBe(true);
  });

  it('the player\'s own Italy loses heart, but its government stands and it signs nothing (ruling 99)', () => {
    const g = axis();
    g.human = 'ITA';
    fall(g, 'ITA', 'UK', 12, 39);
    landIn(g, 'Sicily', 'UK');
    for (const t of [40, 41, 42, 43, 44]) { g.turn = t; heartMonth(g); }
    expect(heart(g, 'ITA')).toBe(40);
    expect(g.flags['event:regime:ITA']).toBeUndefined();
    expect(g.nations.ITA.alive).toBe(true);
    expect(leavings(g).some((L) => L.nation === 'ITA')).toBe(false);
  });
});
