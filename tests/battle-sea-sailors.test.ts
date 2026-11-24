// Men in the water on the field (release 2's sailor rules; naval-battles/sailors-r2/form.md, the design director's
// ruling 12:58Z, oil 17:39Z and figures 17:42Z; data/battle/sailors.json): who goes into the water and who goes down
// with her, the abandon-ship call, burning oil, pick-ups in the fight and after it, and the switch that keeps all of it
// off as shipped. Who wins never moves: every check of that runs the same battle with the switch off and on.
import { describe, expect, it } from 'vitest';
import { DT, type FieldEvent } from '../src/battle/core/types';
import { aliveAfter, SAILORS, SEA, waterOf } from '../src/battle/sea/numbers';
import { SeaSim, type SeaLosses } from '../src/battle/sea/sim';
import { plainTicket, sailorsOf, type PlainFleet } from '../src/battle/sea/ticket';
import { foughtOf } from '../src/battle/sea/handoff';
import { PRACTICE } from '../src/battle/sea/practice';
import type { SeaSailors, SeaUnit } from '../src/battle/sea/types';
import SEAS from '../data/campaign/seas.json';
import FAR_SEAS from '../data/campaign/far-seas.json';

const HOME: PlainFleet = { name: 'Home Fleet', nat: 'UK', ships: { bb: 2, cv: 1, ca: 3, dd: 8 } };
const BATTLE: PlainFleet = { name: 'Battle Fleet', nat: 'GER', ships: { bb: 2, ca: 3, dd: 12 } };
const KIDO: PlainFleet = { name: 'Striking Force', nat: 'JAP', ships: { bb: 2, cv: 2, ca: 2, dd: 8 } };
const PACIFIC: PlainFleet = { name: 'Pacific Fleet', nat: 'USA', ships: { bb: 2, cv: 2, ca: 3, dd: 8 } };
const NEW = new Set(['sea:men', 'sea:oil', 'sea:abandon', 'sea:rescue']);

type Opts = { att?: PlainFleet[]; def?: PlainFleet[]; odds?: number; human?: 'att' | 'def' | null; sailors?: SeaSailors };
const sim = (seed: number, o: Opts = {}): SeaSim => new SeaSim(plainTicket({ att: o.att ?? [HOME], def: o.def ?? [BATTLE], seed, odds: o.odds ?? 1, human: o.human ?? null, sailors: o.sailors }));
/** The whole action, every event in order. */
function fight(s: SeaSim, each?: (s: SeaSim, ev: FieldEvent[]) => void): FieldEvent[] {
  const all: FieldEvent[] = [];
  for (let i = 0; i < 1e5 && !s.over; i++) { s.step(); const ev = s.view().events; all.push(...ev); each?.(s, ev); }
  return all;
}
const x = (e: FieldEvent): Record<string, any> => e.extra as Record<string, any>;
const losses = (s: SeaSim): SeaLosses => s.over!.extra as unknown as SeaLosses;
const COLD: SeaSailors = { water: 'cold' };
/** A private part of the sim, for the checks that set up one blow by hand. */
const inner = (s: SeaSim): any => s as any;
const piece = (s: SeaSim, side: 'att' | 'def', cls: string): SeaUnit => s.units.find((u) => u.side === side && u.extra.cls === cls)!;

describe('the record and the switch', () => {
  it("the water table covers every sea of the campaign, a band a month", () => {
    const ids = [...(SEAS as any).zones, ...(FAR_SEAS as any).zones].map((z: { id: string }) => z.id);
    expect(ids.length).toBeGreaterThan(90);
    for (const id of ids) for (let m = 0; m < 12; m++) expect(waterOf(id, m), `${id} ${m}`).not.toBeNull();
    expect(waterOf('natl', 0)).toBe('cold');
    expect(waterOf('wmed', 7)).toBe('warm');
    expect(waterOf('nowhere', 0)).toBeNull();
  });

  it("the share alive runs in straight lines between the record's points, none past the last (S5)", () => {
    expect(aliveAfter('cold', 0)).toBe(1);
    expect(aliveAfter('cold', 0.5)).toBeCloseTo(0.8, 9);
    expect(aliveAfter('cold', 1)).toBeCloseTo(0.6, 9);
    expect(aliveAfter('temperate', 2)).toBeCloseTo(0.825, 9);
    expect(aliveAfter('temperate', 48)).toBeCloseTo(0.03, 9);
    expect(aliveAfter('temperate', 48.01)).toBe(0);
    expect(aliveAfter('cold', 96)).toBe(0);
    // Warm water runs on to Indianapolis's 35% at 100 hours: found after 4 days, 37% are alive.
    expect(aliveAfter('warm', 96)).toBeCloseTo(0.65 - 0.3 * (48 / 52), 9);
    expect(aliveAfter('warm', 100)).toBeCloseTo(0.35, 9);
  });

  it("arctic water, under 4 C (the design director, 18:50Z; the historian's 4b): its finer points, and only where the table lays it", () => {
    expect(aliveAfter('arctic', 0.25)).toBeCloseTo(0.85, 9);
    expect(aliveAfter('arctic', 0.5)).toBeCloseTo(0.6, 9);
    expect(aliveAfter('arctic', 0.75)).toBeCloseTo(0.4, 9);
    expect(aliveAfter('arctic', 1)).toBeCloseTo(0.2, 9);
    expect(aliveAfter('arctic', 1.5)).toBeCloseTo(0.05, 9);
    expect(aliveAfter('arctic', 2.25)).toBeCloseTo(0.025, 9);
    expect(aliveAfter('arctic', 3)).toBe(0);
    expect(aliveAfter('arctic', 96)).toBe(0);
    // Every letter in the table is a band, and 'a' reads arctic.
    for (const [sea, months] of Object.entries(SAILORS.water)) if (!sea.startsWith('_')) expect(months, sea).toMatch(/^[actw]{12}$/);
    SAILORS.water.__probe = 'acccccccccca';
    try {
      expect(waterOf('__probe', 0)).toBe('arctic');
      expect(waterOf('__probe', 1)).toBe('cold');
    } finally {
      delete SAILORS.water.__probe;
    }
  });

  it('the figures are the design director\'s (17:42Z): no floating dead, the oil and the afloat time marked as knobs', () => {
    expect(SAILORS.down).toMatchObject({ sudden: 0.42, fought: 0.25, early: 0.01 });
    expect(SAILORS.dead.share).toBe(0);
    expect(SAILORS.found).toMatchObject({ stop: 1.5, leave: 96 });
    expect(SAILORS.pick).toMatchObject({ wreck: 1, early: 5 });
    expect(SAILORS.refuse.navies).toEqual(['JAP']);
    expect((SAILORS.oil as any).knobs).toBe(true);
    expect((SAILORS.abandon as any).knobs).toBe(true);
  });

  it('off as shipped: no sailor on the ticket, no sailor event, nothing new in the result', () => {
    const t = plainTicket({ att: [HOME], def: [BATTLE], seed: 11 });
    expect('sailors' in t.place).toBe(false);
    const s = sim(11);
    const ev = fight(s);
    expect(ev.some((e) => NEW.has(e.what))).toBe(false);
    const l = losses(s);
    expect(l).not.toHaveProperty('sailors');
    expect(l).not.toHaveProperty('rescue');
    expect(l).not.toHaveProperty('abandoned');
    // And the orders that need men in the water are refused.
    const s2 = sim(11, { human: 'att' });
    expect(s2.command({ do: 'sea:abandon', units: [piece(s2, 'att', 'dd').id], early: true } as any, 'att')).toBe(false);
    expect(s2.command({ do: 'sea:rescue', units: [piece(s2, 'att', 'dd').id], wreck: { unit: 1, ship: 1 } } as any, 'att')).toBe(false);
  });

  it("the campaign's ticket carries the sea's band this month only in a grand campaign with the switch on", () => {
    const g = { flags: { open: 1, sailors: 1 } as Record<string, number>, seas: [{ key: 'natl' }, { key: 'wmed' }], turn: 1 } as any;
    // Turn 1 is September 1939.
    expect(sailorsOf(g, 0)).toEqual({ water: 'temperate' });
    expect(sailorsOf(g, 1)).toEqual({ water: 'warm' });
    g.turn = 5; // January 1940
    expect(sailorsOf(g, 0)).toEqual({ water: 'cold' });
    expect(sailorsOf({ ...g, flags: { open: 1 } }, 0)).toBeNull();
    expect(sailorsOf({ ...g, flags: { sailors: 1 } }, 0)).toBeNull();
  });
});

describe('who wins never moves (form F: the staff stops only after the fight and fights for every ship)', () => {
  const strip = (ev: FieldEvent[]): string => JSON.stringify(ev.filter((e) => !NEW.has(e.what)));
  const cases: [string, Opts][] = [
    ['Home Fleet against the Battle Fleet', {}],
    ['at 0.6', { odds: 0.6 }],
    ['at 1.6', { odds: 1.6 }],
    ['carriers and the Japanese', { att: [KIDO], def: [PACIFIC] }],
    ...PRACTICE.slice(0, 3).map((p): [string, Opts] => [p.name, { att: p.att, def: p.def }]),
  ];
  for (const [name, o] of cases) {
    it(`${name}: the same action on and off, every number, every event but the new ones`, () => {
      for (const seed of [11, 12, 13]) {
        const off = sim(seed, o), on = sim(seed, { ...o, sailors: { water: 'temperate' } });
        const e0 = fight(off), e1 = fight(on);
        expect(strip(e1)).toBe(strip(e0));
        expect(on.tally).toEqual(off.tally);
        const r0 = off.over!, r1 = on.over!;
        const { sailors, rescue, abandoned, ...rest } = r1.extra as any;
        expect(rest).toEqual(r0.extra);
        expect(Object.values(abandoned as Record<number, Record<string, number>>).every((s) => Object.values(s).every((n) => n === 0))).toBe(true);
        expect({ ...r1, extra: 0, lines: 0 }).toEqual({ ...r0, extra: 0, lines: 0 });
        expect(r1.lines.slice(0, r0.lines.length)).toEqual(r0.lines);
        expect(foughtOf(r1)).toEqual(foughtOf(r0));
        // No ship stops in the fight, and every wreck's men are told.
        expect(e1.some((e) => e.what === 'sea:rescue' || e.what === 'sea:abandon')).toBe(false);
        expect(e1.filter((e) => e.what === 'sea:men').length).toBe(e1.filter((e) => e.what === 'sea:sunk').length);
        void sailors; void rescue;
      }
    });
  }
});

describe('men in the water (form A)', () => {
  it("every ship that goes down puts her crew in the water at her wreck, a quarter of them down with her (fought for)", () => {
    const s = sim(11, { odds: 0.6, sailors: COLD });
    const ev = fight(s);
    const sunk = ev.filter((e) => e.what === 'sea:sunk');
    expect(sunk.length).toBeGreaterThan(2);
    for (const e of sunk) {
      const i = ev.indexOf(e), men = ev[i + 1], oil = ev[i + 2];
      expect(men.what).toBe('sea:men');
      expect(oil.what).toBe('sea:oil');
      expect(men.unit).toBe(e.unit);
      const crew = SEA.classes[x(e).cls as 'dd'].crew;
      expect(x(men).men + x(men).down + x(men).dead).toBe(crew);
      // Gunfire: slow, fought for.
      expect(x(men).down).toBe(Math.round(crew * 0.25));
      expect(x(men).dead).toBe(0);
      expect(x(men).early).toBe(false);
      expect([men.x, men.y]).toEqual([oil.x, oil.y]);
    }
    // At the sunk zone the field already lays for her.
    const zones = s.view().zones.filter((z) => z.kind === 'sunk');
    for (const m of ev.filter((e) => e.what === 'sea:men')) expect(zones.some((z) => z.unit === m.unit && z.x === m.x && z.y === m.y)).toBe(true);
    // {unit, ship} keys each wreck once.
    const keys = ev.filter((e) => e.what === 'sea:men').map((e) => `${e.unit}:${x(e).ship}`);
    expect(new Set(keys).size).toBe(keys.length);
  });

  it("a destroyer a torpedo salvo or aircraft sink goes too fast for any call: 42% down with her, Hammann's case (S1, S2; widened 19:15Z: Jarvis, Sims)", () => {
    for (const blow of ['torp', 'gun', 'air'] as const) {
      const s = sim(11, { sailors: { water: 'warm', early: [1, 2] } });
      const dd = piece(s, 'def', 'dd'), inn = inner(s);
      inn.blow = blow;
      inn.wear(dd, inn.hullOf(dd) * 0.3);
      inn.blow = 'gun';
      const men = s.view().events.find((e) => e.what === 'sea:men')!;
      // A torpedo or aircraft: sudden whatever the call; to gunfire her fleet's call to abandon early leaves 1% aboard.
      expect(x(men).down).toBe(blow === 'gun' ? 2 : 84);
      expect(x(men).early).toBe(blow === 'gun');
    }
    // A big ship a torpedo sinks is fought for: the field cannot see her go in under five minutes.
    const s = sim(11, { sailors: COLD });
    const ca = piece(s, 'def', 'ca'), inn = inner(s);
    inn.blow = 'torp';
    inn.wear(ca, inn.hullOf(ca));
    const men = s.view().events.find((e) => e.what === 'sea:men')!;
    expect(x(men).down).toBe(Math.round(SEA.classes.ca.crew * 0.25));
  });
});

describe('burning oil (form D, as narrowed 17:39Z)', () => {
  it('burns only where a carrier went down to a torpedo salvo or to aircraft; every other slick is unlit', () => {
    const one = (cls: 'cv' | 'bb', blow: 'torp' | 'air' | 'gun'): FieldEvent => {
      const s = sim(11, { att: [KIDO], def: [PACIFIC], sailors: COLD });
      const u = piece(s, 'def', cls), inn = inner(s);
      inn.blow = blow;
      inn.wear(u, inn.hullOf(u));
      return s.view().events.find((e) => e.what === 'sea:oil')!;
    };
    for (const blow of ['torp', 'air'] as const) {
      const lit = one('cv', blow);
      expect(x(lit).litR).toBe(x(lit).r);
      expect(x(lit).r).toBe(SAILORS.oil.r.cv);
      expect(x(lit).until).toBeCloseTo(lit.t + SAILORS.oil.burn, 6);
      expect(lit.text).toMatch(/^Oil burns on the water where a carrier of the Pacific Fleet went down\.$/);
    }
    for (const [cls, blow] of [['cv', 'gun'], ['bb', 'torp'], ['bb', 'air']] as const) {
      const unlit = one(cls, blow);
      expect(x(unlit).litR).toBe(0);
      expect(x(unlit).until).toBeNull();
      expect(unlit.text).toBe('');
    }
  });

  it('takes no man and delays no pick-up: a lit wreck\'s men are counted as an unlit one\'s', () => {
    const count = (blow: 'torp' | 'gun'): number => {
      const s = sim(11, { att: [KIDO], def: [PACIFIC], sailors: COLD });
      const u = piece(s, 'def', 'cv'), inn = inner(s);
      inn.blow = blow;
      inn.wear(u, inn.hullOf(u));
      inn.blow = 'gun';
      return x(s.view().events.find((e) => e.what === 'sea:men')!).men;
    };
    expect(count('torp')).toBe(count('gun'));
  });
});

describe('the abandon-ship call (form C)', () => {
  it("abandon early: her men leave first, in good order, and her sinking is told when her boats are clear", () => {
    const s = sim(11, { odds: 0.6, sailors: { water: 'cold', early: [1] } });
    const ev = fight(s);
    const ab = ev.filter((e) => e.what === 'sea:abandon');
    expect(ab.length).toBeGreaterThan(0);
    for (const a of ab) {
      expect(a.side).toBe('att');
      const men = ev[ev.indexOf(a) + 1];
      expect(men.what).toBe('sea:men');
      expect(x(men).early).toBe(true);
      expect(x(men).down).toBe(Math.round(SEA.classes[x(a).cls as 'dd'].crew * 0.01));
      const sunk = ev.find((e) => e.what === 'sea:sunk' && e.unit === a.unit && e.t >= a.t && e.x === a.x && e.y === a.y)!;
      expect(sunk).toBeDefined();
      // Her sinking names the hull her abandonment did (the sea scene's ask).
      expect(x(sunk).ship).toBe(x(a).ship);
      expect(sunk.t).toBeCloseTo(Math.min(a.t + SAILORS.abandon.afloat, s.t), 6);
      expect(ev[ev.indexOf(sunk) + 1].what).toBe('sea:oil');
    }
    expect(losses(s).sailors!.att.call.early.length).toBe(ab.length);
    expect(losses(s).sailors!.att.call.abandon).toBe('early');
    expect(losses(s).sailors!.def.call.abandon).toBe('fight');
  });

  it("a ship worn through past the field's limit is lost instead of making port; one that fell out of the line still steams home", () => {
    const s = sim(11, { sailors: { water: 'cold', early: [2] } });
    const inn = inner(s), ca = piece(s, 'def', 'ca');
    inn.caps.def.ca = 0; // the field's limit already reached for its cruisers
    // Out of the line with hull left: she can steam, and makes for port.
    inn.wear(ca, inn.hullOf(ca) * 0.6);
    let ev = s.view().events;
    expect(ev.some((e) => e.what === 'sea:burning')).toBe(true);
    expect(ev.some((e) => e.what === 'sea:abandon')).toBe(false);
    // Worn through: abandoned, booked lost to her fleet, and no burning ship.
    const ca2 = s.units.find((u) => u.side === 'def' && u.extra.cls === 'ca' && u.id !== ca.id)!;
    inn.wear(ca2, inn.hullOf(ca2));
    ev = s.view().events;
    expect(ev.map((e) => e.what).filter((w) => w.startsWith('sea:'))).toEqual(['sea:abandon', 'sea:men']);
    expect(ca2.state).toBe('gone');
    expect(inn.fleetLoss.abandoned[2].ca).toBe(1);
    expect(inn.fleetLoss.burnt[2].ca).toBe(1);
    expect(inn.fleetLoss.sunk[2].ca).toBe(0);
    // The field's limit does not count her; the campaign counts her against her fleet.
    expect(inn.lostShips.def.ca).toBe(0);
    s.command({ do: 'withdrawAll', side: 'def' } as any, 'def');
    fight(s);
    expect(losses(s).abandoned![2].ca).toBe(1);
    expect(foughtOf(s.over!).sunk[2].ca).toBe(losses(s).sunk[2].ca + 1);
  });

  it("the player changes one ship's call as she is hit; it is no order to her helm", () => {
    const s = sim(11, { human: 'def', sailors: COLD });
    const inn = inner(s), dd = piece(s, 'def', 'dd');
    expect(s.command({ do: 'sea:abandon', units: [dd.id], early: true } as any, 'def')).toBe(true);
    expect(s.command({ do: 'sea:abandon', units: [dd.id], early: true } as any, 'att')).toBe(false);
    expect(s.orderOf(dd.id)).toBeUndefined();
    inn.wear(dd, inn.hullOf(dd) * 0.3);
    const men = s.view().events.find((e) => e.what === 'sea:men')!;
    expect(x(men).early).toBe(true);
    // Her fleet's other pieces still fight for their ships.
    const dd2 = s.units.find((u) => u.side === 'def' && u.extra.cls === 'dd' && u.id !== dd.id)!;
    inn.wear(dd2, inn.hullOf(dd2) * 0.3);
    expect(x(s.view().events.find((e) => e.what === 'sea:men')!).early).toBe(false);
  });
});

describe('after the fight (form B2, A4)', () => {
  it("the side holding the water stops, its own men saved and the enemy's its prisoners, at 1.5 hours; the counts add up", () => {
    const s = sim(11, { odds: 0.6, sailors: COLD });
    fight(s);
    const l = losses(s), w = s.over!.winner!, beaten = w === 'att' ? 'def' : 'att';
    expect(s.over!.how).not.toBe('time');
    for (const side of ['att', 'def'] as const) {
      const m = l.sailors![side];
      expect(m.crew).toBe(m.down + m.dead + m.water);
      expect(m.lost).toBe(m.crew - m.saved - m.prisoners);
      expect(m.call.stop).toBe(side === w ? 'stop' : null);
    }
    expect(l.sailors![w].prisoners).toBe(0);
    expect(l.sailors![beaten].saved).toBe(0);
    expect(l.sailors![beaten].prisoners).toBeGreaterThan(0);
    // Every pick-up is the holder's, by one of its fleets, and its prisoners are the enemy's men.
    const own = new Set(s.ticket.place.fleets.filter((f) => f.side === w).map((f) => f.id));
    for (const r of l.rescue!) expect(own.has(r.fleet)).toBe(true);
    expect(l.rescue!.reduce((n, r) => n + r.prisoners, 0)).toBe(l.sailors![beaten].prisoners);
    expect(s.over!.lines.at(-1)).toMatch(/sailors: [\d,]+ saved/);
  });

  it('leave them: their men are found four days later by whoever holds the water, and only warm water leaves any alive', () => {
    const run = (water: 'cold' | 'warm', stop: 'stop' | 'leave'): SeaLosses => {
      const s = sim(11, { odds: 0.6, sailors: { water, stop: { att: stop, def: stop } } });
      fight(s);
      return losses(s);
    };
    const coldLeave = run('cold', 'leave'), warmLeave = run('warm', 'leave'), warmStop = run('warm', 'stop');
    // The holder's own men are saved and the enemy's are its prisoners (the design director, 18:42Z), a little over a
    // third alive after four days and the minutes before the fight ended.
    const held = warmLeave.sailors!.att.call.stop ? 'att' : 'def';
    for (const side of ['att', 'def'] as const) {
      expect(coldLeave.sailors![side].saved + coldLeave.sailors![side].prisoners).toBe(0);
      const m = warmLeave.sailors![side], found = side === held ? m.saved : m.prisoners;
      expect(side === held ? m.prisoners : m.saved).toBe(0);
      if (m.water > 0) expect(found / m.water).toBeGreaterThan(0.36);
      if (m.water > 0) expect(found / m.water).toBeLessThan(0.38);
    }
    expect(coldLeave.rescue).toEqual([]);
    // Picked up at 1.5 hours after the fight in warm water, all but a hundredth or two are alive (98% at 3 hours).
    const holder = warmStop.sailors!.att.call.stop ? 'att' : 'def';
    const beaten = holder === 'att' ? 'def' : 'att';
    for (const [side, n] of [[holder, warmStop.sailors![holder].saved], [beaten, warmStop.sailors![beaten].prisoners]] as const) {
      const w = warmStop.sailors![side].water;
      if (w > 0) expect(n / w).toBeGreaterThan(0.98);
      expect(n).toBeLessThanOrEqual(w);
    }
  });

  it("each wreck's men are in the water from her own sinking: men who went in early wait longest for the pick-up", () => {
    const s = sim(11, { odds: 0.6, sailors: { water: 'temperate' } });
    fight(s);
    const inn = inner(s), end = inn.t, l = losses(s);
    const mains = inn.wrecks.filter((w: any) => !w.guest);
    expect(new Set(mains.map((w: any) => w.t)).size).toBeGreaterThan(1);
    for (const r of l.rescue!) {
      const w = mains.find((k: any) => k.unit === r.wreck.unit && k.ship === r.wreck.ship);
      expect(r.men).toBe(Math.round(w.water * aliveAfter('temperate', (end + SAILORS.found.stop * 3600 - w.t) / 3600)));
    }
  });

  it("on time neither side broke off, and each stops for its own: no prisoners", () => {
    let found = false;
    for (const seed of [11, 12, 13, 14, 15, 16, 17, 18]) {
      const s = sim(seed, { sailors: COLD });
      s.ticket.limit = 300;
      fight(s);
      if (s.over!.how !== 'time' || losses(s).sailors!.att.crew + losses(s).sailors!.def.crew === 0) continue;
      found = true;
      for (const side of ['att', 'def'] as const) {
        expect(losses(s).sailors![side].prisoners).toBe(0);
        expect(losses(s).sailors![side].call.stop).toBe('stop');
      }
    }
    if (!found) {
      // No ship went down in 300 s on these seeds: one sunk by hand, then the limit.
      const s = sim(11, { sailors: COLD });
      s.ticket.limit = 30;
      const inn = inner(s), dd = piece(s, 'att', 'dd');
      inn.wear(dd, inn.hullOf(dd) * 0.3);
      fight(s);
      expect(s.over!.how).toBe('time');
      expect(losses(s).sailors!.att.prisoners).toBe(0);
      expect(losses(s).sailors!.att.saved).toBeGreaterThan(0);
    }
  });

  it("the Japanese navy's men refuse an enemy's pick-up: none become prisoners, and they are lost (S8)", () => {
    const s = sim(11, { att: [KIDO], def: [PACIFIC], sailors: { water: 'warm' } });
    const inn = inner(s), dd = piece(s, 'att', 'dd');
    inn.wear(dd, inn.hullOf(dd) * 0.3);
    s.command({ do: 'withdrawAll', side: 'att' } as any, 'att');
    fight(s);
    expect(s.over!.winner).toBe('def');
    const m = losses(s).sailors!.att;
    expect(m.water).toBeGreaterThan(0);
    expect(m.prisoners).toBe(0);
    expect(m.saved).toBe(0);
    expect(m.lost).toBe(m.crew);
  });
});

describe('picking up survivors in the fight (form B1)', () => {
  /** The player's own destroyers sent to the first wreck of their side's to go down. */
  function rescue(seed: number, water: SeaSailors['water'] = 'temperate'): { s: SeaSim; ev: FieldEvent[]; by: number } {
    const s = sim(seed, { odds: 0.6, human: 'att', sailors: { water } });
    s.ai.att = true;
    let by = -1;
    const ev = fight(s, (s2, e) => {
      if (by >= 0) return;
      const m = e.find((k) => k.what === 'sea:men' && k.side === 'att');
      if (!m) return;
      const dd = s2.units.filter((u) => u.side === 'att' && u.extra.cls === 'dd' && u.state === 'steady' && u.id !== m.unit).at(-1)!;
      expect(s2.command({ do: 'sea:rescue', units: [dd.id], wreck: { unit: m.unit!, ship: x(m).ship } } as any, 'att')).toBe(true);
      by = dd.id;
    });
    return { s, ev, by };
  }

  it("she lies stopped beside the wreck and takes a man a minute a ship aboard, told as she starts, at each man and as she leaves", () => {
    const { s, ev, by } = rescue(11);
    const told = ev.filter((e) => e.what === 'sea:rescue');
    expect(told.length).toBeGreaterThan(2);
    expect(told.every((e) => e.unit === by)).toBe(true);
    expect(x(told[0]).men).toBe(0);
    expect(told[0].text).toMatch(/stop to pick men out of the water\.$/);
    const last = told.at(-1)!;
    expect(x(last).done).toBe(true);
    expect(last.text).toMatch(/picked up \d+ m[ae]n\.$/);
    expect(told.slice(0, -1).every((e) => !x(e).done)).toBe(true);
    // A man more each time, at one a minute for each of her four ships: about 15 seconds apart.
    const counts = told.slice(1, -1).map((e) => x(e).men);
    counts.forEach((n, i) => expect(n).toBe(i + 1));
    if (told.length > 3) expect(told[2].t - told[1].t).toBeCloseTo(60 / 4, 0);
    // Her side's men picked up in the fight are saved.
    expect(losses(s).sailors!.att.saved).toBeGreaterThanOrEqual(x(last).men);
  });

  it('she comes to the wreck and stops there; stopped, she combs no tracks and keeps no line or station', () => {
    const s = sim(11, { odds: 0.6, human: 'att', sailors: COLD });
    s.ai.att = true;
    // One of her own side's destroyers lost as the fleets draw up, and a cruiser of hers sent to her wreck.
    const inn = inner(s), dd = piece(s, 'att', 'dd');
    inn.wear(dd, inn.hullOf(dd) * 0.3);
    const m = s.view().events.find((e) => e.what === 'sea:men')!;
    const mine = s.units.filter((u) => u.side === 'att' && u.extra.cls === 'ca')[0];
    expect(s.command({ do: 'sea:rescue', units: [mine.id], wreck: { unit: m.unit!, ship: 1 } } as any, 'att')).toBe(true);
    let stopped = false;
    for (let i = 0; i < Math.round(900 / DT) && !s.over && live(mine); i++) {
      s.step();
      const near = Math.hypot(mine.x - m.x!, mine.y - m.y!) <= 200;
      if (near && mine.extra.way === 0) stopped = true;
      if (stopped && s.orderOf(mine.id)?.kind === 'rescue') {
        expect(inner(s).combs.has(mine.id)).toBe(false);
        expect(mine.extra.lineAt).toBeUndefined();
      }
    }
    expect(stopped).toBe(true);
  });

  it('a wreck emptied before she starts: her order lapses, with nothing told', () => {
    const s = sim(11, { odds: 0.6, human: 'att', sailors: COLD });
    s.ai.att = true;
    const inn = inner(s), dd = piece(s, 'att', 'dd');
    inn.wear(dd, inn.hullOf(dd) * 0.3);
    const m = s.view().events.find((e) => e.what === 'sea:men')!;
    const mine = s.units.filter((u) => u.side === 'att' && u.extra.cls === 'ca')[0];
    expect(s.command({ do: 'sea:rescue', units: [mine.id], wreck: { unit: m.unit!, ship: 1 } } as any, 'att')).toBe(true);
    // Another ship (or the sea) has taken the last of them before she comes up.
    inn.wrecks.find((w: { unit: number; ship: number }) => w.unit === m.unit && w.ship === 1).alive = 0;
    s.step();
    expect(s.orderOf(mine.id)?.kind).not.toBe('rescue');
    expect(s.view().events.some((e) => e.what === 'sea:rescue')).toBe(false);
  });

  it("men she has picked up share her fate when a ship of hers is lost: her share go down with her people's share, the rest into the water at her wreck", () => {
    const s = sim(11, { odds: 0.6, human: 'att', sailors: { water: 'warm' } });
    s.ai.att = true;
    const inn = inner(s), dds = s.units.filter((u) => u.side === 'att' && u.extra.cls === 'dd');
    const [lost, by] = [dds[0], dds.at(-1)!];
    inn.wear(lost, inn.hullOf(lost) * 0.3);
    s.view();
    expect(s.command({ do: 'sea:rescue', units: [by.id], wreck: { unit: lost.id, ship: 1 } } as any, 'att')).toBe(true);
    for (let i = 0; i < 1e4 && !s.over && !((inn.rescuing.get(by.id)?.got ?? 0) >= 12); i++) s.step();
    expect(s.over).toBeFalsy();
    const got = inn.rescuing.get(by.id).got, ships = by.extra.ships, picked = inn.wrecks[0].picked.att;
    expect(got).toBeGreaterThanOrEqual(12);
    s.view();
    inn.wear(by, inn.hullOf(by) * (1 / ships + 0.05));
    const men = s.view().events.find((e) => e.what === 'sea:men' && e.unit === by.id)!;
    // A destroyer fought for: a quarter of her share go down with her, the rest swim.
    const hers = got / ships;
    expect(x(men).guests).toBe(Math.round(hers * (1 - SAILORS.down.fought)));
    expect(inn.wrecks[0].picked.att).toBeCloseTo(picked - hers, 6);
    const guest = inn.wrecks.find((w: any) => w.guest)!;
    expect(guest).toMatchObject({ unit: by.id, ship: x(men).ship, side: 'att', crew: 0, water: 0 });
    expect(guest.alive).toBeCloseTo(hers * (1 - SAILORS.down.fought), 6);
    // The rest of her piece still carries theirs, and the count still adds up at the end.
    fight(s);
    const m = losses(s).sailors!.att;
    expect(m.crew).toBe(m.down + m.dead + m.water);
    expect(m.lost).toBe(m.crew - m.saved - m.prisoners);
  });

  it("no pick-up at an enemy wreck whose navy's men refuse it, nor with nobody alive", () => {
    const s = sim(11, { att: [PACIFIC], def: [KIDO], human: 'att', sailors: COLD });
    const inn = inner(s), dd = piece(s, 'def', 'dd'), mine = piece(s, 'att', 'dd');
    inn.wear(dd, inn.hullOf(dd) * 0.3);
    expect(s.command({ do: 'sea:rescue', units: [mine.id], wreck: { unit: dd.id, ship: 1 } } as any, 'att')).toBe(false);
    expect(s.command({ do: 'sea:rescue', units: [mine.id], wreck: { unit: dd.id, ship: 2 } } as any, 'att')).toBe(false);
    // Her own side may pick them up.
    const s2 = sim(11, { att: [PACIFIC], def: [KIDO], human: 'def', sailors: COLD });
    const i2 = inner(s2), d2 = piece(s2, 'def', 'dd');
    i2.wear(d2, i2.hullOf(d2) * 0.3);
    const other = s2.units.filter((u) => u.side === 'def' && u.extra.cls === 'dd').at(-1)!;
    expect(s2.command({ do: 'sea:rescue', units: [other.id], wreck: { unit: d2.id, ship: 1 } } as any, 'def')).toBe(true);
  });
});

const live = (u: SeaUnit): boolean => u.state !== 'gone' && u.state !== 'withdrawn';
