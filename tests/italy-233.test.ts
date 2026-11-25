// The Allied landing in Italy keeps history's season (ruling 233; the grand campaign, computer staffs only). By the
// book (stops 1 and 2) nobody lands on Italian home soil before July 1943, nor reaches its mainland before September,
// the march over the Strait of Messina included. In the usual game (stop 3) nobody lands there before April 1943.
// Either way nobody lands while an Axis army holds the African shore facing it (Tunis, Sfax, Mareth); once an enemy
// of Italy stands on its home ground that rule is spent. Stops 4 and 5 leave the landing to the staff, a player is
// never bound, and no history campaign is touched. (The riders on the staff's order of shores and on the Italian
// fleet are in italy-233-staff.test.ts. Rider (h), the armistice for the mainland alone, was tried and put away.)
import { beforeAll, describe, expect, it } from 'vitest';
import historyData from '../data/campaign/history.json';
import openData from '../data/campaign/open.json';
import { atWar, createGame, setWar } from '../src/campaign/game';
import { HISTORY, setStop, shoreOpen, straitOpen } from '../src/campaign/history';
import { blocked } from '../src/campaign/rivers';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const RULE = HISTORY.shores.find((s) => s.id === 'italy')!;
const USUAL = RULE.usual!, FACING = RULE.facing!, MAINLAND = RULE.mainland!;
const SOUTH = ['Calabria', 'Apulia', 'Naples', 'Rome'];
const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
/** A grand war in the given month with Italy and Britain at war, and the African shore in the hands named. */
const war = (turn: number, holder: string | null = null, stop = 3, human: string | null = null): Game => {
  const g = grand(human);
  if (stop !== 3) expect(setStop(g, stop, true)).toBe(true);
  setWar(g, 'ITA', 'UK');
  g.turn = turn;
  if (holder) for (const name of FACING) g.byName[name].ctrl = holder;
  return g;
};
const open = (g: Game, name: string): boolean => { const r = g.byName[name]; return shoreOpen(g, r.owner, !!r.overseas, r.name); };

describe('the data', () => {
  it('gives Italy\'s shore its months and the shore facing it', () => {
    const row = historyData.shores.find((s) => s.id === 'italy')!;
    expect(row).toMatchObject({ from: 47, mainland: 49, usual: 44, islands: ['Sicily', 'Sardinia'], facing: ['Tunis', 'Sfax', 'Mareth'] });
    const g = grand();
    for (const name of [...row.islands!, ...row.facing!, ...SOUTH]) expect(g.byName[name], name).toBeDefined();
    for (const name of [...row.islands!, ...SOUTH]) { expect(g.byName[name].owner).toBe('ITA'); expect(g.byName[name].overseas).toBeFalsy(); }
    expect(historyData._about).toContain('ruling 233');
  });
});

describe('the usual game (stop 3)', () => {
  it('lands nobody on Italian home soil before April 1943, the islands included', () => {
    const g = war(USUAL - 1, 'UK');
    for (const name of ['Sicily', 'Sardinia', ...SOUTH]) expect(open(g, name), name).toBe(false);
    g.turn = USUAL;
    for (const name of ['Sicily', 'Sardinia', ...SOUTH]) expect(open(g, name), name).toBe(true);
  });

  it('nor while an Axis army holds any of the African shore facing it, whoever\'s flag flies over the rest', () => {
    // A neutral there is no bar (no computer staff fights Vichy for it).
    const g = war(USUAL + 6, 'ESP');
    expect(atWar(g, 'ESP', 'ITA')).toBe(false);
    expect(open(g, 'Sicily')).toBe(true);
    // Germany in one of the three: shut. Italy itself: shut.
    setWar(g, 'GER', 'UK');
    g.byName[FACING[0]].ctrl = 'GER';
    expect(open(g, 'Sicily')).toBe(false);
    expect(open(g, 'Naples')).toBe(false);
    g.byName[FACING[0]].ctrl = 'ITA';
    expect(open(g, 'Sicily')).toBe(false);
    // An Axis corps standing there under another flag: shut.
    g.byName[FACING[0]].ctrl = 'ESP';
    expect(open(g, 'Sicily')).toBe(true);
    const c = g.corps.find((x) => x.nat === 'ITA' && !x.dead && x.kind !== 'fort')!;
    g.byName[FACING[0]].units.push(c);
    expect(open(g, 'Sicily')).toBe(false);
    g.byName[FACING[0]].units.pop();
    // In Allied hands: open.
    for (const name of FACING) g.byName[name].ctrl = 'UK';
    expect(open(g, 'Sicily')).toBe(true);
  });

  it('is a rule for the first crossing: spent once an enemy of Italy stands on its home ground', () => {
    const g = war(USUAL + 6, 'ITA');
    expect(open(g, 'Naples')).toBe(false);
    // The Red Army comes over the Alps.
    g.byName.Friuli.ctrl = 'UK';
    expect(open(g, 'Naples')).toBe(true);
    expect(open(g, 'Sicily')).toBe(true);
    g.byName.Friuli.ctrl = 'ITA';
    expect(open(g, 'Naples')).toBe(false);
    // The month is not spent with it.
    g.byName.Friuli.ctrl = 'UK';
    g.turn = USUAL - 1;
    expect(open(g, 'Naples')).toBe(false);
  });

  it('leaves Italy\'s colonies and every other shore as they were, and the Strait to the staff', () => {
    const g = war(USUAL - 10, 'GER');
    for (const r of g.regions) {
      if (r.owner === 'ITA' && !r.overseas) continue;
      expect(shoreOpen(g, r.owner, !!r.overseas, r.name), r.name).toBe(true);
    }
    expect(g.byName.Tripoli.overseas).toBe(true);
    expect(open(g, 'Tripoli')).toBe(true);
    expect(straitOpen(g, { nat: 'UK' }, g.byName.Sicily, g.byName.Calabria)).toBe(true);
  });
});

describe('by the book (stops 1 and 2)', () => {
  it('Sicily and Sardinia from July 1943, the mainland from September', () => {
    const g = war(RULE.from - 1, 'UK', 1);
    expect(open(g, 'Sicily')).toBe(false);
    expect(open(g, 'Sardinia')).toBe(false);
    g.turn = RULE.from;
    expect(open(g, 'Sicily')).toBe(true);
    expect(open(g, 'Sardinia')).toBe(true);
    for (const name of ['Calabria', 'Apulia', 'Naples', 'Anzio']) expect(open(g, name), name).toBe(false);
    g.turn = MAINLAND - 1;
    expect(open(g, 'Naples')).toBe(false);
    g.turn = MAINLAND;
    for (const name of ['Calabria', 'Apulia', 'Naples', 'Anzio']) expect(open(g, name), name).toBe(true);
    // (Asked without a province, the shore's own month answers, as before.)
    g.turn = RULE.from;
    expect(shoreOpen(g, 'ITA', false)).toBe(true);
  });

  it('and not while an Axis army holds the African shore, until Italy is entered', () => {
    const g = war(RULE.from, 'ITA', 1);
    expect(open(g, 'Sicily')).toBe(false);
    g.byName.Sardinia.ctrl = 'UK';
    expect(open(g, 'Sicily')).toBe(true);
    // The book's month for the mainland still stands.
    expect(open(g, 'Naples')).toBe(false);
    g.turn = MAINLAND;
    expect(open(g, 'Naples')).toBe(true);
  });

  it('the march over the Strait is a crossing too: no computer corps goes from Sicily to Calabria before September', () => {
    const g = war(MAINLAND - 1, 'UK', 1);
    const [sicily, calabria] = [g.byName.Sicily, g.byName.Calabria];
    sicily.ctrl = 'UK';
    const foot = (nat: string): Corps => g.corps.find((c) => c.nat === nat && !c.dead && c.kind === 'inf')!;
    expect(blocked(g, foot('UK'), calabria, sicily.id)).toBe(true);
    // Not the other way, not for Italy's own side, and not from anywhere else.
    expect(blocked(g, foot('UK'), sicily, calabria.id)).toBe(false);
    expect(blocked(g, foot('ITA'), calabria, sicily.id)).toBe(false);
    expect(blocked(g, foot('UK'), calabria, g.byName.Apulia.id)).toBe(false);
    g.turn = MAINLAND;
    expect(blocked(g, foot('UK'), calabria, sicily.id)).toBe(false);
    // A player's own corps marches when he likes, and so does an ally's that marches with his army.
    const mine = war(MAINLAND - 1, 'UK', 1, 'UK');
    const c = mine.corps.find((x) => x.nat === 'UK' && !x.dead && x.kind === 'inf')!;
    expect(blocked(mine, c, mine.byName.Calabria, mine.byName.Sicily.id)).toBe(false);
    expect(straitOpen(mine, { nat: 'USA' }, mine.byName.Sicily, mine.byName.Calabria)).toBe(true); // (America is not at war with Italy here)
    setWar(mine, 'ITA', 'USA');
    expect(straitOpen(mine, { nat: 'USA' }, mine.byName.Sicily, mine.byName.Calabria)).toBe(false);
    expect(straitOpen(mine, { nat: 'USA', joint: 'UK' }, mine.byName.Sicily, mine.byName.Calabria)).toBe(true);
  });

  it('within the stop\'s slack at stop 2, the mainland two months behind the islands', () => {
    for (const seed of [3, 4, 5, 6]) {
      const g = startOpen(createGame(seed, null));
      expect(setStop(g, 2, true)).toBe(true);
      setWar(g, 'ITA', 'UK');
      let isle = 0, main = 0, strait = 0;
      for (let t = RULE.from - 2; t <= MAINLAND + 2; t++) {
        g.turn = t;
        if (!isle && open(g, 'Sicily')) isle = t;
        if (!main && open(g, 'Naples')) main = t;
        if (!strait && straitOpen(g, { nat: 'UK' }, g.byName.Sicily, g.byName.Calabria)) strait = t;
      }
      expect(Math.abs(isle - RULE.from), `seed ${seed}`).toBeLessThanOrEqual(1);
      expect(main - isle).toBe(MAINLAND - RULE.from);
      expect(strait).toBe(main);
    }
  });

  it('once the book is closed on the second front, the usual game\'s rule holds instead', () => {
    const g = war(RULE.from - 1, 'UK', 1);
    expect(open(g, 'Sicily')).toBe(false);
    expect(straitOpen(g, { nat: 'UK' }, g.byName.Sicily, g.byName.Calabria)).toBe(false);
    g.flags['hist:off:second'] = g.turn;
    expect(open(g, 'Sicily')).toBe(true);
    expect(open(g, 'Naples')).toBe(true);
    expect(straitOpen(g, { nat: 'UK' }, g.byName.Sicily, g.byName.Calabria)).toBe(true);
    g.turn = USUAL - 1;
    expect(open(g, 'Sicily')).toBe(false);
  });
});

describe('who is not bound', () => {
  it('the staffs at the two loosest stops, and every history campaign', () => {
    for (const stop of [4, 5]) {
      const g = war(20, 'GER', stop);
      setWar(g, 'GER', 'UK');
      expect(open(g, 'Sicily'), `stop ${stop}`).toBe(true);
      expect(open(g, 'Naples'), `stop ${stop}`).toBe(true);
      expect(straitOpen(g, { nat: 'UK' }, g.byName.Sicily, g.byName.Calabria)).toBe(true);
    }
    const s = createGame(1, null);
    s.turn = 20;
    expect(shoreOpen(s, 'ITA', false, 'Sicily')).toBe(true);
    expect(shoreOpen(s, 'ITA', false, 'Naples')).toBe(true);
    expect(straitOpen(s, { nat: 'UK' }, s.byName.Sicily, s.byName.Calabria)).toBe(true);
  });
});

/**
 * Ground that open.json's own lists say is not the Italian mainland, whoever owns it (descent.planned 'not'): not home
 * soil here. (Since the computer's air forces fight back, airwar.json ai.back, seed 17's Greek army counter-attacks
 * overland into Albania in month 19; the design director, 24 Nov 2026 20:00Z: the rule working, read from open.json.)
 */
const NOT_HOME = new Set(Object.values((openData as unknown as { descent: { planned: Record<string, { not?: string[] }> } }).descent.planned).flatMap((p) => p.not ?? []));

describe('a computer-run grand campaign', () => {
  let first = 0, who = '';
  beforeAll(() => {
    const g = grand(null, 17);
    const onSoil = (): string => {
      for (const r of g.regions as Region[]) {
        if (r.owner !== 'ITA' || r.overseas || NOT_HOME.has(r.name)) continue;
        const u = r.units.find((c) => !c.dead && c.kind !== 'fort' && atWar(g, 'ITA', c.nat));
        if (u) return u.nat;
        if (atWar(g, 'ITA', r.ctrl)) return r.ctrl;
      }
      return '';
    };
    while (g.turn < USUAL) {
      runTurnAI(g);
      const by = onSoil();
      if (by && !first) { first = g.turn; who = by; }
    }
  }, 240000);

  it('sees no Allied army on Italian home soil before April 1943 (in Version 86 the Americans took Sardinia in November 1942)', () => {
    expect(`${who} ${first}`).toBe(' 0');
  });
});
