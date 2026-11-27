// Airfields as targets (Brian, 22 November 2026; the design director's ruling of 22 Nov 05:55Z on the design in
// air-battles/airfields/airfield-targets-design.md): one state per province's field, written by damageField only
// (raids, the sky raid's bombs, the spies, the plough-up) and mended by Repair; strips and concrete; the airfield raid;
// the staffs. Behind its own switch, off as shipped: off, the war is release 1's.
import { afterEach, describe, expect, it } from 'vitest';
import { AIR, airfieldsHold, fighterTech, fly, forecastAir, assessAir, whyNotBase, whyNotMission, fieldClass } from '../src/campaign/air';
import { raidKinds } from '../src/campaign/airwar';
import { fieldWrecked, setAirPiece, whyNotField, wreckField, wrecked } from '../src/campaign/airversion';
import { offers, deliverArsenal, isStripBuild, orderOffer, workLabel, worksUpkeep } from '../src/campaign/arsenal';
import {
  FIELD, FIELD_SEEN, aiFieldRepair, fieldKnown, fieldRepairView, damageField, fieldDamaged, fieldKey, fieldLost, fieldRepairCost, fieldRules, fieldShut, fieldTier, fieldView, fieldsMonth,
  orderFieldRepair, planFieldHit, undoFieldRepair, whyNotFieldRepair,
} from '../src/campaign/fieldstate';
import { allied, atWar, createGame } from '../src/campaign/game';
import { prizeMarks, prizeOut, wreckPrize } from '../src/campaign/prizes';
import { techMul, RESEARCH } from '../src/campaign/research';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { airfieldTargetsOn, airfieldTargetsShipped, setAirfieldTargets } from '../src/campaign/turn';
import type { AirFleet, Build, Game } from '../src/campaign/types';

/** A grand war nobody plays, with airfields as targets on (or off). */
const grand = (seed = 3, on = true): Game => { const g = startOpen(createGame(seed, null)); if (on) setAirfieldTargets(g, true); return g; };
/** The province of a nation's largest land group, and the groups based there. */
function mainField(g: Game, nat: string): { id: number; on: AirFleet[] } {
  const f = g.airFleets.filter((x) => x.nat === nat && x.ship === undefined && x.ftr + x.bmr > 0).sort((a, b) => b.ftr + b.bmr - (a.ftr + a.bmr))[0];
  return { id: f.base, on: g.airFleets.filter((x) => x.base === f.base && x.ship === undefined) };
}
const foeOf = (g: Game, nat: string) => (n: string): boolean => atWar(g, nat, n);
const shipped = FIELD.on;
afterEach(() => { FIELD.on = shipped; });

describe('the switch', () => {
  it('is on as shipped in release 2 (the gate held; the design director, 22 November 18:27Z), set only in a grand campaign, and kept with the save', () => {
    expect(airfieldTargetsShipped()).toBe(true);
    const off = grand(3, false);
    expect(fieldRules(off)).toBe(false);
    expect(airfieldTargetsOn(off)).toBe(false);
    const g = grand();
    expect(fieldRules(g)).toBe(true);
    expect(loadGame(saveGame(g))!.flags.fields).toBe(1);
    const hist = createGame(3, null);
    expect(setAirfieldTargets(hist, true)).toBe(false);
    expect(fieldRules(hist)).toBe(false);
  });

  it('off: nothing is written, nothing new is offered, and the old wreck reads as release 1', () => {
    const g = grand(3, false);
    const { id, on } = mainField(g, 'GER');
    expect(damageField(g, id, 'raid', { through: 500, foe: foeOf(g, 'UK') })).toBeNull();
    expect(Object.keys(g.flags).some((k) => /^(fsh|flv|fht|frp|frc|afc):/.test(k))).toBe(false);
    expect(raidKinds(g, 'UK').some((k) => k.key === 'airfield')).toBe(false);
    const works = offers(g, 'GER', id).filter((o) => o.group === 'works').map((o) => [o.key, o.label]);
    expect(works.some(([k]) => k === 'work:strip')).toBe(false);
    expect(works.find(([k]) => k === 'work:field')![1]).toBe('Airfields');
    // A burned level written by hand is ignored off: the room is release 1's.
    const room = airfieldsHold(g, id);
    g.flags[fieldKey('flv', id)] = 1;
    expect(airfieldsHold(g, id)).toBe(room);
    delete g.flags[fieldKey('flv', id)];
    // Release 1's wreck (ruling 151(b)): only with the field piece, only this month. (Release 2 begins new grand
    // campaigns with the field piece on, Brian's switch of 22 November for the spies; release 1's is off.)
    setAirPiece(g, 'field', false);
    g.flags[`wreck:${id}`] = g.turn;
    expect(fieldShut(g, id)).toBe(false);
    expect(wrecked(g, on[0])).toBe(false);
    setAirPiece(g, 'field', true);
    expect(fieldShut(g, id)).toBe(true);
    expect(wrecked(g, on[0])).toBe(true);
    expect(fieldWrecked(g, id)).toBe(true);
    g.flags[`wreck:${id}`] = g.turn - 1;
    expect(wrecked(g, on[0])).toBe(false);
  });
});

describe('a hit on a field (damageField, the one writer)', () => {
  it('shuts the runway only where the bombers through reach a tenth of the room', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    const room = airfieldsHold(g, id);
    const few = damageField(g, id, 'raid', { through: FIELD.shut * room - 1, foe: foeOf(g, 'UK') })!;
    expect(few.closed).toBe(false);
    expect(fieldShut(g, id)).toBe(false);
    expect(g.flags[fieldKey('fht', id)]).toBe(g.turn);
    const enough = damageField(g, id, 'raid', { through: FIELD.shut * room, foe: foeOf(g, 'UK') })!;
    expect(enough.closed).toBe(true);
    expect(fieldShut(g, id)).toBe(true);
    expect(g.flags[fieldKey('fsh', id)]).toBe(g.turn);
  });

  it('burns a level for every 150 through, two at most, never below one level of room', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    const built = airfieldsHold(g, id);
    expect(planFieldHit(g, id, 'raid', { through: 149, foe: foeOf(g, 'UK') })!.levels).toBe(0);
    expect(planFieldHit(g, id, 'raid', { through: 300, foe: foeOf(g, 'UK') })!.levels).toBe(Math.min(2, built / 500 - 1));
    const h = damageField(g, id, 'raid', { through: 2000, foe: foeOf(g, 'UK') })!;
    expect(h.levels).toBe(Math.min(FIELD.levels, built / 500 - 1));
    expect(airfieldsHold(g, id)).toBe(built - h.levels * 500);
    for (let k = 0; k < 6; k++) damageField(g, id, 'raid', { through: 2000, foe: foeOf(g, 'UK') });
    expect(airfieldsHold(g, id)).toBe(500);
    expect(fieldLost(g, id)).toBe(built / 500 - 1);
    // A burned level lowers the field; the tier stays.
    expect(fieldTier(g, id)).toBe('full');
  });

  it('destroys aircraft parked there at the record\'s rate: radar cuts it, surprise doubles it, at most 0.4 of those parked, bombers first', () => {
    const g = grand();
    const { id, on } = mainField(g, 'GER');
    const parked = on.reduce((s, f) => s + f.ftr + f.bmr, 0);
    const p = planFieldHit(g, id, 'raid', { through: 100, foe: foeOf(g, 'UK') })!;
    expect(p.ground).toBeCloseTo(Math.min(FIELD.mostGround * parked, 100 * FIELD.ground), -1);
    const radar = planFieldHit(g, id, 'raid', { through: 100, radar: 0.5, foe: foeOf(g, 'UK') })!;
    expect(radar.ground).toBeLessThan(p.ground);
    const surprise = planFieldHit(g, id, 'raid', { through: 100, surprise: true, foe: foeOf(g, 'UK') })!;
    expect(surprise.ground).toBeGreaterThan(p.ground);
    const huge = planFieldHit(g, id, 'raid', { through: 100000, foe: foeOf(g, 'UK') })!;
    expect(huge.ground).toBeLessThanOrEqual(Math.ceil(FIELD.mostGround * parked) + on.length);
    // The fighters that went up are not on the ground.
    const up = new Map(on.map((f) => [f.id, 1]));
    const flown = planFieldHit(g, id, 'raid', { through: 100, foe: foeOf(g, 'UK'), up })!;
    expect(flown.losses.reduce((s, x) => s + x.ftr, 0)).toBe(0);
    // Losses are booked whole: the groups and the pool.
    const before = on.reduce((s, f) => s + f.ftr + f.bmr, 0);
    const pilots = g.airPool.GER?.pilots ?? 0;
    const hit = damageField(g, id, 'raid', { through: 100, foe: foeOf(g, 'UK') })!;
    expect(on.reduce((s, f) => s + f.ftr + f.bmr, 0)).toBe(before - hit.ground);
    expect(g.airPool.GER.pilots).toBe(pilots + hit.losses.reduce((s, x) => s + x.ftr, 0));
  });

  it('grounds the groups there after the raid: no mission, no move in or out, not up against the next raid', () => {
    const g = grand();
    const { id, on } = mainField(g, 'GER');
    const f = on.find((x) => x.ftr > 0)!;
    expect(wrecked(g, f)).toBe(false);
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    expect(wrecked(g, f)).toBe(true);
    expect(whyNotField(g, f)).toMatch(/runway .* cratered until it is repaired/);
    expect(whyNotMission(g, f, 'superiority', id)).toMatch(/cratered/);
    const elsewhere = g.regions.find((r) => r.ctrl === 'GER' && r.id !== id && airfieldsHold(g, r.id) > 0 && !fieldShut(g, r.id))!;
    expect(whyNotBase(g, f, elsewhere)).toMatch(/^Grounded/);
    const other = g.airFleets.find((x) => x.nat === 'GER' && x.ship === undefined && x.base !== id)!;
    expect(whyNotBase(g, other, g.regions[id])).toMatch(/no aircraft can land/);
  });
});

describe('repair', () => {
  it('is priced from what a field costs to build, never under PR 19\'s least', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    expect(fieldRepairCost(g, id)).toBe(FIELD.price.runway.full + 2 * FIELD.price.level.full);
    expect(FIELD.price.derived).toBe(true);
    const strip = g.regions.find((r) => fieldClass(g, r.id) === 1 && r.ctrl === 'GER')!;
    damageField(g, strip.id, 'agent');
    expect(fieldTier(g, strip.id)).toBe('strip');
    expect(fieldRepairCost(g, strip.id)).toBe(Math.max(FIELD.price.least, FIELD.price.runway.strip));
  });

  it('opens the runway at the next month, brings a level back each month after, and goes off the books whole', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    const pp = g.nations.GER.pp = 100;
    const cost = fieldRepairCost(g, id);
    expect(orderFieldRepair(g, 'GER', id)).toBe(true);
    expect(g.nations.GER.pp).toBe(pp - cost);
    expect(whyNotFieldRepair(g, 'GER', id)).toMatch(/^Repair in hand/);
    fieldsMonth(g);
    expect(fieldShut(g, id)).toBe(true);
    g.turn++; fieldsMonth(g);
    expect(fieldShut(g, id)).toBe(false);
    expect(fieldLost(g, id)).toBe(2);
    g.turn++; fieldsMonth(g);
    expect(fieldLost(g, id)).toBe(1);
    g.turn++; fieldsMonth(g);
    expect(fieldLost(g, id)).toBe(0);
    expect(fieldDamaged(g, id)).toBe(false);
    expect(g.flags[fieldKey('frp', id)]).toBeUndefined();
  });

  it('is cancelled by a new hit, the unspent money lost, and the player may take it back in the month', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    g.nations.GER.pp = 100;
    orderFieldRepair(g, 'GER', id);
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    expect(g.flags[fieldKey('frp', id)]).toBeUndefined();
    expect(g.nations.GER.pp).toBeLessThan(100);
    // The take-back is the player's, in the month he ordered.
    g.human = 'GER';
    const left = g.nations.GER.pp;
    orderFieldRepair(g, 'GER', id);
    expect(undoFieldRepair(g, 'GER', id)).toBe(true);
    expect(g.nations.GER.pp).toBe(left);
    orderFieldRepair(g, 'GER', id);
    g.turn++;
    expect(undoFieldRepair(g, 'GER', id)).toBe(false);
  });

  it('is dropped when the province changes hands before the work is done', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    g.nations.GER.pp = 100;
    orderFieldRepair(g, 'GER', id);
    g.regions[id].since = g.turn;
    g.turn++; fieldsMonth(g);
    expect(fieldShut(g, id)).toBe(true);
    expect(g.flags[fieldKey('frp', id)]).toBeUndefined();
  });

  it('a staff repairs a field with its groups on it at once, leaves an empty one under raids, and keeps its reserve', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    const empty = g.regions.find((r) => r.ctrl === 'GER' && airfieldsHold(g, r.id) > 0 && !g.airFleets.some((f) => f.base === r.id && f.ship === undefined))!;
    damageField(g, empty.id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    g.nations.GER.pp = 0;
    expect(aiFieldRepair(g, 'GER', (n) => allied(g, 'GER', n))).toEqual([]);
    g.nations.GER.pp = 200;
    const done = aiFieldRepair(g, 'GER', (n) => allied(g, 'GER', n));
    expect(done).toContain(g.regions[id].name);
    expect(done).not.toContain(empty.name);
  });
});

describe('the spies and the plough-up write the same state', () => {
  it('an agent\'s wreck is the light hit: shut, nothing burned or destroyed, the crews have it open next month at no price', () => {
    const g = grand();
    const { id, on } = mainField(g, 'GER');
    const planes = on.reduce((s, f) => s + f.ftr + f.bmr, 0);
    const h = damageField(g, id, 'agent')!;
    expect(h.closed).toBe(true);
    expect(h.levels + h.ground).toBe(0);
    expect(on.reduce((s, f) => s + f.ftr + f.bmr, 0)).toBe(planes);
    expect(damageField(g, id, 'saboteurs')).toBeNull();
    g.turn++; fieldsMonth(g);
    expect(fieldShut(g, id)).toBe(false);
    // The release-1 call routes into the one state with the switch on.
    expect(wreckField(g, id, (n) => atWar(g, 'UK', n))).toBe(true);
    expect(fieldShut(g, id)).toBe(true);
  });

  it('the plough-up: by order shut and two levels, under scorched earth shut and one; prizeOut reads the field', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    const r = g.regions[id];
    expect(prizeOut(g, r, 'airfield')).toBe(0);
    expect(wreckPrize(g, r, 'airfield', 3, 'GER', 'plough')).toBe(true);
    expect(fieldShut(g, id)).toBe(true);
    expect(fieldLost(g, id)).toBe(Math.min(FIELD.plough.order, airfieldsHold(g, id) / 500 + fieldLost(g, id) - 1));
    expect(prizeOut(g, r, 'airfield')).toBe(99);
    g.nations.GER.pp = 100;
    orderFieldRepair(g, 'GER', id);
    expect(prizeOut(g, r, 'airfield')).toBe(1);
    const other = g.regions.find((x) => x.ctrl === 'FRA' && fieldClass(g, x.id) >= 2)!;
    expect(wreckPrize(g, other, 'airfield', 2, undefined, 'burn')).toBe(true);
    expect(fieldLost(g, other.id)).toBe(Math.min(FIELD.plough.burn, fieldLost(g, other.id)));
    expect(fieldShut(g, other.id)).toBe(true);
  });
});

describe('strips and concrete', () => {
  it('offers a field strip and a concrete airfield on the one counter of built levels', () => {
    const g = grand();
    const bare = g.regions.find((r) => r.ctrl === 'GER' && airfieldsHold(g, r.id) === 0)!;
    g.nations.GER.pp = 500;
    const os = offers(g, 'GER', bare.id).filter((o) => o.group === 'works');
    const strip = os.find((o) => o.key === 'work:strip')!, field = os.find((o) => o.key === 'work:field')!;
    expect(strip.label).toBe('Field strip');
    expect(strip.months).toBe(1);
    expect(field.label).toBe('Concrete airfield');
    expect(strip.why).toBeNull();
    const big = mainField(g, 'GER').id;
    expect(offers(g, 'GER', big).find((o) => o.key === 'work:strip')!.why).toMatch(/concrete airfield/);
    // A strip laid: a strip; a concrete level on top makes it a full field.
    // A strip is ordered as a level of 'field' marked 'strip': one counter, one cap (the city glance's ask).
    const o = orderOffer(g, 'GER', 'work:strip', bare.id)!;
    expect(o.item).toBe('field');
    expect(isStripBuild(o)).toBe(true);
    expect(offers(g, 'GER', bare.id).find((x) => x.key === 'work:field')!.size).toBe('level 2');
    expect(workLabel(g, 'field', 'strip')).toBe('Field strip');
    expect(workLabel(g, 'field')).toBe('Concrete airfield');
    expect(workLabel(grand(3, false), 'field')).toBe('Airfields');
    g.nations.GER.queue = g.nations.GER.queue.filter((x) => x !== o);
    const b = (item: string, strip = false): Build => ({ id: g.nextBuild++, what: 'work', item, name: '', region: bare.id, ordered: g.turn, eta: g.turn, cost: 0, ...(strip ? { kind: 'strip' as never } : {}) });
    deliverArsenal(g, 'GER', b('field', true));
    expect(fieldTier(g, bare.id)).toBe('strip');
    expect(airfieldsHold(g, bare.id)).toBe(500);
    expect(worksUpkeep(g, 'GER').find((l) => l.key === 'works:field')!.amount).toBeGreaterThan(0);
    deliverArsenal(g, 'GER', b('field'));
    expect(fieldTier(g, bare.id)).toBe('full');
    expect(airfieldsHold(g, bare.id)).toBe(1000);
  });

  it('bombers need concrete: none based on a strip, and a bomber group left on one flies no raid', () => {
    const g = grand();
    const strip = g.regions.find((r) => fieldClass(g, r.id) === 1 && r.ctrl === 'GER' && (g.flags[`wk:field:${r.id}`] ?? 0) === 0)!;
    const bmr = g.airFleets.find((f) => f.nat === 'GER' && f.kind === 'bmr' && f.ship === undefined)!;
    expect(whyNotBase(g, bmr, strip)).toMatch(/Bombers need a concrete airfield/);
    const ftr = g.airFleets.find((f) => f.nat === 'GER' && f.kind === 'ftr' && f.ship === undefined)!;
    expect(whyNotBase(g, ftr, strip) ?? '').not.toMatch(/concrete/);
    bmr.base = strip.id;
    const target = g.regions.find((r) => atWar(g, 'GER', r.ctrl))!;
    expect(whyNotMission(g, bmr, 'bomb', target.id)).toMatch(/Bombers need a concrete airfield to fly from/);
  });

  it('a fighter group with jets on a strip flies without the jets\' edge; on concrete the edge is whole', () => {
    const g = grand();
    const f = g.airFleets.find((x) => x.nat === 'GER' && x.kind === 'ftr' && x.ship === undefined)!;
    g.nations.GER.tech!.done.push('jets');
    const whole = fighterTech(g, f);
    expect(whole).toBe(techMul(g, 'GER', 'fighters'));
    const strip = g.regions.find((r) => fieldClass(g, r.id) === 1 && r.ctrl === 'GER' && (g.flags[`wk:field:${r.id}`] ?? 0) === 0)!;
    f.base = strip.id;
    expect(fighterTech(g, f)).toBeCloseTo(Math.max(1, whole - RESEARCH.fields.fighters.per));
    g.flags.fields = 0;
    expect(fighterTech(g, f)).toBe(whole);
  });
});

describe('the airfield raid and the sky raid', () => {
  /** A British bomber group (and the fighters with it) moved beside a German field, at war. */
  function raid(g: Game): { id: number; fleets: AirFleet[] } {
    const { id } = mainField(g, 'GER');
    const T = g.regions[id];
    const near = g.regions.filter((r) => r.id !== id && fieldTier(g, r.id) === 'full').sort((a, b) => Math.hypot(a.x - T.x, a.y - T.y) - Math.hypot(b.x - T.x, b.y - T.y))[0].id;
    const fleets = g.airFleets.filter((f) => f.nat === 'UK' && f.ship === undefined && f.bmr > 0).slice(0, 2);
    const esc = g.airFleets.find((f) => f.nat === 'UK' && f.ship === undefined && f.kind === 'ftr')!;
    for (const f of [...fleets, esc]) { f.base = near; f.ready = 1; f.bmr = f.kind === 'ftr' ? 0 : Math.max(f.bmr, 400); }
    return { id, fleets: [...fleets, esc] };
  }

  it('is on the planner\'s menu only with the switch, and its card forecasts the field', () => {
    const g = grand();
    expect(raidKinds(g, 'UK').some((k) => k.key === 'airfield')).toBe(true);
    const { id, fleets } = raid(g);
    expect(whyNotMission(g, fleets[0], 'airfield', id)).toBeNull();
    const a = assessAir(g, fleets, 'airfield', id);
    expect(a.why).toBeNull();
    const fc = forecastAir(g, a, 'airfield', id);
    expect(fc.field).toBeDefined();
    expect(fc.worth).toBeGreaterThan(0);
  });

  it('the based fighters go up first; the field is hit after, and the report says what it did', () => {
    const g = grand();
    const { id, fleets } = raid(g);
    const a = assessAir(g, fleets, 'airfield', id);
    expect(a.def.some((x) => x.f.base === id)).toBe(true);
    const rep = fly(g, 'UK', 'airfield', id, fleets)!;
    expect(rep.mission).toBe('airfield');
    expect(rep.field).toBeDefined();
    expect(rep.field!.region).toBe(id);
    if (rep.field!.shut) expect(fieldShut(g, id)).toBe(true);
    expect(g.flags[fieldKey('fht', id)]).toBe(g.turn);
  });

  it('the sky raid\'s bombers hit the field through damageField; off, release 1\'s rate on the ground is untouched', () => {
    const on = grand();
    const { id, fleets } = raid(on);
    const rep = fly(on, 'UK', 'superiority', id, fleets)!;
    expect(rep).not.toBeNull();
    expect(on.flags[fieldKey('fht', id)]).toBe(on.turn);
    const off = grand(3, false);
    const r2 = raid(off);
    fly(off, 'UK', 'superiority', r2.id, r2.fleets);
    expect(Object.keys(off.flags).some((k) => /^(fsh|flv|fht):/.test(k))).toBe(false);
    expect(AIR.damage.airfields.ground).toBeGreaterThan(0);
  });
});

describe('the card and the save', () => {
  it('a field view says tier, room, shut, burned, the repair and its price; the state keeps with the save', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    const v = fieldView(g, id, 'GER')!;
    expect(v.tier).toBe('full');
    expect(v.shut).toBe(true);
    expect(v.burned).toBe(2);
    expect(v.price).toBe(fieldRepairCost(g, id));
    expect(v.words).toMatch(/runway shut/);
    const back = loadGame(saveGame(g))!;
    expect(fieldShut(back, id)).toBe(true);
    expect(fieldLost(back, id)).toBe(2);
    expect(fieldView(grand(3, false), id)).toBeNull();
  });
});

describe('what the city glance and the bridge read (its fit check, and the design director at 06:21Z)', () => {
  it('fieldRepairView: null off; the holder sees shut, levels, room and price; a repair in hand says so', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    expect(fieldRepairView(grand(3, false), id, 'GER')).toBeNull();
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    const v = fieldRepairView(g, id, 'GER')!;
    expect(v.shut).toBe(true);
    expect(v.lost).toBe(2);
    expect(v.roomNow).toBe(airfieldsHold(g, id));
    expect(v.cost).toBe(fieldRepairCost(g, id));
    expect(v.months).toBe(3);
    g.nations.GER.pp = 100;
    orderFieldRepair(g, 'GER', id);
    expect(fieldRepairView(g, id, 'GER')!.why).toMatch(/^Repair in hand: open next month/);
  });

  it('one fog gate: own, ally, a placed agent, or seen at FIELD_SEEN; the unknown shows no shut and no room', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    damageField(g, id, 'raid', { through: 1000, foe: foeOf(g, 'UK') });
    expect(FIELD_SEEN).toBe(2);
    expect(fieldKnown(g, 'GER', id)).toBe(true);
    const blind = new Uint8Array(g.regions.length);
    expect(fieldKnown(g, 'UK', id, blind)).toBe(false);
    const v = fieldRepairView(g, id, 'UK')!;
    if (!v.known) { expect(v.shut).toBe(false); expect(v.roomNow).toBe(-1); expect(v.crowded).toBe(false); }
    const glimpse = new Uint8Array(g.regions.length); glimpse[id] = 1;
    expect(fieldKnown(g, 'UK', id, glimpse)).toBe(false);
    glimpse[id] = 2;
    expect(fieldKnown(g, 'UK', id, glimpse)).toBe(true);
  });

  it('a shut field is not also a burning prize on the map', () => {
    const g = grand();
    const { id } = mainField(g, 'GER');
    wreckPrize(g, g.regions[id], 'airfield', 3, 'GER', 'plough');
    expect(prizeMarks(g).some((m) => m.region === id && m.burning.includes('airfield'))).toBe(false);
  });
});

