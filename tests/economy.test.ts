// The war economy, sea transport and saved games.
import { describe, expect, it } from 'vitest';
import { addCorps, createGame, removeCorps } from '../src/campaign/game';
import { PRODUCTION, cancelBuild, income, orderCorps, orderFort } from '../src/campaign/economy';
import { loadGame, peekSave, saveGame, staleSave } from '../src/campaign/save';
import { DIPLOMACY, tradePartners } from '../src/campaign/diplomacy';
import { beginHuman, humanUp, orderBuildCorps, orderShip, passMonth, runTurnAI, shipTargets, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** A game with the player at the controls of one nation, at the start of its first month. */
function asPlayer(nat: string, seed = 7): Game {
  const g = createGame(seed, nat);
  let guard = 0;
  while (!humanUp(g) && guard++ < 60) stepAI(g);
  beginHuman(g);
  return g;
}

describe('production', () => {
  it('a nation at war earns more than the same nation at peace', () => {
    const g = createGame(1, null);
    expect(income(g, 'GER')).toBeGreaterThan(20);
    const trade = tradePartners(g, 'ITA').length * DIPLOMACY.tradeIncome;
    const ita = income(g, 'ITA') - trade;
    g.nations.ITA.atWar.push('FRA');
    expect(income(g, 'ITA') - trade).toBeCloseTo(ita / PRODUCTION.peaceIncome, 5);
  });

  it('a corps is paid for at once and arrives when its months are up', () => {
    const g = asPlayer('GER');
    const ger = g.nations.GER;
    ger.pp = 100;
    const before = g.corps.filter((c) => c.nat === 'GER').length;
    const b = orderBuildCorps(g, 'inf', ger.capital)!;
    expect(b).not.toBeNull();
    expect(ger.pp).toBe(100 - PRODUCTION.builds.inf.cost);
    expect(b.eta).toBe(g.turn + PRODUCTION.builds.inf.months);
    expect(g.corps.some((c) => c.name === b.name)).toBe(false);
    for (let i = 0; i < PRODUCTION.builds.inf.months; i++) expect(passMonth(g)).toBe(true);
    const made = g.corps.find((c) => c.name === b.name && c.nat === 'GER');
    expect(made).toBeDefined();
    expect(made!.divs).toBe(PRODUCTION.builds.inf.divs);
    expect(ger.queue.length).toBe(0);
    expect(g.corps.filter((c) => c.nat === 'GER').length).toBeGreaterThan(before - 5);
  });

  it('refuses an order the treasury cannot cover, or one outside the home country', () => {
    const g = asPlayer('GER');
    g.nations.GER.pp = 5;
    expect(orderCorps(g, 'GER', 'arm', g.nations.GER.capital)).toBeNull();
    g.nations.GER.pp = 500;
    expect(orderCorps(g, 'GER', 'arm', g.nations.FRA.capital)).toBeNull();
    expect(g.nations.GER.pp).toBe(500);
  });

  it('an order can be cancelled for a full refund only in the month it was placed', () => {
    const g = asPlayer('FRA');
    const fra = g.nations.FRA;
    fra.pp = 90;
    const b = orderCorps(g, 'FRA', 'inf', fra.capital)!;
    expect(cancelBuild(g, 'FRA', b.id)).toBe(true);
    expect(fra.pp).toBe(90);
    const kept = orderCorps(g, 'FRA', 'inf', fra.capital)!;
    passMonth(g);
    expect(cancelBuild(g, 'FRA', kept.id)).toBe(false);
  });

  it('fortifications raise a region one level and face every neighbour', () => {
    const g = asPlayer('FRA');
    const fra = g.nations.FRA;
    const paris = g.regions[fra.capital];
    const was = paris.fort?.lv ?? 0;
    fra.pp = 200;
    const b = orderFort(g, 'FRA', paris.id)!;
    expect(b).not.toBeNull();
    for (let i = 0; i < PRODUCTION.fort.months && fra.alive; i++) passMonth(g);
    expect(paris.fort?.lv).toBe(was + 1);
    expect(paris.fort?.vs).toContain('FRA');
  });

  it('computer-run nations at war order new corps once their mobilisation is over', () => {
    const g = createGame(3, null);
    for (let t = 1; t <= 18; t++) runTurnAI(g);
    const named = g.corps.filter((c) => c.nat === 'GER' && /^[LXVI]+ (Panzer )?Corps$/.test(c.name) && c.xp <= 0.3);
    expect(g.nations.GER.queue.length + named.length).toBeGreaterThan(0);
    expect(g.nations.GER.pp).toBeLessThanOrEqual(PRODUCTION.aiBank[1]);
  });
});

describe('sea transport', () => {
  it('Britain can ship a corps from a home port to a friendly port across the Channel', () => {
    const g = asPlayer('UK');
    const c = g.corps.find((x) => x.nat === 'UK' && x.kind !== 'fort' && g.regions[x.reg].port && g.regions[x.reg].owner === 'UK')!;
    expect(c).toBeDefined();
    const targets = shipTargets(g, c);
    const abroad = targets.find((r) => r.owner === 'FRA');
    expect(abroad).toBeDefined();
    expect(orderShip(g, c, abroad!.id)).toBe(true);
    expect(c.reg).toBe(abroad!.id);
    expect(c.ap).toBe(0);
    expect(abroad!.units).toContain(c);
  });

  it('a nation with no troopships cannot ship', () => {
    const g = asPlayer('FRA');
    expect(g.nations.FRA.sealift).toBe(0);
    const c = g.corps.find((x) => x.nat === 'FRA' && x.kind !== 'fort' && g.regions[x.reg].port)!;
    expect(c).toBeDefined();
    expect(shipTargets(g, c).length).toBe(0);
  });
});

describe('saved games', () => {
  it('a loaded game plays out exactly as the unsaved one would have', () => {
    const straight = createGame(11, null);
    for (let t = 1; t <= 12; t++) runTurnAI(straight);

    const first = createGame(11, null);
    for (let t = 1; t <= 5; t++) runTurnAI(first);
    const text = saveGame(first);
    expect(peekSave(text)).toEqual({ human: null, turn: 6, open: false, level: null });
    const second = loadGame(text)!;
    expect(second).not.toBeNull();
    for (let t = 6; t <= 12; t++) runTurnAI(second);

    expect(second.turn).toBe(straight.turn);
    const key = (g: Game) => g.reports.slice(-150).map((r) => `${r.id}:${r.turn}.${r.week}:${r.regionName}:${r.level}:${r.attLost}:${r.defLost}`);
    expect(key(second)).toEqual(key(straight));
    expect(second.regions.map((r) => r.ctrl)).toEqual(straight.regions.map((r) => r.ctrl));
    expect(second.corps.map((c) => `${c.id}@${c.reg}:${c.str.toFixed(4)}`)).toEqual(straight.corps.map((c) => `${c.id}@${c.reg}:${c.str.toFixed(4)}`));
  });

  it('a save taken in the middle of the player\'s month keeps orders and treasury', () => {
    const g = asPlayer('GER');
    g.nations.GER.pp = 80;
    const b = orderBuildCorps(g, 'arm', g.nations.GER.capital)!;
    const h = loadGame(saveGame(g))!;
    expect(h.human).toBe('GER');
    expect(humanUp(h)).toBe(true);
    expect(h.nations.GER.pp).toBe(80 - b.cost);
    expect(h.nations.GER.queue.map((q) => q.name)).toEqual([b.name]);
    expect(h.corps.length).toBe(g.corps.length);
    for (const r of h.regions) expect(r.units.length).toBe(g.regions[r.id].units.length);
  });

  it('rejects text that is not a save', () => {
    expect(loadGame('not a save')).toBeNull();
    expect(loadGame('{"v":99}')).toBeNull();
    expect(peekSave('{}')).toBeNull();
  });

  it('knows a campaign saved on an earlier, smaller map and does not offer to continue it', () => {
    const g = createGame(4, null);
    const good = saveGame(g);
    expect(staleSave(good)).toBe(false);
    const old = JSON.parse(good);
    old.regions = old.regions.slice(0, 315);
    const text = JSON.stringify(old);
    expect(staleSave(text)).toBe(true);
    expect(peekSave(text)).toBeNull();
    expect(loadGame(text)).toBeNull();
    expect(staleSave('not a save')).toBe(false);
  });

  it('continues a campaign saved before Cassino and Anzio were cut out of the map: each starts as the ground it was cut from stands', () => {
    const g = createGame(4, null);
    for (let t = 0; t < 2; t++) runTurnAI(g);
    // The war as it would have been saved on the map of 373 regions: the Gustav Line dug in the Abruzzo with two
    // German corps on it, Rome German behind it, and the British in Naples.
    const abruzzo = g.byName['Abruzzo'], rome = g.byName['Rome'], naples = g.byName['Naples'];
    for (const r of [abruzzo, rome, naples, g.byName['Cassino'], g.byName['Anzio']]) for (const c of [...r.units]) removeCorps(g, c);
    abruzzo.ctrl = rome.ctrl = 'GER';
    abruzzo.fort = { lv: 2, vs: ['ITA'] };
    naples.ctrl = 'UK';
    const first = addCorps(g, 'GER', 'First on the line', 'inf', 2, abruzzo.id, {});
    const second = addCorps(g, 'GER', 'Second on the line', 'mtn', 2, abruzzo.id, {});
    const old = JSON.parse(saveGame(g));
    old.regions = old.regions.slice(0, 373);
    for (const k of Object.keys(old.supply)) old.supply[k] = old.supply[k].slice(0, 373);
    const text = JSON.stringify(old);
    expect(staleSave(text)).toBe(false);
    expect(peekSave(text)).toMatchObject({ turn: g.turn });
    const h = loadGame(text)!;
    expect(h.regions).toHaveLength(g.regions.length);
    expect(h.byName['Cassino']).toMatchObject({ owner: 'ITA', ctrl: 'GER', fort: { lv: 2, vs: ['ITA'] } });
    expect(h.byName['Anzio']).toMatchObject({ owner: 'ITA', ctrl: 'GER', units: [] });
    expect(h.byName['Anzio'].fort ?? null).toBeNull();
    // The works are its own from now on, not a share of its parent's.
    expect(h.byName['Cassino'].fort).not.toBe(h.byName['Abruzzo'].fort);
    // Cassino faces the British in Naples, so one of the two corps on the line takes post there. Anzio is behind it and stays empty.
    expect(h.byName['Cassino'].units.map((c) => c.id)).toEqual([first.id]);
    expect(h.byName['Abruzzo'].units.map((c) => c.id)).toEqual([second.id]);
    expect(h.corps.find((c) => c.id === first.id)!.reg).toBe(h.byName['Cassino'].id);
    // Everything else is where it was, and the war goes on.
    expect(h.corps.length).toBe(g.corps.length);
    for (const r of h.regions) if (r.id < 373) expect(r.ctrl).toBe(g.regions[r.id].ctrl);
    expect(h.supplyDirty).toBe(true);
    runTurnAI(h);
    expect(h.turn).toBe(g.turn + 1);
    for (const n of Object.keys(h.supply)) expect(h.supply[n]).toHaveLength(h.regions.length);
  });

  it('a lone garrison stays where it is: the carved place is held, but empty', () => {
    const g = createGame(4, null);
    const abruzzo = g.byName['Abruzzo'], naples = g.byName['Naples'];
    for (const r of [abruzzo, naples, g.byName['Cassino']]) for (const c of [...r.units]) removeCorps(g, c);
    abruzzo.ctrl = 'GER';
    naples.ctrl = 'UK';
    const only = addCorps(g, 'GER', 'Alone on the line', 'inf', 2, abruzzo.id, {});
    const old = JSON.parse(saveGame(g));
    old.regions = old.regions.slice(0, 373);
    const h = loadGame(JSON.stringify(old))!;
    expect(h.byName['Cassino']).toMatchObject({ ctrl: 'GER', units: [] });
    expect(h.byName['Abruzzo'].units.map((c) => c.id)).toEqual([only.id]);
  });
});
