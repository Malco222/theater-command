// The United States, Operation Torch and Tunisia: a great power that is not on the map, an event that wakes
// a sleeping nation and one that divides a fallen one, conditions on who holds a list of places, corps
// landed together in one lift, and the orders added to a theatre's plan for the war from Morocco to Tunis.
import { describe, expect, it } from 'vitest';
import type { Corps, Game, PlanPhase, Region } from '../src/campaign/types';
import { addCorps, assess, atWar, createGame, freshOffensive, relocate, theName } from '../src/campaign/game';
import { theatrePhases } from '../src/campaign/ai';
import { generalOf } from '../src/campaign/commanders';
import { holds, monthlyDiplomacy } from '../src/campaign/diplomacy';
import { income } from '../src/campaign/economy';
import { openSeas, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { current, runTurnAI, stepAI } from '../src/campaign/turn';

const NOV_41 = 27, DEC_41 = 28, FEB_42 = 30, NOV_42 = 39, DEC_42 = 40, JAN_43 = 41, FEB_43 = 42, MARCH_43 = 43;
const WEST = ['Casablanca', 'Fez', 'Marrakech', 'Oran', 'Algiers', 'Constantine'];
const TASK_FORCE = 'Western Task Force (2nd Armored and 9th Infantry Divisions)';

/** One war on the historical plan, remembered as each of these months opens, with Germany about to move. */
const opening: Record<number, string> = {};
{
  const g = createGame(3, null);
  for (let t = 1; t < MARCH_43; t++) {
    runTurnAI(g);
    if ([NOV_41, DEC_41, FEB_42, NOV_42, DEC_42, JAN_43, FEB_43, MARCH_43].includes(t + 1)) opening[t + 1] = saveGame(g);
  }
}
/** A fresh copy of the war as that month opens, for a test to play on from. */
const at = (t: number): Game => loadGame(opening[t])!;
/** Lets the computer play until it is this nation's move. */
const until = (g: Game, nat: string): void => { let guard = 0; while (current(g) !== nat && guard++ < 60) stepAI(g); };
/** Plays this nation's month and stops there. */
const play = (g: Game, nat: string): void => { until(g, nat); stepAI(g); };

const of = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead);
const named = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && !c.dead)!;
const place = (g: Game, name: string): string => g.regions[named(g, name).reg].name;
const field = (r: Region, nat: string): Corps[] => r.units.filter((u) => u.nat === nat && u.kind !== 'fort' && !u.hold);
const africa = (g: Game, nat: string): { phase: PlanPhase | null; base: string | null } | undefined => theatrePhases(g, nat).find((x) => x.theatre === 'North Africa');
const told = (g: Game, turn: number): string[] => g.headlines.filter((h) => h.turn === turn).map((h) => h.text);

describe('the United States', () => {
  it('sleeps until December 1941: not in the war, with no army anywhere on the map', () => {
    const g = at(NOV_41);
    expect(g.nations.USA).toMatchObject({ alive: false, dormant: true, atWar: [] });
    expect(of(g, 'USA')).toEqual([]);
    expect(g.flags['event:pearl-harbor']).toBeUndefined();
  });

  it('comes in with Pearl Harbor, at war with Germany and Italy, and the news is told before the declarations', () => {
    const g = at(DEC_41);
    expect(g.nations.USA).toMatchObject({ alive: true, dormant: false, align: 'allies' });
    expect(atWar(g, 'USA', 'GER')).toBe(true);
    expect(atWar(g, 'USA', 'ITA')).toBe(true);
    expect(g.flags['event:pearl-harbor']).toBe(DEC_41);
    const news = told(g, DEC_41);
    const pearl = news.findIndex((t) => /Pearl Harbor/.test(t));
    expect(pearl).toBeGreaterThanOrEqual(0);
    expect(news.indexOf('The United States declares war on Germany.')).toBeGreaterThan(pearl);
    expect(news.indexOf('The United States declares war on Italy.')).toBeGreaterThan(pearl);
    // Told once: the event's own headline is not printed a second time after the declarations.
    expect(news.filter((t) => /Pearl Harbor/.test(t))).toHaveLength(1);
  });

  it('the war Germany did not open gives its army no fresh offensive', () => {
    const g = at(DEC_41);
    expect(g.nations.GER.offStart.USA).toBeUndefined();
    expect(freshOffensive(g, 'GER', 'USA')).toBe(false);
  });

  it('is a power off the map: its ships come in from the North Atlantic, and its factories count though no region holds them', () => {
    const g = at(DEC_41);
    const usa = g.nations.USA;
    expect(usa.capital).toBe(-1);
    expect(g.regions.some((r) => r.owner === 'USA' || r.ctrl === 'USA')).toBe(false);
    expect(usa.offmap).toEqual({ sea: 'natl', industry: 60 });
    expect(openSeas(g, 'USA')[zoneOf(g, 'natl')]).toBe(1);
    const before = income(g, 'USA');
    expect(before).toBeGreaterThanOrEqual(60);
    usa.offmap = null;
    expect(income(g, 'USA')).toBeCloseTo(before - 60, 5);
    expect([...openSeas(g, 'USA')].some(Boolean)).toBe(false);
  });

  it('raises nothing on the map: its army is the one that sails for Torch', () => {
    const g = at(NOV_42);
    expect(of(g, 'USA')).toEqual([]);
    play(g, 'USA');
    expect(of(g, 'USA').map((c) => c.name).sort()).toEqual(['3rd Infantry Division', 'II Corps (1st Infantry and 1st Armored Divisions)', TASK_FORCE]);
    // Its commanders come with it.
    expect(generalOf(g, named(g, TASK_FORCE))?.name).toBe('George S. Patton');
    expect(generalOf(g, named(g, 'II Corps (1st Infantry and 1st Armored Divisions)'))?.name).toBe('Lloyd Fredendall');
  });

  it('with no airfields on the map of its own, its air force flies only for its own army', () => {
    const g = at(JAN_43);
    const british = field(g.byName['El Agheila'], 'UK'), american = field(g.byName['Biskra'], 'USA');
    expect(british.length).toBeGreaterThan(0);
    expect(american.length).toBeGreaterThan(0);
    const uk = assess(g, british, g.byName['Sirte']), us = assess(g, american, g.byName['Sfax']);
    g.nations.USA.air = 0;
    const uk0 = assess(g, british, g.byName['Sirte']), us0 = assess(g, american, g.byName['Sfax']);
    expect(uk0.A).toBe(uk.A);
    expect(uk0.D).toBe(uk.D);
    expect(us0.A).toBeLessThan(us.A);
  });

  it('is named with its article', () => {
    const g = createGame(1, null);
    expect(theName(g.nations.USA, true)).toBe('The United States');
    expect(theName(g.nations.USA)).toBe('the United States');
    expect(theName(g.nations.SOV)).toBe('the Soviet Union');
    expect(theName(g.nations.GER, true)).toBe('Germany');
  });
});

describe('what an event can wait on', () => {
  it('a war between two nations, and a nation still asleep', () => {
    const before = at(NOV_41), after = at(DEC_41);
    expect(holds(before, 'dormant:USA')).toBe(true);
    expect(holds(after, 'dormant:USA')).toBe(false);
    expect(holds(before, 'war:USA,GER')).toBe(false);
    expect(holds(after, 'war:USA,GER')).toBe(true);
    expect(holds(after, 'war:GER,USA')).toBe(true);
    expect(holds(after, 'war:USA,SOV')).toBe(false);
    expect(holds(after, 'war:USA,XXX')).toBe(false);
  });

  it('one place, or every place, on a list being in the hands of a bloc', () => {
    const g = at(NOV_42);
    const list = WEST.join(',');
    expect(holds(g, `anyHeld:allies,${list}`)).toBe(false);
    expect(holds(g, `allHeld:allies,${list}`)).toBe(false);
    g.byName['Oran'].ctrl = 'USA';
    expect(holds(g, `anyHeld:allies,${list}`)).toBe(true);
    expect(holds(g, `allHeld:allies,${list}`)).toBe(false);
    expect(holds(g, `anyHeld:axis,${list}`)).toBe(false);
    for (const n of WEST) g.byName[n].ctrl = 'UK';
    expect(holds(g, `allHeld:allies,${list}`)).toBe(true);
    // A name that is not on the map is held by nobody.
    expect(holds(g, `allHeld:allies,${list},Atlantis`)).toBe(false);
    expect(holds(g, 'anyHeld:allies,Atlantis')).toBe(false);
  });
});

describe("Darlan's ceasefire", () => {
  it('waits for the Allies to be ashore in French North Africa', () => {
    const g = at(NOV_42);
    monthlyDiplomacy(g);
    expect(g.flags['event:darlan']).toBeUndefined();
    expect(g.nations.VIC.alive).toBe(true);
    expect(g.byName['Algiers'].ctrl).toBe('VIC');
  });

  it('ends Vichy France and divides what it held: North Africa and the Levant to the Allies, the rest of France and Tunisia to the Axis', () => {
    const g = at(NOV_42);
    g.byName['Oran'].ctrl = 'USA';
    monthlyDiplomacy(g);
    expect(g.flags['event:darlan']).toBe(NOV_42);
    expect(g.nations.VIC).toMatchObject({ alive: false, align: 'independent', atWar: [] });
    expect(g.nations.VIC.fellOn).toMatchObject({ turn: NOV_42, to: 'USA' });
    expect(of(g, 'VIC')).toEqual([]);
    expect(g.regions.filter((r) => r.ctrl === 'VIC')).toEqual([]);
    expect(Object.values(g.nations).some((n) => n.atWar.includes('VIC'))).toBe(false);
    const ctrl = (names: string[]): string[] => names.map((n) => g.byName[n].ctrl);
    expect(ctrl(['Casablanca', 'Fez', 'Marrakech', 'Colomb-Bechar', 'Oran'])).toEqual(Array(5).fill('USA'));
    expect(ctrl(['Algiers', 'Constantine', 'Laghouat', 'Biskra', 'Ouargla', 'Syria', 'Lebanon', 'Damascus'])).toEqual(Array(8).fill('UK'));
    expect(ctrl(['Tunis', 'Lyon', 'Toulouse', 'Marseille'])).toEqual(Array(4).fill('GER'));
    expect(ctrl(['Sfax', 'Mareth', 'Riviera', 'Corsica'])).toEqual(Array(4).fill('ITA'));
    // The ground keeps its owner: it is held, not annexed.
    expect(g.byName['Algiers'].owner).toBe('VIC');
    // The first Axis troops are flown in to hold the bridgehead.
    expect(g.byName['Tunis'].units.map((u) => `${u.nat} ${u.name}`)).toEqual(['GER Division von Broich']);
    expect(g.byName['Sfax'].units.map((u) => u.name)).toEqual(['Superga Division']);
    expect(g.byName['Mareth'].units.map((u) => u.name)).toEqual(['Saharan Group']);
    expect(told(g, NOV_42).some((t) => /Admiral Darlan orders a ceasefire/.test(t))).toBe(true);
  });

  it('a share goes to the first of its takers still in the war', () => {
    const g = at(NOV_42);
    g.nations.USA.alive = false;
    g.byName['Algiers'].ctrl = 'UK';
    monthlyDiplomacy(g);
    expect(g.nations.VIC.alive).toBe(false);
    expect(g.byName['Casablanca'].ctrl).toBe('UK');
    expect(g.byName['Oran'].ctrl).toBe('UK');
    expect(g.byName['Tunis'].ctrl).toBe('GER');
  });

  it('on the historical plan it comes the month after the landings, and Tunisia is in Axis hands when December opens', () => {
    const g = at(DEC_42);
    expect(g.flags['event:darlan']).toBe(DEC_42);
    expect(g.byName['Casablanca'].ctrl).toBe('USA');
    expect(g.byName['Algiers'].ctrl).toBe('UK');
    expect(g.byName['Tunis'].ctrl).toBe('GER');
    expect(g.byName['Mareth'].ctrl).toBe('ITA');
  });
});

describe('the end in Africa', () => {
  it('is announced once Tunis, Sfax, Mareth and Tripoli are all in Allied hands, and not before', () => {
    const g = at(DEC_42);
    for (const n of ['Tunis', 'Mareth', 'Tripoli']) g.byName[n].ctrl = 'UK';
    monthlyDiplomacy(g);
    expect(g.flags['event:africa-cleared']).toBeUndefined();
    g.byName['Sfax'].ctrl = 'USA';
    const before = g.headlines.length;
    monthlyDiplomacy(g);
    expect(g.flags['event:africa-cleared']).toBe(DEC_42);
    expect(g.headlines.slice(before).some((h) => /masters of the North African shores/.test(h.text))).toBe(true);
  });
});

describe('Torch in the plan for North Africa', () => {
  it('Britain goes to war with Vichy the month it lands, and the 78th Division gets ashore in Algeria', () => {
    const g = at(NOV_42);
    expect(atWar(g, 'UK', 'VIC')).toBe(false);
    expect(africa(g, 'UK')!.phase).toMatchObject({ name: 'The pursuit, and Operation Torch', declare: ['VIC'] });
    play(g, 'UK');
    expect(atWar(g, 'UK', 'VIC')).toBe(true);
    expect(g.flags['war:UK:VIC']).toBe(NOV_42);
    const ashore = place(g, 'First Army: 78th Division');
    expect(['Algiers', 'Constantine']).toContain(ashore);
    expect(g.byName[ashore].ctrl).toBe('UK');
  });

  it('corps the plan sends in one lift land together and fight as one force', () => {
    const g = at(NOV_42);
    play(g, 'USA');
    expect(place(g, TASK_FORCE)).toBe('Casablanca');
    expect(place(g, '3rd Infantry Division')).toBe('Casablanca');
    expect(place(g, 'II Corps (1st Infantry and 1st Armored Divisions)')).toBe('Oran');
    const fight = g.reports.find((r) => r.turn === NOV_42 && r.regionName === 'Casablanca')!;
    expect(fight).toMatchObject({ attNat: 'USA', taken: true });
    expect(fight.attackers.map((u) => u.name).sort()).toEqual(['3rd Infantry Division', TASK_FORCE]);
  });

  it('a shore too strongly held to storm is passed over for the next place on the list', () => {
    const g = at(NOV_42);
    for (let i = 0; i < 3; i++) addCorps(g, 'VIC', `Reinforced garrison ${i + 1}`, 'inf', 3, g.byName['Casablanca'].id, { hold: 1 });
    play(g, 'USA');
    expect(g.reports.some((r) => r.turn === NOV_42 && r.regionName === 'Casablanca')).toBe(false);
    expect(g.byName['Casablanca'].ctrl).toBe('VIC');
    expect(place(g, TASK_FORCE)).toBe('Fez');
    expect(place(g, '3rd Infantry Division')).toBe('Fez');
  });
});

describe('the orders added for Tunisia', () => {
  it('a phase can name the place its army falls back on, in place of the theatre\'s usual base', () => {
    expect(africa(at(DEC_42), 'GER')).toMatchObject({ phase: { name: 'Back to Tripolitania' }, base: 'Tripoli' });
    expect(africa(at(JAN_43), 'GER')).toMatchObject({ phase: { name: 'The retreat into Tunisia', base: 'Mareth' }, base: 'Mareth' });
    expect(africa(at(JAN_43), 'ITA')!.base).toBe('Mareth');
    expect(africa(at(MARCH_43), 'ITA')).toMatchObject({ phase: { name: 'The Mareth Line', base: 'Tunis' }, base: 'Tunis' });
    expect(africa(at(JAN_43), 'UK')!.base).toBe('Alexandria');
  });

  it('in January 1943 the Germans at Sirte go back past Tripoli to the Mareth Line', () => {
    const g = at(JAN_43);
    const rearguard = field(g.byName['Sirte'], 'GER').map((c) => c.name);
    expect(rearguard.length).toBeGreaterThan(0);
    play(g, 'GER');
    for (const name of rearguard) expect(place(g, name)).toBe('Mareth');
    expect(field(g.byName['Tripoli'], 'GER')).toEqual([]);
  });

  it('Kasserine: the force the plan keeps on the Mareth Line stays there, and only the rest attacks', () => {
    const g = at(FEB_43);
    expect(africa(g, 'GER')!.phase).toMatchObject({ name: 'Kasserine', posture: 'attack', targets: ['Biskra'], staging: { Mareth: 2 } });
    const line = field(g.byName['Mareth'], 'GER').map((c) => c.name);
    expect(line).toHaveLength(2);
    play(g, 'GER');
    const stroke = g.reports.filter((r) => r.turn === FEB_43 && r.attNat === 'GER' && r.regionName === 'Biskra');
    expect(stroke.length).toBeGreaterThan(0);
    for (const f of stroke) for (const u of f.attackers) expect(line).not.toContain(u.name);
    expect(field(g.byName['Mareth'], 'GER').map((c) => c.name)).toEqual(line);
  });

  it('with the stroke over, the panzers left out in front are called back into the bridgehead', () => {
    const g = at(MARCH_43);
    const biskra = g.byName['Biskra'];
    for (const u of [...biskra.units]) relocate(g, u, g.byName['Constantine']);
    biskra.ctrl = 'GER';
    g.supplyDirty = true;
    relocate(g, named(g, '10th Panzer Division'), biskra);
    expect(africa(g, 'GER')!.phase!.abandon).toEqual(expect.arrayContaining(['Biskra', 'Constantine']));
    play(g, 'GER');
    expect(['Sfax', 'Tunis']).toContain(place(g, '10th Panzer Division'));
  });

  it('the Americans attack nothing in Tunisia until the spring', () => {
    expect(africa(at(JAN_43), 'USA')!.phase).toMatchObject({ name: 'East to the Tunisian frontier', posture: 'defend', staging: { Biskra: 2 } });
    expect(africa(at(MARCH_43), 'USA')!.phase).toMatchObject({ name: 'El Guettar', posture: 'attack', targets: ['Sfax'] });
  });
});

describe('a game saved before the United States was in it', () => {
  /** The save as an older build would have written it: no United States in the order of play or among the nations. */
  const older = (g: Game): string => {
    const d = JSON.parse(saveGame(g));
    const k = d.order.indexOf('USA');
    d.order.splice(k, 1);
    if (d.phase > k) d.phase--;
    delete d.nations.USA;
    for (const n of Object.values(d.nations) as { atWar: string[] }[]) n.atWar = n.atWar.filter((x) => x !== 'USA');
    for (const f of ['event:pearl-harbor', 'war:USA:GER', 'war:USA:ITA']) delete d.flags[f];
    return JSON.stringify(d);
  };

  it('loads with the United States in its place after Britain, and the month goes on with whoever was moving', () => {
    for (const who of ['GER', 'UK', 'BEL', 'VIC']) {
      const g = at(FEB_42);
      until(g, who);
      const back = loadGame(older(g))!;
      expect(back.order.indexOf('USA')).toBe(back.order.indexOf('UK') + 1);
      expect(current(back)).toBe(who);
      expect(back.nations.USA).toMatchObject({ alive: false, dormant: true });
    }
  });

  it('and the United States comes into the war when the next month opens', () => {
    const back = loadGame(older(at(FEB_42)))!;
    runTurnAI(back);
    expect(back.nations.USA.alive).toBe(true);
    expect(atWar(back, 'USA', 'GER')).toBe(true);
    expect(back.flags['event:pearl-harbor']).toBe(FEB_42 + 1);
  });
});
