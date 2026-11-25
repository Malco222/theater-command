// Supply keeps each country's friends among the holders from one call to the next while friendsKeyOf (game.ts) is
// the same, and its sea lanes' answers about seas that have not changed: it names everything allied() and atWar()
// read of the war. These tests stop a later hand that teaches either something new without teaching the key. Every
// read the two make of the game is recorded on the whole-world chart, where the far side's client ties are read too,
// and each must be one the key also reads; and each thing the key names, changed, changes the key.
import { describe, expect, it, vi } from 'vitest';

vi.hoisted(() => { (globalThis as { TC_FAR?: unknown }).TC_FAR = 1; });

import { CLIENTS } from '../src/campaign/far';
import { allied, atWar, createGame, friendsKeyOf, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { Game } from '../src/campaign/types';
import { farOn } from '../src/campaign/world';

/** The game seen through a recorder: every property read, as a path from the game ("g.nations.GER.align"). */
function recorded(g: Game): { seen: Game; reads: Set<string> } {
  const reads = new Set<string>();
  const wrap = (obj: object, path: string): object =>
    new Proxy(obj, {
      get(t, k, r) {
        const v = Reflect.get(t, k, r);
        if (typeof k === 'symbol') return v;
        const at = `${path}.${k}`;
        reads.add(at);
        return v !== null && typeof v === 'object' ? wrap(v, at) : v;
      },
    });
  return { seen: wrap(g, 'g') as Game, reads };
}

/** A read with the country named as '*', and anything read of a list of enemies as the list itself. */
const kindOf = (path: string): string => path.replace(/^g\.nations\.[^.]*/, 'g.nations.*').replace(/^(g\.nations\.\*\.atWar)\..*$/, '$1');

const readsOf = (g: Game, ask: (seen: Game) => void): Set<string> => {
  const { seen, reads } = recorded(g);
  ask(seen);
  return new Set([...reads].map(kindOf));
};

const wide = (seed: number): Game => startOpen(createGame(seed, null));

describe("supply's kept friends and lanes: friendsKeyOf names everything allied() and atWar() read", () => {
  it('on the whole-world chart, every read allied() and atWar() make, in peace and in war, is one the key makes too', () => {
    const g = wide(11);
    expect(farOn(g)).toBe(true);
    expect(Object.keys(CLIENTS).length).toBeGreaterThan(0); // (the far side's ties are asked: not an empty test)
    const ids = [...Object.keys(g.nations), '', 'nobody'];
    const real = Object.keys(g.nations);
    const asks = (seen: Game): void => {
      for (const a of ids) for (const b of ids) allied(seen, a, b);
      for (const a of real) for (const b of ids) atWar(seen, a, b);
    };
    const key = readsOf(g, (seen) => friendsKeyOf(seen));
    const check = (): void => {
      const missing = [...readsOf(g, asks)].filter((k) => !key.has(k));
      expect(missing, `allied() or atWar() reads what friendsKeyOf does not: ${missing.join(', ')}`).toEqual([]);
    };
    check();
    // A client at war with its patron's enemies and with its patron, and a patron's enemy fallen: tieHolds' other turns.
    for (const [client, row] of Object.entries(CLIENTS)) {
      if (!g.nations[client] || !g.nations[row.patron]) continue;
      for (const e of row.wars ?? []) if (g.nations[e] && e !== row.patron) setWar(g, row.patron, e);
    }
    check();
    const [client, row] = Object.entries(CLIENTS).find(([c, r]) => g.nations[c] && g.nations[r.patron])!;
    setWar(g, client, row.patron);
    check();
  });

  it('each thing the key names, changed, changes the key', () => {
    const g = wide(12);
    const id = Object.keys(g.nations).find((n) => g.nations[n].alive)!;
    const n = g.nations[id];
    const other = Object.keys(g.nations).find((x) => x !== id && !n.atWar.includes(x))!;
    const was = friendsKeyOf(g);
    const moves = (change: () => void, undo: () => void): boolean => { change(); const now = friendsKeyOf(g); undo(); return now !== was; };
    const align = n.align, league = n.league, alive = n.alive;
    expect(moves(() => { n.align = n.align === 'axis' ? 'allies' : 'axis'; }, () => { n.align = align; })).toBe(true);
    expect(moves(() => { n.league = !league; }, () => { n.league = league; })).toBe(true);
    expect(moves(() => { n.alive = !alive; }, () => { n.alive = alive; })).toBe(true);
    expect(moves(() => { n.atWar.push(other); }, () => { n.atWar.pop(); })).toBe(true);
    expect(friendsKeyOf(g)).toBe(was);
  });
});
