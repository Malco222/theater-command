// Version 61: fronts that move again (the grand campaign). Works wear down under attack, and nobody digs in a
// month in which he was attacked, so a second blow meets what the first one left. The first month's digging does
// half the job. A new offensive catches a line off balance for a month, and for three the first time an army
// attacks a country. Attacked from three sides a defence cannot cover every road, and guns go in against works
// without taking room on the roads. The Battle Ticket says how good the troops are and how far the works have
// got. Terms go to whoever stands on the ground, and a town never holds more corps than it has room for.
// None of this is in the historical scenarios. The numbers are in data/campaign/defence.json.
import { describe, expect, it } from 'vitest';
import { DEFENCE, DUG_IN, GUNS, addCorps, assess, atWar, capitulate, createGame, declareWar, endPhase, frontage, limitAttackers, relocate, resolveBattle, worksOf, worksShare } from '../src/campaign/game';
import { fireEvent } from '../src/campaign/diplomacy';
import { loadGame, saveGame } from '../src/campaign/save';
import { garrisonOf, joinTown, roomIn, siegeSweep } from '../src/campaign/siege';
import { startOpen } from '../src/campaign/staff';
import { attackOutlook, beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

const free = (seed: number, human: string | null = null): Game => startOpen(createGame(seed, human));
function playing(human = 'GER', seed = 3): Game {
  const g = free(seed, human);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

interface Line { target: Region; from: Region; def: Corps[]; att: Corps[] }
/** A Polish line round Lodz with German infantry in front of it, in supply. The German offensive is an old one. */
function line(g: Game, defenders = 2, attackers = 2): Line {
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  for (const u of [...from.units]) if (u.nat === 'GER' && u.kind !== 'fort') relocate(g, u, g.byName['Berlin']);
  const def = Array.from({ length: defenders }, (_, i) => addCorps(g, 'POL', `Line ${i + 1}`, 'inf', 3, target.id, {}));
  const att = Array.from({ length: attackers }, (_, i) => addCorps(g, 'GER', `Storm ${i + 1}`, 'inf', 3, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  g.nations.GER.offStart.POL = g.turn - 6;
  g.nations.GER.lastFight.POL = g.turn - 1;
  return { target, from, def, att };
}
const works = (cs: Corps[], months: number): void => { for (const c of cs) { c.dug = true; c.dugFor = months; } };
const M = DEFENCE.entrench.months;
const W = DEFENCE.wear;
function again(g: Game, l: Line): { g: Game; l: Line } {
  const h = loadGame(saveGame(g))!;
  const find = (c: Corps): Corps => h.corps.find((x) => x.id === c.id)!;
  return { g: h, l: { target: h.regions[l.target.id], from: h.regions[l.from.id], def: l.def.map(find), att: l.att.map(find) } };
}
/** A battle the attack loses, found by trying saves of the same game until the day's luck says so. */
function thrownBack(g0: Game, l0: Line): { g: Game; l: Line; rep: ReturnType<typeof resolveBattle> } {
  for (let k = 0; k < 40; k++) {
    const p = again(g0, l0);
    p.g.rng.next(); for (let i = 0; i < k; i++) p.g.rng.next();
    const rep = resolveBattle(p.g, p.l.att, p.l.target);
    if (!rep.taken && !rep.siege && !rep.gates) return { ...p, rep };
  }
  throw new Error('the attack was never thrown back');
}

describe('works wear down under attack', () => {
  it('the first month of digging does half the job, and three finish it', () => {
    expect(DEFENCE.entrench.share).toEqual([0.5, 0.75, 1]);
    expect(worksShare(0)).toBe(0);
    expect(worksShare(0.5)).toBeCloseTo(0.25, 6);
    expect(worksShare(1)).toBeCloseTo(0.5, 6);
    expect(worksShare(2)).toBeCloseTo(0.75, 6);
    expect(worksShare(M)).toBe(1);
    expect(worksShare(M + 4)).toBe(1);
    const g = playing('GER');
    const { target, def, att } = line(g);
    for (const [months, share] of [[1, 0.5], [2, 0.75], [M, 1]] as const) {
      works(def, months);
      const a = assess(g, att, target);
      expect(a.prepared).toBeCloseTo(share, 6);
      expect(a.works).toBeCloseTo(months, 6);
    }
    // More months than the works take add nothing, to the line or to what an attack must wear away.
    works(def, M + 5);
    expect(worksOf(def[0])).toBe(M);
    expect(assess(g, att, target).works).toBeCloseTo(M, 6);
  });

  it('an attack thrown back takes a month off them, and whoever was attacked does no digging that month', () => {
    const g = playing('GER');
    const l0 = line(g, 3, 2);
    works(l0.def, M + 2);
    const a = assess(g, l0.att, l0.target);
    expect(a.odds).toBeGreaterThan(W.fullAt);
    expect(a.wear).toBeCloseTo(W.perAttack, 6);
    const { g: h, l, rep } = thrownBack(g, l0);
    expect(rep.worn).toBeCloseTo(W.perAttack, 1);
    expect(rep.works).toBeCloseTo(M - W.perAttack, 1);
    for (const c of l.def) { expect(c.dug).toBe(true); expect(c.dugFor).toBeCloseTo(M - W.perAttack, 6); expect(c.pressed).toBe(true); }
    // The second blow meets less: the same force has better luck against what is left.
    for (const c of l.att) { c.ap = c.apMax; c.fought = false; }
    const next = assess(h, l.att, l.target);
    expect(next.prepared).toBeLessThan(a.prepared);
    expect(next.dice[0]).toBeGreaterThan(a.dice[0]);
    // Their own month ends: no digging in a month under attack, and the mark is cleared.
    const rested = (): void => { for (const c of l.def) { c.fought = false; c.ap = c.apMax; } };
    rested();
    endPhase(h, 'POL');
    for (const c of l.def) { expect(c.dug).toBe(true); expect(c.dugFor).toBeCloseTo(M - W.perAttack + W.regrow, 6); expect(c.pressed).toBeUndefined(); }
    // A quiet month builds them up again.
    rested();
    endPhase(h, 'POL');
    for (const c of l.def) expect(c.dugFor).toBeCloseTo(M - W.perAttack + W.regrow + 1, 6);
  });

  it('a jab by a weak force only scratches a line, and guns in the attack wear it faster', () => {
    const g = playing('GER');
    const { target, def, att } = line(g, 4, 1);
    works(def, M);
    att[0].str = 0.5;
    const jab = assess(g, att, target);
    expect(jab.odds).toBeLessThan(W.fullAt);
    expect(jab.wear).toBeCloseTo(W.perAttack * (jab.odds / W.fullAt), 6);
    expect(jab.wear).toBeLessThan(W.perAttack);
    // Guns: by the share they take off the fieldworks.
    const fresh = line(g, 2, 3);
    works(fresh.def, M);
    const guns = addCorps(g, 'GER', 'Test guns', 'art', 2, fresh.from.id, {});
    guns.supplied = true;
    const plain = assess(g, fresh.att, fresh.target);
    const withGuns = assess(g, [...fresh.att, guns], fresh.target);
    const soften = Math.min(GUNS.softenMost, GUNS.soften * guns.divs * guns.str);
    expect(plain.wear).toBeCloseTo(W.perAttack, 6);
    expect(withGuns.wear).toBeCloseTo(W.perAttack * (1 + W.guns * soften), 6);
    // Never more than there is.
    works(fresh.def, 0.4);
    expect(assess(g, [...fresh.att, guns], fresh.target).wear).toBeCloseTo(0.4, 6);
  });

  it('fieldworks worn under a month give less cover, and battered flat give none', () => {
    const g = playing('GER');
    const { target, def, att } = line(g);
    const open = assess(g, att, target);
    works(def, 1);
    expect(assess(g, att, target).odds).toBeCloseTo(open.odds / DUG_IN, 6);
    works(def, 0.5);
    const half = assess(g, att, target);
    expect(half.odds).toBeCloseTo(open.odds / (1 + (DUG_IN - 1) * 0.5), 6);
    expect(half.prepared).toBeCloseTo(0.25, 6);
    works(def, 0);
    const flat = assess(g, att, target);
    expect(flat.odds).toBeCloseTo(open.odds, 6);
    expect(flat.prepared).toBe(0);
    expect(flat.wear).toBe(0);
    expect(flat.lines.find((x) => x.label === 'Dug in')!.value).toContain('battered flat');
  });

  it('none of it in a historical scenario', () => {
    const g = createGame(3, 'GER');
    const target = g.byName['Lodz'];
    const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
    const def = [addCorps(g, 'POL', 'Line 1', 'inf', 3, target.id, {})];
    const att = [addCorps(g, 'GER', 'Storm 1', 'inf', 3, from.id, {}), addCorps(g, 'GER', 'Storm 2', 'inf', 3, from.id, {})];
    def[0].dug = true;
    const a = assess(g, att, target);
    expect(a.works).toBe(0);
    expect(a.wear).toBe(0);
    expect(a.prepared).toBe(0);
    expect(a.lines.some((x) => x.label === 'Troops')).toBe(false);
    expect(a.lines.find((x) => x.label === 'Dug in')!.value).toContain('have had a month to dig in');
    resolveBattle(g, att, target);
    for (const c of def) expect(c.pressed).toBeUndefined();
    expect(Object.keys(g.flags).some((k) => k.startsWith('opening:'))).toBe(false);
  });
});

describe('a new offensive catches a line off balance', () => {
  const P = DEFENCE.prepared;
  it('for its first month, and after three quiet months it is a new offensive again', () => {
    const g = playing('GER');
    const { target, def, att } = line(g);
    works(def, M);
    const N = g.nations.GER;
    expect(N.blitz).toBeTruthy();
    expect(P.surpriseMonths).toBe(1);
    // An old offensive, fought last month: nothing.
    expect(assess(g, att, target).prepared).toBeCloseTo(1, 6);
    // Begun this month.
    N.offStart.POL = g.turn; N.lastFight.POL = g.turn;
    const caught = assess(g, att, target);
    expect(caught.prepared).toBeCloseTo(1 - P.surprise, 6);
    expect(caught.lines.find((x) => x.label === 'Dug in')!.value).toContain('the new offensive has caught the line off balance');
    // Begun last month: the surprise is over.
    N.offStart.POL = g.turn - 1; N.lastFight.POL = g.turn - 1;
    expect(assess(g, att, target).prepared).toBeCloseTo(1, 6);
    // Three quiet months: the next blow opens a new one.
    N.offStart.POL = g.turn - 8; N.lastFight.POL = g.turn - 3;
    expect(assess(g, att, target).prepared).toBeCloseTo(1 - P.surprise, 6);
    N.lastFight.POL = g.turn - 2;
    expect(assess(g, att, target).prepared).toBeCloseTo(1, 6);
  });

  it('for three months in the first offensive an army ever makes on a country', () => {
    const g = playing('GER');
    const l0 = line(g);
    works(l0.def, M);
    const N = g.nations.GER;
    // Never fought them: the first blow is the start of it.
    delete N.lastFight.POL; delete N.offStart.POL;
    expect(assess(g, l0.att, l0.target).prepared).toBeCloseTo(1 - P.surprise, 6);
    resolveBattle(g, l0.att, l0.target);
    expect(g.flags['opening:GER:POL']).toBe(g.turn);
    expect(N.offStart.POL).toBe(g.turn);
    // Month by month from there.
    expect(P.openingMonths).toBeGreaterThan(P.surpriseMonths);
    const h = playing('GER');
    const l = line(h);
    works(l.def, M);
    for (let age = 0; age <= P.openingMonths; age++) {
      h.nations.GER.offStart.POL = h.turn - age; h.nations.GER.lastFight.POL = h.turn; h.flags['opening:GER:POL'] = h.turn - age;
      expect(assess(h, l.att, l.target).prepared).toBeCloseTo(age < P.openingMonths ? 1 - P.surprise : 1, 6);
    }
    // A later offensive on the same country is an ordinary one.
    h.nations.GER.offStart.POL = h.turn - 1; h.nations.GER.lastFight.POL = h.turn; h.flags['opening:GER:POL'] = h.turn - 12;
    expect(assess(h, l.att, l.target).prepared).toBeCloseTo(1, 6);
  });

  it('only for an army trained to it', () => {
    const g = playing('GER');
    const { target, from, def } = line(g);
    // The same ground the other way: Poles attacking a German line.
    const guard = [addCorps(g, 'GER', 'Guard 1', 'inf', 3, from.id, {}), addCorps(g, 'GER', 'Guard 2', 'inf', 3, from.id, {})];
    for (const c of guard) c.supplied = true;
    works(guard, M);
    expect(g.nations.POL.blitz).toBeFalsy();
    delete g.nations.POL.lastFight.GER; delete g.nations.POL.offStart.GER;
    // A first blow, and nothing in it for an army that has not learned the method.
    const plain = assess(g, def, from).prepared;
    expect(plain).toBeGreaterThan(0);
    g.nations.POL.blitz = true;
    expect(assess(g, def, from).prepared).toBeCloseTo(plain * (1 - P.surprise), 6);
    expect(target.ctrl).toBe('POL');
  });
});

describe('room on the roads', () => {
  /** A Polish province with German ground on three sides or more, and test infantry on each of them. */
  function ringed(g: Game, perSide: number): { target: Region; sides: Region[]; att: Corps[] } {
    const target = g.regions.find((r) => r.ctrl === 'POL' && !r.cap && r.nb.filter((j) => g.regions[j].ctrl === 'GER').length >= 3)!;
    expect(target).toBeTruthy();
    const sides = target.nb.filter((j) => g.regions[j].ctrl === 'GER').map((j) => g.regions[j]).slice(0, 3);
    for (const j of target.nb) for (const u of [...g.regions[j].units]) if (u.nat === 'GER' && u.kind !== 'fort') relocate(g, u, g.byName['Berlin']);
    if (!target.units.some((u) => u.nat === 'POL')) addCorps(g, 'POL', 'Line 1', 'inf', 3, target.id, {});
    const att: Corps[] = [];
    for (const s of sides) for (let i = 0; i < perSide; i++) { const c = addCorps(g, 'GER', `Storm ${s.id}-${i}`, 'inf', 3, s.id, {}); c.supplied = true; att.push(c); }
    return { target, sides, att };
  }

  it('attacked from three sides, each side carries one more corps', () => {
    const g = playing('GER');
    const F = DEFENCE.frontage;
    expect(F.sides).toBe(3);
    const cap0 = 5;
    const { target, sides, att } = ringed(g, cap0 + 2);
    const cap = frontage(target);
    const per = (cs: Corps[], s: Region): number => cs.filter((c) => c.reg === s.id).length;
    const two = limitAttackers(g, att.filter((c) => c.reg !== sides[2].id), target);
    for (const s of sides.slice(0, 2)) expect(per(two, s)).toBe(cap);
    const three = limitAttackers(g, att, target);
    for (const s of sides) expect(per(three, s)).toBe(cap + F.extra);
  });

  it('against works two artillery corps go in over and above the roads, and against open ground they wait their turn', () => {
    const g = playing('GER');
    const F = DEFENCE.frontage;
    const { target, from, def } = line(g, 2, 0);
    const cap = frontage(target);
    const inf = Array.from({ length: cap + 1 }, (_, i) => addCorps(g, 'GER', `Storm ${i}`, 'inf', 3, from.id, {}));
    const guns = Array.from({ length: F.guns + 1 }, (_, i) => addCorps(g, 'GER', `Guns ${i}`, 'art', 2, from.id, {}));
    for (const c of [...inf, ...guns]) c.supplied = true;
    const kinds = (cs: Corps[]): string => cs.map((c) => c.kind).sort().join(',');
    // Nobody dug in, no fortress: the roads carry the strongest, and guns are the weakest corps there is.
    expect(target.fort).toBeFalsy();
    expect(kinds(limitAttackers(g, [...inf, ...guns], target))).toBe(Array(cap).fill('inf').join(','));
    // Dug in: the infantry fills the roads and two corps of guns fire over its head.
    works(def, 1);
    const going = limitAttackers(g, [...inf, ...guns], target);
    expect(going.filter((c) => c.kind === 'inf').length).toBe(cap);
    expect(going.filter((c) => c.kind === 'art').length).toBe(F.guns);
    // Guns support somebody: with no one else to go in they take the roads themselves.
    expect(limitAttackers(g, guns.slice(0, F.guns), target).length).toBe(F.guns);
    // The forecast and the battle count the same force.
    const look = attackOutlook(g, [...inf, ...guns], target)!;
    expect(look.lines.find((x) => x.label === 'Artillery')).toBeTruthy();
  });
});

describe('the Battle Ticket', () => {
  it('says how good the troops are, division for division', () => {
    const g = playing('GER');
    const { target, def, att } = line(g);
    for (const c of att) c.xp = 0.1;
    for (const c of def) c.xp = 0.1;
    const q = g.nations.GER.quality / g.nations.POL.quality;
    const even = assess(g, att, target).lines.find((x) => x.label === 'Troops');
    if (q < 1.1 && q > 1 / 1.1) expect(even).toBeUndefined();
    // Veterans in the line, green men in the attack.
    for (const c of def) c.xp = 0.9;
    for (const c of att) c.xp = 0;
    g.nations.GER.quality = g.nations.POL.quality;
    const row = assess(g, att, target).lines.find((x) => x.label === 'Troops')!;
    expect(row.value).toBe('Green attackers against veteran defenders');
    const k = (0.85 + 0.4 * 0.9) / 0.85;
    expect(row.effect).toContain(`each defending division counts for about ${k.toFixed(1)} attacking ones`);
    expect(row.effect).toContain('the more experienced troops');
    // And the other way about.
    for (const c of def) c.xp = 0;
    for (const c of att) c.xp = 0.9;
    expect(assess(g, att, target).lines.find((x) => x.label === 'Troops')!.effect).toContain(`each attacking division counts for about ${k.toFixed(1)} defending ones`);
  });

  it('has one line for the fieldworks: how far the works have got, the luck of the day, and what the attack takes off them', () => {
    const g = playing('GER');
    const { target, def, att } = line(g);
    expect(assess(g, att, target).lines.some((x) => x.label === 'Dug in')).toBe(false);
    works(def, 1);
    const young = assess(g, att, target);
    expect(young.lines.some((x) => x.label === 'Prepared defence')).toBe(false);
    const row = young.lines.find((x) => x.label === 'Dug in')!;
    expect(row.value).toBe(`The defenders are dug in: their works are a month old of the ${M} they take to finish`);
    expect(row.effect).toContain(`Troops in fieldworks defend ${Math.round((DUG_IN - 1) * 100)}% stronger`);
    expect(row.effect).toContain('The day runs against the attack');
    expect(row.effect).toContain('Won or lost, this attack takes a month off the works, which leaves nothing of them');
    works(def, M);
    const done = assess(g, att, target).lines.find((x) => x.label === 'Dug in')!;
    expect(done.value).toBe('The defenders are dug in: their works are finished');
    expect(done.effect).toContain('Won or lost, this attack takes a month off the works');
    expect(done.effect).not.toContain('leaves nothing');
    // Cut off, the line is ground like any other.
    for (const c of def) c.supplied = false;
    expect(assess(g, att, target).lines.find((x) => x.label === 'Dug in')!.effect).toContain('Cut off from supply');
  });
});

describe('terms go to whoever stands on the ground', () => {
  function italy(g: Game): { rome: Region; naples: Region } {
    declareWar(g, 'SOV', 'ITA');
    g.flags['event:mussolini-falls'] = g.turn - 2;
    const rome = g.byName['Rome'], naples = g.byName['Naples'];
    // The Italian army is away at the front: nobody stands in the field round either town.
    const away = g.regions.find((r) => r.ctrl === 'ITA' && r !== rome && r !== naples)!;
    for (const r of [rome, naples]) for (const u of [...r.units]) if (u.kind !== 'fort') relocate(g, u, away);
    return { rome, naples };
  }
  it('Italy gives in to the army on its soil, not to a Germany that holds none of it', () => {
    const g = free(5);
    const { rome, naples } = italy(g);
    for (const r of [rome, naples]) { const c = addCorps(g, 'SOV', `Front ${r.id}`, 'inf', 3, r.id, {}); if (c.town !== undefined) throw new Error('set down inside the town'); }
    expect(g.regions.some((r) => r.owner === 'ITA' && (r.ctrl === 'GER' || r.units.some((u) => u.nat === 'GER')))).toBe(false);
    const news = g.headlines.length;
    expect(fireEvent(g, 'italian-armistice')).toBe(true);
    expect(g.nations.ITA.alive).toBe(false);
    expect(g.nations.ITA.fellOn).toMatchObject({ to: 'SOV', how: 'terms' });
    expect(rome.ctrl).toBe('SOV');
    expect(naples.ctrl).toBe('SOV');
    expect(g.regions.filter((r) => r.owner === 'ITA').every((r) => r.ctrl !== 'GER' && r.ctrl !== 'ITA')).toBe(true);
    const told = g.headlines.slice(news).map((h) => h.text).join(' ');
    expect(told).toContain('the Soviet Union takes over what is left of the country');
    expect(told).not.toContain('German troops disarm');
  });

  it('with German troops in Rome the treaty runs as history wrote it, but an army keeps the province it stands on', () => {
    const g = free(5);
    const { rome, naples } = italy(g);
    addCorps(g, 'SOV', 'Front', 'inf', 3, naples.id, {});
    addCorps(g, 'GER', 'Garrison', 'inf', 3, rome.id, {});
    const news = g.headlines.length;
    expect(fireEvent(g, 'italian-armistice')).toBe(true);
    expect(g.nations.ITA.fellOn).toMatchObject({ to: 'GER', how: 'terms' });
    expect(rome.ctrl).toBe('GER');
    expect(naples.ctrl).toBe('SOV');
    expect(g.headlines.slice(news).map((h) => h.text).join(' ')).toContain('German troops disarm the Italian army');
  });
});

describe('a town never holds more corps than it has room for', () => {
  it('when its works are blown up the last corps in comes out into the country round it', () => {
    const g = free(5);
    const r = g.regions.find((x) => !!x.fort && x.ctrl === 'FRA' && !x.units.some((u) => u.nat !== 'FRA'))!;
    expect(r).toBeTruthy();
    for (const u of [...r.units]) if (u.kind !== 'fort') relocate(g, u, g.byName['Paris']);
    for (const c of [...garrisonOf(r)]) relocate(g, c, g.byName['Paris']);
    const room = roomIn(g, r);
    const inside = Array.from({ length: room }, (_, i) => { const c = addCorps(g, 'FRA', `Garrison ${i}`, 'inf', 2, r.id, {}); r.units.splice(r.units.indexOf(c), 1); joinTown(g, r, c); return c; });
    expect(garrisonOf(r).length).toBe(room);
    siegeSweep(g);
    expect(garrisonOf(r).length).toBe(room);
    // The fortress goes: the town holds fewer.
    r.fort = null;
    const less = roomIn(g, r);
    expect(less).toBeLessThan(room);
    siegeSweep(g);
    expect(garrisonOf(r).length).toBe(less);
    const out = inside.filter((c) => c.town === undefined);
    expect(out.length).toBe(room - less);
    for (const c of out) { expect(c.dead).toBeFalsy(); expect(g.regions[c.reg].units).toContain(c); }
  });
});

describe('ground a fallen power held in a dead country', () => {
  it('stays with the ally who stands on it, and only the empty rest goes to the victor', () => {
    const g = free(5);
    declareWar(g, 'GER', 'YUG');
    capitulate(g, 'YUG', 'GER');
    const yug = g.regions.filter((r) => r.owner === 'YUG' && r.ctrl === 'GER');
    expect(yug.length).toBeGreaterThan(1);
    if (!atWar(g, 'SOV', 'GER')) declareWar(g, 'SOV', 'GER');
    if (!atWar(g, 'SOV', 'ITA')) declareWar(g, 'SOV', 'ITA');
    const [held, empty] = yug;
    for (const r of [held, empty]) for (const u of [...r.units]) relocate(g, u, g.byName['Berlin']);
    const c = addCorps(g, 'ITA', 'Occupation corps', 'inf', 2, held.id, {});
    expect(held.units).toContain(c);
    capitulate(g, 'GER', 'SOV');
    expect(g.nations.GER.alive).toBe(false);
    expect(held.ctrl).toBe('ITA');
    expect(empty.ctrl).toBe('SOV');
    expect(c.dead).toBeFalsy();
    expect(c.reg).toBe(held.id);
  });
});
