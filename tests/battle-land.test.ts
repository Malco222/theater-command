// The land battle (src/battle/land): the field from the province, blocks from divisions balanced to the attack card,
// orders, the end, replay, the fairness test against the dice (ruling 217(3)), and the hand-back to the campaign
// through the rules' own attack order. The design director's rulings 217 and 217(2)(a) of 2026-11-17.
import { describe, expect, it } from 'vitest';
import { diceChance, runOut, type BreakCause } from '../src/battle/core';
import { BEND, buildField, CITY_PLANS, cardKnows, corpsOn, coverAt, groundAt, GROUNDS, LAND, LandBattle, MOTION, PRACTICE_GROUNDS, blocksFor, handBack, landTicket, practiceReckoning, practiceTicket, pursuitLine, sampleTicket, goingIn, planKey, streetsOf, wornWords, type LandPlace, type LandUnit, type PracticeGround } from '../src/battle/land';
import { ENGINEERING, riverBetween } from '../src/campaign/rivers';
import { createGame, hostileUnits, type FieldBill } from '../src/campaign/game';
import { blocked } from '../src/campaign/rivers';
import { startOpen } from '../src/campaign/staff';
import { townName, townSize } from '../src/campaign/towns';
import { guardForce, guardMen } from '../src/campaign/siege';
import { attachWorld } from '../src/campaign/world';
import { beginHuman, candidates, humanUp, orderAttack, previewAttack, stepAI } from '../src/campaign/turn';
import { FORT_MUL, type Corps, type Game, type Region } from '../src/campaign/types';

const count = (f: ReturnType<typeof buildField>, k: (typeof GROUNDS)[number]): number => f.ground.filter((g) => GROUNDS[g] === k).length;

describe('the field is built from the province', () => {
  it('the same province and seed give the same field', () => {
    const t = sampleTicket('hills', 1.2, { river: true });
    expect(buildField(t.place, 5)).toEqual(buildField(t.place, 5));
    expect(buildField(t.place, 5).ground).not.toEqual(buildField(t.place, 6).ground);
  });
  it('a river has its bridge, blown when the campaign says the bridges are down', () => {
    const up = buildField(sampleTicket('plains', 1, { river: true }).place, 3);
    expect(count(up, 'water')).toBeGreaterThan(100);
    expect(count(up, 'bridge')).toBeGreaterThan(0);
    const down = buildField(sampleTicket('plains', 1, { river: true, bridgeDown: true }).place, 3);
    expect(count(down, 'bridge')).toBe(0);
    expect(count(down, 'blown')).toBeGreaterThan(0);
  });
  it('the ground follows the terrain: woods in forest, rock in mountains, marsh in marsh, streets in a city, works for a fortress', () => {
    const f = (t: Parameters<typeof sampleTicket>[0], o = {}) => buildField(sampleTicket(t, 1, o).place, 9);
    expect(count(f('forest'), 'woods')).toBeGreaterThan(count(f('plains'), 'woods') * 2);
    expect(count(f('mountain'), 'rock')).toBeGreaterThan(50);
    expect(count(f('marsh'), 'marsh')).toBeGreaterThan(1000);
    expect(count(f('urban'), 'buildings')).toBeGreaterThan(count(f('plains'), 'buildings') * 2);
    expect(count(f('plains', { fort: 2 }), 'bunker')).toBeGreaterThan(0);
  });
  it('a capital or a city is fought in its streets, a town or a village about its houses as before (Brian; b511)', () => {
    // Brian, 18 November: "if are attacking in hills than have that or a forrest or towns ect"; the design director:
    // the share of streets follows the city's size on the map.
    const built = (fl: ReturnType<typeof buildField>) => (count(fl, 'buildings') + count(fl, 'suburb') + count(fl, 'industry')) / fl.ground.length;
    const at = (t: Parameters<typeof sampleTicket>[0], town: LandPlace['townSize']) => buildField(sampleTicket(t, 1, { town }).place, 9);
    for (const seed of [1, 2, 3]) {
      const cap = buildField(sampleTicket('urban', 1, { town: 'capital' }).place, seed);
      expect(built(cap)).toBeGreaterThan(0.6);
      // Its core fills the defender's part of the field; its suburbs reach back to the attacker's start.
      const row = (r: number) => cap.ground.slice(r * cap.cols, (r + 1) * cap.cols).map((k) => GROUNDS[k]);
      expect(row(Math.round(cap.rows * 0.8)).filter((k) => k === 'buildings').length).toBeGreaterThan(cap.cols * 0.6);
      expect(row(8).filter((k) => k === 'suburb').length).toBeGreaterThan(cap.cols * 0.3);
      expect(cap.landmark).toBeDefined();
      const city = buildField(sampleTicket('urban', 1, { town: 'village' }).place, seed);
      expect(built(city)).toBeGreaterThan(0.22);
      expect(built(city)).toBeLessThan(0.45);
    }
    // A capital in hill country is a capital's streets all the same; a town or a village keeps its cluster.
    expect(built(at('hills', 'capital'))).toBeGreaterThan(0.6);
    expect(built(at('hills', 'town'))).toBeLessThan(0.05);
    expect(built(at('plains', 'village'))).toBeLessThan(0.02);
    // Every built cell has its storeys, and nothing else has any: three to five in a core.
    const cap = at('urban', 'capital');
    cap.ground.forEach((k, i) => expect(cap.storeys![i] > 0).toBe(['buildings', 'suburb', 'industry'].includes(GROUNDS[k])));
    expect(Math.max(...cap.storeys!)).toBe(5);
    // The new grounds come after the old, whose codes saved fields keep.
    expect(GROUNDS.slice(13)).toEqual(['suburb', 'industry', 'park']);
  });
  const at = (town: string, region: number, seed: number) => {
    const t = sampleTicket('urban', 1.2, { town: 'city', seed });
    return buildField({ ...t.place, region, town, name: town }, seed);
  };
  it("a real city's street plan is its own (the design director, b618): the same every battle, never another city's", () => {
    const plan = (f: ReturnType<typeof buildField>) => JSON.stringify([f.town, f.landmark, f.storeys]);
    // Warsaw is laid out alike however the battle falls; Lodz is not Warsaw.
    expect(plan(at('Warsaw', 36, 11))).toBe(plan(at('Warsaw', 36, 12)));
    expect(plan(at('Warsaw', 36, 11))).not.toBe(plan(at('Lodz', 37, 11)));
    // Its streets and the roads through it are the same cells every time; everything else is the battle's own: the
    // ground before the city differs from battle to battle.
    const roads = (f: ReturnType<typeof buildField>) => f.ground.map((k) => +(GROUNDS[k] === 'road')).join('');
    expect(roads(at('Warsaw', 36, 11))).toBe(roads(at('Warsaw', 36, 12)));
    const front = (f: ReturnType<typeof buildField>) => f.ground.slice(0, f.cols * 10).filter((k) => GROUNDS[k] !== 'road').join(',');
    expect(front(at('Warsaw', 36, 11))).not.toBe(front(at('Warsaw', 36, 12)));
    // A practice ground's town is no place on the map: it keeps the battle's plan, a new one each fight.
    expect(plan(at('Millbrook', -1, 11))).not.toBe(plan(at('Millbrook', -1, 12)));
  });
  it('each real city keeps its own plan by its own key, picked by rule (the design director, b625)', () => {
    const g = createGame(1, 'GER'); attachWorld(g);
    const keys: string[] = [];
    for (const r of g.regions) {
      let size: ReturnType<typeof townSize>; try { size = townSize(g, r); } catch { continue; }
      if (!streetsOf({ townSize: size, terrain: r.terrain } as LandPlace)) continue;
      keys.push(planKey({ region: r.id, name: r.name, town: townName(r) } as LandPlace));
    }
    // No two real cities share a key (the two Birminghams have one each), so none shares a plan.
    expect(new Set(keys).size).toBe(keys.length);
    expect(keys).toContain('Birmingham'); expect(keys).toContain('Birmingham, Alabama');
    // Every kept plan belongs to a real city and is one of the first ten of its sequence.
    for (const [k, n] of Object.entries(CITY_PLANS)) { expect(keys, k).toContain(k); expect(Number.isInteger(n) && n >= 2 && n <= 10, `${k} ${n}`).toBe(true); }
    // A kept plan number lays another plan from the same name.
    const was = CITY_PLANS.Warsaw;
    try {
      CITY_PLANS.Warsaw = 1; const one = JSON.stringify(at('Warsaw', 36, 11).storeys);
      CITY_PLANS.Warsaw = 2; const two = JSON.stringify(at('Warsaw', 36, 11).storeys);
      expect(two).not.toBe(one);
    } finally { if (was === undefined) delete CITY_PLANS.Warsaw; else CITY_PLANS.Warsaw = was; }
  });
});

describe('blocks from divisions', () => {
  it('one a division, at least six a side, and never two divisions folded into one (ruling 226(10))', () => {
    const f = (divs: number[]) => blocksFor(divs.map((d, i) => ({ corps: i, name: '', nat: 'X', kind: 'inf', divs: d, men: 1, str: 1, org: 1, xp: 0, share: 1 / divs.length })));
    expect(f([3, 3, 2])).toEqual([3, 3, 2]);
    expect(f([2]).reduce((a, b) => a + b, 0)).toBeGreaterThanOrEqual(3);
    expect(f([1, 1]).reduce((a, b) => a + b, 0)).toBe(6);
    expect(f([8, 8, 8, 8]).reduce((a, b) => a + b, 0)).toBe(32);
  });
  it('each side\'s blocks add up to the power the card counted, so the odds are the card\'s', () => {
    const b = new LandBattle(sampleTicket('plains', 1.5));
    const sum = (s: string) => b.units.filter((u) => u.side === s).reduce((a, u) => a + u.power0, 0);
    expect(sum('att')).toBeCloseTo(100);
    expect(sum('att') / sum('def')).toBeCloseTo(1.5);
    for (const u of b.units) { expect(u.figures).toBeGreaterThan(0); expect(u.w).toBeGreaterThan(0); expect(u.act).toBe('idle'); }
  });
  it("a regiment holds its division's frontage, with a third of its models (b418)", () => {
    // The fortress works practice: two corps of two divisions a side of the line, cut into regiments to make six blocks.
    const b = new LandBattle(practiceTicket(PRACTICE_GROUNDS.find((p) => p.fort)!, { seed: 3 }));
    const regs = b.units.filter((u) => u.name.includes('Regiment'));
    expect(regs.length).toBeGreaterThan(0);
    for (const u of regs) {
      expect(u.w).toBe(LAND.size[u.kind][0]);
      expect(u.tray).toBe(Math.max(1, Math.round(LAND.figures[u.kind] / LAND.blocks.regiments)));
      expect(u.figures0).toBeLessThanOrEqual(u.tray);
    }
  });
  it('fieldworks give the cover of the ground they were dug into, never more (b418)', () => {
    const b = new LandBattle(sampleTicket('forest', 1.3, { seed: 7, dug: true }));
    const f = b.field, dug = Object.entries(f.dugInto ?? {});
    expect(dug.length).toBeGreaterThan(0);
    for (const [i, g] of dug) {
      const k = Number(i), x = (k % f.cols) * f.cell + 1, y = Math.floor(k / f.cols) * f.cell + 1;
      expect(GROUNDS[f.ground[k]]).toBe('works');
      expect(coverAt(f, x, y)).toBe(GROUNDS[g]);
    }
  });
  it('the field opens with both armies drawn up in their own halves', () => {
    const b = new LandBattle(sampleTicket('forest', 1.2));
    for (const u of b.units) expect(u.side === 'att' ? u.y < b.field.h / 2 : u.y > b.field.h / 2).toBe(true);
  });
});

describe('orders and the end', () => {
  it('a block marches where it is sent, and says what it is doing for the models', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    b.ai.def = false;
    const u = b.units.find((x) => x.side === 'att' && x.kind === 'inf')!;
    expect(b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 200 }, 'att')).toBe(true);
    let marched = false;
    for (let i = 0; i < 600; i++) { b.step(); if (u.act === 'march') marched = true; }
    expect(marched).toBe(true);
    expect(Math.abs(u.y - (u.home[1] + 200))).toBeLessThan(10);
    expect(u.order).toBeNull();
  });
  it('two blocks sent to one spot do not end up standing in each other', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    b.ai.def = false;
    const [u, v] = b.units.filter((x) => x.side === 'att' && x.kind === 'inf');
    b.command({ do: 'move', units: [u.id], x: 1500, y: 700 }, 'att');
    b.command({ do: 'move', units: [v.id], x: 1500, y: 700 }, 'att');
    for (let i = 0; i < 1500; i++) b.step();
    // Side by side or one behind the other, but not on top of each other.
    const lat = Math.abs(u.x - v.x), dep = Math.abs(u.y - v.y);
    expect(lat >= (u.w + v.w) / 2 - 2 || dep >= (u.d + v.d) / 2 - 2).toBe(true);
  });
  it('tanks and lorries cross a river by its bridge from anywhere on their bank, however the bridge lies (b418)', () => {
    // A lone block, sent from along its own bank or from well back, to a point across: by a march order (its way worked
    // out once) and by an attack (worked out afresh every tick). It is across once it stands 50 m past the river's middle.
    let tried = 0, crossed = 0;
    const stuck: string[] = [];
    for (let s = 1; s <= 20; s++) {
      const b = new LandBattle(sampleTicket('plains', 1.3, { seed: s * 7919 + 13, river: true }));
      const f = b.field, mid = f.riverMid!, at = (x: number): number => mid[Math.min(f.cols - 1, Math.floor(x / f.cell))];
      const u = b.units.find((x) => x.side === 'att' && x.kind === 'arm')!, foe = b.units.find((x) => x.side === 'def' && x.kind !== 'art')!;
      for (const x of b.units) if (x !== u && x !== foe) x.state = 'withdrawn';
      for (const kind of ['arm', 'mot'] as LandUnit['kind'][]) for (const how of ['move', 'attack']) for (const sx of [300, 1500, 2700]) for (const back of [false, true]) {
        const tx = sx < 1500 ? 2500 : 500;
        Object.assign(u, { kind, x: sx, y: back ? 300 : at(sx) - 110, face: 0, order: null, path: [], state: 'steady' });
        Object.assign(foe, { x: tx, y: at(tx) + 350, order: null, path: [] });
        (b as unknown as { send(u: LandUnit, o: object): void }).send(u, how === 'move' ? { kind: 'move', x: tx, y: foe.y } : { kind: 'attack', x: tx, y: foe.y, target: foe.id });
        let ok = false;
        for (let i = 0; i < 5000 && !ok; i++) { (b as unknown as { move(u: LandUnit): void }).move(u); ok = u.y > at(u.x) + 50; }
        tried++; if (ok) crossed++; else stuck.push(`seed ${s} ${kind} ${how} from ${sx}${back ? ' well back' : ''}: stands at ${u.x.toFixed(0)}, ${u.y.toFixed(0)}`);
      }
    }
    expect(stuck).toEqual([]);
    expect(crossed).toBe(tried);
  });
  it('tanks and lorries find their way round rock, over a road that crosses a ridge on the slant, out of rock they stand in (b418)', () => {
    // A lone block sent from the attack's edge to a point across the river, in the hills and the mountains, wherever the
    // ground lets it get there at all (not a pocket shut in by rock). It gets there: it never stands for good at an outcrop.
    const stuck: string[] = [];
    let tried = 0;
    for (const [ground, seeds] of [['mountain', 10], ['hills', 6]] as const) for (let s = 1; s <= seeds; s++) {
      const b = new LandBattle(sampleTicket(ground, 1.3, { seed: s * 7919 + 13, river: true }));
      const f = b.field, mid = f.riverMid!, at = (x: number): number => mid[Math.min(f.cols - 1, Math.floor(x / f.cell))];
      const u = b.units.find((x) => x.side === 'att' && x.kind === 'arm')!;
      for (const x of b.units) if (x !== u) x.state = 'withdrawn';
      for (const kind of ['arm', 'mot'] as LandUnit['kind'][]) for (const sx of [300, 1500, 2700]) {
        const tx = sx < 1500 ? 2500 : 500, ty = at(tx) + 350, pace = LAND.pace[MOTION[kind]];
        // Whether the ground lets it get there: side to side over ground it can enter, or out of what it stands in.
        const C = f.cols, cellOf = (x: number, y: number): number => Math.floor(y / f.cell) * C + Math.floor(x / f.cell);
        const go = (i: number): boolean => pace[GROUNDS[f.ground[i]]] > 0, seen = new Set([cellOf(sx, 300)]), open = [cellOf(sx, 300)];
        while (open.length) {
          const i = open.pop()!;
          for (const j of [i - 1, i + 1, i - C, i + C]) {
            if (j < 0 || j >= f.ground.length || Math.abs((j % C) - (i % C)) > 1 || seen.has(j) || (!go(j) && go(i))) continue;
            seen.add(j); open.push(j);
          }
        }
        if (!seen.has(cellOf(tx, ty)) || !go(cellOf(tx, ty))) continue;
        Object.assign(u, { kind, x: sx, y: 300, face: 0, order: null, path: [], state: 'steady' });
        (b as unknown as { send(u: LandUnit, o: object): void }).send(u, { kind: 'move', x: tx, y: ty });
        for (let i = 0; i < 12000 && Math.hypot(u.x - tx, u.y - ty) > 30; i++) (b as unknown as { move(u: LandUnit): void }).move(u);
        tried++;
        if (Math.hypot(u.x - tx, u.y - ty) > 30) stuck.push(`${ground} seed ${s} ${kind} from ${sx}: stands at ${u.x.toFixed(0)}, ${u.y.toFixed(0)} on ${GROUNDS[f.ground[cellOf(u.x, u.y)]]}`);
      }
    }
    expect(stuck).toEqual([]);
    expect(tried).toBeGreaterThan(80);
  });
  it('a block falling back leaves the field by its own edge, back over its bridge where a river lies behind it (b418)', () => {
    for (const river of [false, true]) {
      const b = new LandBattle(sampleTicket('plains', 1.2, { river, seed: 11, humanAtt: true }));
      b.ai.def = false;
      const u = b.units.find((x) => x.side === 'att' && x.kind === 'arm')!;
      for (const x of b.units) if (x !== u) x.state = 'withdrawn';
      const go = b as unknown as { send(u: LandUnit, o: object): void; move(u: LandUnit): void };
      if (river) {
        const mid = b.field.riverMid!, at = (x: number): number => mid[Math.floor(x / b.field.cell)];
        go.send(u, { kind: 'move', x: 1000, y: at(1000) + 150 });
        for (let i = 0; i < 3000 && u.order; i++) go.move(u);
        expect(u.y).toBeGreaterThan(at(u.x));
      }
      expect(b.command({ do: 'withdraw', units: [u.id] }, 'att')).toBe(true);
      for (let i = 0; i < 4000 && u.state !== 'withdrawn'; i++) go.move(u);
      expect(u.state).toBe('withdrawn');
    }
  });
  it('a block that cannot get there goes as near as it can and stands, without looking for its way every tick (b418)', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { river: true, bridgeDown: true, seed: 11, humanAtt: true }));
    b.ai.def = false;
    const u = b.units.find((x) => x.side === 'att' && x.kind === 'arm')!;
    for (const x of b.units) if (x !== u) x.state = 'withdrawn';
    const mid = b.field.riverMid!, at = (x: number): number => mid[Math.floor(x / b.field.cell)];
    const go = b as unknown as { send(u: LandUnit, o: object): void; move(u: LandUnit): void; findWay(...a: unknown[]): unknown };
    go.send(u, { kind: 'move', x: 1500, y: at(1500) + 200 });
    for (let i = 0; i < 1500; i++) go.move(u);
    // At the water's edge on its own bank, its march over.
    expect(u.y).toBeLessThan(at(u.x));
    expect(u.order).toBeNull();
    let looks = 0;
    const find = go.findWay.bind(b);
    go.findWay = (...a: unknown[]): unknown => { looks++; return find(...a); };
    go.send(u, { kind: 'move', x: 1500, y: at(1500) + 300 });
    for (let i = 0; i < 300; i++) go.move(u);
    expect(looks).toBeLessThanOrEqual(2);
    expect(u.order).toBeNull();
  });
  it('a player cannot order the other side\'s blocks', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    const e = b.units.find((x) => x.side === 'def')!;
    expect(b.command({ do: 'move', units: [e.id], x: 10, y: 10 }, 'att')).toBe(false);
  });
  it('withdrawing the whole army ends it, and the field is theirs', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { humanAtt: true }));
    expect(b.command({ do: 'withdrawAll', side: 'att' }, 'att')).toBe(true);
    expect(b.over?.winner).toBe('def');
    expect(b.over?.how).toBe('withdrew');
    expect(b.over!.R).toBeLessThan(1.2);
  });
  it('finish it for me: the dice at the ratio then standing, inside their run', () => {
    for (const odds of [0.8, 1.2, 1.8]) {
      const b = new LandBattle(sampleTicket('hills', odds, { humanAtt: true }));
      b.ai.att = true;
      for (let i = 0; i < 900; i++) b.step();
      b.command({ do: 'dice' }, 'att');
      expect(b.over).not.toBeNull();
      expect(b.over!.R).toBeGreaterThanOrEqual(odds * 0.8 - 1e-6);
      expect(b.over!.R).toBeLessThanOrEqual(odds * 1.2 + 1e-6);
      expect(b.over!.winner).toBe(b.over!.R >= 1.2 ? 'att' : 'def');
    }
  });
  it('the same ticket and the same orders replay the same battle', () => {
    const play = () => { const b = new LandBattle(sampleTicket('urban', 1.3, { seed: 44 })); b.ai.att = b.ai.def = true; runOut(b); return [b.over, b.units.map((u) => [u.x, u.y, u.str, u.mor])]; };
    expect(play()).toEqual(play());
  });
  it('an army that breaks ends the battle, and the result is on the right side of the line', () => {
    const b = new LandBattle(sampleTicket('plains', 2));
    b.ai.att = b.ai.def = true;
    runOut(b);
    expect(b.over).not.toBeNull();
    expect(b.over!.winner === 'att').toBe(b.over!.R >= 1.2);
    expect(b.over!.seconds).toBeLessThan(b.ticket.limit);
  });
});

describe('fair to the dice (ruling 217(3))', () => {
  // Two computer generals carry the ground about as often as the dice would at the same odds, on every ground. The
  // measure: the mean gap between the field's win rate and the dice's chance over a spread of odds, 24 battles each
  // (24 since the day's luck, ruling 226: each battle now draws its own fortune, so 16 left the gap mostly chance).
  const ODDS = [0.8, 1.0, 1.1, 1.2, 1.3, 1.4, 1.6, 1.8];
  const RUNS = 24;
  const gaps: Record<string, number> = {};
  for (const ground of ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as const) {
    it(`on ${ground}`, () => {
      let gap = 0;
      for (const odds of ODDS) {
        let won = 0;
        for (let s = 1; s <= RUNS; s++) { const b = new LandBattle(sampleTicket(ground, odds, { seed: s * 7919 + 13 })); b.ai.att = b.ai.def = true; runOut(b); if (b.over!.winner === 'att') won++; }
        gap += Math.abs(won / RUNS - diceChance(odds, [0.8, 1.2]));
      }
      gaps[ground] = gap / ODDS.length;
      expect(gaps[ground]).toBeLessThanOrEqual(0.13);
    }, 120000);
  }
  it('and within a tenth across all grounds together', () => {
    const all = Object.values(gaps);
    expect(all.length).toBe(7);
    expect(all.reduce((a, b) => a + b, 0) / all.length).toBeLessThanOrEqual(0.1);
  });
});

describe('a battle fought to its end costs what the campaign bills for it (the design director, b356)', () => {
  // The campaign charges each side its losses from R alone (resolveBattle in game.ts: the attack 0.14 / R between 2%
  // and 20%; the defence 0.07 + 0.1 R between 6% and 45%, six tenths of that where it holds). Before any corps' own
  // toll, command or works, which the field does not model. The field's own losses must tell the same story: on
  // average within a fifth of the bill for the R it hands back, winners and losers alike, and most battles inside the
  // run of bills the dice could have rolled at the card's odds, give or take a quarter.
  const clamp = (x: number, a: number, b: number): number => Math.max(a, Math.min(b, x));
  const billAtt = (R: number): number => clamp(0.14 / R, 0.02, 0.2);
  const billDef = (R: number): number => clamp(0.07 + 0.1 * R, 0.06, 0.45) * (R >= 1.2 ? 1 : 0.6);
  it('on every ground, computer against computer', () => {
    const cells: Record<string, { a: number; d: number; n: number }> = { attWon: { a: 0, d: 0, n: 0 }, defWon: { a: 0, d: 0, n: 0 } };
    let n = 0, inA = 0, inD = 0;
    for (const ground of ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as const) {
      for (const odds of [0.8, 1.0, 1.2, 1.4, 1.8, 2.5]) {
        for (let s = 1; s <= 4; s++) {
          const b = new LandBattle(sampleTicket(ground, odds, { seed: s * 7919 + 13 }));
          b.ai.att = b.ai.def = true;
          runOut(b);
          const o = b.over!, k = b.ticket.reckoning;
          const c = cells[o.winner === 'att' ? 'attWon' : 'defWon'];
          c.a += o.lost.att / billAtt(o.R); c.d += o.lost.def / billDef(o.R); c.n++;
          const lo = k.odds * k.dice[0], hi = k.odds * k.dice[1];
          const ds = [billDef(lo), billDef(hi), billDef(clamp(1.2, lo, hi))];
          if (o.lost.att >= billAtt(hi) * 0.75 && o.lost.att <= billAtt(lo) * 1.25) inA++;
          if (o.lost.def >= Math.min(...ds) * 0.75 && o.lost.def <= Math.max(...ds) * 1.25) inD++;
          n++;
        }
      }
    }
    for (const c of Object.values(cells)) {
      expect(c.n).toBeGreaterThan(20);
      expect(c.a / c.n).toBeGreaterThan(0.8); expect(c.a / c.n).toBeLessThan(1.25);
      expect(c.d / c.n).toBeGreaterThan(0.8); expect(c.d / c.n).toBeLessThan(1.25);
    }
    expect(inA / n).toBeGreaterThanOrEqual(0.7);
    expect(inD / n).toBeGreaterThanOrEqual(0.7);
  }, 120000);
});

describe('a fight, not a blink (the design director, ruling 226)', () => {
  const GROUNDS7 = ['plains', 'forest', 'hills', 'mountain', 'marsh', 'urban', 'desert'] as const;
  /** Select every block, right-click the enemy nearest them, Space: the order a newcomer gives first (mark 4). */
  const plainAttack = (b: LandBattle): void => {
    const mine = b.units.filter((u) => u.side === 'att' && u.kind !== 'fort');
    const cx = mine.reduce((a, u) => a + u.x, 0) / mine.length, cy = mine.reduce((a, u) => a + u.y, 0) / mine.length;
    const foes = b.units.filter((u) => u.side === 'def');
    const near = foes.reduce((m, f) => (Math.hypot(f.x - cx, f.y - cy) < Math.hypot(m.x - cx, m.y - cy) ? f : m), foes[0]);
    expect(b.command({ do: 'attack', units: mine.map((u) => u.id), target: near.id }, 'att')).toBe(true);
  };

  it('"what you had won stays won" only when the field moved his way by five points or more (the design director, 20:51)', () => {
    let said = 0, quiet = 0;
    for (const ground of GROUNDS7) {
      for (const s of [1, 2, 3]) {
        const b = new LandBattle(sampleTicket(ground, 1.2, { seed: s * 7919 + 13 }));
        const st0 = b.standing(), share0 = st0.att / (st0.att + st0.def);
        b.ai.def = true;
        plainAttack(b);
        for (let i = 0; i < 20000 && !b.over && (b.contactAt === null || b.t < b.contactAt + 20 * s); i++) b.step();
        if (b.over) continue;
        const st = b.standing(), gain = st.att / (st.att + st.def) - share0;
        b.toDice();
        const line = b.over!.lines[0];
        expect(line.includes('what you had won stays won')).toBe(gain >= 0.05);
        if (gain >= 0.05) said++; else quiet++;
      }
    }
    expect(said + quiet).toBeGreaterThan(10);
  });

  it('an unscouted defence shows him only where it stands until one of his has fought it, and the fight is the same (b464(2))', () => {
    for (const [ground, seed] of [['plains', 3], ['hills', 11], ['desert', 29]] as const) {
      const play = (fog: boolean): { b: LandBattle; said: string[] } => {
        const b = new LandBattle(sampleTicket(ground, 1.3, { humanAtt: true, seed }));
        b.fog = fog;
        b.ai.def = true;
        const said: string[] = [];
        if (fog) {
          expect(b.units.filter((u) => u.side === 'def').some((u) => b.known(u))).toBe(false);
          expect(b.units.filter((u) => u.side === 'att').every((u) => b.known(u))).toBe(true);
        }
        plainAttack(b);
        for (let i = 0; i < 20000 && !b.over; i++) {
          b.step();
          for (const e of b.view('att').events) {
            const u = e.unit !== undefined ? b.unit(e.unit) : undefined;
            // A dispatch names a block of theirs only once he knows it: before that, "Enemy troops are attacking your ..."
            // or nothing at all, words or sound (the UI director, b464).
            if (u && u.side === 'def' && !b.known(u)) expect(e.text).toMatch(/^\S+ troops are attacking your /);
            if (e.text) said.push(e.text);
          }
          if (fog && b.over === null) {
            for (const u of b.units) if (u.side === 'def' && !u.found) for (const t of said) expect(t.includes(u.name)).toBe(false);
          }
        }
        return { b, said };
      };
      const open = play(false), shut = play(true);
      // Words and pictures only: the same battle to the last block.
      expect(shut.b.over).toEqual(open.b.over);
      expect(shut.b.units.map((u) => [u.x, u.y, u.str, u.mor, u.state])).toEqual(open.b.units.map((u) => [u.x, u.y, u.str, u.mor, u.state]));
      // Met in the fight, a block is known; once the battle is over, every block is.
      expect(shut.b.units.some((u) => u.side === 'def' && u.found)).toBe(true);
      expect(shut.b.units.every((u) => shut.b.known(u))).toBe(true);
    }
  });
  it('under fog, the blocks of a corps the card identified show as the card shows them; the rest stay plain (b476(3))', () => {
    const b = new LandBattle(sampleTicket('plains', 1.3, { humanAtt: true, seed: 3 }));
    b.fog = true;
    const named = b.ticket.def.forces[0].corps;
    b.seen = new Set([named]);
    const theirs = b.units.filter((u) => u.side === 'def');
    expect(theirs.some((u) => u.corps === named)).toBe(true);
    expect(theirs.some((u) => u.corps !== named)).toBe(true);
    for (const u of theirs) expect(b.known(u)).toBe(u.corps === named);
  });
  it('a block his guns shell from far back is known to him; theirs shelling his from far back is not (b464(2))', () => {
    const t = sampleTicket('plains', 1.3, { humanAtt: true, seed: 3 });
    const gf = t.def.forces.find((f) => f.kind === 'art')!;
    t.att.forces.push({ ...gf, corps: 999, nat: t.att.forces[0].nat, dug: false, name: 'Gun group' });
    const b = new LandBattle(t);
    b.fog = true;
    b.ai.def = true;
    const gun = b.units.find((u) => u.side === 'att' && u.kind === 'art')!;
    const foes = b.units.filter((u) => u.side === 'def' && u.kind !== 'art');
    const foe = foes.reduce((m, f) => (Math.hypot(f.x - gun.x, f.y - gun.y) < Math.hypot(m.x - gun.x, m.y - gun.y) ? f : m));
    expect(b.command({ do: 'attack', units: [gun.id], target: foe.id }, 'att')).toBe(true);
    for (let i = 0; i < 600 && gun.engaged !== foe.id; i++) b.step();
    expect(gun.engaged).toBe(foe.id);
    expect(b.gap(gun, foe)).toBeGreaterThan(LAND.guns.closeAt);
    expect(b.known(foe)).toBe(true);
    const far = b.units.filter((u) => u.side === 'def' && u.kind === 'art' && u.engaged !== undefined && b.gap(u, b.unit(u.engaged)!) >= LAND.guns.closeAt);
    expect(far.length).toBeGreaterThan(0);
    expect(far.some((u) => b.known(u))).toBe(false);
  });

  it('a river crossing is a bridgehead, not a heap: no box drawn on another, none left standing in the river (the art director, b439)', () => {
    // The practice river, as the art director looks at it: at a minute, a minute and a half and two minutes.
    const p = PRACTICE_GROUNDS.find((x) => x.river)!;
    const box = (u: LandUnit): [number, number][] => {
      const [fx, fy, rx, ry] = [Math.sin(u.face), Math.cos(u.face), Math.cos(u.face), -Math.sin(u.face)];
      return [[1, 1], [1, -1], [-1, -1], [-1, 1]].map(([a, c]) => [u.x + rx * a * u.w / 2 + fx * c * u.d / 2, u.y + ry * a * u.w / 2 + fy * c * u.d / 2]);
    };
    // Drawn on one another: no line between the two boxes keeps them apart (2 m given for the ink).
    const onEach = (A: [number, number][], B: [number, number][]): boolean => [A, B].every((P) => P.every(([x1, y1], i) => {
      const [x2, y2] = P[(i + 1) % 4], L = Math.hypot(x2 - x1, y2 - y1), nx = (y2 - y1) / L, ny = (x1 - x2) / L;
      const a = A.map(([x, y]) => x * nx + y * ny), b = B.map(([x, y]) => x * nx + y * ny);
      return Math.max(...a) - 2 >= Math.min(...b) && Math.max(...b) - 2 >= Math.min(...a);
    }));
    let pictures = 0, heaped = 0, wet = 0;
    for (let s = 1; s <= 12; s++) {
      const b = new LandBattle(practiceTicket(p, { humanAtt: true, seed: s * 7919 + 13 }));
      b.ai.att = false; b.ai.def = true;
      plainAttack(b);
      for (let i = 1; i <= 1200 && !b.over; i++) {
        b.step();
        if (i % 300 || i < 600) continue;
        pictures++;
        const up = b.units.filter((u) => u.kind !== 'fort' && u.state !== 'gone' && u.state !== 'withdrawn' && u.state !== 'routing');
        for (let a = 0; a < up.length; a++) for (let c = a + 1; c < up.length; c++) if (up[a].side === up[c].side && onEach(box(up[a]), box(up[c]))) heaped++;
        for (const u of up) if (u.speed < 0.5 && GROUNDS[b.field.ground[Math.floor(u.y / b.field.cell) * b.field.cols + Math.floor(u.x / b.field.cell)]] === 'water') wet++;
      }
    }
    expect(pictures).toBeGreaterThan(30);
    // Before b439 these 36 pictures held 15 pairs of boxes on one another, mostly at the bridge, and two blocks stood in
    // the river; now 3, each passing: the reserve going up behind the line (b453: a block finding no room within its
    // reach presses in beside a friend, where before it stood off out of reach and never fired).
    expect(heaped / pictures).toBeLessThan(0.15);
    expect(wet).toBe(0);
  });

  it('no block stands in the river: it wades over without stopping, and none is left in it when the battle ends (b464(5))', () => {
    const p = PRACTICE_GROUNDS.find((x) => x.river)!;
    let wading = 0, stood = 0, left = 0;
    for (let s = 1; s <= 12; s++) {
      const b = new LandBattle(practiceTicket(p, { humanAtt: true, seed: s * 7919 + 13 }));
      b.ai.att = false; b.ai.def = true;
      plainAttack(b);
      const wet = (u: LandUnit): boolean => ['water', 'blown'].includes(GROUNDS[b.field.ground[Math.floor(u.y / b.field.cell) * b.field.cols + Math.floor(u.x / b.field.cell)]]);
      const up = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
      while (!b.over) {
        b.step();
        for (const u of b.units) if (up(u) && u.state !== 'routing' && wet(u)) { wading++; if (u.speed < 0.5) stood++; }
      }
      for (const u of b.units) if (up(u) && wet(u)) left++;
    }
    // Before b464 the foot crawled the last 20 m out of the river's bed at 0.8 m a second, and 97 blocks in 120 of these
    // battles were left standing in it when the battle ended.
    expect(wading).toBeGreaterThan(0);
    expect(stood).toBe(0);
    expect(left).toBe(0);
  }, 60000);

  /** A river practice card on which some of the attack's corps come on over dry ground, as XIX Corps at Pomorze. */
  const partly = (s: number, dry: number[], end: 'west' | 'east', odds = 1.3) => {
    const t = sampleTicket('plains', odds, { seed: s * 7919 + 13, humanAtt: true, river: true });
    t.place.dryEnd = end;
    t.att.forces = t.att.forces.map((f, i) => (dry.includes(i) ? { ...f, cross: false } : f));
    return t;
  };
  /** As partly, the attack lopsided: one corps of fourteen divisions comes on over dry ground, and one division crosses. */
  const lopsided = (s: number, end: 'west' | 'east') => {
    const t = partly(s, [0], end);
    t.att.forces = [{ ...t.att.forces[0], divs: 14, share: 0.9 }, { ...t.att.forces[1], divs: 1, share: 0.1 }];
    return t;
  };
  const onRiver = (b: LandBattle, x: number, y: number): boolean => ['water', 'blown', 'bridge'].includes(groundAt(b.field, x, y));

  it('the river lies only before the corps the card sends over it: one it brings on over dry ground meets none on its way (b464(5))', () => {
    const cases: [() => ReturnType<typeof partly>, number[], 'west' | 'east'][] = [];
    for (const dry of [[2], [0], [0, 2]]) for (const end of ['west', 'east'] as const) for (let s = 1; s <= 4; s++) cases.push([() => partly(s, dry, end), dry, end]);
    // Few lanes cross: the river still lies before them, and its bridge there stands clear of where it comes round.
    for (const end of ['west', 'east'] as const) for (let s = 1; s <= 6; s++) cases.push([() => lopsided(s, end), [0], end]);
    for (const [make, dry, end] of cases) {
      const b = new LandBattle(make()), f = b.field;
      expect(f.bend?.dry).toBe(end);
      const att = b.units.filter((u) => u.side === 'att'), dryXs = att.filter((u) => u.dry).map((u) => u.x), wetXs = att.filter((u) => !u.dry).map((u) => u.x);
      expect(dryXs.length).toBe(dry.reduce((a, i) => a + blocksFor(make().att.forces)[i], 0));
      // Those that do not cross draw up together at their end of the field, those that cross at the other.
      expect(end === 'west' ? Math.max(...dryXs) < Math.min(...wetXs) : Math.min(...dryXs) > Math.max(...wetXs)).toBe(true);
      // Straight up its lane to the defence's line: no water, no bridge, for one that does not cross; the river for one that does.
      const meets = (x0: number, x1: number, y0: number): boolean => {
        for (let y = y0; y <= f.zones.def[1]; y += f.cell / 2) for (let x = x0; x <= x1; x += f.cell / 2) if (onRiver(b, x, y)) return true;
        return false;
      };
      for (const u of att) expect(meets(u.x - u.w / 2, u.x + u.w / 2, u.y)).toBe(!u.dry);
      // Nobody draws up on the river, and the defence's whole line stands on its far side, the bend away to the south.
      for (const u of b.units) expect(onRiver(b, u.x, u.y)).toBe(false);
      for (const u of b.units.filter((v) => v.side === 'def')) expect(u.y).toBeGreaterThan(f.riverMid![Math.min(f.cols - 1, Math.floor(u.x / f.cell))] + 60);
      // Its bridges stand only where it runs across before the lanes that cross; there is one there.
      let there = 0;
      for (let i = 0; i < f.ground.length; i++) {
        if (!['bridge', 'blown'].includes(GROUNDS[f.ground[i]])) continue;
        const x = (i % f.cols) * f.cell + f.cell / 2;
        expect((x - f.bend!.x) * (end === 'west' ? 1 : -1)).toBeGreaterThan(LAND.mouth + 60);
        there++;
      }
      expect(there).toBeGreaterThan(0);
    }
  });

  it('no block stands in the river where only some corps cross either, and those that do not cross never wade (b464(5))', () => {
    let wading = 0, stood = 0, left = 0, dryWet = 0, dryMet = 0, battles = 0;
    for (let s = 1; s <= 8; s++) {
      const b = new LandBattle(partly(s, s % 3 ? [2] : [0], s % 2 ? 'west' : 'east'));
      b.ai.att = false; b.ai.def = true;
      plainAttack(b);
      const up = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
      const wet = (u: LandUnit): boolean => ['water', 'blown'].includes(groundAt(b.field, u.x, u.y));
      while (!b.over) {
        b.step();
        for (const u of b.units) if (up(u) && u.state !== 'routing' && wet(u)) { wading++; if (u.speed < 0.5) stood++; if (u.dry) dryWet++; }
      }
      for (const u of b.units) if (up(u) && wet(u)) left++;
      if (b.units.some((u) => u.dry && u.met)) dryMet++;
      battles++;
    }
    expect(wading).toBeGreaterThan(0);
    expect(stood).toBe(0);
    expect(left).toBe(0);
    expect(dryWet).toBe(0);
    // And they fight: over dry ground, as the card counts them.
    expect(dryMet).toBe(battles);
  }, 60000);

  it('an attack too wide for its lanes either side of the bend draws up in further rows behind, no two blocks on top of each other (b492(3))', () => {
    for (const end of ['west', 'east'] as const) for (let s = 1; s <= 2; s++) {
      const t = partly(s, [0], end);
      t.att.forces = [{ ...t.att.forces[0], divs: 16 }, { ...t.att.forces[1], divs: 8 }, t.att.forces[2]];
      const b = new LandBattle(t), f = b.field, z = f.zones.att, att = b.units.filter((u) => u.side === 'att');
      expect(f.bend?.dry).toBe(end);
      // Both lanes are too narrow for their foot in one line: it goes on in a row behind.
      for (const dry of [true, false]) expect(new Set(att.filter((u) => !!u.dry === dry && u.kind === 'inf').map((u) => u.y)).size).toBeGreaterThan(1);
      for (let i = 0; i < att.length; i++) for (let j = i + 1; j < att.length; j++) {
        const a = att[i], c = att[j], dx = Math.abs(a.x - c.x) - (a.w + c.w) / 2, dy = Math.abs(a.y - c.y) - (a.d + c.d) / 2;
        // Clear of each other, and GAP between two side by side in a row.
        expect(Math.max(dx, dy)).toBeGreaterThan(0);
        if (a.y === c.y) expect(dx).toBeGreaterThanOrEqual(40 - 1e-6);
      }
      for (const u of att) {
        // Each in its own lanes, BEND.clear either side of the bend kept clear, within the attack's zone, off the river.
        const off = (u.dry ? 1 : -1) * (end === 'west' ? f.bend!.x - u.x : u.x - f.bend!.x);
        expect(off - u.w / 2).toBeGreaterThanOrEqual(BEND.clear - 1e-6);
        expect(u.x - u.w / 2).toBeGreaterThanOrEqual(10 - 1e-6);
        expect(u.x + u.w / 2).toBeLessThanOrEqual(f.w - 10 + 1e-6);
        expect(u.y - u.d / 2).toBeGreaterThanOrEqual(z[1]);
        expect(u.y + u.d / 2).toBeLessThanOrEqual(z[3]);
        expect(onRiver(b, u.x, u.y)).toBe(false);
      }
    }
  });


  it('a river never makes an attack better than none: the tanks brought on over dry ground and the foot sent across win no more than with no river at all (b494)', () => {
    // The design director's rule (b492(2), b494): every river attack, whole or partial, at or below its no-river twin,
    // the same ticket with its river taken away. The hold the river gives the defence counts only as far as the attack's
    // tanks and lorries must cross with it, so here, the armour dry, it adds nothing, and the wading is the river's cost.
    let river = 0, none = 0, n = 0;
    for (const odds of [1.2, 1.3]) for (const end of ['west', 'east'] as const) for (let s = 1; s <= 20; s++) {
      const t = partly(s, [2], end, odds);
      for (const [tk, wet] of [[t, true], [{ ...t, place: { ...t.place, river: null, dryEnd: undefined } }, false]] as const) {
        const b = new LandBattle(tk);
        b.ai.att = false; b.ai.def = true;
        if (wet) expect(b.field.bend?.dry).toBe(end);
        plainAttack(b);
        runOut(b);
        if (b.over!.winner === 'att') { if (wet) river++; else none++; }
      }
      n++;
    }
    expect(river / n).toBeLessThanOrEqual(none / n + 0.03);
  }, 180000);

  it('where every corps crosses, or none does, the river does not bend: the field and the fight are those of a card with no say (b464(5))', () => {
    for (let s = 1; s <= 3; s++) {
      const t = sampleTicket('plains', 1.3, { seed: s * 7919 + 13, humanAtt: true, river: true });
      const all = { ...t, att: { ...t.att, forces: t.att.forces.map((f) => ({ ...f, cross: true })) } };
      const none = { ...t, att: { ...t.att, forces: t.att.forces.map((f) => ({ ...f, cross: false })) } };
      const run = (tt: typeof t) => { const b = new LandBattle(tt); b.ai.att = false; b.ai.def = true; plainAttack(b); runOut(b); return b; };
      const [b0, b1, b2] = [run(t), run(all), run(none)];
      expect(b0.field.bend).toBeUndefined();
      expect(b0.field.course).toBeUndefined();
      for (const b of [b1, b2]) {
        expect(b.field).toEqual(b0.field);
        expect(b.units.map((u) => [u.x, u.y, u.str, u.state])).toEqual(b0.units.map((u) => [u.x, u.y, u.str, u.state]));
        expect(b.over).toEqual(b0.over);
      }
      // Where only some cross, it bends: the same seed gives another river.
      expect(new LandBattle(partly(s, [2], 'west')).field.ground).not.toEqual(new LandBattle(t).field.ground);
    }
  }, 60000);

  it('a close battle lasts minutes from the first contact, and never breaks an army most of which has not fought (mark 2)', () => {
    const fights: number[] = [];
    for (const ground of GROUNDS7) {
      for (const odds of [0.8, 1.0, 1.2, 1.5]) {
        for (let s = 1; s <= 3; s++) {
          const b = new LandBattle(sampleTicket(ground, odds, { seed: s * 7919 + 13 }));
          b.ai.att = b.ai.def = true;
          runOut(b);
          expect(b.contactAt).not.toBeNull();
          fights.push(b.t - b.contactAt!);
          const broke = b.log.find((e) => e.what === 'broken');
          if (broke) {
            // The army that broke: at least half its strength had been in the fight.
            const us = b.units.filter((u) => u.side === broke.side);
            const P = us.reduce((a, u) => a + u.power0, 0);
            const fought = us.filter((u) => u.met || u.str < u.str0 * 0.97 || u.state === 'gone').reduce((a, u) => a + u.power0, 0);
            expect(fought / P).toBeGreaterThanOrEqual(0.5);
            // Routing belongs to the blocks that broke; the rest fall back with their army (after the slice 3 review).
            for (const u of us) expect(u.fellBack ? u.state !== 'routing' : ['routing', 'gone', 'withdrawn'].includes(u.state)).toBe(true);
          }
        }
      }
    }
    fights.sort((a, c) => a - c);
    const at = (p: number): number => fights[Math.floor(p * (fights.length - 1))];
    expect(at(0.05)).toBeGreaterThanOrEqual(90);
    expect(at(0.5)).toBeGreaterThanOrEqual(120);
    expect(at(0.5)).toBeLessThanOrEqual(240);
  }, 120000);

  it('"broke after" reads the field\'s own time from the first contact to the break, to the half minute (b464(6))', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { seed: 5 }));
    const said = (from: number, to: number): string => {
      b.contactAt = from; b.t = to;
      return (b as unknown as { brokeAfter(s: 'att' | 'def'): string }).brokeAfter('def');
    };
    // "Of fighting" on every form (the UI director, b464).
    expect(said(100, 110)).toMatch(/ broke within half a minute of fighting\.$/);
    expect(said(100, 135)).toMatch(/ broke after half a minute of fighting\.$/);
    expect(said(100, 165)).toMatch(/ broke after a minute of fighting\.$/);
    expect(said(100, 185)).toMatch(/ broke after a minute and a half of fighting\.$/);
    expect(said(100, 220)).toMatch(/ broke after two minutes of fighting\.$/);
    expect(said(100, 250)).toMatch(/ broke after two and a half minutes of fighting\.$/);
    expect(said(40, 400)).toMatch(/ broke after six minutes of fighting\.$/);
  });

  it('one block running falls back alone: the army bar counts only its share', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { seed: 99 }));
    const u = b.units.find((x) => x.side === 'att' && x.kind === 'inf')!;
    const before = b.breaking('att');
    u.str = u.str0 * 0.5; u.mor = 0;
    for (let i = 0; i < 5; i++) b.step();
    expect(u.state).toBe('routing');
    expect(b.over).toBeNull();
    expect(b.breaking('att')).toBeGreaterThan(before);
    expect(b.breaking('att')).toBeLessThan(0.75);
    expect(b.view().breaking!.att).toBeCloseTo(b.breaking('att'), 6);
  });
  it('held at the brink, a small army gives ground with its worn rather than stand to be ground down (b496(1))', () => {
    // A hopeless attack by one division, three regiments, with the plain order, fought out. Sent straight back in when
    // it gives ground, the worn regiment stands at the brink as it did before b496(1).
    const fight = (stand: boolean) => {
      const t = sampleTicket('plains', 0.3, { seed: 7, humanAtt: true });
      t.att.forces = [{ ...t.att.forces.find((f) => f.kind === 'inf')!, divs: 1, share: 1 }];
      const b = new LandBattle(t);
      b.ai.def = true;
      const mine = b.units.filter((u) => u.side === 'att');
      expect(mine.length).toBeLessThanOrEqual(LAND.fresh.upTo);
      const cx = mine.reduce((a, u) => a + u.x, 0) / mine.length, cy = mine.reduce((a, u) => a + u.y, 0) / mine.length;
      const foe = b.units.filter((u) => u.side === 'def').reduce((m, e) => (Math.hypot(e.x - cx, e.y - cy) < Math.hypot(m.x - cx, m.y - cy) ? e : m));
      expect(b.command({ do: 'attack', units: mine.map((u) => u.id), target: foe.id }, 'att')).toBe(true);
      const gave: { u: LandUnit; str: number; steady: boolean; bar: number; text: string }[] = [];
      while (!b.over) {
        b.step();
        for (const e of b.view('att').events) {
          if (e.what !== 'land:gives') continue;
          const u = b.unit(e.unit!)!;
          gave.push({ u, str: u.str / u.str0, steady: u.state === 'steady', bar: b.breaking('att'), text: e.text });
          if (stand) b.command({ do: 'attack', units: [u.id], target: foe.id }, 'att');
        }
      }
      return { b, mine, gave };
    };
    const { b, mine, gave } = fight(false);
    // Once the army stood at the brink, the regiment worn past being fresh gave ground: it fell back, and the player read it.
    expect(gave.length).toBe(1);
    const g = gave[0];
    expect(g.bar).toBeCloseTo(0.99, 6);
    expect(g.str < LAND.fresh.str || !g.steady).toBe(true);
    expect(g.text).toBe(`${g.u.name} is worn out and falls back; the rest of its army holds.`);
    expect(g.u.gives).toBe(true);
    expect(g.u.order?.kind === 'withdraw' || g.u.state === 'withdrawn').toBe(true);
    // Mark 2 as before: the army broke only once another regiment was worn, most of it having fought.
    expect(b.over!.how).toBe('rout');
    expect(mine.filter((u) => u !== g.u).some((u) => u.str < u.str0 * LAND.fresh.str || u.state !== 'steady')).toBe(true);
    // And it cost less than standing there: the regiment that gave ground came off with more of its strength, and the army
    // with less lost.
    const st = fight(true);
    const twin = st.mine.find((u) => u.id === g.u.id)!;
    expect(g.u.str / g.u.str0).toBeGreaterThan(twin.str / twin.str0 + 0.05);
    expect(b.over!.lost.att).toBeLessThan(st.b.over!.lost.att - 0.01);
  });

  it('worn past what it will stand, a small army holds at the brink while most of it is fresh, in contact or not (b476(1), b480)', () => {
    // A small army, all in contact: the regiments worn are past what the army will stand, but most of it is fresh, so it
    // holds at the brink.
    const t = sampleTicket('plains', 1.2, { seed: 99 });
    t.att.forces = [{ ...t.att.forces.find((f) => f.kind === 'inf')!, divs: 1, share: 1 }];
    const b = new LandBattle(t);
    const mine = b.units.filter((x) => x.side === 'att' && x.kind !== 'fort');
    expect(mine.length).toBeGreaterThanOrEqual(3);
    expect(mine.length).toBeLessThanOrEqual(LAND.fresh.upTo);
    for (const u of mine) u.met = true;
    const all = mine.reduce((a, u) => a + u.power0, 0), share = (us: LandUnit[]): number => us.reduce((a, u) => a + u.power0, 0) / all;
    const worn: LandUnit[] = [], next: LandUnit[] = [];
    for (const u of mine) if (share([...worn, u]) < 0.5) worn.push(u); else if (share([...worn, ...next]) <= 0.5) next.push(u);
    for (const u of worn) u.str = u.str0 * 0.4;
    expect(b.wornShare('att')).toBeGreaterThan(LAND.spent.att);
    expect(share(worn)).toBeLessThan(0.5);
    expect(b.breaking('att')).toBeCloseTo(0.99, 6);
    // Under nine tenths, a regiment is fresh no longer: now most of it has fought, and it breaks.
    expect(share([...worn, ...next])).toBeGreaterThan(0.5);
    for (const u of next) u.str = u.str0 * 0.89;
    expect(b.breaking('att')).toBeGreaterThanOrEqual(1);
    // Nor is a whole one that wavers.
    for (const u of next) { u.str = u.str0; u.state = 'wavering'; }
    expect(b.breaking('att')).toBeGreaterThanOrEqual(1);
    for (const u of next) u.state = 'steady';
    expect(b.breaking('att')).toBeCloseTo(0.99, 6);
  });
  it('a larger army holds at the brink only by those not yet in the fight, as before (b480)', () => {
    const b = new LandBattle(sampleTicket('plains', 1.2, { seed: 99 }));
    const mine = b.units.filter((x) => x.side === 'att' && x.kind !== 'fort').sort((a, c) => c.power0 - a.power0);
    expect(mine.length).toBeGreaterThan(LAND.fresh.upTo);
    const all = mine.reduce((a, u) => a + u.power0, 0);
    const worn: LandUnit[] = [];
    for (const u of [...mine].reverse()) { if ((worn.reduce((a, x) => a + x.power0, 0) + u.power0) / all >= 0.45) break; worn.push(u); }
    for (const u of worn) u.str = u.str0 * 0.6;
    expect(b.wornShare('att')).toBeGreaterThan(LAND.spent.att);
    // Most of it not yet in the fight: it holds.
    expect(b.breaking('att')).toBeCloseTo(0.99, 6);
    // All of it in contact, most of it whole: in a large army that is an attack called off, and it breaks.
    for (const u of mine) u.met = true;
    expect(b.breaking('att')).toBeGreaterThanOrEqual(1);
  });
  it('a small defence behind fortress works breaks as before: the card prices the works already (b488)', () => {
    const brink = (fort: number): number => {
      const t = sampleTicket('plains', 1.2, { seed: 99 });
      t.def.forces = [{ ...t.def.forces.find((f) => f.kind === 'inf')!, divs: 1, share: 1 }];
      t.place = { ...t.place, fort };
      const b = new LandBattle(t);
      const theirs = b.units.filter((x) => x.side === 'def' && x.kind !== 'fort');
      expect(theirs.length).toBeLessThanOrEqual(LAND.fresh.upTo);
      for (const u of theirs) u.met = true;
      // A third of it worn hard, the rest whole and steady.
      theirs[0].str = theirs[0].str0 * 0.2;
      expect(b.wornShare('def')).toBeGreaterThan(LAND.spent.def);
      return b.breaking('def');
    };
    // In the open the fresh regiments hold it at the brink (Mechili's cure); behind works of any level they do not.
    expect(brink(0)).toBeCloseTo(0.99, 6);
    for (const fort of [1, 2, 3]) expect(brink(fort)).toBeGreaterThanOrEqual(1);
  });
  it('a large army that breaks says why: its bar and what drove it, as breaking() reckons them (b496(2))', () => {
    // The fields at the break agree with the army as it stands, and come alike in the result, the 'broken' event and the view.
    const agrees = (b: LandBattle): BreakCause => {
      const o = b.over!, why = o.brokeBy!, s = o.broke!;
      expect(o.how).toBe('rout');
      expect(why.side).toBe(s);
      const mine = b.units.filter((u) => u.side === s);
      expect(mine.length).toBeGreaterThan(LAND.fresh.upTo);
      expect(why.bar).toBe(1);
      expect(b.breaking(s)).toBeGreaterThanOrEqual(1);
      expect(why.worn).toBeCloseTo(b.wornShare(s), 3);
      expect([why.spent, why.breaks, why.fought, why.wornAt]).toEqual([LAND.spent[s], LAND.breaks, LAND.fought, LAND.fresh.str]);
      // The bar is the nearer of the two shares against their marks, and 'by' names it.
      const w = why.worn / why.spent, r = why.running / why.breaks;
      expect(Math.max(w, r)).toBeGreaterThanOrEqual(0.995);
      expect(why.by).toBe(r >= w ? 'running' : 'worn');
      expect(why.holding).toBe(false);
      expect(why.small).toBe(false);
      expect(why.fresh).toBeLessThanOrEqual(LAND.fought);
      expect(why.blocks).toBe(mine.length);
      expect(why.blocksWorn).toBe(mine.filter((u) => u.str < u.str0 * LAND.fresh.str).length);
      expect(why.routing).toBe(mine.filter((u) => u.state === 'routing').length);
      expect(why.fled).toBe(mine.filter((u) => u.state === 'gone').length);
      expect(b.log.find((e) => e.what === 'broken')!.brokeBy).toEqual(why);
      expect(b.view().breakingBy![s]).toEqual(why);
      return why;
    };
    // Fought out, computer against computer: eight blocks worn across the line.
    const fought = new LandBattle(sampleTicket('plains', 1.2, { seed: 99 }));
    fought.ai.att = fought.ai.def = true;
    runOut(fought);
    expect(agrees(fought).by).toBe('worn');
    // Worn out with every card Steady (the third playtester's Epirus): two blocks hard hit, the rest nine tenths or more.
    const worn = new LandBattle(sampleTicket('plains', 1.2, { seed: 99 }));
    const eight = worn.units.filter((u) => u.side === 'att');
    expect(eight.length).toBe(8);
    [1, 0.92, 0.92, 0.91, 0.99, 0.56, 0.65, 1].forEach((k, i) => { eight[i].str = eight[i].str0 * k; eight[i].met = true; });
    worn.step();
    const w = agrees(worn);
    expect([w.by, w.blocksWorn, w.routing, w.held]).toEqual(['worn', 2, 0, false]);
    expect(eight.every((u) => u.state === 'steady')).toBe(true);
    // Half its power running, hardly worn.
    const ran = new LandBattle(sampleTicket('plains', 1.2, { seed: 99 }));
    const big = ran.units.filter((u) => u.side === 'att').sort((a, c) => c.power0 - a.power0);
    const all = big.reduce((a, u) => a + u.power0, 0);
    let off = 0;
    for (const u of big) { if (off / all >= LAND.breaks) break; u.state = 'routing'; u.mor = 0; u.met = true; off += u.power0; }
    ran.step();
    const r = agrees(ran);
    expect([r.by, r.routing, r.held]).toEqual(['running', 4, false]);
    expect(r.worn).toBeLessThan(LAND.spent.att);
    // Worn past its mark while most of it has not fought: held at the brink, then broken once all of it is in contact.
    const held = new LandBattle(sampleTicket('plains', 1.2, { seed: 99 }));
    const line = held.units.filter((u) => u.side === 'att').sort((a, c) => a.power0 - c.power0);
    const hit: LandUnit[] = [], whole = line.reduce((a, u) => a + u.power0, 0);
    for (const u of line) { if ((hit.reduce((a, x) => a + x.power0, 0) + u.power0) / whole >= 0.45) break; hit.push(u); }
    for (const u of hit) u.str = u.str0 * 0.6;
    for (let i = 0; i < 10; i++) held.step();
    const now = held.view().breakingBy!.att;
    expect(held.over).toBeNull();
    expect([now.bar, now.by, now.holding, now.held, now.heldFor]).toEqual([0.99, 'worn', true, true, 1]);
    expect(now.bar).toBeCloseTo(held.breaking('att'), 6);
    expect(now.fresh).toBeGreaterThan(LAND.fought);
    for (const u of line) u.met = true;
    held.step();
    const h = agrees(held);
    expect([h.by, h.held, h.heldFor]).toEqual(['worn', true, 1]);
  });

  // The plain attack honours the card (mark 4, after the retest): at the telling odds, where the card gives neither
  // side a sure thing, on every ground and behind a river and fortress works, 40 battles at each odds. And dug in: the
  // card counts a defence's fieldworks in its strength, so the field gives them no cover on top (b407).
  const TELLING = [1.1, 1.2, 1.3, 1.4];
  const telling: Record<string, { gap: number; bias: number }> = {};
  const EVERY: { key: string; ground: (typeof GROUNDS7)[number]; river?: boolean; fort?: number; dug?: boolean }[] = [
    ...GROUNDS7.map((g) => ({ key: g, ground: g })), { key: 'a river crossing', ground: 'plains', river: true }, { key: 'fortress works', ground: 'hills', fort: 2 },
    { key: 'dug in on open ground', ground: 'plains', dug: true },
  ];
  for (const e of EVERY) {
    it(`the plain attack honours the card at the telling odds: ${e.key} (mark 4)`, () => {
      let gap = 0, bias = 0;
      for (const odds of TELLING) {
        let won = 0;
        for (let s = 1; s <= 40; s++) {
          const b = new LandBattle(sampleTicket(e.ground, odds, { seed: s * 7919 + 13, humanAtt: true, river: e.river, fort: e.fort, dug: e.dug }));
          b.ai.att = false; b.ai.def = true;
          plainAttack(b);
          runOut(b);
          if (b.over!.winner === 'att') won++;
        }
        const d = won / 40 - diceChance(odds, [0.8, 1.2]);
        gap += Math.abs(d); bias += d;
      }
      telling[e.key] = { gap: gap / TELLING.length, bias: bias / TELLING.length };
      expect(telling[e.key].bias).toBeGreaterThanOrEqual(-0.1);
      expect(telling[e.key].bias).toBeLessThanOrEqual(0.1);
      expect(telling[e.key].gap).toBeLessThanOrEqual(0.15);
    }, 120000);
  }
  it('and within a tenth across them all together (mark 4)', () => {
    const all = Object.values(telling);
    expect(all.length).toBe(EVERY.length);
    expect(all.reduce((a, g) => a + g.gap, 0) / all.length).toBeLessThanOrEqual(0.1);
    expect(Math.abs(all.reduce((a, g) => a + g.bias, 0) / all.length)).toBeLessThanOrEqual(0.04);
  });
  it('a river never makes an attack better than the same attack with none: behind one, no more won than on open country (b494)', () => {
    // 'a river crossing' is the plains card with a river before the defence, the same armies, odds and seeds: its twin.
    // The rule is at or below; three points is the noise of 160 battles a side, and the old hold, 1.3 on every block that
    // crossed, stood 7.5 points over.
    expect(telling['a river crossing']).toBeDefined();
    expect(telling['a river crossing'].bias).toBeLessThanOrEqual(telling.plains.bias + 0.03);
  });

  // Each practice ground's card is reckoned as the campaign's attack card would reckon those armies there (the design
  // director, b400), the defence sized to the ground so it is a contest (b406), and the plain attack wins about as often
  // as that card says: 120 battles on each, within a tenth.
  it("a practice ground's card prices its ground the campaign's way (b400)", () => {
    const at = (p: PracticeGround): number => practiceReckoning(p).odds;
    const open = at({ terrain: 'plains' });
    expect(open).toBeGreaterThan(1.2);
    expect(open).toBeLessThan(1.5);
    // Behind a river every corps attacks over the bridges at the campaign's cut, and nothing else changes.
    expect(at({ terrain: 'plains', river: true }) / open).toBeCloseTo(ENGINEERING.river.attack, 6);
    // Close country, mountains and works cost the attack; the open desert costs the defence on foot.
    for (const t of ['forest', 'hills', 'mountain', 'marsh', 'urban'] as const) expect(at({ terrain: t })).toBeLessThan(open);
    expect(at({ terrain: 'mountain' })).toBeLessThan(at({ terrain: 'hills' }));
    expect(at({ terrain: 'hills', fort: 2 })).toBeLessThan(at({ terrain: 'hills' }) / FORT_MUL[2] + 1e-9);
    expect(at({ terrain: 'desert' })).toBeGreaterThan(open);
    // The practice ticket carries that card, and the field takes its strengths from it.
    for (const p of PRACTICE_GROUNDS) {
      const t = practiceTicket(p, { seed: 5 });
      expect(t.reckoning.odds).toBeCloseTo(at(p), 9);
      expect(t.place.terrain).toBe(p.terrain);
      expect(!!t.place.river).toBe(!!p.river);
      expect(t.place.fort).toBe(p.fort ?? 0);
      // The defence on the field is the one the card reckoned: its divisions, its kind and its strength.
      const def = t.def.forces;
      expect(def.filter((f) => f.kind !== 'art').map((f) => f.divs)).toEqual(p.def ?? [3, 3]);
      expect(def.filter((f) => f.kind === 'art').reduce((a, f) => a + f.divs, 0)).toBe(p.guns ?? 1);
      for (const f of def) { expect(f.str).toBe(p.str ?? 0.9); if (f.kind !== 'art') expect(f.kind).toBe(p.kind ?? 'inf'); }
      expect(def.reduce((a, f) => a + f.share, 0)).toBeCloseTo(1, 9);
      // The attack is the same on every ground.
      expect(t.att.forces.map((f) => [f.kind, f.divs, f.str])).toEqual(practiceTicket(PRACTICE_GROUNDS[0]).att.forces.map((f) => [f.kind, f.divs, f.str]));
    }
  });
  it("the card names a worn defence only below 85, read as its meter reads (the UI director, b408)", () => {
    expect(wornWords(1)).toBe('the defence at full strength');
    expect(wornWords(0.9)).toBe('');
    expect(wornWords(0.85)).toBe('');
    expect(wornWords(0.85 - 1e-9)).toBe('');
    expect(wornWords(0.84)).toBe('the defence worn to four fifths strength');
    expect(wornWords(0.65)).toBe('the defence worn to two thirds strength');
    expect(wornWords(0.5)).toBe('the defence worn to half strength');
  });
  it('every practice card is a contest: the staff gives it between 4 and 8 in 10 (the design director, b406)', () => {
    for (const p of PRACTICE_GROUNDS) {
      const k = practiceReckoning(p);
      const c = diceChance(k.odds, k.dice);
      expect(c).toBeGreaterThanOrEqual(0.4);
      expect(c).toBeLessThanOrEqual(0.8);
    }
  });
  for (const p of PRACTICE_GROUNDS) {
    const key = `${p.terrain}${p.river ? ', a river crossing' : ''}${p.fort ? ', fortress works' : ''}`;
    it(`the plain attack honours the practice card as shown: ${key} (mark 4, b400, b406)`, () => {
      let won = 0, chance = 0;
      for (let s = 1; s <= 120; s++) {
        const t = practiceTicket(p, { seed: s * 7919 + 13, humanAtt: true });
        chance = diceChance(t.reckoning.odds, t.reckoning.dice);
        const b = new LandBattle(t);
        b.ai.att = false; b.ai.def = true;
        plainAttack(b);
        runOut(b);
        if (b.over!.winner === 'att') won++;
      }
      expect(Math.abs(won / 120 - chance)).toBeLessThanOrEqual(0.1);
    }, 120000);
  }
});

describe('the hand-off with a real campaign', () => {
  function playing(seed = 3): Game {
    const g = startOpen(createGame(seed, 'GER'));
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    return g;
  }
  const anAttack = (g: Game): { c: Corps; r: Region } => {
    for (const c of g.corps.filter((x) => x.nat === 'GER' && !x.dead && x.kind === 'inf' && !x.town && x.ap > 0)) {
      for (const j of g.regions[c.reg].nb) { const r = g.regions[j]; if (hostileUnits(g, r, 'GER').length && !blocked(g, c, r, c.reg) && previewAttack(g, [c], r)) return { c, r }; }
    }
    throw new Error('no attack');
  };
  it('Sedan is crossed where the record has it: three bridges over the Meuse, the road\'s by the town and two more (b439)', () => {
    const g = playing();
    const sedan = g.regions.find((x) => x.name === 'Sedan')!, ard = g.regions.find((x) => x.name === 'Ardennes')!;
    const c = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.kind === 'inf' && !x.town)!;
    c.reg = ard.id; c.ap = Math.max(c.ap, 1);
    if (!hostileUnits(g, sedan, 'GER').length) return;
    const t = landTicket(g, [c], sedan);
    if (!t) return;
    expect(t.place.river).toBe('Meuse');
    expect(t.place.crossings).toEqual([0.12, 0.85]);
    const f = buildField(t.place, t.seed), BRIDGE = GROUNDS.indexOf(t.place.bridgeDown ? 'blown' : 'bridge');
    // Spans of bridge across the field, apart from one another.
    const cols = [...new Set([...f.ground.keys()].filter((i) => f.ground[i] === BRIDGE).map((i) => i % f.cols))].sort((a, b) => a - b);
    const spans = cols.filter((x, k) => k === 0 || x - cols[k - 1] > 2).length;
    expect(spans).toBeGreaterThanOrEqual(3);
  });
  it('the record\'s crossings give a bridge with its road at each, and nowhere else (b439)', () => {
    const t = sampleTicket('forest', 1.3, { river: true, seed: 5 });
    const plain = buildField(t.place, t.seed), more = buildField({ ...t.place, crossings: [0.12, 0.85] }, t.seed);
    const BRIDGE = GROUNDS.indexOf('bridge'), ROAD = GROUNDS.indexOf('road');
    for (const x of [0.12, 0.85]) {
      const c = Math.floor((x * more.w) / more.cell);
      const col = [...Array(more.rows).keys()].map((r) => more.ground[r * more.cols + c]);
      expect(col.filter((k) => k === BRIDGE).length).toBeGreaterThan(0);
      expect(col.filter((k) => k === ROAD).length).toBeGreaterThan(8);
      // The river is crossed by the bridge from road to road: nothing else in that column is water.
      expect(col.some((k) => k === GROUNDS.indexOf('water'))).toBe(false);
    }
    // Elsewhere the field is as it was.
    let other = 0;
    for (let i = 0; i < plain.ground.length; i++) {
      const c = i % plain.cols;
      if (Math.abs(c - Math.floor((0.12 * plain.w) / plain.cell)) > 1 && Math.abs(c - Math.floor((0.85 * plain.w) / plain.cell)) > 1 && plain.ground[i] !== more.ground[i]) other++;
    }
    expect(other).toBe(0);
  });
  it('the ticket says which corps cross the river as the card counts them: one with no river before it does not (b464(5))', () => {
    const g = playing();
    let partial = 0;
    for (const r of g.regions) {
      if (!hostileUnits(g, r, 'GER').length) continue;
      const cs = g.corps.filter((c) => c.nat === 'GER' && !c.dead && !c.town && c.ap > 0 && r.nb.includes(c.reg) && !blocked(g, c, r, c.reg));
      const t = cs.length ? landTicket(g, cs, r) : null;
      if (!t) continue;
      for (const f of t.att.forces) {
        const from = g.corps.find((c) => c.id === f.corps)!.reg;
        expect(f.cross === false).toBe(!!t.place.river && !riverBetween(g, from, r.id));
      }
      const m = /^(\d+) of the (\d+) attacking corps cross/.exec(t.lines.find((l) => l.label === 'River')?.value ?? '');
      if (m) { expect(t.att.forces.filter((f) => f.cross !== false).length).toBe(Number(m[1])); partial++; }
      // The end of the field those that do not cross take, where some do not (the river bends away from it).
      expect(t.place.dryEnd !== undefined).toBe(t.att.forces.some((f) => f.cross === false) && t.att.forces.some((f) => f.cross !== false));
    }
    // Pomorze in the first month: XIX Corps from Farther Pomerania, XXI Corps over the Vistula from Elbing.
    expect(partial).toBeGreaterThan(0);
  });
  it('the ticket carries the card\'s own reckoning and the province', () => {
    const g = playing();
    const { c, r } = anAttack(g);
    const t = landTicket(g, [c], r)!;
    const a = previewAttack(g, [c], r)!;
    expect(t.reckoning.odds).toBeCloseTo(a.odds);
    expect(t.reckoning.A).toBeCloseTo(a.A);
    expect(t.place.terrain).toBe(r.terrain);
    expect(t.att.human).toBe(true);
    expect(t.att.forces.map((f) => f.corps)).toEqual(goingIn(g, [c], r).map((x) => x.id));
    // The defence's corps, and the town's guard where it fights beside them in the field (ruling 232): a block of its
    // own, under the guard's own id (the coordinator's ruling, 22 November).
    expect(t.def.forces.length).toBe(a.defenders.length + (a.guard ? 1 : 0));
    expect(t.def.forces.filter((f) => f.corps >= 0).map((f) => f.corps)).toEqual(a.defenders.map((x) => x.id));
    if (a.guard) {
      const guard = t.def.forces.find((f) => f.corps === guardForce(r))!;
      expect(guard.name).toBe(`Town guard of ${townName(r)}`);
      expect(guard.men).toBe(guardMen(g, r));
      expect(guard.dug).toBe(true);
    }
    expect(t.att.forces.reduce((s, f) => s + f.share, 0)).toBeCloseTo(1);
    expect(t.def.forces.reduce((s, f) => s + f.share, 0)).toBeCloseTo(1);
  });
  it('the town\'s guard that fights beside the field army stands on the field and pays the share its side lost there (ruling 232, b464)', () => {
    const g = playing();
    const { c, r } = anAttack(g);
    const a = previewAttack(g, [c], r)!;
    expect(a.guard ?? 0).toBeGreaterThan(0);
    const t = landTicket(g, [c], r)!;
    const b = new LandBattle(t);
    const guards = b.units.filter((u) => u.corps === guardForce(r));
    expect(guards.length).toBeGreaterThan(0);
    expect(guards.every((u) => u.side === 'def' && u.dug && u.name.startsWith('Town guard of'))).toBe(true);
    // No secret on a fogged field (the card names it), and no corps on the card's count.
    expect(cardKnows(g, t).has(guardForce(r))).toBe(true);
    expect(corpsOn(t.def)).toBe(t.def.forces.length - 1);
    b.ai.att = b.ai.def = true;
    runOut(b);
    const o = b.over!;
    expect(o.forces!.some((f) => f.corps === guardForce(r))).toBe(true);
    const men = guardMen(g, r);
    const rep = handBack(g, [c], r, o)!;
    const row = rep.defenders.find((u) => (u as { guard?: boolean }).guard)!;
    expect(row).toBeDefined();
    if (!rep.taken) expect(row.lost).toBe(Math.round(men * o.lost.def));
    else expect(row.fate).toBe('surrendered');
  });
  it('a fought battle comes back through the rules: the report carries the field\'s R, and the ground follows it', () => {
    const g = playing();
    const { c, r } = anAttack(g);
    const t = landTicket(g, [c], r)!;
    const b = new LandBattle(t);
    b.ai.att = true;
    runOut(b);
    const rep = handBack(g, [c], r, b.over!)!;
    expect(rep).not.toBeNull();
    expect(rep.odds).toBeCloseTo(b.over!.R);
    expect(rep.taken || rep.siege === true).toBe(b.over!.R >= 1.2);
    // A capital or a city was fought in its streets: the report says so, for the after card's words (the third playtester).
    expect(rep.streets).toBe(streetsOf(t.place) ?? undefined);
  });
  it('a second attack on the same place this month keeps its ground and draws a fresh day\'s luck (the review, 21:55)', () => {
    // Over a handful of places: an attack thrown back leaves a report, and the next ticket for that place counts it.
    let differs = 0, n = 0;
    for (const seed of [3, 4, 5, 6, 7]) {
      const g = playing(seed);
      const { c, r } = anAttack(g);
      const t0 = landTicket(g, [c], r)!;
      expect(t0.attempt).toBe(g.reports.filter((x) => x.turn === g.turn && x.region === r.id && x.attNat === t0.att.nat).length);
      g.reports.push({ ...(g.reports[0] ?? {}), turn: g.turn, region: r.id, attNat: t0.att.nat, taken: false } as (typeof g.reports)[number]);
      const t1 = landTicket(g, [c], r)!;
      expect(t1.attempt).toBe((t0.attempt ?? 0) + 1);
      expect(t1.seed).toBe(t0.seed);
      expect(new LandBattle(t1).field.cols).toBe(new LandBattle(t0).field.cols);
      if (new LandBattle(t1).fortune !== new LandBattle(t0).fortune) differs++;
      n++;
    }
    expect(differs).toBe(n);
  });
  it('the after card\'s headline and its field line speak in one measure, within a fifth (ruling 226(6))', () => {
    let n = 0;
    for (const seed of [3, 5, 7, 9, 11]) {
      const g = playing(seed);
      const { c, r } = anAttack(g);
      const t = landTicket(g, [c], r);
      if (!t) continue;
      const b = new LandBattle(t);
      b.ai.att = true;
      runOut(b);
      const rep = handBack(g, [c], r, b.over!)!;
      const sent = rep.attackers.reduce((a, u) => a + u.men, 0), lost = rep.attackers.reduce((a, u) => a + u.lost, 0);
      const k = lost / sent / Math.max(1e-6, b.over!.lost.att);
      expect(k).toBeGreaterThan(1 / 1.2);
      expect(k).toBeLessThan(1.2);
      n++;
    }
    expect(n).toBeGreaterThanOrEqual(3);
  });
  it('a corps of one division fights under that division\'s own name: 7th Armoured\'s regiments are 7th Armoured\'s (b464(3))', () => {
    const g = playing();
    const { c, r } = anAttack(g);
    // The cold check's Mechili: the British 7th Armoured Division, a corps of one division, alone in front of the attack.
    const seventh = g.corps.find((x) => x.nat === 'UK' && x.name === '7th Armoured Division' && !x.dead)!;
    const was = g.regions[seventh.reg];
    was.units = was.units.filter((u) => u !== seventh);
    seventh.reg = r.id;
    r.units = r.units.filter((u) => u.nat === 'GER').concat(seventh);
    const t = landTicket(g, [c], r)!;
    // (Beside it, the town's guard where it fights in the field: ruling 232, a block of its own.)
    expect(t.def.forces.filter((f) => f.corps >= 0).map((f) => f.divisions!.map((d) => d.name))).toEqual([['7th Armoured Division']]);
    const blocks = new LandBattle(t).units.filter((u) => u.side === 'def' && u.corps >= 0);
    expect(blocks.length).toBe(LAND.blocks.regiments);
    expect(blocks.map((u) => u.name)).toEqual(['1st', '2nd', '3rd'].map((n) => `${n} Regiment, 7th Armoured`));
    expect(blocks.every((u) => u.parent === '7th Armoured')).toBe(true);
  });
  it('an army that broke pays the rules\' bill for that result, or the field\'s if larger; the winner pays the field\'s (b464)', () => {
    type Rep = NonNullable<ReturnType<typeof handBack>>;
    const lostBy = (x: Rep, s: 'att' | 'def'): number => (s === 'att' ? Math.max(x.attLost, x.attackers.reduce((a, u) => a + u.lost, 0)) : Math.max(x.defLost, x.defenders.reduce((a, u) => a + u.lost, 0)));
    let n = 0, pursued = 0, same = 0;
    for (const seed of [3, 5, 7, 9, 11, 13, 15, 17]) {
      const g = playing(seed), twin = playing(seed);
      const { c, r } = anAttack(g);
      const t = landTicket(g, [c], r);
      if (!t) continue;
      const b = new LandBattle(t);
      b.ai.att = b.ai.def = true;
      runOut(b);
      const o = b.over!;
      const rep = handBack(g, [c], r, o)!;
      const s = o.broke;
      if (!s) { expect(pursuitLine(t, o, rep)).toBeNull(); continue; }
      expect(s).toBe(o.winner === 'att' ? 'def' : 'att');
      // SEND THEM IN from the same save: the staff's roll is the odds the beaten army is billed at, never the field's own
      // (b476(2)). Where the roll gives the same result, the beaten army pays the staff's bill, or the field's if larger.
      const tw = anAttack(twin);
      const staff = orderAttack(twin, [tw.c], tw.r)!;
      const men = (x: 'att' | 'def'): number => t[x].forces.reduce((a, f) => a + f.men, 0);
      const field = o.lost[s] * men(s);
      if (staff.taken === rep.taken) { expect(lostBy(rep, s) / Math.max(lostBy(staff, s), field)).toBeCloseTo(1, 1); same++; }
      // The winner as before: within a fifth of what the field cost it.
      const w = s === 'att' ? 'def' : 'att';
      if (o.lost[w] > 0.01) { const k = lostBy(rep, w) / (o.lost[w] * men(w)); expect(k).toBeGreaterThan(1 / 1.2); expect(k).toBeLessThan(1.2); }
      // The after card says where the rest came from, in the UI director's words, only where there is a rest.
      const line = pursuitLine(t, o, rep);
      const more = lostBy(rep, s) - Math.round(field);
      if (more > Math.max(10, field * 0.01)) {
        expect(line).toBe(`After ${t[s].human ? 'your army' : `the ${t[s].adj} army`} broke, it lost ${more.toLocaleString('en-GB')} more men in the pursuit and as prisoners.`);
        expect(line).toMatch(/^After (your|the [A-Z][a-z]+) army broke, it lost \d{1,3}(,\d{3})* more men in the pursuit and as prisoners\.$/);
        pursued++;
      } else expect(line).toBeNull();
      n++;
    }
    expect(n).toBeGreaterThanOrEqual(2);
    expect(same).toBeGreaterThanOrEqual(2);
    expect(pursued).toBeGreaterThanOrEqual(1);
  });
  it('a broken army of several corps pays the whole of the rules\' bill, though the field hit one corps hardest (b464)', () => {
    // Poznan, September 1939: Army Poznan and the Wielkopolska Cavalry at long odds. The field hits the cavalry hardest,
    // so its share of the bill reaches the most a corps can lose; the rest of the pursuit falls on Army Poznan.
    type Rep = NonNullable<ReturnType<typeof handBack>>;
    const lost = (x: Rep): number => Math.max(x.defLost, x.defenders.reduce((a, u) => a + u.lost, 0));
    let n = 0;
    for (const seed of [4, 6, 7]) {
      const g = playing(seed), twin = playing(seed);
      const r = g.regions.find((x) => x.name === 'Poznan')!, cs = candidates(g, r);
      const t = landTicket(g, cs, r);
      if (!t || t.def.forces.length < 2) continue;
      const b = new LandBattle(t);
      b.ai.att = b.ai.def = true;
      runOut(b);
      const o = b.over!;
      if (o.broke !== 'def') continue;
      const rep = handBack(g, cs, r, o)!;
      // SEND THEM IN from the same save: the staff's roll (b476(2)).
      const staff = orderAttack(twin, candidates(twin, twin.regions[r.id]), twin.regions[r.id])!;
      if (!staff.taken) continue;
      expect(lost(rep) / lost(staff)).toBeCloseTo(1, 1);
      n++;
    }
    expect(n).toBeGreaterThanOrEqual(2);
  });
  it('an army pulled out after contact pays the rules\' bill for that defeat times its bar squared, or its field loss (b474)', () => {
    type Rep = NonNullable<ReturnType<typeof handBack>>;
    const lost = (x: Rep, s: 'att' | 'def'): number => (s === 'att' ? Math.max(x.attLost, x.attackers.reduce((a, u) => a + u.lost, 0)) : Math.max(x.defLost, x.defenders.reduce((a, u) => a + u.lost, 0)));
    // Pomorze as the Poles, the Germans' XIX and XXI Corps attacking: WITHDRAW pressed at a bar, or before the armies meet.
    const out = (at: number): { rep: Rep; staff: Rep; field: number; bar: number | null; line: string | null } => {
      const g = playing(3), twin = playing(3);
      const pick = (x: typeof g) => { const r = x.regions.find((y) => y.name === 'Pomorze')!; return { r, cs: candidates(x, r).filter((c) => c.name === 'XIX Corps' || c.name === 'XXI Corps') }; };
      const p = pick(g), t = landTicket(g, p.cs, p.r)!;
      t.att.human = false; t.def.human = true;
      const b = new LandBattle(t);
      b.ai.att = b.ai.def = true;
      for (let i = 0; i < 20000 && !b.over; i++) { if (at === 0 && b.t > 3) break; b.step(); if (at > 0 && b.contactAt !== null && b.breaking('def') >= at) break; }
      expect(b.over).toBeNull();
      expect(b.command({ do: 'withdrawAll', side: 'def' }, 'def')).toBe(true);
      const o = b.over!;
      expect(o.how).toBe('withdrew');
      const rep = handBack(g, p.cs, p.r, o)!, q = pick(twin);
      // The staff's bill: SEND THEM IN from the same save, at its own roll (b476(2)).
      return { rep, staff: orderAttack(twin, q.cs, q.r)!, field: o.lost.def * t.def.forces.reduce((a, f) => a + f.men, 0), bar: o.pulled?.bar ?? null, line: pursuitLine(t, o, rep) };
    };
    // Before the armies meet: the field's loss only, and nothing to tell.
    const early = out(0);
    expect(early.bar).toBeNull();
    expect(lost(early.rep, 'def')).toBeLessThanOrEqual(Math.round(early.field * 1.15) + 1);
    expect(early.line).toBeNull();
    for (const at of [0.3, 0.6, 0.9]) {
      const x = out(at);
      expect(x.bar!).toBeGreaterThanOrEqual(at);
      const want = Math.max(x.field, lost(x.staff, 'def') * x.bar! ** 2);
      expect(lost(x.rep, 'def') / want).toBeGreaterThan(0.97);
      expect(lost(x.rep, 'def') / want).toBeLessThan(1.03);
      // Pulled out early, it pays its field loss and no more is told; late, the rearguard's line says what the rest cost.
      if (at === 0.3) expect(x.line).toBeNull();
      else expect(x.line).toMatch(/^As your army withdrew under fire, it lost \d{1,3}(,\d{3})* more men in the rearguard and as stragglers\.$/);
    }
  });
  it('an attack called off under fire pays its own bill for that repulse the same way (b474)', () => {
    type Rep = NonNullable<ReturnType<typeof handBack>>;
    const lost = (x: Rep): number => Math.max(x.attLost, x.attackers.reduce((a, u) => a + u.lost, 0));
    const call = (bill?: FieldBill, fought: number | null = 0.9): Rep => { const g = playing(5); const { c, r } = anAttack(g); return orderAttack(g, [c], r, fought ?? undefined, bill)!; };
    // The rules' bill for that defeat at the staff's own roll, as SEND THEM IN draws it (b476(2)), not at the field's ratio.
    const staff = lost(call(undefined, null));
    for (const bar of [0.4, 0.8, 1]) {
      const rep = call({ corps: {}, att: 0.02, def: 0.01, whole: true, pulled: { side: 'att', bar } });
      expect(lost(rep) / Math.max(staff * bar * bar, 1)).toBeCloseTo(1, 1);
    }
  });
  it('with no battle fought, the rules run exactly as before', () => {
    const g1 = playing(5), g2 = playing(5);
    const a1 = anAttack(g1), a2 = anAttack(g2);
    const r1 = orderAttack(g1, [a1.c], a1.r), r2 = orderAttack(g2, [a2.c], a2.r, undefined);
    expect(JSON.stringify(r1)).toBe(JSON.stringify(r2));
    expect(g1.rng.state).toBe(g2.rng.state);
  });
  it('"the cost fell evenly" weighs each corps\' share of its own men, not men lost (b464(6))', () => {
    // Mlawa as the cold check fought it: I Corps (three divisions) and Corps Wodrig (two), the field's own split.
    const paid = (sI: number, sW: number): { text: string; rows: string } => {
      const g = playing(3);
      const r = g.regions.find((x) => x.name === 'Mlawa')!;
      const cs = candidates(g, r).filter((c) => c.name === 'I Corps' || c.name === 'Corps Wodrig');
      expect(cs.length).toBe(2);
      const men = (c: Corps): number => (c.name === 'I Corps' ? 45000 : 30000), s = (c: Corps): number => (c.name === 'I Corps' ? sI : sW);
      const att = cs.reduce((a, c) => a + men(c) * s(c), 0) / cs.reduce((a, c) => a + men(c), 0);
      const rep = orderAttack(g, cs, r, 1.6, { corps: Object.fromEntries(cs.map((c) => [c.id, s(c)])), att, def: 0.22, whole: true })!;
      return { text: rep.paid!.att, rows: rep.attackers.map((u) => `${u.name} ${u.lost}/${u.men}`).join(', ') };
    };
    // The cold check's Mlawa: Wodrig lost 13.7% of its men and the most men, I Corps 7.1%. It used to read "evenly".
    expect(paid(0.071, 0.137).text).toBe('Corps Wodrig paid most of it.');
    expect(paid(0.08, 0.08).text).toBe('The cost fell evenly on your corps.');
    // Wodrig hit harder for its size but I Corps lost more men: neither "evenly" nor "paid most of it" is true.
    const split = paid(0.1, 0.138);
    expect(split.text, split.rows).toBe('');
  });
});
