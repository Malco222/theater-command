// The pursuit of a beaten army (Units decide, #7 step 4(b); the design director's rulings of 24 Nov 2026, 12:41Z and
// 12:43Z; the historian's historian/prisoner-share-1124.md). Who is faster comes from the ground's own march costs. A
// faster enemy already beside a road out takes the men on foot who can go no other way (Beda Fomm); a faster pursuit
// with the road open takes a quarter of the men on foot who do not ride (Alamein); a stack that rides, or a pursuer
// no faster over that ground, gives nothing beyond the battle's own (Compass, forest, mountain). Off, nothing changes.
import { describe, expect, it } from 'vitest';
import { addCorps, assess, capture, createGame, ensureSupply, pace, removeCorps, resolveBattle } from '../src/campaign/game';
import { garrisonOf } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import type { Corps, Game, Kind, Region, Terrain } from '../src/campaign/types';

function hand(g: Game, r: Region, to: string): void {
  for (const u of [...r.units, ...garrisonOf(r)]) removeCorps(g, u);
  capture(g, r, to);
}

interface Field { g: Game; target: Region; rear: Region; flank: Region; def: Corps[]; att: Corps[] }
/**
 * September 1939: a Polish stack at Poznan, beaten from Neumark. Its one road out is Lodz (empty, with room), which
 * does not touch Neumark; every other neighbour of Poznan is German and empty. 'flank' is a German province beside
 * Lodz, not Poznan, where a blocking force can stand.
 */
function field(units: boolean, defKinds: Kind[], attKinds: Kind[], ground: Terrain = 'plains'): Field {
  const g = startOpen(createGame(3, null));
  if (units) g.flags.unitsDecide = 1;
  const target = g.byName['Poznan'], from = g.byName['Neumark'], rear = g.byName['Lodz'];
  expect(target.nb).toContain(from.id);
  expect(target.nb).toContain(rear.id);
  expect(from.nb).not.toContain(rear.id);
  for (const j of target.nb) if (j !== from.id && j !== rear.id) hand(g, g.regions[j], 'GER');
  for (const u of [...target.units, ...from.units, ...rear.units]) removeCorps(g, u);
  rear.terrain = ground;
  const flank = rear.nb.map((j) => g.regions[j]).find((r) => r.id !== target.id && !target.nb.includes(r.id))!;
  hand(g, flank, 'GER');
  const def = defKinds.map((k, i) => addCorps(g, 'POL', `Line ${i + 1}`, k, 2, target.id, {}));
  const att = attKinds.map((k, i) => addCorps(g, 'GER', `Storm ${i + 1}`, k, 3, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  return { g, target, rear, flank, def, att };
}

const fight = (f: Field) => resolveBattle(f.g, f.att, f.target, { fought: 3 });
const fates = (rep: ReturnType<typeof fight>) => rep.defenders.filter((u) => !u.guard).map((u) => u.fate);
const chased = (rep: ReturnType<typeof fight>) => rep.defenders.filter((u) => !u.guard).map((u) => Math.round(((u.chased ?? 0) / Math.max(1, u.men)) * 100));
const FOOT: Kind[] = ['inf', 'inf', 'inf', 'inf', 'mot'];
const PANZERS: Kind[] = ['arm', 'arm', 'mot', 'mot'];

describe('the pace of a pursuit, from the ground', () => {
  it('open ground: tanks and lorries outrun foot; forest: lorries do, tanks do not; mountain and marsh: nobody', () => {
    const f = field(true, ['inf', 'mtn'], ['arm', 'mot']);
    const [inf, mtn] = f.def, [arm, mot] = f.att;
    const at = (t: Terrain) => { f.rear.terrain = t; return [pace(f.g, arm, f.rear), pace(f.g, mot, f.rear), pace(f.g, inf, f.rear), pace(f.g, mtn, f.rear)]; };
    expect(at('plains')).toEqual([1, 1, 2, 2]);
    expect(at('forest')).toEqual([2, 1, 2, 2]);
    expect(at('mountain')).toEqual([3, 3, 3, 2]);
    expect(at('marsh')).toEqual([3, 3, 3, 3]);
  });
});

describe('a beaten stack chased off the field', () => {
  it('off: falls back whole, as today', () => {
    const rep = fight(field(false, FOOT, PANZERS));
    expect(fates(rep)).toEqual(['retreated', 'retreated', 'retreated', 'retreated', 'retreated']);
    expect(chased(rep)).toEqual([0, 0, 0, 0, 0]);
  });

  it('Alamein: a faster pursuit with the road open takes a quarter of the foot, none of the lorried', () => {
    const f = field(true, FOOT, PANZERS);
    const rep = fight(f);
    expect(fates(rep)).toEqual(['retreated', 'retreated', 'retreated', 'retreated', 'retreated']);
    for (const c of f.def) expect(c.reg).toBe(f.rear.id);
    // Of the men each corps had left after the battle, a quarter of the foot; of the stack, about 0.25 x 4/5.
    const left = rep.defenders.filter((u) => !u.guard).map((u, i) => (u.chased ?? 0) / ((u.chased ?? 0) + f.g.corps.find((c) => c.id === f.def[i].id)!.divs * f.def[i].str * 15000));
    for (const s of left.slice(0, 4)) expect(s).toBeCloseTo(0.25, 2);
    expect(left[4]).toBe(0);
    expect(rep.prisoners).toBe(rep.defenders.reduce((s, u) => s + (u.chased ?? 0), 0));
  });

  it('Compass: a stack that rides gives nothing beyond the battle\'s own', () => {
    const rep = fight(field(true, ['mot', 'mot', 'arm'], PANZERS));
    expect(fates(rep)).toEqual(['retreated', 'retreated', 'retreated']);
    expect(chased(rep)).toEqual([0, 0, 0]);
    expect(rep.prisoners).toBe(0);
  });

  it('a pursuer on foot is no faster than foot: nothing beyond the battle\'s own', () => {
    const rep = fight(field(true, FOOT, ['inf', 'inf', 'inf', 'inf']));
    expect(chased(rep)).toEqual([0, 0, 0, 0, 0]);
  });

  it('forest: armour does not outrun foot there, lorried troops do (ground.json gives lorries 1 in forest)', () => {
    expect(chased(fight(field(true, FOOT, ['arm', 'arm', 'arm'], 'forest')))).toEqual([0, 0, 0, 0, 0]);
    const rep = fight(field(true, FOOT, ['mot', 'mot', 'mot', 'mot'], 'forest'));
    expect(chased(rep).slice(0, 4).every((p) => p > 0)).toBe(true);
    expect(chased(rep)[4]).toBe(0);
  });

  it('a winner more disorganised than the men it chases follows at foot pace (DD 13:10Z): nothing beyond the battle\'s own', () => {
    const f = field(true, FOOT, PANZERS);
    for (const c of f.att) c.org = 0.15;
    const rep = fight(f);
    expect(fates(rep)).toEqual(['retreated', 'retreated', 'retreated', 'retreated', 'retreated']);
    for (const c of f.att.filter((u) => !u.dead)) for (const d of f.def) expect(c.org).toBeLessThan(d.org);
    expect(chased(rep)).toEqual([0, 0, 0, 0, 0]);
  });

  it('mountain: nobody outruns anybody', () => {
    expect(chased(fight(field(true, FOOT, PANZERS, 'mountain')))).toEqual([0, 0, 0, 0, 0]);
  });

  const blockAt = (f: Field, kind: Kind, divs: number, str = 1): Corps => {
    const b = addCorps(f.g, 'GER', 'Block', kind, divs, f.flank.id, {});
    b.supplied = true; b.ap = b.apMax; b.str = str; b.org = 1;
    return b;
  };
  const breakouts = (f: Field) => f.g.reports.filter((r) => r.region === f.rear.id && r.attNat === 'POL');

  it('Mons: a strong block holds the breakout; the foot is taken whole, the lorried corps gets out', () => {
    const f = field(true, FOOT, PANZERS);
    const block = blockAt(f, 'arm', 6);
    const ap = block.ap;
    const rep = fight(f);
    const out = breakouts(f);
    expect(out.length).toBe(1);
    expect(out[0].taken).toBe(false);
    expect(out[0].breakout).toBe(true);
    expect(fates(rep)).toEqual(['surrendered', 'surrendered', 'surrendered', 'surrendered', 'retreated']);
    expect(f.def[4].reg).toBe(f.rear.id);
    expect(rep.defenders[0].roadsCut).toEqual([f.rear.id]);
    expect(block.reg).toBe(f.flank.id);
    expect(block.ap).toBe(ap - pace(f.g, block, f.rear));
    expect(rep.prisoners).toBeGreaterThan(0);
  });

  it('Montelimar: a thin block is broken through; the stack falls back by that road and the foot pays only the share', () => {
    const f = field(true, FOOT, PANZERS);
    const block = blockAt(f, 'mot', 1, 0.2);
    const rep = fight(f);
    const out = breakouts(f);
    expect(out.length).toBe(1);
    expect(out[0].taken).toBe(true);
    expect(fates(rep)).toEqual(['retreated', 'retreated', 'retreated', 'retreated', 'retreated']);
    for (const c of f.def) expect(c.reg).toBe(f.rear.id);
    // Of the men each foot corps had left after the battle and the breakout, a quarter; none of the lorried.
    const left = rep.defenders.filter((u) => !u.guard).map((u, i) => (u.chased ?? 0) / ((u.chased ?? 0) + f.def[i].divs * f.def[i].str * 15000));
    for (const s of left.slice(0, 4)) expect(s).toBeCloseTo(0.25, 2);
    expect(left[4]).toBe(0);
    // The beaten cutter is booked by the ordinary rules: it falls back off the road or is taken there.
    expect(f.rear.units).not.toContain(block);
    expect(block.dead === true || f.g.regions[block.reg].units.includes(block)).toBe(true);
  });

  it('one breakout per beaten stack, and none where nobody would be cut off', () => {
    const f = field(true, FOOT, PANZERS);
    fight(f);
    expect(breakouts(f).length).toBe(0);
    const h = field(true, ['mot', 'arm'], PANZERS);
    blockAt(h, 'arm', 6);
    fight(h);
    expect(breakouts(h).length).toBe(0);
  });

  it('a block more disorganised than the beaten corps cuts nothing: the stack falls back, the foot pays the share', () => {
    const f = field(true, FOOT, PANZERS);
    const block = blockAt(f, 'arm', 6);
    block.org = 0.05;
    const ap = block.ap;
    const rep = fight(f);
    expect(breakouts(f).length).toBe(0);
    expect(fates(rep)).toEqual(['retreated', 'retreated', 'retreated', 'retreated', 'retreated']);
    expect(block.ap).toBe(ap);
  });

  it('a road with its own side standing in it cannot be cut', () => {
    const f = field(true, FOOT, PANZERS);
    const block = addCorps(f.g, 'GER', 'Block', 'arm', 2, f.flank.id, {});
    block.supplied = true; block.ap = block.apMax;
    const guard = addCorps(f.g, 'POL', 'Reserve', 'inf', 1, f.rear.id, {});
    guard.supplied = true;
    const rep = fight(f);
    expect(fates(rep)).toEqual(['retreated', 'retreated', 'retreated', 'retreated', 'retreated']);
  });

  it('a block with no fuel (unsupplied) or no action points left cuts nothing', () => {
    for (const [k, spoil] of [[0, (c: Corps) => { c.supplied = false; }], [1, (c: Corps) => { c.ap = 0; }]] as const) {
      const f = field(true, FOOT, PANZERS);
      const block = addCorps(f.g, 'GER', 'Block', 'arm', 2, f.flank.id, {});
      block.supplied = true; block.ap = block.apMax;
      ensureSupply(f.g);
      spoil(block);
      expect([k, fates(fight(f))]).toEqual([k, ['retreated', 'retreated', 'retreated', 'retreated', 'retreated']]);
    }
  });

  it('off: the block changes nothing', () => {
    const f = field(false, FOOT, PANZERS);
    const block = addCorps(f.g, 'GER', 'Block', 'arm', 2, f.flank.id, {});
    block.supplied = true; block.ap = block.apMax;
    expect(fates(fight(f))).toEqual(['retreated', 'retreated', 'retreated', 'retreated', 'retreated']);
  });
});

describe('the card reads the pursuit to both sides (129(d))', () => {
  const line = (f: Field) => assess(f.g, f.att, f.target).lines.find((l) => l.label === 'Pursuit');
  it('the attacker reads their foot, the defender ours', () => {
    const f = field(true, FOOT, PANZERS);
    f.g.human = 'GER';
    expect(line(f)?.value).toBe('Their foot cannot outrun our armour');
    f.g.human = 'POL';
    expect(line(f)?.value).toBe('Our foot cannot outrun their armour');
  });
  it('a cut road is named', () => {
    const f = field(true, FOOT, PANZERS);
    const block = addCorps(f.g, 'GER', 'Block', 'arm', 2, f.flank.id, {});
    block.supplied = true; block.ap = block.apMax;
    f.g.human = 'POL';
    expect(line(f)?.value).toBe(`Our road out is cut at ${f.rear.name}`);
    expect(assess(f.g, f.att, f.target).lines.find((l) => l.label === 'Breakout')?.value).toBe(`Breaking out at ${f.rear.name}`);
    expect(block.ap).toBe(block.apMax);
    expect(block.reg).toBe(f.flank.id);
    expect(f.flank.units).toContain(block);
    expect(f.rear.units).not.toContain(block);
  });
  it('off, or a stack that rides: no line', () => {
    expect(line(field(false, FOOT, PANZERS))).toBeUndefined();
    expect(line(field(true, ['mot', 'arm'], PANZERS))).toBeUndefined();
  });
});
