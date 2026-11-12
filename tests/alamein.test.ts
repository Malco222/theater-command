// The orders an army overseas can be given that were added for the desert in 1942: a plan that depends
// on how its war has gone, giving ground up and falling back on its base, keeping a force at a place,
// gathering its armour where no railway runs, standing quiet, paying a price of its own for one prize,
// and works that are built or stripped in a named month.
import { describe, expect, it } from 'vitest';
import { STACK, KIND, type Corps, type Game, type PlanPhase, type Region } from '../src/campaign/types';
import { addCorps, assess, createGame, friendlyUnits, removeCorps, startPhase } from '../src/campaign/game';
import { theatrePhases } from '../src/campaign/ai';
import { loadGame, saveGame } from '../src/campaign/save';
import { current, runTurnAI, stepAI } from '../src/campaign/turn';

const DEC_41 = 28, JAN_42 = 29, MARCH_42 = 31, MAY_42 = 33, JUNE_42 = 34, JULY_42 = 35, AUG_42 = 36, SEPT_42 = 37, OCT_42 = 38, NOV_42 = 39, DEC_42 = 40;
const CYRENAICA = ['Bardia', 'Bir Hakeim', 'Jarabub', 'Tobruk', 'Mechili', 'Derna', 'Benghazi', 'Agedabia', 'Jalo'];

/** One war on the historical plan, remembered as each of these months opens, with Germany about to move. */
const opening: Record<number, Game> = {};
{
  const g = createGame(3, null);
  for (let t = 1; t < DEC_42; t++) {
    runTurnAI(g);
    if ([DEC_41, JAN_42, MARCH_42, MAY_42, JUNE_42, JULY_42, AUG_42, SEPT_42, OCT_42, NOV_42, DEC_42].includes(t + 1)) opening[t + 1] = loadGame(saveGame(g))!;
  }
}
/** A fresh copy of the war as that month opens, for a test to play on from. */
const at = (t: number): Game => loadGame(saveGame(opening[t]))!;
/** Lets the computer play until it is this nation's move. */
const until = (g: Game, nat: string): void => { let guard = 0; while (current(g) !== nat && guard++ < 60) stepAI(g); };
/** Plays this nation's month and stops there. */
const play = (g: Game, nat: string): void => { until(g, nat); stepAI(g); };

const axis = (nat: string): boolean => nat === 'GER' || nat === 'ITA';
const field = (r: Region, ok: (nat: string) => boolean): Corps[] => r.units.filter((u) => ok(u.nat) && u.kind !== 'fort' && !u.hold);
const british = (r: Region): Corps[] => field(r, (n) => n === 'UK');
const desert = (g: Game, nat: string): PlanPhase | null => theatrePhases(g, nat).find((x) => x.theatre === 'the Western Desert')?.phase ?? null;
const news = (g: Game, from: number, what: RegExp): number => g.headlines.slice(from).filter((h) => what.test(h.text)).length;

describe('a plan that depends on how the war has gone', () => {
  it('the Axis leaves Cyrenaica in December 1941 only once the frontier is lost', () => {
    const g = at(DEC_41);
    expect(g.byName['Bardia'].ctrl).toBe('UK');
    expect(desert(g, 'GER')).toMatchObject({ name: 'The retreat to El Agheila', posture: 'defend' });
    expect(desert(g, 'ITA')!.abandon).toEqual(expect.arrayContaining(['Tobruk', 'Benghazi', 'Agedabia']));
    // Had Crusader failed, there would be no such order and the siege lines would simply be held.
    g.byName['Bardia'].ctrl = 'ITA';
    expect(desert(g, 'GER')).toBeNull();
    expect(desert(g, 'ITA')).toBeNull();
  });

  it('the British stand at Gazala while Bir Hakeim holds, and make for El Alamein the month it falls', () => {
    const g = at(MAY_42);
    expect(g.byName['Bir Hakeim'].ctrl).toBe('UK');
    expect(desert(g, 'UK')).toMatchObject({ name: 'The Gazala battles', staging: { Tobruk: 3 } });
    g.byName['Bir Hakeim'].ctrl = 'GER';
    expect(desert(g, 'UK')).toMatchObject({ name: 'The retreat to El Alamein', staging: { 'El Alamein': 5 } });
    expect(desert(g, 'UK')!.abandon).toEqual(expect.arrayContaining(['Tobruk', 'Mersa Matruh']));
  });

  it('of several phases that fit the month, the first in the list is the one followed', () => {
    // In June both conditions of the retreat can be true at once, and so can the unconditional stand at Gazala.
    const g = at(JUNE_42);
    g.byName['Bir Hakeim'].ctrl = 'GER';
    g.byName['Tobruk'].ctrl = 'GER';
    expect(desert(g, 'UK')!.name).toBe('The retreat to El Alamein');
    g.byName['Bir Hakeim'].ctrl = 'UK';
    expect(desert(g, 'UK')!.name).toBe('The retreat to El Alamein');
    g.byName['Tobruk'].ctrl = 'UK';
    expect(desert(g, 'UK')!.name).toBe('The Gazala battles');
  });

  it('the Axis stays dug in before El Alamein until its line there is broken', () => {
    const g = at(NOV_42);
    expect(g.byName['Mersa Matruh'].ctrl).toBe('UK');
    expect(desert(g, 'GER')).toMatchObject({ name: 'The long retreat', quiet: true });
    g.byName['Mersa Matruh'].ctrl = 'GER';
    expect(desert(g, 'GER')).toBeNull();
  });
});

describe('an army overseas told to give ground up', () => {
  it('marches for its base, and is out before the enemy can get round it', () => {
    const g = at(DEC_41);
    const there = CYRENAICA.flatMap((n) => field(g.byName[n], axis));
    expect(there.length).toBeGreaterThanOrEqual(6);
    const men = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);
    const before = men(g.corps.filter((c) => axis(c.nat) && g.regions[c.reg].theatre === g.byName['Tobruk'].theatre && c.kind !== 'fort'));
    runTurnAI(g);
    for (const n of CYRENAICA) expect(field(g.byName[n], axis), n).toEqual([]);
    // It is an army still: it left the ground and kept the men.
    const after = men(g.corps.filter((c) => axis(c.nat) && g.regions[c.reg].theatre === g.byName['Tobruk'].theatre && c.kind !== 'fort'));
    expect(after).toBeGreaterThan(0.9 * before);
    for (const id of there.map((c) => c.id)) expect(g.corps.some((c) => c.id === id)).toBe(true);
    expect(axis(g.byName['El Agheila'].ctrl)).toBe(true);
    expect(field(g.byName['El Agheila'], axis).length).toBeGreaterThanOrEqual(3);
  });

  it('falls back on the base its theatre names, not on its capital', () => {
    const g = at(DEC_41);
    const ids = CYRENAICA.flatMap((n) => field(g.byName[n], (x) => x === 'GER')).map((c) => c.id);
    expect(ids.length).toBeGreaterThanOrEqual(2);
    play(g, 'GER');
    // Every German corps is nearer Tripoli than it was, and none has tried to go home to Berlin.
    for (const id of ids) {
      const c = g.corps.find((x) => x.id === id)!;
      expect(g.regions[c.reg].theatre).toBe(g.byName['Tripoli'].theatre);
      expect(CYRENAICA).not.toContain(g.regions[c.reg].name);
    }
  });

  it('on a single road the corps in front move on so that those behind can get out', () => {
    const g = at(DEC_41);
    // The stage behind the army is already full of its own troops, who have no order to go anywhere.
    const agheila = g.byName['El Agheila'];
    while (friendlyUnits(g, agheila, 'GER').length < STACK) addCorps(g, 'GER', `Depot troops ${agheila.units.length}`, 'inf', 1, 'El Agheila', {});
    const depot = agheila.units.filter((u) => u.name.startsWith('Depot troops')).map((u) => u.id);
    const coming = CYRENAICA.flatMap((n) => field(g.byName[n], (x) => x === 'GER')).length;
    expect(coming).toBeGreaterThanOrEqual(2);
    play(g, 'GER');
    for (const n of CYRENAICA) expect(field(g.byName[n], (x) => x === 'GER'), n).toEqual([]);
    // Some of the depot troops were sent on down the road to let the field army in.
    const moved = depot.filter((id) => g.corps.find((c) => c.id === id)!.reg !== agheila.id);
    expect(moved.length).toBeGreaterThanOrEqual(1);
    expect(friendlyUnits(g, agheila, 'GER').length).toBeLessThanOrEqual(STACK);
  });

  it('the British leave the Gazala line for El Alamein once it is turned, and the garrison of Tobruk stays', () => {
    const g = at(MAY_42);
    const tobruk = g.byName['Tobruk'];
    expect(british(tobruk).length).toBeGreaterThanOrEqual(2);
    runTurnAI(g);
    expect(axis(g.byName['Bir Hakeim'].ctrl)).toBe(true);
    for (const n of ['Tobruk', 'Bir Hakeim', 'Bardia', 'Sidi Barrani', 'Mersa Matruh']) expect(british(g.byName[n]), n).toEqual([]);
    expect(tobruk.ctrl).toBe('UK');
    expect(tobruk.units.some((u) => u.nat === 'UK' && u.hold)).toBe(true);
    expect(british(g.byName['El Alamein']).length).toBeGreaterThanOrEqual(4);
  });
});

describe('works that are built or stripped in a named month', () => {
  it('Tobruk is no longer a fortress by May 1942', () => {
    const g = at(MAY_42);
    const tobruk = g.byName['Tobruk'];
    expect(tobruk.fort?.lv).toBe(2);
    const from = g.headlines.length;
    // It is the holder's own doing, in the holder's own month.
    startPhase(g, 'GER');
    expect(tobruk.fort?.lv).toBe(2);
    startPhase(g, 'UK');
    expect(tobruk.fort?.lv).toBe(1);
    expect(news(g, from, /mines and wire of Tobruk/)).toBe(1);
    startPhase(g, 'UK');
    expect(news(g, from, /mines and wire of Tobruk/)).toBe(1);
  });

  it('a fortress already in other hands is left as it is', () => {
    const g = at(MAY_42);
    const tobruk = g.byName['Tobruk'];
    tobruk.ctrl = 'ITA';
    const from = g.headlines.length;
    startPhase(g, 'UK');
    startPhase(g, 'ITA');
    expect(tobruk.fort?.lv).toBe(2);
    expect(news(g, from, /mines and wire of Tobruk/)).toBe(0);
  });

  it('the Axis digs in before El Alamein in September, and the works face the British', () => {
    const g = at(SEPT_42);
    const matruh = g.byName['Mersa Matruh'];
    expect(matruh.ctrl).toBe('GER');
    expect(matruh.fort).toBeFalsy();
    const from = g.headlines.length;
    const tanks = british(g.byName['El Alamein']);
    const open = assess(g, tanks, matruh);
    startPhase(g, 'GER');
    expect(matruh.fort).toMatchObject({ lv: 1, vs: ['UK'] });
    expect(news(g, from, /digs in before El Alamein/)).toBe(1);
    const dug = assess(g, tanks, matruh);
    expect(dug.odds).toBeLessThan(open.odds);
    expect(dug.lines.some((l) => l.label === 'Fortifications' && /Level 1/.test(l.value))).toBe(true);
  });

  it('a save made before the month and loaded after it gives the same works', () => {
    const g = at(SEPT_42);
    runTurnAI(g);
    const back = loadGame(saveGame(g))!;
    expect(back.byName['Mersa Matruh'].fort).toEqual(g.byName['Mersa Matruh'].fort);
    expect(back.byName['Tobruk'].fort).toEqual(g.byName['Tobruk'].fort);
  });
});

describe('gathering the armour where no railway runs', () => {
  const fast = (r: Region): Corps[] => british(r).filter((u) => KIND[u.kind].mobile);

  it('Montgomery lands at Alexandria in August 1942 with a fresh armoured corps', () => {
    const X_CORPS = 'X Corps (8th and 10th Armoured Divisions)';
    const g = at(SEPT_42);
    const monty = g.generals.filter((x) => x.name === 'Bernard Montgomery');
    expect(monty.length).toBe(1);
    const c = g.corps.find((x) => x.id === monty[0].corps)!;
    expect(c).toMatchObject({ nat: 'UK', kind: 'arm', name: X_CORPS });
    expect(g.regions[c.reg].name).toBe('Alexandria');
    // A month earlier there was no such corps, and he was not in Africa.
    const aug = at(AUG_42);
    expect(aug.corps.some((x) => x.name === X_CORPS)).toBe(false);
    const was = aug.corps.find((x) => x.id === aug.generals.find((x) => x.name === 'Bernard Montgomery')!.corps);
    if (was) expect(aug.regions[was.reg].theatre).not.toBe(aug.byName['Alexandria'].theatre);
  });

  it('in September the plan calls the armour forward, and it marches up while infantry makes room', () => {
    const g = at(SEPT_42);
    const alamein = g.byName['El Alamein'];
    expect(desert(g, 'UK')).toMatchObject({ name: 'Montgomery gathers the armour', concentrate: { 'El Alamein': 4 } });
    expect(friendlyUnits(g, alamein, 'UK').length).toBe(STACK);
    expect(fast(alamein).length).toBe(3);
    const monty = g.generals.find((x) => x.name === 'Bernard Montgomery')!;
    play(g, 'UK');
    expect(fast(alamein).length).toBeGreaterThanOrEqual(4);
    expect(friendlyUnits(g, alamein, 'UK').length).toBeLessThanOrEqual(STACK);
    expect(g.regions[g.corps.find((x) => x.id === monty.corps)!.reg].name).toBe('El Alamein');
    // Nobody attacked anything on the way.
    expect(g.reports.filter((r) => r.turn === SEPT_42 && r.attNat === 'UK')).toEqual([]);
  });
});

describe('an army overseas told to stand quiet', () => {
  it('does not step forward into ground the enemy has left empty', () => {
    const g = at(MARCH_42);
    expect(desert(g, 'GER')).toMatchObject({ name: 'Building up before Gazala', quiet: true });
    const hakeim = g.byName['Bir Hakeim'];
    for (const c of [...hakeim.units]) removeCorps(g, c);
    expect(hakeim.units.length).toBe(0);
    play(g, 'GER');
    play(g, 'ITA');
    expect(hakeim.ctrl).toBe('UK');
    expect(hakeim.units.length).toBe(0);
  });

  it('and does not counter-attack ground it lost a month ago', () => {
    const g = at(MARCH_42);
    const from = g.reports.length;
    runTurnAI(g);
    expect(g.reports.slice(from).filter((r) => axis(r.attNat) && g.regions[r.region].theatre === g.byName['Tobruk'].theatre)).toEqual([]);
  });
});

describe('a price of its own for one prize, overseas', () => {
  it('the Axis throws itself at El Alamein in July 1942 at odds it would take nowhere else', () => {
    const g = at(JULY_42);
    const ph = desert(g, 'GER')!;
    expect(ph).toMatchObject({ name: 'El Alamein', minOdds: 1.2, oddsFor: { 'El Alamein': 0.2 } });
    const from = g.reports.length;
    runTurnAI(g);
    const tries = g.reports.slice(from).filter((r) => r.regionName === 'El Alamein' && axis(r.attNat));
    expect(tries.length).toBeGreaterThanOrEqual(1);
    for (const r of tries) expect(r.taken).toBe(false);
    expect(g.byName['El Alamein'].ctrl).toBe('UK');
  });

  it('without that order the same army in the same month does not try', () => {
    const g = at(JULY_42);
    const phases = [desert(g, 'GER')!, desert(g, 'ITA')!];
    const kept = phases.map((p) => p.oddsFor);
    try {
      for (const p of phases) delete p.oddsFor;
      const from = g.reports.length;
      runTurnAI(g);
      expect(g.reports.slice(from).filter((r) => r.regionName === 'El Alamein' && axis(r.attNat))).toEqual([]);
    } finally {
      phases.forEach((p, i) => { p.oddsFor = kept[i]; });
    }
  });
});

describe('reinforcements for the desert in 1942', () => {
  it('each side is sent what history sent, in the month history sent it', () => {
    const names = (g: Game, nat: string): string[] => g.corps.filter((c) => c.nat === nat).map((c) => c.name);
    const may = at(MAY_42), oct = at(OCT_42);
    expect(names(may, 'UK')).toEqual(expect.arrayContaining(['1st South African and 50th Divisions', '1st Armoured Division']));
    expect(names(may, 'ITA')).toContain('Littorio Armoured Division');
    expect(names(may, 'UK')).not.toContain('New Zealand and 9th Australian Divisions');
    expect(names(oct, 'UK')).toEqual(expect.arrayContaining(['New Zealand and 9th Australian Divisions', 'X Corps (8th and 10th Armoured Divisions)', '44th and 51st Divisions']));
  });

  it('the Axis reinforcements of August land at Tobruk, which it has to have taken first', () => {
    const g = at(SEPT_42);
    const tobruk = g.byName['Tobruk'];
    expect(axis(tobruk.ctrl)).toBe(true);
    expect(tobruk.units.map((u) => u.name)).toEqual(expect.arrayContaining(['164th Light Division and Ramcke Brigade', 'Folgore Parachute Division']));
    // With Tobruk still British they would have had nowhere to land.
    const held = at(AUG_42);
    held.byName['Tobruk'].ctrl = 'UK';
    startPhase(held, 'GER');
    startPhase(held, 'ITA');
    expect(held.corps.some((c) => c.name === '164th Light Division and Ramcke Brigade' || c.name === 'Folgore Parachute Division')).toBe(false);
  });
});
