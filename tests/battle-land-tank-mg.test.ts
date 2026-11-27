// A tank's machine guns (release 2; Brian, 22 November 04:10Z: "make sure the machine gun from the gunner inside the
// tank also shoots some times at infantry", and "the top machine gun cant be used unless the tank commander mans the
// gun"). Looks only (ruling 217): the rules take the same men; the drawn battle shows which gun took them.
import { describe, expect, it } from 'vitest';
import { LandBattle, sampleTicket, withTeams } from '../src/battle/land';
import { BOOK } from '../src/battle/land/tactics';

const sig = (b: LandBattle): string => JSON.stringify({ over: b.over, t: b.t, units: b.units.map((u) => [u.id, u.state, u.str, u.mor, u.x, u.y, u.face, u.figures, u.engaged]) });

function fight(att: string, def: string, seed = 5) {
  const b = new LandBattle(withTeams(sampleTicket('plains', 1.3, { seed, nat: { att, def }, style: { att: 'methodical', def: 'bold' } })));
  const d = b.drillOn({ sections: true, platoons: true });
  const out = { mg: 0, kills: 0, g: { coax: 0, hull: 0, roof: 0 } as Record<string, number>, noGun: 0, roofButtoned: 0, cm: [] as { on: number; ok: number; t: number }[], cmBad: 0, shellKillsOnFoot: 0, mgKillsOnFoot: 0 };
  let last = 0;
  for (let k = 0; !b.over && k < 30000; k++) {
    b.step();
    let view: ReturnType<typeof d.view> | null = null;
    for (const r of d.rounds()) {
      if (r.n <= last) continue;
      last = r.n;
      const by = r.by !== undefined ? b.unit(r.by) : undefined, on = b.unit(r.on);
      if (r.cm) out.cm.push({ on: r.on, ok: r.ok!, t: b.t });
      if (by?.kind !== 'arm') continue;
      if (r.r === 'kill' && on && on.kind !== 'arm') { if (r.w === 'mg') out.mgKillsOnFoot++; else out.shellKillsOnFoot++; }
      if (r.w !== 'mg') continue;
      out.mg++;
      if (r.r === 'kill') out.kills++;
      if (!r.g) { out.noGun++; continue; }
      out.g[r.g]++;
      if (r.g === 'roof') {
        view ??= d.view();
        const tank = view.find((x) => x.id === by.id)?.men.find((m) => m.k === r.k);
        if (tank?.b) out.roofButtoned++;
      }
    }
    // A tank whose commander was struck fights buttoned up from then.
    if (out.cm.length && k % 10 === 0) {
      view ??= d.view();
      for (const c of out.cm) { const m = view.find((x) => x.id === c.on)?.men.find((x) => x.k === c.ok && x.d === undefined); if (m && !m.b) out.cmBad++; }
    }
  }
  return { b, out };
}

describe('a tank\'s machine guns', () => {
  it('take a share of the enemy\'s foot she hits, each round naming its gun; the roof gun only with the commander up', () => {
    const { out } = fight('USA', 'GER');
    expect(out.mg).toBeGreaterThan(50);
    expect(out.noGun).toBe(0);
    expect(out.kills).toBeGreaterThan(0);
    expect(out.mgKillsOnFoot).toBeGreaterThan(0);
    expect(out.shellKillsOnFoot).toBeGreaterThan(0);
    expect(out.g.coax).toBeGreaterThan(0);
    expect(out.g.roof).toBeGreaterThan(0);
    expect(out.roofButtoned).toBe(0);
    expect(out.cmBad).toBe(0);
  }, 300_000);

  it('only the armies that carried a roof gun fire one', () => {
    expect(BOOK.tankMg.roof).toEqual(['USA', 'GER']);
    const { out } = fight('SOV', 'ITA', 3);
    expect(out.mg).toBeGreaterThan(0);
    expect(out.g.roof).toBe(0);
  }, 300_000);

  it('change nothing in the fight', () => {
    const t = () => withTeams(sampleTicket('plains', 1.3, { seed: 5, nat: { att: 'USA', def: 'GER' }, style: { att: 'methodical', def: 'bold' } }));
    const plain = new LandBattle(t()), drawn = new LandBattle(t());
    drawn.drillOn({ sections: true, platoons: true });
    for (let k = 0; k < 30000 && !(plain.over && drawn.over); k++) { plain.step(); drawn.step(); }
    expect(sig(drawn)).toBe(sig(plain));
  }, 300_000);
});
