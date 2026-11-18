// Weather (ruling 127, the sky's words ruling 143; the grand campaign, behind a switch). Each month each weather zone is
// under a clear sky, cloud or a closed sky, dealt from the seed by the zone's odds for the month. Cloud: fewer bombs
// hit, fewer bombers are found and brought down, air support is worth half. A closed sky: nothing flies in the zone
// for either side. A mission needs an open sky at its base and its target. History's skies at stops 1 and 2.
import { afterEach, describe, expect, it } from 'vitest';
import { airFleetsOf, assessAir, fightersOver, forecastAir, whyNotMission } from '../src/campaign/air';
import { groupRow, planRaid } from '../src/campaign/airwar';
import { assess, createGame, setWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { monthCard, markFor } from '../src/campaign/monthcard';
import { airOver } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import {
  WEATHER, forecastLine, grounded, groundedReport, oddsOf, setWeather, skyAt, skyOf, skyView, weatherMonth, zoneName, zoneOfRegion, zoneOfSea,
} from '../src/campaign/weather';
import type { Game } from '../src/campaign/types';

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
const at = (g: Game, name: string): number => g.regions.find((r) => r.name === name)!.id;
const sea = (g: Game, name: string): number => g.seas.find((z) => z.name === name)!.id;
const zone = (key: string): number => WEATHER.zones.findIndex((z) => z.key === key);

// A zone's sky set for certain in every month, for a test, and put back after.
const kept: [number, [number, number, number][]][] = [];
function force(key: string, sky: 'clear' | 'cloud' | 'closed'): void {
  const i = zone(key);
  kept.push([i, WEATHER.zones[i].odds]);
  const row: [number, number, number] = sky === 'clear' ? [10, 0, 0] : sky === 'cloud' ? [0, 10, 0] : [0, 0, 10];
  WEATHER.zones[i].odds = Array.from({ length: 12 }, () => [...row] as [number, number, number]);
}
afterEach(() => { while (kept.length) { const [i, o] = kept.pop()!; WEATHER.zones[i].odds = o; } });
/** A fresh campaign with weather on and the zones' skies forced (forced before the first look, which deals the month). */
function sky(skies: Record<string, 'clear' | 'cloud' | 'closed'>, human: string | null = 'UK', seed = 3): Game {
  for (const [k, s] of Object.entries(skies)) force(k, s);
  const g = free(seed, human);
  setWeather(g, true);
  return g;
}

describe('the switch', () => {
  it('is off in the data, and a campaign begun with it off has no weather', () => {
    expect(WEATHER.on).toBe(false);
    force('nwe', 'closed');
    const g = free(3, 'UK');
    expect(g.flags.weather).toBeUndefined();
    expect(skyAt(g, at(g, 'Kent'))).toBe('clear');
    expect(skyView(g, 'UK')).toBeNull();
    const f = airFleetsOf(g, 'UK').find((x) => x.bmr > 0)!;
    setWar(g, 'UK', 'GER');
    expect(whyNotMission(g, f, 'bomb', at(g, 'Ruhr')) ?? '').not.toMatch(/sky/);
    expect(monthCard(g, markFor(g), 'UK').sky).toBeUndefined();
  });

  it('a historical scenario never has it', () => {
    const g = createGame(3, 'GER');
    setWeather(g, true);
    expect(skyAt(g, at(g, 'Kent'))).toBe('clear');
    expect(skyView(g, 'GER')).toBeNull();
  });

  it('saves with the campaign', () => {
    const g = sky({ nwe: 'closed' });
    const h = loadGame(saveGame(g))!;
    expect(h.flags.weather).toBe(1);
    expect(skyAt(h, at(h, 'Kent'))).toBe('closed');
  });
});

describe('the zones', () => {
  it('every province on the map and every sea of Europe and the Americas has a zone, a theatre wide', () => {
    const g = free(3);
    expect(g.regions.filter((r) => zoneOfRegion(g, r.id) < 0)).toEqual([]);
    expect(g.seas.filter((z) => zoneOfSea(g, z.id) < 0).map((z) => z.name).sort()).toEqual(['Gulf of Aden', 'Mozambique Channel']);
    for (let i = 0; i < WEATHER.zones.length; i++) expect(g.regions.filter((r) => zoneOfRegion(g, r.id) === i).length + g.seas.filter((z) => zoneOfSea(g, z.id) === i).length).toBeGreaterThanOrEqual(2);
  });

  it('puts the places where the war was fought under the right sky', () => {
    const g = free(3);
    const key = (n: string): string => WEATHER.zones[zoneOfRegion(g, at(g, n))].key;
    expect(key('Ardennes')).toBe('nwe');
    expect(key('Kent')).toBe('nwe');
    expect(key('Ruhr')).toBe('central');
    expect(key('Polish Silesia')).toBe('central');
    expect(key('Moscow')).toBe('nrus');
    expect(key('Stalingrad')).toBe('srus');
    expect(key('Malta')).toBe('italy');
    expect(key('Tobruk')).toBe('nafrica');
    expect(key('Palestine')).toBe('mideast');
    expect(key('Albania')).toBe('balkans');
    expect(WEATHER.zones[zoneOfSea(g, sea(g, 'North Sea'))].key).toBe('nwe');
    expect(WEATHER.zones[zoneOfSea(g, sea(g, 'Barents Sea'))].key).toBe('arctic');
  });

  it('names each zone plainly in 22 letters or fewer, with a plate, snow months and twelve months of odds in tenths', () => {
    for (const z of WEATHER.zones) {
      expect(z.name.length).toBeLessThanOrEqual(22);
      expect(z.name).not.toMatch(/\d/);
      expect(z.plate).toHaveLength(2);
      expect(z.snow.every((m) => m >= 1 && m <= 12)).toBe(true);
      expect(z.odds).toHaveLength(12);
      for (const o of z.odds) { expect(o.reduce((a, b) => a + b, 0)).toBe(10); expect(o.every((x) => Number.isInteger(x) && x >= 0)).toBe(true); }
    }
    expect(zoneName(zone('nwe'))).toBe('North-west Europe');
    expect(zoneName(zone('arctic'))).toBe('the Arctic');
  });
});

describe('the sky dealt', () => {
  it('is the same however often it is asked, and never touches the game\'s own dice', () => {
    const g = free(5);
    setWeather(g, true);
    const state = JSON.stringify(g.rng);
    const a = WEATHER.zones.map((_, i) => skyOf(g, i, 30));
    const b = WEATHER.zones.map((_, i) => skyOf(g, i, 30));
    expect(a).toEqual(b);
    expect(JSON.stringify(g.rng)).toBe(state);
  });

  it('comes up as often as the odds say', () => {
    // North-west Europe in December: 2 clear, 5 cloud, 3 closed in ten.
    const n = 600, tally = { clear: 0, cloud: 0, closed: 0 };
    const g = free(1);
    setWeather(g, true);
    for (let s = 0; s < n; s++) { g.seed = 1000 + s; tally[skyOf(g, zone('nwe'), 4)]++; }
    expect(oddsOf(zone('nwe'), 4)).toEqual([2, 5, 3]);
    expect(tally.closed / n).toBeGreaterThan(0.24);
    expect(tally.closed / n).toBeLessThan(0.36);
    expect(tally.clear / n).toBeGreaterThan(0.14);
    expect(tally.clear / n).toBeLessThan(0.26);
  });

  it('keeps history\'s skies at stops 1 and 2, December 1944 over the Ardennes first', () => {
    expect(WEATHER.book.length).toBeLessThan(10);
    expect(WEATHER.book[0]).toMatchObject({ turn: 64, zone: 'nwe', sky: 'closed' });
    const g = free(1);
    setWeather(g, true);
    setStop(g, 1, true);
    for (let s = 0; s < 20; s++) { g.seed = 50 + s; expect(skyOf(g, zone('nwe'), 64)).toBe('closed'); }
    expect(forecastLine(g, zone('nwe'), 64)).toBe('December: a closed sky, as history had it.');
    // At stop 3 the month is dealt like any other.
    setStop(g, 3, true);
    const dealt = new Set<string>();
    for (let s = 0; s < 40; s++) { g.seed = 50 + s; dealt.add(skyOf(g, zone('nwe'), 64)); }
    expect(dealt.size).toBeGreaterThan(1);
  });
});

describe('a closed sky', () => {
  it('grounds every group based under it, and nothing flies over it', () => {
    const g = sky({ nwe: 'closed', central: 'clear' });
    setWar(g, 'UK', 'GER');
    const raf = airFleetsOf(g, 'UK').find((x) => x.bmr > 0 && x.ship === undefined)!;
    expect(whyNotMission(g, raf, 'bomb', at(g, 'Ruhr'))).toBe('Grounded this month: the sky is closed over North-west Europe.');
    const lw = airFleetsOf(g, 'GER').find((x) => x.bmr > 0)!;
    expect(whyNotMission(g, lw, 'bomb', at(g, 'Midlands'))).toBe('Closed sky this month: nothing flies over North-west Europe.');
    expect(assessAir(g, [lw], 'bomb', at(g, 'Midlands')).why).toMatch(/^Closed sky this month/);
    // The group's card says why, and next month's chances.
    expect(groupRow(g, raf, 'UK').sky).toEqual(['Grounded this month: the sky is closed over North-west Europe.', 'Next month: closed, for certain.']);
    // The raid card cannot open, and says why.
    const p = planRaid(g, 'bomb', at(g, 'Midlands'), undefined, 'GER');
    expect(p.why).toBe('Closed sky this month: nothing flies over North-west Europe.');
    expect(p.sky).toBe('Closed sky over Midlands: nothing flies there this month.');
  });

  it('keeps the fighters based under it on the ground, and no fighters bar a beach under it', () => {
    const g = sky({ nwe: 'closed', central: 'clear' });
    setWar(g, 'UK', 'GER');
    expect(airFleetsOf(g, 'UK').filter((f) => f.ship === undefined).every((f) => grounded(g, f))).toBe(true);
    expect(airFleetsOf(g, 'GER').some((f) => grounded(g, f))).toBe(false);
    expect(fightersOver(g, at(g, 'Kent'), 'GER')).toBe(0);
    const open = sky({ nwe: 'clear' });
    setWar(open, 'UK', 'GER');
    expect(fightersOver(open, at(open, 'Kent'), 'GER')).toBeGreaterThan(0);
  });

  it('takes the air out of a land battle for both sides, and says so on the attack card', () => {
    const run = (s: 'clear' | 'cloud' | 'closed' | null) => {
      const g = s ? sky({ central: s }, 'GER') : free(3, 'GER');
      setWar(g, 'GER', 'POL');
      const att = g.corps.filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort').slice(0, 3);
      return assess(g, att, g.regions[at(g, 'Polish Silesia')]);
    };
    const off = run(null), clear = run('clear'), cloud = run('cloud'), closed = run('closed');
    expect(clear.A).toBe(off.A);
    expect(clear.D).toBe(off.D);
    expect(clear.lines.find((l) => l.label === 'The sky')).toBeUndefined();
    // Germany has the stronger air force: it lends the attack less under cloud, and nothing under a closed sky.
    expect(off.A).toBeGreaterThan(closed.A);
    expect(cloud.A).toBeCloseTo((off.A + closed.A) / 2, 6);
    expect(closed.lines.find((l) => l.label === 'The sky')?.value).toBe('The sky is closed over Polish Silesia this month: no aircraft will fly for either side.');
    expect(closed.lines.find((l) => l.label === 'Air')).toBeUndefined();
    expect(cloud.lines.find((l) => l.label === 'The sky')?.value).toBe('Cloud over Polish Silesia this month: air support is worth half for both sides.');
    expect(cloud.lines.find((l) => l.label === 'Air')).toBeDefined();
  });

  it('leaves a sea without air cover, and halves it under cloud', () => {
    const z = (g: Game) => sea(g, 'North Sea');
    const run = (s: 'clear' | 'cloud' | 'closed') => { const g = sky({ nwe: s }); setWar(g, 'UK', 'GER'); return airOver(g, z(g), 'UK', true); };
    const clear = run('clear');
    expect(clear).toBeGreaterThan(0);
    expect(run('cloud')).toBeCloseTo(clear / 2, 9);
    expect(run('closed')).toBe(0);
  });
});

describe('cloud', () => {
  it('lets everything fly, with fewer bombs on target, fewer fighters finding the bombers, and fewer brought down by the guns', () => {
    const run = (s: 'clear' | 'cloud') => {
      const g = sky({ nwe: 'clear', central: s });
      setWar(g, 'UK', 'GER');
      const raf = airFleetsOf(g, 'UK').filter((x) => x.bmr > 0 && x.ship === undefined);
      const a = assessAir(g, raf, 'bomb', at(g, 'Ruhr'));
      return { a, fc: forecastAir(g, a, 'bomb', at(g, 'Ruhr')), g };
    };
    const clear = run('clear'), cloud = run('cloud');
    expect(cloud.a.why).toBeNull();
    expect(clear.a.aim).toBe(1);
    expect(cloud.a.aim).toBe(WEATHER.cloud.bombsDay);
    expect(cloud.a.D).toBeCloseTo(clear.a.D * WEATHER.cloud.met, 6);
    expect(cloud.a.gunLoss).toBeLessThan(clear.a.gunLoss);
    expect(cloud.fc.cut).toBeLessThan(clear.fc.cut);
    // The raid card says why its numbers are lower.
    expect(planRaid(cloud.g, 'bomb', at(cloud.g, 'Ruhr'), undefined, 'UK').sky).toBe('Cloud over Ruhr: fewer bombs hit, and fewer bombers are found.');
    expect(planRaid(clear.g, 'bomb', at(clear.g, 'Ruhr'), undefined, 'UK').sky).toBe('Clear sky over Ruhr.');
  });

  it('by night costs the bombs less than by day', () => {
    expect(WEATHER.cloud.bombsNight).toBeGreaterThan(WEATHER.cloud.bombsDay);
    expect(WEATHER.cloud.gunsNight).toBeLessThan(WEATHER.cloud.gunsDay);
  });
});

describe('the forecast and THE SKY', () => {
  it('gives next month with the true odds, naming the closed chance', () => {
    const g = free(3);
    setWeather(g, true);
    // November in North-west Europe: [2, 5, 3]; December in Italy: [3, 5, 2]; July in North Africa: [9, 1, 0].
    expect(forecastLine(g, zone('nwe'), 3)).toBe('November: likely cloud, 1 chance in 2; closed, 3 in 10.');
    expect(forecastLine(g, zone('italy'), 4)).toBe('December: likely cloud, 1 chance in 2; closed, 1 in 5.');
    expect(forecastLine(g, zone('nafrica'), 11)).toBe('July: likely clear, 9 chances in 10.');
    // A tie names both, the worse sky first.
    expect(forecastLine(g, zone('central'), 5)).toBe('January: likely cloud, 2 chances in 5; closed, 3 in 10.');
    expect(forecastLine(g, zone('med'), 4)).toBe('December: cloud or clear, 2 chances in 5 each; closed, 1 in 5.');
  });

  it('puts closed zones first on the month card, cloud on one line, and always one clear line', () => {
    const g = sky({ nwe: 'closed', central: 'cloud', scand: 'cloud' }, 'GER');
    setWar(g, 'GER', 'UK');
    // German air groups under North-west Europe's closed sky and Scandinavia's cloud.
    const lw = airFleetsOf(g, 'GER');
    lw[0].base = at(g, 'Holland');
    lw[1].base = at(g, 'Oslo');
    const v = skyView(g, 'GER')!;
    expect(v.open).toBe(true);
    expect(v.lines.map((l) => l.text)).toEqual([
      'North-west Europe: closed sky. Nothing flies there this month.',
      'Central Europe and Scandinavia: cloud. Raids and air support count for less.',
      'Clear sky everywhere else you fly.',
    ]);
    expect(v.lines[0].forecast).toEqual(['October: closed, for certain.']);
    expect(v.tip).toBe(WEATHER.words.tip);
    expect(monthCard(g, markFor(g), 'GER').sky?.lines.length).toBe(3);
  });

  it('keeps its clear line in a month that is clear everywhere he flies', () => {
    const g = sky({ central: 'clear', nwe: 'clear', scand: 'clear', italy: 'clear', balkans: 'clear', nrus: 'clear', srus: 'clear' }, 'GER');
    setWar(g, 'GER', 'POL');
    const v = skyView(g, 'GER')!;
    expect(v.lines).toEqual([{ sky: 'clear', text: 'Clear sky everywhere you fly.', forecast: [] }]);
    expect(v.open).toBe(false);
    expect(v.tip).toBeNull();
  });

  it('reports the groups the closed sky kept down, naming its month', () => {
    const g = sky({ nwe: 'closed' }, 'UK');
    setWar(g, 'UK', 'GER');
    g.turn = 2;
    weatherMonth(g);
    const n = airFleetsOf(g, 'UK').filter((f) => f.ftr + f.bmr > 0 && WEATHER.zones[zoneOfRegion(g, f.base)].key === 'nwe').length;
    expect(groundedReport(g, 'UK')).toBe(`In September the closed sky over North-west Europe kept ${n} of your air groups on the ground.`);
  });
});

describe('a war with weather', () => {
  it('plays to its end with the computer\'s staffs flying round the skies', () => {
    const g = free(7);
    setWeather(g, true);
    for (let k = 0; k < 14; k++) runTurnAI(g);
    expect(g.turn).toBe(15);
    expect(g.airReports.length).toBeGreaterThan(0);
    // Nothing flew from or into a closed sky.
    for (const r of g.airReports) {
      const z = r.region >= 0 ? zoneOfRegion(g, r.region) : zoneOfSea(g, r.zone);
      expect(skyOf(g, z, r.turn)).not.toBe('closed');
    }
  }, 60000);
});
