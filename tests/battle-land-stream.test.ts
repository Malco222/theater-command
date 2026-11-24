// A capital's or a city's own river through its streets (Brian, 18 November: the battle's ground matches the map; the
// design director, b526): the hand list of real cities, how the river lies to the attack, never between the armies, its
// bridges by the record (b596), one river a field, and the rest of the field as it was.
import { describe, expect, it } from 'vitest';
import { CITY_RIVERS, GROUNDS, LAND, LandBattle, bearingOf, buildField, sampleTicket, streamFor, streamOf, type LandField, type Stream } from '../src/battle/land';

const kind = (f: LandField, c: number, r: number): string => GROUNDS[f.ground[r * f.cols + c]];
const wet = (k: string): boolean => k === 'water' || k === 'bridge' || k === 'blown';
const stream = (lie: Stream['lie'], o: Partial<Stream> = {}): Stream => ({ name: 'Test', lie, flows: lie === 'across' ? 'east' : 'north', great: true, ...(lie === 'shore' ? { side: 'west' } : {}), ...o });
const cityTicket = (size: 'city' | 'capital', terrain: 'urban' | 'plains' | 'hills' | 'forest', seed: number, s?: Stream) => {
  const t = sampleTicket(terrain, 1.2, { town: size, seed });
  if (s) t.place.stream = s;
  return t;
};
const LIES: [string, Stream][] = [
  ['up', stream('up')], ['up, a second meeting it', stream('up', { flows: 'south', join: { name: 'Second', side: 'west' } })],
  ['across', stream('across')], ['across, a second meeting it', stream('across', { flows: 'west', join: { name: 'Second', side: 'east' } })],
  ['across, a second from the west', stream('across', { join: { name: 'Second', side: 'west' } })],
  ['a shore', stream('shore')], ['a lesser river up the field', stream('up', { great: false })],
  ['a shore ahead, behind the defence', stream('shore', { side: 'back', flows: 'east' })],
];

describe("the hand list follows the real city (b526)", () => {
  it('the four the ruling names: the Dnieper through Kiev, the Danube through Budapest, the Sava and Danube meeting at Belgrade, the Tagus a shore at Lisbon', () => {
    expect(CITY_RIVERS.Kiev).toMatchObject({ river: 'Dnieper', form: 'through' });
    expect(CITY_RIVERS.Budapest).toMatchObject({ river: 'Danube', form: 'through' });
    expect(CITY_RIVERS.Belgrade).toMatchObject({ river: 'Danube', form: 'meet', join: 'Sava' });
    expect(CITY_RIVERS.Lisbon).toMatchObject({ river: 'Tagus', form: 'shore' });
  });
  it('every entry is whole: its way, a meeting its second, a shore its water', () => {
    for (const [town, r] of Object.entries(CITY_RIVERS)) {
      expect(r.river, town).toBeTruthy();
      expect(['through', 'meet', 'shore'], town).toContain(r.form);
      expect(r.flows >= 0 && r.flows < 360, town).toBe(true);
      if (r.form === 'meet') expect(!!r.join && r.joinFrom! >= 0 && r.joinFrom! < 360, town).toBe(true);
      if (r.form === 'shore') expect(r.water! >= 0 && r.water! < 360, town).toBe(true);
    }
  });
});

describe('how the river lies to the attack: never between the armies (b526)', () => {
  it('within 45 degrees of the attack it runs up the field; any other way across it, behind the defence', () => {
    // Budapest's Danube runs south: attacked from the east (heading west) it lies across, from the north up the field.
    expect(streamOf(CITY_RIVERS.Budapest, 270)).toMatchObject({ lie: 'across', flows: 'west' });
    expect(streamOf(CITY_RIVERS.Budapest, 180)).toMatchObject({ lie: 'up', flows: 'north' });
    expect(streamOf(CITY_RIVERS.Budapest, 0)).toMatchObject({ lie: 'up', flows: 'south' });
    // Warsaw from the west, September 1939: the Vistula behind the defence, Praga beyond it.
    expect(streamFor('Warsaw', bearingOf(0, 0, 10, 0))).toMatchObject({ name: 'Vistula', lie: 'across' });
    // Belgrade from the south: the Danube across, the Sava coming in from the west.
    expect(streamOf(CITY_RIVERS.Belgrade, 0)).toMatchObject({ lie: 'across', flows: 'east', join: { name: 'Sava', side: 'west' } });
    // Lisbon from the east: the Tagus along the field's west side, the attack's left.
    expect(streamOf(CITY_RIVERS.Lisbon, 250)).toMatchObject({ lie: 'shore', side: 'west' });
    expect(streamFor('Millbrook', 0)).toBeUndefined();
    // A shore straight ahead of the attack lies behind the defence (b595): Stalingrad from the west, the Volga at its
    // back; from the east, the water behind the attack, along a side edge as before.
    expect(streamOf(CITY_RIVERS.Stalingrad, 90)).toMatchObject({ lie: 'shore', side: 'back' });
    expect(streamOf(CITY_RIVERS.Stalingrad, 270).side).not.toBe('back');
    // Each city's own road bridges, by the historian's record (b596): Belgrade one over each river, Gorky none over the Volga.
    expect(streamOf(CITY_RIVERS.Belgrade, 0)).toMatchObject({ bridges: 1, join: { bridges: 1 } });
    expect(streamOf(CITY_RIVERS.Gorky, 0).bridges).toBe(0);
  });
  it('the map\'s y runs south: a bearing is a compass bearing', () => {
    expect(bearingOf(0, 0, 0, -1)).toBeCloseTo(0);
    expect(bearingOf(0, 0, 1, 0)).toBeCloseTo(90);
    expect(bearingOf(0, 0, 0, 1)).toBeCloseTo(180);
    expect(bearingOf(0, 0, -1, 0)).toBeCloseTo(270);
  });
  it('east and west are the ground\'s true width at the latitude: the charts draw a degree of longitude as at 50 N', () => {
    // 40 to a degree of latitude from 71.4 N: at 50 N one step north-east is 45 degrees; at 60 N, where a degree of
    // longitude is narrower than the chart draws it, the same step is nearer north.
    const at = (lat: number): number => (71.4 - lat) * 40;
    expect(bearingOf(0, at(50), 1, at(50) - 1)).toBeCloseTo(45, 1);
    expect(bearingOf(0, at(60), 1, at(60) - 1)).toBeCloseTo((Math.atan(Math.cos(Math.PI / 3) / Math.cos((50 * Math.PI) / 180)) * 180) / Math.PI, 1);
  });
  it('keyed by the town as the game names it: Seoul (Keijo under Japan), Molotov (Perm until 1940) once', () => {
    expect(CITY_RIVERS.Seoul).toMatchObject({ river: 'Han', form: 'shore' });
    expect(CITY_RIVERS.Keijo).toBeUndefined();
    expect(CITY_RIVERS.Perm).toBeUndefined();
    expect(CITY_RIVERS.Molotov).toMatchObject({ river: 'Kama', form: 'shore' });
  });
});

describe("the city's river on the field (b526)", () => {
  it('one river a field: where the card prices a crossing, that is the river and the city adds none', () => {
    const t = sampleTicket('urban', 1.2, { town: 'city', river: true, seed: 3 });
    const plain = buildField(t.place, t.seed);
    t.place.stream = stream('across');
    const f = buildField(t.place, t.seed);
    expect(f.stream).toBeUndefined();
    expect(f.ground).toEqual(plain.ground);
  });
  it('a town or a village has none: only a capital or a city is fought in its streets', () => {
    const t = sampleTicket('plains', 1.2, { town: 'town', seed: 3 });
    t.place.stream = stream('across');
    expect(buildField(t.place, t.seed).stream).toBeUndefined();
  });
  it('the rest of the field is as it was: away from the river, every cell is its twin\'s', () => {
    for (const [, s] of LIES) {
      const t = cityTicket('city', 'urban', 11);
      const twin = buildField(t.place, t.seed);
      t.place.stream = s;
      const f = buildField(t.place, t.seed);
      const lines = [f.stream!.course, ...(f.stream!.join ? [f.stream!.join.course] : [])];
      const near = (x: number, y: number): boolean => lines.some((l) => l.some(([px, py]) => Math.hypot(px - x, py - y) < 260)) || (f.stream!.lie === 'shore' && (x < 400 || x > f.w - 400));
      let same = 0, far = 0;
      for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
        if (near(c * f.cell + f.cell / 2, r * f.cell + f.cell / 2)) continue;
        far++;
        const i = r * f.cols + c;
        if (f.ground[i] === twin.ground[i]) same++;
        // Its ground may take a road the water's bridges lead on to; its lie of the land and its houses never change.
        expect(f.height[i]).toBe(twin.height[i]);
        expect(f.storeys?.[i]).toBe(twin.storeys?.[i]);
      }
      expect(same / far).toBeGreaterThan(0.995);
    }
  });
  it('the same city and seed give the same river', () => {
    const t = cityTicket('capital', 'plains', 5, stream('across', { join: { name: 'Second', side: 'east' } }));
    expect(buildField(t.place, t.seed)).toEqual(buildField(t.place, t.seed));
  });
  it('it lies as the ruling has it: up the field through the core, across behind the defence, a shore at the side', () => {
    for (const size of ['city', 'capital'] as const) for (let seed = 1; seed <= 4; seed++) {
      for (const [name, s] of LIES) {
        const b = new LandBattle(cityTicket(size, 'urban', seed * 37, s));
        const f = b.field, St = f.stream!;
        expect(St.lie, name).toBe(s.lie);
        const water: [number, number][] = [];
        for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) if (kind(f, c, r) === 'water') water.push([c * f.cell + f.cell / 2, r * f.cell + f.cell / 2]);
        expect(water.length, name).toBeGreaterThan(150);
        const def = b.units.filter((u) => u.side === 'def'), att = b.units.filter((u) => u.side === 'att');
        const from = (pts: [number, number][], [x, y]: [number, number]): number => Math.min(...pts.map(([px, py]) => Math.hypot(px - x, py - y)));
        if (s.lie === 'across' || St.side === 'back') {
          // All of its water behind the defence's rearmost row, and nowhere near the attack.
          const back = Math.max(...def.map((u) => u.y + u.d / 2));
          const own = St.join ? water.filter((p) => from(St.course, p) <= from(St.join!.course, p)) : water;
          expect(Math.min(...own.map((p) => p[1])), name).toBeGreaterThan(back + 40);
        }
        if (s.lie === 'across' && St.join) {
          // A second comes in from its side edge beyond the defence's flank (b596): every defending block on the heart's
          // side of it, and all its water beyond the attack's front.
          const J = St.join, xs = J.course.map((p) => p[0]).filter((x) => x >= 0 && x <= f.w), west = J.course[0][0] < f.w / 2;
          expect(J.course[0][0] < 0 || J.course[0][0] > f.w, name).toBe(true);
          for (const u of def) expect(west ? u.x - u.w / 2 > Math.max(...xs) : u.x + u.w / 2 < Math.min(...xs), `${name} ${u.name}`).toBe(true);
          expect(Math.min(...J.course.map((p) => p[1])), name).toBeGreaterThan(Math.max(...att.map((u) => u.y + u.d / 2)) + 300);
        }
        if (s.lie === 'up') {
          // It runs the field's length beside the heart, both armies on the heart's bank: never between them.
          expect(Math.min(...St.course.map((p) => p[1]))).toBeLessThan(0);
          expect(Math.max(...St.course.map((p) => p[1]))).toBeGreaterThan(f.h);
          const xAt = (y: number): number => St.course.reduce((m, p) => (Math.abs(p[1] - y) < Math.abs(m[1] - y) ? p : m))[0];
          const heartWest = f.town!.x < xAt(f.town!.y);
          for (const side of [att, def]) expect(side.every((u) => (u.x < xAt(u.y)) === heartWest), name).toBe(true);
        }
        if (s.lie === 'shore' && St.side !== 'back') expect(Math.max(...water.map((p) => p[0])), name).toBeLessThan(f.zones.att[0]);
      }
    }
  });
  it('no block is drawn up in the water or a stone\'s throw from it, on either side', () => {
    for (const size of ['city', 'capital'] as const) for (const terrain of ['urban', 'plains', 'hills', 'forest'] as const) for (let seed = 1; seed <= 3; seed++) for (const [name, s] of LIES) {
      const b = new LandBattle(cityTicket(size, terrain, seed * 101 + 7, s));
      const f = b.field;
      const at = (x: number, y: number): string => kind(f, Math.min(f.cols - 1, Math.max(0, Math.floor(x / f.cell))), Math.min(f.rows - 1, Math.max(0, Math.floor(y / f.cell))));
      for (const u of b.units) {
        for (let dx = -u.w / 2 - 60; dx <= u.w / 2 + 60; dx += 20) for (let dy = -u.d / 2 - 60; dy <= u.d / 2 + 60; dy += 20) expect(wet(at(u.x + dx, u.y + dy)), `${name} ${size} ${terrain} ${seed} ${u.name}`).toBe(false);
        expect(u.side === 'att' ? u.y <= f.zones.att[3] : u.y >= f.zones.def[1], `${name} ${size} ${terrain} ${seed} ${u.name} in its zone`).toBe(true);
      }
    }
  });
  it('beside a river up the field a block keeps its place in its row: never stepped out in front of its zone (b526 review)', () => {
    // A city, the defence one infantry division: its row stood flush against its lane's edge, and the stone's throw
    // stepped its first regiment 80 m forward, out of its zone, and still beside the water.
    const t = sampleTicket('urban', 1.2, { town: 'city', seed: 10726 });
    const proto = t.def.forces[0];
    t.def.forces = [{ ...proto, corps: 11, name: 'C0', kind: 'inf', divs: 1, men: 15000 * proto.str, share: 1, first: 1 }];
    t.place.stream = stream('up');
    const b = new LandBattle(t), f = b.field;
    const at = (x: number, y: number): string => kind(f, Math.min(f.cols - 1, Math.max(0, Math.floor(x / f.cell))), Math.min(f.rows - 1, Math.max(0, Math.floor(y / f.cell))));
    for (const u of b.units) {
      expect(u.side === 'att' ? u.y <= f.zones.att[3] : u.y >= f.zones.def[1], u.name).toBe(true);
      for (let dx = -u.w / 2 - 60; dx <= u.w / 2 + 60; dx += 10) for (let dy = -u.d / 2 - 60; dy <= u.d / 2 + 60; dy += 10) expect(wet(at(u.x + dx, u.y + dy)), u.name).toBe(false);
    }
  });
  it('a bridge on every avenue: the roads either side of it still meet, over its bridges', () => {
    for (const size of ['city', 'capital'] as const) for (let seed = 1; seed <= 4; seed++) for (const [name, s] of LIES) {
      if (s.lie === 'shore') continue;
      const t0 = cityTicket(size, 'urban', seed * 53);
      const f = buildField(t0.place, t0.seed);
      const t = cityTicket(size, 'urban', seed * 53, s);
      const g = buildField(t.place, t.seed);
      expect(g.ground.filter((k) => GROUNDS[k] === 'bridge').length, name).toBeGreaterThan(0);
      // Walk the road from where it comes on at the attacker's edge, over road and bridges only: it reaches every
      // stretch of road that was joined to it before the river was laid, but for what the water took.
      const reach = (h: LandField): Set<number> => {
        const deck = (i: number): boolean => GROUNDS[h.ground[i]] === 'road' || GROUNDS[h.ground[i]] === 'bridge';
        const start = [...Array(h.cols).keys()].find((c) => deck(c))!;
        const seen = new Set([start]), todo = [start];
        while (todo.length) {
          const i = todo.pop()!;
          for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
            const c = (i % h.cols) + dc, r = Math.floor(i / h.cols) + dr, j = r * h.cols + c;
            if (c < 0 || r < 0 || c >= h.cols || r >= h.rows || seen.has(j) || !deck(j)) continue;
            seen.add(j); todo.push(j);
          }
        }
        return seen;
      };
      const before = reach(f), after = reach(g);
      // The road up the field to the far edge, and the road across it to either side, are still one road.
      const far = [...after].some((i) => Math.floor(i / g.cols) === g.rows - 1);
      const sides = [...after].some((i) => i % g.cols === 0) && [...after].some((i) => i % g.cols === g.cols - 1);
      expect(far, `${name} ${size} ${seed}`).toBe([...before].some((i) => Math.floor(i / f.cols) === f.rows - 1));
      expect(sides, `${name} ${size} ${seed}`).toBe([...before].some((i) => i % f.cols === 0) && [...before].some((i) => i % f.cols === f.cols - 1));
      // And most of the road still joined: the river takes what it covers, and no more.
      const kept = [...before].filter((i) => after.has(i) || GROUNDS[g.ground[i]] === 'water').length;
      expect(kept / before.size, `${name} ${size} ${seed}`).toBeGreaterThan(0.9);
    }
  });
  it('no stretch of road is cut off: a road over it slantwise, two avenues meeting at the water, one dipping into it', () => {
    // The fields where the review found road cut off by the water (b526 review): a crossing dropped beside another's
    // bridge, a slanted avenue left unpaired, a road the water cut along one bank, a second river along an avenue.
    const cases: [string, 'urban' | 'plains', 'city' | 'capital', number, Stream, number][] = [
      ['across, beside another bridge', 'urban', 'city', 1219, stream('across'), 0],
      ['a lesser across, beside another bridge', 'urban', 'city', 2923, stream('across', { great: false }), 0],
      ['across, slantwise', 'urban', 'city', 263, stream('across'), 0],
      ['up, slantwise', 'urban', 'capital', 132, stream('up'), 0],
      ['up, dipping in and out', 'plains', 'capital', 1856, stream('up'), 0],
      ['up, a second meeting it', 'plains', 'capital', 3214, stream('up', { join: { name: 'Second', side: 'east' } }), 0],
      // The final review (b526): two avenues crossing in the water, bridged pair by pair and no longer joined; an avenue
      // over the first river's bridge and on over a second, which gave it no bridge of its own.
      ['across, two avenues crossing in the water', 'urban', 'capital', 182150, stream('across'), 0],
      ['up, four roads meeting in the water', 'urban', 'capital', 4608083, stream('up'), 0],
      ['up, over the first bridge and on over a second', 'urban', 'capital', 7959411, stream('up', { join: { name: 'Second', side: 'east' } }), 0],
      ['across, over the first bridge and on over a second', 'urban', 'capital', 134636, stream('across', { join: { name: 'Second', side: 'west' } }), 0],
      // The b596 review: a second river from the west edge met the road across the field at a shallow slant, its two
      // ends further apart than a bridge paired, and the road from the west edge was cut off.
      ['across, a second at a slant over the road across', 'urban', 'city', -4163177, stream('across', { join: { name: 'Second', side: 'west' } }), 0],
      ['across, a second at a slant (2)', 'urban', 'city', 4722859, stream('across', { join: { name: 'Second', side: 'west' } }), 0],
      ['across, a second at a slant (3)', 'urban', 'city', 223651, stream('across', { join: { name: 'Second', side: 'west' } }), 0],
    ];
    const reach = (h: LandField): Set<number> => {
      const deck = (i: number): boolean => GROUNDS[h.ground[i]] === 'road' || GROUNDS[h.ground[i]] === 'bridge';
      const start = [...Array(h.cols).keys()].find((c) => deck(c))!;
      const seen = new Set([start]), todo = [start];
      while (todo.length) {
        const i = todo.pop()!;
        for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
          const c = (i % h.cols) + dc, r = Math.floor(i / h.cols) + dr, j = r * h.cols + c;
          if (c < 0 || r < 0 || c >= h.cols || r >= h.rows || seen.has(j) || !deck(j)) continue;
          seen.add(j); todo.push(j);
        }
      }
      return seen;
    };
    for (const [name, terrain, size, seed, s, most] of cases) {
      const t = sampleTicket(terrain, 1.2, { town: size, seed });
      const f = buildField(t.place, t.seed);
      t.place.stream = s;
      const g = buildField(t.place, t.seed), before = reach(f), after = reach(g);
      const cut = [...before].filter((i) => (GROUNDS[g.ground[i]] === 'road' || GROUNDS[g.ground[i]] === 'bridge') && !after.has(i)).length;
      expect(cut, name).toBeLessThanOrEqual(most);
    }
  });
  it('a second river with no road over it has its one bridge, over its own water and not the first one\'s', () => {
    const t = sampleTicket('urban', 1.2, { town: 'city', seed: 7932 });
    t.place.stream = stream('across', { join: { name: 'Second', side: 'east' } });
    const f = buildField(t.place, t.seed), J = f.stream!.join!;
    const from = (pts: [number, number][], x: number, y: number): number => Math.min(...pts.map(([px, py]) => Math.hypot(px - x, py - y)));
    let deck = 0;
    for (let r = 0; r < f.rows; r++) for (let c = 0; c < f.cols; c++) {
      const x = c * f.cell + f.cell / 2, y = r * f.cell + f.cell / 2;
      if (kind(f, c, r) === 'bridge' && from(J.course, x, y) < from(f.stream!.course, x, y)) deck++;
    }
    expect(deck).toBeGreaterThan(0);
  });
  it('bridges by the record (b596): no more than the city had, those nearest the heart, and none where it had none', () => {
    const decks = (f: LandField): [number, number][][] => {
      // Each bridge: its deck cells, joined.
      const seen = new Set<number>(), out: [number, number][][] = [];
      for (let i0 = 0; i0 < f.ground.length; i0++) {
        if (seen.has(i0) || GROUNDS[f.ground[i0]] !== 'bridge') continue;
        const todo = [i0], deck: [number, number][] = [];
        seen.add(i0);
        while (todo.length) {
          const i = todo.pop()!;
          deck.push([(i % f.cols) * f.cell + f.cell / 2, Math.floor(i / f.cols) * f.cell + f.cell / 2]);
          for (let dr = -1; dr <= 1; dr++) for (let dc = -1; dc <= 1; dc++) {
            const c = (i % f.cols) + dc, r = Math.floor(i / f.cols) + dr, j = r * f.cols + c;
            if (c < 0 || r < 0 || c >= f.cols || r >= f.rows || seen.has(j) || GROUNDS[f.ground[j]] !== 'bridge') continue;
            seen.add(j); todo.push(j);
          }
        }
        out.push(deck);
      }
      return out;
    };
    for (const [name, s] of LIES) {
      if (s.lie === 'shore' || s.join) continue;
      for (const seed of [53, 106, 159]) {
        const all = buildField(cityTicket('capital', 'urban', seed, s).place, seed);
        const two = buildField(cityTicket('capital', 'urban', seed, { ...s, bridges: 2 }).place, seed);
        const none = buildField(cityTicket('capital', 'urban', seed, { ...s, bridges: 0 }).place, seed);
        expect(decks(all).length, name).toBeGreaterThan(2);
        expect(decks(two).length, name).toBeGreaterThanOrEqual(1);
        expect(decks(two).length, name).toBeLessThanOrEqual(2);
        expect(decks(none).length, name).toBe(0);
        // Those it keeps are the nearest the heart of those it would lay.
        const near = (d: [number, number][]): number => Math.min(...d.map(([x, y]) => Math.hypot(x - all.town!.x, y - all.town!.y)));
        const kept = decks(two).map(near).sort((a, b) => a - b), every = decks(all).map(near).sort((a, b) => a - b);
        kept.forEach((k, i) => expect(k, `${name} ${seed} deck ${i}`).toBeLessThanOrEqual(every[i] + 25));
      }
    }
  });
  it('a narrow defence with its guns keeps clear of the water too (the front the river kept behind)', () => {
    const mk = (seed: number, def: [string, number][], s: Stream) => {
      const t = sampleTicket('urban', 1.2, { town: 'capital', seed });
      const proto = t.def.forces[0];
      t.def.forces = def.map(([k, divs], i) => ({ ...proto, corps: 11 + i, name: `C${i}`, kind: k as typeof proto.kind, divs, men: divs * 15000 * proto.str, share: 1 / def.length, first: 1 }));
      t.place.stream = s;
      return t;
    };
    for (const def of [[['arm', 1], ['art', 2]], [['mot', 2], ['art', 1]], [['inf', 3], ['art', 1]]] as [string, number][][]) for (const seed of [16935, 16999, 17077]) for (const s of [stream('across'), stream('up'), stream('up', { join: { name: 'Second', side: 'west' } })]) {
      const b = new LandBattle(mk(seed, def, s));
      const f = b.field;
      const at = (x: number, y: number): string => kind(f, Math.min(f.cols - 1, Math.max(0, Math.floor(x / f.cell))), Math.min(f.rows - 1, Math.max(0, Math.floor(y / f.cell))));
      for (const u of b.units) for (let dx = -u.w / 2 - 60; dx <= u.w / 2 + 60; dx += 10) for (let dy = -u.d / 2 - 60; dy <= u.d / 2 + 60; dy += 10) expect(wet(at(u.x + dx, u.y + dy)), `${s.lie} ${seed} ${u.name}`).toBe(false);
    }
  });
  it('a battle by a city river is fought to its end, and none is left standing in the water', () => {
    for (const [name, s] of LIES.slice(0, 4)) for (const seed of [3, 4]) {
      const t = cityTicket('city', 'plains', seed * 211, s);
      const b = new LandBattle({ ...t, att: { ...t.att, human: false } });
      for (let i = 0; i < 1e5 && !b.over; i++) b.step();
      expect(b.over, name).toBeTruthy();
      for (const u of b.units) {
        if (u.state === 'gone' || u.state === 'withdrawn') continue;
        const k = kind(b.field, Math.min(b.field.cols - 1, Math.floor(u.x / b.field.cell)), Math.min(b.field.rows - 1, Math.floor(u.y / b.field.cell)));
        expect(k === 'water' || k === 'blown', `${name} ${seed} ${u.name}`).toBe(false);
      }
    }
  });
  it("a defence whose line ends on the water is held as the river's (b526, b596(2)): up the field hold.stream, beside a second river hold.join", () => {
    const at = (s?: Stream): { on: boolean; k: number } => {
      const b = new LandBattle(cityTicket('city', 'urban', 4242, s)) as unknown as { flankOnRiver: boolean; flankHold: number };
      return { on: b.flankOnRiver, k: b.flankHold };
    };
    expect(at()).toEqual({ on: false, k: 1 });
    expect(at(stream('across'))).toEqual({ on: false, k: 1 });
    expect(at(stream('across', { flows: 'west', join: { name: 'Second', side: 'east', bridges: 1 }, bridges: 2 }))).toEqual({ on: true, k: LAND.hold.join });
    const up = at(stream('up', { bridges: 3 }));
    expect(up.k).toBe(up.on ? LAND.hold.stream : 1);
    expect(LAND.hold.join).toBeLessThan(LAND.hold.stream);
  });
  it("a defence's runner caught in the water at the end is booked as fled, an attack's runner in it as before (b608(3))", () => {
    const b = new LandBattle(cityTicket('city', 'urban', 4242, stream('shore', { side: 'back', flows: 'east' })));
    const f = b.field, water = f.ground.findIndex((k) => GROUNDS[k] === 'water');
    const wx = (water % f.cols) * f.cell + f.cell / 2, wy = Math.floor(water / f.cols) * f.cell + f.cell / 2;
    const lost = (u: (typeof b.units)[number], x: number, y: number): number => { [u.x, u.y] = [x, y]; return b.lostShares()[u.side]; };
    for (const side of ['def', 'att'] as const) {
      const u = b.units.find((v) => v.side === side && v.kind !== 'fort')!, [x0, y0] = [u.x, u.y];
      u.state = 'routing';
      const dry = lost(u, x0, y0), wet = lost(u, wx, wy);
      const share = (r: number): number => (u.power0 * r * u.str) / u.str0 / (b as unknown as { power: Record<string, number> }).power[side];
      if (side === 'def') expect(wet - dry).toBeCloseTo(share(LAND.count.gone.def - LAND.count.routing.def), 9);
      else expect(wet).toBeCloseTo(dry, 12);
      [u.x, u.y] = [x0, y0]; u.state = 'steady';
    }
  });
});

describe('a city along its shore as a strip (the art director, 19 November)', () => {
  const back = stream('shore', { side: 'back', flows: 'east' });
  const built = (k: string): boolean => k === 'buildings' || k === 'suburb' || k === 'industry' || k === 'park';
  it('Stalingrad is one, its strip only where its Volga lies behind the defence', () => {
    expect(CITY_RIVERS.Stalingrad.strip).toBe(true);
    expect(streamOf(CITY_RIVERS.Stalingrad, 78)).toMatchObject({ lie: 'shore', side: 'back', strip: true });
    expect(streamOf(CITY_RIVERS.Stalingrad, 198).strip).toBeUndefined();
    expect(streamOf(CITY_RIVERS.Saratov, 117).strip).toBeUndefined();
  });
  it('strip cities by the design director (b619): marked only where the historian finds one bank and about 4 to 1', () => {
    const marked = Object.entries(CITY_RIVERS).filter(([, r]) => r.strip).map(([n]) => n).sort();
    expect(marked).toEqual(['Khabarovsk', 'Krasnoyarsk', 'Molotov', 'Stalingrad', 'Tbilisi']);
    for (const n of ['Saratov', 'New York', 'Chungking', 'Zaporozhye']) if (CITY_RIVERS[n]) expect(CITY_RIVERS[n].strip, n).toBeUndefined();
    // A shore city shows its strip only with its water straight ahead of the attack (behind the defence).
    expect(streamOf(CITY_RIVERS.Molotov, 330)).toMatchObject({ lie: 'shore', side: 'back', strip: true });
    expect(streamOf(CITY_RIVERS.Khabarovsk, 270)).toMatchObject({ lie: 'shore', side: 'back', strip: true });
    expect(streamOf(CITY_RIVERS.Molotov, 150).strip).toBeUndefined();
    // A river through the city lies up or across, never as a shore: the mark is carried, not drawn.
    for (const n of ['Tbilisi', 'Krasnoyarsk']) for (let hd = 0; hd < 360; hd += 15) expect(streamOf(CITY_RIVERS[n], hd).strip, `${n} ${hd}`).toBeUndefined();
  });
  it('its streets run the whole width along the water and reach only a little way inland; a round town leaves open flanks', () => {
    for (const seed of [11, 12, 13, 14]) {
      const strip = buildField(cityTicket('city', 'urban', seed, { ...back, strip: true }).place, seed);
      const round = buildField(cityTicket('city', 'urban', seed, back).place, seed);
      const edge = strip.streetEdge!, flank = (f: LandField): number => [0, 1, 2, f.cols - 3, f.cols - 2, f.cols - 1].filter((c) => (f.streetEdge?.[c] ?? -1) >= 0).length;
      expect(edge.filter((y) => y >= 0).length / strip.cols, `${seed}`).toBeGreaterThan(0.95);
      expect(flank(strip), `${seed}`).toBe(6);
      expect(flank(round), `${seed}`).toBe(0);
      // Nothing built more than about 1100 m inland of the field's back edge, the water's side.
      for (let r = 0; r < strip.rows; r++) for (let c = 0; c < strip.cols; c++) if (built(kind(strip, c, r))) expect(strip.h - r * strip.cell, `${seed} ${c},${r}`).toBeLessThan(1300);
      // Its heart is in its streets, and no block is drawn up in the water.
      expect(strip.landmark && strip.landmark.y > strip.h - 700).toBe(true);
      const b = new LandBattle(cityTicket('city', 'urban', seed, { ...back, strip: true }));
      for (const u of b.units) expect(kind(b.field, Math.floor(u.x / b.field.cell), Math.floor(u.y / b.field.cell)) === 'water', `${seed} ${u.name}`).toBe(false);
    }
  });
  it('a capital, or a shore at the side, is laid as before', () => {
    for (const s of [back, stream('shore')]) {
      const t = cityTicket('capital', 'plains', 77, s), u = cityTicket('capital', 'plains', 77, { ...s, strip: true });
      expect(buildField(u.place, 77).ground).toEqual(buildField(t.place, 77).ground);
    }
    const side = cityTicket('city', 'urban', 78, stream('shore')), sideS = cityTicket('city', 'urban', 78, { ...stream('shore'), strip: true });
    expect(buildField(sideS.place, 78).ground).toEqual(buildField(side.place, 78).ground);
  });
});
