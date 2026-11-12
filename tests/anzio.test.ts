// Italy in 1944: the two places the campaign turns on, formations that grow by taking in new divisions, are
// made by breaking up others, or are only lent and go home; a beachhead that digs in the month it is won; a
// plan whose phases wait on the ground (no counter-attack without a beachhead, no retreat while the line
// holds); a retreat that takes the garrisons with it; and the armies of 1944, which are not those of 1942.
import { describe, expect, it } from 'vitest';
import type { Corps, Game, Region } from '../src/campaign/types';
import { assess, capture, createGame, endPhase, removeCorps, startPhase } from '../src/campaign/game';
import { theatrePhases } from '../src/campaign/ai';
import { monthlyDiplomacy } from '../src/campaign/diplomacy';
import { loadGame, saveGame } from '../src/campaign/save';
import { current, runTurnAI, stepAI } from '../src/campaign/turn';

const JAN_44 = 53, FEB_44 = 54, MAR_44 = 55, APR_44 = 56, MAY_44 = 57, JUNE_44 = 58, JULY_44 = 59, AUG_44 = 60;
const VI_43 = 'Fifth Army: VI Corps (36th and 34th Infantry Divisions)';
const VI = 'Fifth Army: VI Corps (3rd and 45th Infantry, 1st Armored and British 1st and 5th Divisions)';
const SEVENTH = ['Seventh Army: II Corps (1st, 9th and 45th Infantry Divisions)', 'Seventh Army: Provisional Corps (3rd Infantry, 2nd Armored and 82nd Airborne Divisions)'];
const II = 'Fifth Army: II Corps (34th and 36th Infantry Divisions)', II_MAY = 'Fifth Army: II Corps (34th, 36th, 85th and 88th Infantry Divisions)';
const FRENCH = 'Fifth Army: French Expeditionary Corps (2nd Moroccan and 3rd Algerian Divisions)';
const FRENCH_MAY = 'Fifth Army: French Expeditionary Corps (2nd Moroccan, 3rd Algerian, 4th Moroccan Mountain and 1st Free French Divisions)';
const X = 'Fifth Army: X Corps (British 46th, 56th and 7th Armoured Divisions)';
const IV = 'Fifth Army: IV Corps (91st Infantry, 6th South African Armoured and Brazilian 1st Divisions)';
const XIII_43 = 'Eighth Army: XIII Corps (5th, 50th and 78th Divisions)', XIII = 'Eighth Army: XIII Corps (4th, 78th and 6th Armoured Divisions)';
const XIII_MAY = 'Eighth Army: XIII Corps (4th, 78th, 8th Indian and 6th Armoured Divisions)';
const CANADIANS = 'Eighth Army: I Canadian Corps (1st Canadian Infantry and 5th Canadian Armoured Divisions)';
const NEW_ZEALAND = 'New Zealand Corps (2nd New Zealand and 4th Indian Divisions)', POLES = 'Eighth Army: Polish II Corps (3rd Carpathian and 5th Kresowa Divisions)';
const HAUCK = 'Group Hauck (305th and 334th Divisions)';

/** One war on the historical plan, remembered as each of these months opens, with Germany about to move. */
const opening: Record<number, string> = {};
{
  const g = createGame(3, null);
  for (let t = 1; t < AUG_44; t++) {
    runTurnAI(g);
    if ([JAN_44, FEB_44, MAR_44, APR_44, MAY_44, JUNE_44, JULY_44, AUG_44].includes(t + 1)) opening[t + 1] = saveGame(g);
  }
}
/** A fresh copy of the war as that month opens, for a test to play on from. */
const at = (t: number): Game => loadGame(opening[t])!;
/** Lets the computer play until it is this nation's move. */
const until = (g: Game, nat: string): void => { let guard = 0; while (current(g) !== nat && guard++ < 60) stepAI(g); };
/** Plays this nation's month and stops there. */
const play = (g: Game, nat: string): void => { until(g, nat); stepAI(g); };
/** Opens this nation's month (arrivals, works) without giving any orders. */
const open = (g: Game, nat: string): void => { until(g, nat); g.week = 1; startPhase(g, nat); };

const named = (g: Game, name: string): Corps | undefined => g.corps.find((c) => c.name === name && !c.dead);
const place = (g: Game, name: string): string | null => { const c = named(g, name); return c ? g.regions[c.reg].name : null; };
const told = (g: Game, re: RegExp): { turn: number; week: number }[] => g.headlines.filter((h) => re.test(h.text)).map((h) => ({ turn: h.turn, week: h.week }));
const fought = (g: Game, nat: string, from: number): { place: string; taken: boolean }[] =>
  g.reports.slice(from).filter((r) => r.attNat === nat).map((r) => ({ place: r.regionName, taken: r.taken }));
const R = (g: Game, name: string): Region => g.byName[name];
const near = (g: Game, name: string): string[] => R(g, name).nb.map((i) => g.regions[i].name).sort();
const phase = (g: Game, nat: string): string | undefined => theatrePhases(g, nat).find((p) => p.theatre === 'Italy')?.phase?.name;
const germansAt = (g: Game, name: string): Corps[] => R(g, name).units.filter((u) => u.nat === 'GER');
/** Hands a place to a nation with nobody left in it. */
const give = (g: Game, name: string, nat: string): void => { for (const c of [...R(g, name).units]) removeCorps(g, c); capture(g, R(g, name), nat); };

describe('the ground of 1944', () => {
  it('Cassino is the mountain that bars the road from Naples to Rome, and Anzio a small port behind it', () => {
    const g = at(JAN_44);
    expect(R(g, 'Cassino').terrain).toBe('mountain');
    expect(near(g, 'Cassino')).toEqual(['Abruzzo', 'Anzio', 'Naples', 'Rome']);
    expect(R(g, 'Anzio').terrain).toBe('plains');
    expect(R(g, 'Anzio').port).toBeTruthy();
    expect(near(g, 'Anzio')).toEqual(['Cassino', 'Rome']);
    // Nobody walks from Naples to Rome any more without passing one or the other end of the Gustav Line.
    expect(near(g, 'Naples')).toEqual(['Abruzzo', 'Apulia', 'Cassino']);
    for (const nm of ['Cassino', 'Anzio', 'Emilia', 'Liguria']) expect(R(g, nm).theatre).toBe(R(g, 'Rome').theatre);
  });
});

describe('formations that grow, are made from others, and go home', () => {
  it('new divisions join a corps already at the front: it keeps what it has learned and takes the new name', () => {
    const g = at(JAN_44);
    const before = named(g, VI_43)!;
    const { id, xp, reg } = before;
    expect(before).toMatchObject({ kind: 'inf', divs: 2 });
    open(g, 'USA');
    const after = named(g, VI)!;
    expect(after).toMatchObject({ id, xp, reg, kind: 'mot', divs: 5 });
    expect(named(g, VI_43)).toBeUndefined();
    expect(g.corps.filter((c) => !c.dead && /^Fifth Army: VI Corps/.test(c.name))).toHaveLength(1);
  });

  it('the new divisions come at full strength, and the corps they join is as rested as it was', () => {
    const g = at(JAN_44);
    const c = named(g, VI_43)!;
    c.str = 0.5;
    open(g, 'USA');
    // Two worn divisions and three fresh ones, and whatever replacements the month brought the worn ones.
    expect(named(g, VI)!.str).toBeGreaterThanOrEqual((0.5 * 2 + 3) / 5);
    expect(named(g, VI)!.str).toBeLessThan(0.95);
  });

  it('if the corps they were meant for is gone, they form up on their own', () => {
    const g = at(JAN_44);
    removeCorps(g, named(g, VI_43)!);
    open(g, 'USA');
    expect(named(g, VI)).toMatchObject({ kind: 'mot', divs: 3 });
    expect(place(g, VI)).toBe('Naples');
  });

  it('a corps made by breaking up others strikes them off: the Seventh Army goes to England in January', () => {
    const g = at(JAN_44);
    for (const nm of SEVENTH) expect(named(g, nm)).toBeDefined();
    open(g, 'USA');
    for (const nm of SEVENTH) expect(named(g, nm)).toBeUndefined();
    expect(named(g, II)).toMatchObject({ kind: 'inf', divs: 2 });
    expect(place(g, II)).toBe('Naples');
    expect(place(g, FRENCH)).toBe('Naples');
    expect(told(g, /Eisenhower, Montgomery, Bradley and Patton have left the Mediterranean/)).toHaveLength(1);
  });

  it('the Eighth Army is rebuilt the same way, corps for corps', () => {
    const g = at(JAN_44);
    const old = named(g, XIII_43)!.id;
    open(g, 'UK');
    expect(named(g, XIII_43)).toBeUndefined();
    expect(named(g, XIII)).toMatchObject({ kind: 'inf', divs: 3 });
    expect(named(g, XIII)!.id).not.toBe(old);
    expect(place(g, XIII)).toBe('Naples');
    expect(place(g, CANADIANS)).toBe('Apulia');
  });

  it('April: two new American divisions, the French mountain troops and the Poles come into the line', () => {
    const g = at(APR_44);
    const id = named(g, II)!.id;
    open(g, 'USA');
    expect(named(g, II_MAY)).toMatchObject({ id, kind: 'inf', divs: 4 });
    expect(named(g, FRENCH_MAY)).toMatchObject({ kind: 'mtn', divs: 4 });
    // The British corps that had fought under the Fifth Army since Salerno leaves it.
    expect(named(g, X)).toBeUndefined();
    expect(told(g, /the 85th and the 88th, join the Fifth Army/)).toHaveLength(1);
    open(g, 'UK');
    expect(place(g, POLES)).toBe('Naples');
    expect(named(g, NEW_ZEALAND)).toBeUndefined();
    expect(named(g, XIII_MAY)).toMatchObject({ kind: 'inf', divs: 4 });
    expect(told(g, /General Anders brings the Polish II Corps into the line/)).toHaveLength(1);
  });

  it('formations only lent to the campaign leave it when their time is up: Dragoon takes two corps in August', () => {
    const g = at(AUG_44);
    for (const nm of [VI, FRENCH_MAY, II_MAY, IV]) expect(named(g, nm)).toBeDefined();
    open(g, 'USA');
    for (const nm of [VI, FRENCH_MAY]) expect(named(g, nm)).toBeUndefined();
    for (const nm of [II_MAY, IV]) expect(named(g, nm)).toBeDefined();
    expect(told(g, /American formations lent to the campaign in Italy leave for other fronts/)).toEqual([{ turn: AUG_44, week: 1 }]);
  });

  it('and not a month before', () => {
    const g = at(JULY_44);
    open(g, 'USA');
    for (const nm of [VI, FRENCH_MAY]) expect(named(g, nm)).toBeDefined();
    expect(told(g, /leave for other fronts/).filter((h) => h.turn >= JULY_44)).toHaveLength(0);
  });

  it('a lent corps that was destroyed is not news when its time comes', () => {
    const g = at(AUG_44);
    for (const nm of [VI, FRENCH_MAY]) removeCorps(g, named(g, nm)!);
    open(g, 'USA');
    expect(told(g, /leave for other fronts/).filter((h) => h.turn >= JULY_44)).toHaveLength(0);
  });
});

describe('a beachhead digs in', () => {
  it('Shingle: the landing finds Anzio empty, and the works go up as the month closes', () => {
    const g = at(JAN_44);
    until(g, 'USA');
    expect(R(g, 'Anzio').ctrl).toBe('GER');
    expect(R(g, 'Anzio').fort ?? null).toBeNull();
    const seen = g.reports.length;
    stepAI(g);
    expect(R(g, 'Anzio').ctrl).toBe('USA');
    expect(place(g, VI)).toBe('Anzio');
    expect(R(g, 'Anzio').fort).toMatchObject({ lv: 2 });
    expect(told(g, /The beachhead at Anzio is dug in/)).toHaveLength(1);
    // Nobody was there to fight for it.
    expect(fought(g, 'USA', seen).filter((f) => f.place === 'Anzio')).toEqual([]);
  });

  it('works of that kind are dug by whoever holds the place when their own month closes, and only in the month named', () => {
    const g = at(JAN_44);
    give(g, 'Anzio', 'USA');
    endPhase(g, 'UK');
    expect(R(g, 'Anzio').fort ?? null).toBeNull();
    endPhase(g, 'USA');
    expect(R(g, 'Anzio').fort).toMatchObject({ lv: 2 });
    endPhase(g, 'USA');
    expect(told(g, /The beachhead at Anzio is dug in/)).toHaveLength(1);
    // A beachhead won a month late gets nothing.
    const late = at(FEB_44);
    late.byName['Anzio'].fort = null;
    endPhase(late, 'USA');
    expect(R(late, 'Anzio').fort ?? null).toBeNull();
  });

  it('they are not dug for the Germans, who never lost the place', () => {
    const g = at(JAN_44);
    endPhase(g, 'GER');
    expect(R(g, 'Anzio').fort ?? null).toBeNull();
  });

  it('the ordinary kind are still dug as the month opens and not as it closes: the Gothic Line in July', () => {
    const g = at(JULY_44);
    endPhase(g, 'GER');
    expect(R(g, 'Emilia').fort ?? null).toBeNull();
    open(g, 'GER');
    expect(R(g, 'Emilia').fort).toMatchObject({ lv: 3 });
    expect(R(g, 'Liguria').fort).toMatchObject({ lv: 2 });
    expect(told(g, /building the Gothic Line across the northern Apennines/)).toHaveLength(1);
  });

  it('an attack on the beachhead from Rome meets the works head on', () => {
    const g = at(FEB_44);
    const beach = R(g, 'Anzio'), force = germansAt(g, 'Rome');
    expect(force.length).toBeGreaterThan(0);
    const fort = beach.fort;
    const withWorks = assess(g, force, beach).odds;
    beach.fort = null;
    const without = assess(g, force, beach).odds;
    beach.fort = fort;
    expect(without / withWorks).toBeCloseTo(1.7, 2);
  });
});

describe('history\'s plan for Italy in 1944', () => {
  it('names each month\'s business', () => {
    expect(['GER', 'UK', 'USA'].map((nat) => phase(at(JAN_44), nat))).toEqual(['The Gustav Line and the beachhead', 'The Adriatic front in winter', 'Operation Shingle']);
    expect(['GER', 'UK', 'USA'].map((nat) => phase(at(FEB_44), nat))).toEqual(['Operation Fischfang', 'The second battle of Cassino', 'The beachhead']);
    expect(['GER', 'UK', 'USA'].map((nat) => phase(at(APR_44), nat))).toEqual(['The Gustav Line and the beachhead', 'The Eighth Army moves west', 'The beachhead']);
    expect(['GER', 'UK', 'USA'].map((nat) => phase(at(MAY_44), nat))).toEqual(['The Gustav Line and the beachhead', 'Operation Diadem', 'Operation Diadem']);
    expect(['GER', 'UK', 'USA'].map((nat) => phase(at(JUNE_44), nat))).toEqual(['The retreat north of Rome', 'Up the Adriatic', 'The road to Rome']);
    expect(['GER', 'UK', 'USA'].map((nat) => phase(at(AUG_44), nat))).toEqual(['The Gothic Line', 'Ancona', 'To the Arno']);
  });

  it('there is no counter-attack without a beachhead to attack', () => {
    const g = at(FEB_44);
    give(g, 'Anzio', 'GER');
    expect(phase(g, 'GER')).toBe('The Gustav Line and the beachhead');
    const seen = g.reports.length;
    play(g, 'GER');
    expect(fought(g, 'GER', seen)).toEqual([]);
  });

  it('Fischfang: the Germans go for the beachhead once, in February, and are beaten off', () => {
    const g = at(FEB_44);
    const seen = g.reports.length;
    play(g, 'GER');
    expect(fought(g, 'GER', seen)).toEqual([{ place: 'Anzio', taken: false }]);
    expect(R(g, 'Anzio').ctrl).toBe('USA');
    // March: the line is held and nothing more is risked.
    const march = at(MAR_44);
    const from = march.reports.length;
    play(march, 'GER');
    expect(fought(march, 'GER', from)).toEqual([]);
  });

  it('Cassino is attacked once a month through the winter, and not at all in April', () => {
    for (const [t, by] of [[JAN_44, 'USA'], [FEB_44, 'UK'], [MAR_44, 'UK']] as [number, string][]) {
      const g = at(t);
      const seen = g.reports.length;
      play(g, 'USA');
      const at_ = g.reports.slice(seen).filter((r) => r.regionName === 'Cassino');
      expect(at_.map((r) => [r.attNat, r.taken])).toEqual([[by, false]]);
    }
    const g = at(APR_44);
    const seen = g.reports.length;
    play(g, 'USA');
    expect(g.reports.slice(seen).filter((r) => ['Cassino', 'Anzio', 'Abruzzo'].includes(r.regionName))).toEqual([]);
  });

  it('the retreat waits for the Gustav Line to break', () => {
    // June, but with Cassino still German: the line is held.
    const held = at(JUNE_44);
    give(held, 'Cassino', 'GER');
    expect(phase(held, 'GER')).toBe('The Gustav Line and the beachhead');
    // August, with Cassino gone but Rome still German: the army is on its way north, and not yet on the Gothic Line.
    const late = at(AUG_44);
    give(late, 'Rome', 'GER');
    expect(phase(late, 'GER')).toBe('The retreat north of Rome');
  });

  it('once it breaks the Germans give up Rome and the mountains without a fight, and the garrisons go with them', () => {
    const g = at(JUNE_44);
    expect(named(g, HAUCK)!.hold).toBeTruthy();
    expect(place(g, HAUCK)).toBe('Abruzzo');
    const seen = g.reports.length;
    play(g, 'GER');
    for (const nm of ['Cassino', 'Anzio', 'Abruzzo', 'Rome']) expect(germansAt(g, nm)).toHaveLength(0);
    expect(place(g, HAUCK)).toBe('Marche');
    expect(named(g, HAUCK)!.hold).toBeFalsy();
    expect(germansAt(g, 'Tuscany').length + germansAt(g, 'Marche').length).toBeGreaterThanOrEqual(8);
    expect(fought(g, 'GER', seen)).toEqual([]);
    // The Allies walk in.
    play(g, 'USA');
    expect(R(g, 'Rome').ctrl).toBe('USA');
    expect(['UK', 'USA']).toContain(R(g, 'Abruzzo').ctrl);
    expect(g.reports.slice(seen).filter((r) => r.regionName === 'Rome')).toEqual([]);
  });

  it('the main army is not railed into Italy: the theatre fights with what it was sent', () => {
    const g = at(JAN_44);
    const k = R(g, 'Rome').theatre;
    const outside = new Set(g.corps.filter((c) => c.nat === 'GER' && !c.dead && g.regions[c.reg].theatre !== k).map((c) => c.id));
    play(g, 'GER');
    expect(g.corps.filter((c) => !c.dead && outside.has(c.id) && g.regions[c.reg].theatre === k)).toEqual([]);
  });
});

describe('the armies of 1944', () => {
  it('the Luftwaffe leaves the front in March, and the Allied and German armies are re-rated in May', () => {
    expect(at(FEB_44).nations.GER.air).toBe(100);
    const april = at(APR_44);
    expect(april.nations.GER.air).toBe(45);
    expect(april.flags['event:luftwaffe-1944']).toBe(MAR_44);
    expect(april.nations.USA).toMatchObject({ quality: 0.9, air: 70 });
    expect(april.nations.GER.quality).toBe(1.25);
    const may = at(MAY_44);
    expect(may.nations.USA).toMatchObject({ quality: 1, air: 140 });
    expect(may.nations.UK).toMatchObject({ quality: 1.05, air: 90 });
    expect(may.nations.GER).toMatchObject({ quality: 1.15, air: 45 });
    expect(told(may, /The Allied armies of 1944 are not those of 1942/)).toEqual([{ turn: MAY_44, week: 1 }]);
  });

  it('none of it happens to a Germany that is not at war with America', () => {
    const g = at(FEB_44);
    g.nations.GER.atWar = g.nations.GER.atWar.filter((x) => x !== 'USA');
    g.nations.USA.atWar = g.nations.USA.atWar.filter((x) => x !== 'GER');
    g.turn = MAR_44;
    monthlyDiplomacy(g);
    expect(g.nations.GER.air).toBe(100);
    expect(g.flags['event:luftwaffe-1944']).toBeUndefined();
  });

  it('the fall of Cassino and of Rome is news of the month it happened in', () => {
    // History: Cassino fell in May and Rome in June. The news is read as the next month opens, and dated to the last.
    const june = at(JUNE_44);
    expect(told(june, /The Gustav Line is broken/)).toEqual([{ turn: MAY_44, week: 4 }]);
    expect(june.flags['event:gustav-line-broken']).toBe(JUNE_44);
    const july = at(JULY_44);
    expect(told(july, /Rome is free/)).toEqual([{ turn: JUNE_44, week: 4 }]);
    // It is told once.
    monthlyDiplomacy(july);
    expect(told(july, /Rome is free/)).toHaveLength(1);
  });

  it('there is no such news in a war where Italy never changed sides', () => {
    const g = at(MAY_44);
    delete g.flags['event:italian-armistice'];
    give(g, 'Cassino', 'USA');
    g.turn = JUNE_44;
    monthlyDiplomacy(g);
    expect(told(g, /The Gustav Line is broken/)).toHaveLength(0);
    g.flags['event:italian-armistice'] = 49;
    monthlyDiplomacy(g);
    expect(told(g, /The Gustav Line is broken/)).toHaveLength(1);
  });
});
