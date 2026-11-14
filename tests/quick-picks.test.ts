// Version 64: four quick picks about the line and the men (the grand campaign; none of it in the historical
// scenarios, and none of it used by a computer staff yet).
//
// 1. Hand over the fieldworks. Works belong to the ground: a dug-in corps that marches off leaves them behind, a
//    corps of its side that marches in (or already stands there without works) has them at once and keeps them at
//    the month's end, empty works lose a month's work every month, and the enemy finds nothing he can use.
// 2. Fold two worn corps into one. The men of both fill the one that is kept, the better general commands it, and
//    the one that is emptied goes home as a cadre, to be raised again from the arsenal at half the price.
// 3. Probe the line. One corps feels out the army in the province beside it: every corps there is identified and
//    its works are a month the worse, for a few men and half the corps' month. It is not a battle.
// 4. Bomb the line. An air mission that takes months off the fieldworks of an army that is dug in.
import { describe, expect, it } from 'vitest';
import { AIRWAR, lineWear } from '../src/campaign/air';
import { offers, orderOffer } from '../src/campaign/arsenal';
import { generalOf, seat } from '../src/campaign/commanders';
import { cadreTerms, cadresOf, cancelBuild, corpsCost, deliver } from '../src/campaign/economy';
import { foldChoices, foldCorps, foldView, whyNotFold } from '../src/campaign/fold';
import {
  DEFENCE, addCorps, capture, createGame, endPhase, groundWorks, moveCorps, probeCost, probeLine, railMove, relocate, removeCorps, resolveBattle, startPhase, whyNoProbe, worksOf,
} from '../src/campaign/game';
import { corpsKnown, setFog } from '../src/campaign/intel';
import { MANPOWER, menLeft } from '../src/campaign/manpower';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { diaryText } from '../src/campaign/stories';
import {
  attackOutlook, beginHuman, fieldworksLeft, foldChoicesAt, humanUp, orderFold, orderProbe, orderRaid, previewRaid, raidMenu, raidTargetsFor, softenOptions, stepAI,
} from '../src/campaign/turn';
import { MEN_PER_DIV, type Corps, type Game, type Region } from '../src/campaign/types';

/** A grand campaign with the player's first month open, as the game's own screen would have it. */
function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const M = DEFENCE.entrench.months;
const copy = (g: Game): Game => loadGame(saveGame(g))!;
const fresh = (c: Corps): Corps => { c.supplied = true; c.ap = c.apMax; c.fought = false; return c; };
const works = (cs: Corps[], months: number): void => { for (const c of cs) { c.dug = true; c.dugFor = months; } };

/** Two German provinces side by side, far from any enemy, with nobody standing in them. */
function rear(g: Game): { a: Region; b: Region } {
  const a = g.byName['Berlin'];
  const b = g.regions[a.nb.find((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER' && !g.regions[j].nb.some((k) => g.regions[k].ctrl !== 'GER'))!];
  for (const r of [a, b]) for (const u of [...r.units]) if (u.kind !== 'fort') removeCorps(g, u);
  return { a, b };
}

describe('hand over the fieldworks', () => {
  it('a dug-in corps that marches off leaves its works on the ground', () => {
    const g = playing('GER');
    const { a, b } = rear(g);
    const old = fresh(addCorps(g, 'GER', 'Old Line', 'inf', 3, a.id, {}));
    works([old], M);
    expect(groundWorks(g, a.id)).toBeNull();
    expect(moveCorps(g, old, b)).toBe(true);
    expect(old.dug).toBeUndefined();
    expect(groundWorks(g, a.id)).toEqual({ months: M, corps: 1, nat: 'GER' });
    expect(fieldworksLeft(g, a.id)).toEqual({ months: M, corps: 1, nat: 'GER' });
    // The record is saved with the campaign.
    expect(groundWorks(copy(g), a.id)).toEqual({ months: M, corps: 1, nat: 'GER' });
  });

  it('a corps that marches in has them at once, and keeps them at the month\'s end though it marched', () => {
    const g = playing('GER');
    const { a, b } = rear(g);
    const old = fresh(addCorps(g, 'GER', 'Old Line', 'inf', 3, a.id, {}));
    const relief = fresh(addCorps(g, 'GER', 'Relief', 'inf', 3, b.id, {}));
    works([old], 2);
    moveCorps(g, old, b);
    expect(moveCorps(g, relief, a)).toBe(true);
    expect(relief.dug).toBe(true);
    expect(worksOf(relief)).toBe(2);
    expect(groundWorks(g, a.id)).toBeNull();
    // The month ends: the corps that left has nothing, and the relief stands in what it found, no more and no less.
    endPhase(g, 'GER');
    expect(old.dug).toBeUndefined();
    expect(relief.dug).toBe(true);
    expect(worksOf(relief)).toBe(2);
    expect(relief.relief).toBeUndefined();
    // And a quiet month after that it digs on like anyone else.
    fresh(relief);
    endPhase(g, 'GER');
    expect(worksOf(relief)).toBe(3);
  });

  it('a relief already standing there without works takes them the moment the old corps leaves', () => {
    const g = playing('GER');
    const { a, b } = rear(g);
    const old = fresh(addCorps(g, 'GER', 'Old Line', 'inf', 3, a.id, {}));
    const relief = fresh(addCorps(g, 'GER', 'Relief', 'inf', 3, b.id, {}));
    works([old], M);
    moveCorps(g, relief, a);
    expect(relief.dug).toBeUndefined(); // the works are manned: there is nothing lying empty to take
    moveCorps(g, old, b);
    expect(relief.dug).toBe(true);
    expect(worksOf(relief)).toBe(M);
    expect(groundWorks(g, a.id)).toBeNull();
    endPhase(g, 'GER');
    expect(worksOf(relief)).toBe(M);
  });

  it('a corps that comes by rail takes them over too, and one that then attacks loses them like any attacker', () => {
    const g = playing('GER');
    const { a, b } = rear(g);
    const old = fresh(addCorps(g, 'GER', 'Old Line', 'inf', 3, a.id, {}));
    works([old], M);
    moveCorps(g, old, b);
    const far = g.regions.find((r) => r.ctrl === 'GER' && r.owner === 'GER' && r.id !== a.id && r.id !== b.id && !r.nb.includes(a.id))!;
    const relief = fresh(addCorps(g, 'GER', 'Relief', 'inf', 3, far.id, {}));
    if (railMove(g, relief, a)) {
      expect(relief.dug).toBe(true);
      expect(worksOf(relief)).toBe(M);
      relief.fought = true;
      endPhase(g, 'GER');
      expect(relief.dug).toBeUndefined();
    }
  });

  it('works standing empty lose a month of work with each month, and are gone when nothing is left', () => {
    const g = playing('GER');
    const { a, b } = rear(g);
    const old = fresh(addCorps(g, 'GER', 'Old Line', 'inf', 3, a.id, {}));
    works([old], 2);
    moveCorps(g, old, b);
    startPhase(g, 'POL');
    expect(groundWorks(g, a.id)?.months).toBe(2); // another country's month is not theirs
    startPhase(g, 'GER');
    expect(groundWorks(g, a.id)?.months).toBe(1);
    startPhase(g, 'GER');
    expect(groundWorks(g, a.id)).toBeNull();
  });

  it('two corps\' works wait for two reliefs, and troops put down by any other means take nothing', () => {
    const g = playing('GER');
    const { a, b } = rear(g);
    const one = fresh(addCorps(g, 'GER', 'Old One', 'inf', 3, a.id, {})), two = fresh(addCorps(g, 'GER', 'Old Two', 'inf', 3, a.id, {}));
    works([one], 3); works([two], 1);
    moveCorps(g, one, b); moveCorps(g, two, b);
    expect(groundWorks(g, a.id)).toEqual({ months: 2, corps: 2, nat: 'GER' });
    // Beaten troops, or troops set down by an order from the capital, do not retire into a ready line.
    const beaten = fresh(addCorps(g, 'GER', 'Beaten', 'inf', 3, b.id, {}));
    relocate(g, beaten, a);
    expect(beaten.dug).toBeUndefined();
    expect(groundWorks(g, a.id)?.corps).toBe(2);
    const r1 = fresh(addCorps(g, 'GER', 'Relief One', 'inf', 3, b.id, {}));
    moveCorps(g, r1, a);
    expect(worksOf(r1)).toBe(2);
    expect(groundWorks(g, a.id)).toEqual({ months: 2, corps: 1, nat: 'GER' });
  });

  it('the enemy finds nothing he can use: works go with the ground', () => {
    const g = playing('GER');
    const { a, b } = rear(g);
    const old = fresh(addCorps(g, 'GER', 'Old Line', 'inf', 3, a.id, {}));
    works([old], M);
    moveCorps(g, old, b);
    capture(g, a, 'POL');
    expect(groundWorks(g, a.id)).toBeNull();
  });

  it('a corps that goes forward after a battle won leaves its works behind it', () => {
    const g = playing('GER');
    const target = g.byName['Lodz'];
    const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
    for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
    for (const u of [...from.units]) if (u.nat === 'GER' && u.kind !== 'fort') relocate(g, u, g.byName['Berlin']);
    const weak = addCorps(g, 'POL', 'Thin Line', 'inf', 1, target.id, {});
    weak.str = 0.3; weak.supplied = true;
    const att = [0, 1, 2].map((i) => fresh(addCorps(g, 'GER', `Storm ${i + 1}`, 'inf', 3, from.id, {})));
    works(att, M);
    const rep = resolveBattle(g, att, target);
    expect(rep.taken).toBe(true);
    const went = att.filter((c) => c.reg === target.id).length;
    expect(went).toBeGreaterThan(0);
    const stayed = att.filter((c) => c.reg === from.id && !c.dug).length;
    // Whoever went forward left works: handed to a corps that stayed without any, or lying on the ground.
    expect((groundWorks(g, from.id)?.corps ?? 0) + att.filter((c) => c.reg === from.id && c.relief).length).toBe(Math.min(went, went + stayed));
  });

  it('the historical scenarios keep the old rule: works are lost when a corps leaves', () => {
    const g = createGame(3, 'GER');
    const a = g.byName['Berlin'];
    const b = g.regions[a.nb.find((j) => g.regions[j].ctrl === 'GER')!];
    for (const u of [...b.units]) if (u.kind !== 'fort') removeCorps(g, u);
    const old = fresh(addCorps(g, 'GER', 'Old Line', 'inf', 3, a.id, {}));
    works([old], M);
    if (moveCorps(g, old, b)) expect(groundWorks(g, a.id)).toBeNull();
  });
});

describe('fold two worn corps into one', () => {
  /** Two worn German infantry corps standing together at Berlin, the first with a general. */
  function pair(sa = 0.5, sb = 0.4): { g: Game; a: Corps; b: Corps; at: Region } {
    const g = playing('GER');
    const at = g.byName['Berlin'];
    const a = fresh(addCorps(g, 'GER', 'First Worn', 'inf', 3, at.id, { xp: 0.6 })), b = fresh(addCorps(g, 'GER', 'Second Worn', 'inf', 3, at.id, { xp: 0.2 }));
    a.str = sa; b.str = sb;
    return { g, a, b, at };
  }

  it('is offered only for two worn corps of the same kind standing together in supply', () => {
    const { g, a, b, at } = pair();
    expect(whyNotFold(g, a, b)).toBeNull();
    expect(foldChoices(g, at.id, 'GER').filter((v) => [a.id, b.id].includes(v.keep) && [a.id, b.id].includes(v.gone)).length).toBe(2);
    expect(foldChoicesAt(g, at.id).some((v) => v.keep === a.id && v.gone === b.id)).toBe(true);
    b.str = MANPOWER.fold.worn;
    expect(whyNotFold(g, a, b)).toMatch(/under 70%/);
    b.str = 0.4;
    const tanks = fresh(addCorps(g, 'GER', 'Worn Tanks', 'arm', 2, at.id, {}));
    tanks.str = 0.3;
    expect(whyNotFold(g, a, tanks)).toMatch(/same kind/);
    b.supplied = false;
    expect(whyNotFold(g, a, b)).toMatch(/Cut off/);
    b.supplied = true;
    relocate(g, b, g.regions[at.nb[0]]);
    expect(whyNotFold(g, a, b)).toMatch(/same province/);
    // And there is no such order in a historical scenario.
    const h = createGame(3, 'GER');
    const x = addCorps(h, 'GER', 'X', 'inf', 3, 'Berlin', {}), y = addCorps(h, 'GER', 'Y', 'inf', 3, 'Berlin', {});
    x.str = 0.3; y.str = 0.3;
    expect(whyNotFold(h, x, y)).toMatch(/grand campaign/);
    expect(foldCorps(h, x, y)).toBe(false);
  });

  it('the men of both fill the one that is kept, and what it knows is the average of the men now in it', () => {
    const { g, a, b, at } = pair(0.5, 0.4);
    const pool = menLeft(g, 'GER');
    const v = foldView(g, a, b);
    expect(v.why).toBeNull();
    expect(v.str).toBeCloseTo(0.9, 6);
    expect(v.back).toBe(0);
    expect(v.xp).toBeCloseTo((0.6 * 1.5 + 0.2 * 1.2) / 2.7, 6);
    const org = (a.org * 1.5 + b.org * 1.2) / 2.7 - MANPOWER.fold.org;
    b.ap = 1;
    expect(orderFold(g, a.id, b.id)).toBe(true);
    expect(a.str).toBeCloseTo(0.9, 6);
    expect(a.xp).toBeCloseTo(v.xp, 6);
    expect(a.org).toBeCloseTo(org, 6);
    expect(a.ap).toBe(1); // the fewer moves of the two
    expect(menLeft(g, 'GER')).toBeCloseTo(pool, 6);
    expect(g.corps.includes(b)).toBe(false);
    expect(at.units.includes(b)).toBe(false);
    expect(cadresOf(g, 'GER').map((c) => c.id)).toEqual([b.id]);
    expect(diaryText(g, a, a.log![a.log!.length - 1])).toBe(`Took in the men of Second Worn at ${at.name}.`);
    expect(diaryText(g, b, b.log![b.log!.length - 1])).toBe(`Its men went to First Worn at ${at.name}. A cadre was kept to raise it again.`);
    expect(g.headlines[g.headlines.length - 1].text).toMatch(/Second Worn is folded into First Worn/);
  });

  it('men over go back to the reserve', () => {
    const { g, a, b } = pair(0.65, 0.6);
    const pool = menLeft(g, 'GER');
    const v = foldView(g, a, b);
    expect(v.str).toBe(1);
    expect(v.back).toBe(Math.round(0.25 * 3 * MEN_PER_DIV));
    expect(foldCorps(g, a, b)).toBe(true);
    expect(a.str).toBe(1);
    expect(menLeft(g, 'GER')).toBeCloseTo(pool + 0.75, 6);
  });

  it('the better general commands, and the other waits for a corps', () => {
    const { g, a, b } = pair();
    const junior = seat(g, a, 'Ernst Probe'), senior = seat(g, b, 'Otto Besser');
    junior.level = 1; senior.level = 3;
    const v = foldView(g, a, b);
    expect(v.general).toBe('Otto Besser');
    expect(v.stepsDown).toBe('Ernst Probe');
    foldCorps(g, a, b);
    expect(generalOf(g, a)?.name).toBe('Otto Besser');
    expect(senior.corps).toBe(a.id);
    expect(junior.corps).toBe(-1);
    // The other way round the sitting general stays, and the emptied corps' general waits.
    const p = pair();
    const keepGen = seat(p.g, p.a, 'Karl Oben'), goneGen = seat(p.g, p.b, 'Hans Unten');
    keepGen.level = 4; goneGen.level = 2;
    foldCorps(p.g, p.a, p.b);
    expect(generalOf(p.g, p.a)?.name).toBe('Karl Oben');
    expect(goneGen.corps).toBe(-1);
  });

  it('the kept corps takes over the better fieldworks of the two', () => {
    const { g, a, b } = pair();
    works([b], M);
    expect(foldView(g, a, b).works).toBe(M);
    foldCorps(g, a, b);
    expect(worksOf(a)).toBe(M);
  });

  it('the cadre is raised again from the arsenal at half price: the same corps, with new men', () => {
    const { g, a, b, at } = pair();
    b.nick = 'the Old Guard';
    foldCorps(g, a, b);
    const n = g.nations.GER;
    n.pp = 500;
    const no = n.corpsNo, pool = menLeft(g, 'GER');
    const offer = offers(g, 'GER', at.id).find((o) => o.key === `cadre:${b.id}`)!;
    expect(offer).toBeTruthy();
    expect(offer.group).toBe('army');
    expect(offer.kind).toBe('inf');
    expect(offer.why).toBeNull();
    expect(offer.cost).toBeCloseTo(Math.round(corpsCost(g, 'GER', 'inf') * MANPOWER.fold.price * 10) / 10, 6);
    expect(cadreTerms(g, 'GER', b).cost).toBe(offer.cost);
    const build = orderOffer(g, 'GER', offer.key, at.id)!;
    expect(build.cadre).toBe(b.id);
    expect(build.name).toBe('Second Worn');
    expect(n.pp).toBeCloseTo(500 - offer.cost, 6);
    expect(menLeft(g, 'GER')).toBeCloseTo(pool - 3, 6);
    expect(n.corpsNo).toBe(no);
    // While it is forming it is not offered twice, and the order can be taken back in its own month.
    expect(offers(g, 'GER', at.id).some((o) => o.key === `cadre:${b.id}`)).toBe(false);
    expect(cancelBuild(g, 'GER', build.id)).toBe(true);
    expect(n.pp).toBeCloseTo(500, 6);
    expect(menLeft(g, 'GER')).toBeCloseTo(pool, 6);
    expect(n.corpsNo).toBe(no);
    expect(cadresOf(g, 'GER').length).toBe(1);
    // Ordered again and saved: the cadre and its order come back with the campaign.
    const again = orderOffer(g, 'GER', offer.key, at.id)!;
    const h = copy(g);
    expect(cadresOf(h, 'GER').map((c) => c.name)).toEqual(['Second Worn']);
    h.turn = again.eta;
    const xp = b.xp;
    deliver(h, 'GER');
    const back = h.corps.find((c) => c.id === b.id)!;
    expect(back).toBeTruthy();
    expect(back.name).toBe('Second Worn');
    expect(back.nick).toBe('the Old Guard');
    expect(back.dead).toBe(false);
    expect(back.str).toBe(1);
    expect(back.xp).toBeGreaterThanOrEqual(xp * MANPOWER.fold.xp - 1e-9);
    expect(h.regions[back.reg].units.includes(back)).toBe(true);
    expect(cadresOf(h, 'GER').length).toBe(0);
    expect(diaryText(h, back, back.log![back.log!.length - 1])).toMatch(/^Raised again round its cadre at /);
  });
});

/** A Polish line at Lodz and German corps beside it, as the Version 63 tests build one. */
function line(g: Game, defenders = 2, attackers = 2): { target: Region; from: Region; def: Corps[]; att: Corps[] } {
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  for (const u of [...from.units]) if (u.nat === 'GER' && u.kind !== 'fort') relocate(g, u, g.byName['Berlin']);
  const def = Array.from({ length: defenders }, (_, i) => fresh(addCorps(g, 'POL', `Line ${i + 1}`, 'inf', 3, target.id, {})));
  const att = Array.from({ length: attackers }, (_, i) => fresh(addCorps(g, 'GER', `Storm ${i + 1}`, 'inf', 3, from.id, {})));
  return { target, from, def, att };
}

describe('probe the line', () => {
  it('names every corps there and takes a month off their works, for a few men and half the corps\' month', () => {
    const g = playing('GER');
    setFog(g, true);
    const l = line(g, 2, 2);
    works(l.def, M);
    const c = l.att[0];
    const before = { str: c.str, org: c.org, ap: c.ap, men: l.def.map((d) => d.str), reports: g.reports.length, last: g.nations.GER.lastFight.POL, ctrl: l.target.ctrl };
    expect(whyNoProbe(g, c, l.target)).toBeNull();
    const cost = probeCost(g, c);
    const rep = orderProbe(g, c, l.target)!;
    expect(rep).toBeTruthy();
    expect(rep.found).toEqual(['Line 1', 'Line 2']);
    expect(rep.wear).toBe(DEFENCE.probe!.wear);
    expect(rep.works).toBe(M - DEFENCE.probe!.wear);
    expect(rep.lost).toBe(cost);
    for (const d of l.def) {
      expect(corpsKnown(g, d, 'GER').level).toBe('exact');
      expect(worksOf(d)).toBe(M - DEFENCE.probe!.wear);
      expect(d.pressed).toBe(true);
    }
    // The defenders lose no men, the prober a few, some of its order and its moves; it has fought.
    expect(l.def.map((d) => d.str)).toEqual(before.men);
    expect(c.str).toBeLessThan(before.str);
    expect(c.str).toBeGreaterThan(before.str * 0.9);
    expect(c.org).toBeCloseTo(before.org - DEFENCE.probe!.org, 6);
    expect(c.ap).toBe(before.ap - DEFENCE.probe!.ap);
    expect(c.fought).toBe(true);
    // It is not a battle: nothing is filed, no offensive is opened, nothing changes hands.
    expect(g.reports.length).toBe(before.reports);
    expect(g.nations.GER.lastFight.POL).toBe(before.last);
    expect(l.target.ctrl).toBe(before.ctrl);
    expect(g.headlines[g.headlines.length - 1].text).toMatch(/Storm 1 probes the line at Lodz and finds Line 1 and Line 2\. Their fieldworks are a month the worse/);
  });

  it('a line is probed once a month, by a fighting corps with the moves for it', () => {
    const g = playing('GER');
    const l = line(g, 2, 2);
    works(l.def, M);
    const guns = fresh(addCorps(g, 'GER', 'The Guns', 'art', 1, l.from.id, {}));
    expect(whyNoProbe(g, guns, l.target)).toMatch(/fighting corps/);
    l.att[1].ap = 1;
    expect(whyNoProbe(g, l.att[1], l.target)).toMatch(/takes 2 of a corps' moves/);
    const far = fresh(addCorps(g, 'GER', 'Far Away', 'inf', 3, 'Berlin', {}));
    expect(whyNoProbe(g, far, l.target)).toMatch(/cannot reach/);
    expect(orderProbe(g, l.att[0], l.target)).toBeTruthy();
    fresh(l.att[1]);
    expect(whyNoProbe(g, l.att[1], l.target)).toMatch(/probed this month already/);
    expect(orderProbe(g, l.att[1], l.target)).toBeNull();
    g.turn++;
    expect(whyNoProbe(g, l.att[1], l.target)).toBeNull();
  });

  it('the attack card offers it, with the corps the staff can best spare', () => {
    const g = playing('GER');
    setFog(g, true);
    const l = line(g, 2, 2);
    works(l.def, 2);
    l.att[1].str = 0.6;
    const s = softenOptions(g, l.att, l.target)!;
    expect(s.works).toBe(2);
    expect(s.probe.corps).toBe(l.att[1].id);
    expect(s.probe.why).toBeNull();
    expect(s.probe.wear).toBe(1);
    expect(s.probe.ap).toBe(DEFENCE.probe!.ap);
    expect(s.probe.men).toBe(probeCost(g, l.att[1]));
    // With the defenders named the staff gives one figure, not a band.
    const dark = attackOutlook(g, l.att, l.target)!;
    orderProbe(g, l.att[1], l.target);
    const lit = attackOutlook(g, l.att, l.target)!;
    expect(lit.exact).toBe(true);
    if (!dark.exact) expect(dark.note).toMatch(/A probe by one corps/);
    expect(softenOptions(g, l.att, l.target)!.probe.why).toMatch(/probed this month already/);
  });

  it('is not an order in a historical scenario', () => {
    const g = createGame(3, 'GER');
    const target = g.byName['Lodz'];
    const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
    const c = fresh(addCorps(g, 'GER', 'Storm', 'inf', 3, from.id, {}));
    expect(whyNoProbe(g, c, target)).toMatch(/grand campaign/);
    expect(probeLine(g, c, target)).toBeNull();
  });
});

describe('bomb the line', () => {
  const foes = (g: Game, r: Region): Corps[] => r.units.filter((u) => g.nations.GER.atWar.includes(u.nat));
  const flat = (g: Game): void => { for (const c of g.corps) if (c.nat !== 'GER') { delete c.dug; delete c.dugFor; } };
  /** A Polish army the German bombers can reach, dug in for `months`. */
  function dugTarget(g: Game, months: number): Region {
    const t = raidTargetsFor(g, 'ground').find((x) => previewRaid(g, 'ground', x.id).bombers > 0)!;
    const r = g.regions[t.id];
    works(foes(g, r), months);
    return r;
  }

  it('is on the planner\'s menu, against armies that are dug in and nowhere else', () => {
    const g = playing('GER');
    expect(raidMenu(g).map((k) => k.key)).toContain('line');
    flat(g);
    expect(raidTargetsFor(g, 'line').length).toBe(0);
    const r = dugTarget(g, M);
    expect(raidTargetsFor(g, 'line').map((t) => t.id)).toEqual([r.id]);
    expect(raidTargetsFor(g, 'line')[0].what).toMatch(/dug in, in works of 3 months, finished/);
  });

  it('takes months off the works of the troops there, and leaves the men alone', () => {
    const g = playing('GER');
    flat(g);
    const r = dugTarget(g, M);
    const poles = foes(g, r);
    const str = poles.map((c) => c.str), org = poles.map((c) => c.org);
    const plan = previewRaid(g, 'line', r.id);
    expect(plan.why).toBeNull();
    expect(plan.wear).toBeGreaterThan(0);
    expect(plan.wear).toBeLessThanOrEqual(AIRWAR.line.most);
    expect(plan.effects[0]).toMatch(/off the fieldworks at/);
    const soft = softenOptions(g, [], r)!;
    expect(soft.bomb?.why).toBeNull();
    expect(soft.bomb?.wear).toBeCloseTo(Math.min(plan.wear!, M), 6);
    const rep = orderRaid(g, 'line', r.id, plan.picked)!;
    expect(rep.mission).toBe('line');
    const left = Math.max(...poles.map(worksOf));
    expect(left).toBeLessThan(M);
    expect(left).toBeGreaterThanOrEqual(M - AIRWAR.line.most - 1e-9);
    expect(rep.result).toMatch(/off the fieldworks at|Too few bombers/);
    expect(poles.map((c) => c.str)).toEqual(str);
    expect(poles.map((c) => c.org)).toEqual(org);
    expect(poles.every((c) => !c.pressed)).toBe(true);
    // The groups that flew it fly nothing else this month.
    expect(previewRaid(g, 'line', r.id, plan.picked).why).not.toBeNull();
  });

  it('a hundred and twenty bombers through take a month off, and never more than two', () => {
    expect(lineWear(AIRWAR.line.scale)).toBe(1);
    expect(lineWear(AIRWAR.line.scale / 2)).toBe(0.5);
    expect(lineWear(10000)).toBe(AIRWAR.line.most);
    expect(lineWear(0)).toBe(0);
  });
});
