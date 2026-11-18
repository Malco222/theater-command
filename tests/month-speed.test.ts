// Month's end made quicker without changing a thing: the supply picture asks of every nation with ships which seas are
// closed to it, and a picture of the seas worked out once (SeaPicture) answers as closedTo does. Held together here on
// grand campaigns stirred up at random: fleets at sea everywhere, wars declared, the sky won over coasts.
import { describe, expect, it } from 'vitest';
import { setWar, createGame } from '../src/campaign/game';
import { SeaPicture, closedTo, openSeas } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';

/** A small steady stream of numbers, so the stirring is the same every run. */
const stream = (seed: number) => () => ((seed = (Math.imul(seed, 1103515245) + 12345) >>> 0) / 2 ** 32);

function stirred(seed: number): Game {
  const g = startOpen(createGame(seed, null));
  const rnd = stream(seed);
  const nats = Object.keys(g.nations).filter((n) => g.nations[n].alive);
  // Wars between nations picked at random, some across the blocs and some inside them.
  for (let k = 0; k < 25; k++) {
    const a = nats[Math.floor(rnd() * nats.length)], b = nats[Math.floor(rnd() * nats.length)];
    if (a !== b) setWar(g, a, b);
  }
  // Every fleet put to sea somewhere, a few of them made very strong.
  for (const f of g.fleets) {
    f.zone = Math.floor(rnd() * g.seas.length);
    f.stance = rnd() < 0.8 ? 'patrol' : 'port';
    if (rnd() < 0.2) f.ships = { ...f.ships, bb: f.ships.bb + 30 };
  }
  // The sky won over coasts, lately and long ago.
  for (const z of g.seas) for (const id of z.coast) {
    if (rnd() < 0.15) g.sky[id] = { nat: nats[Math.floor(rnd() * nats.length)], turn: g.turn - Math.floor(rnd() * 4) };
  }
  // Air forces of every size.
  for (const n of nats) g.nations[n].air = Math.floor(rnd() * 400);
  return g;
}

describe('month speed: the picture of the seas answers as closedTo does', () => {
  for (const seed of [3, 11, 29]) {
    it(`every sea, every nation, seed ${seed}`, () => {
      const g = stirred(seed);
      const pic = new SeaPicture(g);
      let closed = 0;
      for (const nat of Object.keys(g.nations)) {
        if (!g.nations[nat].alive) continue;
        for (const z of g.seas) {
          const want = closedTo(g, z.id, nat);
          expect(pic.closed(z.id, nat)).toBe(want);
          if (want) closed++;
        }
        expect([...openSeas(g, nat, pic)]).toEqual([...openSeas(g, nat)]);
      }
      // The stirring closes seas to somebody: the check is not of open water only.
      expect(closed).toBeGreaterThan(0);
    });
  }
});
