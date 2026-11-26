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
import { AIRWAR, assessAir, canStrafe, cover, fighterReach } from '../src/campaign/air';
import { createGame, setWar } from '../src/campaign/game';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import { strafeWatch, strafedLine } from '../src/campaign/strafers';
import type { Game } from '../src/campaign/types';
import { SKY } from '../src/battle/air/book';
import { AirSim, airBattle } from '../src/battle/air/sim';
import type { FieldEvent } from '../src/battle/core/types';
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
      // (Changed in the air lanes' fourth round, the UI director's words of 15:04Z: with the switch on the report keeps
      // who went down, AirReport.strafed, here none. That one field apart, the raid books exactly as off. rb is the
      // report inside b.g, so the delete below takes the field off the saved game too: the saves are then compared with
      // that one field apart, as the reports are. tests/air-troops-words-r2.test.ts checks that a save keeps the field.)
      expect(rb.strafed).toEqual([]);
      expect('strafed' in ra).toBe(false);
      delete rb.strafed;
      // (Changed in round seven, the UI director 19:17Z, reading 4: with the switch on, where the cut prints as 0% the
      // report says "The aircraft found little to hit." in place of "... by 0%", and so does the raid's headline, which
      // carries the report's sentence. That sentence apart too, as off.)
      const nil = (x: string): string => x.replace(/The raids cut the organisation of [^.]* by 0%\. They fight the weaker for it until they have rested\./g, 'The aircraft found little to hit.');
      expect(ra.result).toMatch(/^The raids cut the organisation of .* by 0%/);
      expect(rb.result).toBe('The aircraft found little to hit.');
      expect(JSON.stringify(rb)).toBe(nil(JSON.stringify(ra)));
      expect(saveGame(b.g)).toBe(nil(saveGame(a.g)));
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

// The fight a group carries (the design director, 18:14Z and 18:19Z): "carried = 1 - (the defenders' weight it met / its
// own fighters' weight), floor 0" ... "the defenders' weight met is each defending group in contact, at its starting
// weight, shared by weight among the raid's groups that met it" ... "reading the share afresh at every go-down and pass
// closes both field faults". The raid as round six's first check flew it: the bombers, Groupement de Chasse 3 at Lorraine,
// Groupement de Chasse 2 moved to Verdun and Groupement de Chasse 1 to Namur, against 2. Jagddivision at a strength the
// setup sets (setup only).
describe('the fight a group carries after its air fight (18:14Z): read afresh at every go-down and every pass', () => {
  const GC3 = 10;
  function raid(defenders: number) {
    const g = startOpen(createGame(7, 'FRA'));
    setWar(g, 'FRA', 'GER');
    g.airFleets.find((f) => f.id === 36)!.base = g.regions.find((r) => r.name === 'Verdun')!.id;
    g.airFleets.find((f) => f.id === 9)!.base = g.regions.find((r) => r.name === 'Namur')!.id;
    const d = g.airFleets.find((f) => f.id === 3)!;
    d.ftr = defenders; d.est.ftr = Math.max(d.est.ftr, defenders);
    const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
    const s = airBattle(ticketOf(g, target, [38, GC3, 36, 9], 'FRA')) as AirSim;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const S = s as any;
    const unit = s.units.find((u) => u.side === 'att' && u.corps === GC3)!;
    // Every pass the group begins (its share k), and the fighters each pass it makes counts on the troops (n: its fighters
    // as the pass is made).
    const passes: { k: number; n: number; add: number }[] = [];
    const pass = S.pass.bind(S);
    S.pass = (u: AirSim['units'][number]) => {
      const st = S.strafing.get(u.id), run = st?.run, done = st?.done, was = S.strafed;
      pass(u);
      if (u.id !== unit.id || !st) return;
      if (!run && st.run) passes.push({ k: st.k, n: u.n, add: 0 });
      if (st.done > done) Object.assign(passes[passes.length - 1], { n: u.n, add: S.strafed - was });
    };
    return { s, S, unit, passes };
  }
  const reachOf = (u: AirSim['units'][number]): number => Math.min(1, u.fuel0 / SKY.fuel.full);

  it('as the computer flies it: every escort goes down with 1 - D / E of its fight once its own kill ledger is spent, and each pass counts in proportion', () => {
    const { s, S, unit, passes } = raid(200);
    const down: { vig: number; c: number }[] = [];
    const go = S.goDown.bind(S);
    S.goDown = (u: AirSim['units'][number], box: number | null) => { go(u, box); if (u.id === unit.id && u.job.do === 'strafe') down.push({ vig: u.vig, c: S.carries(u) }); };
    fight(s);
    // Every French fighter group met 2. Jagddivision, so each carries 1 less its weight over all of theirs.
    const W = (u: AirSim['units'][number]): number => S.weight(u, u.n0);
    const att = s.units.filter((u) => u.side === 'att' && u.air === 'ftr'), def = s.units.filter((u) => u.side === 'def' && u.air === 'ftr');
    for (const u of att) expect([...S.met.get(u.id)]).toEqual(def.map((d) => d.id));
    const carry = 1 - def.reduce((t, d) => t + W(d), 0) / att.reduce((t, u) => t + W(u), 0);
    expect(carry).toBeGreaterThan(0.5);
    expect(carry).toBeLessThan(0.6);
    // Ruling 4 on the measure: its air fight over (the ledger spent, vig below SKY.spent), it still goes down with what it carries.
    expect(down.length).toBeGreaterThan(0);
    for (const x of down) { expect(x.vig).toBeLessThan(SKY.spent); expect(x.c).toBeCloseTo(carry, 12); }
    expect(passes.length).toBe(TROOPS.strafe.passes);
    for (const p of passes) {
      expect(p.k).toBeCloseTo(carry, 12);
      expect(p.add).toBeCloseTo((p.n * reachOf(unit) * carry) / TROOPS.strafe.passes, 9);
    }
    expect(s.outcome().strafers).toContain(GC3);
  });

  it('sent down, then to hunt until fought out, then down again: it strafes with what it carries after the hunt, and with none it goes home (round six\'s first check, its first fault)', () => {
    for (const defenders of [200, 800]) {
      const { s, S, unit, passes } = raid(defenders);
      s.ai.att = false; s.ai.def = true;
      expect(s.command({ do: 'air:strafe', units: [unit.id] }, 'att')).toBe(true);
      expect(unit.job.do).toBe('strafe');
      expect(s.command({ do: 'air:hunt', units: [unit.id] }, 'att')).toBe(true);
      let guard = 0;
      while (!s.over && !(unit.vig < SKY.spent) && guard++ < 40_000) s.step();
      while (!s.over && unit.contacts.length && guard++ < 40_000) s.step();
      expect(unit.vig).toBeLessThan(SKY.spent);
      expect(S.met.get(unit.id)?.size).toBe(1);
      const carried = S.carries(unit);
      expect(s.command({ do: 'air:hunt', units: [unit.id] }, 'att')).toBe(true);
      expect(s.command({ do: 'air:strafe', units: [unit.id] }, 'att')).toBe(true);
      while (!s.over && guard++ < 40_000) s.step();
      if (defenders === 800) {
        // Outweighed by the defenders it met: it carries none, and the order sends it home.
        expect(carried).toBe(0);
        expect(passes).toEqual([]);
        expect(s.outcome().strafers).not.toContain(GC3);
      } else {
        // It carries part of its fight, never all of it: every pass at that share.
        expect(carried).toBeGreaterThan(0);
        expect(carried).toBeLessThan(1);
        expect(passes.length).toBe(TROOPS.strafe.passes);
        for (const p of passes) { expect(p.k).toBeCloseTo(carried, 12); expect(p.add).toBeCloseTo((p.n * reachOf(unit) * carried) / TROOPS.strafe.passes, 9); }
      }
    }
  });

  it('jumped on its passes by a defender it had not met: the passes flown stand, the passes still to fly count at the less it carries (round six\'s first check, its second fault)', () => {
    const { s, S, unit, passes } = raid(200);
    s.ai.att = true; s.ai.def = true;
    // Sent down at once, before it meets anyone: it carries all of its fight.
    expect(s.command({ do: 'air:strafe', units: [unit.id] }, 'att')).toBe(true);
    let guard = 0;
    while (!s.over && !(S.strafing.get(unit.id)?.done >= 1) && guard++ < 40_000) s.step();
    expect(S.met.get(unit.id)).toBeUndefined();
    // (Setup: the German group is put on its tail, at its height, to jump it on its passes.)
    for (const d of s.units.filter((u) => u.side === 'def' && u.air === 'ftr')) { d.job = { do: 'intercept', on: unit.id }; d.x = unit.x + 300; d.y = unit.y; d.band = unit.band; d.to = unit.band; }
    while (!s.over && guard++ < 40_000) s.step();
    expect(S.met.get(unit.id)?.size).toBe(1);
    const after = S.carries(unit);
    expect(after).toBeGreaterThan(0);
    expect(after).toBeLessThan(1);
    expect(passes[0].k).toBe(1);
    expect(passes[0].add).toBeCloseTo((passes[0].n * reachOf(unit)) / TROOPS.strafe.passes, 9);
    expect(passes.length).toBe(TROOPS.strafe.passes);
    for (const p of passes.slice(1)) { expect(p.k).toBeCloseTo(after, 12); expect(p.add).toBeCloseTo((p.n * reachOf(unit) * after) / TROOPS.strafe.passes, 9); }
  });

  // The design director, 25 November 2026 19:09Z, item 2: "'A group with none goes home' is decided at the go-down: a
  // group that reads 0 there turns for home and is gone for the raid, and no later share brings it back ... Add a test:
  // a group sent home at 0 never strafes later in that raid." Round five's edge raid (309 German fighters): Groupement de
  // Chasse 3 alone is outweighed by 2. Jagddivision (it reads 0), and the raid's groups together are not (each reads
  // about 0.35 once all have met it).
  it('a group sent home at 0 at its go-down never strafes later in that raid: not when its share rises, not on the staff\'s later ticks, not on the player\'s later order (19:09Z, item 2)', () => {
    const { s, S, unit, passes } = raid(309);
    const foe = s.units.find((u) => u.side === 'def' && u.air === 'ftr')!;
    const others = s.units.filter((u) => u.side === 'att' && u.air === 'ftr' && u.id !== unit.id);
    s.ai.att = true; s.ai.def = true;
    // Groupement de Chasse 3 is sent ahead to hunt (the player's order), and meets the German group alone, before the
    // raid's other groups: outweighed, it reads 0.
    expect(s.command({ do: 'air:hunt', units: [unit.id] }, 'att')).toBe(true);
    let guard = 0;
    while (!s.over && !S.met.get(unit.id)?.size && guard++ < 40_000) s.step();
    expect([...S.met.get(unit.id)]).toEqual([foe.id]);
    for (const u of others) expect(S.met.get(u.id)).toBeUndefined();
    expect(S.carries(unit)).toBe(0);
    // Its go-down, by the player's order: it reads 0 there, turns for home and is gone for the raid. The order is answered
    // (the UI director, 19:10Z A6), once, to the side that flies it as the battle's other lines are.
    // ({Group} as the battle names a group at the head of its other lines, names.ts told: "The Groupement de Chasse 3".)
    const A6 = 'The Groupement de Chasse 3 has no fight left to strafe with and turns for home.';
    const said = (): FieldEvent[] => s.history().filter((e) => e.what === 'air:no-strafe');
    expect(said()).toEqual([]);
    expect(s.command({ do: 'air:strafe', units: [unit.id] }, 'att')).toBe(true);
    expect(unit.job.do).toBe('home');
    expect(S.strafing.get(unit.id)).toBeUndefined();
    expect(said().map((e) => [e.text, e.unit, e.side])).toEqual([[A6, unit.id, 'att']]);
    // (Setup: its kill ledger spent, as a group's that has fought itself out, so that every other thing the staff asks
    // before it sends a group down, strafeDue, can hold for it on the ticks below.)
    unit.vig = SKY.spent / 2;
    // Its share rises: the computer flies the raid on, the raid's other groups meet the same German group, and its
    // weight is shared among them.
    while (!s.over && !others.some((u) => S.met.get(u.id)?.has(foe.id)) && guard++ < 40_000) { s.step(); expect(unit.job.do).not.toBe('strafe'); }
    const risen = S.carries(unit);
    expect(risen).toBeGreaterThan(0);
    // The player's later order, its share above 0: another order first, then down to strafe. It turns for home again.
    expect(s.command({ do: 'air:hunt', units: [unit.id] }, 'att')).toBe(true);
    expect(unit.job.do).toBe('hunt');
    expect(s.command({ do: 'air:strafe', units: [unit.id] }, 'att')).toBe(true);
    expect(unit.job.do).toBe('home');
    expect(said().map((e) => e.text)).toEqual([A6, A6]);
    // And sent down again on its way home: the same answer.
    expect(s.command({ do: 'air:strafe', units: [unit.id] }, 'att')).toBe(true);
    expect(unit.job.do).toBe('home');
    expect(said().length).toBe(3);
    // The staff's later ticks, to the end of the raid: never sent down again.
    let most = risen;
    while (!s.over && guard++ < 40_000) { s.step(); most = Math.max(most, S.carries(unit)); expect(unit.job.do).not.toBe('strafe'); }
    expect(s.over).toBeTruthy();
    expect(most).toBeGreaterThan(0.3);
    // Never a pass, never charged the ground fire, never among the field's strafers; the raid's other groups strafe.
    expect(passes).toEqual([]);
    expect(S.strafing.get(unit.id)).toBeUndefined();
    expect(s.outcome().strafers).not.toContain(GC3);
    expect(s.outcome().strafers!.length).toBeGreaterThan(0);
    // Said only on the player's orders, never by the staff's go-downs.
    expect(said().length).toBe(3);
  });
});

describe('the fuel to go down (the design director, 19:09Z, item 1: "reach is fuel"): a group that did not fight', () => {
  afterEach(() => { strafeWatch.tell = undefined; });
  /** The Soviet raid on the Polish troops at Wilno (round six's gate): its three fighter divisions 176 km out, 0.88 of their range. */
  function wilno(k = 0) {
    const g = startOpen(createGame(7, 'SOV'));
    setWar(g, 'SOV', 'POL');
    g.airFleets.find((f) => f.id === 14)!.base = g.regions.find((r) => r.name === 'Wilno')!.id;
    g.seed = (7 + 7919 * k) >>> 0;
    g.flags.airBattles = k;
    flyKinds(['ground']);
    const target = raidTargets(g, 'ground', 'SOV').find((x) => g.regions[x.id].name === 'Wilno')!.id;
    return { g, target, ids: raidGroups(g, 'ground', target, 'SOV') };
  }
  it('the same raid, settled and flown, agree on who has the fuel to go down: the rule\'s reach (0.9 of the range) decides, and inside it the board\'s fuel never refuses one', () => {
    const { g, target, ids } = wilno();
    const a = assessAir(g, g.airFleets.filter((f) => ids.includes(f.id)), 'ground', target);
    const ftr = a.att.filter((x) => x.ftr > 0);
    const G = AIRWAR.ground.reach!;
    expect(G).toBe(0.9);
    expect(ftr.length).toBe(3);
    for (const x of ftr) {
      const R = fighterReach(g, x.f.nat, x.f.base);
      expect(x.d / R).toBeGreaterThan(0.85);
      expect(x.d / R).toBeLessThan(G);
      expect(canStrafe(g, x)).toBe(true);
    }
    // Settled, with no defender up (setup: the Polish fighters stay on the ground): all three go down.
    const quiet = loadGame(saveGame(g))!;
    quiet.airFleets.find((f) => f.id === 14)!.ftr = 0;
    let heard: { route: string; strafers: number[] }[] = [];
    strafeWatch.tell = (x) => { heard.push(JSON.parse(JSON.stringify(x))); };
    launchRaid(quiet, 'ground', target, ids, 'SOV');
    const settled = heard.find((x) => x.route === 'settled')!.strafers;
    expect([...settled].sort((x, y) => x - y)).toEqual(ftr.map((x) => x.f.id).sort((x, y) => x - y));
    const R = fighterReach(g, 'SOV', ftr[0].f.base);
    // Flown, the same raid with no defender up (setup: the defending fighters taken off the ticket), each group's board
    // fuel SKY.fuel.full x cover(f x R, R) as the ticket gives it at that share f of its range (ticket.ts), the share swept
    // up to the rule's own reach: every group the rule gives the reach goes down and flies all its passes, as the settled
    // raid sends it; the board's fuel test (sim.ts fuelFor) holds on every one at its go-down.
    for (const f of [0.5, 0.7, 0.8, 0.85, G]) {
      const t = ticketOf(g, target, ids, 'SOV');
      for (const x of t.def.forces) if (x.kind !== 'bmr') delete t.place.starts[x.corps];
      const mine = t.att.forces.filter((x) => x.kind !== 'bmr').map((x) => x.corps);
      for (const id of mine) t.place.starts[id].fuel = SKY.fuel.full * cover(f * R, R);
      expect([...t.place.marks!.law.reach!].sort((x, y) => x - y)).toEqual([...settled].sort((x, y) => x - y));
      const s = airBattle(t);
      const S = s as unknown as { fuelFor(u: unknown): boolean; strafing: Map<number, { done: number }> };
      const asked: boolean[] = [];
      const own = S.fuelFor.bind(s);
      S.fuelFor = (u) => { const ok = own(u); asked.push(ok); return ok; };
      fight(s);
      expect(asked.length).toBeGreaterThan(0);
      expect(asked.every(Boolean)).toBe(true);
      expect(s.outcome().strafers).toEqual([...settled].sort((x, y) => x - y));
      for (const u of s.units.filter((x) => x.side === 'att' && x.air === 'ftr')) expect(S.strafing.get(u.id)?.done).toBe(TROOPS.strafe.passes);
    }
  });
});

// The design director, 25 November 2026 20:55Z, on the fight's half of ruling 1 (C1): "Bingo is where the settled raid
// prices the fight's fuel: a group turns for home with what it takes to get there. So a group within the rule's reach that
// turned at bingo is not asked for fuel again at its go-down; asking twice counts the fight twice. Beyond the reach, fuel
// is asked as before." Two raids whose defence outlasts each group's kill ledger, with groups near the rule's reach: the
// Soviet raid on Wilno (the gate's: three fighter divisions at 0.88 of their range) and the British raid on Frankfurt from
// Alsace (No. 11 Group at 0.89, against 2. Jagddivision set to 20). Fought as the gate flies them, every group the settled
// raid sends down goes down; bingo is as it is, so a group may still turn for home short of fuel on its passes.
describe('the fuel to go down after a fight (the design director, 20:55Z, C1): a group within the rule\'s reach is not asked for fuel again at its go-down', () => {
  afterEach(() => { strafeWatch.tell = undefined; });
  type Raid = { nat: string; wars: string[]; target: string; ids?: number[]; move: Record<number, string>; def?: [number, number][] };
  const WILNO: Raid = { nat: 'SOV', wars: ['POL'], target: 'Wilno', move: { 14: 'Wilno' } };
  const FRANKFURT: Raid = { nat: 'UK', wars: ['GER'], target: 'Frankfurt', ids: [35, 5], move: { 5: 'Alsace', 35: 'Alsace' }, def: [[3, 20]] };
  const SAAR200: Raid = { nat: 'FRA', wars: ['GER'], target: 'Saar', ids: [38, 10, 36, 9], move: { 36: 'Verdun', 9: 'Namur' } };
  /** The raid as the gate sets it up, the k-th battle's own luck. */
  function setUp(r: Raid, k: number) {
    const g = startOpen(createGame(7, r.nat));
    for (const w of r.wars) setWar(g, r.nat, w);
    for (const [id, at] of Object.entries(r.move)) g.airFleets.find((f) => f.id === +id)!.base = g.regions.find((x) => x.name === at)!.id;
    for (const [id, n] of r.def ?? []) { const d = g.airFleets.find((f) => f.id === id)!; d.ftr = n; d.est.ftr = Math.max(d.est.ftr, n); }
    g.seed = (7 + 7919 * k) >>> 0;
    g.flags.airBattles = k;
    flyKinds(['ground']);
    const target = raidTargets(g, 'ground', r.nat).find((x) => g.regions[x.id].name === r.target)!.id;
    return { g, target, ids: r.ids ?? raidGroups(g, 'ground', target, r.nat) };
  }
  /** Settled with no battle: the groups the staff's own raid sends down. */
  function settled(r: Raid, k: number): number[] {
    const { g, target, ids } = setUp(r, k);
    const heard: { route: string; strafers: number[] }[] = [];
    strafeWatch.tell = (x) => { heard.push(JSON.parse(JSON.stringify(x))); };
    launchRaid(g, 'ground', target, ids, r.nat);
    return [...heard.find((x) => x.route === 'settled')!.strafers].sort((x, y) => x - y);
  }
  /** Fought to the end by the computer on both sides: the groups that went down (began a pass) and the field's strafers. */
  function flown(r: Raid, k: number) {
    const { g, target, ids } = setUp(r, k);
    const t = ticketOf(g, target, ids, r.nat);
    const s = airBattle(t);
    const S = s as unknown as { strafing: Map<number, { done: number }> };
    fight(s);
    const down = s.units.filter((u) => u.side === 'att' && u.air === 'ftr' && (S.strafing.get(u.id)?.done ?? 0) >= 1).map((u) => u.corps).sort((x, y) => x - y);
    return { s, t, down, strafers: s.outcome().strafers ?? [] };
  }

  it('Wilno, fought as the gate flies it: the three Soviet fighter divisions go down on every seed, as the settled raid sends them', () => {
    const want = settled(WILNO, 0);
    expect(want.length).toBe(3);
    for (const k of [0, 1, 2]) {
      const f = flown(WILNO, k);
      expect([...f.t.place.marks!.law.reach!].sort((x, y) => x - y)).toEqual(want);
      expect(f.down).toEqual(want);
      expect([...f.strafers].sort((x, y) => x - y)).toEqual(want);
      // Each says so in the battle's own words.
      for (const u of f.s.units.filter((x) => x.side === 'att' && want.includes(x.corps))) {
        expect(f.s.history().some((e) => e.what === 'air:strafe' && e.unit === u.id && /goes down to strafe the troops at Wilno\.$/.test(e.text ?? ''))).toBe(true);
      }
    }
  });

  it('the British raid on Frankfurt from Alsace (No. 11 Group near the edge of its reach): it goes down on every seed, as the settled raid sends it', () => {
    const want = settled(FRANKFURT, 0);
    expect(want).toEqual([5]);
    for (const k of [0, 1]) {
      const f = flown(FRANKFURT, k);
      expect(f.down).toEqual(want);
      expect([...f.strafers].sort((x, y) => x - y)).toEqual(want);
    }
  });

  it('with the rule on, a group without the rule\'s reach never goes down, and is never asked for fuel to go down (Groupement de Chasse 1 at Namur, 0.98 of its range)', () => {
    const want = settled(SAAR200, 0);
    expect(want).not.toContain(9);
    for (const k of [0, 1]) {
      const { g, target, ids } = setUp(SAAR200, k);
      const t = ticketOf(g, target, ids, 'FRA');
      expect(t.place.marks!.law.reach).not.toContain(9);
      const s = airBattle(t);
      const S = s as unknown as { fuelFor(u: { corps: number }): boolean; strafing: Map<number, { done: number }> };
      const asked: number[] = [];
      const own = S.fuelFor.bind(s);
      S.fuelFor = (u) => { asked.push(u.corps); return own(u); };
      const far = s.units.find((u) => u.side === 'att' && u.corps === 9)!;
      fight(s, () => { expect(far.job.do).not.toBe('strafe'); });
      expect(asked).not.toContain(9);
      expect(S.strafing.get(far.id)).toBeUndefined();
      expect(s.outcome().strafers).not.toContain(9);
      expect([...s.outcome().strafers!].sort((x, y) => x - y)).toEqual(want);
    }
  });
});

// The design director, 25 November 2026 16:04Z: a group goes down when it is "Fought: no enemy fighter is engaging that
// group, and it has the reach and the fuel". think()'s branch for a raid with no bombers left on the board (sim.ts) once
// sent a group down while a German group was still engaging it (round seven's check). The Saar with the French bombers
// cut to one (setup), lost before the target, against 2. Jagddivision at 750: both French escorts are still engaged
// as the last bomber goes. Under the strafers' rule neither begins its go-down until it is free of the air fight; it
// fights on meanwhile, and goes down, or home with no fight left (19:09Z, item 2), once free.
describe('the go-down waits for the end of the air fight (the design director, 16:04Z): think\'s branch for a raid with no bombers left', () => {
  it('a group never begins its go-down while a defending fighter group is in contact with it, and is never left without a job', () => {
    const g = startOpen(createGame(7, 'FRA'));
    setWar(g, 'FRA', 'GER');
    g.airFleets.find((f) => f.id === 36)!.base = g.regions.find((r) => r.name === 'Verdun')!.id;
    const d = g.airFleets.find((f) => f.id === 3)!;
    d.ftr = 750; d.est.ftr = Math.max(d.est.ftr, 750);
    const ids = [38, 10, 36];
    for (const id of ids) { const f = g.airFleets.find((q) => q.id === id)!; if (f.bmr > 0) f.bmr = 1; }
    g.seed = (911 + 7907 * 14) >>> 0;
    g.flags.airBattles = 84;
    const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
    const s = airBattle(ticketOf(g, target, ids, 'FRA'));
    const S = s as unknown as { goDown(u: AirSim['units'][number], box: number | null): boolean; strafing: Map<number, { over: boolean }> };
    const raid = s.units.filter((u) => u.side === 'att' && u.air === 'ftr');
    const downs: { id: number; contacts: number }[] = [];
    const go = S.goDown.bind(s);
    S.goDown = (u, box) => { downs.push({ id: u.id, contacts: u.contacts.length }); return go(u, box); };
    // The case is real: the last bomber leaves the board while the raid's escorts are engaged.
    let engagedAsBombersGo = 0, gone = false;
    fight(s, (sim) => {
      if (gone) return;
      if (sim.units.some((b) => b.side === 'att' && b.air === 'bmr' && b.n > 0 && b.job.do !== 'home')) return;
      gone = true;
      engagedAsBombersGo = raid.filter((u) => u.contacts.length > 0).length;
    });
    expect(gone).toBe(true);
    expect(engagedAsBombersGo).toBeGreaterThan(0);
    expect(downs.length).toBeGreaterThan(0);
    for (const x of downs) expect(x.contacts).toBe(0);
    // Each ends the raid with a job: on its way home, or down on the troops.
    for (const u of raid) expect(['home', 'strafe']).toContain(u.job.do);
  });
});

// The UI director, 25 November 2026 21:22Z, g: "A name in its own language is bare ("Groupement de Chasse 3", "JG 26");
// an English one takes "the", "The" at the head of a sentence. That is theGroupName (arsenal.ts:236). The lane's report
// lines take it: A6, A5 ... Groupement de Chasse 3 has no fight left to strafe with and turns for home." A6 is said by the
// battle as it answers the order, so it is printed here from a fought battle: round five's edge raid (309 German
// fighters), Groupement de Chasse 3 sent ahead to hunt so that it meets 2. Jagddivision alone and reads 0, then ordered
// down; once under its own name, once renamed in English (setup: the group's name only).
describe('A6 names its group by the lane\'s one rule (the UI director, 21:22Z, g)', () => {
  const GC3 = 10;
  function a6(name: string | null): string[] {
    const g = startOpen(createGame(7, 'FRA'));
    setWar(g, 'FRA', 'GER');
    g.airFleets.find((f) => f.id === 36)!.base = g.regions.find((r) => r.name === 'Verdun')!.id;
    g.airFleets.find((f) => f.id === 9)!.base = g.regions.find((r) => r.name === 'Namur')!.id;
    const d = g.airFleets.find((f) => f.id === 3)!;
    d.ftr = 309; d.est.ftr = Math.max(d.est.ftr, 309);
    if (name) g.airFleets.find((f) => f.id === GC3)!.name = name;
    const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
    const s = airBattle(ticketOf(g, target, [38, GC3, 36, 9], 'FRA')) as AirSim;
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const S = s as any;
    const unit = s.units.find((u) => u.side === 'att' && u.corps === GC3)!;
    if (name) expect(unit.name).toBe(name);
    s.ai.att = true; s.ai.def = true;
    expect(s.command({ do: 'air:hunt', units: [unit.id] }, 'att')).toBe(true);
    let guard = 0;
    while (!s.over && !S.met.get(unit.id)?.size && guard++ < 40_000) s.step();
    expect(S.carries(unit)).toBe(0);
    expect(s.command({ do: 'air:strafe', units: [unit.id] }, 'att')).toBe(true);
    return s.history().filter((e) => e.what === 'air:no-strafe').map((e) => e.text ?? '');
  }
  it('a name in its own language bare, an English one with "The" at the head of the sentence', () => {
    expect(a6(null)).toEqual(['Groupement de Chasse 3 has no fight left to strafe with and turns for home.']);
    expect(a6('2nd Fighter Group')).toEqual(['The 2nd Fighter Group has no fight left to strafe with and turns for home.']);
  });
});

// The UI director, 25 November 2026 21:22Z, j and k, and 21:28Z (k after the fuel rulings, and the new case): "{Group}
// went down to strafe, but was caught by enemy fighters before it could do any harm."; "{Group} was too short of fuel to
// go down and turned for home." (a group with the reach that bingo turned for home before it went down); "{Group} went
// down to strafe, but ran short of fuel and turned for home before it could do any harm." (one that went down and was
// turned for home by bingo before it finished a pass); two "{A} and {B} ...", three or more "{A}, {B} and {C} ...". "A
// group that went down takes the line of what ended its pass, as the battle knows it; the A5 line only where neither fuel
// nor a jump ended it." Each printed from a fought battle, the Saar with all three French fighter groups at Lorraine and
// the bombers, the computer on both sides, booked back through fly():
// - too short of fuel to go down: setup, the named groups' board fuel set to 450 s on the ticket (as round seven's
//   unfought fuel test sets it), so bingo comes while they still escort the bombers;
// - ran short after going down: setup, the board fuel set to 1,000 s (or 980 s), so bingo comes on the way down or just
//   as the first pass begins;
// - caught: no setup but the defence (2. Jagddivision at 700, as the edge raids set it) and the player's order at the
//   start sending the named groups down at once, before the air fight; on seeds 2 to 5 the German fighters catch them.
describe('what ended a group\'s strafing, as the battle knows it (the UI director, 21:22Z j and k, 21:28Z)', () => {
  const GC1 = 9, GC3 = 10, GC2 = 36;
  const NAMES: Record<number, string> = { [GC1]: 'Groupement de Chasse 1', [GC2]: 'Groupement de Chasse 2', [GC3]: 'Groupement de Chasse 3' };
  function flown(k: number, opts: { def?: number; fuel?: number; order?: boolean; who: number[] }) {
    const g = startOpen(createGame(7, 'FRA'));
    setWar(g, 'FRA', 'GER');
    for (const id of [GC1, GC2]) g.airFleets.find((f) => f.id === id)!.base = g.regions.find((x) => x.name === 'Lorraine')!.id;
    if (opts.def !== undefined) { const d = g.airFleets.find((f) => f.id === 3)!; d.ftr = opts.def; d.est.ftr = Math.max(d.est.ftr, opts.def); }
    g.seed = (7 + 7919 * k) >>> 0;
    g.flags.airBattles = k;
    const target = raidTargets(g, 'ground', 'FRA').find((x) => g.regions[x.id].name === 'Saar')!.id;
    const ids = [38, GC1, GC2, GC3];
    const t = ticketOf(g, target, ids, 'FRA');
    if (opts.fuel !== undefined) for (const id of opts.who) t.place.starts[id].fuel = opts.fuel;
    const s = airBattle(t) as AirSim;
    const S = s as unknown as { strafing: Map<number, { done: number }> };
    const mine = s.units.filter((u) => u.side === 'att' && opts.who.includes(u.corps));
    s.ai.att = true; s.ai.def = true;
    if (opts.order) expect(s.command({ do: 'air:strafe', units: mine.map((u) => u.id) }, 'att')).toBe(true);
    let guard = 0;
    while (!s.over && guard++ < 20_000) s.step();
    expect(s.over).toBeTruthy();
    const o = s.outcome();
    const rep = launchRaid(g, 'ground', target, ids, 'FRA', foughtAir(o))!;
    return { g, s, S, o, rep, mine, line: strafedLine(rep, 'FRA') };
  }
  const bingo = (s: AirSim, u: AirSim['units'][number]) => s.history().find((e) => e.what === 'air:bingo' && e.unit === u.id);
  const firstStrafe = (s: AirSim, u: AirSim['units'][number]) => s.history().find((e) => e.what === 'air:strafe' && e.unit === u.id);

  it('k: a group with the reach that bingo turns for home before its go-down; one, two, three', () => {
    const one = flown(0, { fuel: 450, who: [GC3] });
    expect(one.o.strafeEnd).toEqual({ [GC3]: 'short' });
    expect(one.S.strafing.get(one.mine[0].id)).toBeUndefined();
    // The battle's own bingo line stands as it is.
    expect(bingo(one.s, one.mine[0])!.text).toMatch(/is short of fuel and turns for home\.$/);
    expect(one.rep.strafeEnd).toEqual({ short: [NAMES[GC3]] });
    expect(one.line).toBe('Groupement de Chasse 1 and Groupement de Chasse 2 went down and strafed. Groupement de Chasse 3 was too short of fuel to go down and turned for home.');
    // Kept in the save.
    expect(loadGame(saveGame(one.g))!.airReports.at(-1)!.strafeEnd).toEqual(one.rep.strafeEnd);
    const two = flown(0, { fuel: 450, who: [GC1, GC3] });
    // (On this seed Groupement de Chasse 2 does not go down either: nobody went down, so the line for nobody leads.)
    expect(two.o.strafers).toEqual([]);
    expect(two.line).toBe('No fighter group went down to strafe. Groupement de Chasse 1 and Groupement de Chasse 3 were too short of fuel to go down and turned for home.');
    // All three: none went down, so the line for nobody, and then theirs.
    const three = flown(0, { fuel: 450, who: [GC1, GC2, GC3] });
    expect(three.line).toBe('No fighter group went down to strafe. Groupement de Chasse 1, Groupement de Chasse 2 and Groupement de Chasse 3 were too short of fuel to go down and turned for home.');
  });

  it('the new case: a group that went down and that bingo turned for home before it could do any harm; one, two, three', () => {
    const one = flown(0, { fuel: 1000, who: [GC3] });
    const u = one.mine[0];
    expect(one.o.strafeEnd).toEqual({ [GC3]: 'fuel' });
    // It went down (began its first pass), then bingo: no pass made, nothing booked on the troops.
    expect(firstStrafe(one.s, u)!.t).toBeLessThanOrEqual(bingo(one.s, u)!.t);
    expect(one.S.strafing.get(u.id)!.done).toBe(0);
    expect(one.o.strafedBy?.[GC3] ?? 0).toBe(0);
    expect(bingo(one.s, u)!.text).toMatch(/is short of fuel and turns for home\.$/);
    expect(one.rep.strafeEnd).toEqual({ fuel: [NAMES[GC3]] });
    expect(one.line).toBe('Groupement de Chasse 1 and Groupement de Chasse 2 went down and strafed. Groupement de Chasse 3 went down to strafe, but ran short of fuel and turned for home before it could do any harm.');
    const two = flown(0, { fuel: 1000, who: [GC1, GC3] });
    expect(two.line).toBe('Groupement de Chasse 2 went down and strafed. Groupement de Chasse 1 and Groupement de Chasse 3 went down to strafe, but ran short of fuel and turned for home before they could do any harm.');
    const three = flown(0, { fuel: 1000, who: [GC1, GC2, GC3] });
    expect(three.line).toBe('Groupement de Chasse 1, Groupement de Chasse 2 and Groupement de Chasse 3 went down to strafe, but ran short of fuel and turned for home before they could do any harm.');
    // A group whose bingo came after a fighter's harm or more strafed: its line is the strafers' (Wilno's divisions,
    // the British raid: the battle knows their bingo, the report says they strafed).
    const late = flown(0, { fuel: 1100, who: [GC3] });
    expect(late.o.strafeEnd).toEqual({ [GC3]: 'fuel' });
    expect(late.o.strafedBy![GC3]).toBeGreaterThanOrEqual(1);
    expect('strafeEnd' in late.rep).toBe(false);
    expect(late.line).toBe('Groupement de Chasse 1, Groupement de Chasse 2 and Groupement de Chasse 3 went down and strafed.');
  });

  it('j: a group caught by enemy fighters on its way down or its passes before it could do any harm; one, two, three', () => {
    const one = flown(2, { def: 700, order: true, who: [GC3] });
    expect(one.o.strafeEnd).toEqual({ [GC3]: 'caught' });
    expect(one.o.strafedBy?.[GC3] ?? 0).toBe(0);
    expect(one.rep.strafeEnd).toEqual({ caught: [NAMES[GC3]] });
    expect(one.line).toBe('Groupement de Chasse 3 went down to strafe, but was caught by enemy fighters before it could do any harm.');
    const two = flown(2, { def: 700, order: true, who: [GC1, GC3] });
    expect(two.line).toBe('Groupement de Chasse 1 and Groupement de Chasse 3 went down to strafe, but were caught by enemy fighters before they could do any harm.');
    const three = flown(2, { def: 700, order: true, who: [GC1, GC2, GC3] });
    expect(three.line).toBe('Groupement de Chasse 1, Groupement de Chasse 2 and Groupement de Chasse 3 went down to strafe, but were caught by enemy fighters before they could do any harm.');
    // Beside groups that strafed, the strafers first: seed 4, Groupement de Chasse 3 caught, the other two not.
    const mixed = flown(4, { def: 700, order: true, who: [GC1, GC2, GC3] });
    expect(mixed.line).toBe('Groupement de Chasse 1 and Groupement de Chasse 2 went down and strafed. Groupement de Chasse 3 went down to strafe, but was caught by enemy fighters before it could do any harm.');
    // The defender reads none of it, as before.
    expect(strafedLine(mixed.rep, 'GER')).toBe('');
  });

  it('as built (no setup), the Saar: nothing ended short, no field in the outcome or the report', () => {
    const f = flown(0, { who: [] });
    expect('strafeEnd' in f.o).toBe(false);
    expect('strafeEnd' in f.rep).toBe(false);
    expect(saveGame(f.g).includes('"strafeEnd"')).toBe(false);
  });
});
