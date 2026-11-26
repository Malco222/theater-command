// History's dates for the western Allies' own landings (Version 79; the design director's ruling 60, on the staff
// report card of Version 77: America's staff landed in France in 1943 in half the wars and lost 22 corps of 54).
//
// For a computer staff opening a front of its own. A player lands when he likes, and joining a front already held
// stays as built.
// 1. America, at the stops of the history dial up to "History's shape": the Italian mainland not before July 1943,
//    France not before May 1944. After its date the build-up and the forecast still decide. Islands and colonies are
//    open always. France's date is set aside while an enemy of Germany's stands on German home ground. At the two
//    loosest stops there are no dates.
// 2. Canada: its corps are shipped to Britain and stay there as the island's guard. They go on to the continent not
//    before January 1943, and only behind a British or American army.
// 3. Britain gets a list at those stops: the Italian mainland from July 1943, France from May 1944, Greece from
//    October 1944. Not Norway, Denmark or Albania.
import { describe, expect, it } from 'vitest';
import { landingWatch } from '../src/campaign/ai';
import { addCorps, atWar, capture, createGame, declareWar, removeCorps } from '../src/campaign/game';
import { HISTORY, setStop, stopDef } from '../src/campaign/history';
import { loadGame, saveGame } from '../src/campaign/save';
import { FAR_STAGE } from '../src/campaign/stage';
import { builtUp, frontAshore, joinsOn, massing, oceanRuns, opensOn, seaFoe, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { type Corps, type Game, type Region } from '../src/campaign/types';
import { beyondEurope } from '../src/campaign/world';
import open from '../data/campaign/open.json';

interface List { open: string[]; join?: string[]; with?: number; from?: Record<string, number>; lapse?: Record<string, string>; not?: string[]; book?: boolean; behind?: string[]; base?: string[]; joinFrom?: number }
const PLANNED = (open as unknown as { descent: { planned: Record<string, List> } }).descent.planned;
const JAN_42 = 29, DEC_42 = 40, JAN_43 = 41, JUN_43 = 46, JUL_43 = 47, APR_44 = 56, MAY_44 = 57, SEP_44 = 61, OCT_44 = 62;
const wide = (seed = 1, human: string | null = null, stop = 3): Game => { const g = startOpen(createGame(seed, human)); if (stop !== 3) expect(setStop(g, stop, true)).toBe(true); return g; };
const R = (g: Game, name: string): Region => { const r = g.byName[name]; expect(r, name).toBeDefined(); return r; };
const field = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
/** True while an enemy of Germany's stands on German home ground, or holds any of it. */
const germanyEntered = (g: Game): boolean => g.regions.some((r) => r.owner === 'GER' && !r.overseas && (atWar(g, 'GER', r.ctrl) || r.units.some((u) => !u.dead && atWar(g, 'GER', u.nat))));
/** A province taken for a country, with nobody else's corps left on it. */
const hold = (g: Game, name: string, nat: string): Region => { const r = R(g, name); for (const c of [...r.units]) removeCorps(g, c); capture(g, r, nat); return r; };

/** One war on the wider world with nobody playing, to October 1944: every landing a staff made, and month by month where Canada's corps stood. */
const war: Record<number, string> = {};
const landings: { turn: number; nat: string; to: string; owner: string; island: boolean; overseas: boolean; wave: boolean; entered: boolean }[] = [];
const canada: { turn: number; across: number; inBritain: number; where: string[] }[] = [];
{
  // Seed 8 (seed 7 until Version 84, whose rules of what an attack brings and faces moved every war): nobody stands on
  // Germany's ground in January 1943. (America's own landing was Marseille in June 1944 until ruling 227; since then it
  // is Sardinia in July 1943.)
  const g = wide(8);
  landingWatch.tell = (e) => { landings.push({ turn: g.turn, nat: e.nat, to: e.to.name, owner: e.to.owner, island: e.to.nb.length === 0, overseas: e.to.overseas, wave: e.wave, entered: germanyEntered(g) }); };
  while (g.turn < OCT_44) {
    runTurnAI(g);
    if ([JAN_42, DEC_42, JAN_43].includes(g.turn)) war[g.turn] = saveGame(g);
    const over = field(g, 'CAN').filter((c) => !beyondEurope(g.regions[c.reg]));
    canada.push({ turn: g.turn, across: over.length, inBritain: over.filter((c) => g.regions[c.reg].owner === 'UK' && !g.regions[c.reg].overseas).length, where: over.map((c) => g.regions[c.reg].name) });
  }
  landingWatch.tell = null;
}
const at = (turn: number): Game => loadGame(war[turn])!;

describe('the lists as history wrote them (ruling 60)', () => {
  it('America and Britain have dates for the Italian mainland and for France, Britain for Greece too, and Canada waits behind them', () => {
    expect(PLANNED.USA.open).toEqual(['FRA', 'ITA']);
    expect(PLANNED.USA.from).toEqual({ ITA: JUL_43, FRA: MAY_44 });
    expect(PLANNED.USA.lapse).toEqual({ FRA: 'GER' });
    expect(PLANNED.USA.with).toBe(9);
    expect(PLANNED.UK.open).toEqual(['ITA', 'FRA', 'GRE']);
    expect(PLANNED.UK.from).toEqual({ ITA: JUL_43, FRA: MAY_44, GRE: OCT_44 });
    expect(PLANNED.UK.lapse).toEqual({ FRA: 'GER' });
    expect(PLANNED.UK.book).toBe(true);
    expect(PLANNED.UK.join).toBeUndefined();
    for (const id of ['NOR', 'DEN', 'ALB']) expect(PLANNED.UK.open).not.toContain(id);
    for (const id of ['USA', 'UK']) expect(PLANNED[id].not).toEqual(['Albania']);
    expect(PLANNED.CAN.open).toEqual([]);
    expect(PLANNED.CAN.base).toEqual(['UK']);
    expect(PLANNED.CAN.behind).toEqual(['UK', 'USA']);
    expect(PLANNED.CAN.joinFrom).toBe(JAN_43);
    // The dates hold at the stops up to "History's shape", and not where neutrals have ambitions of their own.
    expect(HISTORY.stops.map((s) => !s.ambitions)).toEqual([true, true, true, false, false]);
  });
});

describe('America\'s staff waits for history\'s dates', () => {
  it('the Italian mainland not before July 1943, France not before May 1944, at each stop up to History\'s shape', () => {
    for (const stop of [1, 2, 3]) {
      const g = wide(1, null, stop);
      expect(stopDef(g).stop, `stop ${stop}`).toBe(stop);
      for (const [name, from] of [['Naples', JUL_43], ['Apulia', JUL_43], ['Sicily', JUL_43], ['Brittany', MAY_44], ['Bordeaux', MAY_44], ['Calais', MAY_44]] as const) {
        for (const turn of [1, JAN_43, from - 1]) { g.turn = turn; expect(opensOn(g, 'USA', R(g, name)), `${name}, month ${turn}, stop ${stop}`).toBe(false); }
        for (const turn of [from, from + 1, 80]) { g.turn = turn; expect(opensOn(g, 'USA', R(g, name)), `${name}, month ${turn}, stop ${stop}`).toBe(true); }
      }
    }
  });

  it('islands and colonies are open always, and what was never on the list stays off it', () => {
    const g = wide();
    for (const turn of [1, JAN_42, JUN_43, APR_44, MAY_44]) {
      g.turn = turn;
      // Sardinia and Corsica are islands taken whole (no land frontier); Tripoli and Rhodes are colonies overseas.
      for (const name of ['Sardinia', 'Corsica']) { expect(R(g, name).nb, name).toEqual([]); expect(opensOn(g, 'USA', R(g, name)), `${name}, month ${turn}`).toBe(true); }
      for (const name of ['Tripoli', 'Rhodes', 'Algiers']) { expect(R(g, name).overseas, name).toBe(true); expect(opensOn(g, 'USA', R(g, name)), `${name}, month ${turn}`).toBe(true); }
      for (const name of ['Bergen', 'Holland', 'Hamburg', 'Athens', 'Albania']) expect(opensOn(g, 'USA', R(g, name)), `${name}, month ${turn}`).toBe(false);
    }
    // Sicily has a land frontier (the strait): it is dated with the mainland, to the month of history's own landing there.
    expect(R(g, 'Sicily').nb.length).toBeGreaterThan(0);
    expect(R(g, 'Albania').owner).toBe('ITA');
  });

  it('after its date the build-up still decides', () => {
    const g = at(JAN_42);
    g.turn = MAY_44;
    const few = field(g, 'USA').filter((c) => !beyondEurope(g.regions[c.reg])).length;
    expect(few).toBeLessThan(PLANNED.USA.with!);
    expect(opensOn(g, 'USA', R(g, 'Brittany'))).toBe(true);
    expect(builtUp(g, 'USA', R(g, 'Brittany'))).toBe(false);
  });

  it('France\'s date is set aside while an enemy of Germany\'s stands on German home ground; Italy\'s is not', () => {
    const g = wide();
    g.turn = JAN_43;
    if (!atWar(g, 'GER', 'UK')) declareWar(g, 'UK', 'GER');
    expect(germanyEntered(g)).toBe(false);
    expect(opensOn(g, 'USA', R(g, 'Brittany'))).toBe(false);
    // A British corps on German ground, which Germany still holds.
    const east = R(g, 'Konigsberg');
    for (const u of [...east.units]) removeCorps(g, u);
    const c = addCorps(g, 'UK', 'Test Corps', 'inf', 2, east.id, {});
    expect(c.reg).toBe(east.id);
    expect(east.ctrl).toBe('GER');
    expect(germanyEntered(g)).toBe(true);
    expect(opensOn(g, 'USA', R(g, 'Brittany'))).toBe(true);
    expect(opensOn(g, 'UK', R(g, 'Brittany'))).toBe(true);
    g.turn = JUN_43;
    expect(opensOn(g, 'USA', R(g, 'Naples'))).toBe(false);
    expect(opensOn(g, 'UK', R(g, 'Naples'))).toBe(false);
    // Thrown out again, the date stands again.
    removeCorps(g, c);
    expect(opensOn(g, 'USA', R(g, 'Brittany'))).toBe(false);
    // German ground in an enemy's hands counts with nobody standing on it; a colony or a conquest of Germany's does not.
    capture(g, R(g, 'Memel'), 'UK');
    expect(opensOn(g, 'USA', R(g, 'Brittany'))).toBe(true);
    capture(g, R(g, 'Memel'), 'GER');
    hold(g, 'Jutland', 'GER');
    addCorps(g, 'UK', 'Test Corps 2', 'inf', 2, 'Jutland', {});
    expect(opensOn(g, 'USA', R(g, 'Brittany'))).toBe(false);
  });

  it('at the two loosest stops there are no dates; where nobody has read the books there is no list', () => {
    const loose = wide(1, null, 4);
    for (const name of ['Brittany', 'Naples', 'Sicily']) expect(opensOn(loose, 'USA', R(loose, name)), name).toBe(true);
    expect(opensOn(loose, 'USA', R(loose, 'Bergen'))).toBe(false);
    expect(opensOn(loose, 'USA', R(loose, 'Albania'))).toBe(false);
    const any = wide(1, null, 5);
    for (const name of ['Brittany', 'Naples', 'Bergen', 'Albania']) expect(opensOn(any, 'USA', R(any, name)), name).toBe(true);
  });
});

describe('Britain\'s list', () => {
  it('the Italian mainland from July 1943, France from May 1944, Greece from October 1944; not Norway, Denmark or Albania', () => {
    for (const stop of [1, 2, 3]) {
      const g = wide(1, null, stop);
      for (const [name, from] of [['Naples', JUL_43], ['Sicily', JUL_43], ['Brittany', MAY_44], ['Normandy', MAY_44], ['Athens', OCT_44], ['Salonika', OCT_44]] as const) {
        for (const turn of [1, from - 1]) { g.turn = turn; expect(opensOn(g, 'UK', R(g, name)), `${name}, month ${turn}, stop ${stop}`).toBe(false); }
        for (const turn of [from, 80]) { g.turn = turn; expect(opensOn(g, 'UK', R(g, name)), `${name}, month ${turn}, stop ${stop}`).toBe(true); }
      }
      for (const turn of [1, JUL_43, OCT_44, 80]) {
        g.turn = turn;
        for (const name of ['Bergen', 'Oslo', 'Jutland', 'Albania', 'Holland', 'Hamburg']) expect(opensOn(g, 'UK', R(g, name)), `${name}, month ${turn}, stop ${stop}`).toBe(false);
        // Islands and colonies at any time: Crete, Sardinia and Corsica taken whole, Rhodes and Libya overseas.
        for (const name of ['Crete', 'Sardinia', 'Corsica', 'Rhodes', 'Tripoli', 'Benghazi']) expect(opensOn(g, 'UK', R(g, name)), `${name}, month ${turn}, stop ${stop}`).toBe(true);
        // It goes by ship to any front its side holds, as before, and no build-up is asked of it.
        for (const name of ['Bergen', 'Holland', 'Brittany', 'Athens']) { expect(joinsOn(g, 'UK', R(g, name)), name).toBe(true); expect(builtUp(g, 'UK', R(g, name)), name).toBe(true); }
      }
    }
  });

  it('the list is kept by the book only: at the two loosest stops Britain\'s staff goes where it judges best', () => {
    for (const stop of [4, 5]) {
      const g = wide(1, null, stop);
      for (const name of ['Bergen', 'Jutland', 'Albania', 'Brittany', 'Naples', 'Athens', 'Holland']) expect(opensOn(g, 'UK', R(g, name)), `${name}, stop ${stop}`).toBe(true);
    }
    // And every other staff has none at any stop.
    const g = wide();
    for (const nat of ['GER', 'ITA', 'SOV', 'FRA']) for (const name of ['Bergen', 'Brittany', 'Albania', 'Athens']) expect(opensOn(g, nat, R(g, name)), `${nat} ${name}`).toBe(true);
  });
});

describe('Canada\'s corps are the island\'s guard', () => {
  it('they go on to the continent not before January 1943, and only behind a British or American army', () => {
    const g = at(DEC_42);
    expect(g.turn).toBe(DEC_42);
    for (const k of Object.keys(g.flags)) if (k.startsWith('st:lodged:')) delete g.flags[k];
    // Britain's own harbours are its base at any time.
    for (const name of ['Northumbria', 'Hampshire', 'Kent']) expect(joinsOn(g, 'CAN', R(g, name)), name).toBe(true);
    // A colony overseas is open to her at any time on the older chart. With the whole world on it the island's guard
    // holds against that too (ruling 231, 3, and its rider b462): Tripoli is no harbour of hers, whoever holds it.
    expect(joinsOn(g, 'CAN', R(g, 'Tripoli'))).toBe(!FAR_STAGE);
    expect(joinsOn(g, 'CAN', R(g, 'Tripoli'), 'UK')).toBe(!FAR_STAGE);
    expect(joinsOn(g, 'USA', R(g, 'Tripoli'))).toBe(true);
    // December 1942: a harbour in France in British hands with the Germans next door is Britain's front, and America's, and not yet Canada's.
    const brest = hold(g, 'Brittany', 'UK');
    expect(brest.nb.some((j) => atWar(g, 'CAN', g.regions[j].ctrl))).toBe(true);
    expect(joinsOn(g, 'CAN', brest)).toBe(false);
    expect(joinsOn(g, 'CAN', brest, 'UK')).toBe(false);
    expect(frontAshore(g, 'USA')!.ports).toContain(brest);
    expect(frontAshore(g, 'CAN')?.ports ?? []).not.toContain(brest);
    // January 1943: now it is.
    g.turn = JAN_43;
    expect(joinsOn(g, 'CAN', brest)).toBe(true);
    expect(frontAshore(g, 'CAN')!.ports).toContain(brest);
    // A harbour France holds with no British or American corps near it is not behind them.
    capture(g, brest, 'FRA');
    for (const j of brest.nb) for (const c of [...g.regions[j].units]) if (c.nat === 'UK' || c.nat === 'USA') removeCorps(g, c);
    expect(joinsOn(g, 'CAN', brest)).toBe(false);
    // The landing of one of them is, whoever holds the quay today; so is a harbour with a corps of theirs in it or next to it.
    expect(joinsOn(g, 'CAN', brest, 'USA')).toBe(true);
    expect(joinsOn(g, 'CAN', brest, 'FRA')).toBe(false);
    const c = addCorps(g, 'USA', 'Test Corps', 'inf', 2, brest.id, {});
    expect(joinsOn(g, 'CAN', brest)).toBe(true);
    removeCorps(g, c);
    expect(joinsOn(g, 'CAN', brest)).toBe(false);
    // Ground that was never on its list stays off it, and a colony overseas is open as before (with the whole world on
    // the chart it stays shut to her for good, in 1943 as before it: her army never went to Africa as a formation).
    expect(joinsOn(g, 'CAN', R(g, 'Bergen'), 'UK')).toBe(false);
    expect(joinsOn(g, 'CAN', R(g, 'Tripoli'))).toBe(!FAR_STAGE);
    expect(joinsOn(g, 'CAN', R(g, 'Tripoli'), 'UK')).toBe(!FAR_STAGE);
  });

  it('at the two loosest stops Canada keeps no date, and goes only into a British or American landing that is ashore to stay (Version 83)', () => {
    for (const stop of [4, 5]) {
      const g = wide(1, null, stop);
      // A harbour an ally merely holds is no longer enough (ruling 89(e); the landing ashore to stay: strength-83.test.ts).
      expect(joinsOn(g, 'CAN', R(g, 'Holland')), `stop ${stop}`).toBe(false);
      expect(joinsOn(g, 'CAN', R(g, 'Brittany')), `stop ${stop}`).toBe(false);
      // Its base in Britain is open as ever, and so is a colony overseas.
      expect(joinsOn(g, 'CAN', R(g, 'Kent')), `stop ${stop}`).toBe(true);
      expect(joinsOn(g, 'CAN', R(g, 'Tripoli')), `stop ${stop}`).toBe(true);
    }
  });

  it('its army is shipped to Britain\'s own harbours and nowhere else', () => {
    const g = at(JAN_42);
    const runs = oceanRuns(g, 'CAN');
    expect(runs.length).toBe(1);
    for (const p of runs[0].to) expect(p.owner === 'UK' && !p.overseas, p.name).toBe(true);
  });

  it('in a war nobody plays every Canadian corps across the ocean stands in Britain until 1943', () => {
    const early = canada.filter((m) => m.turn < JAN_43);
    expect(early.some((m) => m.across > 0)).toBe(true);
    for (const m of early) expect(m.inBritain, `month ${m.turn}: ${m.where.join(', ')}`).toBe(m.across);
  });
});

describe('the war nobody plays', () => {
  it('no staff of the western Allies opens a front of its own before history\'s date for that shore', () => {
    const mine = landings.filter((l) => (l.nat === 'USA' || l.nat === 'UK') && !l.wave && !l.island && !l.overseas);
    for (const l of mine) {
      const what = `${l.nat} at ${l.to} (${l.owner}), month ${l.turn}`;
      expect(['FRA', 'ITA', ...(l.nat === 'UK' ? ['GRE'] : [])], what).toContain(l.owner);
      expect(l.to, what).not.toBe('Albania');
      if (l.owner === 'ITA') expect(l.turn, what).toBeGreaterThanOrEqual(JUL_43);
      if (l.owner === 'GRE') expect(l.turn, what).toBeGreaterThanOrEqual(OCT_44);
      if (l.owner === 'FRA' && !l.entered) expect(l.turn, what).toBeGreaterThanOrEqual(MAY_44);
    }
    // Canada's makes none on a mainland.
    expect(landings.filter((l) => l.nat === 'CAN' && !l.island && !l.overseas)).toEqual([]);
  });
});

describe('a staff that waits for its date', () => {
  it('picks no enemy for a crossing whose harbours in reach are all closed to it this month', () => {
    // January 1943 in the war nobody plays: Germany holds the Channel coast, and France's month has not come.
    const g = at(JAN_43);
    expect(germanyEntered(g)).toBe(false);
    // (A quiet year on land with Germany, as when this war was first measured. Since ruling 227 the desert war runs on
    // into the autumn of 1942 in every war of seeds 1 to 13, and a staff still fighting him on land looks for no
    // crossing whatever its list says: so the year is made quiet here, and only the list is left to tell the two apart.
    // Italy's war is kept alive the other way: since ruling 229 Britain clears the desert by the summer of 1941 in this
    // war, and Sardinia, an island, is on no list's dates, so a quiet half-year with Italy would make it the enemy.)
    const quiet = (x: Game): void => {
      x.nations.UK.lastFight.GER = Math.min(x.nations.UK.lastFight.GER ?? 0, JAN_43 - 12); x.nations.GER.lastFight.UK = Math.min(x.nations.GER.lastFight.UK ?? 0, JAN_43 - 12);
      x.nations.UK.lastFight.ITA = JAN_43 - 1; x.nations.ITA.lastFight.UK = JAN_43 - 1;
    };
    quiet(g);
    for (const k of ['st:foe:UK', 'st:foeAt:UK']) delete g.flags[k];
    expect(seaFoe(g, 'UK')).toBeNull();
    // Where the list is put away the same staff, the same month, has an enemy across the water in its eye: Germany
    // across the Channel, or, since ruling 229, Finland, at war with Britain beside Germany in this war and never yet
    // fought, so the quietest enemy of all. Neither is on the list's dates in January 1943.
    const loose = at(JAN_43);
    quiet(loose);
    for (const k of ['st:foe:UK', 'st:foeAt:UK']) delete loose.flags[k];
    expect(setStop(loose, 4)).toBe(true);
    expect(['GER', 'FIN']).toContain(seaFoe(loose, 'UK')?.foe);
  });

  it('is not fighting a war nobody can fight: it will come that month', () => {
    // Britain in January 1942: Germany holds the harbours of France, and France's month has not come.
    const g = at(JAN_42);
    expect(g.regions.some((r) => r.owner === 'FRA' && !r.overseas && r.port && r.ctrl === 'GER')).toBe(true);
    expect(massing(g, 'UK')).toBe(true);
    // From May 1944 it waits only for Greece's month, where an enemy holds a harbour of the Greek mainland; from October for nothing.
    g.turn = MAY_44;
    expect(massing(g, 'UK')).toBe(g.regions.some((r) => r.owner === 'GRE' && r.nb.length > 0 && r.port && r.seas.length > 0 && atWar(g, 'UK', r.ctrl)));
    g.turn = OCT_44;
    expect(massing(g, 'UK')).toBe(false);
    // America with its army gathered in Britain waits too, where it used to be free to sign after a year.
    const a = at(JAN_42);
    for (let k = field(a, 'USA').filter((c) => !beyondEurope(a.regions[c.reg])).length; k < PLANNED.USA.with!; k++) addCorps(a, 'USA', `Gathered Corps ${k}`, 'inf', 2, 'Cornwall', {});
    expect(builtUp(a, 'USA', R(a, 'Brittany'))).toBe(true);
    for (const turn of [JAN_42, JAN_43, APR_44]) { a.turn = turn; expect(massing(a, 'USA'), `month ${turn}`).toBe(true); }
    a.turn = MAY_44;
    expect(massing(a, 'USA')).toBe(false);
    // With nobody across the ocean it is not gathering and not waiting, as before; and where there are no dates, nobody waits.
    const none = at(JAN_42);
    for (const c of field(none, 'USA').filter((x) => !beyondEurope(none.regions[x.reg]))) removeCorps(none, c);
    expect(massing(none, 'USA')).toBe(false);
    const loose = at(JAN_42);
    expect(setStop(loose, 4)).toBe(true);
    expect(massing(loose, 'UK')).toBe(false);
  });
});

describe('a player is never bound', () => {
  it('the lists are a computer staff\'s: a player\'s America or Britain has no staff crossing the sea for it', () => {
    for (const nat of ['USA', 'UK']) {
      const g = wide(1, nat);
      g.turn = SEP_44;
      expect(seaFoe(g, nat)).toBeNull();
      expect(seaFoe(g, nat, 1)).toBeNull();
    }
  });
});
