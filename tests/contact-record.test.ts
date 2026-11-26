// The contact rule's fight half: the battle record (the design director's ruling of 06:10Z, 26 November 2026: "A corps
// is held while it took part in the battle last resolved on its ground (the board's record, no window)"). The board
// kept no such fact, so the game keeps one: `g.lastBattle`, for each province the ids of the living corps that took
// part in the last land battle resolved on it, under the switch 'fromWait' only. No month in it, no count.
// The design: /home/user/runs/contact2/SPEC.md sections 3 and 6.2, with its section 13.
// The record field: kept whole in a save under the switch, never written with it off, and gone when it is turned off.
// The writer: game.ts `resolveBattle` alone (a walk-in and a probe write nothing), and the lineage: a corps made of a
// corps that took part took part too, where the game carries `fought` (a part broken off, a part rejoined, a fold's
// kept corps), and a cadre raised again under its old id took part in nothing.
// Seed 1, the opening board: Germany's II Corps in the Grenzmark, Poland's Army Poznan and its cavalry beside it.
import { describe, expect, it } from 'vitest';
import { setFromWait } from '../src/campaign/arrivalwaits';
import { breakUp, rejoin, setBreakUp } from '../src/campaign/breakup';
import { deliver } from '../src/campaign/economy';
import { foldCorps } from '../src/campaign/fold';
import { addCorps, atWar, createGame, probeLine, relocate, resolveBattle } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

const open = (): Game => startOpen(createGame(1, null));
/** The Grenzmark's defender, II Corps, and Poland's two corps in Poznan, fresh; nobody else of Germany's beside them. */
function board(on: boolean): { g: Game; def: Corps; att: Corps[] } {
  const g = open();
  if (on) setFromWait(g, true);
  const gz = g.byName.Grenzmark;
  const def = gz.units.find((u) => u.nat === 'GER' && u.name === 'II Corps')!;
  const att = g.byName.Poznan.units.filter((u) => u.nat === 'POL');
  for (const u of att) { u.ap = u.apMax; u.str = 1; u.org = 1; }
  expect(atWar(g, 'POL', 'GER')).toBe(true);
  expect(att.map((u) => u.name)).toEqual(['Army Poznan', 'Wielkopolska Cavalry']);
  return { g, def, att };
}
/** Seed 1 with Germany seated, in his own month, corps broken up under that switch. */
function seated(): Game {
  const g = startOpen(createGame(1, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 50) stepAI(g);
  setBreakUp(g, true);
  setFromWait(g, true);
  return g;
}

describe('the record field', () => {
  it('a save made under the switch keeps the record whole, and loads it back the same', () => {
    const g = open();
    setFromWait(g, true);
    g.lastBattle = { [g.byName.Berlin.id]: [3, 7], [g.byName.Warsaw.id]: [11] };
    const back = loadGame(saveGame(g))!;
    expect(back.lastBattle).toEqual(g.lastBattle);
    expect(JSON.parse(saveGame(back)).lastBattle).toEqual(g.lastBattle);
  });

  it('a war that never had the switch on writes no record in its save', () => {
    const g = open();
    expect(g.lastBattle).toBeUndefined();
    expect('lastBattle' in JSON.parse(saveGame(g))).toBe(false);
  });

  it('turning the switch off deletes the record, so the save is a never-switched war in this field', () => {
    const g = open();
    setFromWait(g, true);
    g.lastBattle = { [g.byName.Berlin.id]: [3] };
    setFromWait(g, false);
    expect(g.lastBattle).toBeUndefined();
    expect('lastBattle' in JSON.parse(saveGame(g))).toBe(false);
    // On again: an empty record, never a battle older than those missed while off.
    setFromWait(g, true);
    expect(g.lastBattle).toBeUndefined();
  });

  it('a record left on a war whose switch is cleared by hand is not saved', () => {
    const g = open();
    g.lastBattle = { [g.byName.Berlin.id]: [3] };
    expect('lastBattle' in JSON.parse(saveGame(g))).toBe(false);
  });
});

describe('the writer: the battle last resolved on a province', () => {
  it('lists the living attackers and defenders, in that order, in place of what the province held', () => {
    const { g, def, att } = board(true);
    const gz = g.byName.Grenzmark;
    g.lastBattle = { [gz.id]: [987654] };
    const rep = resolveBattle(g, att, gz);
    expect(rep.yielded).toBeFalsy();
    const want = [...att, def].filter((c) => !c.dead).map((c) => c.id);
    expect(want.length).toBeGreaterThan(0);
    expect(g.lastBattle![gz.id]).toEqual(want);
    // (No other province is written.)
    expect(Object.keys(g.lastBattle!)).toEqual([String(gz.id)]);
  });

  it('a corps destroyed in the battle is not listed', () => {
    const { g, def, att } = board(true);
    const gz = g.byName.Grenzmark;
    def.str = 0.05;
    resolveBattle(g, att, gz);
    expect(def.dead).toBe(true);
    expect(g.lastBattle![gz.id]).toEqual(att.filter((c) => !c.dead).map((c) => c.id));
    expect(g.lastBattle![gz.id]).not.toContain(def.id);
  });

  it('a battle given up without fighting (a walk-in) writes nothing, and the record stays as it was', () => {
    const { g, def, att } = board(true);
    const gz = g.byName.Grenzmark;
    def.str = 0.2; def.org = 0.3;
    def.yields = true;
    g.lastBattle = { [gz.id]: [987654] };
    const rep = resolveBattle(g, att, gz);
    expect(rep.yielded).toBe(true);
    expect(def.reg).not.toBe(gz.id);
    expect(g.lastBattle).toEqual({ [gz.id]: [987654] });
  });

  it('a probe is no battle: it writes nothing', () => {
    const { g, att } = board(true);
    const gz = g.byName.Grenzmark;
    const before = { [gz.id]: [987654] };
    g.lastBattle = structuredClone(before);
    const p = probeLine(g, att[0], gz);
    expect(p).not.toBeNull();
    expect(g.lastBattle).toEqual(before);
  });

  it('with the switch off the same battle writes no record, and the save has none', () => {
    const { g, att } = board(false);
    resolveBattle(g, att, g.byName.Grenzmark);
    expect(g.lastBattle).toBeUndefined();
    expect('lastBattle' in JSON.parse(saveGame(g))).toBe(false);
  });

  it('a war switched on keeps its record through a save; switched off, the record is gone', () => {
    const { g, att } = board(true);
    resolveBattle(g, att, g.byName.Grenzmark);
    expect(g.lastBattle).toBeDefined();
    expect(loadGame(saveGame(g))!.lastBattle).toEqual(g.lastBattle);
    setFromWait(g, false);
    expect(g.lastBattle).toBeUndefined();
  });
});

describe('the lineage: a corps made of one that took part took part too', () => {
  it('a part broken off a listed corps is listed with it', () => {
    const g = seated();
    const c = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.divs >= 3 && x.kind !== 'fort' && x.town === undefined && !x.joint)!;
    g.lastBattle = { [c.reg]: [c.id], 1: [5] };
    const p = breakUp(g, c, [1])!;
    expect(p).not.toBeNull();
    expect(g.lastBattle).toEqual({ [c.reg]: [c.id, p.id], 1: [5] });
  });

  it('a listed part rejoining an unlisted corps makes that corps listed', () => {
    const g = seated();
    const c = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.divs >= 3 && x.kind !== 'fort' && x.town === undefined && !x.joint)!;
    const p = breakUp(g, c, [1])!;
    g.lastBattle = { [c.reg]: [p.id] };
    expect(rejoin(g, p, c)).toBe(true);
    expect(g.lastBattle).toEqual({ [c.reg]: [p.id, c.id] });
  });

  it("a listed corps folded into an unlisted one makes the kept corps listed; raised again from its cadre, it is dropped", () => {
    const g = open();
    setFromWait(g, true);
    const at = g.byName.Brandenburg ?? g.byName.Berlin;
    const keep = addCorps(g, 'GER', 'Test Keep Corps', 'inf', 2, at.id, {});
    const gone = addCorps(g, 'GER', 'Test Gone Corps', 'inf', 2, at.id, {});
    for (const c of [keep, gone]) { c.str = 0.3; c.supplied = true; relocate(g, c, at); }
    g.lastBattle = { 77: [gone.id], 78: [gone.id, 4] };
    expect(foldCorps(g, keep, gone)).toBe(true);
    expect(g.lastBattle).toEqual({ 77: [gone.id, keep.id], 78: [gone.id, 4, keep.id] });
    // Raised again from its cadre under its old id: it took part in nothing.
    const n = g.nations.GER;
    expect(n.cadres?.some((x) => x.id === gone.id)).toBe(true);
    n.queue.push({ id: g.nextBuild++, what: 'corps', kind: gone.kind, divs: gone.divs, name: gone.name, region: at.id, ordered: g.turn, eta: g.turn, cost: 0, cadre: gone.id });
    deliver(g, 'GER');
    expect(gone.dead).toBe(false);
    expect(g.lastBattle).toEqual({ 77: [keep.id], 78: [4, keep.id] });
  });

  it('with the switch off nothing is carried', () => {
    const g = seated();
    setFromWait(g, false);
    const c = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.divs >= 3 && x.kind !== 'fort' && x.town === undefined && !x.joint)!;
    breakUp(g, c, [1]);
    expect(g.lastBattle).toBeUndefined();
  });
});
