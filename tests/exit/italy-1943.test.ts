// Sicily and Italy, run against the campaign rules with every nation on the historical plan and nobody at
// the controls. In June 1943 an Allied air command forms at Malta and wins the sky over Sicily. In July the
// Italian army loses heart, the Eighth Army lands in Sicily from Malta and the Seventh Army follows it
// ashore. Mussolini falls as August opens and Italy signs an armistice as September opens: Germany takes
// over Rome and the north, the Allies get Sardinia and Corsica. In September the Germans leave the south
// for the mountains below Rome, the Eighth Army crosses the strait and lands in the heel, and the Fifth
// Army lands at Salerno and takes Naples. From October the Gustav Line is dug, and it holds to the year's end.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { allied, createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

const JUNE_43 = 46, JULY_43 = 47, AUG_43 = 48, SEPT_43 = 49, OCT_43 = 50, DEC_43 = 52;
/** The ground the campaign is fought over, with Malta, where the invasion gathers. */
const ITALY = ['Malta', 'Sicily', 'Calabria', 'Apulia', 'Naples', 'Abruzzo', 'Rome', 'Marche', 'Tuscany', 'Sardinia', 'Corsica'];
const SOUTH = ['Calabria', 'Apulia', 'Naples'];
const NORTH = ['Rome', 'Tuscany', 'Marche', 'Lombardy', 'Liguria'];
const AFRICA = ['Tunis', 'Algiers', 'Oran', 'Tripoli', 'Alexandria'];
const EIGHTH = ['Eighth Army: XIII Corps (5th, 50th and 78th Divisions)', 'Eighth Army: XXX Corps (51st, 1st Canadian and 1st Airborne Divisions)'];
const SEVENTH = ['Seventh Army: II Corps (1st, 9th and 45th Infantry Divisions)', 'Seventh Army: Provisional Corps (3rd Infantry, 2nd Armored and 82nd Airborne Divisions)'];
const FIFTH = ['Fifth Army: VI Corps (36th and 34th Infantry Divisions)', 'Fifth Army: X Corps (British 46th, 56th and 7th Armoured Divisions)'];

interface Fight { turn: number; place: string; by: string; odds: number; taken: boolean; attLost: number; defLost: number }
interface Month {
  /** Who holds each place as the month ends. */
  held: Record<string, string>;
  /** Each side's corps and divisions on the ground the campaign is fought over, and where the Germans stand. */
  allies: { corps: number; divs: number; men: number };
  germans: { corps: number; divs: number; at: Record<string, number> };
  italians: number;
  /** British and American divisions still in Africa. */
  africa: { UK: number; USA: number };
  works: number;
}

interface Run {
  g: Game;
  months: Record<number, Month>;
  fights: Fight[];
  /** How well the Italian army fights as each month opens. */
  quality: Record<number, number>;
  /** The month each piece of news came, or 0. */
  news: { heart: number; mussolini: number; armistice: number; gustav: number };
  /** Landings beaten off at the water's edge. */
  thrownBack: number;
  /** The fights for the sky over Sicily: the month, and whether the Allies won it. */
  sky: { turn: number; won: boolean }[];
  /** Where the invasion force stood as July ended, by corps. */
  husky: Record<string, string | null>;
  /** The air command as June ends: where it is based, or null if it never formed. */
  airBase: string | null;
  /** Italy's fleets and air fleets as September ends. */
  italianFleets: number;
  italianAir: number;
}

function history(seed: number): Run {
  const g = createGame(seed, null);
  const ids = new Set(ITALY.map((nm) => g.byName[nm].id));
  const run: Run = {
    g, months: {}, fights: [], quality: {}, news: { heart: 0, mussolini: 0, armistice: 0, gustav: 0 }, thrownBack: 0, sky: [], husky: {}, airBase: null, italianFleets: -1, italianAir: -1,
  };
  let seen = 0, read = 0, flown = 0;
  for (let t = 1; t <= DEC_43; t++) {
    // The month's diplomacy is settled as the month opens: read the Italian army before the next one does.
    if (t >= JUNE_43) run.quality[t] = g.nations.ITA.quality;
    runTurnAI(g);
    if (t >= JUNE_43) {
      for (const r of g.reports.slice(seen)) if (ids.has(r.region)) run.fights.push({ turn: t, place: r.regionName, by: r.attNat, odds: r.odds, taken: r.taken, attLost: r.attLost, defLost: r.defLost });
      for (const h of g.headlines.slice(read)) if (/thrown back into the sea/.test(h.text)) run.thrownBack++;
      for (const a of g.airReports.slice(flown)) if (a.mission === 'superiority' && a.place === 'Sicily' && a.attNat === 'UK') run.sky.push({ turn: t, won: a.sky });
      const here = g.corps.filter((c) => !c.dead && c.kind !== 'fort' && ids.has(c.reg));
      const of = (...nats: string[]) => here.filter((c) => nats.includes(c.nat));
      const at: Record<string, number> = {};
      for (const c of of('GER')) at[g.regions[c.reg].name] = (at[g.regions[c.reg].name] ?? 0) + c.divs;
      const inAfrica = (nat: string): number => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && g.regions[c.reg].theatre >= 0 && !ids.has(c.reg)).reduce((s, c) => s + c.divs, 0);
      run.months[t] = {
        held: Object.fromEntries([...ITALY, ...NORTH, ...AFRICA].map((nm) => [nm, g.byName[nm].ctrl])),
        allies: { corps: of('UK', 'USA').filter((c) => !c.hold).length, divs: of('UK', 'USA').filter((c) => !c.hold).reduce((s, c) => s + c.divs, 0), men: of('UK', 'USA').filter((c) => !c.hold).reduce((s, c) => s + men(c), 0) },
        germans: { corps: of('GER').length, divs: of('GER').reduce((s, c) => s + c.divs, 0), at },
        italians: g.corps.filter((c) => c.nat === 'ITA' && !c.dead).length,
        africa: { UK: inAfrica('UK'), USA: inAfrica('USA') },
        works: g.byName['Abruzzo'].fort?.lv ?? 0,
      };
    }
    seen = g.reports.length; read = g.headlines.length; flown = g.airReports.length;
    if (t === JUNE_43) {
      const f = g.airFleets.find((x) => x.name === 'Mediterranean Air Command');
      run.airBase = f ? g.regions[f.base].name : null;
    }
    if (t === JULY_43) for (const nm of [...EIGHTH, ...SEVENTH]) { const c = g.corps.find((x) => x.name === nm && !x.dead); run.husky[nm] = c ? g.regions[c.reg].name : null; }
    if (t === SEPT_43) { run.italianFleets = g.fleets.filter((f) => f.nat === 'ITA').length; run.italianAir = g.airFleets.filter((f) => f.nat === 'ITA').length; }
  }
  run.news = {
    heart: g.flags['event:italy-loses-heart'] ?? 0, mussolini: g.flags['event:mussolini-falls'] ?? 0, armistice: g.flags['event:italian-armistice'] ?? 0,
    gustav: g.headlines.find((h) => /Gustav Line/.test(h.text))?.turn ?? 0,
  };
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map(history);
const count = (ok: (r: Run) => boolean): number => runs.filter(ok).length;
const ours = (r: Run, t: number, ...places: string[]): boolean => places.every((nm) => allied(r.g, 'UK', r.months[t].held[nm]) || ['UK', 'USA'].includes(r.months[t].held[nm]));
const holds = (r: Run, t: number, nat: string, ...places: string[]): boolean => places.every((nm) => r.months[t].held[nm] === nat);
const landing = (r: Run, by: string, place: string): Fight | undefined => r.fights.find((f) => f.by === by && f.place === place);
/** Men each side lost in the battles of the campaign. */
const lost = (r: Run): { allies: number; axis: number } => {
  const out = { allies: 0, axis: 0 };
  for (const f of r.fights) { const a = f.by === 'UK' || f.by === 'USA'; out.allies += a ? f.attLost : f.defLost; out.axis += a ? f.defLost : f.attLost; }
  return out;
};

describe('exit test: Sicily and Italy on the historical plan', () => {
  it('June 1943: Italy still holds its own country, Africa is lost and Malta is British', () => {
    for (const r of runs) {
      expect(holds(r, JUNE_43, 'ITA', 'Sicily', 'Calabria', 'Apulia', 'Naples', 'Rome', 'Sardinia', 'Lombardy')).toBe(true);
      expect(r.months[JUNE_43].held['Malta']).toBe('UK');
      expect(r.months[JUNE_43].germans.corps).toBe(0);
    }
    expect(count((r) => ours(r, JUNE_43, ...AFRICA))).toBeGreaterThanOrEqual(18);
  });

  it('an Allied air command forms at Malta in June and wins the sky over Sicily before the landing', () => {
    expect(count((r) => r.airBase === 'Malta')).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.sky.some((s) => s.turn === JUNE_43 && s.won))).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.sky.some((s) => s.turn <= JULY_43 && s.won))).toBeGreaterThanOrEqual(19);
  });

  it('the Italian army loses heart as July opens', () => {
    expect(count((r) => r.quality[JUNE_43] === 0.75 && r.quality[JULY_43] === 0.45 && r.news.heart === JULY_43)).toBeGreaterThanOrEqual(18);
  });

  it('the invasion force is drawn from the armies in Africa: twelve divisions leave them in July', () => {
    for (const r of runs) for (const nm of [...EIGHTH, ...SEVENTH]) expect(r.husky[nm]).not.toBeNull();
    expect(count((r) => r.months[JUNE_43].africa.UK - r.months[JULY_43].africa.UK >= 5)).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.months[JUNE_43].africa.USA - r.months[JULY_43].africa.USA >= 5)).toBeGreaterThanOrEqual(17);
  });

  it('Husky: Sicily is taken from the sea in July, and both armies are ashore by the end of the month', () => {
    expect(count((r) => ours(r, JULY_43, 'Sicily'))).toBeGreaterThanOrEqual(17);
    expect(count((r) => ours(r, AUG_43, 'Sicily'))).toBeGreaterThanOrEqual(19);
    // It is stormed, not walked into, at odds that leave little to chance, and nobody is thrown back into the sea.
    expect(count((r) => { const f = landing(r, 'UK', 'Sicily') ?? landing(r, 'USA', 'Sicily'); return !!f && f.taken && f.turn === JULY_43 && f.odds >= 1.2 && f.odds <= 2.6; })).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.thrownBack === 0)).toBeGreaterThanOrEqual(17);
    expect(count((r) => [...EIGHTH, ...SEVENTH].every((nm) => r.husky[nm] === 'Sicily'))).toBeGreaterThanOrEqual(16);
  });

  it('the Germans hold the far side of the strait through August and do not try to retake the island', () => {
    expect(count((r) => (r.months[JULY_43].germans.at['Calabria'] ?? 0) >= 2 && (r.months[AUG_43].germans.at['Calabria'] ?? 0) >= 3)).toBeGreaterThanOrEqual(17);
    for (const r of runs) expect(r.fights.filter((f) => f.by === 'GER')).toHaveLength(0);
  });

  it('Mussolini falls as August opens and Italy leaves the war as September opens', () => {
    expect(count((r) => r.news.mussolini === AUG_43 && r.news.armistice === SEPT_43)).toBeGreaterThanOrEqual(17);
    expect(count((r) => !r.g.nations.ITA.alive)).toBeGreaterThanOrEqual(19);
    for (const r of runs) {
      if (!r.news.armistice) continue;
      // Its army, fleet and air force are gone, and nobody is at war with it.
      expect(r.months[DEC_43].italians).toBe(0);
      expect(r.italianFleets <= 0 || r.news.armistice > SEPT_43).toBe(true);
      expect(r.italianAir <= 0 || r.news.armistice > SEPT_43).toBe(true);
      expect(Object.values(r.g.nations).every((n) => !n.atWar.includes('ITA'))).toBe(true);
    }
  });

  it('Germany takes over Rome and the north, and the Allies get Sardinia and Corsica', () => {
    expect(count((r) => holds(r, SEPT_43, 'GER', ...NORTH))).toBeGreaterThanOrEqual(18);
    expect(count((r) => ours(r, SEPT_43, 'Sardinia', 'Corsica'))).toBeGreaterThanOrEqual(18);
  });

  it('September: the Germans leave the south, the Eighth Army crosses the strait and the heel is taken', () => {
    expect(count((r) => SOUTH.every((nm) => !r.months[SEPT_43].germans.at[nm]))).toBeGreaterThanOrEqual(17);
    expect(count((r) => ours(r, SEPT_43, 'Calabria', 'Apulia'))).toBeGreaterThanOrEqual(17);
  });

  it('Avalanche: the Fifth Army lands at Salerno in September against the German rearguard, and Naples is Allied by October', () => {
    expect(count((r) => { const f = landing(r, 'USA', 'Naples'); return !!f && f.taken && f.turn === SEPT_43; })).toBeGreaterThanOrEqual(15);
    expect(count((r) => ours(r, OCT_43, 'Naples'))).toBeGreaterThanOrEqual(18);
    for (const r of runs) for (const nm of FIFTH) expect(r.g.corps.some((c) => c.name === nm)).toBe(true);
  });

  it('the Gustav Line is dug across the mountains in October and holds, with Rome behind it, to the end of the year', () => {
    expect(count((r) => r.news.gustav === OCT_43 && r.months[OCT_43].works === 2)).toBeGreaterThanOrEqual(17);
    expect(count((r) => (r.months[OCT_43].germans.at['Abruzzo'] ?? 0) >= 6)).toBeGreaterThanOrEqual(17);
    expect(count((r) => holds(r, DEC_43, 'GER', 'Abruzzo', 'Rome', 'Tuscany'))).toBeGreaterThanOrEqual(19);
  });

  it('the year ends with two Allied armies in southern Italy and a German army in front of Rome', () => {
    expect(count((r) => r.months[DEC_43].allies.corps >= 7 && r.months[DEC_43].allies.divs >= 17)).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.months[DEC_43].germans.corps >= 5 && r.months[DEC_43].germans.divs >= 9)).toBeGreaterThanOrEqual(17);
    expect(count((r) => ours(r, DEC_43, 'Sicily', 'Calabria', 'Apulia', 'Naples', 'Malta'))).toBeGreaterThanOrEqual(17);
  });

  it('the battles cost what such battles cost: thousands, not armies', () => {
    expect(count((r) => { const l = lost(r); return l.allies >= 5000 && l.allies <= 50000; })).toBeGreaterThanOrEqual(17);
    expect(count((r) => { const l = lost(r); return l.axis >= 10000 && l.axis <= 80000; })).toBeGreaterThanOrEqual(17);
  });
});
