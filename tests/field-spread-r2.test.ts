// The field spread, step 1 and step 1b's keys (release 2; rules-builder/release2/field-spread/form.md, ruled by the
// design director 24 Nov 2026 19:47Z, "Go ahead with step 1"). One draw per battle, keyed on the field ticket's own seed
// and attempt: the fortune share the field already fights on, and one break point per side in place of LAND.spent. A
// step inside Units decide, never beside the honest win chance, bench-only through the form's steps 1-4. Off, no ticket
// carries it and every field is the one without it; the field reads each key only where the ticket carries it.
import { describe, expect, it } from 'vitest';
import { addCorps, createGame, relocate } from '../src/campaign/game';
import { landTicket } from '../src/battle/land/ticket';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import { breakPoint, levelN, setSpread, sigmaOne, spreadDraw, spreadKeys, spreadOn, zOf } from '../src/campaign/spread';
import { REF_NERVE, SPREAD, ledgerK, nerveSplit, refStaying, type NerveParts } from '../src/battle/land/spread';
import { LandBattle } from '../src/battle/land/sim';
import { LAND } from '../src/battle/land/rules';
import { startNerve, type LandUnit } from '../src/battle/land/units';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import { liveTicket } from '../src/battle/land/live';
import type { LandPlace } from '../src/battle/land/field';
import type { FieldResult, FieldTicket, NervePart, Side, SideTicket, SpreadReckoning } from '../src/battle/core/types';
import stage from '../data/campaign/stage.json';
import spreadJson from '../data/campaign/spread.json';
import campaignBridge from '../unreal/TheaterCommand/Rules/campaign-bridge.ts?raw';
import battleBridge from '../unreal/TheaterCommand/Rules/battle-bridge.ts?raw';
import landBridge from '../unreal/TheaterCommand/Rules/land-bridge.ts?raw';
import staffText from '../src/campaign/staff.ts?raw';
import stageText from '../src/campaign/stage.ts?raw';
import type { Corps, Game, Region } from '../src/campaign/types';

type T = FieldTicket<LandPlace>;
const SIDES: Side[] = ['att', 'def'];

/** The grand campaign of September 1939 with Germany's general at the table, his first month open; flags as asked. */
function war(flags: Record<string, number> = {}, seed = 3): Game {
  const g = startOpen(createGame(seed, 'GER'));
  Object.assign(g.flags, flags);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}

interface Line { target: Region; from: Region; def: Corps[]; att: Corps[] }
/** A Polish line round Lodz with German infantry in front of it, in supply (as tests/win-chance-r2.test.ts). */
function line(g: Game, defenders = 2, attackers = 2, divs = 3): Line {
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  for (const u of [...from.units]) if (u.nat === 'GER' && u.kind !== 'fort') relocate(g, u, g.byName['Berlin']);
  const def = Array.from({ length: defenders }, (_, i) => addCorps(g, 'POL', `Line ${i + 1}`, 'inf', 3, target.id, {}));
  const att = Array.from({ length: attackers }, (_, i) => addCorps(g, 'GER', `Storm ${i + 1}`, 'inf', divs, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  return { target, from, def, att };
}
const ticket = (g: Game, l: Line): T => landTicket(g, l.att, l.target)!;
/** The Lodz ticket of a plain campaign, attackers corps against two, fought headless: the computer's staffs on both sides. */
const lodz = (attackers = 2): T => { const g = war(); const t = ticket(g, line(g, 2, attackers)); t.att.human = false; t.def.human = false; return t; };
/** Re-seeded as the bench does (fs/final/benchq.ts). */
const reseed = (t: T, s: number): T => { const u = structuredClone(t); u.seed = (u.seed + s * 7919) >>> 0; return u; };
const withSpread = (t: T, k: SpreadReckoning): T => { const u = structuredClone(t); u.reckoning.spread = k; return u; };
function fight(t: T): LandBattle {
  const b = new LandBattle(structuredClone(t));
  for (let i = 0; i < 400000 && !b.over; i++) b.step();
  return b;
}
/** Steps until `secs` battle seconds after the first contact (or the end). */
function toAfterContact(b: LandBattle, secs: number): void {
  for (let i = 0; i < 400000 && !b.over && (b.contactAt === null || b.t < b.contactAt + secs); i++) b.step();
}
// The sim's private parts the tests read.
type Inside = { brink(s: Side): { bar: number; worn: number; running: number; holding: boolean }; brokenShare(s: Side): number; led: { D: Record<Side, number>; R: Record<Side, number>; K: Record<Side, number>; draw: Record<Side, number> } | null; power: Record<Side, number>; coming: { u: LandUnit }[]; blow(u: LandUnit, t: LandUnit): { dmg: number } };
const inside = (b: LandBattle): Inside => b as unknown as Inside;
const r3 = (x: number): number => Math.round(x * 1000) / 1000;

describe('the switch (stage.json spread)', () => {
  it('is off: stage.json carries spread false with its note, and a grand campaign laid out without it carries no flag', () => {
    expect((stage as { spread?: boolean }).spread).toBe(false);
    expect(typeof (stage as { _spread?: string })._spread).toBe('string');
    const g = startOpen(createGame(3, null));
    expect(g.flags.spread).toBeUndefined();
    expect(spreadOn(g)).toBe(false);
    // Bench-only: no grand start, stage reader or bridge sets it.
    for (const text of [campaignBridge, battleBridge, landBridge, staffText, stageText]) {
      expect(text.length).toBeGreaterThan(1000);
      expect(text).not.toMatch(/setSpread|flags\.spread|SPREAD_STAGE/);
    }
  });

  it('is a step inside Units decide, and never beside the honest win chance', () => {
    const none = war();
    expect(setSpread(none, true)).toBe(false);
    expect(none.flags.spread).toBeUndefined();
    const both = war({ unitsDecide: 1, winChance: 1 });
    expect(setSpread(both, true)).toBe(false);
    expect(both.flags.spread).toBeUndefined();
    expect(spreadOn(both)).toBe(false);
    const g = war({ unitsDecide: 1 });
    expect(spreadOn(g)).toBe(false);
    expect(setSpread(g, true)).toBe(true);
    expect(g.flags.spread).toBe(1);
    expect(spreadOn(g)).toBe(true);
    // Set beside it by hand, the switch still reads off (the read guard).
    g.flags.winChance = 1;
    expect(spreadOn(g)).toBe(false);
    delete g.flags.winChance;
    expect(spreadOn(g)).toBe(true);
    expect(setSpread(g, false)).toBe(true);
    expect('spread' in g.flags).toBe(false);
    expect(spreadOn(g)).toBe(false);
  });
});

describe('off writes nothing', () => {
  it('with the switch off no ticket carries reckoning.spread', () => {
    const games = [war(), war({ unitsDecide: 1 }), war({ spread: 1 }), war({ unitsDecide: 1, spread: 1, winChance: 1 })];
    const ts = games.map((g) => ticket(g, line(g)));
    for (const t of ts) expect('spread' in t.reckoning).toBe(false);
    // The flag without Units decide is no switch at all: the ticket is the plain one, key for key.
    expect(JSON.stringify(ts[2])).toBe(JSON.stringify(ts[0]));
  });

  it('with the switch on, laying the draw throws no die and writes nothing in the game', () => {
    const g = war({ unitsDecide: 1 });
    const l = line(g);
    expect(setSpread(g, true)).toBe(true);
    const rng = (g.rng as unknown as { s: number }).s, saved = saveGame(g);
    const t = ticket(g, l);
    expect((g.rng as unknown as { s: number }).s).toBe(rng);
    expect(saveGame(g)).toBe(saved);
    // Step 1 as laid: exactly { W }, the draw on the ticket's own seed and attempt.
    expect(Object.keys(t.reckoning.spread!)).toEqual(['W']);
    expect(t.reckoning.spread!.W).toEqual(spreadKeys(t).W);
    // And the ticket is otherwise the one Units decide alone gives.
    const h = war({ unitsDecide: 1 }), plain = ticket(h, line(h));
    const { spread: _s, ...rest } = t.reckoning;
    expect(JSON.stringify({ ...t, reckoning: rest })).toBe(JSON.stringify(plain));
  });
});

describe('the one draw', () => {
  it("is keyed on the ticket's own seed and attempt", () => {
    const g = war({ unitsDecide: 1 });
    const l = line(g);
    setSpread(g, true);
    const t = ticket(g, l);
    const W = t.reckoning.spread!.W;
    for (const s of SIDES) expect(W[s]).toBe(breakPoint(spreadDraw(t.seed, t.attempt).u[s], 0.08, sigmaOne()));
    // A second attack on the place this month: a fresh draw.
    const t2 = structuredClone(t);
    t2.attempt = (t.attempt ?? 0) + 1;
    const W2 = spreadKeys(t2).W;
    expect(W2.att).not.toBe(W.att);
    expect(W2.def).not.toBe(W.def);
    // Save and reload: the same draw.
    const h = loadGame(saveGame(g))!;
    const tr = landTicket(h, l.att.map((c) => h.corps.find((x) => x.id === c.id)!), h.regions[l.target.id])!;
    expect(tr.reckoning.spread).toEqual(t.reckoning.spread);
    // The nation and the date never enter (seed kept): the same keys, and the same N.
    const u = structuredClone(t);
    u.att.nat = 'SOV'; u.att.adj = 'Soviet'; u.def.nat = 'FRA'; u.def.adj = 'French'; u.date = 'June 1944'; u.turn = 58;
    for (const f of [...u.att.forces, ...u.def.forces]) f.nat = 'ITA';
    expect(spreadKeys(u)).toEqual(spreadKeys(t));
    expect(spreadKeys(u, { level: true })).toEqual(spreadKeys(t, { level: true }));
  });

  it('draws as the bench drew it', () => {
    const d = spreadDraw(4122285092);
    expect(d.u.att).toBe(0.8279341515153646);
    expect(d.u.def).toBe(0.670666839228943);
    expect(d.uF).toBe(0.1173795263748616);
    expect(breakPoint(d.u.att, 0.08, sigmaOne())).toBe(0.17396497069517813);
    expect(breakPoint(d.u.def, 0.08, sigmaOne())).toBe(0.11498139532797455);
    expect(0.8 + 0.4 * d.uF).toBe(0.8469518105499447);
    const e = spreadDraw(4122293011, 0);
    expect(breakPoint(e.u.att, 0.08, sigmaOne())).toBe(0.021983481892993324);
    expect(breakPoint(e.u.def, 0.08, sigmaOne())).toBe(0.04001313229350039);
  });

  it('holds the fortune share the field fights on', () => {
    const t0 = lodz();
    for (const attempt of [0, 1, 2]) {
      const t = structuredClone(t0);
      t.attempt = attempt;
      const [d0, d1] = t.reckoning.luck ?? t.reckoning.dice;
      expect(new LandBattle(t).fortune).toBe(1 + LAND.fortune * (d0 + (d1 - d0) * spreadDraw(t.seed, t.attempt).uF - 1));
    }
  });

  it('is lognormal about the level, held to [0.01, 0.40]', () => {
    expect(breakPoint(0.5, 0.08, sigmaOne())).toBe(0.08);
    expect(breakPoint(0.5, 0.08, 0.766)).toBe(0.08);
    const top = breakPoint(0.975, 0.08, sigmaOne());
    expect(Math.abs(top - 0.4)).toBeLessThan(1e-3);
    expect(top).toBeLessThanOrEqual(0.4);
    expect(breakPoint(0.9999, 0.08, sigmaOne())).toBe(0.4);
    expect(breakPoint(1e-12, 0.08, sigmaOne())).toBe(0.01);
    expect(sigmaOne()).toBeCloseTo(Math.log(5) / 1.96, 12);
    expect(sigmaOne()).toBeCloseTo(0.8211, 4);
  });
});

describe('the field reads it only where present', () => {
  const step1 = { W: { att: 0.13, def: 0.18 } };
  it('the plumbing test: a ticket whose spread carries only W {att 0.13, def 0.18} fights exactly as one without it', () => {
    expect(LAND.spent).toEqual({ att: 0.13, def: 0.18 });
    const broke: (Side | undefined)[] = [];
    for (const base of [lodz(2), lodz(3)]) for (const s of [1, 2]) {
      const t = reseed(base, s);
      const b0 = fight(t), b1 = fight(withSpread(t, step1));
      expect(b0.over).not.toBeNull();
      expect(b0.contactAt).not.toBeNull();
      broke.push(b0.over!.broke);
      expect(JSON.stringify(b1.over)).toBe(JSON.stringify(b0.over));
      expect(JSON.stringify(b1.log)).toBe(JSON.stringify(b0.log));
      expect(b1.t).toBe(b0.t);
    }
    // Fought out: an army broke on its mark in some of them.
    expect(broke.some((x) => x !== undefined)).toBe(true);
  });

  it('and with move 1 beside it, the same again', () => {
    const t = reseed(lodz(2), 1);
    const b0 = fight(t), b1 = fight(withSpread(t, { ...step1, move: 1 }));
    expect(JSON.stringify(b1.over)).toBe(JSON.stringify(b0.over));
    expect(JSON.stringify(b1.log)).toBe(JSON.stringify(b0.log));
    expect(b1.t).toBe(b0.t);
  });

  it('spentOf reads W: the bar is worn over W, and BreakCause.spent carries it', () => {
    const t = reseed(lodz(2), 1), W = { att: 0.05, def: 0.3 };
    const b = new LandBattle(withSpread(t, { W }));
    expect(b.spentOf('att')).toBe(0.05);
    expect(new LandBattle(structuredClone(t)).spentOf('att')).toBe(LAND.spent.att);
    toAfterContact(b, 30);
    expect(b.breakCause('att').spent).toBe(0.05);
    expect(b.breakCause('def').spent).toBe(0.3);
    for (const s of SIDES) {
      const k = inside(b).brink(s), running = inside(b).brokenShare(s);
      expect(b.breaking(s)).toBe(Math.min(k.holding ? 0.99 : 1, Math.max(running / LAND.breaks, b.wornShare(s) / W[s])));
    }
    for (let i = 0; i < 400000 && !b.over; i++) b.step();
    if (b.over?.broke) expect(b.over.brokeBy!.spent).toBe(W[b.over.broke]);
    // The key is read: the fight is not the plain one.
    const plain = fight(t);
    expect(JSON.stringify(b.over)).not.toBe(JSON.stringify(plain.over));
  });
});

describe('the mask (choice 6 A)', () => {
  it("the player sees his own bar true and the enemy's against the level his staff reads, held at 0.95 until he breaks", () => {
    const t = reseed(lodz(3), 1);
    const b = new LandBattle(withSpread(t, { W: { att: 0.4, def: 0.3 } }));
    let held = 0;
    for (let i = 0; i < 400000 && !b.over; i++) {
      b.step();
      if (i % 10) continue;
      const v = b.view('att'), plain = b.view();
      expect(v.breaking!.att).toBe(b.breaking('att'));
      expect(v.breakingBy!.att).toEqual(b.breakCause('att'));
      expect(plain.breaking!.def).toBe(b.breaking('def'));
      expect(plain.breakingBy!.def).toEqual(b.breakCause('def'));
      if ((b.over as FieldResult | null)?.broke === 'def') { expect(v.breaking!.def).toBe(1); break; }
      const shown = Math.min(0.95, Math.max(inside(b).brokenShare('def') / LAND.breaks, b.wornShare('def') / 0.08));
      expect(v.breaking!.def).toBe(shown);
      expect(v.breaking!.def).toBeLessThanOrEqual(0.95);
      const c = v.breakingBy!.def;
      expect(c.spent).toBe(0.08);
      expect(c.bar).toBe(r3(shown));
      expect([c.holding, c.held, c.heldFor, c.by]).toEqual([false, false, 0, 'none']);
      expect('ledger' in c).toBe(false);
      if (shown === 0.95 && b.breaking('def') < 0.95) held++;
    }
    // The defence was worn past the shown level while its true bar stood short of the brink: the bar told nothing.
    expect(held).toBeGreaterThan(0);
  });

  it('without the spread, or with nobody playing, every bar is the true one', () => {
    const t = reseed(lodz(2), 2);
    const plain = new LandBattle(structuredClone(t)), spread = new LandBattle(withSpread(t, { W: { att: 0.1, def: 0.12 }, shown: { att: 0.03, def: 0.03 } }));
    toAfterContact(plain, 40);
    toAfterContact(spread, 40);
    for (const s of SIDES) for (const as of ['att', 'def', undefined, null] as (Side | undefined | null)[]) {
      expect(plain.shownBar(s, as)).toBe(plain.breaking(s));
      expect(plain.causeAs(s, as)).toEqual(plain.breakCause(s));
      if (as === 'att' || as === 'def') continue;
      expect(spread.shownBar(s, as)).toBe(spread.breaking(s));
      expect(spread.causeAs(s, as)).toEqual(spread.breakCause(s));
    }
    // Seen by its own side, the true one too; by the other, the shown one.
    expect(spread.shownBar('att', 'att')).toBe(spread.breaking('att'));
    expect(spread.shownBar('att', 'def')).toBe(Math.min(0.95, Math.max(inside(spread).brokenShare('att') / LAND.breaks, spread.wornShare('att') / 0.03)));
  });

  it('live.ts sends the shown bar to the 3D game', () => {
    const piece = PIECES.find((p) => p.key === 'normandy-1944')!;
    const k: SpreadReckoning = { W: { att: 0.2, def: 0.3 }, shown: { att: 0.08, def: 0.001 } };
    const run = (me: Side | null): { views: { t: number; breaking?: Record<Side, number> }[]; seen: Map<number, { att: number; def: number; shown: number }> } => {
      const t = ticketOf(piece, 11, me);
      t.reckoning.spread = k;
      const L = liveTicket(t, { me });
      // What the rules held at each view (the feed takes its view as the step ends).
      const seen = new Map<number, { att: number; def: number; shown: number }>();
      const view = L.sim.view.bind(L.sim);
      (L.sim as { view: LandBattle['view'] }).view = (as?: Side) => {
        seen.set(Math.round(L.sim.t * 100) / 100, { att: L.sim.breaking('att'), def: L.sim.breaking('def'), shown: L.sim.shownBar('def', 'att') });
        return view(as);
      };
      const views: { t: number; breaking?: Record<Side, number> }[] = [];
      for (let T = 1; T <= 200 && !L.sim.over; T++) {
        const d = L.to(T, 100000);
        views.push(...(d.views as { t: number; breaking?: Record<Side, number> }[]));
        if (L.sim.contactAt !== null && L.sim.t > L.sim.contactAt + 60) break;
      }
      return { views, seen };
    };
    const mine = run('att');
    let masked = 0;
    for (const v of mine.views) {
      const at = mine.seen.get(v.t);
      // (the view the feed took as it opened, before the watch was laid: nothing fought yet, every bar 0)
      if (!at) { expect([v.t, v.breaking]).toEqual([0, { att: 0, def: 0 }]); continue; }
      expect(v.breaking!.att).toBe(r3(at.att));
      expect(v.breaking!.def).toBe(r3(at.shown));
      expect(v.breaking!.def).toBeLessThanOrEqual(0.95);
      if (v.breaking!.def === 0.95 && at.def < 0.95) masked++;
    }
    expect(masked).toBeGreaterThan(0);
    // With nobody playing (a film), the true bars.
    const film = run(null);
    for (const v of film.views) {
      const at = film.seen.get(v.t);
      if (!at) { expect([v.t, v.breaking]).toEqual([0, { att: 0, def: 0 }]); continue; }
      expect(v.breaking).toEqual({ att: r3(at.att), def: r3(at.def) });
    }
  });

  it("the enemy's 'near breaking' dispatch comes on the bar the player is shown, never on his true bar", () => {
    const piece = PIECES.find((p) => p.key === 'normandy-1944')!;
    // The player attacks (set after the battle is built, so the staffs still fight both sides and judge() speaks to him).
    const run = (k: SpreadReckoning | null): { i: number; worn: number; shown: number; prev: number; bar: number; prevBar: number } | null => {
      const t = ticketOf(piece, 11, null);
      if (k) t.reckoning.spread = k;
      const b = new LandBattle(t);
      t.att.human = true;
      let prev = 0, prevBar = 0;
      for (let i = 0; i < 400000 && !b.over; i++) {
        b.step();
        const v = b.view('att');
        if (v.events.some((e) => e.what === 'nearBreak' && e.side === 'def')) {
          // Never while the bar he is shown stands short of three quarters.
          expect(v.breaking!.def).toBeGreaterThanOrEqual(0.75);
          return { i, worn: b.wornShare('def'), shown: v.breaking!.def, prev, bar: b.breaking('def'), prevBar };
        }
        prev = v.breaking!.def; prevBar = b.breaking('def');
      }
      return null;
    };
    // Without the spread, as today: on the true bar, the step it reaches three quarters.
    const off = run(null)!;
    expect(off.prevBar).toBeLessThan(0.75);
    expect(off.bar).toBeGreaterThanOrEqual(0.75);
    // With it, on the bar shown (worn against the level his staff reads, 0.08), the same step whatever the enemy drew.
    const mid = run({ W: { att: 0.4, def: 0.12 } })!, high = run({ W: { att: 0.4, def: 0.4 } })!;
    for (const x of [mid, high]) {
      expect(x.prev).toBeLessThan(0.75);
      expect(x.shown).toBeGreaterThanOrEqual(0.75);
      expect(x.worn).toBeGreaterThanOrEqual(0.75 * 0.08);
      expect(x.worn).toBeLessThan(0.75 * 0.08 + 0.002);
      // His true bar stood well short of it: the moment tells nothing of his draw.
      expect(x.bar).toBeLessThan(0.75);
    }
    expect(high.i).toBe(mid.i);
    // An enemy that drew low breaks before the bar shown reaches three quarters: no dispatch tells it first.
    expect(run({ W: { att: 0.4, def: 0.03 } })).toBeNull();
  });
});

describe("step 1b's keys", () => {
  const side = (org: number, mor: number, xp: number, general: number): SideTicket =>
    ({ nat: 'X', adj: 'X', color: '#000', human: false, general: general ? { name: 'G', level: general } : null, forces: [{ corps: 1, name: 'c', nat: 'X', kind: 'inf', divs: 3, men: 1, str: 1, org, xp, mor, share: 1 }] }) as unknown as SideTicket;

  it('N reads organisation, morale, experience and the general against the reference army', () => {
    expect(levelN(side(1, 0.6, 0.3, 0), 'att')).toBeCloseTo(1, 12);
    expect(levelN(side(0.5, 0.4, 0.05, 0), 'att')).toBeCloseTo(0.639, 2);
    expect(levelN(side(0.7, 0.5, 0.3, 0), 'att')).toBeCloseTo(0.826, 2);
    expect(levelN(side(1, 0.6, 0.5, 2), 'def')).toBeCloseTo(1.170, 2);
    expect(levelN(side(1, 0.8, 0.8, 3), 'def')).toBeCloseTo(1.500, 2);
    for (const [x, y] of [[levelN(side(0.5, 0.4, 0.05, 0), 'att'), 0.639], [levelN(side(0.7, 0.5, 0.3, 0), 'att'), 0.826], [levelN(side(1, 0.6, 0.5, 2), 'att'), 1.17], [levelN(side(1, 0.8, 0.8, 3), 'att'), 1.5]]) expect(Math.abs(x - y)).toBeLessThan(0.005);
    expect(refStaying('att')).toBeCloseTo(1.6089, 4);
    expect(ledgerK('att')).toBeCloseTo(0.04972, 5);
    // A side's corps weighed by their shares; an unidentified one read as the reference (the mask).
    const two = side(0.5, 0.4, 0.05, 0);
    two.forces.push({ ...two.forces[0], corps: 2, org: 1, mor: 0.8, xp: 0.8 });
    two.forces[0].share = 0.25; two.forces[1].share = 0.75;
    const n0 = levelN(side(0.5, 0.4, 0.05, 0), 'att'), n1 = levelN(side(1, 0.8, 0.8, 0), 'att');
    expect(levelN(two, 'att')).toBeCloseTo(0.25 * n0 + 0.75 * n1, 12);
    expect(levelN(two, 'att', (f) => f.corps !== 2)).toBeCloseTo(0.25 * n0 + 0.75, 12);
  });

  it('the move: spreadKeys lays (Nd/Na)^(p/c) and the field multiplies the attack\'s blows by fortune x move', () => {
    const t = reseed(lodz(2), 1);
    const k = spreadKeys(t, { level: true });
    expect(k.move).toBe(Math.pow(k.N!.def / k.N!.att, 0.54 / 0.84));
    const step1 = { W: { att: 0.13, def: 0.18 } };
    const b0 = new LandBattle(withSpread(t, step1)), b1 = new LandBattle(withSpread(t, { ...step1, move: 1.2 }));
    const pair = (b: LandBattle, s: Side): [LandUnit, LandUnit] => [b.units.find((u) => u.side === s && u.kind !== 'fort')!, b.units.find((u) => u.side !== s && u.kind !== 'fort')!];
    for (const s of SIDES) {
      const [u0, e0] = pair(b0, s), [u1, e1] = pair(b1, s);
      const r = inside(b1).blow(u1, e1).dmg / inside(b0).blow(u0, e0).dmg;
      expect(Math.abs(r - (s === 'att' ? 1.2 : 1 / 1.2))).toBeLessThan(1e-12);
    }
  });

  it('blocks read the reference for their own nerve under N, and only under N', () => {
    const t = reseed(lodz(2), 1);
    const on = new LandBattle(withSpread(t, spreadKeys(t, { level: true }))), off = new LandBattle(withSpread(t, spreadKeys(t)));
    const all = (b: LandBattle): LandUnit[] => [...b.units, ...inside(b).coming.map((c) => c.u)];
    for (const u of all(on)) { expect(u.mor0).toBe(REF_NERVE.mor0); expect(u.mor).toBe(REF_NERVE.mor0); }
    expect(REF_NERVE.mor0).toBeCloseTo(0.92, 12);
    for (const u of all(off)) {
      const f = [...t[u.side].forces, ...(t[u.side].joiners ?? [])].find((x) => x.corps === u.corps)!;
      expect(u.mor0).toBe(startNerve(f.org, f.mor));
    }
    // One formula: the old inline expression, over a grid (morale absent too).
    for (const org of [0, 0.3, 0.5, 0.85, 1]) for (const mor of [undefined, 0, 0.4, 0.6, 0.7, 1]) {
      expect(startNerve(org, mor)).toBe(Math.min(1, 0.45 + 0.35 * org + 0.2 * (mor ?? 0.6)));
    }
  });

  it('the ledger with every part of m at 1 counts each block\'s losses at its own experience and the general', () => {
    const t0 = reseed(lodz(2), 1);
    delete t0.att.air; delete t0.def.air;
    const k = spreadKeys(t0, { level: true, ledger: true, m: { flank: false, rear: false } });
    expect(k.ledger!.m).toEqual({ flank: false, rear: false, fire: false, ambush: false, air: false });
    k.ledger!.draw = { att: 1, def: 1 };
    const t = withSpread(t0, k);
    const b = new LandBattle(t);
    // The reserve from the blocks' own organisation and morale, before they read the reference.
    for (const s of SIDES) {
      const us = b.units.filter((u) => u.side === s);
      const P = us.reduce((a, u) => a + u.power0, 0);
      const R = us.reduce((a, u) => { const f = t[s].forces.find((x) => x.corps === u.corps)!; return a + u.power0 * (startNerve(f.org, f.mor) - LAND.nerve.rout); }, 0) / P;
      expect(inside(b).led!.R[s]).toBeCloseTo(R, 12);
      expect(inside(b).led!.K[s]).toBe(ledgerK(s));
    }
    toAfterContact(b, 60);
    for (const s of SIDES) {
      const gen = t[s].general?.level ?? 0;
      const want = b.units.filter((u) => u.side === s).reduce((a, u) => a + u.power0 * (1 - u.str / u.str0) * LAND.nerve.perLoss[s] * Math.max(0.3, 1 - LAND.nerve.xp * u.xp - LAND.nerve.general * gen), 0) / inside(b).power[s];
      const D = inside(b).led!.D[s];
      expect(want).toBeGreaterThan(0);
      expect(Math.abs(D - want) / want).toBeLessThan(1e-9);
      const L = inside(b).led!;
      expect(b.breakCause(s).ledger).toBe(r3(D / (L.K[s] * L.R[s] * 1)));
    }
  });

  it('each part of m is read at the army or at the block, never both', () => {
    const parts: NervePart[] = ['flank', 'rear', 'fire', 'ambush', 'air'];
    const blows: NerveParts[] = [
      { aspect: 'front', nm: 1, fire: 1.5, ambush: 2 },
      { aspect: 'flank', nm: 2, fire: 2.5, ambush: 1 },
      { aspect: 'rear', nm: 3, fire: 1.8, ambush: 2.5 },
      { aspect: 'rear', nm: 3, fire: 1, ambush: 1 },
    ];
    for (let bits = 0; bits < 32; bits++) {
      const on = Object.fromEntries(parts.map((p, i) => [p, !!(bits & (1 << i))])) as Record<NervePart, boolean>;
      for (const p of blows) {
        const sp = nerveSplit(p, on);
        expect(Math.abs(sp.army * sp.block - p.nm * p.fire * p.ambush)).toBeLessThan(1e-15);
        const aspectOn = p.aspect !== 'front' && on[p.aspect];
        expect(sp.army).toBe((aspectOn ? p.nm : 1) * (on.fire ? p.fire : 1) * (on.ambush ? p.ambush : 1));
        expect(sp.block).toBe((aspectOn ? 1 : p.nm) * (on.fire ? 1 : p.fire) * (on.ambush ? 1 : p.ambush));
        // A front blow has no aspect part, whatever is on.
        if (p.aspect === 'front') expect(sp.army).toBe((on.fire ? p.fire : 1) * (on.ambush ? p.ambush : 1));
      }
    }
  });

  it('the residual by what is read', () => {
    const base = lodz(2);
    const sig = (t: T, k: SpreadReckoning): number[] => {
      const { u } = spreadDraw(t.seed, t.attempt), out: number[] = [];
      for (const s of SIDES) {
        const W = k.W[s], z = zOf(u[s]);
        if (W <= SPREAD.hold[0] * 1.001 || W >= SPREAD.hold[1] * 0.999 || Math.abs(z) < 0.2) continue;
        out.push(Math.log(W / (SPREAD.level * (k.N?.[s] ?? 1))) / z);
      }
      return out;
    };
    const read = (parts: Parameters<typeof spreadKeys>[1]): number[] => {
      const all: number[] = [];
      for (let s = 0; s < 12; s++) { const t = reseed(base, s); all.push(...sig(t, spreadKeys(t, parts))); }
      expect(all.length).toBeGreaterThan(5);
      return all;
    };
    for (const x of read({})) expect(x).toBeCloseTo(sigmaOne(), 10);
    for (const x of read({ level: true, ledger: false })) expect(x).toBeCloseTo(0.791, 10);
    for (const x of read({ level: true, ledger: true, m: { flank: false, rear: false } })) expect(x).toBeCloseTo(0.791, 10);
    for (const x of read({ level: true, ledger: true })) expect(x).toBeCloseTo(0.766, 10);
    for (const x of read({ level: true, ledger: true, m: { flank: false, rear: false, air: true } })) expect(x).toBeCloseTo(0.766, 10);
    // Under the ledger its residual draw is W over the front-on level: held as W is.
    const t = reseed(base, 3), k = spreadKeys(t, { level: true, ledger: true });
    for (const s of SIDES) expect(k.ledger!.draw[s]).toBe(k.W[s] / (SPREAD.level * k.N![s]));
  });
});

describe('data', () => {
  it('every number in spread.json carries its note and its mark', () => {
    const d = spreadJson as Record<string, unknown>;
    const keys = Object.keys(d).filter((k) => !k.startsWith('_'));
    expect(keys.length).toBeGreaterThanOrEqual(9);
    for (const k of keys) {
      const note = d[`_${k}`];
      expect(typeof note, k).toBe('string');
      expect(note as string, k).toMatch(/(^|[\s(;,])[FLDMG]([\s:,;)]|$)/);
    }
    // p and c are derived, with their source named.
    expect(d._move as string).toMatch(/^D \(fs\/final\/benchsum\.cjs/);
    expect(SPREAD.move).toEqual({ p: 0.54, c: 0.84 });
    // Step 1 as laid: step 1b's keys off, so the campaign lays exactly { W }.
    expect(SPREAD.steps).toEqual({ level: false, ledger: false });
    expect(SPREAD.m).toEqual({ flank: true, rear: true, fire: false, ambush: false, air: false });
  });
});
