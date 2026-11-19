// The West in 1944: a theatre that does not exist until its month comes; works that face the sea and a harbour
// built off an open beach; tactical air forces whose fighters reach the invasion coast; armies that wait on a
// defeat before they are re-formed; a boundary between allies; garrisons left behind for others to watch; and
// ground an occupier has walked away from, handed back.
import { describe, expect, it } from 'vitest';
import type { AirFleet, Corps, Game, Region } from '../src/campaign/types';
import { addCorps, assess, capture, createGame, removeCorps, startPhase } from '../src/campaign/game';
import { theatrePhases } from '../src/campaign/ai';
import { airRange, assessAir } from '../src/campaign/air';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { current, runTurnAI, stepAI } from '../src/campaign/turn';
import theatres from '../data/campaign/theatres.json';

const APR_44 = 56, MAY_44 = 57, JUNE_44 = 58, JULY_44 = 59, AUG_44 = 60, SEPT_44 = 61, OCT_44 = 62, NOV_44 = 63, DEC_44 = 64, JAN_45 = 65;
const MONTHS = [APR_44, MAY_44, JUNE_44, JULY_44, AUG_44, SEPT_44, OCT_44, NOV_44, DEC_44, JAN_45];
const WEST = (theatres as any).west as { regions: string[]; plans: Record<string, { entrust?: Record<string, string[]>; mask?: string[] }[]> };
const BRITISH = WEST.plans.USA.find((p) => p.entrust)!.entrust!.UK, AMERICAN = WEST.plans.UK.find((p) => p.entrust)!.entrust!.USA;

const UK_I = 'Second Army: I Corps (3rd British, 3rd Canadian and 6th Airborne Divisions)';
const UK_XXX = 'Second Army: XXX Corps (50th, 49th and 7th Armoured Divisions)';
const UK_VIII = 'Second Army: VIII Corps (11th Armoured, Guards Armoured and 15th Scottish Divisions)';
const US_V = 'First Army: V Corps (1st, 2nd and 29th Infantry Divisions)';
const US_VII = 'First Army: VII Corps (4th, 9th and 90th Infantry, 82nd and 101st Airborne Divisions)';
const US_XV = 'Third Army: XV Corps (5th Armored, French 2nd Armored and 79th Infantry Divisions)';
const FIFTEENTH = 'Fifteenth Army: LXXXI and LXXXII Corps', FIFTEENTH_SEPT = 'Fifteenth Army: LXVII and LXXXVIII Corps';
const STUDENT = 'First Parachute Army: II Parachute Corps and LXXXVI Corps';
const SIXTH_PANZER = 'Sixth Panzer Army: I SS Panzer Corps (1st and 12th SS Panzer Divisions)';

/** One war on the historical plan, remembered as each of these months opens, with Germany about to move. */
const opening: Record<number, string> = {};
{
  const g = createGame(3, null);
  for (let t = 1; t < JAN_45; t++) {
    runTurnAI(g);
    if (MONTHS.includes(t + 1)) opening[t + 1] = saveGame(g);
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
const told = (g: Game, re: RegExp): number[] => g.headlines.filter((h) => re.test(h.text)).map((h) => h.turn);
/** The battles a nation has fought in the West since a point in the record: Italy's are another story. */
const fought = (g: Game, nat: string, from: number): { place: string; taken: boolean }[] =>
  g.reports.slice(from).filter((r) => r.attNat === nat && WEST.regions.includes(r.regionName)).map((r) => ({ place: r.regionName, taken: r.taken }));
const R = (g: Game, name: string): Region => g.byName[name];
const phase = (g: Game, nat: string): string | undefined => theatrePhases(g, nat).find((p) => p.theatre === 'the West')?.phase?.name;
const fleet = (g: Game, name: string): AirFleet => g.airFleets.find((f) => f.name === name)!;
/** Hands a place to a nation with nobody left in it. */
const give = (g: Game, name: string, nat: string): void => { for (const c of [...R(g, name).units]) removeCorps(g, c); capture(g, R(g, name), nat); };
const works = (g: Game, attackers: Corps[], name: string): string => assess(g, attackers, R(g, name)).lines.find((l) => l.label === 'Fortifications')?.effect ?? '';

describe('a theatre that opens in its own month', () => {
  it('until May 1944 the West is no theatre: its ground belongs to the main fronts, as it did in 1940', () => {
    const g = at(APR_44);
    for (const nm of ['Normandy', 'Paris', 'Rhineland', 'Hampshire', 'Brussels']) expect(R(g, nm).theatre).toBe(-1);
    expect(phase(g, 'GER')).toBeUndefined();
    expect(phase(g, 'UK')).toBeUndefined();
  });

  it('it opens as May does, with every nation that has troops there on its plan', () => {
    const g = at(MAY_44);
    const k = R(g, 'Normandy').theatre;
    expect(k).toBeGreaterThanOrEqual(0);
    for (const nm of WEST.regions) expect(R(g, nm).theatre).toBe(k);
    // Ground beside it stays where it was.
    expect(R(g, 'Kent').theatre).toBe(-1);
    expect(R(g, 'Bavaria').theatre).toBe(-1);
    expect(phase(g, 'GER')).toBe('The Atlantic Wall');
    open(g, 'UK');
    expect(phase(g, 'UK')).toBe('Waiting for the weather');
  });

  it('a saved game remembers that it is open', () => {
    const g = loadGame(saveGame(at(JULY_44)))!;
    expect(R(g, 'Normandy').theatre).toBeGreaterThanOrEqual(0);
    expect(phase(g, 'GER')).toBe('The ring round the beachhead');
  });

  it('in free play it never opens, and nothing history sent there is sent', () => {
    const g = createGame(5, null);
    startOpen(g);
    while (g.turn <= JUNE_44) runTurnAI(g);
    expect(R(g, 'Normandy').theatre).toBe(-1);
    expect(R(g, 'Normandy').port).toBe(false);
    expect(g.corps.some((c) => [UK_I, UK_XXX, US_VII, STUDENT].includes(c.name))).toBe(false);
    // No invasion air forces, and no Atlantic Wall dug by the calendar.
    expect(g.airFleets.some((f) => ['Second Tactical Air Force', 'US Ninth Air Force'].includes(f.name))).toBe(false);
    expect(R(g, 'Calais').fort).toBeNull();
    expect(R(g, 'Normandy').fort).toBeNull();
    expect(told(g, /Atlantic Wall|mined and staked|phantom army/)).toEqual([]);
  }, 60000);
});

describe('the Atlantic Wall and the harbour off the beach', () => {
  it('the works of May 1944 face the sea: a landing from England meets them, an army already ashore does not', () => {
    const g = at(JUNE_44);
    expect(R(g, 'Calais').fort).toEqual({ lv: 2, vs: ['UK'] });
    expect(R(g, 'Normandy').fort).toEqual({ lv: 1, vs: ['UK'] });
    expect(works(g, [named(g, UK_I)!], 'Calais')).toMatch(/head-on/);
    // The same corps, once it stands on French ground next door.
    const ashore = addCorps(g, 'UK', 'A corps already ashore', 'inf', 3, 'Somme', {});
    expect(works(g, [ashore], 'Calais')).toMatch(/face the other way/);
  });

  it('the West Wall is re-armed in September, and still faces the way it did in 1939', () => {
    const before = at(SEPT_44), g = at(SEPT_44);
    expect(R(before, 'Rhineland').fort?.lv).toBe(1);
    open(g, 'GER');
    for (const nm of ['Rhineland', 'Eifel', 'Saar', 'Gelderland']) expect(R(g, nm).fort?.lv).toBe(3);
    expect(R(g, 'Rhineland').fort?.vs).toEqual(R(before, 'Rhineland').fort?.vs);
    expect(R(g, 'Gelderland').fort?.vs).toEqual(['NED']);
  });

  it('Normandy has no port until the Allies build one: the harbour goes in as the month of the landing closes', () => {
    const g = at(JUNE_44);
    expect(R(g, 'Normandy').port).toBe(false);
    play(g, 'UK');
    expect(R(g, 'Normandy').ctrl).toBe('UK');
    expect(R(g, 'Normandy').port).toBe(true);
    expect(g.flags[`harbour:${R(g, 'Normandy').id}`]).toBe(JUNE_44);
    expect(told(g, /D-Day, 6 June 1944/)).toEqual([JUNE_44]);
    // The works left facing the sea are no use to the men who came from it: the beachhead faces inland.
    expect(R(g, 'Normandy').fort).toEqual({ lv: 1, vs: ['FRA'] });
  });

  it('a saved game keeps the harbour, and a new war does not have it', () => {
    const g = loadGame(saveGame(at(JULY_44)))!;
    expect(R(g, 'Normandy').port).toBe(true);
    expect(R(createGame(3, null), 'Normandy').port).toBe(false);
  });

  it('the follow-up corps come in over it', () => {
    const g = at(JULY_44);
    expect(named(g, UK_VIII)).toBeUndefined();
    open(g, 'UK');
    expect(place(g, UK_VIII)).toBe('Normandy');
  });

  it('the Germans do not build it: a beachhead they kept has no harbour', () => {
    const g = at(JUNE_44);
    for (const c of g.corps.filter((x) => [UK_I, UK_XXX].includes(x.name))) removeCorps(g, c);
    play(g, 'UK');
    expect(R(g, 'Normandy').ctrl).toBe('GER');
    expect(R(g, 'Normandy').port).toBe(false);
  });

  it('if the landing is put off a month, so is the harbour', () => {
    const g = at(JUNE_44);
    // The weather does not break: nobody sails in June.
    for (const c of g.corps.filter((x) => [UK_I, UK_XXX, US_V, US_VII].includes(x.name))) removeCorps(g, c);
    while (g.turn < JULY_44) stepAI(g);
    expect(R(g, 'Normandy').ctrl).toBe('GER');
    expect(R(g, 'Normandy').port).toBe(false);
    addCorps(g, 'UK', UK_I, 'inf', 3, 'Hampshire', { xp: 0.3 });
    addCorps(g, 'UK', UK_XXX, 'mot', 3, 'Hampshire', { xp: 0.4 });
    until(g, 'UK');
    expect(phase(g, 'UK')).toBe('Operation Overlord');
    stepAI(g);
    expect(R(g, 'Normandy').ctrl).toBe('UK');
    expect(g.flags[`harbour:${R(g, 'Normandy').id}`]).toBe(JULY_44);
    expect(told(g, /The Allies are ashore in Normandy/)).toEqual([JULY_44]);
  });
});

describe('the tactical air forces', () => {
  it('form in England in April with fighters that reach the invasion coast, which Fighter Command\'s do not', () => {
    const g = at(MAY_44);
    const taf = fleet(g, 'Second Tactical Air Force'), ninth = fleet(g, 'US Ninth Air Force');
    expect(g.regions[taf.base].name).toBe('Hampshire');
    expect(g.regions[ninth.base].name).toBe('West Country');
    expect(airRange(taf).ftr).toBe(400);
    expect(airRange(ninth).ftr).toBe(600);
    const home = g.airFleets.find((f) => f.nat === 'UK' && f.name !== taf.name && f.name !== ninth.name)!;
    expect(airRange(home).ftr).toBeLessThan(airRange(taf).ftr);
    expect(airRange(home).bmr).toBe(airRange(taf).bmr);
  });

  it('over Normandy its fighters count, and the same aircraft under an ordinary command would not', () => {
    const g = at(MAY_44);
    open(g, 'UK');
    const taf = fleet(g, 'Second Tactical Air Force');
    const there = assessAir(g, [taf], 'superiority', R(g, 'Normandy').id);
    expect(there.why).toBeNull();
    expect(there.E).toBeGreaterThan(0);
    const plain = assessAir(g, [{ ...taf, name: 'An ordinary command' }], 'superiority', R(g, 'Normandy').id);
    expect(plain.E).toBe(0);
  });

  it('they win the sky over both beaches in May, and the news calls it the Allies\'', () => {
    const g = at(JUNE_44);
    const won = g.airReports.filter((a) => a.turn === MAY_44 && a.mission === 'superiority' && a.sky).map((a) => a.place).sort();
    expect(won).toEqual(expect.arrayContaining(['Cotentin', 'Normandy']));
    expect(told(g, /fights for the sky over Normandy.*The Allied air forces now hold the sky there/)).toEqual([MAY_44]);
  });
});

describe('history\'s plan for the West', () => {
  it('names each month\'s business for the British', () => {
    const names = [MAY_44, JUNE_44, JULY_44, AUG_44, SEPT_44, OCT_44, NOV_44, DEC_44].map((t) => { const g = at(t); until(g, 'UK'); return phase(g, 'UK'); });
    expect(names).toEqual(['Waiting for the weather', 'Operation Overlord', 'Operation Goodwood', 'Falaise and the Seine', 'The pursuit to Antwerp, and Market Garden', 'Market Garden and the Scheldt', 'Clearing the Scheldt', 'Winter on the Maas']);
  });

  it('and for the Americans', () => {
    const names = [MAY_44, JUNE_44, JULY_44, AUG_44, SEPT_44, OCT_44, NOV_44, DEC_44, JAN_45].map((t) => { const g = at(t); if (t === MAY_44) open(g, 'USA'); else until(g, 'USA'); return phase(g, 'USA'); });
    expect(names).toEqual(['Waiting for the weather', 'Operation Overlord', 'Operation Cobra', 'The breakout', 'To the German frontier', 'Aachen and the mud of Lorraine', 'Metz, Strasbourg and the Hurtgen Forest', 'The quiet front', 'Closing the Bulge']);
  });

  it('the German plan waits on the ground: the wall until someone lands, the ring until Paris goes, then the retreat', () => {
    const names = [MAY_44, JUNE_44, JULY_44, AUG_44, SEPT_44, OCT_44, NOV_44, DEC_44, JAN_45].map((t) => phase(at(t), 'GER'));
    expect(names).toEqual(['The Atlantic Wall', 'The Atlantic Wall', 'The ring round the beachhead', 'The ring round the beachhead', 'The retreat to the West Wall', 'The retreat to the West Wall', 'The West Wall', 'The Ardennes offensive', 'Holding the Bulge']);
  });

  it('with no beachhead there is no ring, and with Paris still German no retreat and no Ardennes offensive', () => {
    const july = at(JULY_44);
    give(july, 'Normandy', 'GER'); give(july, 'Cotentin', 'GER');
    expect(phase(july, 'GER')).toBe('The Atlantic Wall');
    const dec = at(DEC_44);
    give(dec, 'Paris', 'GER');
    expect(phase(dec, 'GER')).toBe('The ring round the beachhead');
  });

  it('Goodwood is one attack, made at long odds, and it fails', () => {
    const g = at(JULY_44);
    until(g, 'UK');
    const seen = g.reports.length;
    stepAI(g);
    expect(fought(g, 'UK', seen)).toEqual([{ place: 'Rouen', taken: false }]);
  });

  it('in August the German armour goes west, and the British break the line it left', () => {
    const g = at(AUG_44);
    const tanks = (nm: string): number => R(g, nm).units.filter((u) => u.nat === 'GER' && u.kind === 'arm').length;
    const before = tanks('Rouen');
    expect(before).toBeGreaterThanOrEqual(2);
    play(g, 'GER');
    expect(tanks('Rouen')).toBe(0);
    expect(tanks('Maine')).toBeGreaterThanOrEqual(4);
    const seen = g.reports.length;
    play(g, 'UK');
    expect(fought(g, 'UK', seen)).toEqual([{ place: 'Rouen', taken: true }]);
  });

  it('Market Garden is tried once, and the West Wall at Aachen once', () => {
    const g = at(OCT_44);
    until(g, 'UK');
    let seen = g.reports.length;
    stepAI(g);
    expect(fought(g, 'UK', seen).filter((f) => f.place === 'Gelderland')).toEqual([{ place: 'Gelderland', taken: false }]);
    until(g, 'USA');
    seen = g.reports.length;
    stepAI(g);
    expect(fought(g, 'USA', seen).filter((f) => f.place === 'Rhineland')).toEqual([{ place: 'Rhineland', taken: false }]);
  });

  it('the Ardennes offensive is one stroke at one place, and in January it is undone', () => {
    const g = at(DEC_44);
    let seen = g.reports.length;
    play(g, 'GER');
    expect(place(g, SIXTH_PANZER)).not.toBeNull();
    expect(fought(g, 'GER', seen)).toEqual([{ place: 'Ardennes', taken: true }]);
    while (g.turn < JAN_45) stepAI(g);
    expect(R(g, 'Ardennes').ctrl).toBe('GER');
    until(g, 'USA');
    seen = g.reports.length;
    stepAI(g);
    expect(fought(g, 'USA', seen)).toEqual([{ place: 'Ardennes', taken: true }]);
  });
});

describe('armies that wait on a defeat', () => {
  it('the armies of the West Wall are put together in September from what came out of France', () => {
    const g = at(SEPT_44);
    expect(named(g, FIFTEENTH)).toBeDefined();
    expect(named(g, FIFTEENTH_SEPT)).toBeUndefined();
    open(g, 'GER');
    expect(named(g, FIFTEENTH)).toBeUndefined();
    expect(place(g, FIFTEENTH_SEPT)).toBe('Rotterdam');
    expect(place(g, STUDENT)).toBe('Gelderland');
  });

  it('but only once Paris is lost: while it holds, the summer\'s formations fight on where they stand', () => {
    const g = at(SEPT_44);
    give(g, 'Paris', 'GER');
    open(g, 'GER');
    expect(place(g, FIFTEENTH)).toBe('Calais');
    expect(named(g, FIFTEENTH_SEPT)).toBeUndefined();
    expect(named(g, STUDENT)).toBeUndefined();
    // The works do not wait: the wall is re-armed by the calendar.
    expect(R(g, 'Rhineland').fort?.lv).toBe(3);
  });

  it('if Paris goes a month late, so do they', () => {
    const g = at(SEPT_44);
    give(g, 'Paris', 'GER');
    open(g, 'GER');
    expect(named(g, STUDENT)).toBeUndefined();
    give(g, 'Paris', 'USA');
    g.turn = OCT_44;
    startPhase(g, 'GER');
    expect(place(g, STUDENT)).toBe('Gelderland');
    expect(named(g, FIFTEENTH)).toBeUndefined();
  });
});

describe('the boundary between the army groups', () => {
  it('each ally closes up to its own front: by the winter nobody stands on the wrong side of the line', () => {
    for (const t of [DEC_44, JAN_45]) {
      const g = at(t);
      const where = (nat: string): string[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort').map((c) => g.regions[c.reg].name);
      expect(where('UK').filter((nm) => AMERICAN.includes(nm))).toEqual([]);
      expect(where('USA').filter((nm) => BRITISH.includes(nm))).toEqual([]);
    }
  });

  it('the boundary is drawn from September, when the armies have room to have one', () => {
    for (const nat of ['UK', 'USA']) for (const p of WEST.plans[nat] as { from: number; entrust?: unknown }[]) expect(!!p.entrust).toBe(p.from >= SEPT_44);
    // Between them the two sectors cover the ground: nothing is in both.
    expect(BRITISH.filter((nm) => AMERICAN.includes(nm))).toEqual([]);
  });
});

describe('garrisons left for others to watch', () => {
  it('the corps that reached the Biscay coast in August does not stay to watch Bordeaux', () => {
    const g = at(SEPT_44);
    expect(place(g, US_XV)).toBe('Poitou');
    expect(R(g, 'Bordeaux').units.some((u) => u.nat === 'GER')).toBe(true);
    const seen = g.reports.length;
    play(g, 'USA');
    expect(place(g, US_XV)).not.toBe('Poitou');
    // Nor is the garrison attacked: it is left to starve.
    expect(fought(g, 'USA', seen).some((f) => f.place === 'Bordeaux')).toBe(false);
    expect(R(g, 'Bordeaux').ctrl).toBe('GER');
  });

  it('without that order it would have stood there all autumn', () => {
    const g = at(SEPT_44);
    for (const p of WEST.plans.USA) delete p.mask;
    try {
      play(g, 'USA');
      expect(place(g, US_XV)).toBe('Poitou');
    } finally {
      for (const p of WEST.plans.USA as { from: number; mask?: string[] }[]) if (p.from >= AUG_44) p.mask = ['Bordeaux'];
    }
  });
});

describe('ground the occupier has left', () => {
  it('in October the empty south and centre of France pass to the Allies, and the places still garrisoned do not', () => {
    const before = at(SEPT_44);
    until(before, 'SOV');
    const empty = WEST.regions.filter((nm) => ['FRA', 'VIC'].includes(R(before, nm).owner) && R(before, nm).ctrl === 'GER' && !R(before, nm).units.some((u) => u.nat === 'GER'));
    expect(empty.length).toBeGreaterThanOrEqual(5);
    const g = at(OCT_44);
    expect(told(g, /France has freed itself/)).toEqual([SEPT_44]);
    for (const nm of ['Toulouse', 'Gascony', 'Limousin', 'Auvergne']) expect(['USA', 'UK']).toContain(R(g, nm).ctrl);
    expect(R(g, 'Bordeaux').ctrl).toBe('GER');
    // The four places the autumn battles are fought for are not given away.
    for (const nm of ['Lorraine', 'Alsace', 'Vosges', 'Belfort']) expect(R(g, nm).ctrl).toBe('GER');
  });

  it('by the new year the pockets have gone the same way: no ground in France is German', () => {
    const g = at(JAN_45);
    expect(WEST.regions.filter((nm) => ['FRA', 'VIC'].includes(R(g, nm).owner) && R(g, nm).ctrl === 'GER')).toEqual([]);
  });
});

describe('how an arrival is told', () => {
  it('troops who crossed the sea land; an army that marched there, or is forming at home, arrives', () => {
    const g = at(JUNE_44);
    expect(told(g, /Omar Bradley lands at Cornwall/)).toEqual([MAY_44]);
    expect(told(g, /Miles Dempsey arrives in Hampshire/)).toEqual([MAY_44]);
    expect(told(g, /Hans von Salmuth arrives in Calais/)).toEqual([MAY_44]);
    expect(told(g, /lands at Calais|lands at Hampshire/)).toEqual([]);
  });
});
