// Finishing the war together (rulings 111 and 115; the grand campaign). When an alliance's chief enemy falls, every
// computer-run great power that was at war with it gives notice to the great powers its allies still fight, and is at
// war with them three months after; so does a great power with no enemy great power of its own left standing. Vichy
// does not outlive a France that is free again in Paris. A freed country takes up again the wars it was in when it
// fell. None of it binds the player's own country, and none of it touches a history campaign.
import { beforeAll, describe, expect, it } from 'vitest';
import lib from '../data/campaign/liberation.json';
import { addCorps, allied, atWar, capitulate, capture, createGame, hostileUnits, removeCorps, setWar } from '../src/campaign/game';
import { fieldCtrl } from '../src/campaign/siege';
import { finishMonthEnd, finishMonthOpen, finishRules, owesWar, warNotices } from '../src/campaign/finish';
import { LIB, liberationMonth, passingCard, rememberWars, warsAtFall } from '../src/campaign/liberation';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game, Region } from '../src/campaign/types';

const clear = (g: Game, r: Region): void => { for (const c of [...r.units]) removeCorps(g, c); };
const take = (g: Game, name: string, nat: string): Region => { const r = g.byName[name]; clear(g, r); capture(g, r, nat); return r; };
/** Takes Paris for a nation with nobody of its enemies left round it, and lets the month's end free France. */
const freeParis = (g: Game, nat: string): void => {
  const cap = take(g, 'Paris', nat);
  for (const j of cap.nb) for (const c of [...g.regions[j].units]) if (atWar(g, c.nat, nat)) removeCorps(g, c);
  liberationMonth(g);
};
/** Makes two nations at peace, as a treaty would. */
const peace = (g: Game, a: string, b: string): void => { g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b); g.nations[b].atWar = g.nations[b].atWar.filter((x) => x !== a); };
/** The Soviet Union as an ally at war with Germany alone, and Britain at war with Italy: the war of 1944 in small. */
const eastern = (g: Game): void => {
  g.nations.SOV.align = 'allies';
  for (const e of [...g.nations.SOV.atWar]) peace(g, 'SOV', e);
  setWar(g, 'SOV', 'GER');
  setWar(g, 'UK', 'GER');
  setWar(g, 'UK', 'ITA');
  peace(g, 'SOV', 'ITA');
};

// One computer-run grand campaign played to the autumn of 1940: France has fallen and Vichy keeps the south.
let base = '';
const world = (human: string | null = null): Game => { const g = loadGame(base)!; g.human = human; return g; };
beforeAll(() => {
  const g = startOpen(createGame(2, null));
  let guard = 0;
  while (g.nations.FRA.alive && guard++ < 30) runTurnAI(g);
  for (let k = 0; k < 3; k++) runTurnAI(g);
  base = saveGame(g);
}, 120000);

describe('the data', () => {
  it('gives three months of notice, as the Soviet Union gave Japan', () => {
    expect(lib.finish.notice).toBe(3);
    expect(lib.finish.push).toBeGreaterThanOrEqual(2);
    expect(LIB.finish.notice).toBe(lib.finish.notice);
    // The letter of ruling 115(b) and (c): Vichy's end and a freed country's old wars do not wait for Germany's fall.
    expect(lib.finish.before).toBe(true);
  });
});

describe('a fallen country\'s wars', () => {
  it('are remembered from the moment it falls', () => {
    const g = world();
    expect(g.nations.FRA.alive).toBe(false);
    expect(g.nations.VIC.alive).toBe(true);
    expect(warsAtFall(g, 'FRA')).toContain('GER');
    expect(g.nations.FRA.atWar).toEqual([]);
  });

  it('are taken up again as it returns, where its liberator\'s alliance still fights them', () => {
    const g = world();
    eastern(g);
    // France was at war with Italy when it fell; the Soviet Union, which frees it, never was.
    g.flags['fw:FRA:ITA'] = g.nations.FRA.fellOn!.turn;
    freeParis(g, 'SOV');
    expect(g.nations.FRA.alive).toBe(true);
    expect(g.nations.GER.alive).toBe(true);
    expect(atWar(g, 'FRA', 'GER')).toBe(true);
    expect(atWar(g, 'FRA', 'ITA')).toBe(true);
    expect(atWar(g, 'SOV', 'ITA')).toBe(false);
  });

  it('wait for the month its liberators\' chief enemy falls where the data says so', () => {
    const g = world();
    eastern(g);
    g.flags['fw:FRA:ITA'] = g.nations.FRA.fellOn!.turn;
    LIB.finish.before = false;
    try {
      freeParis(g, 'SOV');
      expect(g.nations.FRA.alive).toBe(true);
      expect(atWar(g, 'FRA', 'GER')).toBe(true);
      // While Germany stands it fights its liberator's wars and no others.
      expect(atWar(g, 'FRA', 'ITA')).toBe(false);
      finishMonthEnd(g);
      expect(atWar(g, 'FRA', 'ITA')).toBe(false);
      capitulate(g, 'GER', 'SOV');
      finishMonthEnd(g);
      expect(atWar(g, 'FRA', 'ITA')).toBe(true);
      expect(atWar(g, 'SOV', 'ITA')).toBe(false);
      expect(g.headlines.some((h) => /France takes up its old war with Italy again/.test(h.text))).toBe(true);
      // And a country freed after the fall takes them up as it returns.
      const h = world();
      eastern(h);
      h.flags['fw:FRA:ITA'] = h.nations.FRA.fellOn!.turn;
      capitulate(h, 'GER', 'SOV');
      freeParis(h, 'UK');
      expect(atWar(h, 'FRA', 'ITA')).toBe(true);
    } finally { LIB.finish.before = true; }
  });

  it('are left alone where nobody of its liberator\'s side fights them any longer', () => {
    const g = world();
    eastern(g);
    g.flags['fw:FRA:ITA'] = g.nations.FRA.fellOn!.turn;
    for (const n of Object.values(g.nations)) if (n.id !== 'ITA' && allied(g, n.id, 'SOV')) peace(g, n.id, 'ITA');
    freeParis(g, 'SOV');
    capitulate(g, 'GER', 'SOV');
    finishMonthEnd(g);
    expect(g.nations.FRA.alive).toBe(true);
    expect(atWar(g, 'FRA', 'ITA')).toBe(false);
  });

  it('are not kept in a history campaign', () => {
    const g = createGame(1, null);
    setWar(g, 'FRA', 'GER');
    rememberWars(g, 'FRA');
    expect(warsAtFall(g, 'FRA')).toEqual([]);
    expect(finishRules(g)).toBe(false);
  });
});

describe('Vichy', () => {
  it('ends in the month France is free again in Paris: its ground and its troops are France\'s', () => {
    const g = world();
    eastern(g);
    const held = g.regions.filter((r) => r.ctrl === 'VIC').map((r) => r.id);
    const troops = g.corps.filter((c) => c.nat === 'VIC' && !c.dead).map((c) => c.id);
    expect(held.length).toBeGreaterThan(5);
    freeParis(g, 'UK');
    expect(g.nations.FRA.alive).toBe(true);
    expect(g.nations.GER.alive).toBe(true);
    finishMonthEnd(g);
    expect(g.nations.VIC.alive).toBe(false);
    expect(g.regions.filter((r) => r.ctrl === 'VIC' || r.owner === 'VIC')).toEqual([]);
    for (const id of held) { expect(g.regions[id].ctrl).toBe('FRA'); expect(g.regions[id].owner).toBe('FRA'); }
    for (const id of troops) expect(g.corps.find((c) => c.id === id)!.nat).toBe('FRA');
    expect(Object.values(g.nations).some((n) => n.atWar.includes('VIC'))).toBe(false);
    expect(g.regions.filter((r) => r.cap && r.owner === 'FRA').map((r) => r.name)).toEqual(['Paris']);
    // It is told as the end of a government, on the roll both interfaces read.
    const p = g.passings.find((x) => x.nat === 'VIC')!;
    expect(p.by).toBe('FRA');
    const card = passingCard(g, p);
    expect(card.title).toBe('The government at Vichy is no more');
    expect(card.text).toMatch(/France's again/);
    expect(g.headlines.some((h) => /government at Vichy is no more/.test(h.text))).toBe(true);
    // And it does not come back.
    finishMonthEnd(g);
    expect(g.passings.filter((x) => x.nat === 'VIC').length).toBe(1);
  });

  /** Vichy in the Axis, its Tunisia Command and the German 90th Light standing together at Mareth, which `holder` holds. */
  const mareth = (holder: string): { g: Game; mareth: Region; tc: number; light: number } => {
    const g = world();
    eastern(g);
    g.nations.VIC.align = 'axis_cobelligerent';
    for (const e of [...g.nations.VIC.atWar]) peace(g, 'VIC', e);
    const r = g.byName['Mareth'];
    clear(g, r);
    r.ctrl = holder;
    const tc = addCorps(g, 'VIC', 'Tunisia Command', 'inf', 1, 'Mareth', {}).id;
    const light = addCorps(g, 'GER', '90th Light Division', 'mot', 1, 'Mareth', {}).id;
    expect(allied(g, 'VIC', 'GER')).toBe(true);
    freeParis(g, 'UK');
    return { g, mareth: r, tc, light };
  };
  /** Every field on the map with troops of two nations at war in it. */
  const mixed = (g: Game): string[] => g.regions.filter((r) => r.units.some((a) => r.units.some((b) => atWar(g, a.nat, b.nat)))).map((r) => r.name);

  it('leaves no enemies side by side: the Germans at Mareth fall back from ground that is France\'s again (field-ITA-1, December 1944)', () => {
    const { g, mareth: r, tc, light } = mareth('VIC');
    finishMonthEnd(g);
    expect(g.nations.VIC.alive).toBe(false);
    expect(r.ctrl).toBe('FRA');
    const french = g.corps.find((c) => c.id === tc)!, german = g.corps.find((c) => c.id === light)!;
    expect(french.nat).toBe('FRA');
    expect(french.reg).toBe(r.id);
    expect(german.dead).toBeFalsy();
    expect(german.reg).not.toBe(r.id);
    // To ground of its own side, with no enemy in it.
    const to = g.regions[german.reg];
    expect(allied(g, 'GER', fieldCtrl(g, to))).toBe(true);
    expect(hostileUnits(g, to, 'GER')).toEqual([]);
    expect(mixed(g)).toEqual([]);
    expect(g.headlines.some((h) => h.text === `With Vichy gone, German troops leave Mareth for ${to.name}: the ground is France's.`)).toBe(true);
  });

  it('where the Germans hold the ground, Vichy\'s troops, France\'s now, are the ones to fall back', () => {
    const { g, mareth: r, tc, light } = mareth('GER');
    finishMonthEnd(g);
    const french = g.corps.find((c) => c.id === tc)!, german = g.corps.find((c) => c.id === light)!;
    expect(german.reg).toBe(r.id);
    expect(french.nat).toBe('FRA');
    expect(french.reg).not.toBe(r.id);
    expect(allied(g, 'FRA', fieldCtrl(g, g.regions[french.reg]))).toBe(true);
    expect(mixed(g)).toEqual([]);
  });

  it('stands while France has not returned, and while a player is playing it', () => {
    const g = world();
    eastern(g);
    capitulate(g, 'GER', 'SOV');
    finishMonthEnd(g);
    expect(g.nations.VIC.alive).toBe(true);
    const h = world('VIC');
    eastern(h);
    freeParis(h, 'UK');
    capitulate(h, 'GER', 'SOV');
    finishMonthEnd(h);
    expect(h.nations.FRA.alive).toBe(true);
    expect(h.nations.VIC.alive).toBe(true);
  });

  it('waits for Germany\'s fall where the data says so', () => {
    const g = world();
    eastern(g);
    freeParis(g, 'UK');
    LIB.finish.before = false;
    try {
      finishMonthEnd(g);
      expect(g.nations.VIC.alive).toBe(true);
      capitulate(g, 'GER', 'SOV');
      finishMonthEnd(g);
      expect(g.nations.VIC.alive).toBe(false);
      expect(g.headlines.some((h) => /With Germany beaten, its ground/.test(h.text))).toBe(true);
    } finally { LIB.finish.before = true; }
  });
});

describe('an alliance whose enemy has fallen', () => {
  it('turns on the great power its allies still fight: notice that month, war three months after', () => {
    const g = world();
    eastern(g);
    const fell = g.turn;
    expect(owesWar(g, 'SOV', 'ITA')).toBe(true);
    capitulate(g, 'GER', 'SOV');
    expect(g.nations.GER.alive).toBe(false);
    expect(warsAtFall(g, 'GER')).toContain('SOV');
    finishMonthEnd(g);
    expect(warNotices(g)).toEqual([{ who: 'SOV', on: 'ITA', given: fell, due: fell + 3 }]);
    expect(g.headlines.some((h) => h.turn === fell && /gives Italy notice/.test(h.text))).toBe(true);
    // Nothing happens before the months are up.
    for (g.turn = fell + 1; g.turn < fell + 3; g.turn++) { finishMonthOpen(g); expect(atWar(g, 'SOV', 'ITA')).toBe(false); }
    g.turn = fell + 3;
    finishMonthOpen(g);
    expect(atWar(g, 'SOV', 'ITA')).toBe(true);
    expect(warNotices(g)).toEqual([]);
    expect(g.headlines.some((h) => h.turn === fell + 3 && /declares war on Italy/.test(h.text))).toBe(true);
    // Its staff presses the new war from the first month.
    expect(g.flags['st:on:SOV:ITA']).toBe(fell + 3);
  });

  it('warns a player it is aimed at on a card of his own', () => {
    const g = world('ITA');
    eastern(g);
    capitulate(g, 'GER', 'SOV');
    finishMonthEnd(g);
    const w = g.headlines.find((h) => h.warn === 'War is coming');
    expect(w).toBeTruthy();
    expect(w!.nat).toBe('ITA');
    expect(w!.about).toBe('SOV');
    expect(w!.text).toMatch(/declare war on you in/);
  });

  it('never binds the player\'s own country', () => {
    const g = world('SOV');
    eastern(g);
    const fell = g.turn;
    capitulate(g, 'GER', 'SOV');
    finishMonthEnd(g);
    expect(warNotices(g)).toEqual([]);
    g.turn = fell + 3;
    finishMonthOpen(g);
    expect(atWar(g, 'SOV', 'ITA')).toBe(false);
  });

  it('drops the notice when the war it stood for is over', () => {
    const g = world();
    eastern(g);
    const fell = g.turn;
    capitulate(g, 'GER', 'SOV');
    finishMonthEnd(g);
    expect(warNotices(g).length).toBe(1);
    for (const n of Object.values(g.nations)) if (n.id !== 'ITA' && allied(g, n.id, 'SOV')) peace(g, n.id, 'ITA');
    g.turn = fell + 3;
    finishMonthOpen(g);
    expect(atWar(g, 'SOV', 'ITA')).toBe(false);
    expect(warNotices(g)).toEqual([]);
  });

  it('asks nothing of a great power that was not at war with the one that fell', () => {
    const g = world();
    eastern(g);
    peace(g, 'SOV', 'GER');
    capitulate(g, 'GER', 'UK');
    finishMonthEnd(g);
    expect(warNotices(g)).toEqual([]);
  });

  it('goes on with its own war when a great power falls that was not the chief enemy', () => {
    // France falls to Germany and Britain stands: Germany's staff is given nothing new.
    const g = startOpen(createGame(2, null));
    capitulate(g, 'FRA', 'GER');
    expect(g.nations.FRA.alive).toBe(false);
    const flags = { ...g.flags };
    finishMonthEnd(g);
    expect(g.flags).toEqual(flags);
    // And Italy falls with Germany standing: the Allies go on against Germany as they were.
    const h = world();
    eastern(h);
    setWar(h, 'SOV', 'ITA');
    capitulate(h, 'ITA', 'UK');
    expect(h.nations.ITA.alive).toBe(false);
    const was = { ...h.flags };
    finishMonthEnd(h);
    expect(h.flags).toEqual(was);
    expect(warNotices(h)).toEqual([]);
  });

  it('turns on its allies\' enemies once it has no great power of its own left to beat, whoever it was that fell', () => {
    const g = world();
    eastern(g);
    // The Soviet Union at war with Italy alone, and Britain with Italy and Germany.
    peace(g, 'SOV', 'GER');
    setWar(g, 'SOV', 'ITA');
    const fell = g.turn;
    capitulate(g, 'ITA', 'UK');
    finishMonthEnd(g);
    expect(warNotices(g)).toEqual([{ who: 'SOV', on: 'GER', given: fell, due: fell + 3 }]);
    // Britain has Germany still to beat, and is told nothing new.
    expect(g.flags['st:on:UK:GER'] ?? 0).not.toBe(fell);
  });

  it('names the great powers still standing an offensive for those already at war with them', () => {
    const g = world();
    eastern(g);
    const fell = g.turn;
    capitulate(g, 'GER', 'SOV');
    finishMonthEnd(g);
    expect(g.flags['st:on:UK:ITA']).toBe(fell);
    expect(g.flags['st:push:UK']).toBe(fell + lib.finish.push - 1);
  });

  it('does nothing in the last act, where every power fights for itself', () => {
    const g = world();
    eastern(g);
    g.flags.dom = g.turn;
    capitulate(g, 'GER', 'SOV');
    finishMonthEnd(g);
    expect(warNotices(g)).toEqual([]);
  });
});

describe('a computer-run grand campaign', () => {
  it('plays to 1945 with every notice of war either standing or kept', () => {
    const g = loadGame(base)!;
    let guard = 0;
    while (g.turn < 70 && guard++ < 80) {
      runTurnAI(g);
      for (const n of warNotices(g)) { expect(g.nations[n.who].alive).toBe(true); expect(n.due).toBe(n.given + 3); }
      if (!g.nations.FRA.alive || !g.nations.FRA.freedOn) continue;
      // A France that is free again in Paris has no Vichy beside it once the month is out.
      if (g.regions[g.nations.FRA.capital].ctrl === 'FRA') expect(g.nations.VIC.alive).toBe(false);
    }
  }, 240000);
});
