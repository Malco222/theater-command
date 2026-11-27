// Rearguards in a full battle, with Units decide on (release 2, rearguard.ts; the design director's 19:22Z and 19:26Z).
// September 1939, the pursuit test's field: a Polish stack at Poznan, beaten by panzers from Neumark, falls back into
// Lodz. The pursuers lose exactly the time the rules decide, once a battle, and the pursuit is slowed by no more.
import { describe, expect, it } from 'vitest';
import { addCorps, capture, createGame, removeCorps, resolveBattle } from '../src/campaign/game';
import { garrisonOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Kind, Region } from '../src/campaign/types';

function hand(g: Game, r: Region, to: string): void {
  for (const u of [...r.units, ...garrisonOf(r)]) removeCorps(g, u);
  capture(g, r, to);
}

interface Field { g: Game; target: Region; rear: Region; def: Corps[]; att: Corps[]; guard: Corps }
/**
 * The field, with one more Polish corps of 'divs' divisions standing in 'where' (Lodz behind, or Poznan itself): the
 * rearguard when 'hold', an ordinary corps otherwise. Everything else is the same, so the two fights can be compared.
 */
function field(hold: boolean, where: 'rear' | 'target', divs: number, defKinds: Kind[] = ['inf', 'inf']): Field {
  const g = startOpen(createGame(3, null));
  g.flags.unitsDecide = 1;
  g.flags.rearguard = 1;
  const target = g.byName['Poznan'], from = g.byName['Neumark'], rear = g.byName['Lodz'];
  for (const j of target.nb) if (j !== from.id && j !== rear.id) hand(g, g.regions[j], 'GER');
  for (const u of [...target.units, ...from.units, ...rear.units]) removeCorps(g, u);
  rear.terrain = 'plains';
  const flank = rear.nb.map((j) => g.regions[j]).find((r) => r.id !== target.id && !target.nb.includes(r.id))!;
  hand(g, flank, 'GER');
  const def = defKinds.map((k, i) => addCorps(g, 'POL', `Line ${i + 1}`, k, 2, target.id, {}));
  const att = (['arm', 'arm', 'mot', 'mot'] as Kind[]).map((k, i) => addCorps(g, 'GER', `Storm ${i + 1}`, k, 3, from.id, {}));
  const at = where === 'rear' ? rear : target;
  const guard = addCorps(g, 'POL', 'Guard', 'inf', divs, at.id, {});
  for (const c of [...def, ...att, guard]) { c.supplied = true; c.ap = c.apMax; }
  // (the order as the staff or the player gives it: held where it stands, its marching given up)
  if (hold) { guard.rear = [g.turn, guard.ap, at.id]; guard.ap = 0; }
  return { g, target, rear, def, att, guard };
}

const fight = (f: Field) => resolveBattle(f.g, f.att, f.target, { fought: 3 });
const chasedOf = (rep: ReturnType<typeof fight>, name: string): number => rep.defenders.find((u) => u.name === name)?.chased ?? 0;

describe('rearguards in a full battle (Units decide)', () => {
  it('a beaten stack falls back into a rearguard\'s province: each fast pursuer loses its held delay once, and the race is slowed by exactly that', () => {
    for (const divs of [1, 2]) {
      const held = run(true, divs), open = run(false, divs);
      expect(held.rep.defenders.filter((u) => /^Line/.test(u.name)).map((u) => u.fate)).toEqual(['retreated', 'retreated']);
      expect(open.rep.defenders.filter((u) => /^Line/.test(u.name)).map((u) => u.fate)).toEqual(['retreated', 'retreated']);
      for (const c of held.f.def) expect(c.reg).toBe(held.f.rear.id);
      // Without the order the panzers (a week over open ground) outrun the foot (two) and take their share.
      for (const nm of ['Line 1', 'Line 2']) expect(chasedOf(open.rep, nm)).toBeGreaterThan(0);
      // With it, the race is slowed by the rearguard's held delay (a week a division): with one week the panzers
      // only draw level (1 + 1 is not under 2), so none are taken; with two, none either.
      for (const nm of ['Line 1', 'Line 2']) expect(chasedOf(held.rep, nm)).toBe(0);
      // Each pursuer pressing the chase lost exactly the held delay, once, though two corps were chased.
      held.f.att.forEach((c, i) => {
        if (c.dead) return;
        expect(c.ap).toBeCloseTo(Math.max(0, open.f.att[i].ap - Math.min(2, divs)), 9);
      });
      // (the fight at Poznan had no rearguard in it: no line for it, no time lost there)
      expect(held.rep.rearguard).toBeUndefined();
      // The rearguard stood unfought in Lodz and is a rearguard still.
      expect(held.f.guard.reg).toBe(held.f.rear.id);
      expect(held.f.guard.rear).toBeDefined();
    }
  });

  it('a rearguard fought and broken beside the line: the attackers lose half its delay, once; the corps it fought beside are raced with that time; the rearguard itself is chased as any corps', () => {
    const held = run(true, 2, 'target'), open = run(false, 2, 'target');
    const won = / victory$/.test(held.rep.level);
    expect(won).toBe(true);
    expect(/ victory$/.test(open.rep.level)).toBe(true);
    // Broken: half of two weeks, charged to each attacking corps once.
    expect(held.rep.rearguard).toBe(1);
    expect(held.rep.lines.some((l) => l.label === 'Rearguard' && /was broken/.test(l.value))).toBe(true);
    held.f.att.forEach((c, i) => { if (!c.dead) expect(c.ap).toBeCloseTo(Math.max(0, open.f.att[i].ap - 1), 9); });
    // The line it fought beside is raced with that week: the panzers draw level and take none.
    for (const nm of ['Line 1', 'Line 2']) {
      expect(chasedOf(open.rep, nm)).toBeGreaterThan(0);
      expect(chasedOf(held.rep, nm)).toBe(0);
    }
    // The rearguard's own pursuers are not slowed by its delay: it is chased as the guard is without the order.
    expect(chasedOf(open.rep, 'Guard')).toBeGreaterThan(0);
    expect(chasedOf(held.rep, 'Guard') > 0).toBe(true);
    // Driven out of Poznan, it is a rearguard no longer.
    if (!held.f.guard.dead) expect(held.f.guard.reg).not.toBe(held.f.target.id);
  });

  it('off, the order changes nothing: the same fight as without it', () => {
    const a = field(true, 'rear', 2), b = field(false, 'rear', 2);
    delete a.g.flags.rearguard; delete b.g.flags.rearguard;
    const ra = fight(a), rb = fight(b);
    expect(ra.defenders.map((u) => [u.name, u.fate, u.chased ?? 0])).toEqual(rb.defenders.map((u) => [u.name, u.fate, u.chased ?? 0]));
    a.att.forEach((c, i) => expect(c.ap).toBe(b.att[i].ap));
  });
});

function run(hold: boolean, divs: number, where: 'rear' | 'target' = 'rear'): { f: Field; rep: ReturnType<typeof fight> } {
  const f = field(hold, where, divs);
  return { f, rep: fight(f) };
}
