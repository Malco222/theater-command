// Earned colours through the 3D doorway (release 2; Brian, 25 November 2026: "unique colorings or markings like the
// tuskegee airmen"; 26 November: "the b-17s lost their tail markings", "for the squadron").
//
// The campaign gives a group its colours (airnicks.ts) and the 3D game paints them (Battle/AirUnitColours), but until
// now nothing carried them from the one to the other. Two things must reach the 3D game for a group to wear what it
// has earned, and both are held here:
//   - the board says which groups have colours: place.corpsMarks, rows of { corps, nat, scheme, tier, name }
//     (AirFeed.cpp ReadSky, AirUnitColours::ReadMarks: a row with no corps, no nation, a scheme outside 0 to 15 or a
//     tier outside 1 to 3 is left out);
//   - every air unit says which group it is: extra.corps (AirFeed.cpp reads it as the group's Corps; without it every
//     group is -1 and no row is ever found).
// Paint only: the same battle is fought with them and without them.
import { describe, expect, it } from 'vitest';
import { AIRCOLOURS, AIRNICKS, airRaidFlown } from '../src/campaign/airnicks';
import { airAct } from '../src/campaign/airpages';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { setNicknames } from '../src/campaign/nicknames';
import { startOpen } from '../src/campaign/staff';
import type { AirFleet, AirSide, Game } from '../src/campaign/types';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';

interface Row { corps: number; nat: string; scheme: number; tier: number; name: string }
interface Unit { id: number; side: 'att' | 'def'; kind: string; extra: { corps?: number; model?: string } }

/** The raid the air doorway's own test flies (battle-bridge-air.test.ts): Germany's day raid on Alsace. */
function setup(nicknames = true) {
  const g = startOpen(createGame(7, 'GER'));
  for (const w of ['UK', 'FRA']) setWar(g, 'GER', w);
  if (nicknames) setNicknames(g, true);
  const t = raidTargets(g, 'bomb', 'GER').find((x) => planRaid(g, 'bomb', x.id, undefined, 'GER').place === 'Alsace')!;
  const day = planRaid(g, 'bomb', t.id, undefined, 'GER');
  const TC = battleCalls({ game: () => g, attack: () => null, raid: (k, tg, ids, f) => airAct(g, `raid:${k}:${tg}:${ids.join('.')}`, f) });
  return { g, TC, target: t.id, ids: day.picked };
}
const fleet = (g: Game, id: number): AirFleet => g.airFleets.find((f) => f.id === id)!;
const side = (f: AirFleet, ftr: number, bmr: number): AirSide => ({ nat: f.nat, fleet: f.name, id: f.id, base: f.base, leader: null, ftr, bmr, lostFtr: 0, lostBmr: 0 });
const raid = { odds: 3, bombers: 100, bombed: 90, E: 50, D: 40, sky: false };
/** A group's next mission flown through the campaign's own rule, from one short of 'n' missions done. */
function fly(g: Game, f: AirFleet, n: number, defending = false): void {
  f.done = { n: n - 1, at: 0, x: 0, h: 0 };
  if (defending) airRaidFlown(g, 'bomb', raid, [], [{ f, side: side(f, 40, 0) }], -1);
  else airRaidFlown(g, 'bomb', raid, [{ f, side: side(f, 0, 100) }], [], -1);
}
/**
 * The battle run on until the raid sees a defender (the view is the player's own: the defence is not in it until it is
 * seen), and that view. The same war always meets the same defender at the same moment.
 */
function untilMet(TC: ReturnType<typeof battleCalls>): { units: Unit[]; over?: unknown } {
  TC.battleSpeed(4);
  let v = TC.battleTick(5) as { units: Unit[]; over?: unknown };
  for (let k = 0; k < 2000 && !v.over && !v.units.some((u) => u.side === 'def'); k++) v = TC.battleTick(5) as { units: Unit[]; over?: unknown };
  return v;
}
/** A row as the 3D game's reader keeps it (AirUnitColours::ReadMarks). */
const reads = (r: Row): boolean => Number.isInteger(r.corps) && r.corps >= 0 && r.nat.length > 0 && Number.isInteger(r.scheme) && r.scheme >= 0 && r.scheme <= 15 && Number.isInteger(r.tier) && r.tier >= 1 && r.tier <= 3 && typeof r.name === 'string';

describe('earned colours reach the 3D air battle', () => {
  it('every air unit names its campaign group, and a battle with no colours earned sends no rows', () => {
    const { g, TC, target, ids } = setup();
    const look = TC.airLook('bomb', target, ids).ticket!;
    expect(look.place.corpsMarks).toBeUndefined();
    const st = TC.airStart('bomb', target, ids)!;
    expect((st.sky.place as { corpsMarks?: Row[] }).corpsMarks).toBeUndefined();
    const units = st.view.units as Unit[];
    expect(units.length).toBeGreaterThan(0);
    const of = { att: new Set(look.att.forces.map((f) => f.corps)), def: new Set(look.def.forces.map((f) => f.corps)) };
    for (const u of units) {
      expect(of[u.side].has(u.extra.corps!), `unit ${u.id}`).toBe(true);
      // The id is the campaign group's own, of the unit's own side's nation or its ally's.
      expect(fleet(g, u.extra.corps!)).toBeTruthy();
    }
    // The raid's own groups are the ones the player picked.
    expect(new Set(units.filter((u) => u.side === 'att').map((u) => u.extra.corps))).toEqual(new Set(ids));
    // Still said as the battle goes on, and of the defence once it is seen.
    const v = untilMet(TC);
    expect(v.units.some((u) => u.side === 'def')).toBe(true);
    for (const u of v.units) expect(of[u.side].has(u.extra.corps!), `unit ${u.id}`).toBe(true);
  });

  it('a group that has earned colours is on the board with its scheme and tier, on the raid card and in the battle', () => {
    // (Which group of the defence the raid meets first, from the same battle flown once before.)
    const first = setup();
    first.TC.airStart('bomb', first.target, first.ids);
    const met = untilMet(first.TC).units.find((u) => u.side === 'def')!.extra.corps!;
    const { g, TC, target, ids } = setup();
    const before = TC.airLook('bomb', target, ids).ticket!;
    const bomber = ids.map((id) => fleet(g, id)).find((f) => before.att.forces.some((x) => x.corps === f.id && x.kind === 'bmr'))!;
    const defender = fleet(g, met);
    expect(bomber && defender).toBeTruthy();
    expect(before.def.forces.some((x) => x.corps === defender.id)).toBe(true);
    // The bombers reach the first mark (the spinners and the fin's tip, and a name); the defenders the third (the tail).
    fly(g, bomber, AIRNICKS.need);
    fly(g, defender, AIRCOLOURS.tiers[2], true);
    expect(bomber.colours).toMatchObject({ tier: 1 });
    expect(defender.colours).toMatchObject({ tier: 3 });
    expect(bomber.nick).toBeTruthy();
    const want: Row[] = [
      { corps: bomber.id, nat: bomber.nat, scheme: bomber.colours!.scheme, tier: 1, name: bomber.nick! },
      { corps: defender.id, nat: defender.nat, scheme: defender.colours!.scheme, tier: 3, name: defender.nick ?? '' },
    ];
    const look = TC.airLook('bomb', target, ids).ticket!;
    expect(look.place.corpsMarks).toEqual(want);
    const st = TC.airStart('bomb', target, ids)!;
    const rows = (st.sky.place as { corpsMarks?: Row[] }).corpsMarks!;
    expect(rows).toEqual(want);
    for (const r of rows) expect(reads(r), JSON.stringify(r)).toBe(true);
    // Each row's group is flying: the 3D game finds its models by the same id.
    const wears = (units: Unit[], id: number) => units.filter((u) => u.extra.corps === id);
    expect(wears(st.view.units as Unit[], bomber.id).some((u) => u.kind === 'bmr' && u.side === 'att')).toBe(true);
    const seen = untilMet(TC).units;
    expect(wears(seen, defender.id).length).toBeGreaterThan(0);
    expect(wears(seen, defender.id).every((u) => u.side === 'def')).toBe(true);
    // And only groups with colours have a row.
    expect(rows.map((r) => r.corps).sort()).toEqual(g.airFleets.filter((f) => f.colours).map((f) => f.id).sort());
  });

  it('colours a group keeps are still sent with earned names switched off, without the name', () => {
    const { g, TC, target, ids } = setup();
    const f = fleet(g, ids[0]);
    fly(g, f, AIRNICKS.need);
    expect(f.colours && f.nick).toBeTruthy();
    setNicknames(g, false);
    expect(TC.airLook('bomb', target, ids).ticket!.place.corpsMarks).toEqual([{ corps: f.id, nat: f.nat, scheme: f.colours!.scheme, tier: f.colours!.tier, name: '' }]);
  });

  it('paint only: the same battle is fought, to the same end, with colours and without', () => {
    const fought = (colours: boolean) => {
      const { g, TC, target, ids } = setup();
      if (colours) for (const id of ids) fleet(g, id).colours = { scheme: 3, tier: 3, since: g.turn };
      const st = TC.airStart('bomb', target, ids)!;
      const { corpsMarks, ...place } = st.sky.place as unknown as Record<string, unknown>;
      expect(Array.isArray(corpsMarks)).toBe(colours);
      TC.battleSpeed(4);
      let v: any = null;
      const seen: string[] = [JSON.stringify(st.view), JSON.stringify(place)];
      for (let k = 0; k < 2000 && !(v && v.over); k++) { v = TC.battleTick(5); if (k % 50 === 0) seen.push(JSON.stringify(v)); }
      expect(v.over).toBeTruthy();
      seen.push(JSON.stringify(v));
      const out: any = TC.battleEnd();
      expect(out.ok).toBe(true);
      return { seen, after: JSON.stringify(out.after), report: JSON.stringify(out.report), reports: JSON.stringify(g.airReports), fleets: JSON.stringify(g.airFleets.map(({ colours: _c, ...f }) => f)) };
    };
    const plain = fought(false), painted = fought(true);
    expect(painted.seen).toEqual(plain.seen);
    expect(painted.after).toBe(plain.after);
    expect(painted.report).toBe(plain.report);
    expect(painted.reports).toBe(plain.reports);
    expect(painted.fleets).toBe(plain.fleets);
  }, 120_000);
});
