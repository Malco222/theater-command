// Overlord to the Ardennes, run against the campaign rules with every nation on the historical plan and nobody
// at the controls. In May 1944 the invasion armies gather in the south of England and the tactical air forces win
// the sky over Normandy. In June the British and Canadians land in Normandy and the Americans on the Cotentin, and
// two harbours are built off the open beach. In July the British attack toward Caen fails against the panzers and
// the Americans break out at the western end. In August the German armour goes west for a counter-stroke, the
// pocket behind it is closed, Paris is free and a second landing takes Marseille. September is the pursuit:
// Brussels, Antwerp, Liege, Luxembourg, Lyon. In October the airborne stroke at the Rhine bridges fails and so does
// the first attack on the West Wall. November takes Metz, Strasbourg and Belfort, and fails in the Hurtgen Forest.
// In December two panzer armies come out of the Eifel and take the Ardennes; in January they lose them again.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';
import theatres from '../../data/campaign/theatres.json';

const APR_44 = 56, MAY_44 = 57, JUNE_44 = 58, JULY_44 = 59, AUG_44 = 60, SEPT_44 = 61, OCT_44 = 62, NOV_44 = 63, DEC_44 = 64, JAN_45 = 65;
const WEST = (theatres as any).west as { regions: string[]; plans: Record<string, { entrust?: Record<string, string[]> }[]> };
/** The two English ports the invasion sails from: troops there are not yet in the campaign. */
const QUAYS = ['Hampshire', 'Cornwall'];
/** The boundary between the army groups, as the plans draw it. */
const BRITISH = WEST.plans.USA.find((p) => p.entrust)!.entrust!.UK, AMERICAN = WEST.plans.UK.find((p) => p.entrust)!.entrust!.USA;
const WALL = ['Rhineland', 'Eifel', 'Saar', 'Gelderland'];
const ALLIES = ['UK', 'USA'];

interface Fight { turn: number; place: string; by: string; pre: number; odds: number; taken: boolean; attLost: number; defLost: number; att: number; def: number }
interface Month {
  /** Who holds each place in the theatre as the month ends. */
  held: Record<string, string>;
  /** Field corps and divisions on the Continent, by nation. */
  force: Record<string, { corps: number; divs: number; men: number }>;
  /** German divisions by place. */
  at: Record<string, number>;
  /** The level of the works at each place. */
  works: Record<string, number>;
  /** Corps standing on the wrong side of the boundary between the army groups, and corps watching the Atlantic pockets. */
  astray: number; watching: number;
}

interface Run {
  g: Game;
  months: Record<number, Month>;
  fights: Fight[];
  /** The month each piece of news came, or 0. */
  news: Record<string, number>;
  /** The places over which the Allied air forces won the sky in May. */
  sky: string[];
  /** The month the harbour off Normandy was built, and whether the place counts as a port. */
  harbour: number; port: boolean;
}

function history(seed: number): Run {
  const g = createGame(seed, null);
  const ids = new Set(WEST.regions.map((nm) => g.byName[nm].id));
  const quays = new Set(QUAYS.map((nm) => g.byName[nm].id));
  const run: Run = { g, months: {}, fights: [], sky: [], news: {}, harbour: 0, port: false };
  let seen = 0, flown = 0;
  for (let t = 1; t <= JAN_45; t++) {
    runTurnAI(g);
    if (t >= APR_44) {
      for (const r of g.reports.slice(seen)) if (ids.has(r.region)) run.fights.push({ turn: t, place: r.regionName, by: r.attNat, pre: r.aPower / r.dPower, odds: r.odds, taken: r.taken, attLost: r.attLost, defLost: r.defLost, att: r.attackers.length, def: r.defenders.length });
      for (const a of g.airReports.slice(flown)) if (t === MAY_44 && a.mission === 'superiority' && ALLIES.includes(a.attNat) && a.sky) run.sky.push(a.place);
      const here = g.corps.filter((c) => !c.dead && c.kind !== 'fort' && ids.has(c.reg) && !quays.has(c.reg));
      const force: Month['force'] = {};
      for (const nat of ['UK', 'USA', 'GER']) {
        const cs = here.filter((c) => c.nat === nat);
        force[nat] = { corps: cs.length, divs: cs.reduce((s, c) => s + c.divs, 0), men: cs.reduce((s, c) => s + men(c), 0) };
      }
      const at: Record<string, number> = {};
      for (const c of here) if (c.nat === 'GER') at[g.regions[c.reg].name] = (at[g.regions[c.reg].name] ?? 0) + c.divs;
      const name = (c: { reg: number }): string => g.regions[c.reg].name;
      run.months[t] = {
        held: Object.fromEntries(WEST.regions.map((nm) => [nm, g.byName[nm].ctrl])),
        force, at,
        works: Object.fromEntries(WEST.regions.map((nm) => [nm, g.byName[nm].fort?.lv ?? 0])),
        astray: here.filter((c) => (c.nat === 'UK' && AMERICAN.includes(name(c))) || (c.nat === 'USA' && BRITISH.includes(name(c)))).length,
        watching: here.filter((c) => ALLIES.includes(c.nat) && ['Poitou', 'Gascony'].includes(name(c))).length,
      };
    }
    seen = g.reports.length; flown = g.airReports.length;
  }
  const told = (re: RegExp): number => g.headlines.find((h) => h.turn >= APR_44 && re.test(h.text))?.turn ?? 0;
  run.news = {
    fortitude: told(/phantom army/), wall: told(/mined and staked/), british: told(/British troops land at Normandy from the sea/), american: told(/American troops land at Cotentin from the sea/),
    dday: told(/D-Day, 6 June 1944/), patton: told(/Third Army becomes operational/), dragoon: told(/American troops land at Marseille from the sea/), paris: told(/Paris is free/),
    brussels: told(/Brussels is free/), student: told(/First Parachute Army/), westWall: told(/put the West Wall back in order/), ffi: told(/France has freed itself/),
    arnhem: told(/Market Garden has failed/), aachen: told(/Aachen has surrendered/), secret: told(/two panzer armies in secret/), bulge: told(/calling it the Bulge/), closed: told(/The Bulge is gone/),
  };
  run.harbour = g.flags[`harbour:${g.byName['Normandy'].id}`] ?? 0;
  run.port = g.byName['Normandy'].port;
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map(history);
const count = (ok: (r: Run) => boolean): number => runs.filter(ok).length;
const ours = (r: Run, t: number, ...places: string[]): boolean => places.every((nm) => ALLIES.includes(r.months[t].held[nm]));
const theirs = (r: Run, t: number, ...places: string[]): boolean => places.every((nm) => r.months[t].held[nm] === 'GER');
const battles = (r: Run, place: string, from: number, to = from): Fight[] => r.fights.filter((f) => f.place === place && f.turn >= from && f.turn <= to);
const allied = (r: Run, t: number): { corps: number; divs: number } => ({ corps: r.months[t].force.UK.corps + r.months[t].force.USA.corps, divs: r.months[t].force.UK.divs + r.months[t].force.USA.divs });
/** One battle at a place in a month, by the nation named, that went the way named. */
const one = (r: Run, place: string, t: number, by: string, taken: boolean): Fight | null => {
  const f = battles(r, place, t);
  return f.length === 1 && f[0].by === by && f[0].taken === taken ? f[0] : null;
};
/** Men each side lost in the battles from June to January. */
const lost = (r: Run): { allies: number; germans: number } => {
  const out = { allies: 0, germans: 0 };
  for (const f of r.fights) { const a = ALLIES.includes(f.by); out.allies += a ? f.attLost : f.defLost; out.germans += a ? f.defLost : f.attLost; }
  return out;
};

describe('exit test: the West in 1944 on the historical plan', () => {
  it('until May there is no front in the West: France is held as it has been since 1940, and nobody fights there', () => {
    for (const r of runs) expect(r.fights.filter((f) => f.turn <= MAY_44).length).toBe(0);
    expect(count((r) => theirs(r, APR_44, 'Normandy', 'Cotentin', 'Paris', 'Calais', 'Brussels', 'Marseille') && allied(r, APR_44).divs === 0)).toBe(20);
  });

  it('May: the Atlantic Wall is manned, the invasion armies gather in England, and the air forces win the sky over the beaches', () => {
    expect(count((r) => r.news.fortitude === MAY_44 && r.news.wall === MAY_44)).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.months[MAY_44].works['Calais'] === 2 && r.months[MAY_44].works['Normandy'] === 1)).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.months[MAY_44].force.GER.divs >= 46 && r.months[MAY_44].force.GER.divs <= 60 && (r.months[MAY_44].at['Calais'] ?? 0) >= 6)).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.sky.includes('Normandy') && r.sky.includes('Cotentin'))).toBeGreaterThanOrEqual(18);
    // Nobody is ashore yet.
    for (const r of runs) expect(allied(r, MAY_44).divs).toBe(0);
  });

  it('D-Day: in June the British land in Normandy and the Americans on the Cotentin, each in one battle, and both stay', () => {
    expect(count((r) => ours(r, JUNE_44, 'Normandy', 'Cotentin') && r.news.british === JUNE_44 && r.news.american === JUNE_44 && r.news.dday === JUNE_44)).toBeGreaterThanOrEqual(19);
    expect(count((r) => { const f = one(r, 'Normandy', JUNE_44, 'UK', true); return !!f && f.pre >= 1.5 && f.pre <= 2.2; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => { const f = one(r, 'Cotentin', JUNE_44, 'USA', true); return !!f && f.pre >= 2.5; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => allied(r, JUNE_44).divs >= 12 && allied(r, JUNE_44).divs <= 20)).toBeGreaterThanOrEqual(18);
    // The beachhead is never thrown back into the sea.
    for (const r of runs) for (let t = JUNE_44; t <= JAN_45; t++) expect(ours(r, t, 'Normandy')).toBe(true);
  });

  it('the Mulberry harbours: Normandy has no port, and from June the armies are fed over the open beach', () => {
    expect(count((r) => r.harbour === JUNE_44 && r.port)).toBeGreaterThanOrEqual(19);
    // The build-up goes on through a place that had no harbour in May.
    expect(count((r) => allied(r, JULY_44).divs >= 22 && allied(r, AUG_44).divs >= 38)).toBeGreaterThanOrEqual(18);
  });

  it('July: Goodwood fails in front of the panzers, and Cobra breaks out at the western end', () => {
    expect(count((r) => { const f = one(r, 'Rouen', JULY_44, 'UK', false); return !!f && f.odds < 0.7 && f.def >= 4; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => { const f = one(r, 'Rennes', JULY_44, 'USA', true); return !!f && f.pre >= 1.5; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => theirs(r, JULY_44, 'Rouen', 'Maine', 'Paris') && (r.months[JULY_44].at['Rouen'] ?? 0) >= 9)).toBeGreaterThanOrEqual(18);
  });

  it('August: the pocket is closed, Paris is free, and a second landing takes Marseille', () => {
    expect(count((r) => !!one(r, 'Rouen', AUG_44, 'UK', true) && battles(r, 'Maine', AUG_44).some((f) => f.by === 'USA' && f.taken && f.def >= 4))).toBeGreaterThanOrEqual(18);
    expect(count((r) => ours(r, AUG_44, 'Paris', 'Rouen', 'Maine', 'Orleans', 'Rennes') && r.news.paris === AUG_44 && r.news.patton === AUG_44)).toBeGreaterThanOrEqual(18);
    expect(count((r) => ours(r, AUG_44, 'Marseille') && r.news.dragoon === AUG_44 && !!one(r, 'Marseille', AUG_44, 'USA', true))).toBeGreaterThanOrEqual(18);
    // Paris does not fall before the breakout, and the Germans do not get it back.
    for (const r of runs) { expect(theirs(r, JULY_44, 'Paris')).toBe(true); for (let t = SEPT_44; t <= JAN_45; t++) expect(ours(r, t, 'Paris')).toBe(true); }
  });

  it('September: the pursuit reaches Brussels, Antwerp, Liege, Luxembourg and Lyon, and stops at the German frontier', () => {
    expect(count((r) => ours(r, SEPT_44, 'Brussels', 'Antwerp', 'Lille', 'Somme', 'Calais', 'Flanders'))).toBeGreaterThanOrEqual(18);
    expect(count((r) => ours(r, SEPT_44, 'Liege', 'Luxembourg', 'Verdun', 'Champagne', 'Lyon', 'Burgundy', 'Brittany'))).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.news.brussels === SEPT_44 && r.news.ffi === SEPT_44)).toBeGreaterThanOrEqual(18);
    expect(count((r) => theirs(r, SEPT_44, ...WALL, 'Lorraine', 'Alsace', 'Vosges', 'Belfort', 'Brabant'))).toBeGreaterThanOrEqual(19);
  });

  it('the West Wall is re-armed in September, and the armies that man it are put together from what came out of France', () => {
    expect(count((r) => r.news.westWall === SEPT_44 && r.news.student === SEPT_44 && WALL.every((nm) => r.months[SEPT_44].works[nm] === 3))).toBeGreaterThanOrEqual(19);
    expect(count((r) => (r.months[SEPT_44].at['Gelderland'] ?? 0) >= 8 && (r.months[SEPT_44].at['Rhineland'] ?? 0) >= 4 && (r.months[SEPT_44].at['Eifel'] ?? 0) >= 6)).toBeGreaterThanOrEqual(18);
    // Germany still has an army in the West when the pursuit ends.
    expect(count((r) => r.months[SEPT_44].force.GER.divs >= 40)).toBeGreaterThanOrEqual(18);
  });

  it('October: Market Garden fails at Arnhem, and so does the first attack on the West Wall at Aachen', () => {
    expect(count((r) => { const f = one(r, 'Gelderland', OCT_44, 'UK', false); return !!f && f.odds < 0.8 && f.attLost > 2 * f.defLost; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => { const f = one(r, 'Rhineland', OCT_44, 'USA', false); return !!f && f.odds < 0.8; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => ours(r, OCT_44, 'Brabant', 'Antwerp') && r.news.arnhem === OCT_44 && r.news.aachen === OCT_44)).toBeGreaterThanOrEqual(18);
  });

  it('November: Metz, Strasbourg and Belfort fall, and the Hurtgen Forest does not', () => {
    expect(count((r) => ours(r, NOV_44, 'Lorraine', 'Alsace', 'Vosges', 'Belfort'))).toBeGreaterThanOrEqual(17);
    expect(count((r) => ours(r, NOV_44, 'Lorraine') && ours(r, NOV_44, 'Alsace'))).toBeGreaterThanOrEqual(18);
    expect(count((r) => { const f = one(r, 'Eifel', NOV_44, 'USA', false); return !!f && f.odds < 0.9 && f.attLost > 2 * f.defLost; })).toBeGreaterThanOrEqual(18);
    // None of the four falls before the autumn battles.
    for (const r of runs) expect(theirs(r, OCT_44, 'Lorraine', 'Alsace', 'Vosges', 'Belfort')).toBe(true);
  });

  it('the West Wall and the line of the Dutch rivers hold to the end of the year', () => {
    for (const r of runs) for (const nm of WALL) for (const f of battles(r, nm, JUNE_44, JAN_45)) expect(f.taken).toBe(false);
    expect(count((r) => theirs(r, DEC_44, ...WALL, 'Holland', 'Rotterdam', 'Ruhr', 'Baden'))).toBe(20);
  });

  it('December: two panzer armies come out of the Eifel and take the Ardennes from a thinly held front', () => {
    expect(count((r) => r.news.secret === DEC_44 && r.news.bulge === DEC_44)).toBeGreaterThanOrEqual(18);
    expect(count((r) => { const f = one(r, 'Ardennes', DEC_44, 'GER', true); return !!f && f.pre >= 1.5 && f.def <= 2; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => theirs(r, DEC_44, 'Ardennes') && ours(r, DEC_44, 'Liege', 'Luxembourg', 'Antwerp', 'Brussels'))).toBeGreaterThanOrEqual(18);
    // It is the only battle of the month: the rest of the front is quiet.
    expect(count((r) => r.fights.filter((f) => f.turn === DEC_44).length === 1)).toBeGreaterThanOrEqual(18);
  });

  it('January: the Bulge is closed', () => {
    expect(count((r) => !!one(r, 'Ardennes', JAN_45, 'USA', true) && ours(r, JAN_45, 'Ardennes') && r.news.closed === JAN_45)).toBeGreaterThanOrEqual(17);
    expect(count((r) => theirs(r, JAN_45, ...WALL))).toBe(20);
  });

  it('the army groups keep to their own sectors, and nobody is left watching the Atlantic pockets', () => {
    expect(count((r) => [NOV_44, DEC_44, JAN_45].every((t) => r.months[t].astray === 0))).toBeGreaterThanOrEqual(18);
    expect(count((r) => [SEPT_44, OCT_44, NOV_44, DEC_44].every((t) => r.months[t].watching === 0))).toBeGreaterThanOrEqual(18);
    // The pockets are France's by the end of the year: no ground in France is still German.
    expect(count((r) => WEST.regions.every((nm) => !['FRA', 'VIC'].includes(r.g.byName[nm].owner) || r.months[DEC_44].held[nm] !== 'GER'))).toBeGreaterThanOrEqual(18);
  });

  it('the year ends with sixty Allied divisions on the German frontier and a German army still in front of them', () => {
    expect(count((r) => allied(r, DEC_44).divs >= 55 && allied(r, DEC_44).divs <= 70 && r.months[DEC_44].force.UK.divs >= 15 && r.months[DEC_44].force.USA.divs >= 38)).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.months[DEC_44].force.GER.divs >= 38 && r.months[DEC_44].force.GER.divs <= 62)).toBeGreaterThanOrEqual(18);
    for (const r of runs) expect(r.g.nations.GER.alive).toBe(true);
  });

  it('the campaign costs what it cost: Germany loses about twice what the Allies do', () => {
    expect(count((r) => { const l = lost(r); return l.allies >= 190000 && l.allies <= 360000; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => { const l = lost(r); return l.germans >= 400000 && l.germans <= 720000; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => lost(r).germans > 1.5 * lost(r).allies)).toBeGreaterThanOrEqual(18);
  });
});
