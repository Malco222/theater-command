// Infantry with tanks, the man under the tracks (release 2, LandBattle.escort). The design director, 27 November 2026,
// 05:03Z: "A crush is drawn only where the crushing block IS the booked striker. Otherwise the loss is drawn the
// ordinary way, to the booked block." The rules book each loss to the enemy block that struck (Drill.strikerOf: the
// block owed most for the harm, else the one faced). Before the mend the drill drew a man lying under ANY enemy tank on
// the move as run over by that tank's block, so the drawing and the battle's own books named two different blocks
// (the tank and soldier AI thread's find: Prokhorovka, seed 14, the defender seated: block 4 drawn, block 10 booked).
//
// What is read here: every round the drill draws as a crush (a 'track' round), and beside it the block the rules had
// booked that loss to at that moment. They must be the same block in every crush. Three battles: Italian foot in the
// open in front of Shermans (crushes still happen: the look is not lost), Prokhorovka seed 12 (crushes that were right
// before the mend and stay), and Prokhorovka seed 14 (every crush there named the wrong block before the mend).
import { describe, expect, it } from 'vitest';
import { livePiece, liveTicket, type LiveBattle, type LiveOpts } from '../src/battle/land/live';
import { PIECES, ticketOf } from '../src/battle/land/pieces';
import type { Round } from '../src/battle/land/drill';

const UPTO = 300;

interface Block { id: number }
interface Crush { t: number; by: number | undefined; on: number; booked: number | undefined }
interface Inside {
  strikerOf(u: Block): Block | undefined;
  killRound(by: Block | undefined, vq: { u: Block }, v: unknown, p?: unknown, mate?: boolean, crush?: unknown): void;
}

/** Every crush the drill draws in L from here on: the block drawn, the block struck, the block the rules had booked. */
function watch(L: LiveBattle): Crush[] {
  const out: Crush[] = [];
  const d = L.drill as unknown as Inside;
  const booked = new Map<number, number | undefined>();
  const strikerOf = d.strikerOf.bind(d), killRound = d.killRound.bind(d);
  d.strikerOf = (u) => {
    const s = strikerOf(u);
    booked.set(u.id, s?.id);
    return s;
  };
  d.killRound = (by, vq, v, p, mate, crush) => {
    if (crush) out.push({ t: +L.sim.t.toFixed(1), by: by?.id, on: vq.u.id, booked: booked.get(vq.u.id) });
    killRound(by, vq, v, p, mate, crush);
  };
  return out;
}

/** The battle fought to its end or UPTO seconds: the crushes watched, and the 'track' rounds the drill handed on. */
function fight(L: LiveBattle): { crushes: Crush[]; tracks: Round[] } {
  const crushes = watch(L), tracks: Round[] = [];
  while (!L.sim.over && L.sim.t < UPTO - 1e-6) {
    const d = L.to(Math.min(UPTO, L.sim.t + 1), 100000);
    for (const r of d.rounds as Round[]) if (r.w === 'track') tracks.push(r);
  }
  return { crushes, tracks };
}

/** Normandy, seed 11, with the defence Italian foot standing in the open: no tanks, not dug in, no anti-tank weapon of its own. */
function italianFoot() {
  const t = ticketOf(PIECES.find((x) => x.key === 'normandy-1944')!, 11, 'att');
  return { ...t, def: { ...t.def, nat: 'ITA', forces: t.def.forces.map((f) => ({ ...f, nat: 'ITA', dug: false, kind: f.kind === 'arm' ? 'inf' : f.kind })) } };
}

/** The rows of a played battle that put vehicles and hiding on the field beside escort (seated: odd seeds attack). */
const FIELD = { escort: true, hide: true, patrol: true, phones: true, cutters: true, relief: true, rider: true, mech: true, spg: true, td: true, cars: true, aa: true, light: true };
function prokhorovka(seed: number): LiveBattle {
  return livePiece('prokhorovka-1943', seed, { ...FIELD, me: seed % 2 ? 'att' : 'def' } as LiveOpts);
}

function sameBlock(name: string, crushes: Crush[], tracks: Round[]): void {
  const parted = crushes.filter((c) => c.by === undefined || c.by !== c.booked);
  console.log(`crush = booked striker: ${name}: ${crushes.length} crushes drawn, ${parted.length} by a block the rules had not booked`);
  expect(parted).toEqual([]);
  // Every 'track' round handed to the 3D game is one of the crushes read above, by the same block on the same block.
  expect(tracks.length).toBe(crushes.length);
  tracks.forEach((r, i) => { expect([r.by, r.on]).toEqual([crushes[i].by, crushes[i].on]); });
}

describe('infantry with tanks: a crush is drawn only by the block the rules booked', () => {
  it('Italian foot in the open: men still go under the Shermans, each under the block that struck his', () => {
    const { crushes, tracks } = fight(liveTicket(italianFoot() as never, { me: 'att', escort: true }, 'normandy-1944'));
    expect(crushes.length).toBeGreaterThan(0);
    sameBlock('Italian foot, Normandy 11', crushes, tracks);
  }, 600000);

  it('Prokhorovka, seed 12: the crushes that named the right block before are still drawn', () => {
    const { crushes, tracks } = fight(prokhorovka(12));
    expect(crushes.length).toBeGreaterThan(0);
    sameBlock('Prokhorovka 12', crushes, tracks);
  }, 600000);

  it('Prokhorovka, seed 14: no man is drawn under a tank of a block that did not strike his', () => {
    const { crushes, tracks } = fight(prokhorovka(14));
    sameBlock('Prokhorovka 14', crushes, tracks);
  }, 600000);
});
