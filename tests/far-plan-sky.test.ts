// The plan holds the sky it lands under (rider b631 (iv) to ruling 234; the design director's notes of 12:41Z,
// 13:57Z, 14:11Z and 14:38Z, 19 November 2026). A landing plan names the land air in reach of each shore it still owes a
// landing on where enemy fighters deny the sky, brings a fighter group up where none is in reach, and flies the fit
// ones for that sky before anything else the staff decides, whatever the forecast: over the enemy field that denies
// several of its shores where the plan's bombers reach that field, else over the shore. A shore once denied is the plan's until
// no landing is owed on it: in an open month its groups stand in reach and fly nothing else. For a computer's staff
// on the far side where no book dates the plan's rows: stop 3 and looser. With the far side off:
// tests/pacific-war-off.test.ts.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { afterEach, describe, expect, it } from 'vitest';
import { planSkies } from '../src/campaign/ai';
import { AIR, AIRWAR, airAI, airFleetsOf, assessAir, fightersOver, fightersOverFrom, forecastAir, planSkyKey, skyDenies, skyWatch, whyNotMission } from '../src/campaign/air';
import { addCorps, createGame, declareWar } from '../src/campaign/game';
import { setStop } from '../src/campaign/history';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, AirReport, Game, PlanPhase, Region } from '../src/campaign/types';

type Told = { nat: string; to: Region; shores: Region[]; named: AirFleet[]; flew: AirFleet[]; report: AirReport | null };
const told: Told[] = [];
const listen = (): void => { told.length = 0; skyWatch.tell = (e) => { told.push(e); }; };
afterEach(() => { skyWatch.tell = null; });

const group = (g: Game, nat: string, name: string): AirFleet => airFleetsOf(g, nat).find((f) => f.name === name)!;
const names = (fs: AirFleet[]): string[] => fs.map((f) => f.name).sort();
/** Puts a group on a field as it stands this month: rested there, not moved and not flown. */
const stand = (g: Game, f: AirFleet, place: string, ready = 1): AirFleet => { f.base = g.byName[place].id; f.ready = ready; f.moved = 0; f.mission = 'support'; f.target = -1; return f; };
/**
 * December 1941, Japan at war with America, and Luzon's pursuit squadron at the record's hundred fighters on
 * Lingayen's field: they deny the sky there. Japan's land air stands where the far side's first day put it (the
 * fighters at Hsinking, Hankow and Tokyo), none of it in reach of Luzon.
 */
const war = (stop = 3, human: string | null = null): Game => {
  const g = startOpen(createGame(3, human));
  if (stop !== 3) setStop(g, stop, true);
  g.turn = 28;
  declareWar(g, 'JAP', 'USA');
  group(g, 'USA', '3rd Pursuit Squadron').ftr = 100;
  return g;
};
/** A landing plan with one corps waiting on Formosa's quay for Lingayen, or Aparri if not there. */
const plan = (g: Game): { ph: PlanPhase; corps: ReturnType<typeof addCorps> } => {
  const corps = addCorps(g, 'JAP', 'A division for Lingayen', 'inf', 1, g.byName.Formosa.id, {});
  return { ph: { name: 'a landing plan', from: 28, to: 34, posture: 'attack', landings: [{ corps: corps.name, to: ['Lingayen', 'Aparri'] }] } as PlanPhase, corps };
};

describe("the shores a landing plan still owes a landing on (rider b631 (iv) to ruling 234)", () => {
  it("are every place still an enemy's on the list of a corps that waits in port, in the plan's order, from the month the plan is at war", () => {
    const g = war();
    const { ph, corps } = plan(g);
    const L = g.byName.Lingayen, A = g.byName.Aparri;
    expect(planSkies(g, 'JAP', ph, null)).toEqual([L.id, A.id]);
    // A place that is Japan's already is a landing made.
    A.ctrl = 'JAP';
    expect(planSkies(g, 'JAP', ph, null)).toEqual([L.id]);
    A.ctrl = 'USA';
    // A corps that has left its quay has made its landing; a plan with no landings has none.
    corps.reg = g.byName.Hankow.id;
    expect(planSkies(g, 'JAP', ph, null)).toEqual([]);
    corps.reg = g.byName.Formosa.id;
    expect(planSkies(g, 'JAP', { ...ph, landings: undefined } as PlanPhase, null)).toEqual([]);
    expect(planSkies(g, 'JAP', null, null)).toEqual([]);
    // At peace a shore is nobody's to fight over.
    const peace = startOpen(createGame(3, null));
    peace.turn = 28;
    expect(planSkies(peace, 'JAP', plan(peace).ph, null)).toEqual([]);
  });

  it("are none where the book dates the plan's rows (stops 1 and 2) or for a player's own plan; stops 3 to 5 read them alike", () => {
    for (const stop of [1, 2]) { const g = war(stop); expect(planSkies(g, 'JAP', plan(g).ph, null), `stop ${stop}`).toEqual([]); }
    for (const stop of [3, 4, 5]) { const g = war(stop); expect(planSkies(g, 'JAP', plan(g).ph, null), `stop ${stop}`).toEqual([g.byName.Lingayen.id, g.byName.Aparri.id]); }
    const mine = war(3, 'JAP');
    expect(planSkies(mine, 'JAP', plan(mine).ph, null)).toEqual([]);
  });
});

describe("the plan's fight for the sky it lands under (rider b631 (iv) to ruling 234)", () => {
  it('names every land group in reach of the shore and flies the fit ones first, whatever the forecast; an unfit group is named, not flown and not moved', () => {
    const g = war();
    const L = g.byName.Lingayen;
    expect(skyDenies(g, 'JAP', L.id)).toBe(true);
    const few = stand(g, group(g, 'JAP', '1st Sentai'), 'Formosa'), bombers = stand(g, group(g, 'JAP', '2nd Air Brigade'), 'Formosa'), tired = stand(g, group(g, 'JAP', '3rd Air Brigade'), 'Formosa', 0.4);
    for (const f of [few, bombers, tired]) expect(whyNotMission(g, f, 'superiority', L.id), f.name).toBeNull();
    // Sixty fighters against a hundred over their own field: the staff's own sweep would not be flown on this forecast.
    const fc = forecastAir(g, assessAir(g, [few, bombers], 'superiority', L.id), 'superiority', L.id);
    expect(fc.odds).toBeLessThan(AIR.sky.win * AIRWAR.ai.raids.sky);
    const before = g.airReports.length;
    listen();
    airAI(g, 'JAP', null, [L.id]);
    expect(told.length).toBe(1);
    expect([told[0].nat, told[0].to.name, names(told[0].named), names(told[0].flew)]).toEqual(['JAP', 'Lingayen', ['1st Sentai', '2nd Air Brigade', '3rd Air Brigade'], ['1st Sentai', '2nd Air Brigade']]);
    // It is the first thing the staff flies this month.
    const first = g.airReports[before];
    expect([first.attNat, first.mission, first.region]).toEqual(['JAP', 'superiority', L.id]);
    expect(told[0].report).toBe(first);
    for (const f of [few, bombers]) expect([f.mission, f.target, f.flew], f.name).toEqual(['superiority', L.id, 28]);
    // The tired group is the plan's for the month all the same: it stands where it stood and flies nothing.
    expect([g.regions[tired.base].name, tired.mission, tired.flew === 28, tired.moved === 28]).toEqual(['Formosa', 'support', false, false]);
  });

  it('is not flown by the staff of its own accord: with no plan to tell it of the shore the same groups are not sent for that sky', () => {
    const g = war();
    const L = g.byName.Lingayen;
    const few = stand(g, group(g, 'JAP', '1st Sentai'), 'Formosa'), bombers = stand(g, group(g, 'JAP', '2nd Air Brigade'), 'Formosa');
    listen();
    airAI(g, 'JAP', null);
    expect(told).toEqual([]);
    expect(g.airReports.some((r) => r.turn === 28 && r.attNat === 'JAP' && r.mission === 'superiority' && r.region === L.id)).toBe(false);
    expect([few, bombers].some((f) => f.mission === 'superiority')).toBe(false);
  });

  it('brings one fighter group up where none is in reach, as for a hop, and flies it with the bombers if it is still fit; the homeland guard is not the one', () => {
    const g = war();
    const L = g.byName.Lingayen;
    const bombers = stand(g, group(g, 'JAP', '2nd Air Brigade'), 'Formosa');
    const sentai = group(g, 'JAP', '1st Sentai'), guard = group(g, 'JAP', '3rd Air Brigade');
    expect([g.regions[sentai.base].name, g.regions[guard.base].name]).toEqual(['Hankow', 'Tokyo']);
    for (const f of [sentai, guard]) expect(whyNotMission(g, f, 'superiority', L.id), f.name).not.toBeNull();
    listen();
    airAI(g, 'JAP', null, [L.id]);
    // The Sentai comes forward to a field in reach; the move costs it readiness, and it is still fit to fly.
    expect(sentai.moved).toBe(28);
    expect(g.regions[sentai.base].name).toBe('Formosa');
    expect(1 - AIR.ready.move).toBeGreaterThanOrEqual(AIRWAR.ai.raids.ready);
    expect(sentai.ready).toBeLessThan(1 - AIR.ready.move);
    // (One group, as for a hop: the brigade over the homeland is not brought to the beach. Where the staff's basing
    // rules send it afterwards is their own affair.)
    expect(whyNotMission(g, guard, 'superiority', L.id)).not.toBeNull();
    expect([names(told[0].named), names(told[0].flew)]).toEqual([['1st Sentai', '2nd Air Brigade'], ['1st Sentai', '2nd Air Brigade']]);
    expect(told[0].report?.mission).toBe('superiority');
  });

  it('does not send bombers for a sky with no fighter group: they are named and wait for the one brought up', () => {
    const g = war();
    const L = g.byName.Lingayen;
    const bombers = stand(g, group(g, 'JAP', '2nd Air Brigade'), 'Formosa');
    // No fighter group can come this month: each has had its move.
    for (const f of airFleetsOf(g, 'JAP')) if (f.kind === 'ftr') f.moved = 28;
    listen();
    airAI(g, 'JAP', null, [L.id]);
    expect([names(told[0].named), told[0].flew, told[0].report]).toEqual([['2nd Air Brigade'], [], null]);
    // Named, the bombers are the plan's for the month: not sent on a raid or against ships, and not moved.
    expect([g.regions[bombers.base].name, bombers.mission, bombers.flew === 28, bombers.moved === 28]).toEqual(['Formosa', 'support', false, false]);
  });

  it("passes by a shore whose sky nobody denies, and one that is not an enemy's: the staff is told nothing and its groups are its own", () => {
    const g = war();
    const L = g.byName.Lingayen, A = g.byName.Aparri;
    group(g, 'USA', '3rd Pursuit Squadron').ftr = 30;
    expect(skyDenies(g, 'JAP', L.id)).toBe(false);
    stand(g, group(g, 'JAP', '1st Sentai'), 'Formosa');
    A.ctrl = 'JAP';
    listen();
    airAI(g, 'JAP', null, [L.id, A.id]);
    expect(told).toEqual([]);
  });

  it("goes over the enemy field that by itself denies several of the plan's shores, so the bombers strike its groups on the ground: one fight, for every shore the field denies", () => {
    const g = war();
    const L = g.byName.Lingayen, A = g.byName.Aparri;
    const few = stand(g, group(g, 'JAP', '1st Sentai'), 'Formosa'), bombers = stand(g, group(g, 'JAP', '2nd Air Brigade'), 'Formosa');
    // Lingayen's fighters cover Aparri too: the one field denies both shores, and both are in reach of Formosa.
    expect([skyDenies(g, 'JAP', A.id), skyDenies(g, 'JAP', L.id)]).toEqual([true, true]);
    for (const t of [A, L]) {
      const by = fightersOverFrom(g, t.id, 'JAP');
      expect([...by.keys()], t.name).toEqual([L.id]);
      expect(by.get(L.id), t.name).toBeCloseTo(fightersOver(g, t.id, 'JAP'), 9);
      expect(by.get(L.id), t.name).toBeGreaterThanOrEqual(AIR.sky.least);
    }
    expect([whyNotMission(g, few, 'superiority', A.id), whyNotMission(g, few, 'superiority', L.id)]).toEqual([null, null]);
    const onField = group(g, 'USA', '1st Bombardment Squadron');
    expect([g.regions[onField.base].name, onField.ready]).toEqual(['Lingayen', 1]);
    listen();
    // Aparri is first in the plan's order; the fight goes where the fighters that deny it stand.
    airAI(g, 'JAP', null, [A.id, L.id]);
    expect(told.map((e) => [e.to.name, e.shores.map((r) => r.name), names(e.named), names(e.flew)])).toEqual([['Lingayen', ['Aparri', 'Lingayen'], ['1st Sentai', '2nd Air Brigade'], ['1st Sentai', '2nd Air Brigade']]]);
    expect([told[0].report?.mission, told[0].report?.region]).toEqual(['superiority', L.id]);
    for (const f of [few, bombers]) expect([f.mission, f.target], f.name).toEqual(['superiority', L.id]);
    // The bombers that got through found the field in use: the groups on it were struck where they stood.
    expect(told[0].report?.result).toMatch(/^The airfields of .* were bombed/);
    expect(onField.ready).toBeLessThan(1);
    // Both shores are the plan's skies from this month.
    expect([g.flags[planSkyKey('JAP', A.id)], g.flags[planSkyKey('JAP', L.id)]]).toEqual([28, 28]);
  });

  it("goes over the first denied shore with every group that reaches it, as first built, where no bomber group of the plan's reaches that field: there is no reason to go there", () => {
    const g = war();
    const L = g.byName.Lingayen, A = g.byName.Aparri;
    const few = stand(g, group(g, 'JAP', '1st Sentai'), 'Formosa');
    // Fighters only: nothing to strike the field with.
    listen();
    airAI(g, 'JAP', null, [A.id, L.id]);
    expect(told.map((e) => [e.to.name, e.shores.map((r) => r.name), names(e.named), names(e.flew)])).toEqual([['Aparri', ['Aparri'], ['1st Sentai'], ['1st Sentai']], ['Lingayen', ['Lingayen'], [], []]]);
    expect([few.mission, few.target]).toEqual(['superiority', A.id]);
    // Bombers that reach the first shore and not the field fly over the shore with the fighters.
    // (Foochow's field: 993 km from Aparri on the globe, inside its bombers' 1,048, and 1,141 from Lingayen, beyond
    // them. The war this was first seen in had them on Hengyang, by the map's line and at the bombers' older 1,300.)
    const far = war();
    far.byName.Foochow.ctrl = 'JAP';
    const fighters = stand(far, group(far, 'JAP', '1st Sentai'), 'Formosa'), bombers = stand(far, group(far, 'JAP', '2nd Air Brigade'), 'Foochow');
    expect([whyNotMission(far, bombers, 'superiority', A.id) === null, whyNotMission(far, bombers, 'superiority', L.id) === null]).toEqual([true, false]);
    listen();
    airAI(far, 'JAP', null, [A.id, L.id]);
    expect(told.map((e) => [e.to.name, e.shores.map((r) => r.name), names(e.named), names(e.flew)])).toEqual([['Aparri', ['Aparri'], ['1st Sentai', '2nd Air Brigade'], ['1st Sentai', '2nd Air Brigade']], ['Lingayen', ['Lingayen'], [], []]]);
    for (const f of [fighters, bombers]) expect([f.mission, f.target], f.name).toEqual(['superiority', A.id]);
    expect(told[0].report?.result).toBe('The bombers found no airfields in use.');
  });

  it("goes over the first shore too where the plan's bombers reach the field and no fighter group can: bombers do not fight for a sky alone; and a field that denies one shore only is not fought over", () => {
    // (America's plan here, where the others are Japan's. Since the ranges were laid by the record Japan's fighters
    // reach as far as its bombers, 1,048 km, so a field its bombers reach is one a fighter group of its own can be
    // brought up to. America's bombers out-reach its fighters: 1,243 km against 300 in December 1941.)
    const g = war();
    const L = g.byName.Lingayen, A = g.byName.Aparri, F = g.byName.Formosa;
    // Japan holds Lingayen and Aparri, and the hundred fighters on Formosa's field deny both (745 and 583 km of their
    // 1,048). Manila's bombers reach Formosa (903 km). Its pursuit squadron reaches Lingayen (173) and neither Aparri
    // (342) nor Formosa, and America holds no field nearer to either.
    for (const r of [L, A]) r.ctrl = 'JAP';
    stand(g, group(g, 'JAP', '3rd Air Brigade'), 'Formosa');
    const few = stand(g, group(g, 'USA', '3rd Pursuit Squadron'), 'Manila'), bombers = stand(g, group(g, 'USA', '1st Bombardment Squadron'), 'Manila');
    for (const t of [L, A]) expect(fightersOverFrom(g, t.id, 'USA').get(F.id), t.name).toBeGreaterThanOrEqual(AIR.sky.least);
    expect([whyNotMission(g, few, 'superiority', L.id) === null, whyNotMission(g, few, 'superiority', A.id) === null, whyNotMission(g, few, 'superiority', F.id) === null, whyNotMission(g, bombers, 'superiority', F.id) === null]).toEqual([true, false, false, true]);
    listen();
    airAI(g, 'USA', null, [L.id, A.id]);
    expect(told.map((e) => [e.to.name, e.shores.map((r) => r.name), names(e.named), names(e.flew)])).toEqual([['Lingayen', ['Lingayen'], ['1st Bombardment Squadron', '3rd Pursuit Squadron'], ['1st Bombardment Squadron', '3rd Pursuit Squadron']], ['Aparri', ['Aparri'], [], []]]);
    expect(few.moved === 28).toBe(false);
    // One shore, denied from a field elsewhere (Manila's pursuit squadron over Lingayen, and Formosa's groups reach
    // both): the fight is over the shore, as for any one shore.
    const one = war();
    group(one, 'USA', '3rd Pursuit Squadron').base = one.byName.Manila.id;
    stand(one, group(one, 'JAP', '1st Sentai'), 'Formosa'); stand(one, group(one, 'JAP', '2nd Air Brigade'), 'Formosa');
    listen();
    airAI(one, 'JAP', null, [one.byName.Lingayen.id]);
    expect(told.map((e) => [e.to.name, e.shores.map((r) => r.name), names(e.flew)])).toEqual([['Lingayen', ['Lingayen'], ['1st Sentai', '2nd Air Brigade']]]);
  });

  it("gives a group in reach of two of the plan's skies to the first, in the plan's order", () => {
    const g = war();
    const L = g.byName.Lingayen, S = g.byName.Legaspi;
    // Two fields, each denying one shore: Lingayen's own, and a second squadron's at Legaspi.
    const second = group(g, 'USA', '1st Pursuit Squadron');
    second.base = S.id; second.ftr = 100;
    expect([skyDenies(g, 'JAP', L.id), skyDenies(g, 'JAP', S.id)]).toEqual([true, true]);
    const few = stand(g, group(g, 'JAP', '1st Sentai'), 'Formosa');
    listen();
    airAI(g, 'JAP', null, [L.id, S.id]);
    expect(told.map((e) => [e.to.name, names(e.named), names(e.flew)])).toEqual([['Lingayen', ['1st Sentai'], ['1st Sentai']], ['Legaspi', [], []]]);
    expect(few.target).toBe(L.id);
  });
});

describe("a shore once denied is the plan's sky until no landing is owed on it (the design director's note of 14:11Z)", () => {
  /** The month after a fight for Lingayen's sky: the squadron that denied it is up over Aparri itself, so the sky over Lingayen is open. */
  const after = (): { g: Game; all: AirFleet[] } => {
    const g = war();
    const all = ['1st Air Brigade', '1st Sentai', '2nd Air Brigade'].map((nm) => stand(g, group(g, 'JAP', nm), 'Formosa'));
    airAI(g, 'JAP', null, [g.byName.Lingayen.id]);
    g.turn = 29;
    for (const f of all) f.ready = 1;
    const sq = group(g, 'USA', '3rd Pursuit Squadron');
    sq.ftr = 100; sq.mission = 'superiority'; sq.target = g.byName.Aparri.id;
    return { g, all };
  };

  it('in an open month its groups stand in reach, named and flown nowhere, where the staff alone would have its own use for them', () => {
    const { g, all } = after();
    const L = g.byName.Lingayen;
    expect([g.flags[planSkyKey('JAP', L.id)], skyDenies(g, 'JAP', L.id)]).toEqual([28, false]);
    listen();
    airAI(g, 'JAP', null, [L.id]);
    expect(told.map((e) => [e.to.name, e.shores.map((r) => r.name), names(e.named), e.flew, e.report])).toEqual([['Lingayen', ['Lingayen'], ['1st Air Brigade', '1st Sentai', '2nd Air Brigade'], [], null]]);
    for (const f of all) expect([g.regions[f.base].name, f.mission, f.flew === 29, f.moved === 29, !!f.close], f.name).toEqual(['Formosa', 'support', false, false, false]);
    expect(g.flags[planSkyKey('JAP', L.id)]).toBe(28);
    // The same month with no plan to hold them: the staff's basing rules take the big fighter brigade back to China.
    const twin = after();
    for (const k of Object.keys(twin.g.flags)) if (k.startsWith('sky:plan:')) delete twin.g.flags[k];
    listen();
    airAI(twin.g, 'JAP', null);
    expect(told).toEqual([]);
    expect([twin.g.regions[twin.all[0].base].name === 'Formosa', twin.all[0].moved]).toEqual([false, 29]);
  });

  it('and fly for it again in any month it is denied', () => {
    const { g, all } = after();
    const L = g.byName.Lingayen;
    airAI(g, 'JAP', null, [L.id]);
    g.turn = 30;
    for (const f of all) f.ready = 1;
    const sq = group(g, 'USA', '3rd Pursuit Squadron');
    sq.ftr = 100; sq.mission = 'support'; sq.target = -1;
    delete g.sky[L.id];
    expect(skyDenies(g, 'JAP', L.id)).toBe(true);
    listen();
    airAI(g, 'JAP', null, [L.id]);
    expect(told.map((e) => [e.to.name, names(e.named), names(e.flew), e.report?.mission])).toEqual([['Lingayen', ['1st Air Brigade', '1st Sentai', '2nd Air Brigade'], ['1st Air Brigade', '1st Sentai', '2nd Air Brigade'], 'superiority']]);
    expect(g.flags[planSkyKey('JAP', L.id)]).toBe(28);
  });

  it('is let go when no landing is owed there: the mark goes, and the groups are the staff\'s own again', () => {
    const { g } = after();
    const L = g.byName.Lingayen;
    listen();
    airAI(g, 'JAP', null, []);
    expect(told).toEqual([]);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('sky:plan:'))).toEqual([]);
    // A shore never denied is never the plan's: an open sky over a shore the plan owes holds nobody.
    const fresh = war();
    group(fresh, 'USA', '3rd Pursuit Squadron').ftr = 30;
    stand(fresh, group(fresh, 'JAP', '1st Sentai'), 'Formosa');
    listen();
    airAI(fresh, 'JAP', null, [fresh.byName.Lingayen.id]);
    expect(told).toEqual([]);
    expect(Object.keys(fresh.flags).filter((k) => k.startsWith('sky:plan:'))).toEqual([]);
    expect(L.ctrl).toBe('USA');
  });
});
