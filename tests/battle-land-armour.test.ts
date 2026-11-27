// Measured armour (release 2; Brian, 22 November 04:03Z to 04:16Z: the named tanks have real advantages from their
// measured performance against their peers, and German tanks are far weaker at the back, so hits there pay and raise the
// chance they explode). The modeller's sheet (data/battle/armour.json) weighs a tank's or a gun's blow on a tank by the
// face it strikes and the range; the computer's tanks go round a tank whose front they cannot get through; and a tank
// knocked out ends out of the fight, burning or blown up, by her armour and the face struck.
import { describe, expect, it } from 'vitest';
import { LAND, LandBattle, sampleTicket, withTeams } from '../src/battle/land';
import { ARMOUR, PAR, armourWeight, fateOf, through, vehicleOf } from '../src/battle/land/armour';
import { turnToDate } from '../src/core/ticket';
import type { FieldTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';
import type { LandUnit } from '../src/battle/land/units';

/** June 1944, as the battle's turn. */
const JUNE44 = 58;

/** A practice field where both sides bring tanks: the defence's own armoured corps beside its infantry and guns. */
function armoured(att: string, def: string, seed: number, turn = JUNE44): FieldTicket<LandPlace> {
  const t = withTeams(sampleTicket('plains', 1.3, { seed, nat: { att, def } }));
  t.turn = turn;
  const f = t.def.forces;
  f[0].share = 0.4; f[1].share = 0.3; f[2].share = 0.1;
  f.push({ ...t.att.forces[2], corps: 14, nat: def, share: 0.2 });
  return withTeams(t);
}

/** An AI-v-AI battle on that field, the tank-on-tank blows tallied by side and face. */
function tally(att: string, def: string, seed: number, measured: boolean) {
  const b = new LandBattle(armoured(att, def, seed));
  b.ai.att = b.ai.def = true;
  b.measured = measured;
  const dmg = { att: 0, def: 0 }, faces = { front: 0, flank: 0, rear: 0 };
  const blow = (b as unknown as { blow: (u: LandUnit, t: LandUnit) => { dmg: number; aspect: 'front' | 'flank' | 'rear' } }).blow.bind(b);
  (b as unknown as { blow: typeof blow }).blow = (u, t) => {
    const o = blow(u, t);
    if (u.kind === 'arm' && t.kind === 'arm') { dmg[u.side] += o.dmg; if (u.side === 'att') faces[o.aspect]++; }
    return o;
  };
  let flankers = 0;
  const seen = new Set<number>();
  for (let k = 0; !b.over && k < 60000; k++) { b.step(); for (const id of b.flanking.keys()) if (!seen.has(id)) { seen.add(id); flankers++; } }
  return { b, dmg, faces, flankers };
}

describe('the sheet', () => {
  it('knows every tank the field draws, and each one\'s duels with the others', () => {
    for (const m of ['PanzerIV', 'PanzerIV:short', 'Panther', 'Sherman', 'T34', 'T34-85', 'Matilda', 'CharB1', 'M13', 'Type97']) {
      expect(ARMOUR.models[m]).toBeDefined();
      expect(ARMOUR.duels[m]?.Sherman).toBeDefined();
    }
    for (const f of ['front', 'side', 'rear'] as const) { expect(PAR[f]).toBeGreaterThan(0.2); expect(PAR[f]).toBeLessThan(1); }
    expect(PAR.front).toBeLessThan(PAR.rear);
  });

  it('a German Panzer IV carries her short 75 until March 1942, then the long gun; a Japanese Type 97 her 47 from April', () => {
    const feb42 = 30, mar42 = 31;
    expect(turnToDate(feb42)).toMatchObject({ year: 1942, month: 1 });
    expect(turnToDate(mar42)).toMatchObject({ year: 1942, month: 2 });
    expect(vehicleOf('arm', 'GER', feb42)).toBe('PanzerIV:short');
    expect(vehicleOf('arm', 'GER', mar42)).toBe('PanzerIV');
    expect(vehicleOf('arm', 'USA', JUNE44)).toBe('Sherman');
    expect(vehicleOf('inf', 'USA', JUNE44)).toBeNull();
    // The Japanese Type 97: her short 57 until the 47 mm turret (April 1942).
    expect(vehicleOf('arm', 'JAP', mar42)).toBe('Type97');
    expect(vehicleOf('arm', 'JAP', mar42 + 1)).toBe('Type97:47');
    expect(through('Type97:47', 'Sherman', 'side', 500, JUNE44)!).toBeGreaterThan(through('Type97', 'Sherman', 'side', 500, JUNE44)!);
  });

  it('weighs a Sherman\'s 75 on a Panther\'s front at all but nothing, her rear above par; the Panther\'s 75 on a Sherman above par', () => {
    const A = LAND.armour, w = (s: string, t: string, f: 'front' | 'side' | 'rear', r = 500) => armourWeight(s, t, f, r, JUNE44, A.lo, A.hi)!;
    expect(through('Sherman', 'Panther', 'front', 500, JUNE44)).toBe(0);
    expect(w('Sherman', 'Panther', 'front')).toBe(A.lo);
    expect(w('Sherman', 'Panther', 'rear')).toBeGreaterThan(1.2);
    expect(w('Panther', 'Sherman', 'front')).toBeGreaterThan(1.2);
    // The T-34's 76 on a long-gunned Panzer IV's front fades with the range; her side does not.
    expect(w('T34', 'PanzerIV', 'front', 100)).toBeGreaterThan(w('T34', 'PanzerIV', 'front', 1500));
    expect(w('T34', 'PanzerIV', 'side', 1500)).toBeGreaterThan(1);
    // An unknown model: the field's own weight stands.
    expect(armourWeight('Nothing', 'Sherman', 'front', 500, JUNE44, A.lo, A.hi)).toBeNull();
  });
});

describe('tank fights, with measured armour on', () => {
  it('Shermans defending can barely hurt a Panther attack from the front; the Panthers strike harder', () => {
    let offD = 0, onD = 0, offA = 0, onA = 0;
    for (const seed of [1, 2]) {
      const off = tally('GER', 'USA', seed, false), on = tally('GER', 'USA', seed, true);
      offD += off.dmg.def; onD += on.dmg.def; offA += off.dmg.att; onA += on.dmg.att;
    }
    expect(offD).toBeGreaterThan(0);
    expect(onD).toBeLessThan(offD * 0.4);
    expect(onA).toBeGreaterThan(offA);
  }, 300_000);

  it('the computer\'s Shermans go round the Panthers to strike their flanks and rear', () => {
    const off = { front: 0, flank: 0, rear: 0 }, on = { ...off };
    for (const seed of [2, 3]) {
      const a = tally('USA', 'GER', seed, false), b = tally('USA', 'GER', seed, true);
      expect(a.flankers).toBe(0);
      expect(b.flankers).toBeGreaterThan(0);
      for (const f of ['front', 'flank', 'rear'] as const) { off[f] += a.faces[f]; on[f] += b.faces[f]; }
    }
    // Their blows on the Panthers' fronts, a share of all their blows on them: most, off; a minority, on.
    const front = (x: typeof on) => x.front / (x.front + x.flank + x.rear);
    expect(front(off)).toBeGreaterThan(0.6);
    expect(front(on)).toBeLessThan(front(off) - 0.3);
  }, 300_000);

  it('a block the 3D set piece draws as another model is weighed as that model', () => {
    const b = new LandBattle(armoured('USA', 'GER', 1));
    const tk = b.units.find((u) => u.side === 'def' && u.kind === 'arm')!, sh = b.units.find((u) => u.side === 'att' && u.kind === 'arm')!;
    expect(b.vehicle(tk)).toBe('Panther');
    expect(b.vehicleIs(tk.id, 'Tiger')).toBe(true);
    expect(b.vehicle(tk)).toBe('Tiger');
    expect(b.armourAt(sh, tk, 'side', 500)).toBe(armourWeight('Sherman', 'Tiger', 'side', 500, JUNE44, LAND.armour.lo, LAND.armour.hi));
    expect(b.vehicleIs(tk.id, 'NoSuchTank')).toBe(false);
    expect(b.vehicleIs(9999, 'Tiger')).toBe(false);
    expect(b.vehicleIs(tk.id, null)).toBe(true);
    expect(b.vehicle(tk)).toBe('Panther');
  });

  it('off, nothing goes round and no blow is weighed', () => {
    const b = new LandBattle(armoured('USA', 'GER', 1));
    b.ai.att = b.ai.def = true;
    const seen = (b as unknown as { armourAt: LandBattle['armourAt'] });
    let asked = 0;
    const at = seen.armourAt.bind(b);
    seen.armourAt = (...a) => { asked++; return at(...a); };
    for (let k = 0; k < 4000 && !b.over; k++) b.step();
    expect(asked).toBe(0);
    expect(b.flanking.size).toBe(0);
  }, 120_000);
});

describe('how a knocked-out tank ends', () => {
  it('a hit in a Panther\'s rear burns and blows up more often than one in her front', () => {
    const count = (face: 'front' | 'rear') => {
      const n = { out: 0, burn: 0, blow: 0 };
      for (let i = 0; i < 100; i++) for (let j = 0; j < 20; j++) n[fateOf('Panther', face, i / 100, j / 20)]++;
      return n;
    };
    const front = count('front'), rear = count('rear');
    expect(rear.burn + rear.blow).toBeGreaterThan(front.burn + front.blow);
    expect(rear.blow).toBeGreaterThan(front.blow);
    expect(fateOf(null, 'side', 0.9, 0)).toBe('out');
  });

  it('every tank the drawn battle knocks out carries her fate', () => {
    const b = new LandBattle(armoured('GER', 'USA', 2));
    b.ai.att = b.ai.def = true;
    b.measured = true;
    const d = b.drillOn({ sections: true, platoons: true });
    const fates = { out: 0, burn: 0, blow: 0 };
    let last = 0;
    for (let k = 0; !b.over && k < 30000; k++) {
      b.step();
      for (const r of d.rounds()) {
        if (r.n <= last) continue;
        last = r.n;
        if (r.fate) { expect(r.r).toBe('kill'); expect(b.unit(r.on)?.kind).toBe('arm'); fates[r.fate]++; }
      }
    }
    expect(fates.out + fates.burn + fates.blow).toBeGreaterThan(0);
    expect(fates.burn + fates.blow).toBeGreaterThan(0);
  }, 300_000);
});
