// Drawing up before the battle (release 2; Brian, 26 November 2026: "i should be able to move my units in a formation
// before the battle begins during the deployed phase"): the formations (src/battle/land/formation.ts), the group form
// of the setup step's land:place (sim.ts) and the calls the game makes (live.ts, land-bridge.ts).
//
// What must hold (the design director, 03:50Z: a control and no battle rule): several blocks placed as one act are
// each held to the very checks of a single place, all of them go or none, and a refusal names the block that cannot
// go; a group of one is a single place; a formation never stands a block outside the player's ground or on another
// block; and a battle in which nothing is drawn up is untouched.
import { describe, expect, it } from 'vitest';
import { drawnUp, FORMATIONS, ROW, STEP, type FormBlock } from '../src/battle/land/formation';
import { livePiece, liveTicket } from '../src/battle/land/live';
import { GROUNDS } from '../src/battle/land/field';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { LAND } from '../src/battle/land/rules';
import { GAP } from '../src/battle/land/units';
import type { Command } from '../src/battle/core/types';
// (the 3D game's own sources read as text, Vite's `?raw`, tests/ahead/raw.d.ts)
import clipboardCpp from '../unreal/TheaterCommand/Source/TheaterCommand/Battle/LandCommand.cpp?raw';
import rulesTableCpp from '../unreal/TheaterCommand/Source/TheaterCommand/Battle/LandLive.cpp?raw';
import bridgeTs from '../unreal/TheaterCommand/Rules/land-bridge.ts?raw';

const ROOM: [number, number, number, number] = [360, 120, 2640, 480];
const DEF: [number, number, number, number] = [360, 1240, 2640, 1800];
const inf = (id: number, x: number, y: number): FormBlock => ({ id, x, y, w: 150, d: 40, rear: false });
const tank = (id: number, x: number, y: number): FormBlock => ({ id, x, y, w: 130, d: 60, rear: false });
const gun = (id: number, x: number, y: number): FormBlock => ({ id, x, y, w: 110, d: 50, rear: true });

/** Every block inside the room, and no two nearer than the setup step's own clearance. */
function sound(slots: { id: number; x: number; y: number }[], blocks: FormBlock[], room: number[]): void {
  const of = new Map(blocks.map((b) => [b.id, b]));
  expect(slots.map((s) => s.id).sort((a, b) => a - b)).toEqual(blocks.map((b) => b.id).sort((a, b) => a - b));
  for (const s of slots) {
    const b = of.get(s.id)!;
    expect(s.x - b.w / 2).toBeGreaterThanOrEqual(room[0]);
    expect(s.x + b.w / 2).toBeLessThanOrEqual(room[2]);
    expect(s.y - b.d / 2).toBeGreaterThanOrEqual(room[1]);
    expect(s.y + b.d / 2).toBeLessThanOrEqual(room[3]);
  }
  for (let i = 0; i < slots.length; i++) for (let j = 0; j < i; j++) {
    const a = slots[i], b = slots[j], A = of.get(a.id)!, B = of.get(b.id)!;
    const apart = Math.abs(a.x - b.x) >= (A.w + B.w) / 2 + LAND.placeClear || Math.abs(a.y - b.y) >= (A.d + B.d) / 2 + LAND.placeClear;
    expect(apart, `blocks ${a.id} and ${b.id} stand clear`).toBe(true);
  }
}

describe('the formations', () => {
  const army = [inf(1, 900, 300), inf(2, 1500, 260), inf(3, 1200, 340), tank(4, 1300, 200), tank(5, 1700, 220), gun(6, 1000, 160), gun(7, 1600, 150)];

  it('a line: side by side in the order they stand, the guns in a row behind', () => {
    const s = drawnUp('line', army, 1, ROOM)!;
    sound(s, army, ROOM);
    const at = new Map(s.map((p) => [p.id, p]));
    const front = [1, 3, 4, 2, 5].map((id) => at.get(id)!);
    expect(new Set(front.map((p) => p.y)).size).toBe(1);
    for (let i = 1; i < front.length; i++) expect(front[i].x).toBeGreaterThan(front[i - 1].x);
    // Side by side with the draw-up's own gap between them.
    expect(front[1].x - front[0].x).toBeCloseTo(150 + GAP, 1);
    // The attack faces up the field: its guns stand a row further down it.
    expect(at.get(6)!.y).toBeCloseTo(front[0].y - ROW, 1);
    expect(at.get(7)!.y).toBe(at.get(6)!.y);
    expect(at.get(6)!.x).toBeLessThan(at.get(7)!.x);
  });

  it('a line for the defence is the mirror: its guns stand further up the field', () => {
    const them = army.map((b) => ({ ...b, y: b.y + 1200 }));
    const s = drawnUp('line', them, -1, DEF)!;
    sound(s, them, DEF);
    const at = new Map(s.map((p) => [p.id, p]));
    expect(at.get(6)!.y).toBeCloseTo(at.get(1)!.y + ROW, 1);
  });

  it('two lines: the half nearest the enemy in front, the other half a row behind, the guns behind both', () => {
    const s = drawnUp('two', army, 1, ROOM)!;
    sound(s, army, ROOM);
    const at = new Map(s.map((p) => [p.id, p]));
    // Nearest the enemy now (the attack looks up the field): 3 (340), 1 (300), 2 (260); then the tanks 5 and 4.
    const first = at.get(3)!.y;
    expect(at.get(1)!.y).toBe(first);
    expect(at.get(2)!.y).toBe(first);
    expect(at.get(4)!.y).toBeCloseTo(first - ROW, 1);
    expect(at.get(5)!.y).toBeCloseTo(first - ROW, 1);
    expect(at.get(6)!.y).toBeCloseTo(first - 2 * ROW, 1);
  });

  it('a wedge: the middle block foremost, each one out from it a step further back', () => {
    const s = drawnUp('wedge', army, 1, ROOM)!;
    sound(s, army, ROOM);
    const at = new Map(s.map((p) => [p.id, p]));
    const order = [1, 3, 4, 2, 5].map((id) => at.get(id)!);
    expect(order[2].y).toBeGreaterThan(order[1].y);
    expect(order[1].y - order[0].y).toBeCloseTo(STEP, 1);
    expect(order[2].y - order[1].y).toBeCloseTo(STEP, 1);
    expect(order[3].y).toBe(order[1].y);
    expect(order[4].y).toBe(order[0].y);
    // The guns behind its hindmost blocks.
    expect(at.get(6)!.y).toBeLessThan(order[0].y);
  });

  it('a column: one behind another, the foremost first and the guns at the rear', () => {
    const few = [inf(1, 900, 300), tank(4, 1300, 200), gun(6, 1000, 160)];
    const s = drawnUp('column', few, 1, ROOM)!;
    sound(s, few, ROOM);
    const at = new Map(s.map((p) => [p.id, p]));
    expect(new Set(s.map((p) => p.x)).size).toBe(1);
    expect(at.get(1)!.y).toBeGreaterThan(at.get(4)!.y);
    expect(at.get(4)!.y).toBeGreaterThan(at.get(6)!.y);
    // Where the ground is too shallow for one file, several files side by side, still inside it.
    const s2 = drawnUp('column', army, 1, ROOM)!;
    sound(s2, army, ROOM);
    expect(new Set(s2.map((p) => p.x)).size).toBeGreaterThan(1);
  });

  it('a row too wide for the ground runs on behind; ground too small for them gives none', () => {
    const many = Array.from({ length: 14 }, (_, i) => inf(i + 1, 400 + i * 150, 300));
    const narrow: [number, number, number, number] = [360, 120, 1400, 480];
    for (const k of FORMATIONS) {
      const s = drawnUp(k, many, 1, narrow)!;
      expect(s, k).not.toBeNull();
      sound(s, many, narrow);
    }
    expect(drawnUp('line', many, 1, [360, 120, 700, 200])).toBeNull();
    expect(drawnUp('line', [], 1, ROOM)).toEqual([]);
  });

  it('stands about the middle of where they stood, and inside the ground when they stood at its edge', () => {
    const edge = [inf(1, 440, 150), inf(2, 600, 150), inf(3, 760, 150), gun(4, 500, 150)];
    for (const k of FORMATIONS) sound(drawnUp(k, edge, 1, ROOM)!, edge, ROOM);
    const mid = [inf(1, 1400, 300), inf(2, 1600, 300)];
    const s = drawnUp('line', mid, 1, ROOM)!;
    expect((s[0].x + s[1].x) / 2).toBeCloseTo(1500, 0);
    expect(s[0].y).toBeCloseTo(300, 0);
  });
});

describe('placing several blocks as one act', () => {
  const open = () => livePiece('normandy-1944', 11, { me: 'att', setup: true });
  const state = (L: ReturnType<typeof open>): string => JSON.stringify(L.sim.units.map((u) => [u.id, u.x, u.y, u.face, u.home, u.order]));

  it('swaps two blocks, which no single place can do, and leaves the rest where they stood', () => {
    const L = open();
    const mine = L.sim.units.filter((u) => u.side === 'att' && L.sim.placeable(u) === null);
    expect(mine.length).toBeGreaterThan(2);
    // Two of a size, so that each fits the other's place.
    const a = mine[0], b = mine.find((u) => u !== a && u.w === a.w && u.d === a.d)!;
    expect(b).toBeDefined();
    const [ax, ay, bx, by] = [a.x, a.y, b.x, b.y];
    expect(L.canPlace(a.id, bx, by)).toBe('Another block stands there.');
    const others = JSON.stringify(L.sim.units.filter((u) => u !== a && u !== b).map((u) => [u.id, u.x, u.y, u.face]));
    expect(L.canPlaceMany([[a.id, bx, by], [b.id, ax, ay]])).toBeNull();
    expect(L.placeMany([[a.id, bx, by], [b.id, ax, ay]])).toBe(true);
    expect([a.x, a.y, b.x, b.y]).toEqual([bx, by, ax, ay]);
    expect(JSON.stringify(L.sim.units.filter((u) => u !== a && u !== b).map((u) => [u.id, u.x, u.y, u.face]))).toBe(others);
  });

  it('is all or none, and names the block that cannot go', () => {
    const L = open();
    const room = L.sim.setupRoom()!;
    const mine = L.sim.units.filter((u) => u.side === 'att' && L.sim.placeable(u) === null);
    const [a, b] = mine;
    const before = state(L);
    // The second is sent outside his ground: neither moves.
    const no = L.canPlaceMany([[a.id, a.x + 1, a.y], [b.id, b.x, room[3] + 300]]);
    expect(no).toEqual({ id: b.id, who: L.sim.named(b), why: 'Outside your ground.' });
    expect(no!.who.length).toBeGreaterThan(0);
    expect(L.placeMany([[a.id, a.x + 1, a.y], [b.id, b.x, room[3] + 300]])).toBe(false);
    expect(L.sim.command({ do: 'land:place', units: [a.id, b.id], places: [[a.x + 1, a.y], [b.x, room[3] + 300]] } as unknown as Command, 'att')).toBe(false);
    expect(state(L)).toBe(before);
    // Two sent to one place: the second is refused by the first's new place.
    expect(L.canPlaceMany([[a.id, a.x, a.y], [b.id, a.x + 5, a.y]])).toMatchObject({ id: b.id, why: 'Another block stands there.' });
    // The same block twice, a block that is not his, a block that is not there, the other side's hand: refused.
    expect(L.canPlaceMany([[a.id, a.x, a.y], [a.id, a.x + 400, a.y]])).not.toBeNull();
    const theirs = L.sim.units.find((u) => u.side === 'def')!;
    expect(L.canPlaceMany([[theirs.id, theirs.x, theirs.y]])).toMatchObject({ id: theirs.id });
    expect(L.canPlaceMany([[987654, 500, 300]])).toMatchObject({ id: 987654, why: 'No such block.' });
    expect(L.sim.command({ do: 'land:place', units: [a.id], places: [[a.x, a.y]] } as unknown as Command, 'def')).toBe(false);
    expect(L.canPlaceMany([])).not.toBeNull();
    expect(state(L)).toBe(before);
  });

  it('a group of one is a single place', () => {
    const one = open(), many = open();
    const u = one.sim.units.filter((v) => v.side === 'att' && one.sim.placeable(v) === null)[0];
    const room = one.sim.setupRoom()!;
    const spot = [[60, 0], [-60, 0], [0, 30], [0, -30], [200, 0], [-200, 0]].map(([dx, dy]) => [u.x + dx, u.y + dy]).find(([x, y]) => x > room[0] && one.canPlace(u.id, x, y) === null)!;
    expect(spot).toBeDefined();
    expect(one.place(u.id, spot[0], spot[1], 0.3)).toBe(true);
    expect(many.placeMany([[u.id, spot[0], spot[1], 0.3]])).toBe(true);
    expect(state(many)).toBe(state(one));
    one.begin(); many.begin();
    expect(JSON.stringify(many.to(20, 100000))).toBe(JSON.stringify(one.to(20, 100000)));
  });

  it('says how far off it stood them where the place worked out is barred, and when the ground has no room', () => {
    const L = open();
    const r = L.drawUp('line');
    expect(r).toMatchObject({ ok: true, why: '', left: [] });
    expect(r.off).toBeGreaterThanOrEqual(0);
    expect(r.placed.length).toBeGreaterThan(2);
    // Drawn up again where they now stand: nothing bars the place worked out, so it is not moved off it.
    expect(L.drawUp('line').off).toBe(0);
    // One named alone is a formation of one.
    const one = L.sim.units.filter((u) => u.side === 'att' && L.sim.placeable(u) === null)[0];
    expect(L.drawUp('wedge', [one.id])).toMatchObject({ ok: true, off: 0 });
    // A block of the other side named: none of his to draw up.
    const theirs = L.sim.units.find((u) => u.side === 'def')!;
    expect(L.drawUp('line', [theirs.id])).toMatchObject({ ok: false, none: 'nothing' });
  });

  it('is refused once the battle has begun', () => {
    const L = open();
    const u = L.sim.units.filter((v) => v.side === 'att')[0];
    L.begin();
    expect(L.placeMany([[u.id, u.x, u.y]])).toBe(false);
    expect(L.drawUp('line').ok).toBe(false);
  });
});

describe('drawing up through the game\'s doorway', () => {
  type TCall = Record<string, (...a: unknown[]) => any>;
  type V = { id: number; x: number; y: number; face: number; w: number; d: number; side: string; kind: string };
  // (a tick in which nothing was placed says nothing new: the units stand as they were last shown)
  let shown: V[] = [];
  const last = (d: { views: { units: V[] }[] }): V[] => (d.views.length ? (shown = d.views[d.views.length - 1].units) : shown);

  for (const me of ['att', 'def'] as const) {
    it(`draws the ${me === 'att' ? 'attack' : 'defence'} up in each formation inside its own ground, facing the enemy`, async () => {
      await import('../unreal/TheaterCommand/Rules/land-bridge');
      const TC = (globalThis as unknown as { TC: TCall }).TC;
      const h = TC.landStart('normandy-1944', null, { me, setup: true });
      const room = h.setup.room as number[];
      const first = last(TC.landTick(1));
      const theirs = JSON.stringify(first.filter((u) => u.side !== me).map((u) => [u.id, u.x, u.y, u.face]));
      for (const k of ['line', 'two', 'wedge', 'column']) {
        const r = TC.landDrawUp(k);
        expect(r, k).toMatchObject({ ok: true, why: '' });
        expect(r.placed).toBeGreaterThan(1);
        const d = TC.landTick(0.5);
        expect(d.reset).toBe(true);
        expect(d.t).toBe(0);
        const us = last(d), mine = us.filter((u) => u.side === me);
        for (const u of mine) {
          expect(u.x - u.w / 2).toBeGreaterThanOrEqual(room[0]);
          expect(u.x + u.w / 2).toBeLessThanOrEqual(room[2]);
          expect(u.y - u.d / 2).toBeGreaterThanOrEqual(room[1]);
          expect(u.y + u.d / 2).toBeLessThanOrEqual(room[3]);
        }
        // The other side stands as it stood.
        expect(JSON.stringify(us.filter((u) => u.side !== me).map((u) => [u.id, u.x, u.y, u.face]))).toBe(theirs);
      }
      // Only some of them: those named are drawn up, the rest stay.
      // (those the formations moved: a block dug in where it stands is no part of them, and says so when named alone)
      const now = last(TC.landTick(0.5)), moved = now.filter((u) => u.side === me && first.some((f) => f.id === u.id && (f.x !== u.x || f.y !== u.y)));
      const some = moved.slice(0, 3);
      expect(some).toHaveLength(3);
      const stays = now.find((u) => u.side === me && !moved.includes(u) && u.kind === 'inf');
      if (stays) {
        const dug = TC.landDrawUp('line', [stays.id]);
        expect(dug).toMatchObject({ ok: false, id: stays.id, placed: 0 });
        expect(dug.why).toMatch(/ is dug in where it stands\.$/);
        expect(dug.why).toContain(dug.who);
        // Named with some that may go: drawn up without it, and the note says which stays.
        const withIt = TC.landDrawUp('line', [stays.id, ...some.map((u) => u.id)]);
        expect(withIt).toMatchObject({ ok: true, placed: 3 });
        expect(withIt.note).toMatch(/ is dug in and stays where it stands\.$/);
        const dugToo = now.filter((u) => u.side === me && !moved.includes(u) && u.kind === 'inf').slice(0, 2);
        if (dugToo.length === 2) {
          expect(TC.landDrawUp('line', dugToo.map((u) => u.id)).why).toBe('They are all dug in where they stand.');
          expect(TC.landDrawUp('line', [...dugToo, ...some].map((u) => u.id)).note).toBe('Two of them are dug in and stay where they stand.');
        }
      }
      const rest = JSON.stringify(now.filter((u) => !some.includes(u)).map((u) => [u.id, u.x, u.y]));
      const r3 = TC.landDrawUp('line', some.map((u) => u.id));
      expect(r3.ok).toBe(true);
      expect(r3.placed).toBe(3);
      const after = last(TC.landTick(0.5));
      expect(JSON.stringify(after.filter((u) => !some.some((s) => s.id === u.id)).map((u) => [u.id, u.x, u.y]))).toBe(rest);
      const three = after.filter((u) => some.some((s) => s.id === u.id));
      expect(new Set(three.map((u) => u.face)).size).toBe(1);
      expect(three[0].face).toBeCloseTo(me === 'att' ? 0 : Math.PI, 2); // (a view rounds a facing to the thousandth)
      // A word that is no formation, and a refusal that names the block.
      expect(TC.landDrawUp('square').ok).toBe(false);
      const no = TC.landPlaceMany([[three[0].id, three[0].x, three[0].y], [three[1].id, three[1].x, room[3] + 500]]);
      expect(no).toMatchObject({ ok: false, id: three[1].id });
      expect(no.who.length).toBeGreaterThan(0);
      expect(no.why).toMatch(/ cannot go there: it would stand outside your ground\.$/);
      expect(no.why).toContain(no.who);
      const onTop = TC.landPlaceMany([[three[0].id, three[1].x + 3, three[1].y]]);
      expect(onTop.why).toMatch(/ cannot go there: another unit already stands there\.$/);
      // (the rules' short lines are made whole sentences, and none names the unit twice)
      const off = TC.landPlaceMany([[three[0].id, Number.NaN, three[0].y]]);
      expect(off.ok).toBe(false);
      expect(off.why).toMatch(/ cannot go there: it would stand off the field\.$/);
      expect(off.why.split(off.who).length).toBe(2);
      // The battle begins and is fought from where they were drawn up.
      expect(TC.landBegin().ok).toBe(true);
      expect(TC.landTick(5).t).toBeGreaterThan(4.9);
      expect(TC.landDrawUp('line').ok).toBe(false);
      TC.landEnd();
    });
  }
});

// The 3D game's clipboard (LandCommand.cpp, plain C++) works a carried group's and a drawn line's places out itself, so
// that the ring under the pointer says at once whether they may stand; the rules still decide. Its two figures are the
// rules' own, held here so that neither drifts: the setup step's clearance, and the gap between blocks side by side.
describe('the 3D clipboard keeps the rules\' figures', () => {
  const cpp: string = clipboardCpp;
  const figure = (name: string): number => Number(new RegExp(`constexpr double ${name} = ([0-9.]+);`).exec(cpp)?.[1]);
  it('the clearance between blocks placed', () => expect(figure('PlaceClear')).toBe(LAND.placeClear));
  it('the gap between blocks side by side', () => expect(figure('DrawGap')).toBe(GAP));
  // (and the Options rows the screen reads are in the game's table, each off until passed)
  it('the rows are in the table, off', () => {
    const table: string = rulesTableCpp;
    expect(table).toMatch(/\{ TEXT\("drawup"\), TEXT\("Draw up units together before battle"\), .*TEXT\("TheaterLandNoDrawUp"\), false, TEXT\("TheaterLandRuleDrawUp"\) \}/);
    expect(table).toMatch(/\{ TEXT\("groups"\), TEXT\("Groups on the number keys"\), .*TEXT\("TheaterLandNoGroups"\), false, TEXT\("TheaterLandRuleGroups"\) \}/);
    // (the map of the field in the corner: Brian, 26 November 2026)
    expect(table).toMatch(/\{ TEXT\("fieldmap"\), TEXT\("A map of the field in the corner"\), .*TEXT\("TheaterLandNoFieldMap"\), false, TEXT\("TheaterLandRuleFieldMap"\) \}/);
  });
});

// A refusal on ground a unit cannot stand on names that ground (the UI director, 26 November 2026 05:45Z): the game
// asks which it is (LiveBattle.barredBy) and the bridge says it in the sentence. Neither set piece has water inside a
// side's own ground, so the ground is asked of a Normandy with a river across it.
describe('a refused placing names the ground that bars the unit', () => {
  it('says the water, a bridge, or nothing on ground a unit may stand on', () => {
    const t = ticketOf(PIECES[0], 11, 'att');
    t.place = { ...t.place, river: 'Vire' };
    const l = liveTicket(t, { me: 'att', setup: true });
    const f = l.sim.field, mine = l.sim.units.filter((u) => u.side === 'att');
    const small = mine.reduce((a, b) => (a.w * a.d <= b.w * b.d ? a : b));
    const cells = (word: string): [number, number][] => {
      const out: [number, number][] = [];
      for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) if (GROUNDS[f.ground[r * f.cols + c]] === word) out.push([(c + 0.5) * f.cell, (r + 0.5) * f.cell]);
      return out;
    };
    const water = cells('water'), bridges = cells('bridge');
    expect(water.length).toBeGreaterThan(50);
    expect(bridges.length).toBeGreaterThan(0);
    for (const [x, y] of bridges) expect(l.barredBy(small.id, x, y)).toBe('bridge');
    // (water with no bridge under any of the unit's box)
    const clear = water.filter(([x, y]) => bridges.every(([bx, by]) => Math.abs(bx - x) > small.w / 2 + f.cell && Math.abs(by - y) > small.d / 2 + f.cell));
    expect(clear.length).toBeGreaterThan(10);
    for (const [x, y] of clear) expect(l.barredBy(small.id, x, y)).toBe('water');
    // (where it stands now it may stand: nothing bars it; and no such unit, or no such place, is nothing)
    expect(l.barredBy(small.id, small.x, small.y)).toBeNull();
    expect(l.barredBy(-7, water[0][0], water[0][1])).toBeNull();
    expect(l.barredBy(small.id, Number.NaN, 100)).toBeNull();
  });

  it('has a whole sentence for each ground, and none that names nothing', () => {
    const src: string = bridgeTs;
    expect(src).toMatch(/const BARRED = \{ water: 'in the water', bridge: 'on a bridge', marsh: 'in a marsh', rock: 'on bare rock', bunker: 'on a bunker' \} as const;/);
    expect(src).toContain("`${name} cannot go there: it cannot stand ${on ? BARRED[on] : 'on water or on a bridge'}.`");
    expect(src).not.toContain('ground like that');
  });
});
