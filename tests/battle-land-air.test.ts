// Air support on the land field (Brian, 2026-11-18 18:58Z: "during ground battles where fighters are in range, you
// should be able to call in air support where the fighters will strafe and drop bombs"; the design director, b533 and
// b536): fighters on call strike when called, carrying the share of the side's strength the card counted for them,
// never more. No air on the ticket, nothing changes; what is held back from the blows is paid where the strikes land;
// no strike before the armies meet; the staff call one left ready; no gun fires on our own men; the bill is strikes
// flown and planes lost by group.
import { describe, expect, it } from 'vitest';
import type { AirOnCall, FieldEvent, Side } from '../src/battle/core';
import { LAND, LandBattle, PRACTICE_AIR, PRACTICE_GROUNDS, practiceReckoning, practiceTicket, sampleTicket, type LandUnit } from '../src/battle/land';

const GROUP = (id: number, nat: string, lose = 0.5): AirOnCall['groups'] => [{ id, name: `${id} Group`, nat, planes: 40, lose }];
const ON = (share: number, strikes = 3, nat = 'GER', lose = 0.5): AirOnCall => ({ share, strikes, groups: GROUP(1, nat, lose) });

/** The plain order: every block of the attack at the enemy block nearest them, the defence on the computer. */
function plain(b: LandBattle): void {
  b.ai.att = false; b.ai.def = true;
  const mine = b.units.filter((u) => u.side === 'att' && u.kind !== 'fort'), foes = b.units.filter((u) => u.side === 'def');
  const cx = mine.reduce((a, u) => a + u.x, 0) / mine.length, cy = mine.reduce((a, u) => a + u.y, 0) / mine.length;
  const t = foes.reduce((m, f) => (Math.hypot(f.x - cx, f.y - cy) < Math.hypot(m.x - cx, m.y - cy) ? f : m), foes[0]);
  b.command({ do: 'attack', units: mine.map((u) => u.id), target: t.id }, 'att');
}

/** Fight it out, every event kept. 'each' runs before every tick. */
function fight(b: LandBattle, each?: (b: LandBattle) => void): FieldEvent[] {
  const ev: FieldEvent[] = [];
  for (let i = 0; i < 1e5 && !b.over; i++) { each?.(b); b.step(); ev.push(...b.view().events); }
  return ev;
}

const ticket = (air?: Partial<Record<Side, AirOnCall>>, seed = 7919 * 3 + 13, date = 'June 1942') =>
  sampleTicket('plains', 1.25, { seed, humanAtt: true, date, ...(air ? { air } : {}) });

describe('no fighters on call, no change', () => {
  it('a side with no share, no strikes or no group is a battle without air: the same end, no flights in the view', () => {
    const run = (air?: Partial<Record<Side, AirOnCall>>) => { const b = new LandBattle(ticket(air)); plain(b); fight(b); return b; };
    const none = run();
    for (const air of [{ att: ON(0) }, { att: ON(0.1, 0) }, { att: { share: 0.1, strikes: 2, groups: [] } }]) {
      const b = run(air);
      expect(b.over).toEqual(none.over);
      expect(b.hasAir('att')).toBe(false);
    }
    const v = new LandBattle(ticket()).view();
    expect('flights' in v).toBe(false);
    expect('air' in v).toBe(false);
    expect(none.over!.air).toBeUndefined();
  });
});

describe('the strikes carry what the card counted, no more', () => {
  it('what the blows held back is paid where the strikes land or lost on empty ground: the ledger adds up', () => {
    for (const air of [{ att: ON(0.15) }, { def: ON(0.1, 2, 'SOV') }, { att: ON(0.05), def: ON(0.03, 2, 'SOV') }]) {
      const b = new LandBattle(ticket(air));
      plain(b); fight(b);
      for (const s of ['att', 'def'] as Side[]) {
        if (!b.hasAir(s)) continue;
        const l = b.airLedger[s];
        expect(l.withheld).toBeGreaterThan(0);
        expect(Math.abs(l.withheld - l.landed - l.wasted)).toBeLessThan(1e-9 * Math.max(1, l.withheld));
      }
    }
  });
  it('the staff calls every strike: none of it is wasted', () => {
    const b = new LandBattle(ticket({ att: ON(0.15) }));
    plain(b);
    const calls = fight(b).filter((e) => e.what === 'land:air-call' && e.side === 'att');
    expect(calls.length).toBeGreaterThan(0);
    expect(calls.every((e) => e.extra!.by === 'staff')).toBe(true);
    expect(b.over!.air!.att![0].sorties).toBe(calls.length);
    expect(b.airLedger.att.landed).toBeGreaterThan(0);
    expect(b.airLedger.att.wasted).toBe(0);
  });
  it('a block that leaves the field still pays what it is owed: pulled out, sent back or fled, never wasted', () => {
    // The defence's blocks the bank owes most are ordered off the field while the attack's staff fly every strike.
    const b = new LandBattle(ticket({ att: ON(0.15) }));
    plain(b);
    let sent = false;
    fight(b, (x) => {
      if (sent || x.contactAt === null || x.t < x.contactAt + 25) return;
      sent = true;
      const bank = (x as unknown as { airBank: Record<Side, Map<number, number>> }).airBank.att;
      const ids = [...bank.keys()].filter((id) => x.unit(id)?.state !== 'gone');
      x.command({ do: 'withdraw', units: ids }, 'def');
    });
    expect(sent).toBe(true);
    expect(b.airLedger.att.wasted).toBe(0);
    const l = b.airLedger.att;
    expect(Math.abs(l.withheld - l.landed)).toBeLessThan(1e-9 * l.withheld);
  });
  it('pulling the whole army out pays the enemy\'s strikes first: the bar and the bill count them', () => {
    const at = (pull: boolean): LandBattle => {
      const b = new LandBattle(ticket({ def: ON(0.15, 3, 'SOV') }));
      plain(b);
      for (let i = 0; i < 1e5 && !b.over && (b.contactAt === null || b.t < b.contactAt + 55); i++) b.step();
      b.command(pull ? { do: 'withdrawAll', side: 'att' } : { do: 'dice' }, 'att');
      return b;
    };
    const pulled = at(true), diced = at(false);
    expect(pulled.airLedger.def.withheld).toBeGreaterThan(0);
    expect(pulled.airLedger.def.wasted).toBe(0);
    expect(pulled.airLedger.def.landed).toBeCloseTo(pulled.airLedger.def.withheld, 9);
    // The same fight to the same second: the same air paid, whichever way it ends.
    expect(pulled.airLedger.def.landed).toBeCloseTo(diced.airLedger.def.landed, 9);
  });
  it('handed to the staff, what is owed is paid before the roll, a strike in the air too, and the chance offered counts it', () => {
    const b = new LandBattle(ticket({ att: ON(0.15) }));
    plain(b);
    // To the moment a strike is on its way in, with the bank filling again behind it.
    for (let i = 0; i < 1e5 && !b.over && !(b.airView().air?.att?.eta ?? 0); i++) b.step();
    for (let i = 0; i < 20 && !b.over; i++) b.step();
    expect(b.over).toBeNull();
    expect(b.flightsNow().length).toBeGreaterThan(0);
    // The staff's chance reads the field as the roll will: with what is owed paid.
    const x = b as unknown as { standing(due?: Map<number, number>): Record<Side, number>; airDue(): Map<number, number>; payAir(): void };
    const seen = x.standing(x.airDue());
    const copy = new LandBattle(ticket({ att: ON(0.15) }));
    plain(copy);
    while (copy.t < b.t) copy.step();
    (copy as unknown as typeof x).payAir();
    const paid = copy.standing();
    expect(seen.att).toBeCloseTo(paid.att, 12);
    expect(seen.def).toBeCloseTo(paid.def, 12);
    const left = b.airView().air!.att!.left, flown = b.log.filter((e) => e.what === 'land:air-call' && e.side === 'att').length;
    b.command({ do: 'dice' }, 'att');
    const l = b.airLedger.att;
    expect(l.withheld).toBeGreaterThan(0);
    expect(Math.abs(l.withheld - l.landed - l.wasted)).toBeLessThan(1e-9 * l.withheld);
    expect(l.wasted).toBe(0);
    // The strikes not yet flown are the staff's to fly in the settlement: the bill counts them.
    expect(b.over!.air!.att![0].sorties).toBe(flown + left);
  });
  it('the same ticket and the same orders give the same battle, strikes and all', () => {
    const a = new LandBattle(ticket({ att: ON(0.1), def: ON(0.05, 2, 'SOV') })), b = new LandBattle(ticket({ att: ON(0.1), def: ON(0.05, 2, 'SOV') }));
    plain(a); plain(b);
    expect(fight(a)).toEqual(fight(b));
    expect(a.over).toEqual(b.over);
  });
});

describe('calling a strike', () => {
  it('none before the armies meet; the first is ready a while after, and the staff call it if it is left', () => {
    const b = new LandBattle(ticket({ att: ON(0.1) }));
    plain(b);
    expect(b.airWhyNot('att')).toBe('After the armies meet');
    while (!b.over && b.contactAt === null) b.step();
    const met = b.contactAt!;
    while (!b.over && b.t < met + LAND.air.first - 1) b.step();
    expect(b.airReady('att')).toBe(false);
    expect(b.airWhyNot('att')).toMatch(/^Ready in 0:0[1-9]$/);
    const ev = fight(b);
    const calls = ev.filter((e) => e.what === 'land:air-call' && e.side === 'att');
    expect(calls.length).toBeGreaterThan(0);
    expect(calls[0].extra!.by).toBe('staff');
    expect(calls[0].t).toBeGreaterThanOrEqual(met + LAND.air.first + LAND.air.wait - 0.15);
    // On the player's side the staff's call says so.
    expect(calls[0].text).toBe('The staff called in the fighters.');
  });
  it('his own call goes down the run he points at, and its guns never fire within reach of his own men', () => {
    const b = new LandBattle(ticket({ att: ON(0.1) }));
    plain(b);
    while (!b.over && !b.airReady('att')) b.step();
    expect(b.over).toBeNull();
    // Down his own line: refused.
    const mine = b.units.filter((u) => u.side === 'att' && u.state !== 'gone');
    const own = mine[0];
    expect(b.airWhyNot('att', [own.x - 100, own.y, own.x + 100, own.y])).toBe('Too close to your own men');
    expect(b.command({ do: 'land:air', x0: own.x - 100, y0: own.y, x1: own.x + 100, y1: own.y }, 'att')).toBe(false);
    // At the enemy block nearest the middle of his line: down its frontage.
    const foe = b.units.filter((u) => u.side === 'def' && u.state !== 'gone').sort((p, q) => Math.abs(p.x - 1500) - Math.abs(q.x - 1500))[0];
    const run = b.airRunAt('att', foe.x, foe.y);
    expect(Math.hypot((run[0] + run[2]) / 2 - foe.x, (run[1] + run[3]) / 2 - foe.y)).toBeLessThan(1);
    if (b.airWhyNot('att', run) === null) {
      expect(b.command({ do: 'land:air', x0: run[0], y0: run[1], x1: run[2], y1: run[3] }, 'att')).toBe(true);
      expect(b.airWhyNot('att')).toBe('A strike is on its way');
      const ev = fight(b);
      const call = ev.find((e) => e.what === 'land:air-call' && e.side === 'att');
      expect(b.log.find((e) => e.what === 'land:air-call' && e.side === 'att')!.extra!.by).toBe('you');
      void call;
    }
  });
});

describe('what the 3D game and the campaign are given', () => {
  it('a strike comes in, strafes down its run and pulls away; bombs fall from the first year of fighter-bombers', () => {
    const b = new LandBattle(ticket({ att: ON(0.15, 3, 'GER') }));
    plain(b);
    let seen = false;
    const ev = fight(b, (x) => { const f = x.view().flights; if (f?.length) seen = true; });
    expect(seen).toBe(true);
    const inn = ev.find((e) => e.what === 'land:air-in')!;
    expect(inn.extra).toMatchObject({ nat: 'GER', year: 1942, alt: LAND.air.low });
    expect(inn.cue).toBe('fighter_pass');
    const strafe = ev.find((e) => e.what === 'land:strafe')!;
    expect(strafe.extra).toMatchObject({ width: LAND.air.wide });
    expect(strafe.cue).toBe('machine_gun');
    // The guns open as the run starts, with the flight's coming in; the words come as it ends.
    expect(strafe.t).toBe(inn.t);
    const done = ev.find((e) => e.what === 'land:strafed' && e.extra!.flight === strafe.extra!.flight)!;
    expect(done.t).toBeGreaterThan(strafe.t);
    expect(done.t - strafe.t).toBeCloseTo(strafe.extra!.secs as number, 0);
    const bombs = ev.filter((e) => e.what === 'shell' && e.cal === 'bomb');
    expect(bombs.length).toBeGreaterThan(0);
    expect(bombs[0].reach).toBe(45);
    // Each bomb is let go before it bursts, and bursts where and when its drop said.
    const drops = ev.filter((e) => e.what === 'land:bomb-drop');
    expect(drops.length).toBe(bombs.length);
    for (const fl of new Set(drops.map((d) => d.extra!.flight))) {
      const ds = drops.filter((d) => d.extra!.flight === fl), bs = bombs.filter((b) => b.extra!.flight === fl);
      expect(bs.length).toBe(ds.length);
      ds.forEach((d, i) => {
        const x = d.extra as { to: { x: number; y: number }; at: number };
        expect(bs[i].t).toBeGreaterThanOrEqual(d.t);
        expect(Math.abs(bs[i].x! - x.to.x)).toBeLessThan(0.11);
        expect(Math.abs(bs[i].y! - x.to.y)).toBeLessThan(0.11);
        expect(Math.abs(bs[i].t - x.at)).toBeLessThan(0.25);
      });
    }
  });
  it('before a nation flew fighter-bombers the run strafes alone: Poland in 1939, Germany in 1939', () => {
    for (const nat of ['POL', 'GER']) {
      const b = new LandBattle(ticket({ att: ON(0.15, 3, nat) }, 7919 * 3 + 13, 'September 1939'));
      plain(b);
      const ev = fight(b);
      expect(ev.some((e) => e.what === 'land:strafe')).toBe(true);
      expect(ev.some((e) => e.what === 'shell' && e.cal === 'bomb')).toBe(false);
    }
  });
  it('a flight called comes in from out along its run\'s line, never from the run itself', () => {
    const b = new LandBattle(ticket({ att: ON(0.15) }));
    plain(b);
    while (!b.over && !b.airReady('att')) b.step();
    const foe = b.units.filter((u) => u.side === 'def' && u.state !== 'gone').sort((p, q) => Math.abs(p.x - 1500) - Math.abs(q.x - 1500))[0];
    const run = b.airRunAt('att', foe.x, foe.y);
    expect(b.callAir('att', run)).toBe(true);
    const f = b.flightsNow()[0];
    expect(f.phase).toBe('in');
    expect(Math.hypot(f.x - run[0], f.y - run[1])).toBeCloseTo(LAND.air.eta * LAND.air.speed, 0);
    // And a drawn run is judged as it will be flown: kept on the field and no longer than LAND.air.len.
    const long = b.airRun([-500, foe.y, 4000, foe.y]);
    expect(Math.hypot(long[2] - long[0], long[3] - long[1])).toBeLessThanOrEqual(LAND.air.len + 0.2);
    expect(long[0]).toBeGreaterThanOrEqual(0);
  });
  it('every group on call flies every strike, as the campaign books it', () => {
    const two: AirOnCall = { share: 0.1, strikes: 3, groups: [{ id: 5, name: '5 Group', nat: 'GER', planes: 40, lose: 0.4 }, { id: 6, name: '6 Group', nat: 'ITA', planes: 30, lose: 0.7 }] };
    const b = new LandBattle(ticket({ att: two }));
    plain(b);
    const ev = fight(b);
    const flown = ev.filter((e) => e.what === 'land:air-call' && e.side === 'att').length;
    expect(flown).toBeGreaterThan(0);
    expect(b.over!.air!.att!.map((g) => g.sorties)).toEqual([flown, flown]);
    expect(b.over!.air!.att![0].lost).toBeCloseTo(0.4 * flown, 9);
    expect(b.over!.air!.att![1].lost).toBeCloseTo(0.7 * flown, 9);
    const fell = ev.filter((e) => e.what === 'air:lost' && e.side === 'att').length;
    expect(fell).toBe(Math.floor(0.4 * flown + 1e-9) + Math.floor(0.7 * flown + 1e-9));
  });
  it('the bill: strikes flown and planes lost by group, at the price the campaign set a strike', () => {
    const b = new LandBattle(ticket({ att: ON(0.1, 3, 'GER', 0.6), def: ON(0.05, 2, 'SOV', 0.4) }));
    plain(b);
    const ev = fight(b);
    const air = b.over!.air!;
    for (const s of ['att', 'def'] as Side[]) {
      const flown = ev.filter((e) => e.what === 'land:air-call' && e.side === s).length;
      expect(air[s]![0].sorties).toBe(flown);
      expect(air[s]![0].lost).toBeCloseTo((s === 'att' ? 0.6 : 0.4) * flown, 9);
    }
    // A plane falls for each whole plane the price comes to, never by the dice.
    const fell = ev.filter((e) => e.what === 'air:lost' && e.side === 'att').length;
    expect(fell).toBeLessThanOrEqual(Math.floor(air.att![0].lost + 1e-9));
  });
  it('the practice ground with fighters: the card counts them as the sky term does', () => {
    const p = PRACTICE_GROUNDS.find((g) => g.air)!;
    const bare = practiceReckoning({ ...p, air: false }), k = practiceReckoning(p);
    expect(k.A).toBeCloseTo(bare.A / (1 - PRACTICE_AIR.att.share), 9);
    expect(k.D).toBeCloseTo(bare.D / (1 - PRACTICE_AIR.def.share), 9);
    const t = practiceTicket(p, { humanAtt: true });
    expect(t.att.air).toEqual(PRACTICE_AIR.att);
    expect(t.def.air).toEqual(PRACTICE_AIR.def);
    // The blocks still share out exactly the card's power.
    const b = new LandBattle(t);
    const sum = (s: Side): number => b.units.filter((u: LandUnit) => u.side === s).reduce((a, u) => a + u.power0, 0);
    expect(sum('att')).toBeCloseTo(t.reckoning.A, 6);
  });
});
