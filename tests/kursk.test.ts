// Kursk and the road to the Dnieper: an event that remakes a nation's army, armies formed late in the war
// that do not hold up its factories, defence lines dug by the calendar, a plan that allows one assault and
// no more, and the orders that take the German army back to the Dnieper and the Red Army after it.
import { describe, expect, it } from 'vitest';
import type { Corps, Game, PlanPhase, Region } from '../src/campaign/types';
import { addCorps, assess, capture, limitAttackers, removeCorps, startPhase } from '../src/campaign/game';
import { phaseFor } from '../src/campaign/ai';
import { generalOf } from '../src/campaign/commanders';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { canBuildYet } from '../src/campaign/economy';
import { loadGame, saveGame } from '../src/campaign/save';
import { createGame } from '../src/campaign/game';
import { current, runTurnAI, stepAI } from '../src/campaign/turn';
import setup from '../data/campaign/setup-1939.json';

const MAY_43 = 45, JUNE_43 = 46, JULY_43 = 47, SEPT_43 = 49, DEC_43 = 52;
const OLD_ARMY = { quality: 0.7, slowCommand: true, maxAttack: 8, air: 70 };
const NEW_ARMY = { quality: 0.8, slowCommand: false, maxAttack: 14, air: 100 };
/** The ground east of the Dnieper that the German plan gives up in September 1943. */
const GIVEN_UP = ['Kursk', 'Orel', 'Kharkov', 'Sumy', 'Poltava', 'Stalino', 'Voroshilovgrad', 'Novorossiysk'];

/** One war on the historical plan, remembered as each of these months opens, with Germany about to move. */
const opening: Record<number, string> = {};
{
  const g = createGame(3, null);
  for (let t = 1; t < DEC_43; t++) {
    runTurnAI(g);
    if ([MAY_43, JUNE_43, JULY_43, SEPT_43, DEC_43].includes(t + 1)) opening[t + 1] = saveGame(g);
  }
}
/** A fresh copy of the war as that month opens, for a test to play on from. */
const at = (t: number): Game => loadGame(opening[t])!;
/** Lets the computer play until it is this nation's move. */
const until = (g: Game, nat: string): void => { let guard = 0; while (current(g) !== nat && guard++ < 60) stepAI(g); };
/** Plays this nation's month and stops there. */
const play = (g: Game, nat: string): void => { until(g, nat); stepAI(g); };

const named = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && !c.dead)!;
const place = (g: Game, name: string): string => g.regions[named(g, name).reg].name;
const field = (r: Region, nat: string): Corps[] => r.units.filter((u) => u.nat === nat && u.kind !== 'fort' && !u.hold);
const told = (g: Game, turn: number): string[] => g.headlines.filter((h) => h.turn === turn).map((h) => h.text);
/** The battles a nation opened in the month it has just played. */
const fought = (g: Game, nat: string, from: number): { place: string; taken: boolean; corps: number }[] =>
  g.reports.slice(from).filter((r) => r.attNat === nat).map((r) => ({ place: r.regionName, taken: r.taken, corps: r.attackers.length }));

describe('the Red Army of 1943', () => {
  it('is still the army of 1941 in May 1943: green troops, a slow command, small attacks', () => {
    const g = at(MAY_43);
    expect(g.nations.SOV).toMatchObject(OLD_ARMY);
    expect(g.flags['event:red-army-1943']).toBeUndefined();
  });

  it('is remade as June 1943 opens, and the newsreel says so once', () => {
    const g = at(JUNE_43);
    expect(g.nations.SOV).toMatchObject(NEW_ARMY);
    expect(g.flags['event:red-army-1943']).toBe(JUNE_43);
    expect(told(g, JUNE_43).filter((t) => /remade the Red Army/.test(t))).toHaveLength(1);
    // Nobody else's army is touched.
    expect(g.nations.GER).toMatchObject({ quality: 1.25, air: 100 });
  });

  it('stays remade in a saved game', () => {
    const g = loadGame(saveGame(at(JUNE_43)))!;
    expect(g.nations.SOV).toMatchObject(NEW_ARMY);
    // And is not remade a second time: the event has fired.
    g.nations.SOV.quality = 0.75;
    monthlyDiplomacy(g);
    expect(g.nations.SOV.quality).toBe(0.75);
  });

  it('a campaign saved after June 1943 by an older version still gets the new army at its next month', () => {
    const g = at(JULY_43);
    delete g.flags['event:red-army-1943'];
    Object.assign(g.nations.SOV, OLD_ARMY);
    monthlyDiplomacy(g);
    expect(g.nations.SOV).toMatchObject(NEW_ARMY);
  });

  it('does not come to a Soviet Union that is no longer at war with Germany', () => {
    const g = at(MAY_43);
    g.nations.SOV.atWar = g.nations.SOV.atWar.filter((x) => x !== 'GER');
    g.nations.GER.atWar = g.nations.GER.atWar.filter((x) => x !== 'SOV');
    g.turn = JUNE_43;
    monthlyDiplomacy(g);
    expect(g.nations.SOV).toMatchObject(OLD_ARMY);
    expect(g.flags['event:red-army-1943']).toBeUndefined();
  });

  it('its attacks are no longer cut by a slow command', () => {
    const g = at(JUNE_43);
    const orel = g.byName['Orel'];
    const pool = g.corps.filter((c) => c.nat === 'SOV' && !c.dead && c.kind !== 'fort' && orel.nb.includes(c.reg)).slice(0, 4);
    expect(pool.length).toBeGreaterThan(0);
    const now = assess(g, pool, orel).A;
    g.nations.SOV.slowCommand = true;
    expect(assess(g, pool, orel).A / now).toBeCloseTo(0.85, 3);
  });

  it('can send up to fourteen corps into one battle where it could send eight', () => {
    const g = at(JUNE_43);
    const orel = g.byName['Orel'];
    const from = orel.nb.map((j) => g.regions[j]).filter((r) => r.ctrl === 'SOV');
    expect(from.length).toBeGreaterThanOrEqual(3);
    // Four fresh corps on each of three roads into Orel: twelve ready to attack.
    const ready: Corps[] = [];
    for (const r of from.slice(0, 3)) for (let k = 0; k < 4; k++) ready.push(addCorps(g, 'SOV', `Test corps ${r.name} ${k}`, 'inf', 3, r.id, {}));
    expect(limitAttackers(g, ready, orel)).toHaveLength(12);
    g.nations.SOV.maxAttack = OLD_ARMY.maxAttack;
    expect(limitAttackers(g, ready, orel)).toHaveLength(8);
  });
});

describe('armies formed late in the war', () => {
  it('are listed apart from the pre-war mobilisation, so they do not hold up the factories', () => {
    const late = (setup as any).formations as any[][];
    expect(late.length).toBeGreaterThan(0);
    const lastCallUp = Math.max(...((setup as any).reinforcements as any[][]).filter((row) => row[1] === 'SOV').map((row) => row[0]));
    expect(lastCallUp).toBeLessThan(MAY_43);
    for (const row of late) expect(row[0]).toBeGreaterThan(lastCallUp);
    // A month before the new armies arrive, the Soviet Union is already ordering new corps.
    expect(canBuildYet(at(MAY_43), 'SOV')).toBe(true);
  });

  it('arrive in June 1943: two fronts and two tank armies in Kursk, tied to its defence, with the reserve behind them', () => {
    const g = at(JUNE_43);
    for (const row of (setup as any).formations) expect(g.corps.some((c) => c.name === row[2])).toBe(false);
    play(g, 'SOV');
    for (const nm of ['Central Front', 'Voronezh Front', '1st Tank Army', '2nd Tank Army']) {
      expect(place(g, nm)).toBe('Kursk');
      expect(named(g, nm).hold).toBe(true);
    }
    expect(named(g, 'Central Front')).toMatchObject({ kind: 'inf', divs: 12 });
    expect(place(g, 'Steppe Front')).toBe('Voronezh');
    expect(place(g, '5th Guards Tank Army')).toBe('Voronezh');
    expect(named(g, 'Steppe Front').hold).toBe(false);
    expect(generalOf(g, named(g, 'Central Front'))?.name).toBe('Konstantin Rokossovsky');
    expect(generalOf(g, named(g, 'Voronezh Front'))?.name).toBe('Nikolai Vatutin');
    expect(generalOf(g, named(g, '5th Guards Tank Army'))?.name).toBe('Pavel Rotmistrov');
  });

  it('form at the capital instead if the place meant for them has fallen', () => {
    const g = at(JUNE_43);
    until(g, 'SOV');
    const kursk = g.byName['Kursk'];
    for (const c of [...kursk.units]) removeCorps(g, c);
    capture(g, kursk, 'GER');
    startPhase(g, 'SOV');
    expect(place(g, 'Central Front')).toBe('Moscow');
    expect(place(g, '1st Tank Army')).toBe('Moscow');
    expect(place(g, 'Steppe Front')).toBe('Voronezh');
  });
});

describe('the defence lines round Kursk', () => {
  it('are dug in June 1943, three belts deep, and the newsreel says so', () => {
    const g = at(JUNE_43);
    expect(g.byName['Kursk'].fort?.lv ?? 0).toBe(0);
    play(g, 'SOV');
    expect(g.byName['Kursk'].fort).toMatchObject({ lv: 3 });
    expect(g.byName['Kursk'].fort!.vs).toContain('SOV');
    expect(told(g, JUNE_43)).toContain('Defence lines are dug around Kursk.');
  });

  it('are not dug if Germany holds Kursk', () => {
    const g = at(JUNE_43);
    until(g, 'SOV');
    const kursk = g.byName['Kursk'];
    for (const c of [...kursk.units]) removeCorps(g, c);
    capture(g, kursk, 'GER');
    startPhase(g, 'SOV');
    expect(kursk.fort?.lv ?? 0).toBe(0);
  });
});

describe('a plan that allows one assault', () => {
  it('June 1943: both armies wait. Germany gathers its tanks at Orel and Kharkov and attacks nothing', () => {
    const g = at(JUNE_43);
    expect(phaseFor(g, 'GER')).toMatchObject({ name: 'Gathering for Citadel', posture: 'defend', quiet: true });
    expect(phaseFor(g, 'SOV')).toMatchObject({ name: 'Digging in round Kursk', posture: 'defend', quiet: true });
    const before = g.reports.length;
    play(g, 'GER');
    play(g, 'SOV');
    expect(g.reports.length).toBe(before);
    const fast = (nm: string): number => g.byName[nm].units.filter((u) => u.nat === 'GER' && (u.kind === 'arm' || u.kind === 'mot')).length;
    expect(fast('Orel')).toBeGreaterThanOrEqual(3);
    expect(fast('Kharkov')).toBeGreaterThanOrEqual(3);
  });

  it('Citadel is made once, against Kursk only, and is not renewed when it is held', () => {
    const g = at(JULY_43);
    expect(phaseFor(g, 'GER')).toMatchObject({ name: 'Operation Citadel', posture: 'attack', renew: false, targets: ['Kursk'] });
    const before = g.reports.length;
    play(g, 'GER');
    const fights = fought(g, 'GER', before);
    expect(fights).toHaveLength(1);
    expect(fights[0]).toMatchObject({ place: 'Kursk', taken: false });
    expect(fights[0].corps).toBeGreaterThanOrEqual(10);
    expect(g.byName['Kursk'].ctrl).toBe('SOV');
  });

  it('the same army, left free to renew the assault, goes in again and again', () => {
    const g = at(JULY_43);
    const ph = phaseFor(g, 'GER') as PlanPhase;
    const before = g.reports.length;
    ph.renew = true;
    try { play(g, 'GER'); } finally { ph.renew = false; }
    expect(fought(g, 'GER', before).filter((f) => f.place === 'Kursk').length).toBeGreaterThan(1);
  });

  it('once Citadel is held, the fronts in the salient are let off the leash and take Orel', () => {
    const g = at(JULY_43);
    play(g, 'GER');
    expect(named(g, 'Central Front').hold).toBe(true);
    const before = g.reports.length;
    play(g, 'SOV');
    expect(named(g, 'Central Front').hold).toBe(false);
    expect(named(g, 'Voronezh Front').hold).toBe(false);
    expect(fought(g, 'SOV', before)).toContainEqual(expect.objectContaining({ place: 'Orel', taken: true }));
    expect(g.byName['Orel'].ctrl).toBe('SOV');
  });
});

describe('the road to the Dnieper', () => {
  it('September 1943: the German army gives up the ground east of the river without a fight', () => {
    const g = at(SEPT_43);
    expect(phaseFor(g, 'GER')).toMatchObject({ name: 'Back to the Dnieper', posture: 'defend', quiet: true });
    expect(phaseFor(g, 'GER')!.abandon).toEqual(GIVEN_UP);
    const there = GIVEN_UP.map((nm) => g.byName[nm]).filter((r) => r.ctrl === 'GER');
    expect(there.reduce((s, r) => s + field(r, 'GER').length, 0)).toBeGreaterThanOrEqual(6);
    const before = g.reports.length;
    play(g, 'GER');
    expect(fought(g, 'GER', before)).toEqual([]);
    for (const r of there) expect(field(r, 'GER')).toEqual([]);
  });

  it('and stands on it: Kiev and the crossings at Dnepropetrovsk and Zaporozhye are manned', () => {
    const g = at(SEPT_43);
    const men = (nm: string): number => field(g.byName[nm], 'GER').length;
    const was = men('Kiev') + men('Dnepropetrovsk') + men('Zaporozhye') + men('Kirovograd');
    play(g, 'GER');
    expect(men('Kiev')).toBeGreaterThanOrEqual(3);
    expect(men('Dnepropetrovsk')).toBeGreaterThanOrEqual(3);
    expect(men('Zaporozhye')).toBeGreaterThanOrEqual(1);
    expect(men('Kiev') + men('Dnepropetrovsk') + men('Zaporozhye') + men('Kirovograd')).toBeGreaterThan(was + 3);
  });

  it('the Red Army follows up the same month and is told to take Smolensk, Bryansk and the Donbas', () => {
    const g = at(SEPT_43);
    const ph = phaseFor(g, 'SOV')!;
    expect(ph).toMatchObject({ name: 'The race for the Dnieper', posture: 'attack' });
    for (const nm of ['Smolensk', 'Bryansk', 'Stalino', 'Poltava']) expect(ph.objectives).toContain(nm);
    // Nothing beyond the river is on its list yet.
    for (const nm of ['Kiev', 'Zhitomir', 'Kirovograd', 'Minsk']) expect(ph.targets).not.toContain(nm);
    play(g, 'GER');
    const before = g.reports.length;
    play(g, 'SOV');
    for (const f of fought(g, 'SOV', before)) expect(ph.targets).toContain(f.place);
    expect(g.byName['Poltava'].ctrl).toBe('SOV');
    expect(g.byName['Stalino'].ctrl).toBe('SOV');
  });

  it('December 1943: if the Red Army has Zhitomir, the panzers strike back at it, and at nothing else', () => {
    const g = at(DEC_43);
    expect(phaseFor(g, 'GER')).toMatchObject({ name: 'The Zhitomir counter-stroke', posture: 'attack', renew: false, targets: ['Zhitomir'] });
    const zh = g.byName['Zhitomir'];
    for (const c of [...zh.units]) removeCorps(g, c);
    capture(g, zh, 'SOV');
    addCorps(g, 'SOV', 'Test rifle corps', 'inf', 2, zh.id, {});
    const before = g.reports.length;
    play(g, 'GER');
    const fights = fought(g, 'GER', before);
    expect(fights.length).toBe(1);
    expect(fights[0]).toMatchObject({ place: 'Zhitomir', taken: true });
  });

  /** December 1943 with the Red Army in Zhitomir in this strength, and what Germany then attacks. */
  const strokeAgainst = (corps: number): { place: string; taken: boolean; corps: number }[] => {
    const g = at(DEC_43);
    const zh = g.byName['Zhitomir'];
    for (const c of [...zh.units]) removeCorps(g, c);
    capture(g, zh, 'SOV');
    for (let k = 0; k < corps; k++) addCorps(g, 'SOV', `Test rifle corps ${k}`, 'inf', 4, zh.id, {});
    const before = g.reports.length;
    play(g, 'GER');
    return fought(g, 'GER', before);
  };

  it('the stroke is made at odds of three to two, once', () => {
    const fights = strokeAgainst(5);
    expect(fights).toHaveLength(1);
    expect(fights[0].place).toBe('Zhitomir');
  });

  it('but not against a Zhitomir held in strength', () => {
    expect(strokeAgainst(10)).toEqual([]);
  });

  it('and where Zhitomir was never lost, the German army attacks nothing in December', () => {
    const g = at(DEC_43);
    expect(g.byName['Zhitomir'].ctrl).toBe('GER');
    const before = g.reports.length;
    play(g, 'GER');
    expect(fought(g, 'GER', before)).toEqual([]);
  });
});
