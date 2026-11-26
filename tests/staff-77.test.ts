// The staffs cross the sea where history planned it (Version 77; the design director's rulings 38 to 40 and 43).
//
// 1. History's shores. A staff with a list (open.json, 'descent.planned') opens a second front of its own only on
//    the shores history planned for it, and goes by ship only to fronts on the ground history took it to. The
//    United States: France and Italy, never Norway. Colonies and islands overseas are open always; a staff with no
//    list goes where it judges best, and so does every staff at the stop of the dial where nobody has read the books.
// 2. The build-up. Such a staff opens no front on a mainland until its army across the ocean is big enough to
//    follow the landing in ('with'). An island taken whole and a colony need no such army. And while that army is
//    still gathering, its war is not "a war nobody can fight": no armistice is signed until a year after it is ready.
// 3. A front is any harbour its side holds near the enemy ('descent.front'), not only a landing still in the
//    lander's hands; and a hop over a narrow sea into one has a ceiling of risk of its own (world.json 'hop').
// 4. A quay must have room: a staff abroad never keeps its landing force at a harbour its allies' corps fill.
// 5. The fleet and the air groups follow the army across the ocean (world.json 'fleet', 'air').
// 6. Canada ships its army, and Brazil its expeditionary corps from July 1944, each no bigger than history's.
// 7. The coast watch counts by the attacker's own army: a German coast is watched with the Soviet Union in the war.
// 8. The art director's twelve colours for the small republics, and where the Canal Zone's name is written.
import { describe, expect, it } from 'vitest';
import { landingWatch } from '../src/campaign/ai';
import { STRENGTH } from '../src/campaign/ashore';
import { AIR, homeCommand, km, whyNotBase } from '../src/campaign/air';
import { addCorps, allied, atWar, capture, createGame, removeCorps } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { NAVY, coastReport, crossingRisk, fleetsOf, seaPath } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { RIVALS, builtUp, frontAshore, joinsOn, landingForce, massing, oceanRuns, opensOn, seaBase, stalemates, startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import { STACK, type Corps, type Game, type Region } from '../src/campaign/types';
import { FAR_STAGE } from '../src/campaign/stage';
import { WORLD, beyondEurope, overseasOf } from '../src/campaign/world';
import open from '../data/campaign/open.json';

const DESCENT = (open as unknown as { descent: { front?: number; planned: Record<string, { open: string[]; join: string[]; with?: number }> } }).descent;
const JAN_42 = 29, JAN_43 = 41, MAY_44 = 57, JUL_44 = 59, SEP_44 = 61;
const wide = (seed = 1, human: string | null = null): Game => startOpen(createGame(seed, human));
const field = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort' && c.kind !== 'eng');
const across = (g: Game, nat: string): Corps[] => field(g, nat).filter((c) => !beyondEurope(g.regions[c.reg]));
const atHome = (g: Game, nat: string): Corps[] => field(g, nat).filter((c) => g.regions[c.reg].owner === nat && !g.regions[c.reg].overseas);

/** One war on the wider world with nobody playing, to September 1944: every landing a staff made, and month by month who stood across the ocean. */
const war: Record<number, string> = {};
const landings: { turn: number; nat: string; to: string; owner: string; island: boolean; overseas: boolean; wave: boolean; across: number }[] = [];
const months: { turn: number; can: number; canHome: number; bra: number; usa: number }[] = [];
{
  // Seed 6 until ruling 232: America's staff made a landing of its own in that war, on Sardinia in October 1942, and Germany holds
  // Norway. Seed 7 until Version 84, whose rules of what an attack brings and faces moved every war; seed 8 until ruling
  // 227, whose air war moved them again (in seed 8 Germany now never takes Norway). Of seeds 1 to 13 under 227, 3, 6, 7,
  // 9 and 12 tell this story in full. Seed 4 since ruling 232 (Warsaw's garrison, b566) and Italy's boats on the
  // record's seas (b564) moved the war again: of seeds 1 to 9 then, 4 and 5 tell it in full. Seed 5 since the fight back
  // (release 1.5.1, airwar.json ai.back) moved it once more: of seeds 3 to 6 then, 5 tells it in full.
  const g = wide(5);
  landingWatch.tell = (e) => { landings.push({ turn: g.turn, nat: e.nat, to: e.to.name, owner: e.to.owner, island: e.to.nb.length === 0, overseas: e.to.overseas, wave: e.wave, across: across(g, e.nat).length }); };
  while (g.turn < SEP_44) {
    runTurnAI(g);
    if ([JAN_42, JAN_43, JUL_44, SEP_44].includes(g.turn)) war[g.turn] = saveGame(g);
    months.push({ turn: g.turn, can: across(g, 'CAN').length, canHome: atHome(g, 'CAN').length, bra: across(g, 'BRA').length, usa: across(g, 'USA').length });
  }
  landingWatch.tell = null;
}
const at = (turn: number): Game => loadGame(war[turn])!;

describe('history\'s shores (rulings 39 and 40)', () => {
  it('the United States opens on France and Italy and joins a front in the west; Norway is on neither list', () => {
    const P = DESCENT.planned;
    expect(P.USA.open).toEqual(['FRA', 'ITA']);
    for (const id of ['FRA', 'ITA', 'BEL', 'NED', 'GER']) expect(P.USA.join).toContain(id);
    for (const id of ['NOR', 'SOV', 'GRE']) { expect(P.USA.open).not.toContain(id); expect(P.USA.join).not.toContain(id); }
    // Canada and Brazil open no front of their own on a mainland: they join their allies'.
    expect(P.CAN.open).toEqual([]);
    expect(P.BRA.open).toEqual([]);
    expect(P.BRA.join).toEqual(['ITA', 'FRA']);
    const g = wide();
    // (May 1944: history's dates for these shores, Version 79, are past. See shores-79.test.ts.)
    g.turn = MAY_44;
    const R = (name: string): Region => { const r = g.byName[name]; expect(r, name).toBeDefined(); return r; };
    expect(opensOn(g, 'USA', R('Brittany'))).toBe(true);
    expect(opensOn(g, 'USA', R('Sicily'))).toBe(true);
    expect(opensOn(g, 'USA', R('Bergen'))).toBe(false);
    expect(opensOn(g, 'USA', R('Holland'))).toBe(false);
    expect(joinsOn(g, 'USA', R('Holland'))).toBe(true);
    expect(joinsOn(g, 'USA', R('Bergen'))).toBe(false);
    // A colony overseas is open to it always (North Africa, where its first landing of the real war went in).
    expect(R('Tripoli').overseas).toBe(true);
    expect(opensOn(g, 'USA', R('Tripoli'))).toBe(true);
    // (To Canada too on the older chart. With the whole world on it a colony across an ocean is shut to her for good:
    // ruling 231, 3, and its rider b462. See shores-79.test.ts.)
    expect(joinsOn(g, 'CAN', R('Tripoli'))).toBe(!FAR_STAGE);
    expect(opensOn(g, 'CAN', R('Brittany'))).toBe(false);
    // A staff with no list goes where it judges best. (Britain's has one since Version 79.)
    for (const nat of ['GER', 'ITA', 'SOV']) for (const name of ['Bergen', 'Brittany', 'Holland']) { expect(opensOn(g, nat, R(name))).toBe(true); expect(joinsOn(g, nat, R(name))).toBe(true); }
    // And where nobody has read the books, so does America's.
    const bold = wide();
    expect(setStop(bold, 5, true)).toBe(true);
    expect(opensOn(bold, 'USA', bold.byName.Bergen)).toBe(true);
    expect(joinsOn(bold, 'USA', bold.byName.Bergen)).toBe(true);
  });

  it('in a war nobody plays America\'s own staff lands in France, in Italy or on a colony, and nowhere else', () => {
    const mine = landings.filter((l) => l.nat === 'USA' && !l.wave);
    expect(mine.length).toBeGreaterThan(0);
    for (const l of mine) expect(l.overseas || ['FRA', 'ITA'].includes(l.owner), `${l.to} (${l.owner})`).toBe(true);
    // Canada's and Brazil's make none on a mainland.
    expect(landings.filter((l) => (l.nat === 'CAN' || l.nat === 'BRA') && !l.island && !l.overseas)).toEqual([]);
  });
});

describe('the build-up', () => {
  it('no front is opened on a mainland until the army across the ocean can follow the landing in', () => {
    const need = DESCENT.planned.USA.with!;
    expect(need).toBeGreaterThanOrEqual(2 * landingForce());
    const g = at(JAN_42);
    expect(across(g, 'USA').length).toBeLessThan(need);
    expect(builtUp(g, 'USA', g.byName.Brittany)).toBe(false);
    // An island taken whole and a colony overseas need no such army.
    expect(g.byName.Corsica.nb).toEqual([]);
    expect(builtUp(g, 'USA', g.byName.Corsica)).toBe(true);
    expect(builtUp(g, 'USA', g.byName.Tripoli)).toBe(true);
    // A staff with no figure is not held by it.
    expect(builtUp(g, 'UK', g.byName.Brittany)).toBe(true);
    expect(builtUp(g, 'CAN', g.byName.Brittany)).toBe(true);
    // With the army gathered in Britain the shore is open; corps at home do not count toward it.
    const quay = g.byName.Cornwall;
    for (let k = across(g, 'USA').length; k < need - 1; k++) addCorps(g, 'USA', `Gathered Corps ${k}`, 'inf', 2, quay.id, {});
    addCorps(g, 'USA', 'Home Corps', 'inf', 2, 'Chicago', {});
    expect(builtUp(g, 'USA', g.byName.Brittany)).toBe(false);
    addCorps(g, 'USA', 'The last of them', 'inf', 2, quay.id, {});
    expect(builtUp(g, 'USA', g.byName.Brittany)).toBe(true);
  });

  it('every landing America\'s staff made on a mainland in that war had the army behind it', () => {
    const need = DESCENT.planned.USA.with!;
    for (const l of landings.filter((x) => x.nat === 'USA' && !x.wave && !x.island && !x.overseas)) expect(l.across, `${l.to}, month ${l.turn}`).toBeGreaterThanOrEqual(need);
  });

  it('an army still gathering across the sea is not a war nobody can fight: no armistice until a year after it is ready', () => {
    const need = DESCENT.planned.USA.with!, year = RIVALS.stalemate!.after;
    // America fights on its own account here, so that only its own war with Germany is weighed: no battle on land
    // between them, no ground in touch, and the war more than a year old.
    const quiet = (): Game => {
      const g = at(JAN_42);
      g.nations.USA.align = 'independent';
      g.nations.USA.lastFight = {};
      for (const e of g.nations.USA.atWar) { delete g.nations[e].lastFight.USA; g.flags[`war:USA:${e}`] = 1; g.flags[`war:${e}:USA`] = 1; }
      g.turn += year;
      return g;
    };
    const signs = (g: Game): boolean => stalemates(g).some(([a, b]) => a === 'GER' && b === 'USA');
    // With nobody across the ocean it is such a war, and it ends as it did before.
    const a = quiet();
    for (const c of across(a, 'USA')) removeCorps(a, c);
    expect(massing(a, 'USA')).toBe(false);
    expect(signs(a)).toBe(true);
    // With an army gathering in Britain it is not, however long the quiet has lasted.
    const g = quiet();
    if (!across(g, 'USA').length) addCorps(g, 'USA', 'The first across', 'inf', 2, g.byName.Cornwall.id, {});
    expect(massing(g, 'USA')).toBe(true);
    expect(signs(g)).toBe(false);
    g.turn += 2 * year;
    expect(signs(g)).toBe(false);
    expect(g.nations.USA.atWar).toContain('GER');
    // Once the army is ready the year is counted from that month.
    for (let k = across(g, 'USA').length; k < need; k++) addCorps(g, 'USA', `Gathered Corps ${k}`, 'inf', 2, g.byName.Cornwall.id, {});
    expect(massing(g, 'USA')).toBe(false);
    expect(signs(g)).toBe(false);
    g.turn += year - 1;
    expect(signs(g)).toBe(false);
    g.turn += 1;
    expect(signs(g)).toBe(true);
    expect(g.nations.USA.atWar).not.toContain('GER');
    // A staff with no such figure is never gathering. (Britain's waits for history's dates since Version 79, where they hold.)
    const loose = at(JAN_42);
    expect(setStop(loose, 4)).toBe(true);
    expect(massing(loose, 'UK')).toBe(false);
    expect(massing(at(JAN_42), 'CAN')).toBe(false);
  });
});

describe('a front is any harbour its side holds near the enemy', () => {
  it('a harbour an ally holds in France with the enemy a march away is one, a landing or not; one in Norway is not America\'s', () => {
    expect(DESCENT.front).toBe(2);
    const g = at(JAN_42);
    const hold = (name: string): Region => {
      const r = g.byName[name];
      for (const c of [...r.units]) removeCorps(g, c);
      capture(g, r, 'UK');
      expect(r.nb.some((j) => atWar(g, 'USA', g.regions[j].ctrl))).toBe(true);
      return r;
    };
    // No landing is on anybody's books: the harbour is simply in British hands, with the Germans next door.
    for (const k of Object.keys(g.flags)) if (k.startsWith('st:lodged:')) delete g.flags[k];
    const brest = hold('Brittany');
    const front = frontAshore(g, 'USA')!;
    expect(front).not.toBeNull();
    expect(front.ports).toContain(brest);
    expect(front.ground[brest.id]).toBe(1);
    expect(front.lodged).toBe(0);
    // Norway is not on America's list: a British harbour there is Britain's front and not America's.
    const bergen = hold('Bergen');
    expect(frontAshore(g, 'USA')!.ports).not.toContain(bergen);
    expect(frontAshore(g, 'UK')).not.toBeNull();
    // An ally who is not in America's war offers no front.
    const was = g.nations.UK.atWar;
    g.nations.UK.atWar = [];
    expect(frontAshore(g, 'USA')?.ports ?? []).not.toContain(brest);
    g.nations.UK.atWar = was;
    // A harbour with the enemy further off than the figure is a rear area, not a front.
    const keep = DESCENT.front;
    DESCENT.front = 0;
    try { expect(frontAshore(g, 'USA')?.ports ?? []).not.toContain(brest); } finally { DESCENT.front = keep; }
    delete DESCENT.front;
    try { expect(frontAshore(g, 'USA')).toBeNull(); } finally { DESCENT.front = keep; }
  });

  it('a corps waiting in Britain hops to it over a narrow sea at a danger no ocean crossing would be sent through', () => {
    const O = WORLD.overseas.USA;
    expect(O.hop!).toBeGreaterThan(O.risk);
    const hop = (ceiling: number): boolean => {
      const g = at(JAN_42);
      const beach = g.byName.Brittany;
      for (const c of [...beach.units]) removeCorps(g, c);
      capture(g, beach, 'UK');
      // (And the army next door sent away. Germany moves first in the month, and since ruling 229 five of its corps stand
      // at Rennes in this war: they would take the empty beach back before America's staff sat down to its orders.)
      for (const j of beach.nb) for (const c of [...g.regions[j].units]) if (c.nat !== 'UK') removeCorps(g, c);
      const quay = g.byName.Cornwall;
      const c = addCorps(g, 'USA', 'Waiting Corps', 'inf', 2, quay.id, {});
      c.supplied = true;
      const path = seaPath(g, 'USA', quay, beach)!;
      expect(path.length).toBeLessThanOrEqual(NAVY.landing.range);
      const danger = crossingRisk(g, 'USA', path);
      const was = { risk: O.risk, hop: O.hop };
      // An ocean crossing at this danger would not be made; the hop's own ceiling decides.
      O.risk = Math.min(O.risk, danger / 2);
      O.hop = ceiling < 0 ? 0 : danger + ceiling;
      // (A test of the hop's own ceiling: the test of strength of Version 83, which would keep one corps out of a
      // harbour with the German army of 1942 next door, is set aside. strength-83.test.ts is its own.)
      const asked = { ...STRENGTH! };
      Object.assign(STRENGTH!, { sure: 0, bold: 0, even: 0 });
      try { runTurnAI(g); } finally { O.risk = was.risk; O.hop = was.hop; Object.assign(STRENGTH!, asked); }
      return !c.dead && c.reg === beach.id;
    };
    expect(hop(0.01)).toBe(true);
    expect(hop(-1)).toBe(false);
  });
});

describe('a quay must have room', () => {
  it('a staff abroad never keeps its landing force at a harbour its allies\' corps fill', () => {
    const g = at(JAN_43);
    // (In January 1943 history's dates, Version 79, leave America's staff no mainland shore to open on: the quay is
    // weighed here at the stop of the dial where the list has no dates.)
    expect(setStop(g, 4)).toBe(true);
    const force = landingForce();
    const roomAt = (p: Region): number => STACK - p.units.filter((u) => !u.dead && u.nat !== 'USA' && allied(g, 'USA', u.nat)).length;
    // Americans enough in Britain for a landing force, standing idle.
    const cornwall = g.byName.Cornwall;
    for (let k = 0; k < force + 1; k++) addCorps(g, 'USA', `Idle Corps ${k}`, 'inf', 2, cornwall.id, {}).supplied = true;
    const first = seaBase(g, 'USA', 1);
    expect(first).not.toBeNull();
    expect(roomAt(first!)).toBeGreaterThanOrEqual(force);
    // British corps fill that harbour: three Americans can never stand there, and the staff moves its quay.
    for (let k = 0; roomAt(first!) >= force; k++) addCorps(g, 'UK', `Garrison Corps ${k}`, 'inf', 2, first!.id, {});
    const next = seaBase(g, 'USA', 1);
    expect(next).not.toBe(first);
    if (next) expect(roomAt(next)).toBeGreaterThanOrEqual(force);
  });
});

describe('the fleet and the air groups follow the army', () => {
  it('the Atlantic Fleet bases itself across the ocean once the first corps is over, and its carrier wings go with it', () => {
    const O = WORLD.overseas.USA;
    expect(O.fleet).toEqual(['Atlantic Fleet']);
    expect(O.air).toBe(0);
    const early = wide(7);
    const home = fleetsOf(early, 'USA').find((f) => f.name === 'Atlantic Fleet')!;
    expect(beyondEurope(home.base)).toBe(true);
    const g = at(JAN_43);
    expect(g.flags['fy:first:USA']).toBe(1);
    const f = fleetsOf(g, 'USA').find((x) => x.name === 'Atlantic Fleet')!;
    expect(f.base).toBeGreaterThanOrEqual(0);
    expect(beyondEurope(f.base)).toBe(false);
    expect(allied(g, 'USA', g.regions[f.base].ctrl)).toBe(true);
    for (const w of g.airFleets.filter((a) => a.ship === f.id)) expect(w.base).toBe(f.base);
    expect(g.headlines.some((h) => /^The Atlantic Fleet crosses the ocean to /.test(h.text))).toBe(true);
    // No land-based group raised for the war overseas is left at home beyond the figure. The home commands (Version
    // 84.2: the First Air Force and the rest) guard their own country and never cross.
    const land = g.airFleets.filter((a) => a.nat === 'USA' && a.ship === undefined);
    expect(land.filter((a) => !homeCommand(a) && !beyondEurope(a.base)).length).toBeGreaterThanOrEqual(4);
    expect(land.filter((a) => homeCommand(a)).length).toBeGreaterThanOrEqual(5);
    expect(land.filter((a) => homeCommand(a)).every((a) => beyondEurope(a.base))).toBe(true);
    // (Since rulings 68(b) and 70(a) a computer staff orders wings all through the war. One fresh from the factories
    // waits at home for its ship, a month or two and no longer: so the count is taken of those found at home today
    // that are still there two months on.)
    const waiting = land.filter((a) => !homeCommand(a) && beyondEurope(a.base)).map((a) => a.id);
    runTurnAI(g);
    runTurnAI(g);
    expect(g.airFleets.filter((a) => waiting.includes(a.id) && beyondEurope(a.base)).length).toBeLessThanOrEqual(O.air!);
  });

  it('an air group on an airfield at home is shipped across, however far, to a field of its side\'s near the army', () => {
    const g = at(JAN_43);
    const like = g.airFleets.find((a) => a.nat === 'UK' && a.ship === undefined)!;
    const home = g.byName.Chicago;
    const group = { ...like, id: 9977, nat: 'USA', name: 'Eighth Air Force', base: home.id, leader: null, moved: -1, lost: { ftr: 0, bmr: 0 } };
    g.airFleets.push(group);
    // Far beyond a month's flight: it goes in crates.
    const far = 'Too far to move in one month.';
    const field = g.regions.find((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.overseas && whyNotBase(g, group, r) === far)!;
    expect(field).toBeDefined();
    expect(km(g, home.id, field.id)).toBeGreaterThan(AIR.ferry);
    expect(whyNotBase(g, group, field, true)).toBeNull();
    runTurnAI(g);
    expect(beyondEurope(group.base)).toBe(false);
    expect(allied(g, 'USA', g.regions[group.base].ctrl)).toBe(true);
    // A player's own air groups are his to move, and his ferry range is what it was.
    const mine = at(JAN_43);
    mine.human = 'USA';
    const kept = { ...group, id: 9978, base: home.id, moved: -1 };
    mine.airFleets.push(kept);
    expect(whyNotBase(mine, kept, mine.regions[field.id])).toBe(far);
  });
});

describe('Canada\'s army and Brazil\'s expeditionary corps', () => {
  it('Canada ships its corps to Britain from the first winter, never more than history\'s, and its home commands stay', () => {
    const O = WORLD.overseas.CAN;
    expect(O.most).toBe(3);
    expect(O.guard).toBe(2);
    const first = months.find((m) => m.can > 0)!;
    expect(first).toBeDefined();
    expect(first.turn).toBeLessThanOrEqual(12);
    expect(Math.max(...months.map((m) => m.can))).toBeLessThanOrEqual(O.most!);
    expect(Math.min(...months.filter((m) => m.turn >= first.turn).map((m) => m.canHome))).toBeGreaterThanOrEqual(O.guard);
    const g = at(JAN_42);
    expect(overseasOf(g, 'CAN')).toBe(O);
    expect(oceanRuns(g, 'CAN').length).toBe(1);
  });

  it('Brazil sends one corps, from July 1944 and not before', () => {
    const O = WORLD.overseas.BRA;
    expect(O.from).toBe(JUL_44);
    expect(O.most).toBe(1);
    // At war from August 1942, with ships, and nobody sails.
    const g = at(JAN_43);
    expect(g.nations.BRA.atWar).toContain('GER');
    expect(overseasOf(g, 'BRA')).toBeNull();
    expect(months.filter((m) => m.turn < JUL_44 && m.bra > 0)).toEqual([]);
    expect(Math.max(...months.map((m) => m.bra))).toBeLessThanOrEqual(1);
    const late = at(JUL_44);
    if (late.nations.GER.alive || late.nations.ITA.alive) expect(overseasOf(late, 'BRA')).toBe(O);
    // A player's Brazil is his own to ship.
    late.human = 'BRA';
    expect(overseasOf(late, 'BRA')).toBeNull();
  });
});

describe('the coast watch counts by the attacker\'s own army (ruling 43)', () => {
  it('a German coast is watched with the Soviet Union in the war', () => {
    const g = at(JAN_43);
    expect(atWar(g, 'SOV', 'GER')).toBe(true);
    expect(g.regions.some((r) => r.ctrl === 'SOV' && r.nb.some((j) => g.regions[j].ctrl === 'GER'))).toBe(true);
    // British troops on a quay within a landing's reach of Germany's own coast, and ships to carry them.
    const coasts = g.regions.filter((r) => r.owner === 'GER' && r.ctrl === 'GER' && !r.overseas && r.seas.length > 0);
    const quay = g.regions.find((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.overseas && r.port && r.seas.length > 0 && coasts.some((c) => seaPath(g, 'UK', r, c, { max: NAVY.landing.range })))!;
    expect(quay).toBeDefined();
    for (const r of g.regions) if (r.port && allied(g, 'UK', r.ctrl)) for (const c of [...r.units]) if (c.nat === 'UK' && c.kind !== 'fort') removeCorps(g, c);
    delete g.flags['st:watch:GER'];
    coastReport(g, 'UK');
    expect(g.flags['st:watch:GER']).toBeUndefined();
    for (let k = 0; k < NAVY.watch!.corps; k++) addCorps(g, 'UK', `Quay Corps ${k}`, 'inf', 2, quay.id, {});
    g.nations.UK.sealift = Math.max(g.nations.UK.sealift, 3);
    coastReport(g, 'UK');
    expect(g.flags['st:watch:GER']).toBe(g.turn);
  });
});

describe('the small republics\' colours and the Canal Zone\'s name (the art director)', () => {
  it('each of the twelve has its own colour, and Panama is plum against the Canal Zone\'s olive', () => {
    const twelve: Record<string, string> = { GUA: '#9383c4', ELS: '#c98494', HON: '#9dbd8c', NIC: '#bf7f73', COS: '#7fae8c', PAN: '#a8607f', HAI: '#8fba7a', DOM: '#b08fb5', ECU: '#cf8f66', BOL: '#6fa58c', PAR: '#c4838f', URU: '#9d8cc4' };
    const g = wide();
    for (const [id, colour] of Object.entries(twelve)) expect(g.nations[id].color, id).toBe(colour);
    // No other country on the map wears any of them.
    for (const [id, colour] of Object.entries(twelve)) expect(Object.values(g.nations).filter((n) => n.color === colour).map((n) => n.id), id).toEqual([id]);
    expect(g.nations.PAN.color).not.toBe(g.nations.USA.color);
  });

  it('the Canal Zone\'s name is written west of its mark and Panama City\'s under its own', () => {
    expect(WORLD.labels).toEqual({ 'Canal Zone': 'west', 'Panama City': 'under' });
    const g = wide();
    for (const name of Object.keys(WORLD.labels!)) expect(g.byName[name], name).toBeDefined();
  });
});
