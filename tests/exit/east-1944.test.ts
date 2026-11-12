// The eastern front of 1944, run against the campaign rules with every nation on the historical plan and nobody
// at the controls. In January the siege of Leningrad is lifted and Kirovograd falls. February takes Rovno and
// Lutsk, and Army Group North stands on the Panther line. March is the mud offensive across the western Ukraine,
// April Odessa, May the Crimea. In June the Red Army strikes Finland at Vyborg and Army Group Centre at Vitebsk
// and Minsk; in July the army group is destroyed at Mogilev and the front goes to the Bug and the San. August
// reaches the Vistula, Warsaw rises, and the blow at Jassy brings Romania over. In September Bulgaria follows,
// Finland signs an armistice, and the German Sixth Army is lost for the second time. October takes Riga,
// Debrecen and Belgrade and shuts Army Group North into Courland. In November the first assault on Budapest
// fails, and the year ends with the city ringed and the front standing on the Vistula.
//
// The plan only sets intentions. Every region still has to be won under the rules the player
// uses, so the bands below are loose on purpose and are checked across twenty sets of dice.
import { describe, expect, it } from 'vitest';
import { atWar, createGame, men } from '../../src/campaign/game';
import { runTurnAI } from '../../src/campaign/turn';
import type { Game } from '../../src/campaign/types';

const DEC_43 = 52, JAN_44 = 53, FEB_44 = 54, MAR_44 = 55, APR_44 = 56, MAY_44 = 57, JUNE_44 = 58, JULY_44 = 59, AUG_44 = 60, SEPT_44 = 61, OCT_44 = 62, NOV_44 = 63, DEC_44 = 64, JAN_45 = 65;
/** The eastern front: the Soviet Union, the countries between it and Germany, and Germany's own eastern provinces. */
const OWNERS = ['SOV', 'POL', 'LIT', 'LAT', 'EST', 'ROM', 'HUN', 'FIN', 'BUL', 'SVK', 'YUG'];
const REICH = ['Konigsberg', 'Elbing', 'Masuria', 'Memel'];
const AXIS = ['GER', 'ROM', 'HUN', 'FIN', 'BUL', 'SVK'];
/** Where the German army stands when the year is over. */
const VISTULA = ['Warsaw', 'Kielce', 'Krakow', 'Masuria', 'Konigsberg'];

interface Fight { turn: number; place: string; by: string; pre: number; odds: number; taken: boolean; attLost: number; defLost: number; prisoners: number; att: number; def: number }
interface Month {
  /** Who holds each place on the front once the month is over and the next has opened. */
  held: Record<string, string>;
  /** Field corps, divisions and men on the front, by nation. */
  force: Record<string, { corps: number; divs: number; men: number }>;
  /** German divisions by place. */
  at: Record<string, number>;
  /** The level of the works at each place. */
  works: Record<string, number>;
  /** Which of Germany's partners are still in the war, and whether Finland is still at war with the Soviet Union. */
  alive: Record<string, boolean>; finnishWar: boolean;
  /** German corps on the front that are out of supply. */
  cut: string[];
}

interface Run {
  g: Game;
  months: Record<number, Month>;
  fights: Fight[];
  /** The month each piece of news is dated, or 0. */
  news: Record<string, number>;
  /** The month the harbour at Courland was built, and whether the place counts as a port. */
  harbour: number; port: boolean;
}

function history(seed: number): Run {
  const g = createGame(seed, null);
  const front = g.regions.filter((r) => OWNERS.includes(r.owner) || REICH.includes(r.name));
  const ids = new Set(front.map((r) => r.id));
  const run: Run = { g, months: {}, fights: [], news: {}, harbour: 0, port: false };
  let seen = 0;
  for (let t = 1; t <= JAN_45; t++) {
    runTurnAI(g);
    if (t >= DEC_43) {
      for (const r of g.reports.slice(seen)) if (ids.has(r.region)) run.fights.push({ turn: t, place: r.regionName, by: r.attNat, pre: r.aPower / r.dPower, odds: r.odds, taken: r.taken, attLost: r.attLost, defLost: r.defLost, prisoners: r.prisoners, att: r.attackers.length, def: r.defenders.length });
      const here = g.corps.filter((c) => !c.dead && c.kind !== 'fort' && ids.has(c.reg));
      const force: Month['force'] = {};
      for (const nat of ['SOV', ...AXIS]) {
        const cs = here.filter((c) => c.nat === nat);
        force[nat] = { corps: cs.length, divs: cs.reduce((s, c) => s + c.divs, 0), men: cs.reduce((s, c) => s + men(c), 0) };
      }
      const at: Record<string, number> = {};
      for (const c of here) if (c.nat === 'GER') at[g.regions[c.reg].name] = (at[g.regions[c.reg].name] ?? 0) + c.divs;
      run.months[t] = {
        held: Object.fromEntries(front.map((r) => [r.name, r.ctrl])),
        force, at,
        works: Object.fromEntries(['Pskov', 'Estonia', 'Courland', 'Budapest'].map((nm) => [nm, g.byName[nm].fort?.lv ?? 0])),
        alive: Object.fromEntries(AXIS.map((n) => [n, g.nations[n].alive])),
        finnishWar: atWar(g, 'FIN', 'SOV'),
        cut: here.filter((c) => c.nat === 'GER' && !c.supplied).map((c) => g.regions[c.reg].name),
      };
    }
    seen = g.reports.length;
  }
  const told = (re: RegExp): number => g.headlines.find((h) => h.turn >= DEC_43 && re.test(h.text))?.turn ?? 0;
  run.news = {
    leningrad: told(/siege of Leningrad is over/), panther: told(/fallen back from Leningrad to the Panther line/), narva: told(/northern anchor of the Panther line/), crimea: told(/Sevastopol has fallen/),
    minsk: told(/Minsk is free/), lublin: told(/Polish committee/), rising: told(/Warsaw has risen/), romania: told(/King Michael/), bulgaria: told(/declared war on Bulgaria/), finland: told(/Finland is out of the war/),
    slovak: told(/Slovakia has risen/), crushed: told(/Warsaw rising is over/), courland: told(/cut off in Courland/), belgrade: told(/Belgrade is free/), horthy: told(/Admiral Horthy/), attila: told(/Attila line/), ring: told(/ring has closed round Budapest/),
  };
  run.harbour = g.flags[`harbour:${g.byName['Courland'].id}`] ?? 0;
  run.port = g.byName['Courland'].port;
  return run;
}

const SEEDS = Array.from({ length: 20 }, (_, i) => i + 1);
const runs = SEEDS.map(history);
const count = (ok: (r: Run) => boolean): number => runs.filter(ok).length;
const ours = (r: Run, t: number, ...places: string[]): boolean => places.every((nm) => r.months[t].held[nm] === 'SOV');
const theirs = (r: Run, t: number, ...places: string[]): boolean => places.every((nm) => AXIS.includes(r.months[t].held[nm]));
const battles = (r: Run, place: string, from: number, to = from): Fight[] => r.fights.filter((f) => f.place === place && f.turn >= from && f.turn <= to);
/** A place the Red Army won in battle that month. */
const stormed = (r: Run, place: string, t: number): Fight | undefined => battles(r, place, t).find((f) => f.by === 'SOV' && f.taken);
const named = (r: Run, re: RegExp): number => r.g.corps.filter((c) => !c.dead && c.nat === 'SOV' && re.test(c.name)).length;
/** Men each side lost in the year's battles, and the prisoners the Red Army took. */
const lost = (r: Run, from = JAN_44, to = JAN_45): { soviet: number; axis: number; prisoners: number } => {
  const out = { soviet: 0, axis: 0, prisoners: 0 };
  for (const f of r.fights) {
    if (f.turn < from || f.turn > to) continue;
    const s = f.by === 'SOV';
    out.soviet += s ? f.attLost : f.defLost; out.axis += s ? f.defLost : f.attLost;
    if (s) out.prisoners += f.prisoners;
  }
  return out;
};

describe('exit test: the eastern front in 1944 on the historical plan', () => {
  it('the year opens on the line 1943 left: Leningrad besieged, the Dnieper crossed at Kiev, the Crimea cut off', () => {
    expect(count((r) => ours(r, DEC_43, 'Leningrad', 'Kiev', 'Smolensk', 'Gomel', 'Zaporozhye', 'Dnepropetrovsk'))).toBe(20);
    expect(count((r) => theirs(r, DEC_43, 'Novgorod', 'Vitebsk', 'Minsk', 'Kirovograd', 'Odessa', 'Lwow') && r.months[DEC_43].held['Karelia'] === 'FIN')).toBe(20);
    expect(count((r) => theirs(r, DEC_43, 'Crimea'))).toBeGreaterThanOrEqual(18);
  });

  it('January: the siege of Leningrad is lifted at Novgorod, and Kirovograd falls', () => {
    expect(count((r) => !!stormed(r, 'Novgorod', JAN_44) && !!stormed(r, 'Kirovograd', JAN_44) && ours(r, JAN_44, 'Novgorod', 'Kirovograd'))).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.news.leningrad === JAN_44)).toBeGreaterThanOrEqual(19);
    // Nothing else is asked of the front in midwinter.
    expect(count((r) => theirs(r, JAN_44, 'Volhynia', 'Uman', 'Vinnitsa', 'Odessa', 'Vitebsk', 'Pskov'))).toBe(20);
  });

  it('February: Rovno and Lutsk, and Army Group North falls back to the Panther line and digs in', () => {
    expect(count((r) => ours(r, FEB_44, 'Volhynia') && battles(r, 'Volhynia', FEB_44).length === 1)).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.news.panther === FEB_44 && r.news.narva === FEB_44 && r.months[FEB_44].works['Pskov'] === 2 && r.months[FEB_44].works['Estonia'] === 2)).toBe(20);
    // The line holds until the summer: Pskov to July, Estonia to September.
    expect(count((r) => theirs(r, JUNE_44, 'Pskov', 'Estonia') && theirs(r, AUG_44, 'Estonia'))).toBe(20);
  });

  it('March: the mud offensive takes Uman, Vinnitsa and Nikolaev and reaches the Carpathians', () => {
    expect(count((r) => ours(r, MAR_44, 'Uman', 'Vinnitsa', 'Nikolaev', 'Bukovina'))).toBeGreaterThanOrEqual(19);
    expect(count((r) => ['Uman', 'Vinnitsa', 'Nikolaev', 'Bukovina'].every((nm) => !!stormed(r, nm, MAR_44)))).toBeGreaterThanOrEqual(18);
    for (const r of runs) expect(theirs(r, FEB_44, 'Uman', 'Vinnitsa', 'Nikolaev')).toBe(true);
  });

  it('April and May: Odessa, then the Crimea, where the Seventeenth Army is lost', () => {
    expect(count((r) => ours(r, APR_44, 'Odessa'))).toBeGreaterThanOrEqual(17);
    expect(count((r) => ours(r, MAY_44, 'Crimea'))).toBe(20);
    expect(count((r) => { const f = stormed(r, 'Crimea', MAY_44); return !!f && f.prisoners >= 35000 && r.news.crimea === MAY_44; })).toBeGreaterThanOrEqual(18);
    // Romania and the centre are not touched in the spring.
    expect(count((r) => theirs(r, MAY_44, 'Bessarabia', 'Moldavia', 'Vitebsk', 'Minsk', 'Mogilev', 'Lwow'))).toBe(20);
  });

  it('June: the Red Army strikes Finland on the Karelian isthmus, and Army Group Centre at Vitebsk and Minsk', () => {
    expect(count((r) => !!stormed(r, 'Vitebsk', JUNE_44) && !!stormed(r, 'Minsk', JUNE_44) && !!stormed(r, 'Karelia', JUNE_44))).toBeGreaterThanOrEqual(19);
    expect(count((r) => ours(r, JUNE_44, 'Vitebsk', 'Minsk', 'Karelia') && r.news.minsk === JUNE_44)).toBeGreaterThanOrEqual(19);
    // The fortified places are manned for it, and Mogilev is still held behind the breakthrough.
    expect(count((r) => (r.months[MAY_44].at['Vitebsk'] ?? 0) >= 10 && theirs(r, JUNE_44, 'Mogilev') && (r.months[JUNE_44].at['Mogilev'] ?? 0) >= 10)).toBeGreaterThanOrEqual(18);
  });

  it('July: Army Group Centre is destroyed at Mogilev, and the front goes to the Bug and the San', () => {
    expect(count((r) => { const f = stormed(r, 'Mogilev', JULY_44); return !!f && f.prisoners >= 60000; })).toBeGreaterThanOrEqual(19);
    expect(count((r) => ours(r, JULY_44, 'Mogilev', 'Polotsk', 'Wilno', 'Polesie', 'Pskov'))).toBeGreaterThanOrEqual(19);
    expect(count((r) => ours(r, JULY_44, 'Lwow', 'Lublin'))).toBeGreaterThanOrEqual(16);
    expect(count((r) => ['Lwow', 'Lublin', 'Brest', 'Bialystok', 'Przemysl'].filter((nm) => ours(r, JULY_44, nm)).length >= 3)).toBeGreaterThanOrEqual(18);
    // The German army in the east is a quarter of a million men smaller than it was in May.
    expect(count((r) => r.months[JULY_44].force.GER.men <= r.months[MAY_44].force.GER.men - 150000)).toBeGreaterThanOrEqual(18);
  });

  it('August: the Vistula is reached, Lublin is free, and Warsaw rises', () => {
    expect(count((r) => ours(r, AUG_44, 'Lwow', 'Lublin', 'Brest', 'Bialystok', 'Przemysl', 'Lithuania'))).toBe(20);
    expect(count((r) => r.news.lublin === JULY_44 || r.news.lublin === AUG_44)).toBe(20);
    // The Polish 1st Army forms on Polish soil, in Lublin or beside it.
    expect(count((r) => named(r, /^Polish 1st Army/) === 1)).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.news.rising === AUG_44 && r.news.crushed === OCT_44)).toBeGreaterThanOrEqual(16);
    // The Red Army stops on the river: Warsaw is not attacked, in August or after.
    for (const r of runs) expect(battles(r, 'Warsaw', JAN_44, JAN_45).length).toBe(0);
    expect(count((r) => (r.months[AUG_44].at['Warsaw'] ?? 0) >= 6)).toBeGreaterThanOrEqual(18);
  });

  it('the blow at Jassy brings Romania over in August, and Bulgaria follows without a shot', () => {
    expect(count((r) => !!stormed(r, 'Moldavia', AUG_44) && r.news.romania === AUG_44 && !r.months[AUG_44].alive.ROM)).toBeGreaterThanOrEqual(19);
    expect(count((r) => ours(r, AUG_44, 'Bucharest', 'Ploiesti', 'Dobruja', 'Oltenia', 'Banat'))).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.news.bulgaria === SEPT_44 && !r.months[AUG_44].alive.BUL && ours(r, AUG_44, 'Sofia', 'Varna', 'Plovdiv'))).toBeGreaterThanOrEqual(19);
    // Both countries are in the war until the summer, and both armies march with the Red Army after it.
    for (const r of runs) expect(r.months[JULY_44].alive.ROM && r.months[JULY_44].alive.BUL).toBe(true);
    expect(count((r) => named(r, /^Romanian (1st|4th) Army/) === 2 && named(r, /^Bulgarian 1st Army/) === 1)).toBeGreaterThanOrEqual(19);
  });

  it('September: the German Sixth Army is lost a second time in Bessarabia, and Estonia is given up', () => {
    expect(count((r) => { const f = stormed(r, 'Bessarabia', SEPT_44); return !!f && f.prisoners >= 55000; })).toBeGreaterThanOrEqual(19);
    expect(count((r) => ours(r, SEPT_44, 'Bessarabia', 'Estonia', 'Odessa'))).toBeGreaterThanOrEqual(19);
    expect(count((r) => battles(r, 'Estonia', JAN_44, JAN_45).length === 0)).toBeGreaterThanOrEqual(18);
  });

  it('Finland signs an armistice in September: the frontier of 1940, Petsamo given up, and the country left standing', () => {
    expect(count((r) => r.news.finland === SEPT_44 && r.months[JULY_44].finnishWar && !r.months[AUG_44].finnishWar)).toBeGreaterThanOrEqual(19);
    expect(count((r) => r.g.byName['Karelia'].owner === 'SOV' && r.g.byName['Petsamo'].owner === 'SOV' && r.g.byName['Helsinki'].ctrl === 'FIN' && r.g.nations.FIN.alive)).toBeGreaterThanOrEqual(19);
    // Karelia was the only battle of that war all year.
    for (const r of runs) expect(r.fights.filter((f) => f.turn >= JAN_44 && f.by === 'SOV' && r.g.byName[f.place].owner === 'FIN' && f.place !== 'Karelia').length).toBe(0);
  });

  it('October: Riga, Debrecen and Belgrade, and Army Group North is shut into Courland and fed by sea', () => {
    expect(count((r) => ours(r, OCT_44, 'Latvia', 'Transylvania', 'Debrecen', 'Belgrade') && r.news.belgrade === OCT_44)).toBeGreaterThanOrEqual(19);
    expect(count((r) => ours(r, NOV_44, 'Memel'))).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.news.courland === OCT_44 && r.harbour === OCT_44 && r.port && r.months[OCT_44].works['Courland'] === 1)).toBeGreaterThanOrEqual(19);
    // The pocket holds to the end of the story, and nobody in it goes hungry.
    expect(count((r) => [OCT_44, NOV_44, DEC_44, JAN_45].every((t) => theirs(r, t, 'Courland') && !r.months[t].cut.includes('Courland')))).toBeGreaterThanOrEqual(19);
    expect(count((r) => (r.months[DEC_44].at['Courland'] ?? 0) >= 6)).toBeGreaterThanOrEqual(17);
    for (const r of runs) expect(battles(r, 'Courland', JAN_44, JAN_45).length).toBe(0);
  });

  it('Hungary is kept in the war: Horthy is overthrown in October and the first assault on Budapest fails in November', () => {
    expect(count((r) => r.news.horthy === OCT_44 && r.news.attila === NOV_44 && r.months[NOV_44].works['Budapest'] === 2)).toBeGreaterThanOrEqual(19);
    expect(count((r) => { const f = battles(r, 'Budapest', NOV_44); return f.length === 1 && f[0].by === 'SOV' && !f[0].taken && f[0].pre < 1.3; })).toBeGreaterThanOrEqual(18);
    // Nobody tries for the city before the autumn.
    for (const r of runs) expect(battles(r, 'Budapest', JAN_44, OCT_44).length).toBe(0);
  });

  it('the year ends with Budapest ringed and still holding, and the front on the Vistula', () => {
    expect(count((r) => theirs(r, JAN_45, 'Budapest') && r.months[JAN_45].alive.HUN)).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.news.ring === DEC_44)).toBeGreaterThanOrEqual(17);
    expect(count((r) => battles(r, 'Budapest', DEC_44).length <= 1)).toBe(20);
    expect(count((r) => theirs(r, JAN_45, ...VISTULA, 'Slovakia'))).toBe(20);
    // January 1945 is the lull before the last offensive: nobody attacks anybody.
    expect(count((r) => r.fights.filter((f) => f.turn === JAN_45).length === 0)).toBeGreaterThanOrEqual(19);
    for (const r of runs) expect(r.g.nations.GER.alive).toBe(true);
  });

  it('the German army never attacks all year, and the Red Army attacks only where its orders point', () => {
    for (const r of runs) expect(r.fights.filter((f) => f.turn >= JAN_44 && f.by !== 'SOV').length).toBe(0);
    // No more than a handful of battles a month: this is a calendar of set-piece offensives, not a brawl along the line.
    expect(count((r) => [JAN_44, FEB_44, MAR_44, APR_44, MAY_44, JUNE_44, JULY_44, AUG_44, SEPT_44, OCT_44, NOV_44, DEC_44].every((t) => r.fights.filter((f) => f.turn === t).length <= 8))).toBe(20);
    // Ground the plan never names is still German when the story ends.
    expect(count((r) => theirs(r, JAN_45, 'Vojvodina', 'Transdanubia', 'Slovakia', 'Krakow'))).toBeGreaterThanOrEqual(18);
  });

  it('the armies: Germany starts the year with over a million men in the east and ends it with fewer, against a Red Army that only grows', () => {
    expect(count((r) => r.months[JAN_44].force.GER.men >= 950000 && r.months[JAN_44].force.GER.men <= 1650000)).toBeGreaterThanOrEqual(18);
    expect(count((r) => r.months[SEPT_44].force.GER.men < r.months[JAN_44].force.GER.men)).toBeGreaterThanOrEqual(17);
    expect(count((r) => r.months[JAN_45].force.SOV.men > r.months[JAN_44].force.SOV.men && r.months[JAN_44].force.SOV.men >= 5 * r.months[JAN_44].force.GER.men)).toBeGreaterThanOrEqual(19);
    // Romania's army is gone from the Axis line of battle after August; Hungary's and Slovakia's are still in it.
    expect(count((r) => r.months[JULY_44].force.ROM.corps >= 8 && r.months[SEPT_44].force.ROM.corps === 0 && r.months[DEC_44].force.SVK.corps === 2)).toBeGreaterThanOrEqual(19);
  });

  it('the year costs Germany and its partners twice what it costs the Red Army, most of it in the summer', () => {
    expect(count((r) => { const l = lost(r); return l.axis >= 750000 && l.axis <= 1250000; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => { const l = lost(r); return l.soviet >= 380000 && l.soviet <= 680000; })).toBeGreaterThanOrEqual(18);
    expect(count((r) => lost(r).prisoners >= 220000)).toBeGreaterThanOrEqual(18);
    expect(count((r) => lost(r, JUNE_44, SEPT_44).axis >= 340000)).toBeGreaterThanOrEqual(18);
  });

  it('the other fronts play out as they did: Rome and Paris are free, and the Bulge is closed in January', () => {
    expect(count((r) => ['UK', 'USA'].includes(r.g.byName['Rome'].ctrl) && ['UK', 'USA'].includes(r.g.byName['Paris'].ctrl) && ['UK', 'USA'].includes(r.g.byName['Antwerp'].ctrl))).toBe(20);
    expect(count((r) => ['UK', 'USA'].includes(r.g.byName['Ardennes'].ctrl))).toBeGreaterThanOrEqual(17);
  });
});
