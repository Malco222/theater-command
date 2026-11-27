// The fitted k lands (release 2, Units decide; the design director 24 Nov 2026 16:09Z and 16:13Z, on
// air-battles/dogfight/fit-gc-1124.md): with planes, pilots' hours and warning priced on the card, the field's fighter
// exchange lands within 1.2x of the card's in every band of the card's exchange, over every sweep and escorted raid
// the staffs would fly from four grand saves, eight seeds each. The field carries the card's type edge through its
// ticket, so the two read one fight.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets } from '../src/campaign/airwar';
import { assessAir, forecastAir } from '../src/campaign/air';
import { FIGHTERS, lawK } from '../src/campaign/fighters';
import { loadGame } from '../src/campaign/save';
import type { Game } from '../src/campaign/types';
import { AirSim } from '../src/battle/air/sim';
import { airTicket } from '../src/battle/air/ticket';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
/** Bands of the card's exchange (the defenders' fighters lost for each of the raid's). */
const BANDS = [0, 0.5, 1, 2, Infinity];
const SEEDS = 8;

describe('k as fitted, card against field by band', () => {
  it('the field\'s exchange is within 1.2x of the card\'s in every band', () => {
    expect(FIGHTERS.law.k).toBeGreaterThan(0);
    const acc = BANDS.slice(1).map(() => ({ n: 0, cf: 0, ca: 0, fd: 0, fa: 0 }));
    let edged = 0;
    for (const [nat, from, seed] of [['GER', 33, 11], ['UK', 29, 11], ['SOV', 29, 7], ['GER', 29, 7]] as const) {
      TC.start({ view: 'grand', human: nat, from, seed });
      const text = TC.save().text;
      const mk = (): Game => { const g = loadGame(text)!; g.flags.open = 1; g.flags.unitsDecide = 1; return g; };
      const g0 = mk();
      expect(lawK(g0)).toBe(FIGHTERS.law.k);
      for (const kind of ['superiority', 'bomb'] as const) {
        for (const t of raidTargets(g0, kind, nat)) {
          const plan = planRaid(g0, kind, t.id, undefined, nat);
          if (!plan.picked?.length) continue;
          const ids = [...plan.picked, ...(kind === 'bomb' ? plan.seats.filter((s) => s.kind === 'ftr' && !plan.picked!.includes(s.id)).slice(0, 2).map((s) => s.id) : [])];
          const g = mk();
          const a = assessAir(g, g.airFleets.filter((f) => ids.includes(f.id)), kind, t.id);
          if (a.why || !a.height || a.E < 5 || a.D < 5) continue;
          if (a.typeEdge) edged++;
          const fc = forecastAir(g, a, kind, t.id);
          const r = airTicket(mk(), kind, t.id, ids, nat);
          if (!r.ticket) continue;
          let A = 0, D = 0;
          for (let s = 1; s <= SEEDS; s++) {
            const tk = JSON.parse(JSON.stringify(r.ticket));
            tk.seed = s * 7919;
            const sim = new AirSim(tk, {});
            while (!sim.over) sim.step();
            for (const x of sim.outcome().groups) { if (x.side === 'att') A += x.lostFtr; else D += x.lostFtr; }
          }
          const X = fc.foeLoss / fc.ftrLoss, b = BANDS.findIndex((v, i) => X >= v && X < BANDS[i + 1]);
          const c = acc[b];
          c.n++; c.cf += fc.foeLoss; c.ca += fc.ftrLoss; c.fd += D / SEEDS; c.fa += A / SEEDS;
        }
      }
    }
    // The types gave an edge somewhere, so the law was read.
    expect(edged).toBeGreaterThan(0);
    let bands = 0;
    for (const c of acc) {
      if (c.n < 5) continue;
      bands++;
      const ratio = (c.fd / c.fa) / (c.cf / c.ca);
      expect(Math.abs(Math.log(ratio))).toBeLessThan(Math.log(1.2));
    }
    expect(bands).toBeGreaterThanOrEqual(3);
  }, 600_000);
});
