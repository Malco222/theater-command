// War aims dealt to the great powers (the sandbox's lock sweep and playtest, finding 4; the pool is the historian's,
// thresholds part 8; after release 1). At stops 4 and 5 a computer-run great power holds two or three of its claims
// as war aims, by the war's seed; they can be read on the Powers tab; its staff takes one up as a great choice when
// its moment has come, warns the holder while the army comes up, and marches. Stops 1 to 3 read none of it.
import { describe, expect, it } from 'vitest';
import { AIMS, aimHolders, aimOf, dealStop, dealt, inDeal, isDealt, upset } from '../src/campaign/aims';
import { claimsMonth, claimsView, readable } from '../src/campaign/claims';
import { majorsOf } from '../src/campaign/diplomacy';
import { atWar, createGame, setWar } from '../src/campaign/game';
import { HISTORY, dialDraw, setStop, stopDef } from '../src/campaign/history';
import { ops, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const grand = (stop: number, human: string | null = null, seed = 3): Game => { const g = startOpen(createGame(seed, human)); if (stop !== 3) setStop(g, stop, true); return g; };
const amb = (id: string) => HISTORY.ambitions.find((a) => a.id === id)!;
const keys = (g: Game, nat: string): string[] => dealt(g, nat).map((x) => x.key);
const fall = (g: Game, nat: string): void => { g.nations[nat].alive = false; g.nations[nat].fellOn = { turn: g.turn, to: 'GER' } as Game['nations'][string]['fellOn']; };

describe('the pool', () => {
  it('is the historian\'s: real countries, ground the holder has at the start, and nothing for the powers that fought to restore borders', () => {
    const g = grand(3);
    for (const a of HISTORY.ambitions) {
      expect(g.nations[a.nat], a.id).toBeTruthy();
      expect(g.nations[a.on], a.id).toBeTruthy();
      for (const nm of a.ground) {
        expect(g.byName[nm], `${a.id}: ${nm}`).toBeTruthy();
        // (Bessarabia is Romanian until Moscow's ultimatum: its row waits on that.)
        if (!a.if) expect(g.byName[nm].owner, `${a.id}: ${nm}`).toBe(a.on);
      }
    }
    expect(HISTORY.claims.deal).toEqual({ stops: [4, 5], count: [2, 3], sets: { baltic: 'the Baltic states' } });
    expect(Object.keys(AIMS).sort()).toEqual(['GER', 'ITA', 'SOV']);
    for (const nat of ['UK', 'FRA', 'USA']) expect(HISTORY.ambitions.some((a) => a.nat === nat)).toBe(false);
    expect(AIMS.SOV.map((x) => x.key)).toEqual(['kars', 'baltic', 'iran']);
    expect(AIMS.ITA.map((x) => x.key)).toEqual(['dalmatia', 'nice', 'malta', 'ticino']);
    expect(AIMS.GER.map((x) => x.key)).toEqual(['tannenbaum', 'ore']);
    // The three Baltic claims are one aim; the Winter War is history's own choice and no part of the deal.
    const baltic = AIMS.SOV.find((x) => x.key === 'baltic')!;
    expect([baltic.name, baltic.rows.map((a) => a.on)]).toEqual(['the Baltic states', ['EST', 'LAT', 'LIT']]);
    expect(aimOf(amb('karelia'))).toBeNull();
    expect(aimOf(amb('latvia'))).toBe(baltic);
  });
});

describe('stops 1 to 3, and the player\'s own country', () => {
  it('are dealt nothing, and a staff\'s great choices are history\'s menu', () => {
    for (const stop of [1, 2, 3]) {
      const g = grand(stop);
      for (const nat of majorsOf(g)) { expect(dealStop(g, nat)).toBe(false); expect(dealt(g, nat)).toEqual([]); }
      for (const x of [...AIMS.SOV, ...AIMS.ITA, ...AIMS.GER]) expect(aimHolders(g, x.nat, x.key)).toEqual([]);
      g.turn = 40;
      fall(g, 'FRA');
      for (const x of [...AIMS.SOV, ...AIMS.ITA, ...AIMS.GER]) expect(aimHolders(g, x.nat, x.key)).toEqual([]);
      expect(claimsView(g).filter((c) => !!aimOf(amb(c.id)))).toEqual([]);
    }
    const mine = grand(5, 'SOV');
    expect(dealt(mine, 'SOV')).toEqual([]);
    expect(dealt(mine, 'ITA').length).toBeGreaterThan(0);
  });

  it('leave every list of great choices as it was but a great power\'s with aims, where the aims come last', () => {
    expect(ops('SOV').map((o) => o.id)).toEqual(['west', 'vulture', 'winter', 'aim:kars', 'aim:baltic', 'aim:iran']);
    expect(ops('GER').slice(-2).map((o) => o.id)).toEqual(['aim:tannenbaum', 'aim:ore']);
    for (const nat of ['UK', 'FRA', 'USA', 'ROM', 'HUN', 'BUL', 'FIN', 'SVK', 'ESP', 'GRE']) expect(ops(nat).some((o) => o.aim)).toBe(false);
    expect(ops('SOV')).toBe(ops('SOV'));
    const o = ops('SOV').find((x) => x.aim === 'baltic')!;
    expect(o.declare).toEqual(['EST', 'LAT', 'LIT']);
    expect([o.ratio, o.prep, o.chance, o.push]).toEqual([HISTORY.claims.odds, HISTORY.claims.notice, HISTORY.claims.chance, HISTORY.claims.push]);
    expect(o.side).toBeUndefined();
  });
});

describe('the deal', () => {
  it('gives a great power two or three of the aims its stop reads, the same all through a war and not the same in every war', () => {
    const seen = new Set<string>();
    for (let seed = 1; seed <= 30; seed++) {
      const g = grand(5, null, seed);
      const ita = keys(g, 'ITA');
      expect(ita.length === 2 || ita.length === 3).toBe(true);
      expect(new Set(ita).size).toBe(ita.length);
      for (const k of ita) expect(AIMS.ITA.some((x) => x.key === k)).toBe(true);
      g.turn = 33;
      expect(keys(g, 'ITA')).toEqual(ita);
      seen.add([...ita].sort().join());
      // Germany's two are both of the stop where anything goes; the powers that fought to restore borders hold none.
      expect(keys(g, 'GER').sort()).toEqual(['ore', 'tannenbaum']);
      for (const nat of ['UK', 'FRA', 'USA']) expect(dealt(g, nat)).toEqual([]);
    }
    expect(seen.size).toBeGreaterThan(3);
  });

  it('reads only the claims that were pressed at stop 4', () => {
    for (let seed = 1; seed <= 10; seed++) {
      const g = grand(4, null, seed);
      expect(keys(g, 'GER')).toEqual([]);
      expect(keys(g, 'SOV').sort()).toEqual(['baltic', 'iran']);
      expect(keys(g, 'ITA').sort()).toEqual(['dalmatia', 'nice']);
    }
  });

  it('stands in for the share where a great power is the claimant, and leaves every other claimant on the share', () => {
    for (let seed = 1; seed <= 20; seed++) {
      const g = grand(5, null, seed);
      const S = stopDef(g);
      for (const a of HISTORY.ambitions) {
        if (a.op || a.if) continue;
        if (inDeal(g, a)) expect(readable(g, a), `${seed} ${a.id}`).toBe(isDealt(g, a));
        else expect(readable(g, a), `${seed} ${a.id}`).toBe(dialDraw(g, 'claim', a.id) < S.share!);
      }
    }
  });

  it('can be read on the Powers tab from the first month', () => {
    const g = grand(4);
    const view = claimsView(g).filter((c) => c.nat === 'SOV').map((c) => [c.id, c.state]);
    expect(view).toEqual([['estonia', 'claims'], ['latvia', 'claims'], ['lithuania', 'claims'], ['iran', 'claims']]);
  });
});

describe('the moment', () => {
  it('at stop 4 waits for the holder to be at war, or for another great power to be hard pressed or beaten', () => {
    const g = grand(4);
    g.turn = 12;
    expect(upset(g, 'SOV')).toBe(false);
    expect(aimHolders(g, 'SOV', 'iran')).toEqual([]);
    expect(aimHolders(g, 'SOV', 'baltic')).toEqual([]);
    // The holder at war with somebody else.
    setWar(g, 'IRN', 'IRQ');
    expect(aimHolders(g, 'SOV', 'iran')).toEqual(['IRN']);
    expect(aimHolders(g, 'SOV', 'baltic')).toEqual([]);
    // France beaten: nobody is free to stop a march.
    fall(g, 'FRA');
    expect(upset(g, 'SOV')).toBe(true);
    expect(aimHolders(g, 'SOV', 'baltic')).toEqual(['EST', 'LAT', 'LIT']);
    // (A great power's own distress is no cover for itself.)
    const h = grand(4);
    h.turn = 12;
    h.nations.SOV.armyPeak = 1e6;
    h.nations.SOV.resilience = 0.01;
    expect([upset(h, 'SOV'), upset(h, 'ITA')]).toEqual([false, true]);
  });

  it('where anything goes is the claimant\'s own month, dealt from the seed, and no sooner', () => {
    for (let seed = 1; seed <= 12; seed++) {
      const g = grand(5, null, seed);
      for (const x of dealt(g, 'SOV')) {
        const month = 3 + Math.floor(dialDraw(g, 'wait', 'SOV', x.key) * stopDef(g).wait!);
        g.turn = month - 1;
        expect(aimHolders(g, 'SOV', x.key), `${seed} ${x.key}`).toEqual([]);
        g.turn = month;
        expect(aimHolders(g, 'SOV', x.key).length, `${seed} ${x.key}`).toBeGreaterThan(0);
      }
    }
  });

  it('never comes against an enemy, an ally, a holder who has lost the ground, or one who has fallen', () => {
    const g = grand(4);
    g.turn = 12;
    fall(g, 'FRA');
    expect(aimHolders(g, 'SOV', 'baltic')).toEqual(['EST', 'LAT', 'LIT']);
    setWar(g, 'SOV', 'EST');
    g.nations.LAT.alive = false;
    g.byName.Lithuania.ctrl = 'GER';
    expect(aimHolders(g, 'SOV', 'baltic')).toEqual([]);
  });
});

describe('the warning', () => {
  it('is given while the army comes up for the aim, and dropped if the staff stands it down', () => {
    const g = grand(4);
    g.turn = 12;
    fall(g, 'FRA');
    claimsMonth(g);
    expect(g.flags['amb:iran']).toBeUndefined();
    // The Soviet staff takes the aim up: its army is coming up.
    const k = ops('SOV').findIndex((o) => o.aim === 'iran');
    g.flags['st:op:SOV'] = k + 1;
    g.flags['st:opAt:SOV'] = g.turn;
    g.turn++;
    claimsMonth(g);
    expect(g.flags['amb:iran']).toBe(g.turn);
    expect(g.headlines.some((h) => /The Soviet Union presses its claim to northern Iran: its army is coming up to the frontier\..* Iran is warned that it may march\./.test(h.text))).toBe(true);
    expect(claimsView(g).find((c) => c.id === 'iran')!.state).toBe('pressing');
    // (The other aim is not being prepared, and nobody is warned for it.)
    expect(g.flags['amb:estonia']).toBeUndefined();
    delete g.flags['st:op:SOV'];
    g.turn++;
    claimsMonth(g);
    expect(g.flags['amb:iran']).toBeUndefined();
    expect(claimsView(g).find((c) => c.id === 'iran')!.state).toBe('claims');
  });
});

describe('a war for an aim', () => {
  it('is the staff\'s own choice: warned for, declared with its reason, and ended by treaty when the claimant holds what it claimed', () => {
    // Where anything goes, a war in which the Soviet Union was dealt Kars: played with nobody at the table until it has marched.
    let g: Game | null = null, month = 0;
    for (let seed = 1; seed <= 40 && !g; seed++) {
      const x = grand(5, null, seed);
      month = 3 + Math.floor(dialDraw(x, 'wait', 'SOV', 'kars') * stopDef(x).wait!);
      if (keys(x, 'SOV').includes('kars') && month <= 8) g = x;
    }
    expect(g).not.toBeNull();
    const w = g!;
    while (w.turn <= month + 14 && !w.flags['amb:go:kars']) runTurnAI(w);
    const went = w.flags['amb:go:kars'];
    expect(went).toBeGreaterThanOrEqual(month);
    // The holder was warned before the blow fell, and the declaration carries its reason.
    const warned = w.headlines.find((h) => /presses its claim to Kars and the eastern provinces/.test(h.text));
    expect(warned).toBeTruthy();
    expect(warned!.turn).toBeLessThan(went);
    expect(w.headlines.some((h) => h.turn === went && /The Soviet Union declares war on Turkey\. It has gone to war for Kars and the eastern provinces\./.test(h.text))).toBe(true);
    expect(w.flags['st:done:SOV:aim:kars']).toBe(went);
    // Played on: the two sign a treaty once the Red Army holds the provinces, or the war is still being fought for them.
    for (let k = 0; k < 8 && !w.flags['amb:done:kars'] && atWar(w, 'SOV', 'TUR'); k++) runTurnAI(w);
    if (w.flags['amb:done:kars']) {
      expect(atWar(w, 'SOV', 'TUR')).toBe(false);
      expect([w.byName.Erzurum.owner, w.byName.Trabzon.owner]).toEqual(['SOV', 'SOV']);
    }
  }, 240000);
});
