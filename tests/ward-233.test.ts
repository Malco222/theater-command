// Ruling 233, riders b612 and b627: the ward. A computer staff's defence counted only its own country and what it had
// taken, so when the Allies took Sardinia no German corps ever stood in Italy, and at Italy's signing the Ally on the
// island inherited Rome and the whole mainland (94 wars of 96). The rule: a computer staff at the dial's looser stops
// (3, 4 and 5) guards the capital of an ally whose own leaving is in play: an enemy holds or stands on a ground that
// ally's leaving event names, and the capital can be reached overland through its own side's ground. One corps, kept
// by the strongest army among the ally's partners on the Continent. The guard is named once and kept until the ward
// ends: whoever can stand on that ally's ground soonest. The trains take it onto that ground where a line is open; within
// two marches of it the guard walks; otherwise it rides as near the capital as the trains run and marches on. Where the
// capital is full of the ally's own corps it stands on the nearest ground that has room (b627: sent as a watched
// harbour's corps is, no German corps reached Italian ground in 34 wars of the 86 where the ward was set, with Rome's
// five places taken by Italy's own guard or with no train able to run the whole way into Rome).
import { describe, expect, it } from 'vitest';
import open from '../data/campaign/open.json';
import { keepWards } from '../src/campaign/ai';
import { leavings } from '../src/campaign/diplomacy';
import { addCorps, allied, capture, createGame, declareWar, hasRoom, relocate } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { cutRail } from '../src/campaign/rails';
import { startOpen, staffPhase, wards } from '../src/campaign/staff';
import type { Corps, Game, PlanPhase, Region } from '../src/campaign/types';

const SIX = ['Sicily', 'Sardinia', 'Calabria', 'Apulia', 'Naples', 'Rome'];
const WARD: PlanPhase = { from: 1, to: 99, posture: 'defend', ward: { Rome: 1 } };

/** September 1939, nobody playing, with Italy in the war beside Germany against Britain. */
function italyAtWar(stop = 3, human: string | null = null): Game {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) expect(setStop(g, stop, true)).toBe(true);
  declareWar(g, 'ITA', 'UK');
  expect(allied(g, 'GER', 'ITA')).toBe(true);
  return g;
}
/** A British corps set down on Calabria, which stands empty: an enemy stands on one of the six grounds. */
const landOnCalabria = (g: Game): Corps => {
  const c = addCorps(g, 'UK', 'Test Landing Corps', 'inf', 2, 'London', {});
  expect(g.byName.Calabria.units).toHaveLength(0);
  relocate(g, c, g.byName.Calabria);
  return c;
};
const guard = (g: Game): Corps | undefined => g.corps.find((c) => c.id === g.flags['ward:GER:Rome:0']);
const germansIn = (g: Game, r: Region): Corps[] => r.units.filter((c) => c.nat === 'GER' && !c.dead);
const onItalianGround = (g: Game): Corps[] => g.corps.filter((c) => c.nat === 'GER' && !c.dead && g.regions[c.reg].owner === 'ITA' && !g.regions[c.reg].overseas);
/**
 * The Brenner's line cut, as bombing cuts it (ruling 233, noted 10:33Z, laid the pass from Tyrol to Alto Adige): the
 * board as it was before, when no line ran from Germany into Italy but through Switzerland, France or Slovenia.
 */
const cutBrenner = (g: Game): void => { expect(cutRail(g, g.byName['Alto Adige'].id, 'bombed')).toBe(true); };
/** A new month for Germany's corps: every one has its marching time again. */
const newMonth = (g: Game): void => { for (const c of g.corps) if (c.nat === 'GER') c.ap = c.apMax; };

describe('ruling 233, rider b612: the ward is set by the ally\'s leaving event', () => {
  it('the dial says where it applies: stops 3, 4 and 5, one corps', () => {
    expect(open.ward).toEqual({ stops: [3, 4, 5], corps: 1 });
  });

  it('in a grand campaign the leaving events name Italy\'s six grounds, and no other ally of Germany\'s', () => {
    const g = italyAtWar();
    const of = (nat: string) => leavings(g).filter((l) => l.nation === nat);
    expect(of('ITA')).toEqual([{ nation: 'ITA', grounds: SIX }]);
    for (const nat of ['ROM', 'BUL', 'FIN', 'HUN', 'SVK']) expect(of(nat)).toEqual([]);
  });

  it('no ward while no enemy is on the six grounds; one for Rome when an enemy stands on one, and when he holds one', () => {
    const g = italyAtWar();
    expect(wards(g, 'GER')).toEqual([]);
    const c = landOnCalabria(g);
    expect(wards(g, 'GER')).toEqual(['Rome']);
    relocate(g, c, g.byName.London);
    expect(wards(g, 'GER')).toEqual([]);
    capture(g, g.byName.Sardinia, 'UK');
    expect(wards(g, 'GER')).toEqual(['Rome']);
  });

  it('one guard, not five: the strongest partner on the Continent keeps it, and Italy keeps none for herself', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    expect(wards(g, 'GER')).toEqual(['Rome']);
    for (const nat of ['HUN', 'SVK', 'ROM', 'BUL', 'ITA']) expect(wards(g, nat)).toEqual([]);
  });

  it('not at the book\'s stops, and never for the player\'s Germany', () => {
    for (const stop of [1, 2]) {
      const g = italyAtWar(stop);
      landOnCalabria(g);
      expect(wards(g, 'GER')).toEqual([]);
    }
    for (const stop of [4, 5]) {
      const g = italyAtWar(stop);
      landOnCalabria(g);
      expect(wards(g, 'GER')).toEqual(['Rome']);
    }
    const g = italyAtWar(3, 'GER');
    landOnCalabria(g);
    expect(wards(g, 'GER')).toEqual([]);
  });

  it('no ward once Rome is no longer Italy\'s, nor while Italy is at peace with whoever stands there', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    capture(g, g.byName.Rome, 'UK');
    expect(wards(g, 'GER')).toEqual([]);
    // (Italy at peace: a neutral's corps on Calabria is no enemy of hers.)
    const h = startOpen(createGame(3, null));
    relocate(h, addCorps(h, 'UK', 'Test Landing Corps', 'inf', 2, 'London', {}), h.byName.Calabria);
    expect(wards(h, 'GER')).toEqual([]);
  });

  it('the staff\'s month carries the ward by name, and Rome is on no staging or watch list for it', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    const ph = staffPhase(g, 'GER');
    expect(ph?.ward).toEqual({ Rome: 1 });
    expect(ph?.staging?.Rome).toBeUndefined();
    expect(ph?.watch ?? []).not.toContain('Rome');
  });
});

describe('ruling 233, rider b627: the guard is named once and kept, and gets there', () => {
  it('one corps of infantry is named, of two divisions or more, and nobody else is sent after it', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    expect(guard(g)).toBeUndefined();
    keepWards(g, 'GER', WARD, new Set());
    const c = guard(g)!;
    expect(c).toBeDefined();
    expect(c.nat).toBe('GER');
    expect(c.kind).toBe('inf');
    expect(c.divs).toBeGreaterThanOrEqual(2);
    expect(c.hold).toBeFalsy();
    for (let month = 2; month <= 6; month++) {
      newMonth(g);
      keepWards(g, 'GER', WARD, new Set());
      expect(guard(g)).toBe(c);
    }
    expect(onItalianGround(g)).toEqual([c]);
  });

  it('it rides as far as the trains run and marches on: on Italian ground in the second month, at Rome in the third', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    cutBrenner(g);
    const rome = g.byName.Rome;
    keepWards(g, 'GER', WARD, new Set());
    const c = guard(g)!;
    // (With the Brenner cut no line runs from Germany into Italy but through Switzerland, France or Slovenia: the trains set him down in Austria.)
    expect(g.regions[c.reg].owner).toBe('GER');
    expect(c.ap).toBe(0);
    newMonth(g);
    keepWards(g, 'GER', WARD, new Set());
    expect(g.regions[c.reg].owner).toBe('ITA');
    newMonth(g);
    keepWards(g, 'GER', WARD, new Set());
    expect(c.reg).toBe(rome.id);
    expect(germansIn(g, rome)).toEqual([c]);
  });

  it('with Rome full of Italy\'s own corps the guard stands beside it on Italian ground, and goes in when there is room', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    const rome = g.byName.Rome;
    // (Rome's room is five corps, engineers apart: Italy's own guard fills it.)
    const probe = g.corps.find((u) => u.nat === 'GER' && u.kind === 'inf')!;
    const fill: Corps[] = [];
    while (hasRoom(g, rome, probe)) fill.push(addCorps(g, 'ITA', `Test Rome Corps ${fill.length + 1}`, 'inf', 2, 'Rome', {}));
    expect(fill.length).toBeGreaterThan(0);
    for (let month = 1; month <= 4; month++) { keepWards(g, 'GER', WARD, new Set()); newMonth(g); }
    const c = guard(g)!;
    expect(c.reg).not.toBe(rome.id);
    expect(g.regions[c.reg].owner).toBe('ITA');
    expect(rome.nb).toContain(c.reg);
    expect(onItalianGround(g)).toEqual([c]);
    // (One of Italy's corps marches off: the guard takes its place.)
    relocate(g, fill[0], g.byName.Naples);
    keepWards(g, 'GER', WARD, new Set());
    expect(c.reg).toBe(rome.id);
  });

  it('a German corps of the field army already standing in Italy is the guard: no other is sent', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    const there = addCorps(g, 'GER', 'Test Corps in Lombardy', 'inf', 3, 'Berlin', {});
    relocate(g, there, g.byName.Lombardy);
    keepWards(g, 'GER', WARD, new Set());
    expect(guard(g)).toBe(there);
    for (let month = 2; month <= 4; month++) { newMonth(g); keepWards(g, 'GER', WARD, new Set()); }
    expect(there.reg).toBe(g.byName.Rome.id);
    expect(onItalianGround(g)).toEqual([there]);
  });

  it('a corps within two marches of Italian ground is named before one the trains would only bring nearer, and walks in the first month', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    // (With the Brenner open the trains would set some corps down in Italy first: here it is cut, and they only bring one nearer.)
    cutBrenner(g);
    // (Two corps in the Tyrol, so that the one taken leaves nobody alone: Alto Adige is the next province.)
    const pair = [1, 2].map((n) => { const c = addCorps(g, 'GER', `Test Tyrol Corps ${n}`, 'inf', 3, 'Berlin', {}); relocate(g, c, g.byName.Tyrol); return c; });
    keepWards(g, 'GER', WARD, new Set());
    const c = guard(g)!;
    expect(pair).toContain(c);
    expect(g.regions[c.reg].owner).toBe('ITA');
    expect(onItalianGround(g)).toEqual([c]);
  });

  it('with a line open onto Italian ground the trains set the guard down on it in the first month', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    // (With the Brenner cut no line crosses the Alps from Germany: the one from Slovenia into Friuli is open once Slovenia is German.)
    cutBrenner(g);
    capture(g, g.byName.Slovenia, 'GER');
    const pair = [1, 2].map((n) => { const c = addCorps(g, 'GER', `Test Slovenia Corps ${n}`, 'inf', 3, 'Berlin', {}); relocate(g, c, g.byName.Slovenia); c.ap = c.apMax; return c; });
    keepWards(g, 'GER', WARD, new Set());
    const c = guard(g)!;
    expect(pair).toContain(c);
    expect(g.regions[c.reg].owner).toBe('ITA');
    expect(c.ap).toBe(0);
    expect(onItalianGround(g)).toEqual([c]);
  });

  it('a guard that falls is replaced; the ward over, nothing is named or moved', () => {
    const g = italyAtWar();
    landOnCalabria(g);
    keepWards(g, 'GER', WARD, new Set());
    const first = guard(g)!;
    first.dead = true;
    newMonth(g);
    keepWards(g, 'GER', WARD, new Set());
    const second = guard(g)!;
    expect(second).toBeDefined();
    expect(second).not.toBe(first);
    const at = second.reg;
    newMonth(g);
    keepWards(g, 'GER', { from: 1, to: 99, posture: 'defend' }, new Set());
    expect(guard(g)).toBe(second);
    expect(second.reg).toBe(at);
    expect(second.ap).toBe(second.apMax);
  });
});
