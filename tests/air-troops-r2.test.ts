// A raid on troops fought out as an air battle (release 2; Brian, 24 November 2026: "if a bomber group is bombing
// troops instead of a factory or town and they get intercepted than i want that playable"; the design director's
// rulings of 22:28Z, design-director/air-every-battle-1124.md; the 3D side's contract, air-battles/every-kind/CONTRACT.md
// sections 2 to 4). Behind its switch (flyKinds / TC_FLY / TC.airKinds), off by default: with it off a raid on troops is
// refused exactly as before. With it on: the army under the raid as pieces on the board (shown, never counted), the
// bombs and the escort's strafing passes on them, the ground fire paid in the strafers, and the campaign's own law
// for the cut, booked back through fly().
// France's raid on the German army in the Saar in September 1939 stands in for any raid on troops: it is cheap to set
// up, both sides have fighters there, and the French escort has the reach to go down on the troops.
import { afterEach, describe, expect, it } from 'vitest';
import { launchRaid, raidTargets } from '../src/campaign/airwar';
import { airAct } from '../src/campaign/airpages';
import { troopCutOf } from '../src/campaign/airfought';
import { AIRWAR } from '../src/campaign/air';
import { createGame, setWar } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { SKY } from '../src/battle/air/book';
import { AirSim, airBattle } from '../src/battle/air/sim';
import { airTicket, flyKinds, foughtAir, raidGroups } from '../src/battle/air/ticket';
import { TROOPS } from '../src/battle/air/troops';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';

afterEach(() => { flyKinds([]); });

/** The French raid on the Saar, the k-th battle's own luck. */
function saar(k = 0, nat = 'FRA', foe = 'GER', name = 'Saar') {
  const g = startOpen(createGame(7, nat));
  setWar(g, nat, foe);
  g.seed = (7 + 7919 * k) >>> 0;
  g.flags.airBattles = k;
  const target = raidTargets(g, 'ground', nat).find((x) => g.regions[x.id].name === name)!.id;
  const ids = raidGroups(g, 'ground', target, nat);
  return { g, target, ids, nat };
}
function ticketOf(g: Game, target: number, ids: number[], nat: string) {
  flyKinds(['ground']);
  const { ticket, why } = airTicket(g, 'ground', target, ids, nat);
  if (!ticket) throw new Error(why ?? 'no ticket');
  return ticket;
}
/** Fought to the end by the computer on both sides. */
function fight(s: AirSim, each?: (s: AirSim) => void): void {
  s.ai.att = true; s.ai.def = true;
  let guard = 0;
  while (!s.over && guard++ < 20_000) { s.step(); each?.(s); }
  expect(s.over).toBeTruthy();
}

describe('the switch (off by default, bit for bit as before)', () => {
  it('off: a raid on troops is refused with the words it always had, and nothing in the game moves', () => {
    const { g, target, ids, nat } = saar();
    expect(flyKinds()).toEqual([]);
    const before = saveGame(g);
    const r = airTicket(g, 'ground', target, ids, nat);
    expect(r.ticket).toBeNull();
    expect(r.why).toBe('Only day raids and fights for the sky can be fought out so far: the staff settles a night raid.');
    expect(saveGame(g)).toBe(before);
    // The bridge says the same, and answers which kinds are on.
    const TC = battleCalls({ game: () => g, attack: () => null, raid: (k, tg, i, f) => airAct(g, `raid:${k}:${tg}:${i.join('.')}`, f) });
    expect(TC.airKinds().kinds).toEqual([]);
    expect(TC.airLook('ground', target, ids).ticket).toBeNull();
  });

  // (Changed on the design director's ruling of 25 November 2026, 05:12Z, folded under this switch at 12:59Z, question
  // 1: this test used to say the staff's raid books the same with the switch on as off. The ruling changes exactly that:
  // with the switch on, the ground fire and the fighters' weight on the troops fall only on the fighters left with the
  // reach to strafe, campaign/strafers.ts. Everything else the staff's raid books stays as with the switch off.)
  it('the staff\'s raid on troops with no battle: on, it books as off except the ground fire and the fighters\' weight, which fall only on the groups with the reach to strafe', () => {
    // Bombers alone: nothing goes down low either way, so the raid books exactly as with the switch off.
    {
      const a = saar(3), b = saar(3);
      const bombers = a.ids.filter((id) => a.g.airFleets.find((f) => f.id === id)!.ftr === 0);
      expect(bombers.length).toBeGreaterThan(0);
      flyKinds([]);
      const ra = launchRaid(a.g, 'ground', a.target, bombers, a.nat)!;
      flyKinds(['ground']);
      const rb = launchRaid(b.g, 'ground', b.target, bombers, b.nat)!;
      expect(JSON.stringify(rb)).toBe(JSON.stringify(ra));
      expect(saveGame(b.g)).toBe(saveGame(a.g));
    }
    // A raid with a fighter group out of reach to strafe (Groupement de Chasse 1 moved to Namur, 216 km off, beyond
    // 0.9 of its 220 km, the design director's reach of 13:35Z; it stood at Belfort, 190 km off, beyond the old 0.6,
    // which is inside 0.9): the defence, the odds and the bombers book as off; the far group loses only what the air fight
    // took (as off would with no ground fire at all), and the weight on the troops is only the strafers'.
    const far = (k: number) => {
      const s = saar(k);
      s.g.airFleets.find((f) => f.id === 9)!.base = s.g.regions.find((r) => r.name === 'Namur')!.id;
      return { ...s, ids: [...s.ids, 9] };
    };
    for (const k of [3, 4]) {
      const a = far(k), b = far(k), c = far(k);
      flyKinds([]);
      const ra = launchRaid(a.g, 'ground', a.target, a.ids, a.nat)!;
      const fire = AIRWAR.ground.fire;
      let rc;
      try { AIRWAR.ground.fire = 0; rc = launchRaid(c.g, 'ground', c.target, c.ids, c.nat)!; } finally { AIRWAR.ground.fire = fire; }
      flyKinds(['ground']);
      const rb = launchRaid(b.g, 'ground', b.target, b.ids, b.nat)!;
      expect(JSON.stringify(rb.def)).toBe(JSON.stringify(ra.def));
      expect(rb.odds).toBe(ra.odds);
      expect(rb.att.map((x) => x.lostBmr)).toEqual(ra.att.map((x) => x.lostBmr));
      expect(rb.att.find((x) => x.id === 9)!.lostFtr).toBe(rc.att.find((x) => x.id === 9)!.lostFtr);
      // With the switch off the far group pays the ground fire too: at Namur, on these two seeds.
      expect(ra.att.find((x) => x.id === 9)!.lostFtr).toBeGreaterThanOrEqual(rc.att.find((x) => x.id === 9)!.lostFtr);
    }
  });

  it('on through the environment, the bridge, or the call', () => {
    expect(flyKinds(['ground', 'night'])).toEqual(['ground']);
    flyKinds([]);
    (globalThis as { TC_FLY?: string }).TC_FLY = 'ground';
    try { expect(flyKinds()).toEqual(['ground']); } finally { delete (globalThis as { TC_FLY?: string }).TC_FLY; }
    expect(flyKinds()).toEqual([]);
  });
});

describe('the army under the raid (place.marks; CONTRACT.md section 2)', () => {
  it('inside the release ring, never more pieces than the data allows, and the same seed lays the same column', () => {
    for (const k of [0, 1, 2]) {
      const { g, target, ids, nat } = saar(k);
      const t = ticketOf(g, target, ids, nat), M = t.place.marks!;
      expect(M).toBeTruthy();
      expect(M.pieces.length).toBeGreaterThan(10);
      expect(M.pieces.length).toBeLessThanOrEqual(TROOPS.pieces);
      expect(M.nat).toBe('GER');
      for (const p of M.pieces) {
        expect(Math.hypot(p.x - SKY.target.x, p.y - SKY.target.y)).toBeLessThanOrEqual(SKY.target.r * 0.92 + 1);
        expect(p.n).toBeGreaterThan(0);
        expect(Object.keys(TROOPS.unit)).toContain(p.what);
      }
      expect(new Set(M.pieces.map((p) => p.id)).size).toBe(M.pieces.length);
      const again = ticketOf(saar(k).g, target, ids, nat).place.marks!;
      expect(JSON.stringify(again)).toBe(JSON.stringify(M));
      // A JSON round trip keeps every field.
      expect(JSON.parse(JSON.stringify(t)).place.marks).toEqual(M);
    }
    const one = ticketOf(saar(0).g, saar(0).target, saar(0).ids, 'FRA').place.marks!, two = ticketOf(saar(1).g, saar(1).target, saar(1).ids, 'FRA').place.marks!;
    expect(JSON.stringify(one.pieces)).not.toBe(JSON.stringify(two.pieces));
  });

  it('the ticket goes to the raider\'s screen, so it carries no enemy corps\'s strength or organisation', () => {
    // (The raider's side knows an enemy corps only as far as its sight and reports go: armies.ts corpsSeen gives a
    // band or nothing. Nothing on the board needs these two figures, so the ticket must not carry them at all.)
    const { g, target, ids, nat } = saar(0);
    const M = ticketOf(g, target, ids, nat).place.marks!;
    expect(M.corps.length).toBeGreaterThan(0);
    for (const c of M.corps) {
      expect(c).not.toHaveProperty('str');
      expect(c).not.toHaveProperty('org');
    }
  });

  it('a corps\'s kind and nation shape the sample', () => {
    const count = (M: { pieces: { what: string; n: number }[] }, what: string): number => M.pieces.filter((p) => p.what === what).reduce((s, p) => s + p.n, 0);
    // Infantry: men, guns and their haul, no tanks; the haul split between lorries and wagons by the nation's share.
    const inf = saar(0);
    for (const c of inf.g.regions[inf.target].units) if (c.nat === 'GER') c.kind = 'inf';
    const I = ticketOf(inf.g, inf.target, inf.ids, inf.nat).place.marks!;
    expect(count(I, 'tank')).toBe(0);
    expect(count(I, 'men')).toBeGreaterThan(0);
    const lorried = count(I, 'lorry') / (count(I, 'lorry') + count(I, 'wagon'));
    expect(Math.abs(lorried - TROOPS.lorried.GER)).toBeLessThan(0.12);
    // Armour: tanks of the nation's own kit that month; motorised, no horses.
    const arm = saar(0);
    for (const c of arm.g.regions[arm.target].units) if (c.nat === 'GER') c.kind = 'arm';
    const A = ticketOf(arm.g, arm.target, arm.ids, arm.nat).place.marks!;
    expect(count(A, 'tank')).toBeGreaterThan(0);
    expect(count(A, 'wagon')).toBe(0);
    const gerTank = A.pieces.find((p) => p.what === 'tank')!.model;
    expect(gerTank).not.toBe('');
    // The French army under a German raid in Alsace: French tanks, and a larger share of lorries.
    const fra = saar(0, 'GER', 'FRA', 'Alsace');
    for (const c of fra.g.regions[fra.target].units) if (c.nat === 'FRA') c.kind = 'arm';
    const F = ticketOf(fra.g, fra.target, fra.ids, fra.nat).place.marks!;
    expect(F.nat).toBe('FRA');
    const fraTank = F.pieces.find((p) => p.what === 'tank')!.model;
    expect(fraTank).not.toBe(gerTank);
  });

  it('on the march in column along a road, in bivouac, or dug in', () => {
    const camp = saar(0);
    const C = ticketOf(camp.g, camp.target, camp.ids, camp.nat).place.marks!;
    expect(C.posture).toBe('camp');
    expect(C.road).toBeNull();

    const march = saar(0);
    for (const c of march.g.regions[march.target].units) if (c.nat === 'GER') c.ap = Math.max(0, c.apMax - 1);
    const Mc = ticketOf(march.g, march.target, march.ids, march.nat).place.marks!;
    expect(Mc.posture).toBe('march');
    expect(Mc.road!.length).toBeGreaterThan(2);
    // Every piece of a column stands on its road (within the road's bend of a straight line through its ends).
    const [ax, ay] = Mc.road![0], [bx, by] = Mc.road![Mc.road!.length - 1], len = Math.hypot(bx - ax, by - ay);
    for (const p of Mc.pieces) expect(Math.abs(((bx - ax) * (ay - p.y) - (ax - p.x) * (by - ay)) / len)).toBeLessThan(0.1 * SKY.target.r + 2);

    const dug = saar(0);
    for (const c of dug.g.regions[dug.target].units) if (c.nat === 'GER') c.dug = true;
    const D = ticketOf(dug.g, dug.target, dug.ids, dug.nat).place.marks!;
    expect(D.posture).toBe('dug');
    expect(D.road).toBeNull();
    expect(D.pieces.every((p) => p.posture === 'dug')).toBe(true);
  });
});

describe('fought out by the computer on both sides', () => {
  it('the events keep to the contract, the men shown never pass the men booked, and the outcome carries the strafing and the cut', () => {
    let strafes = 0, flak = 0, hits = 0;
    for (const k of [0, 1, 2]) {
      const { g, target, ids, nat } = saar(k);
      const t = ticketOf(g, target, ids, nat), M = t.place.marks!;
      const ids0 = new Set(M.pieces.map((p) => p.id));
      const s = new AirSim(t, { show: true });
      fight(s, (sim) => {
        const v = sim.view();
        expect(v.marks).toBeTruthy();
        expect(v.marks!.fire).toBeGreaterThanOrEqual(0);
        expect(v.marks!.fire).toBeLessThanOrEqual(1);
        for (const p of v.marks!.pieces) { expect(ids0.has(p.id)).toBe(true); expect(['ok', 'hit', 'burn', 'out']).toContain(p.state); expect(p.men).toBeGreaterThanOrEqual(0); }
        const { shown, booked } = (sim as unknown as { troops: { tally(): { shown: number; booked: number } } }).troops.tally();
        expect(shown).toBeLessThanOrEqual(booked + 1e-6);
      });
      for (const e of s.history()) {
        if (e.what === 'air:mark-hit') {
          hits++;
          const x = e.extra as Record<string, unknown>;
          expect(ids0.has(x.piece as number)).toBe(true);
          expect(['bomb', 'strafe']).toContain(x.strike);
          expect(['burn', 'wreck', 'blast', 'scatter', 'near']).toContain(x.how);
          expect(typeof x.big).toBe('boolean');
          expect(x.men as number).toBeGreaterThanOrEqual(0);
          expect(Number.isFinite(e.x) && Number.isFinite(e.y)).toBe(true);
          expect(s.units.some((u) => u.id === e.unit)).toBe(true);
        }
        if (e.what === 'air:strafe') {
          strafes++;
          const x = e.extra as { from: number[]; to: number[]; width: number; dur: number };
          expect(x.from.length).toBe(2); expect(x.to.length).toBe(2);
          expect(x.width).toBeGreaterThan(0); expect(x.dur).toBeGreaterThan(0);
          expect(s.units.find((u) => u.id === e.unit)!.air).toBe('ftr');
        }
        if (e.what === 'air:lost' && (e.extra as { by?: string }).by === 'flak') flak++;
      }
      const o = s.outcome();
      expect(o.strafed).toBeGreaterThanOrEqual(0);
      expect(o.troops!.cut).toBeCloseTo(troopCutOf(M.law, o.dropped, o.strafed!), 12);
      expect(foughtAir(o).strafed).toBe(o.strafed);
    }
    expect(hits).toBeGreaterThan(0);
    expect(strafes).toBeGreaterThan(0);
    expect(flak).toBeGreaterThan(0);
  });

  it('fighters held as escort strafe nothing, and earn less', () => {
    let free = 0, held = 0;
    for (const k of [0, 1, 2, 3]) {
      const a = saar(k), b = saar(k);
      const sa = airBattle(ticketOf(a.g, a.target, a.ids, a.nat)) as AirSim;
      fight(sa);
      const sb = airBattle(ticketOf(b.g, b.target, b.ids, b.nat)) as AirSim;
      const ftr = sb.units.filter((u) => u.side === 'att' && u.air === 'ftr').map((u) => u.id);
      expect(sb.command({ do: 'air:strafe', units: ftr, hold: true }, 'att')).toBe(true);
      fight(sb);
      expect(sb.outcome().strafed).toBe(0);
      expect(sb.history().some((e) => e.what === 'air:strafe')).toBe(false);
      free += sa.outcome().troops!.cut; held += sb.outcome().troops!.cut;
    }
    expect(held).toBeLessThan(free);
  });
});

describe('the commands', () => {
  it('a strafer called up goes back to the sky, sent down again it strafes; no other raid takes the order', () => {
    const { g, target, ids, nat } = saar(1);
    const s = airBattle(ticketOf(g, target, ids, nat)) as AirSim;
    s.ai.att = true; s.ai.def = true;
    let guard = 0;
    while (!s.over && !s.units.some((u) => u.job.do === 'strafe') && guard++ < 20_000) s.step();
    const u = s.units.find((x) => x.job.do === 'strafe')!;
    expect(u).toBeTruthy();
    expect(s.command({ do: 'air:band', units: [u.id], band: 1 }, 'att')).toBe(true);
    expect(u.job.do).not.toBe('strafe');
    // The staff leaves it up there (the player's order stands)...
    for (let i = 0; i < 20 && !s.over; i++) s.step();
    expect(u.job.do).not.toBe('strafe');
    // ...until he sends it down.
    expect(s.command({ do: 'air:strafe', units: [u.id] }, 'att')).toBe(true);
    expect(u.job.do).toBe('strafe');
    // A day raid on a town has no troops to strafe.
    flyKinds([]);
    const bomb = startOpen(createGame(7, 'FRA'));
    setWar(bomb, 'FRA', 'GER');
    const tb = raidTargets(bomb, 'bomb', 'FRA').find((x) => airTicket(bomb, 'bomb', x.id, raidGroups(bomb, 'bomb', x.id, 'FRA'), 'FRA').ticket)!;
    const sb = airBattle(airTicket(bomb, 'bomb', tb.id, raidGroups(bomb, 'bomb', tb.id, 'FRA'), 'FRA').ticket!) as AirSim;
    expect(sb.command({ do: 'air:strafe', units: sb.units.map((x) => x.id) }, 'att')).toBe(false);
    expect(sb.view().marks).toBeUndefined();
  });
});

describe('booked back through fly()', () => {
  it('fought to the end: the field\'s losses stand, ground fire is not taken twice, and the cut booked is the field\'s', () => {
    for (const k of [0, 2]) {
      const { g, target, ids, nat } = saar(k);
      const t = ticketOf(g, target, ids, nat);
      const s = airBattle(t) as AirSim;
      fight(s);
      const o = s.outcome(), fought = foughtAir(o);
      const foes = g.regions[target].units.filter((c) => !c.dead && c.nat === 'GER');
      const org0 = foes.map((c) => c.org);
      const rep = launchRaid(g, 'ground', target, ids, nat, fought)!;
      expect(rep).toBeTruthy();
      for (const x of rep.att) {
        const had = fought.lost[x.id!] ?? { ftr: 0, bmr: 0 };
        expect(x.lostFtr, `${x.fleet}`).toBe(had.ftr);
        expect(x.lostBmr).toBe(had.bmr);
      }
      for (const x of rep.def) expect(x.lostFtr).toBe((fought.lost[x.id!] ?? { ftr: 0 }).ftr);
      const cut = troopCutOf(t.place.marks!.law, o.dropped, o.strafed!);
      expect(cut).toBeGreaterThan(0);
      foes.forEach((c, i) => {
        if (org0[i] - cut > 0.1) expect(org0[i] - c.org).toBeCloseTo(cut, 9);
      });
      // A saved game with the fought raid in it loads.
      const back = loadGame(saveGame(g));
      expect(back).toBeTruthy();
      expect(JSON.stringify(back!.airReports[back!.airReports.length - 1])).toBe(JSON.stringify(rep));
    }
  });

  it('handed to the staff partway: never fewer losses than the field had, and the rest flown by the dice', () => {
    let pressed = 0;
    for (const k of [0, 1, 2, 3]) {
      const { g, target, ids, nat } = saar(k);
      const s = airBattle(ticketOf(g, target, ids, nat)) as AirSim;
      s.ai.att = true; s.ai.def = true;
      let guard = 0;
      while (!s.over && s.history().filter((e) => e.what === 'air:lost').length < 8 && guard++ < 20_000) s.step();
      if (s.over) continue;
      pressed++;
      s.command({ do: 'dice' }, 'att');
      const o = s.outcome(), fought = foughtAir(o);
      expect(o.staff).toBeTruthy();
      expect(o.troops!.cut).toBeGreaterThanOrEqual(0);
      const rep = launchRaid(g, 'ground', target, ids, nat, fought)!;
      for (const x of [...rep.att, ...rep.def]) {
        const had = fought.lost[x.id!] ?? { ftr: 0, bmr: 0 };
        expect(x.lostFtr).toBeGreaterThanOrEqual(had.ftr);
        expect(x.lostBmr).toBeGreaterThanOrEqual(had.bmr);
      }
      expect(loadGame(saveGame(g))).toBeTruthy();
    }
    expect(pressed).toBeGreaterThan(0);
  });

  it('through the bridge: switched on, looked at, fought, and booked by the raid order', () => {
    // (France's own month: the raid order is taken only when it is his turn.)
    const g = startOpen(createGame(7, 'FRA'));
    setWar(g, 'FRA', 'GER');
    let guard = 0;
    while (!humanUp(g) && guard++ < 200) stepAI(g);
    beginHuman(g);
    const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
    const ids = raidGroups(g, 'ground', target, 'FRA');
    const TC = battleCalls({ game: () => g, attack: () => null, raid: (k, tg, i, f) => airAct(g, `raid:${k}:${tg}:${i.join('.')}`, f) });
    expect(TC.airKinds(['ground']).kinds).toEqual(['ground']);
    expect(TC.airLook('ground', target, ids).ticket).toBeTruthy();
    const st = TC.airStart('ground', target, ids)!;
    expect((st.view as { marks?: unknown }).marks).toBeTruthy();
    TC.battleSpeed(4);
    let v: Record<string, unknown> = {};
    for (let k = 0; k < 4000 && !v.over; k++) v = (TC.battleTick(5) ?? {}) as Record<string, unknown>;
    expect(v.over).toBeTruthy();
    expect(v.marks).toBeTruthy();
    const r0 = g.airReports.length;
    const out = TC.battleEnd() as { ok: boolean; note?: string };
    expect(out.ok, out.note).toBe(true);
    expect(g.airReports.length).toBe(r0 + 1);
    expect(TC.airKinds([]).kinds).toEqual([]);
  });
});
