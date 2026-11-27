// Ruling 227's opening pace (b444, b446, b447), on the fresh map of the grand campaign: overwhelming air in a theatre
// holds the sky over its battles unless contested, and a staff names the battles of its main effort for its air groups.
import { describe, expect, it } from 'vitest';
import { AIRWAR, airOverBattle, effortBattles, skyByNumbers } from '../src/campaign/air';
import { allied, createGame } from '../src/campaign/game';
import { fieldCtrl } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { hasTech, techOf } from '../src/campaign/tech';
import type { AirReport, Game, PlanPhase } from '../src/campaign/types';

const fresh = (): Game => startOpen(createGame(7, null));
const GER = new Set(['GER']), POL = new Set(['POL']);
const id = (g: Game, n: string): number => g.byName[n].id;
/** A superiority fight over a region this month, won or lost by the raider. */
const fought = (g: Game, region: number, attNat: string, defNat: string, sky: boolean): AirReport =>
  ({ id: g.airReports.length + 1, turn: g.turn, mission: 'superiority', region, zone: -1, place: g.regions[region].name, attNat, defNat, att: [], def: [], lines: [], odds: 1, level: 'Draw', sky } as unknown as AirReport);
describe('ruling 227 (b444, b446): overwhelming air in a theatre holds the sky over its battles', () => {
  it('gives Germany the sky over Poland in September 1939 by its numbers, a theatre it outnumbers several times over', () => {
    const g = fresh();
    expect(g.nations.GER.atWar).toContain('POL');
    for (const n of ['Warsaw', 'Lodz', 'Poznan', 'Krakow']) {
      const r = id(g, n);
      expect(airOverBattle(g, GER, POL, r)).toBeGreaterThanOrEqual(AIRWAR.sky.own * airOverBattle(g, POL, GER, r));
      expect(skyByNumbers(g, GER, POL, r)).toBe('att');
    }
    // Where the Luftwaffe cannot reach and Poland's few can, the sky is Poland's.
    expect(skyByNumbers(g, GER, POL, id(g, 'Wilno'))).toBe('def');
  });

  it('never holds the sky over a defender\'s home ground that has early-warning radar: it always contests', () => {
    const g = fresh();
    const warsaw = id(g, 'Warsaw');
    expect(hasTech(g, 'POL', AIRWAR.sky.radar)).toBe(false);
    expect(skyByNumbers(g, GER, POL, warsaw)).toBe('att');
    techOf(g, 'POL')!.done.push(AIRWAR.sky.radar);
    expect(skyByNumbers(g, GER, POL, warsaw)).toBeNull();
    // Britain has its chain at the start of the war.
    expect(hasTech(g, 'UK', AIRWAR.sky.radar)).toBe(true);
  });

  it('is contested where the sky there has been fought for lately and the side with the numbers did not win it', () => {
    const lost = fresh(), won = fresh(), theirs = fresh();
    const warsaw = id(lost, 'Warsaw');
    lost.airReports.push(fought(lost, warsaw, 'GER', 'POL', false));
    expect(skyByNumbers(lost, GER, POL, warsaw)).toBeNull();
    theirs.airReports.push(fought(theirs, warsaw, 'POL', 'GER', true));
    expect(skyByNumbers(theirs, GER, POL, warsaw)).toBeNull();
    won.airReports.push(fought(won, warsaw, 'GER', 'POL', true));
    expect(skyByNumbers(won, GER, POL, warsaw)).toBe('att');
    // Elsewhere the fight over Warsaw changes nothing.
    expect(skyByNumbers(lost, GER, POL, id(lost, 'Lodz'))).toBe('att');
  });
});

describe('ruling 227 (b447): a staff names the battles of its main effort to its air', () => {
  it('reads the main front\'s places in an attack, the assembly\'s enemy neighbours in a redeployment, and nothing in defence', () => {
    const g = fresh();
    const warsaw = id(g, 'Warsaw'), lodz = id(g, 'Lodz');
    const attack = { posture: 'attack', targets: ['Warsaw', 'Lodz', 'Paris'], main: ['Warsaw', 'Berlin'] } as unknown as PlanPhase;
    // The main front alone, and only what an enemy holds.
    expect(effortBattles(g, 'GER', attack)).toEqual([warsaw]);
    // With no main front named, every live front's places.
    expect(effortBattles(g, 'GER', { ...attack, main: undefined }).sort()).toEqual([lodz, warsaw, id(g, 'Paris')].sort());
    expect(effortBattles(g, 'GER', { ...attack, posture: 'defend' })).toEqual([]);
    expect(effortBattles(g, 'GER', null)).toEqual([]);
    const ready = effortBattles(g, 'GER', { posture: 'redeploy', assembly: ['Upper Silesia'] } as unknown as PlanPhase);
    expect(ready.length).toBeGreaterThan(0);
    for (const r of ready) {
      expect(g.byName['Upper Silesia'].nb).toContain(r);
      expect(allied(g, 'GER', fieldCtrl(g, g.regions[r]))).toBe(false);
    }
  });
});
