// Field telephone lines (release 2; Brian, 22 November 17:33Z: "lets do all three but make it where we can toggle it
// off if someone doesnt want it"). The design director's rulings of 18:36Z, PHONES: Q1 a shell or mortar blow on the
// wire's end block cuts at 0.12 (a knob), an enemy tank crossing cuts outright; Q2 the fallback by army as read; Q3 P6
// in, Germany's double lines (two cuts before orders wait), every other army one line. As the clock starts every block
// of the defence on the field has a wire from the headquarters at the middle of its zone's back edge; while it is down
// every order to that block, the player's or a staff's, waits the army's runner or wireless; a lineman walks out at the
// first cut, splices it in 30 s, and what was waiting goes at once. Off, nothing of it is read: the battle of ec6f8e9c
// to the last number.
import { describe, expect, it } from 'vitest';
import type { Command, FieldEvent, FieldTicket, Side } from '../src/battle/core';
import { LAND, LandBattle, phoneFallback, phoneLines, sampleTicket, withJoiners, withTeams, type LandPlace, type LandUnit, type Wire } from '../src/battle/land';
import { thinkFor } from '../src/battle/land/ai';
import { newCampaign, offers } from '../src/battle/ui/campaign';
import { orderAttack } from '../src/campaign/turn';
import { turnToDate } from '../src/core/ticket';
import { Rng, hashString } from '../src/core/rng';
import { battleCalls, unitOut } from '../unreal/TheaterCommand/Rules/battle-bridge';

type T = FieldTicket<LandPlace>;
const env = (globalThis as { process?: { env: Record<string, string | undefined>; stdout?: { write(s: string): void } } }).process;
const PRINT = !!env?.env.PRINT;

const live = (u: LandUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';

/** The rules' private workings a test reads. */
interface Inside {
  rng: Rng;
  phoneRng: Rng | null;
  blow(u: LandUnit, t: LandUnit): { dmg: number };
  wireBlow(u: LandUnit, t: LandUnit): void;
  wireTanks(was: [LandUnit, number, number][]): void;
  wireCut(w: Wire, x: number, y: number, by: LandUnit): void;
  layWires(): void;
  pending: { at: number; id: number; key?: string }[];
}
const inside = (b: LandBattle): Inside => b as unknown as Inside;
const steps = (b: LandBattle, n: number): void => { for (let k = 0; k < n && !b.over; k++) b.step(); };
const said = (b: LandBattle, what: string): FieldEvent[] => b.log.filter((e) => e.what === what);
const r3 = (v: number): number => Math.round(v * 1000) / 1000;

/**
 * Everything a battle shows and does, every tick (the cutters and smoke test's own, with the dice's place, every key a
 * block carries and its order on the air), less the view keys in 'drop'.
 */
function signed(b: LandBattle, before?: (b: LandBattle, k: number) => void, o: { drop?: string[] } = {}): string {
  const drop = o.drop ?? [];
  const ev: FieldEvent[] = [];
  const keys = new Set<string>();
  for (let k = 0; !b.over && k < 30000; k++) {
    before?.(b, k);
    b.step();
    const v = b.view();
    for (const x of Object.keys(v)) if (!drop.includes(x)) keys.add(x);
    ev.push(...v.events);
  }
  return JSON.stringify({
    over: b.over, t: b.t, keys: [...keys].sort(), scale: b.scale(), page: b.scale(undefined, LAND.scale.page),
    rng: inside(b).rng.state, gates: b.gates,
    units: b.units.map((u) => [u.id, u.corps, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged, u.reserve ?? null, u.power0,
      u.home, u.dry ?? null, u.order, u.path, u.cal ?? null, u.crew ?? null, u.taken ?? null, u.orderAt ?? null, Object.keys(u).sort().join(',')]),
    wrecks: b.wrecks.map((z) => [z.x, z.y, z.at]),
    said: ev.map((e) => [e.t, e.what, e.unit ?? null, e.text]),
    log: b.log.map((e) => [e.t, e.what, e.unit ?? null, e.text]),
  });
}
const digest = (s: string): string => hashString(s).toString(16).padStart(8, '0') + hashString('#' + s).toString(16).padStart(8, '0');

/** The attack with a corps of guns, a sixth of its strength (the practice armies have none). */
const withGuns = (t: T): T => ({
  ...t,
  att: {
    ...t.att,
    forces: [...t.att.forces.map((f) => ({ ...f, share: f.share * 0.85 })), { corps: 4, name: 'Artillery Corps', nat: t.att.nat, kind: 'art', divs: 1, men: 13500, str: 0.9, org: 0.85, xp: 0.3, mor: 0.65, share: 0.15, dug: false, first: 1 }],
  },
});
/** The attack on foot alone: no guns, no tanks, no mortars (a machine-gun team may stay: it never cuts). */
const footOnly = (t: T): T => {
  const fs = t.att.forces.filter((f) => f.kind !== 'arm' && f.kind !== 'art' && f.kind !== 'mor'), all = fs.reduce((a, f) => a + f.share, 0);
  return { ...t, att: { ...t.att, forces: fs.map((f) => ({ ...f, share: f.share / all })) } };
};

// ---- (1) off: the battle of ec6f8e9c ------------------------------------------------------------------------------

/** The defence the player's, his own orders to his blocks as the fight goes (with radio on, they go on the air). */
const playerOrders = (b: LandBattle, k: number): void => {
  const mine = b.units.filter((u) => u.side === 'def' && u.kind === 'inf' && live(u) && u.state !== 'routing');
  if (k === 400 && mine[0]) b.command({ do: 'move', units: [mine[0].id], x: mine[0].x + 60, y: mine[0].y + 100 }, 'def');
  if (k === 900 && mine[1]) {
    const e = b.units.filter((u) => u.side === 'att' && live(u) && !u.hid).sort((a, c) => Math.hypot(a.x - mine[1].x, a.y - mine[1].y) - Math.hypot(c.x - mine[1].x, c.y - mine[1].y))[0];
    if (e) b.command({ do: 'attack', units: [mine[1].id], target: e.id }, 'def');
  }
  if (k === 1300 && mine[2]) b.command({ do: 'hold', units: [mine[2].id] }, 'def');
};
/** Battles fought on ec6f8e9c (scratch pin test, this very signed()): their [R, seconds, digest]. */
const CASES: [string, () => LandBattle, ((b: LandBattle, k: number) => void)?][] = [
  ['guns and teams on an American attack, a German defence, August 1944, plains 1.1 seed 3', () => {
    const b = new LandBattle({ ...withTeams(withGuns(sampleTicket('plains', 1.1, { seed: 3, nat: { att: 'USA', def: 'GER' } }))), turn: 60 });
    b.ai.att = b.ai.def = true;
    return b;
  }],
  ['the defence the player\'s with radio and his staff on, his own orders, fog, a Soviet defence 1941, hills 1.2 seed 5', () => {
    const b = new LandBattle({ ...withTeams(withGuns(sampleTicket('hills', 1.2, { seed: 5, nat: { att: 'GER', def: 'SOV' } }))), turn: 22 });
    b.ticket.def.human = true; b.fog = true; b.radio = true; b.perUnit = true;
    b.ai.att = b.ai.def = true;
    return b;
  }, playerOrders],
  ['a British attack with guns on an Italian city, 1943, odds 1.0 seed 11', () => {
    const b = new LandBattle({ ...withGuns(sampleTicket('urban', 1.0, { seed: 11, town: 'city', nat: { att: 'UK', def: 'ITA' } })), turn: 47 });
    b.ai.att = b.ai.def = true;
    return b;
  }],
];
const BEFORE: Record<string, [number, number, string]> = {
  [CASES[0][0]]: [1.138, 283, 'fc330a0c85ffcdab'],
  [CASES[1][0]]: [1.44, 301, 'aa91223841a2ed75'],
  [CASES[2][0]]: [1.2, 376, 'a39165d997886d3a'],
};
/** The first case with the men drawn (sections), fought on ec6f8e9c: views taken, rounds listed, the views' hash. */
const DRILL_BEFORE: [number, number, string] = [283, 122, '3a17a2e6'];
const fought = (i: number, on = false): { b: LandBattle; s: string } => {
  const [, make, during] = CASES[i];
  const b = make();
  b.phones = on;
  return { b, s: signed(b, during, on ? { drop: ['phones'] } : {}) };
};

describe('(1) switched off: the battle of ec6f8e9c, and nothing of the wires anywhere', () => {
  CASES.forEach(([name], i) => it(name, () => {
    const { b, s } = fought(i);
    const got: [number, number, string] = [Math.round(b.over!.R * 1000) / 1000, b.over!.seconds, digest(s)];
    if (PRINT) { env?.stdout?.write(`    ${JSON.stringify(name)}: ${JSON.stringify(got)},\n`); return; }
    expect(got).toEqual(BEFORE[name]);
    expect(b.wires.length + b.wireCuts.length + b.wireMends.length + b.wireHeld.length).toBe(0);
    expect(b.log.some((e) => e.what === 'land:wire' || e.what === 'land:mend')).toBe(false);
    expect(Object.keys(b.view())).not.toContain('phones');
    expect(b.phonesView()).toEqual({});
    expect(inside(b).phoneRng).toBeNull();
    for (const u of b.units) { expect(b.wireOf(u)).toBeNull(); expect(b.wireDown(u)).toBe(false); expect(unitOut(u, b.t, u.side, b).wire).toBeUndefined(); }
  }, 300_000));
  it('the men drawn: the drill of ec6f8e9c, and no lineman', () => {
    const b = CASES[0][1]();
    const d = b.drillOn({ sections: true });
    let h = 0, n = 0, ln = 0;
    for (let k = 0; !b.over && k < 30000; k++) {
      b.step();
      if (k % 10 === 0) { const v = d.view(); ln += v.filter((x) => x.ln).length; h = hashString(h + ':' + JSON.stringify(v)); n++; }
    }
    expect([n, d.rounds().length, h.toString(16)]).toEqual(DRILL_BEFORE);
    expect(ln).toBe(0);
  }, 300_000);
});

// ---- (2) on, with nothing on the attack that cuts: the same battle -------------------------------------------------

describe('(2) on, with no guns, mortars or tanks on the attack: the same battle, wires laid and never cut', () => {
  const pairs: [string, () => T, ((b: LandBattle) => void)?, ((b: LandBattle, k: number) => void)?][] = [
    ['foot and a machine-gun team on a German defence, plains 1.1 seed 3', () => ({ ...footOnly(withTeams(sampleTicket('plains', 1.1, { seed: 3, nat: { att: 'USA', def: 'GER' } }))), turn: 60 })],
    ['the defence the player\'s with radio on, his own orders, hills 1.0 seed 8', () => footOnly(withTeams(sampleTicket('hills', 1.0, { seed: 8 }))), (b) => { b.ticket.def.human = true; b.radio = true; b.perUnit = true; }, playerOrders],
  ];
  for (const [name, make, setup, during] of pairs) it(name, () => {
    const run = (on: boolean): { b: LandBattle; s: string } => {
      const b = new LandBattle(make());
      b.ai.att = b.ai.def = true; b.phones = on;
      setup?.(b);
      return { b, s: signed(b, during, { drop: ['phones'] }) };
    };
    const off = run(false), on = run(true);
    expect(on.s).toBe(off.s);
    expect(on.b.wires.length).toBe(on.b.units.filter((u) => u.side === 'def').length);
    expect(on.b.wireCuts.length + on.b.wireHeld.length).toBe(0);
    expect(on.b.view().phones!.wires.length).toBe(on.b.wires.length);
  }, 300_000);
});

// ---- (3) what cuts a wire -----------------------------------------------------------------------------------------

/**
 * A rig for the cuts: an American attack with guns and teams on a defence of 'def' in 'turn', nobody's staff thinking,
 * the phones on and the wires laid (as the first step lays them). 'quiet': the attack's blows cut nothing (its guns
 * reach the wires from the first tick), so the only cuts are the test's own.
 */
function rig(def = 'SOV', turn = 60, o: { humanDef?: boolean; seed?: number; quiet?: boolean } = {}): LandBattle {
  const t = { ...withTeams(withGuns(sampleTicket('plains', 1.0, { seed: o.seed ?? 2, nat: { att: 'USA', def } }))), turn };
  if (o.humanDef) { t.att = { ...t.att, human: false }; t.def = { ...t.def, human: true }; }
  const b = new LandBattle(t);
  b.ai.att = b.ai.def = false; b.phones = true;
  inside(b).layWires();
  if (o.quiet) inside(b).wireBlow = (): void => {};
  return b;
}
const of = (b: LandBattle, side: Side, k: (u: LandUnit) => boolean): LandUnit => b.units.find((u) => u.side === side && k(u) && live(u))!;
const gunOf = (b: LandBattle): LandUnit => of(b, 'att', (u) => u.kind === 'art' && !u.team);
const teamOf = (b: LandBattle, team: 'mg' | 'mor'): LandUnit => of(b, 'att', (u) => u.team === team);
const footDef = (b: LandBattle): LandUnit => of(b, 'def', (u) => u.kind === 'inf');

describe('(3) what cuts a wire: a gun\'s or a mortar team\'s blow at 0.12 from the wires\' own dice, a tank crossing outright', () => {
  it('the wires: one to every block of the defence on the field, from the middle of its zone\'s back edge; none to the attack', () => {
    const b = rig();
    const z = b.field.zones.def;
    expect(b.phonesHq()).toEqual([(z[0] + z[2]) / 2, z[3]]);
    expect(b.phonesHq()).toEqual([1500, 1800]);
    const def = b.units.filter((u) => u.side === 'def');
    expect(b.wires.map((w) => w.unit)).toEqual(def.map((u) => u.id));
    expect(b.wires.every((w) => w.side === 'def' && w.cut === 0 && w.lines === 1 && w.hq[0] === 1500 && w.hq[1] === 1800)).toBe(true);
    expect(b.units.filter((u) => u.side === 'att').every((u) => b.wireOf(u) === null)).toBe(true);
    // The wire follows its block: pts runs from the headquarters to where it stands now.
    const u = footDef(b);
    u.x += 120; u.y -= 40;
    expect(b.phonesView().phones!.wires.find((w) => w.unit === u.id)!.pts).toEqual([[1500, 1800], [Math.round(u.x * 10) / 10, Math.round(u.y * 10) / 10]]);
  });
  it('a gun block\'s blow cuts at 0.12, drawn from the phones stream alone, where the wire comes nearest the block\'s front', () => {
    const b = rig();
    const gun = gunOf(b), t = footDef(b), w = b.wireOf(t)!;
    const s0 = inside(b).rng.state, mine = new Rng(hashString(`phones:${b.ticket.seed}`));
    let n = 0;
    while (w.cut === 0 && n < 500) { inside(b).wireBlow(gun, t); n++; }
    // The same draws, made by hand from the same stream: the first under 0.12 cuts.
    let k = 1;
    while (mine.next() >= LAND.phones.cut) k++;
    expect(n).toBe(k);
    expect(inside(b).rng.state).toBe(s0);
    expect(b.wireDown(t)).toBe(true);
    const c = b.wireCuts[0];
    expect([c.wire, c.by, c.t]).toEqual([w.id, gun.id, 0]);
    // Its front (facing the attack, south) is nearer the attack than its middle: the wire's nearest point to it is its end.
    const fx = t.x + Math.sin(t.face) * t.d / 2, fy = t.y + Math.cos(t.face) * t.d / 2, dx = t.x - 1500, dy = t.y - 1800;
    const q = Math.max(0, Math.min(1, ((fx - 1500) * dx + (fy - 1800) * dy) / (dx * dx + dy * dy)));
    expect([c.x, c.y]).toEqual([Math.round((1500 + q * dx) * 10) / 10, Math.round((1800 + q * dy) * 10) / 10]);
    // Down, no more draws for it.
    const p0 = inside(b).phoneRng!.state;
    for (let i = 0; i < 50; i++) inside(b).wireBlow(gun, t);
    expect(inside(b).phoneRng!.state).toBe(p0);
    expect(b.wireCuts.length).toBe(1);
  });
  it('a mortar team cuts too; a machine-gun team, foot and tanks never, and never draw', () => {
    const b = rig();
    const t = footDef(b), w = b.wireOf(t)!;
    const p0 = inside(b).phoneRng!.state;
    for (const u of [teamOf(b, 'mg'), of(b, 'att', (x) => x.kind === 'inf'), of(b, 'att', (x) => x.kind === 'arm')]) for (let i = 0; i < 300; i++) inside(b).wireBlow(u, t);
    expect(inside(b).phoneRng!.state).toBe(p0);
    expect(w.cut).toBe(0);
    let n = 0;
    while (w.cut === 0 && n < 500) { inside(b).wireBlow(teamOf(b, 'mor'), t); n++; }
    expect(w.cut).toBe(1);
    expect(b.wireCuts[0].by).toBe(teamOf(b, 'mor').id);
  });
  it('in the fight: every cut is a gun block\'s, a mortar team\'s or a tank\'s of the attack, and the guns do cut', () => {
    const b = CASES[0][1]();
    b.phones = true;
    steps(b, 30000);
    const bys = b.wireCuts.map((c) => b.unit(c.by)!);
    expect(bys.length).toBeGreaterThan(0);
    expect(bys.every((u) => u.side === 'att' && ((u.kind === 'art' && (!u.team || u.team === 'mor')) || u.kind === 'arm'))).toBe(true);
    expect(bys.some((u) => u.kind === 'art' && !u.team)).toBe(true);
  }, 300_000);
  it('a tank block whose step crosses the wire cuts it outright, where it crosses; one that does not, never', () => {
    const b = rig();
    const tank = of(b, 'att', (u) => u.kind === 'arm'), t = footDef(b), w = b.wireOf(t)!;
    // Half way along the wire, 20 m to its west, driving east across it.
    const mx = (1500 + t.x) / 2, my = (1800 + t.y) / 2;
    const dx = t.x - 1500, dy = t.y - 1800, l = Math.hypot(dx, dy), [nx, ny] = [-dy / l, dx / l];
    const near = (k: number): [number, number] => [mx + nx * k, my + ny * k];
    // Alongside it, never across: nothing.
    [tank.x, tank.y] = near(30);
    inside(b).wireTanks([[tank, ...near(20)]]);
    expect(w.cut).toBe(0);
    const p0 = inside(b).phoneRng!.state;
    [tank.x, tank.y] = near(10);
    inside(b).wireTanks([[tank, ...near(-10)]]);
    expect(w.cut).toBe(1);
    expect(b.wireDown(t)).toBe(true);
    const c = b.wireCuts[0];
    expect(c.by).toBe(tank.id);
    expect(Math.hypot(c.x - mx, c.y - my)).toBeLessThan(0.2);
    expect(inside(b).phoneRng!.state).toBe(p0);
  });
  it('a tank driving across a wire in the fight cuts it as it goes', () => {
    const b = rig('SOV', 60, { quiet: true });
    const tank = of(b, 'att', (u) => u.kind === 'arm'), t = b.units.filter((u) => u.side === 'def' && u.kind === 'inf').sort((a, c) => a.x - c.x)[0], w = b.wireOf(t)!;
    // The tank set down 30 m west of the wire, behind the defence's line (three tenths of the way out from the
    // headquarters, clear of every block), and sent east across it.
    const mx = 1500 + (t.x - 1500) * 0.3, my = 1800 + (t.y - 1800) * 0.3;
    tank.x = mx - 30; tank.y = my;
    b.command({ do: 'move', units: [tank.id], x: mx + 60, y: my }, 'att');
    for (let k = 0; k < 200 && w.cut === 0 && !b.over; k++) b.step();
    expect(w.cut).toBe(1);
    expect(b.wireCuts[0].by).toBe(tank.id);
  });
});

// ---- (4) Germany's double lines -----------------------------------------------------------------------------------

describe('(4) a German wire is two lines: one cut leaves orders going, the second puts it down', () => {
  it('the lines by army: Germany two, every other army one', () => {
    for (const n of ['USA', 'UK', 'CAN', 'AUS', 'NZL', 'SAF', 'IND', 'SOV', 'JAP', 'ITA', 'FRA', 'VIC', 'RED', 'POL']) expect(phoneLines(n)).toBe(1);
    expect(phoneLines('GER')).toBe(2);
    expect(rig('GER').wires.every((w) => w.lines === 2)).toBe(true);
  });
  it('the player\'s order to a German block goes at once after one cut, and waits after the second', () => {
    const b = rig('GER', 60, { humanDef: true, quiet: true });
    steps(b, 10);
    const t = footDef(b), w = b.wireOf(t)!, gun = gunOf(b);
    inside(b).wireCut(w, t.x, t.y + 30, gun);
    expect([w.cut, b.wireDown(t)]).toEqual([1, false]);
    expect(said(b, 'land:wire').map((e) => e.text)).toEqual([`One of the two telephone lines to ${t.name} is cut. Orders still go by the other.`]);
    expect(b.command({ do: 'move', units: [t.id], x: t.x, y: t.y + 50 }, 'def')).toBe(true);
    expect(t.orderAt).toBeUndefined();
    expect(t.order?.kind).toBe('move');
    inside(b).wireCut(w, t.x, t.y + 30, gun);
    expect([w.cut, b.wireDown(t)]).toEqual([2, true]);
    // Germany in 1944: the wireless (Feldfu b/c), 5 s x its army's 1.25.
    expect(said(b, 'land:wire')[1].text).toBe(`The telephone line to ${t.name} is cut. Orders go by radio until it is mended.`);
    expect(b.command({ do: 'move', units: [t.id], x: t.x, y: t.y + 80 }, 'def')).toBe(true);
    expect(t.orderAt).toBe(r3(b.t + 5 * 1.25));
    // One lineman, sent at the first cut; the second adds none.
    expect(b.wireMends.length).toBe(1);
  });
});

// ---- (5) the fallbacks --------------------------------------------------------------------------------------------

describe('(5) a down wire\'s orders go by the army\'s fallback, by its year: the wireless or a runner', () => {
  it('the table: USA and Germany by runner to 1942 and by the wireless from 1943; Britain and her kin the wireless; the rest runners', () => {
    for (const [n, y42, y43] of [['USA', 'runner', 'radio'], ['GER', 'runner', 'radio'], ['UK', 'radio', 'radio'], ['CAN', 'radio', 'radio'], ['AUS', 'radio', 'radio'], ['NZL', 'radio', 'radio'],
      ['SAF', 'radio', 'radio'], ['IND', 'radio', 'radio'], ['SOV', 'runner', 'runner'], ['JAP', 'runner', 'runner'], ['ITA', 'runner', 'runner'], ['FRA', 'runner', 'runner'], ['VIC', 'runner', 'runner'], ['RED', 'runner', 'runner'], ['POL', 'runner', 'runner']] as const) {
      expect([n, phoneFallback(n, 1939), phoneFallback(n, 1942), phoneFallback(n, 1943), phoneFallback(n, 1945)]).toEqual([n, y42, y42, y43, y43]);
    }
    // Read by the battle's year, as the radio's table is: turn 40 is December 1942, 41 January 1943.
    expect([turnToDate(40).year, turnToDate(41).year]).toEqual([1942, 1943]);
    expect(rig('USA', 40).fallbackOf(footDef(rig('USA', 40)))).toBe('runner');
    const b = rig('USA', 41);
    expect(b.fallbackOf(footDef(b))).toBe('radio');
  });
  it('a runner: the metres from the headquarters over 6 m a second; the wireless: the radio\'s wait, longer for a block whose headquarters is hit', () => {
    const s = rig('SOV');
    const u = footDef(s);
    expect(s.fallbackWait(u)).toBeCloseTo(Math.hypot(u.x - 1500, u.y - 1800) / 6, 9);
    const g = rig('UK', 30);
    const v = footDef(g);
    expect(g.fallbackOf(v)).toBe('radio');
    expect(g.fallbackWait(v)).toBe(LAND.radio.delay * 1);
    v.str = 0.4 * v.str0;
    expect(g.fallbackWait(v)).toBe(LAND.radio.delay * LAND.radio.cut);
    const j = rig('JAP', 40);
    const x = footDef(j);
    expect(j.fallbackWait(x)).toBeCloseTo(Math.hypot(x.x - 1500, x.y - 1800) / 6, 9);
  });
  it('the player\'s order waits the fallback with the radio switch off or on; with the wire up, as ever', () => {
    for (const radio of [false, true]) {
      const b = rig('SOV', 60, { humanDef: true, quiet: true });
      b.radio = radio;
      steps(b, 10);
      const u = footDef(b), w = b.wireOf(u)!;
      // Wire up: at once with the radio off, after the radio's wait with it on.
      b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 40 }, 'def');
      expect(u.orderAt).toBe(radio ? r3(b.t + LAND.radio.delay * b.radioOf(u)) : undefined);
      steps(b, 60);
      inside(b).wireCut(w, u.x, u.y, gunOf(b));
      const wait = b.fallbackWait(u), at = r3(b.t + wait);
      b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 90 }, 'def');
      expect(u.orderAt).toBe(at);
      expect(b.wireHeld.at(-1)).toEqual({ t: r3(b.t), unit: u.id, due: at });
      // An order given earlier, as shown (at), counts from then.
      b.command({ do: 'move', units: [u.id], x: u.x, y: u.y + 95, at: b.t - 2 } as Command, 'def');
      expect(u.orderAt).toBe(r3(b.t - 2 + b.fallbackWait(u)));
    }
  });
});

// ---- (6) the staffs' orders ---------------------------------------------------------------------------------------

describe('(6) a staff\'s order to a block with its wire down waits, then lands, and the staff never restarts it', () => {
  it('the computer defence sends a block home: once, held, landing at its due; the same order every think keeps its time', () => {
    const b = rig('SOV', 60, { quiet: true });
    b.ai.def = true;
    steps(b, 5);
    const u = footDef(b), w = b.wireOf(u)!;
    // Down for good (no lineman out), and pushed back off its ground: its staff sends it home every think until it goes.
    w.cut = w.lines;
    Object.assign(u, { y: u.y + 120, order: null, path: [] });
    const home = [...u.home];
    for (let k = 0; k < 15 && !b.wireHeld.length; k++) b.step();
    expect(b.wireHeld.length).toBe(1);
    const h = b.wireHeld[0], due = h.due;
    expect(due).toBe(r3(h.t + Math.hypot(u.x - 1500, u.y - 1800) / 6));
    expect(u.order).toBeNull();
    let thinks = 0;
    while (b.t < due - 0.05) { const t0 = b.t; b.step(); if (Math.floor(b.t) !== Math.floor(t0)) thinks++; }
    expect(thinks).toBeGreaterThan(20);
    // Re-given every think, never restarted: one stamp, the same due, the order not yet there.
    expect(b.wireHeld.length).toBe(1);
    expect(u.orderAt).toBe(due);
    expect(u.order).toBeNull();
    steps(b, 2);
    expect(b.wireHeld[0].went).toBeGreaterThanOrEqual(due);
    expect(b.wireHeld[0].went).toBeLessThan(due + 0.11);
    expect(u.order?.kind).toBe('move');
    expect([u.order!.x, u.order!.y]).toEqual(home);
    expect(u.orderAt).toBeUndefined();
    // It walks home, and the staff gives it nothing more.
    steps(b, 400);
    expect(b.wireHeld.length).toBe(1);
    expect(Math.hypot(u.x - home[0], u.y - home[1])).toBeLessThan(41);
  });
  it('a different order from the staff takes the place of the one on its way, and waits afresh', () => {
    const b = rig('SOV', 60, { quiet: true });
    steps(b, 5);
    const u = footDef(b), w = b.wireOf(u)!;
    w.cut = w.lines;
    const staff = (c: Command): void => { (b as unknown as { staffing: boolean }).staffing = true; b.command(c, 'def'); (b as unknown as { staffing: boolean }).staffing = false; };
    staff({ do: 'move', units: [u.id], x: u.x, y: u.y + 30 });
    const first = u.orderAt!;
    steps(b, 20);
    staff({ do: 'move', units: [u.id], x: u.x, y: u.y + 30 });
    expect(u.orderAt).toBe(first);
    staff({ do: 'move', units: [u.id], x: u.x + 10, y: u.y + 30 });
    expect(u.orderAt).toBe(r3(b.t + b.fallbackWait(u)));
    expect(b.wireHeld.length).toBe(2);
    expect(inside(b).pending.filter((p) => p.id === u.id).length).toBe(1);
  });
  it('a whole battle with the computer on both sides, guns cutting: every held order reached its block, or a later one took its place', () => {
    const b = CASES[0][1]();
    b.phones = true;
    steps(b, 30000);
    expect(b.wireHeld.length).toBeGreaterThan(0);
    for (const u of b.units) {
      const hs = b.wireHeld.filter((h) => h.unit === u.id);
      // Of each block's, all but the last went or gave way to a later one; the last went, or its block left the fight, or the day ended first.
      const last = hs.at(-1);
      if (last && last.went === undefined) expect(!live(u) || u.state === 'routing' || last.due > b.t - 1e-9).toBe(true);
    }
  }, 300_000);
  it('a corps coming up from next door has no wire: its orders go as ever', () => {
    const b = new LandBattle(withJoiners({ ...withTeams(withGuns(sampleTicket('plains', 1.0, { seed: 2, nat: { att: 'USA', def: 'SOV' } }))), turn: 60 }, { def: [{ joined: 0.9 }] }));
    b.ai.att = b.ai.def = false; b.phones = true;
    steps(b, 200);
    const j = b.units.filter((u) => u.joined !== undefined);
    expect(j.length).toBeGreaterThan(0);
    expect(j.every((u) => b.wireOf(u) === null && !b.wireDown(u))).toBe(true);
    expect(b.wires.length).toBe(b.units.filter((u) => u.side === 'def' && u.joined === undefined).length);
  });
});

// ---- (7) the lineman and the mend ---------------------------------------------------------------------------------

describe('(7) the lineman: out at the first cut, at foot pace, 30 s to splice; the mend sends what was waiting at once', () => {
  it('his times; one at a time on a wire; the mend clears every cut', () => {
    const b = rig('SOV', 60, { humanDef: true, quiet: true });
    steps(b, 10);
    const u = footDef(b), w = b.wireOf(u)!, gun = gunOf(b);
    const t0 = b.t;
    inside(b).wireCut(w, 1500 + (u.x - 1500) * 0.5, 1800 + (u.y - 1800) * 0.5, gun);
    const m = b.wireMends[0], d = Math.hypot(u.x - 1500, u.y - 1800) * 0.5;
    expect(m.t0).toBe(r3(t0));
    expect(m.t1).toBeCloseTo(t0 + d / LAND.speed.inf, 1);
    expect(m.t).toBe(r3(m.t1 + LAND.phones.splice));
    expect(LAND.phones.splice).toBe(30);
    // A second cut while he is out: no second lineman.
    steps(b, 20);
    inside(b).wireCut(w, u.x, u.y, gun);
    expect(b.wireMends.length).toBe(1);
    expect(w.cut).toBe(2);
    while (b.t < m.t - 1e-9) b.step();
    b.step();
    expect([w.cut, w.out, b.wireDown(u)]).toEqual([0, null, false]);
    expect(said(b, 'land:mend').map((e) => e.text)).toEqual([`The line to ${u.name} is mended.`]);
  });
  it('an order waiting for a runner longer than the splice takes goes the moment the line is mended', () => {
    const b = rig('SOV', 60, { humanDef: true, quiet: true });
    steps(b, 10);
    const u = footDef(b), w = b.wireOf(u)!;
    // Cut 60 m out from the headquarters (a tank across it there): mended in 10 + 30 s; a runner to the block takes far longer.
    const k = 60 / Math.hypot(u.x - 1500, u.y - 1800);
    inside(b).wireCut(w, 1500 + (u.x - 1500) * k, 1800 + (u.y - 1800) * k, of(b, 'att', (x) => x.kind === 'arm'));
    const m = b.wireMends[0];
    b.command({ do: 'move', units: [u.id], x: u.x - 50, y: u.y }, 'def');
    const h = b.wireHeld[0];
    expect(h.due).toBeGreaterThan(m.t + 20);
    while (b.t < m.t - 1e-9) { b.step(); expect(u.order).toBeNull(); }
    b.step();
    expect(h.went).toBe(r3(b.t - 0.1));
    expect(u.order?.kind).toBe('move');
    expect(u.orderAt).toBeUndefined();
    expect(inside(b).pending.length).toBe(0);
  });
  it('in the drill: one lineman a mend, ln 1, reel out to the cut, splice until the mend, then back along the wire and gone', () => {
    const b = rig('SOV', 60, { humanDef: true, quiet: true });
    const d = b.drillOn({ sections: true });
    steps(b, 10);
    const u = footDef(b), w = b.wireOf(u)!;
    const cx = 1500 + (u.x - 1500) * 0.5, cy = 1800 + (u.y - 1800) * 0.5;
    inside(b).wireCut(w, cx, cy, gunOf(b));
    const m = b.wireMends[0], dist = Math.hypot(m.x - 1500, m.y - 1800);
    const man = (): { p: string; x: number; y: number; k: number; ln?: number } | undefined => d.view('def').find((x) => x.id === u.id)?.ln?.[0];
    const poses: string[] = [];
    let k = 0;
    while (b.t < m.t + dist / 6 + 2 && k++ < 4000) {
      const x = man();
      if (x && poses.at(-1) !== x.p) poses.push(x.p);
      if (x && b.t < m.t1 - 0.5) expect(Math.hypot(x.x - 1500, x.y - 1800)).toBeCloseTo(((b.t - m.t0) * 6), -1);
      if (x) { expect(x.ln).toBe(1); expect(x.k).toBe(300 + m.man); }
      b.step();
    }
    expect(poses).toEqual(['reel', 'splice', 'walk']);
    expect(man()).toBeUndefined();
    // The attack never sees him (it does not see the wires) where the card did not know the defence.
    b.fog = true;
    expect(d.view('att').some((x) => x.ln)).toBe(false);
  });
});

// ---- (8) the dice, the fog, the words, the bridge -----------------------------------------------------------------

describe('(8) never the battle\'s dice; the fog; the words; the bridge', () => {
  it('nothing draws on the battle\'s own dice: with the guns cutting, one draw a blow, as ever', () => {
    const b = CASES[0][1]();
    b.phones = true;
    const I = inside(b), real = I.rng, blow = I.blow.bind(b);
    let draws = 0, blows = 0;
    I.rng = { next: () => { draws++; return real.next(); }, get state() { return real.state; } } as unknown as Rng;
    I.blow = (u: LandUnit, t: LandUnit) => { blows++; return blow(u, t); };
    steps(b, 30000);
    expect(b.wireCuts.length).toBeGreaterThan(0);
    expect(b.over!.how).not.toBe('time');
    expect(draws).toBe(blows);
  }, 300_000);
  it('replays exactly by its seed; a second attack this month (attempt) draws afresh', () => {
    const a = fought(0, true), c = fought(0, true);
    expect(a.s).toBe(c.s);
    expect(JSON.stringify(a.b.phonesView())).toBe(JSON.stringify(c.b.phonesView()));
    const t = { ...CASES[0][1]().ticket, attempt: 1 };
    const e = new LandBattle(t);
    e.phones = true;
    e.step();
    expect(inside(e).phoneRng!.state).not.toBe(new Rng(hashString(`phones:${t.seed}`)).state);
    expect(inside(e).phoneRng!.state === new Rng(hashString(`phones:${t.seed}:1`)).state || e.wireCuts.length > 0).toBe(true);
  }, 300_000);
  it('the wires are the defence\'s: it sees them, the attack only where the view is unfogged, everything with no side', () => {
    const b = rig();
    inside(b).wireCut(b.wireOf(footDef(b))!, 1500, 1500, gunOf(b));
    expect(b.phonesView('def').phones).toBeDefined();
    expect(b.phonesView().phones!.cuts[0].by).toBe(gunOf(b).id);
    expect(b.phonesView('att').phones).toBeDefined();
    b.fog = true;
    expect(b.phonesView('att')).toEqual({});
    expect(b.view('att').phones).toBeUndefined();
    expect(b.view('def').phones!.wires.length).toBe(b.wires.length);
    // Under fog the defence does not name a striker it has not met.
    b.ticket.def.human = true;
    expect(b.phonesView('def').phones!.cuts[0].by).toBeUndefined();
  });
  it('the events go to the defence only: none where the player attacks, words where he defends or nobody plays', () => {
    const att = new LandBattle({ ...withTeams(withGuns(sampleTicket('plains', 1.0, { seed: 2, humanAtt: true, nat: { att: 'USA', def: 'SOV' } }))), turn: 60 });
    att.phones = true; att.ai.att = att.ai.def = false;
    att.step();
    inside(att).wireCut(att.wireOf(footDef(att))!, 1500, 1500, gunOf(att));
    expect(att.log.filter((e) => e.what === 'land:wire').length).toBe(0);
    const def = rig('SOV', 60, { humanDef: true });
    const u = footDef(def);
    inside(def).wireCut(def.wireOf(u)!, 1500, 1500, gunOf(def));
    expect(said(def, 'land:wire')[0]).toMatchObject({ unit: u.id, side: 'def', text: `The telephone line to ${u.name} is cut. Orders go by runner until it is mended.`, extra: { wire: def.wireOf(u)!.id, down: 1, by: 'runner' } });
    const none = rig('SOV');
    inside(none).wireCut(none.wireOf(footDef(none))!, 1500, 1500, gunOf(none));
    expect(said(none, 'land:wire')[0].text).toMatch(/^The telephone line to Enemy .* is cut\. Orders go by runner until it is mended\.$/);
  });
  it('unitOut: wire 0 on one of his own blocks whose wire is down, never the enemy\'s, never while it is up', () => {
    const b = rig('SOV', 60, { humanDef: true });
    const u = footDef(b), v = of(b, 'def', (x) => x.kind === 'inf' && x !== u);
    inside(b).wireCut(b.wireOf(u)!, 1500, 1500, gunOf(b));
    expect(unitOut(u, b.t, 'def', b).wire).toBe(0);
    expect(unitOut(u, b.t, 'att', b).wire).toBeUndefined();
    expect(unitOut(v, b.t, 'def', b).wire).toBeUndefined();
    b.phones = false;
    expect(unitOut(u, b.t, 'def', b).wire).toBeUndefined();
  });
  it('the bridge: { phones: true } lays the wires on the first tick; without it there are none', () => {
    for (const on of [false, true]) {
      const g = newCampaign();
      const TC = battleCalls({ game: () => g, attack: (ids, to, f) => ({ report: orderAttack(g, ids.map((i) => g.corps.find((c) => c.id === i)!), g.regions[to], f) }) });
      const o = offers(g)[0];
      const st = TC.battleStart(o.corps.map((c) => c.id), o.target.id, on ? { phones: true } : undefined)!;
      expect((st.view as Record<string, unknown>).phones).toBeUndefined();
      TC.battleSpeed(1);
      const v = TC.battleTick(1) as { phones?: { wires: { side: string }[] }; units: { side: string; wire?: number }[]; me: string };
      expect(v.units.some((u) => u.wire !== undefined)).toBe(false);
      if (!on) { expect(v.phones).toBeUndefined(); continue; }
      // The player attacks here: the wires are the enemy's, shown to him only where the card knew the defence.
      if (v.phones) expect(v.phones.wires.length).toBe(v.units.filter((u) => u.side === 'def').length);
      expect(v.me).toBe('att');
    }
  });
});
