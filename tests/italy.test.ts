// Sicily and Italy: landing ships that let an army fight its way off a beach, an air command that has to win
// the sky first, an army that loses heart, formations drawn out of an army already overseas, a landing that
// waits for its island, the fall of a government and the armistice that follows it, a retreat that leaves a
// rearguard, and a mountain line dug by the calendar.
import { describe, expect, it } from 'vitest';
import type { Corps, Game, Region } from '../src/campaign/types';
import { addCorps, allied, assess, capture, corpsBase, createGame, startPhase } from '../src/campaign/game';
import { theatrePhases } from '../src/campaign/ai';
import { skyOver } from '../src/campaign/air';
import { generalOf } from '../src/campaign/commanders';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { NAVY, assaultStrength, planLanding, whyNotLand } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { current, runTurnAI, stepAI } from '../src/campaign/turn';
import setup from '../data/campaign/setup-1939.json';

const MAY_43 = 45, JUNE_43 = 46, JULY_43 = 47, AUG_43 = 48, SEPT_43 = 49, OCT_43 = 50;
const XIII = 'Eighth Army: XIII Corps (5th, 50th and 78th Divisions)', XXX = 'Eighth Army: XXX Corps (51st, 1st Canadian and 1st Airborne Divisions)';
const II = 'Seventh Army: II Corps (1st, 9th and 45th Infantry Divisions)', PROV = 'Seventh Army: Provisional Corps (3rd Infantry, 2nd Armored and 82nd Airborne Divisions)';
const V = 'Eighth Army: V Corps (1st Airborne and 8th Indian Divisions)';
const VI = 'Fifth Army: VI Corps (36th and 34th Infantry Divisions)', X = 'Fifth Army: X Corps (British 46th, 56th and 7th Armoured Divisions)';
const HUBE = 'XIV Panzer Corps (Hermann Goring and 15th Panzergrenadier Divisions)', HERR = 'LXXVI Panzer Corps (26th Panzer, 29th Panzergrenadier and 1st Parachute Divisions)';
const REARGUARD = '16th Panzer Division';
const AIR_COMMAND = 'Mediterranean Air Command';
/** The ground of the Italian campaign. */
const ITALY = ['Malta', 'Sicily', 'Calabria', 'Apulia', 'Naples', 'Abruzzo', 'Rome', 'Marche', 'Tuscany', 'Sardinia', 'Corsica'];

/** One war on the historical plan, remembered as each of these months opens, with Germany about to move. */
const opening: Record<number, string> = {};
{
  const g = createGame(3, null);
  for (let t = 1; t < OCT_43; t++) {
    runTurnAI(g);
    if ([MAY_43, JUNE_43, JULY_43, AUG_43, SEPT_43, OCT_43].includes(t + 1)) opening[t + 1] = saveGame(g);
  }
}
/** A fresh copy of the war as that month opens, for a test to play on from. */
const at = (t: number): Game => loadGame(opening[t])!;
/** Lets the computer play until it is this nation's move. */
const until = (g: Game, nat: string): void => { let guard = 0; while (current(g) !== nat && guard++ < 60) stepAI(g); };
/** Plays this nation's month and stops there. */
const play = (g: Game, nat: string): void => { until(g, nat); stepAI(g); };
/** Opens this nation's month (arrivals, new air fleets) without giving any orders. */
const open = (g: Game, nat: string): void => { until(g, nat); g.week = 1; startPhase(g, nat); };

const named = (g: Game, name: string): Corps => g.corps.find((c) => c.name === name && !c.dead)!;
const place = (g: Game, name: string): string | null => { const c = named(g, name); return c ? g.regions[c.reg].name : null; };
const told = (g: Game, turn: number): string[] => g.headlines.filter((h) => h.turn === turn).map((h) => h.text);
const fought = (g: Game, nat: string, from: number): { place: string; taken: boolean; odds: number }[] =>
  g.reports.slice(from).filter((r) => r.attNat === nat).map((r) => ({ place: r.regionName, taken: r.taken, odds: r.odds }));
const R = (g: Game, name: string): Region => g.byName[name];
/** Divisions a nation has in the field in Africa: everything overseas that is not on the ground of the Italian campaign. */
const inAfrica = (g: Game, nat: string, garrisons = false): number =>
  g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && !!c.hold === garrisons && g.regions[c.reg].theatre >= 0 && !ITALY.includes(g.regions[c.reg].name)).reduce((s, c) => s + c.divs, 0);
/** The war as July 1943 opens, but with no Allied air command and nobody holding the sky over Sicily. */
const noAirCommand = (): Game => {
  const g = at(JULY_43);
  g.airFleets = g.airFleets.filter((f) => f.name !== AIR_COMMAND);
  delete g.sky[R(g, 'Sicily').id];
  return g;
};

describe('landing ships and naval gunfire', () => {
  it('troops fight their way off a beach at 60% of their strength, and British and American troops at 80% from June 1943', () => {
    const g = at(MAY_43);
    for (const nat of ['UK', 'USA', 'GER', 'ITA', 'SOV']) expect(assaultStrength(g, nat)).toBe(0.6);
    g.turn = JUNE_43;
    expect(assaultStrength(g, 'UK')).toBe(0.8);
    expect(assaultStrength(g, 'USA')).toBe(0.8);
    for (const nat of ['GER', 'ITA', 'SOV']) expect(assaultStrength(g, nat)).toBe(0.6);
    g.turn = 70;
    expect(assaultStrength(g, 'UK')).toBe(0.8);
    expect(assaultStrength(g, 'GER')).toBe(0.6);
  });

  it('the Battle Ticket for the landing in Sicily says so, and the same force without the new craft is a quarter weaker', () => {
    const g = at(JULY_43);
    open(g, 'UK');
    const force = [named(g, XIII), named(g, XXX)];
    const plan = planLanding(g, force, R(g, 'Sicily'));
    expect(plan.why).toBeNull();
    expect(plan.assess!.lines.find((l) => l.label === 'Landing')!.effect).toBe('Attack cut to 80%');
    const craft = NAVY.landing.craft;
    try {
      NAVY.landing.craft = [];
      const old = planLanding(g, force, R(g, 'Sicily'));
      expect(old.assess!.lines.find((l) => l.label === 'Landing')!.effect).toBe('Attack cut to 60%');
      expect(old.assess!.A / plan.assess!.A).toBeCloseTo(0.75, 3);
      expect(old.assess!.D).toBeCloseTo(plan.assess!.D, 6);
    } finally { NAVY.landing.craft = craft; }
  });
});

describe('the Allied air command in the Mediterranean', () => {
  it('forms at Malta in June 1943, under Tedder, and not before', () => {
    const g = at(JUNE_43);
    expect(g.airFleets.some((f) => f.name === AIR_COMMAND)).toBe(false);
    open(g, 'UK');
    const f = g.airFleets.find((x) => x.name === AIR_COMMAND)!;
    expect(f).toMatchObject({ nat: 'UK', ftr: 500, bmr: 400 });
    expect(g.regions[f.base].name).toBe('Malta');
    expect(f.leader?.name).toBe('Arthur Tedder');
    expect(g.flags[`air:${AIR_COMMAND}`]).toBe(JUNE_43);
    // It does not form twice.
    startPhase(g, 'UK');
    expect(g.airFleets.filter((x) => x.name === AIR_COMMAND)).toHaveLength(1);
  });

  it('does not form at a Malta that Britain has lost', () => {
    const g = at(JUNE_43);
    capture(g, R(g, 'Malta'), 'ITA');
    open(g, 'UK');
    expect(g.airFleets.some((f) => f.name === AIR_COMMAND)).toBe(false);
  });

  it('is sent against Sicily in June and wins the sky there for the invasion', () => {
    const g = at(JUNE_43);
    expect(skyOver(g, R(g, 'Sicily').id)).toBeNull();
    const flown = g.airReports.length;
    play(g, 'UK');
    const rep = g.airReports.slice(flown).find((a) => a.attNat === 'UK' && a.place === 'Sicily')!;
    expect(rep).toMatchObject({ mission: 'superiority', sky: true });
    expect(skyOver(g, R(g, 'Sicily').id)).toBe('UK');
    // No troops go with it: the invasion force has not yet gathered.
    expect(R(g, 'Sicily').ctrl).toBe('ITA');
  });

  it('moves up to airfields in Italy once the heel is taken, and goes over to supporting the armies', () => {
    const g = at(OCT_43);
    play(g, 'UK');
    const f = g.airFleets.find((x) => x.name === AIR_COMMAND)!;
    expect(g.regions[f.base].name).toBe('Apulia');
    expect(f.mission).toBe('support');
  });

  it('without it the invasion fleet cannot cross: Italian fighters hold the sky over Sicily', () => {
    const g = noAirCommand();
    open(g, 'UK');
    expect(whyNotLand(g, [named(g, XIII), named(g, XXX)], R(g, 'Sicily'))).toMatch(/Italian fighters still hold the sky over Sicily.*Win the sky there first/);
    stepAI(g);
    expect(R(g, 'Sicily').ctrl).toBe('ITA');
    expect(place(g, XIII)).toBe('Malta');
    expect(told(g, JULY_43).some((t) => /land at Sicily|thrown back/.test(t))).toBe(false);
  });
});

describe('Italy loses heart', () => {
  it('in June 1943 the Italian army still fights as it always has', () => {
    const g = at(JUNE_43);
    expect(g.nations.ITA.quality).toBe(0.75);
    expect(g.flags['event:italy-loses-heart']).toBeUndefined();
  });

  it('as July opens, with Tunis lost, it does not: the newsreel says so once, and nobody else is touched', () => {
    const g = at(JULY_43);
    expect(allied(g, 'UK', R(g, 'Tunis').ctrl)).toBe(true);
    expect(g.nations.ITA.quality).toBe(0.45);
    expect(g.flags['event:italy-loses-heart']).toBe(JULY_43);
    expect(told(g, JULY_43).filter((t) => /no heart left for this war/.test(t))).toHaveLength(1);
    expect(g.nations.ITA.air).toBe(40);
    expect(g.nations.GER).toMatchObject({ quality: 1.25, air: 100 });
  });

  it('an Italian corps is then worth three fifths of what it was', () => {
    const g = at(JULY_43);
    const c = g.corps.find((x) => x.nat === 'ITA' && !x.dead && x.reg === R(g, 'Sicily').id)!;
    const now = corpsBase(g, c);
    g.nations.ITA.quality = 0.75;
    expect(now / corpsBase(g, c)).toBeCloseTo(0.6, 6);
  });

  it('does not happen to an Italy that still holds Tunis', () => {
    const g = at(JUNE_43);
    R(g, 'Tunis').ctrl = 'ITA';
    g.turn = JULY_43;
    monthlyDiplomacy(g);
    expect(g.nations.ITA.quality).toBe(0.75);
    expect(g.flags['event:italy-loses-heart']).toBeUndefined();
  });

  it('stays that way in a saved game', () => {
    const g = loadGame(saveGame(at(JULY_43)))!;
    expect(g.nations.ITA.quality).toBe(0.45);
  });
});

describe('an army formed out of one already overseas', () => {
  it('the invasion force is listed after every expedition older saves know of', () => {
    const rows = (setup as any).expeditions as any[][];
    const first = rows.findIndex((row) => row[2] === '6th Army');
    expect(first).toBeGreaterThan(0);
    // Saved games remember expeditions by their place on the list: the new rows come last, in one block.
    for (let i = first; i < rows.length; i++) expect(rows[i][0]).toBeGreaterThanOrEqual(JULY_43);
    for (let i = 0; i < first; i++) expect(rows[i][0]).toBeLessThan(JULY_43);
  });

  it('the Eighth Army gathers at Malta in July: six divisions under Montgomery and Leese, taken out of the army in Africa', () => {
    const g = at(JULY_43);
    until(g, 'UK');
    const before = inAfrica(g, 'UK'), garrisons = inAfrica(g, 'UK', true);
    expect(g.corps.some((c) => c.name === XIII || c.name === XXX)).toBe(false);
    g.week = 1; startPhase(g, 'UK');
    for (const nm of [XIII, XXX]) {
      expect(place(g, nm)).toBe('Malta');
      expect(named(g, nm)).toMatchObject({ nat: 'UK', kind: 'inf', divs: 3 });
    }
    expect(generalOf(g, named(g, XXX))?.name).toBe('Oliver Leese');
    expect(before - inAfrica(g, 'UK')).toBe(6);
    // Garrisons are not drawn on.
    expect(inAfrica(g, 'UK', true)).toBe(garrisons);
  });

  it('the Seventh Army gathers there too, out of the American army in Africa', () => {
    const g = at(JULY_43);
    until(g, 'USA');
    const before = inAfrica(g, 'USA');
    g.week = 1; startPhase(g, 'USA');
    expect(named(g, II)).toMatchObject({ nat: 'USA', kind: 'inf', divs: 3 });
    expect(named(g, PROV)).toMatchObject({ nat: 'USA', kind: 'mot', divs: 3 });
    expect(generalOf(g, named(g, PROV))?.name).toBe('George S. Patton');
    expect(generalOf(g, named(g, II))?.name).toBe('Omar Bradley');
    expect(before - inAfrica(g, 'USA')).toBe(Math.min(before, 6));
  });

  it('a British corps under American command is drawn from the British army, not the American', () => {
    const g = at(SEPT_43);
    until(g, 'USA');
    const british = inAfrica(g, 'UK'), american = inAfrica(g, 'USA');
    g.week = 1; startPhase(g, 'USA');
    expect(named(g, X)).toMatchObject({ nat: 'USA', kind: 'mot', divs: 3 });
    expect(british - inAfrica(g, 'UK')).toBe(3);
    // The American corps that comes with it takes what the American army in Africa still has, and arrives all the same.
    expect(named(g, VI)).toMatchObject({ nat: 'USA', divs: 2 });
    expect(american - inAfrica(g, 'USA')).toBe(Math.min(american, 2));
  });

  it('a corps left with no divisions is struck off the list, and corps tied to their ground are never drawn on', () => {
    const g = at(JULY_43);
    until(g, 'UK');
    // Leave the army in Africa with a single small corps in the field: it goes to Malta whole.
    const field = g.corps.filter((c) => c.nat === 'UK' && !c.dead && c.kind !== 'fort' && !c.hold && g.regions[c.reg].theatre >= 0 && !ITALY.includes(g.regions[c.reg].name));
    expect(field.length).toBeGreaterThan(1);
    for (const c of field.slice(1)) c.hold = true;
    field[0].divs = 2;
    const tied = field.slice(1).map((c) => c.divs);
    g.week = 1; startPhase(g, 'UK');
    expect(field[0].dead).toBe(true);
    expect(place(g, XIII)).toBe('Malta');
    // Corps tied to their ground give nothing up, however large they are.
    expect(field.slice(1).map((c) => c.divs)).toEqual(tied);
  });
});

describe('Husky', () => {
  it('July 1943: the Eighth Army storms Sicily from Malta under a sky its air force has won', () => {
    const g = at(JULY_43);
    until(g, 'UK');
    const seen = g.reports.length;
    stepAI(g);
    expect(fought(g, 'UK', seen)).toEqual([expect.objectContaining({ place: 'Sicily', taken: true })]);
    expect(R(g, 'Sicily').ctrl).toBe('UK');
    for (const nm of [XIII, XXX]) expect(place(g, nm)).toBe('Sicily');
    expect(told(g, JULY_43)).toContain('British troops land at Sicily from the sea.');
    // The battle fleet comes up from Alexandria to cover the crossing.
    const fleet = g.fleets.find((f) => f.name === 'Mediterranean Fleet')!;
    expect(g.seas[fleet.zone].key).toBe('central');
  });

  it('the Seventh Army follows it ashore the same month without a fight', () => {
    const g = at(JULY_43);
    play(g, 'UK');
    until(g, 'USA');
    const seen = g.reports.length;
    stepAI(g);
    expect(fought(g, 'USA', seen)).toEqual([]);
    for (const nm of [II, PROV]) expect(place(g, nm)).toBe('Sicily');
  });

  it('the Germans on the far side of the strait leave the island to the Italians and do not counter-attack', () => {
    const g = at(AUG_43);
    expect(R(g, 'Sicily').ctrl).toBe('UK');
    expect(place(g, HUBE)).toBe('Calabria');
    expect(generalOf(g, named(g, HUBE))?.name).toBe('Hans-Valentin Hube');
    expect(theatrePhases(g, 'GER').find((p) => p.theatre === 'Italy')?.phase?.name).toBe('The Strait of Messina');
    const seen = g.reports.length;
    play(g, 'GER');
    expect(fought(g, 'GER', seen).filter((f) => ITALY.includes(f.place))).toEqual([]);
    expect(place(g, HUBE)).toBe('Calabria');
    expect(place(g, HERR)).toBe('Calabria');
    expect(place(g, REARGUARD)).toBe('Naples');
  });

  it('the landing stays on the table for as long as the island is in enemy hands', () => {
    // With no air command the summer passes and Sicily is still Italian.
    const g = noAirCommand();
    runTurnAI(g); runTurnAI(g);
    expect(g.turn).toBe(SEPT_43);
    expect(R(g, 'Sicily').ctrl).toBe('ITA');
    expect(place(g, XIII)).toBe('Malta');
    // Nothing that waits on the island has happened: Mussolini is in power, Italy is in the war, the Germans are on the strait.
    expect(g.flags['event:mussolini-falls']).toBeUndefined();
    expect(g.nations.ITA.alive).toBe(true);
    expect(theatrePhases(g, 'UK').find((p) => p.theatre === 'Italy')?.phase?.name).toBe('Operation Husky');
    expect(theatrePhases(g, 'GER').find((p) => p.theatre === 'Italy')?.phase?.name).toBe('The Strait of Messina');
    play(g, 'GER');
    expect(place(g, HUBE)).toBe('Calabria');
    // Give the Allies the sky in September and the landing is made then.
    g.sky[R(g, 'Sicily').id] = { nat: 'UK', turn: g.turn };
    play(g, 'UK');
    expect(R(g, 'Sicily').ctrl).toBe('UK');
    expect(place(g, XIII)).toBe('Sicily');
  });
});

describe('the fall of Mussolini and the armistice', () => {
  it('Mussolini falls the month after the Allies set foot on Italian soil, and Italy fights on', () => {
    expect(at(JULY_43).flags['event:mussolini-falls']).toBeUndefined();
    const g = at(AUG_43);
    expect(g.flags['event:mussolini-falls']).toBe(AUG_43);
    expect(told(g, AUG_43).filter((t) => /voted against Mussolini/.test(t))).toHaveLength(1);
    expect(g.nations.ITA.alive).toBe(true);
    expect(g.nations.ITA.atWar).toContain('UK');
    expect(g.flags['event:italian-armistice']).toBeUndefined();
  });

  it('he does not fall while Italy holds all of its own country', () => {
    const g = at(JULY_43);
    g.turn = AUG_43;
    monthlyDiplomacy(g);
    expect(g.flags['event:mussolini-falls']).toBeUndefined();
    g.turn = SEPT_43;
    monthlyDiplomacy(g);
    expect(g.flags['event:italian-armistice']).toBeUndefined();
    expect(g.nations.ITA.alive).toBe(true);
  });

  it('the armistice comes a month after his fall, never the same month', () => {
    const g = at(AUG_43);
    delete g.flags['event:mussolini-falls'];
    g.turn = SEPT_43;
    monthlyDiplomacy(g);
    expect(g.flags['event:mussolini-falls']).toBe(SEPT_43);
    expect(g.flags['event:italian-armistice']).toBeUndefined();
    expect(g.nations.ITA.alive).toBe(true);
    g.turn = OCT_43;
    monthlyDiplomacy(g);
    expect(g.flags['event:italian-armistice']).toBe(OCT_43);
    expect(g.nations.ITA.alive).toBe(false);
  });

  it('September 1943: Italy is out of the war, with its army, fleet and air force', () => {
    const before = at(AUG_43);
    expect(before.corps.filter((c) => c.nat === 'ITA' && !c.dead).length).toBeGreaterThan(5);
    const g = at(SEPT_43);
    expect(g.flags['event:italian-armistice']).toBe(SEPT_43);
    expect(g.nations.ITA).toMatchObject({ alive: false, atWar: [] });
    expect(g.corps.filter((c) => c.nat === 'ITA' && !c.dead)).toHaveLength(0);
    expect(g.fleets.filter((f) => f.nat === 'ITA')).toHaveLength(0);
    expect(g.airFleets.filter((f) => f.nat === 'ITA')).toHaveLength(0);
    for (const n of Object.values(g.nations)) expect(n.atWar).not.toContain('ITA');
    expect(told(g, SEPT_43).filter((t) => /signed an armistice/.test(t))).toHaveLength(1);
  });

  it('Germany takes over Rome, the north and the garrisons abroad; the Allies get Sardinia and Corsica and keep what they hold', () => {
    const g = at(SEPT_43);
    for (const nm of ['Rome', 'Naples', 'Calabria', 'Apulia', 'Abruzzo', 'Tuscany', 'Lombardy', 'Liguria', 'Veneto', 'Albania', 'Rhodes']) expect(R(g, nm).ctrl).toBe('GER');
    for (const nm of ['Sardinia', 'Corsica', 'Sicily', 'Tripoli', 'Benghazi']) expect(allied(g, 'UK', R(g, nm).ctrl)).toBe(true);
    // No region anywhere is left in Italian hands.
    expect(g.regions.filter((r) => r.ctrl === 'ITA')).toHaveLength(0);
  });
});

describe('back to the Gustav Line', () => {
  it('September: the Germans leave the toe, the heel and Naples in one month, and a rearguard stays at Naples', () => {
    const g = at(SEPT_43);
    expect(theatrePhases(g, 'GER').find((p) => p.theatre === 'Italy')?.phase?.name).toBe('Back to the Gustav Line');
    const seen = g.reports.length;
    play(g, 'GER');
    expect(place(g, HUBE)).toBe('Abruzzo');
    expect(place(g, HERR)).toBe('Abruzzo');
    expect(place(g, REARGUARD)).toBe('Naples');
    for (const nm of ['Calabria', 'Apulia']) expect(R(g, nm).units.filter((u) => u.nat === 'GER')).toHaveLength(0);
    expect(fought(g, 'GER', seen).filter((f) => ITALY.includes(f.place))).toEqual([]);
  });

  it('the line is dug across the mountains in October, by a Germany that holds them', () => {
    const g = at(OCT_43);
    expect(R(g, 'Abruzzo').fort ?? null).toBeNull();
    play(g, 'GER');
    expect(R(g, 'Abruzzo').fort).toMatchObject({ lv: 2 });
    // The western end of it, above the road to Rome, is dug in the same month.
    expect(R(g, 'Cassino').fort).toMatchObject({ lv: 2 });
    expect(told(g, OCT_43).filter((t) => /from the Garigliano to the Sangro/.test(t))).toHaveLength(1);
    expect(told(g, OCT_43).filter((t) => /anchored on Monte Cassino/.test(t))).toHaveLength(1);
  });

  it('is not dug if the Allies are already in those mountains', () => {
    const g = at(OCT_43);
    for (const u of [...R(g, 'Abruzzo').units]) if (u.nat === 'GER') u.dead = true;
    R(g, 'Abruzzo').units = R(g, 'Abruzzo').units.filter((u) => !u.dead);
    capture(g, R(g, 'Abruzzo'), 'UK');
    play(g, 'GER');
    expect(R(g, 'Abruzzo').fort ?? null).toBeNull();
    expect(told(g, OCT_43).some((t) => /from the Garigliano to the Sangro/.test(t))).toBe(false);
    // Each stretch of the line stands on its own: Cassino is still German, and is still dug.
    expect(R(g, 'Cassino').fort).toMatchObject({ lv: 2 });
  });

  it('an attack up the road from Naples meets the works head-on', () => {
    const g = at(OCT_43);
    play(g, 'GER');
    until(g, 'USA');
    const naples = R(g, 'Naples'), line = R(g, 'Abruzzo');
    expect(naples.ctrl).toBe('USA');
    const force = naples.units.filter((u) => u.nat === 'USA');
    expect(force.length).toBeGreaterThan(0);
    const fort = line.fort;
    const withWorks = assess(g, force, line).odds;
    line.fort = null;
    const without = assess(g, force, line).odds;
    line.fort = fort;
    expect(without / withWorks).toBeCloseTo(1.7, 2);
    // History's plan does not throw the army at them.
    expect(withWorks).toBeLessThan(1.1);
  });
});

describe('Baytown, Slapstick and Avalanche', () => {
  it('September: the Eighth Army walks into the toe and V Corps lands in the heel', () => {
    const g = at(SEPT_43);
    play(g, 'GER');
    until(g, 'UK');
    const seen = g.reports.length;
    stepAI(g);
    expect(R(g, 'Calabria').ctrl).toBe('UK');
    expect(R(g, 'Apulia').ctrl).toBe('UK');
    expect(place(g, V)).toBe('Apulia');
    expect(told(g, SEPT_43)).toContain('British troops land at Apulia from the sea.');
    expect(fought(g, 'UK', seen)).toEqual([]);
    expect(R(g, 'Calabria').units.some((u) => u.nat === 'UK')).toBe(true);
  });

  it('the Fifth Army storms Naples from the sea against the German rearguard', () => {
    const g = at(SEPT_43);
    until(g, 'USA');
    const seen = g.reports.length;
    stepAI(g);
    expect(fought(g, 'USA', seen)).toEqual([expect.objectContaining({ place: 'Naples', taken: true })]);
    expect(R(g, 'Naples').ctrl).toBe('USA');
    for (const nm of [VI, X]) expect(place(g, nm)).toBe('Naples');
    expect(generalOf(g, named(g, VI))?.name).toBe('Mark Clark');
    expect(told(g, SEPT_43)).toContain('American troops land at Naples from the sea.');
  });

  it('a shore too strongly held is left alone: against a panzer army at Naples the Fifth Army goes ashore in the heel', () => {
    const g = at(SEPT_43);
    until(g, 'USA');
    for (let k = 0; k < 2; k++) addCorps(g, 'GER', `Test panzer corps ${k}`, 'arm', 3, R(g, 'Naples').id, { xp: 0.3 });
    const seen = g.reports.length;
    stepAI(g);
    expect(fought(g, 'USA', seen)).toEqual([]);
    expect(R(g, 'Naples').ctrl).toBe('GER');
    for (const nm of [VI, X]) expect(place(g, nm)).toBe('Apulia');
    expect(told(g, SEPT_43).some((t) => /thrown back/.test(t))).toBe(false);
  });
});

describe('plan notes for a campaign that has not begun', () => {
  it('the garrison of Malta gets no orders from the Italian campaign before 1943', () => {
    const g = createGame(1, null);
    expect(theatrePhases(g, 'UK').some((p) => p.theatre === 'Italy')).toBe(false);
    expect(theatrePhases(at(MAY_43), 'UK').some((p) => p.theatre === 'Italy')).toBe(false);
  });

  it('from July 1943 the British plan there is Operation Husky, with Sicily as its base once it is won', () => {
    const g = at(JULY_43);
    expect(theatrePhases(g, 'UK').find((p) => p.theatre === 'Italy')).toMatchObject({ phase: { name: 'Operation Husky' }, base: 'Sicily' });
  });
});
