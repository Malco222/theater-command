// The recorder's set pieces (tools/air-record, the 2bdb2m air line) lay open water under the stage with its true north,
// as AirGround has required since true north came to the board: the map's north turned as the stage is turned. Before
// the join's fix, record.ts laid the ground with no `north` at all (tools/ is outside tsconfig, so tsc never saw it):
// the recordings of Pearl Harbor and Midway told the 3D game nothing of true north, and it took north as up the board.
import { describe, expect, it } from 'vitest';
import { seaGround } from '../src/battle/air/ticket';
import type { AirGround } from '../src/battle/air/ground';

// The recorder is imported as the module it is (node's file and path modules beside it, which the project's tsc has
// no types for), and it records nothing under vitest.
interface Piece { key: string; kind: 'harbour' | 'fleet'; turn: number }
interface Recorder { PIECES: Piece[]; ticketOf: (P: Piece, seed: number) => { place: { ground: AirGround } } }
const recorder = (): Promise<Recorder> => import(/* @vite-ignore */ ['..', 'tools', 'air-record', 'record'].join('/')) as Promise<Recorder>;

describe("the recorder's set pieces keep their true north (record.ts:272)", () => {
  it("open water on a turned board: the map's north turned as the board is, a unit vector", () => {
    expect(seaGround(0).north).toEqual([0, 1]);
    expect(seaGround(Math.PI).north).toEqual([0, -1]);
    expect(seaGround(Math.PI / 2).north).toEqual([-1, 0]);
    for (const turn of [0.7, 2, -1.3]) {
      const [x, y] = seaGround(turn).north;
      expect(Math.hypot(x, y)).toBeCloseTo(1, 5);
    }
    expect(seaGround(1)).toMatchObject({ land: [], rivers: [], borders: [], target: [], terrain: 'sea', inland: false });
  });

  it("Pearl Harbor and Midway: the recorder's ticket lays the ground with the stage's true north", async () => {
    const R = await recorder();
    expect(R.PIECES.map((P) => P.key)).toEqual(['oahu', 'midway-fleet']);
    for (const P of R.PIECES) {
      const g = R.ticketOf(P, 238).place.ground;
      expect(g.north, P.key).toEqual(seaGround(P.turn).north);
      expect(g).toEqual(seaGround(P.turn));
    }
    // Pearl Harbor's stage is turned half round: true north is down the board, not up it.
    expect(R.ticketOf(R.PIECES[0], 238).place.ground.north).toEqual([0, -1]);
  }, 120_000);
});
