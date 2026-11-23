// Release 1.5: ALL FALL BACK (Brian, 23 November 14:47Z: "there should be a fall back button that has all your units
// fall back and once they get to a certain part of the map behind them where they started the battle ends and the units
// retreat"). The land rules' { do: 'land:fallBack' } sends every block of the side back toward its own edge; each is out
// once behind the zone its side drew up in, and the last out ends the battle as that side's withdrawal. The campaign
// books it once, through the attack order, by b474 with the bar taken as the highest it stood from the order until the
// last block was out (the design director, 14:54Z). Battles nobody falls back in are fought and booked as before.
import { describe, expect, it } from 'vitest';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { liveTicket } from '../src/battle/land/live';
import { loadGame, saveGame } from '../src/campaign/save';
import { monthCard, monthMark } from '../src/campaign/monthcard';
import { orderAttack } from '../src/campaign/turn';
import type { FieldResult } from '../src/battle/core/types';

const TC = (globalThis as unknown as { TC: Record<string, (...a: unknown[]) => any> }).TC;
type Look = { ticket: Record<string, any> | null; live?: Record<string, any>; why?: string } | null;

/** Germany, September 1939 (seed 1): XIX Corps (id 1) onto Pomorze (39), the Mac's own round trip. */
function fresh() {
  TC.start({ human: 'GER', from: 1, view: 'grand', seed: 1 });
}

/** Fights the card's battle with him on the attack; 'back' orders that side back that many seconds after contact. */
function fight(look: NonNullable<Look>, back?: { side: 'att' | 'def'; after: number }, opts: Record<string, unknown> = {}) {
  const L = liveTicket(structuredClone(look.ticket) as never, { ...(look.live ?? {}), me: 'att', ...opts } as never);
  let ordered: { t: number; bar: number; ok: boolean } | null = null;
  for (let guard = 0; !L.sim.over && guard < 100000; guard++) {
    // (after < 0: as soon as the clock runs, before the armies meet)
    if (back && !ordered && (back.after < 0 ? L.sim.t > 0 : L.sim.contactAt !== null && L.sim.t >= L.sim.contactAt + back.after)) {
      const bar = L.sim.breaking(back.side);
      const ok = back.side === 'att' ? L.order({ do: 'land:fallBack' } as never) : L.sim.command({ do: 'land:fallBack' } as never, 'def');
      ordered = { t: L.sim.t, bar, ok };
    }
    L.tick(0.5, 1000);
  }
  expect(L.sim.over).toBeTruthy();
  return { L, over: L.sim.over as FieldResult, ordered };
}

describe('release 1.5: ALL FALL BACK', () => {
  it('his whole army goes back behind the ground it started from, and the battle ends as his withdrawal', () => {
    fresh();
    const look = TC.battleLook([1], 39) as Look;
    const { L, over, ordered } = fight(look!, { side: 'att', after: 30 });
    expect(ordered?.ok).toBe(true);
    expect(over.how).toBe('withdrew');
    expect(over.fellBack).toBe('att');
    expect(over.winner).toBe('def');
    expect(over.R).toBeLessThan(1.2);
    expect(over.lines[0]).toMatch(/fell back behind the ground it started from/);
    // (every block of his is out or lost: none still standing on the field)
    expect(L.sim.units.filter((u) => u.side === 'att' && u.state !== 'withdrawn' && u.state !== 'gone' && u.state !== 'routing' && u.kind !== 'fort')).toEqual([]);
    // (out behind the zone it drew up in, not only at the field's edge)
    const back = L.sim.field.zones.att[1];
    for (const u of L.sim.units.filter((x) => x.side === 'att' && x.state === 'withdrawn')) expect(u.y).toBeLessThanOrEqual(back + 1e-6);
    // (it pays by the highest its bar stood from the order on: never less than it stood when he gave it)
    expect(over.pulled?.side).toBe('att');
    expect(over.pulled!.bar).toBeGreaterThanOrEqual(Math.round(ordered!.bar * 1000) / 1000 - 1e-9);
  }, 180_000);

  it('asks once: no new orders after it, and not before the battle has begun', () => {
    fresh();
    const look = TC.battleLook([1], 39) as Look;
    const placing = liveTicket(structuredClone(look!.ticket) as never, { ...(look!.live ?? {}), me: 'att', setup: true } as never);
    if (placing.sim.setup !== null) {
      expect(placing.fallBackWhy()).toBe('The battle has not begun.');
      expect(placing.order({ do: 'land:fallBack' } as never)).toBe(false);
    }
    const L = liveTicket(structuredClone(look!.ticket) as never, { ...(look!.live ?? {}), me: 'att' } as never);
    for (let guard = 0; L.sim.contactAt === null && !L.sim.over && guard < 100000; guard++) L.tick(0.5, 1000);
    expect(L.fallBackWhy()).toBeNull();
    expect(L.order({ do: 'land:fallBack' } as never)).toBe(true);
    expect(L.fallBackWhy()).toMatch(/already falling back/);
    expect(L.order({ do: 'land:fallBack' } as never)).toBe(false);
    const mine = L.sim.units.find((u) => u.side === 'att' && u.state !== 'gone' && u.kind !== 'fort')!;
    expect(L.order({ do: 'move', units: [mine.id], x: mine.x, y: mine.y + 300 } as never)).toBe(false);
    expect(L.order({ do: 'attack', units: [mine.id], target: L.sim.units.find((u) => u.side === 'def')!.id } as never)).toBe(false);
  }, 120_000);

  it('the campaign books it once: the attack is called off, the army stays home and keeps what it did not lose', () => {
    fresh();
    const look = TC.battleLook([1], 39) as Look;
    const g0 = loadGame(TC.save().text)!;
    const mark = monthMark(g0);
    const home = g0.corps.find((c) => c.id === 1)!.reg, men0 = g0.corps.find((c) => c.id === 1)!.str;
    const { over } = fight(look!, { side: 'att', after: 30 });
    const seed = look!.ticket!.seed as number;
    const out = TC.battleSettleLand([1], 39, { over }, seed) as { report?: { level?: string; taken?: boolean; fellBack?: string; attLost: number } };
    expect(out?.report?.level).toMatch(/defeat/i);
    expect(out?.report?.taken).toBe(false);
    expect(out?.report?.fellBack).toBe('att');
    // (asked again: the same answer, nothing booked twice)
    const saved = TC.save().text;
    expect(TC.battleSettleLand([1], 39, { over }, seed)).toEqual(out);
    expect(TC.save().text).toBe(saved);
    const g = loadGame(saved)!;
    const c = g.corps.find((x) => x.id === 1)!;
    expect(c.reg).toBe(home);
    expect(g.regions[39].ctrl).not.toBe('GER');
    // (never less than the field cost it: b474's floor)
    expect(1 - c.str / men0).toBeGreaterThanOrEqual(over.lost.att * 0.98 - 1e-6);
    // (the month's card, at the month's end, says it in words)
    TC.endMonth();
    const row = monthCard(loadGame(TC.save().text)!, mark).rows.find((r) => r.region === 39);
    expect(row?.line).toBe('Your army fell back to fight another day.');
    expect(row?.head).toBe('The attack on Pomorze is called off');
  }, 180_000);

  it('saves and loads with it, and the month closes', () => {
    fresh();
    const look = TC.battleLook([1], 39) as Look;
    const { over } = fight(look!, { side: 'att', after: 30 });
    TC.battleSettleLand([1], 39, { over }, look!.ticket!.seed);
    const text = TC.save().text;
    expect(TC.load(text)).toBeTruthy();
    const g = loadGame(TC.save().text)!;
    expect(g.reports[g.reports.length - 1].fellBack).toBe('att');
    const month = g.turn;
    const done = TC.endMonth() as { view?: { turn?: number } };
    expect(done).toBeTruthy();
    expect(loadGame(TC.save().text)!.turn).toBe(month + 1);
  }, 180_000);

  it('a defence that falls back gives up the province: to a friendly neighbour, or taken where it has no way out', () => {
    // (the rules let either side fall back; the staff never orders it, so the defence's is ordered here by hand)
    for (const boxed of [false, true]) {
      fresh();
      if (boxed) {
        // Every neighbour of Pomorze but the attack's own made German: the Polish army there has no friendly ground to go to.
        // (their troops there sent far off, to a Polish province away from Pomorze)
        const g = loadGame(TC.save().text)!, nb = g.regions[39].nb;
        const far = g.regions.find((r) => r.ctrl === 'POL' && r.id !== 39 && !nb.includes(r.id))!;
        for (const n of nb) {
          g.regions[n].ctrl = 'GER';
          for (const c of g.corps) if (c.reg === n && c.nat !== 'GER') c.reg = far.id;
        }
        TC.load(saveGame(g));
      }
      const look = TC.battleLook([1], 39) as Look;
      expect(look?.ticket).toBeTruthy();
      const before = loadGame(TC.save().text)!;
      const defs = before.corps.filter((c) => c.reg === 39 && c.nat !== 'GER').map((c) => c.id);
      // (ordered back as the clock starts: a defence that waits for the attack to land is broken on the way, a rout)
      const { over, ordered } = fight(look!, { side: 'def', after: -1 });
      expect(ordered?.ok).toBe(true);
      expect(over.how).toBe('withdrew');
      expect(over.fellBack).toBe('def');
      expect(over.winner).toBe('att');
      const out = TC.battleSettleLand([1], 39, { over }, look!.ticket!.seed) as { report?: { taken?: boolean; fellBack?: string; defenders: { id: number; fate?: string }[] } };
      expect(out?.report?.taken).toBe(true);
      expect(out?.report?.fellBack).toBe('def');
      const g = loadGame(TC.save().text)!;
      expect(g.regions[39].ctrl).toBe('GER');
      const left = defs.map((id) => g.corps.find((c) => c.id === id)).filter((c) => !!c);
      if (boxed) expect(left).toEqual([]);
      else for (const c of left) expect(g.regions[c!.reg].ctrl).not.toBe('GER');
    }
  }, 300_000);

  it('a battle nobody falls back in is fought exactly as before', () => {
    fresh();
    const look = TC.battleLook([1], 39) as Look;
    const { over } = fight(look!);
    expect(over.fellBack).toBeUndefined();
    // (pinned on 2dcc018b, before ALL FALL BACK: the same seed, the same day to the second)
    expect({ how: over.how, winner: over.winner, seconds: over.seconds, R: over.R, lost: over.lost }).toEqual(PINNED);
  }, 180_000);

  it('a bill the field laid on one corps is never booked past the side\'s whole bill (b474 with the field\'s split)', () => {
    // Gazala, 1942 (Germany, start 33): an army that fell back half a minute after contact, its field loss all on the 5th
    // Light Division (the other two not yet engaged), its bar at 0.53. Its whole bill is the field's 6.9% (the rules' bill
    // times the bar squared is less); before the mend the 5th Light alone was booked its cap of 60%, 17.5% of the army.
    TC.start({ human: 'GER', from: 33, view: 'grand', seed: 1 });
    const g = loadGame(TC.save().text)!;
    const cs = [240, 264, 306].map((id) => g.corps.find((c) => c.id === id)!);
    expect(cs.every((c) => !!c)).toBe(true);
    const field = { corps: { 240: 0.175, 264: 0, 306: 0 }, att: 0.069, def: 0.0134, whole: true, pulled: { side: 'att' as const, bar: 0.53 } };
    const rep = orderAttack(g, cs, g.regions[342], 0.737, field)!;
    const men = rep.attackers.reduce((a, u) => a + u.men, 0);
    expect(rep.attLost / men).toBeGreaterThanOrEqual(0.069 * 0.98);
    expect(rep.attLost / men).toBeLessThan(0.069 * 1.05);
    // (still on the corps the field hit: the others lose nothing)
    expect(rep.attackers.filter((u) => u.lost > 0).length).toBe(1);
  }, 60_000);
});

const PINNED = { how: 'rout', winner: 'att', seconds: 139, R: 3.327, lost: { att: 0.00903942382543982, def: 0.22122056672925716 } };
