// Close air support (Brian's ask of 2026-11-18, 18:58Z; air.json 'close'): a land fighter group on the standing order
// 'Ground support' flies over its own side's land battles within reach, adds to its side of the air edge on the
// battle card, and pays for each battle it flies in readiness and aircraft, with no dice. The grand campaign only,
// behind a switch: off, nothing changes. Since Brian's ask of 2026-11-21 ("you pick which fighter wings you have on air
// defense that go up and which ones you want to keep in reserve for ground air support", "add it to release 1") the
// player's choice is on in the data, and a staff's own use of it is behind a second switch (close.ai.on), which is off.
import { afterAll, afterEach, beforeAll, describe, expect, it } from 'vitest';
import { AIR, AIRWAR, CLOSE, airAI, airFleetsOf, airRange, assessAir, bookCloseAir, closeEdge, closeOf, closeRules, closeStaffRules, cover, fightersOver, kmTo } from '../src/campaign/air';
import { airAct, airPage } from '../src/campaign/airpages';
import { groupJobs, groupRow, setGroupJob } from '../src/campaign/airwar';
import { assess, createGame, resolveBattle } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, AirReport, Corps, Game, PlanPhase, Region } from '../src/campaign/types';

const was = { on: CLOSE.on, counts: CLOSE.counts, staffs: CLOSE.ai.on };
afterEach(() => { CLOSE.on = was.on; CLOSE.counts = was.counts; CLOSE.ai.on = was.staffs; });
const grand = (): Game => startOpen(createGame(7, 'GER'));
const fighters = (g: Game): AirFleet[] => airFleetsOf(g, 'GER').filter((f) => f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined);
/** A Polish province with German corps next to it, and those corps. */
function front(g: Game): { target: Region; attackers: Corps[] } {
  for (const r of g.regions) {
    if (r.ctrl !== 'POL' || !r.units.some((c) => c.nat === 'POL' && !c.dead)) continue;
    const attackers = r.nb.flatMap((j) => g.regions[j].units).filter((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && c.kind !== 'art');
    if (attackers.length >= 2) return { target: r, attackers };
  }
  throw new Error('no front');
}
/**
 * The sky over the battle fought for this month and not won by Germany: contested, so the air edge is not already at
 * its limit (over Poland in 1939 the Luftwaffe holds the sky by its numbers, and the edge is all but 1 before anyone is on call).
 */
function contest(g: Game, region: number): void {
  g.airReports.push({ id: g.airReports.length + 1, turn: g.turn, mission: 'superiority', region, zone: -1, place: g.regions[region].name, attNat: 'GER', defNat: 'POL', att: [], def: [], lines: [], odds: 1, level: 'Draw', sky: false } as unknown as AirReport);
}
/**
 * Every German land fighter group put on ground support, based beside the front: in 1939 a fighter reaches 200 km, and
 * the groups begin the war at their home fields, out of reach of the Polish border.
 */
function onCall(g: Game): AirFleet[] {
  const { attackers } = front(g);
  return fighters(g).filter((f) => { f.base = attackers[0].reg; return setGroupJob(g, f.id, 'close'); });
}

describe('close air support, switched off', () => {
  it('offers no order, and the battle card is what it was', () => {
    CLOSE.on = false;
    const g = grand();
    expect(closeRules(g)).toBe(false);
    const f = fighters(g)[0];
    expect(groupJobs(g, f.id).map((j) => j.key)).not.toContain('close');
    expect(setGroupJob(g, f.id, 'close')).toBe(false);
    expect(f.close).toBeUndefined();
    const { target, attackers } = front(g);
    const a = assess(g, attackers, target);
    expect(a.air).toBeUndefined();
    expect(a.lines.find((l) => l.label === 'Air')!.value).not.toMatch(/on call/);
    expect(airPage(g, 'airfield', f.base)?.note ?? '').not.toMatch(/ground support/);
  });
});

describe('close air support, switched on (the grand campaign)', () => {
  it('gives a land fighter group the order, which takes it off patrol and off the raids it would meet', () => {
    CLOSE.on = true;
    const g = grand();
    const f = fighters(g)[0];
    const over = g.regions.find((r) => r.ctrl === 'GER' && fightersOver(g, r.id, 'POL') > 0 && fightersOver(g, r.id, 'POL') >= f.ftr * 0.99)!;
    const before = fightersOver(g, over.id, 'POL');
    expect(groupJobs(g, f.id).find((j) => j.key === 'close')!.label).toBe('Ground support');
    expect(setGroupJob(g, f.id, 'close')).toBe(true);
    expect(f.close).toBe(true);
    expect(f.escorts).toBeUndefined();
    const jobs = groupJobs(g, f.id);
    expect(jobs.find((j) => j.key === 'close')!.on).toBe(true);
    expect(jobs.find((j) => j.key === 'patrol')!.on).toBe(false);
    expect(fightersOver(g, over.id, 'POL')).toBeLessThan(before);
    // Back on patrol, or resting, it is off ground support.
    expect(setGroupJob(g, f.id, 'patrol')).toBe(true);
    expect(f.close).toBeUndefined();
    setGroupJob(g, f.id, 'close');
    expect(setGroupJob(g, f.id, 'rest')).toBe(true);
    expect(f.close).toBeUndefined();
  });

  it('adds the groups in reach to the attack on the battle card, and says how many air strikes are on call', () => {
    const g = grand();
    const { target, attackers } = front(g);
    contest(g, target.id);
    CLOSE.on = true;
    const plain = assess(g, attackers, target);
    expect(plain.air).toBeUndefined();
    expect(onCall(g).length).toBeGreaterThan(0);
    const a = assess(g, attackers, target);
    expect(a.air).toBeDefined();
    expect(a.air!.att.groups.length).toBeGreaterThan(0);
    expect(a.air!.att.W).toBeGreaterThan(0);
    expect(a.A).toBeGreaterThan(plain.A);
    expect(a.air!.att.share).toBeGreaterThan(0);
    expect(a.air!.att.strikes).toBeGreaterThanOrEqual(CLOSE.strikes[0]);
    expect(a.air!.att.strikes).toBeLessThanOrEqual(CLOSE.strikes[1]);
    expect(a.air!.edge).toBeLessThanOrEqual(1);
    expect(a.lines.find((l) => l.label === 'Air')!.value).toMatch(/air strikes? on call for the attack/);
    // The on-call term is at most 'most' on the edge, at 'full' aircraft.
    expect(closeEdge(CLOSE.full * 10)).toBeCloseTo(CLOSE.most, 9);
    expect(closeEdge(CLOSE.full / 2)).toBeCloseTo(CLOSE.most / 2, 9);
  });

  it('charges the groups that flew for the battle, once, with no dice, and they fly no other battle that week', () => {
    const g = grand();
    const { target, attackers } = front(g);
    contest(g, target.id);
    CLOSE.on = true;
    onCall(g);
    const a = assess(g, attackers, target);
    const side = a.air!.att;
    expect(side.strikes).toBeGreaterThan(0);
    const before = new Map(side.groups.map((k) => [k.id, { ftr: g.airFleets.find((f) => f.id === k.id)!.ftr, ready: g.airFleets.find((f) => f.id === k.id)!.ready }]));
    const dice = g.rng.state;
    const twin = grand();
    contest(twin, target.id);
    onCall(twin);
    const rep = resolveBattle(g, attackers, target);
    expect(rep.air?.att).toBe(side.strikes);
    for (const k of side.groups) {
      const f = g.airFleets.find((x) => x.id === k.id)!, b = before.get(k.id)!;
      expect(f.ftr).toBe(b.ftr - Math.min(b.ftr, k.lose * side.strikes));
      expect(f.ready).toBeCloseTo(Math.max(AIR.ready.floor, b.ready - CLOSE.ready), 9);
      expect(f.closeAt).toBe(g.turn * 10 + g.week);
    }
    // The land war's dice are thrown as they would be with nobody on call.
    const { target: t2, attackers: a2 } = front(twin);
    CLOSE.on = false;
    resolveBattle(twin, a2, t2);
    CLOSE.on = true;
    expect(g.rng.state).toBe(twin.rng.state);
    expect(dice).not.toBe(g.rng.state);
    // Done for the week: nobody is on call over the next battle.
    const again = closeOf(g, new Set(['GER']), new Set(['POL']), target.id);
    expect(again.groups.filter((k) => side.groups.some((s) => s.id === k.id))).toEqual([]);
  });

  it('loses twice the aircraft under the enemy\'s sky, and books the field\'s own count of strikes where it is given', () => {
    const g = grand();
    const { target } = front(g);
    CLOSE.on = true;
    onCall(g);
    const ger = new Set(['GER']), pol = new Set(['POL']);
    const ours = closeOf(g, ger, pol, target.id, false), under = closeOf(g, ger, pol, target.id, true);
    for (let i = 0; i < ours.groups.length; i++) expect(under.groups[i].lose).toBe(Math.min(g.airFleets.find((f) => f.id === ours.groups[i].id)!.ftr, Math.round(ours.groups[i].planes * CLOSE.loss * CLOSE.enemySky)) || under.groups[i].lose);
    const k = ours.groups.find((x) => x.lose > 0) ?? ours.groups[0];
    const f = g.airFleets.find((x) => x.id === k.id)!;
    const ftr = f.ftr;
    bookCloseAir(g, { edge: 0, att: { ...ours, strikes: 1 }, def: { W: 0, share: 0, strikes: 0, groups: [] } }, { [k.id]: 3 });
    expect(f.ftr).toBe(ftr - Math.min(ftr, k.lose * 3));
    // A group the field did not fly pays nothing.
    for (const o of ours.groups.filter((x) => x.id !== k.id)) expect(g.airFleets.find((x) => x.id === o.id)!.closeAt).toBeUndefined();
  });

  it('never puts a computer\'s group on ground support, and never a carrier wing or a bomber group', () => {
    CLOSE.on = true;
    const g = grand();
    for (const f of airFleetsOf(g, 'POL')) expect(setGroupJob(g, f.id, 'close')).toBe(false);
    for (const f of airFleetsOf(g, 'GER').filter((x) => x.kind === 'bmr')) expect(groupJobs(g, f.id).map((j) => j.key)).not.toContain('close');
    expect(g.airFleets.some((f) => f.close && f.nat !== 'GER')).toBe(false);
  });
});

describe('close air support under a sky already won (the design director\'s b536)', () => {
  it('is not swallowed by the clamp: it carries the edge past 1 and still makes its air strikes', () => {
    CLOSE.on = true;
    const g = grand();
    const { target, attackers } = front(g);
    const plain = assess(g, attackers, target);
    onCall(g);
    const a = assess(g, attackers, target);
    // Over Poland in September 1939 the Luftwaffe holds the sky by its numbers: the edge was all but 1 already.
    expect(a.air!.edge).toBeGreaterThan(1);
    expect(a.air!.edge).toBeLessThanOrEqual(1 + CLOSE.most);
    expect(a.A).toBeGreaterThan(plain.A * 1.005);
    expect(a.air!.att.share).toBeGreaterThanOrEqual(CLOSE.counts);
    expect(a.air!.att.strikes).toBeGreaterThan(0);
    // The words say who holds the sky from the air war alone.
    expect(a.lines.find((l) => l.label === 'Air')!.value).toMatch(/^The attacker's air force holds the sky here by its numbers; /);
  });

  it('is worth about x1.05 on the attack and x1.03 on the defence at a full term, under the grand campaign\'s sky row', () => {
    expect(1 + 0.135 * closeEdge(CLOSE.full)).toBeCloseTo(1.05, 2);
    expect(1 / (1 - 0.09 * closeEdge(CLOSE.full))).toBeCloseTo(1.034, 2);
  });
});

describe('a staff\'s ground support (b536)', () => {
  const attack = (g: Game): PlanPhase => ({ from: g.turn, to: g.turn, posture: 'attack', enemies: ['POL'], objectives: ['Warsaw', 'Lodz', 'Krakow', 'Poznan'] });
  it('puts the land fighters that reach its offensive on ground support, nearest first, up to its share, and leaves the rest on patrol', () => {
    CLOSE.on = true;
    CLOSE.ai.on = true;
    const g = startOpen(createGame(7, null));
    const ph = attack(g);
    const land = airFleetsOf(g, 'GER').filter((f) => f.kind === 'ftr' && f.ftr > 0 && f.ship === undefined);
    // (In September 1939 the staff bases its fighters against the French and British bombers in the west, out of
    // reach of the Polish provinces: two groups are put on the Polish border first, as moved there this month.)
    const border = g.byName.Poznan.nb.find((j) => g.regions[j].ctrl === 'GER')!;
    for (const f of land.slice(0, 2)) { f.base = border; f.moved = g.turn; }
    airAI(g, 'GER', ph);
    const on = land.filter((f) => f.close);
    expect(on.length).toBeGreaterThan(0);
    expect(on.reduce((s, f) => s + f.ftr, 0)).toBeLessThanOrEqual(CLOSE.ai.share * land.reduce((s, f) => s + f.ftr, 0) + 1e-9);
    expect(land.some((f) => !f.close)).toBe(true);
    const battles = ph.objectives!.map((n) => g.byName[n].id);
    for (const f of on) expect(battles.some((b) => cover(kmTo(g, f, b), airRange(f, g).ftr) > 0)).toBe(true);
    // Decided afresh each month: on the defensive, every group patrols.
    airAI(g, 'GER', { ...ph, posture: 'defend' });
    expect(land.some((f) => f.close)).toBe(false);
  });

  it('leaves every group as it was with the switch off', () => {
    CLOSE.on = false;
    const g = startOpen(createGame(7, null)), h = startOpen(createGame(7, null));
    airAI(g, 'GER', attack(g));
    CLOSE.on = true;
    CLOSE.on = false;
    airAI(h, 'GER', attack(h));
    expect(g.airFleets.some((f) => f.close)).toBe(false);
    expect(JSON.stringify(g.airFleets)).toBe(JSON.stringify(h.airFleets));
  });
});

describe('the player picks, the staffs do not yet (Brian\'s ask of 2026-11-21; air.json as it ships)', () => {
  // (Release 1.5's staff doctrine, with the fight back off (airwar.json ai.back): the fight back has its own tests, air-fight-back.test.ts.)
  beforeAll(() => { AIRWAR.ai.back!.on = false; });
  afterAll(() => { AIRWAR.ai.back!.on = true; });
  const attack = (g: Game): PlanPhase => ({ from: g.turn, to: g.turn, posture: 'attack', enemies: ['POL'], objectives: ['Warsaw', 'Lodz', 'Krakow', 'Poznan'] });
  it('ships with the player\'s choice on and the staffs\' own switch off', () => {
    expect(CLOSE.on).toBe(true);
    expect(CLOSE.ai.on).toBe(false);
    const g = grand();
    expect(closeRules(g)).toBe(true);
    expect(closeStaffRules(g)).toBe(false);
  });

  it('offers a land fighter group both duties, one of them lit: Patrol the sky or Ground support', () => {
    const g = grand();
    const f = fighters(g)[0];
    const jobs = groupJobs(g, f.id);
    expect(jobs.map((j) => j.key)).toEqual(expect.arrayContaining(['patrol', 'close', 'rest']));
    expect(jobs.filter((j) => j.on).map((j) => j.key)).toEqual(['patrol']);
    expect(setGroupJob(g, f.id, 'close')).toBe(true);
    expect(groupJobs(g, f.id).filter((j) => j.on).map((j) => j.key)).toEqual(['close']);
    // Its row on the airfield's page says so: it is not on patrol, and it meets no raid.
    expect(groupRow(g, f).doing).toMatch(/^On ground support/);
    expect(groupRow(g, f).doing).not.toMatch(/patrol/i);
    // (The words as the UI director ruled them, 21 November 06:11Z: ui-director/words-wing-duty.md.)
    expect(groupRow(g, f).doing).toBe("On ground support: on call over its own side's battles within its reach, and meets no raid");
    expect(airPage(g, 'airfield', f.base)!.note).toContain('A fighter group patrols its sky, sticks with a bomber group as escort, flies ground support for the armies, or rests; any group can fly one raid a month, planned from its target.');
    expect(groupJobs(g, f.id).find((j) => j.key === 'close')!.forecast).toMatch(/one battle a week, and only while its readiness is 50% or more\. Each battle costs it 10% readiness and some of its aircraft, twice as many where the enemy holds the sky\. While it does this it is off air defence: it meets no raid and escorts no bomber\.$/);
    expect(airPage(g, `group:${f.id}`)!.sections[0].rows.map((r) => r.title)).toContain('Ground support');
    expect(airAct(g, `job:${f.id}:patrol`)?.ok).toBe(true);
    expect(groupRow(g, f).doing).toMatch(/^On patrol/);
    expect(airAct(g, `job:${f.id}:close`)?.ok).toBe(true);
    expect(setGroupJob(g, f.id, 'patrol')).toBe(true);
    expect(f.close).toBeUndefined();
    expect(groupJobs(g, f.id).filter((j) => j.on).map((j) => j.key)).toEqual(['patrol']);
  });

  it('sends up against a raid only the groups left on air defence', () => {
    const g = grand();
    const pol = airFleetsOf(g, 'POL').find((f) => f.bmr > 0)!;
    // A German province the Polish bombers reach, and every German land fighter group based in it.
    const target = g.regions.find((r) => r.ctrl === 'GER' && cover(kmTo(g, pol, r.id), airRange(pol, g).bmr) > 0)!;
    const ours = fighters(g);
    for (const f of ours) f.base = target.id;
    const up = (): number[] => assessAir(g, [pol], 'bomb', target.id).def.map((x) => x.f.id).sort((a, b) => a - b);
    const all = up();
    expect(all).toEqual(ours.map((f) => f.id).sort((a, b) => a - b));
    // Half of them held for the armies: they stay down, and the other half go up.
    const held = ours.filter((_, i) => i % 2 === 0);
    for (const f of held) expect(setGroupJob(g, f.id, 'close')).toBe(true);
    expect(up()).toEqual(ours.filter((f) => !held.includes(f)).map((f) => f.id).sort((a, b) => a - b));
    // Put back on air defence, they go up again.
    for (const f of held) expect(setGroupJob(g, f.id, 'patrol')).toBe(true);
    expect(up()).toEqual(all);
  });

  it('leaves a war fought by staffs alone as it was: no staff puts a group on ground support', () => {
    const g = startOpen(createGame(7, null)), h = startOpen(createGame(7, null));
    const border = g.byName.Poznan.nb.find((j) => g.regions[j].ctrl === 'GER')!;
    for (const x of [g, h]) for (const f of airFleetsOf(x, 'GER').filter((y) => y.kind === 'ftr' && y.ftr > 0 && y.ship === undefined).slice(0, 2)) { f.base = border; f.moved = x.turn; }
    airAI(g, 'GER', attack(g));
    CLOSE.on = false;
    airAI(h, 'GER', attack(h));
    expect(g.airFleets.some((f) => f.close)).toBe(false);
    expect(JSON.stringify(g.airFleets)).toBe(JSON.stringify(h.airFleets));
  });
});
