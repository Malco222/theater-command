// Division numbers on the far chart (release 2, the grand campaign; src/campaign/armies.ts).
//
// The design director's skip of 12:49Z, 25 November 2026, is one rule for both charts: a stand-in passes over any
// number that a dated row of its country names, or that a living division carries. Two things on the far chart went
// round it, found by the joiner's six-year traces of the far chart and by the second review of "a division is laid
// once" (joins/r2-head-1125o-onto-1c67ea0d-NOTES.md):
//
// - The divisions the Far East sends west (pacific.json `west`) are rows of another shape, [the corps it leaves, how
//   many, the name it goes by], and the skip never read them: the 21st, 26th, 32nd and 65th Rifle Divisions were
//   stand-ins of four Soviet corps on the first day, and the corps the wave split off read the same names.
// - A corps that passes under another flag (the Netherlands Indies' garrisons into Britain's keeping, Indochina's
//   garrison under Japan's demand) kept the numbers of the army that raised it: eight KNIL garrisons read "4th" to
//   "11th Infantry Division" beside Britain's own.
//
// Labels only: no strength is laid or lost, and no war moves. With the far side's switch off nothing here is read.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import pacific from '../data/campaign/pacific.json';
import { divisionsNamed, divisionsOf, numberSpokenFor, numbersUnderFlag } from '../src/campaign/armies';
import { addCorps, createGame } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

const far = (seed = 2): Game => startOpen(createGame(seed, null), true);
const names = (g: Game, c: Corps): string[] => divisionsOf(g, c).map((d) => d.name);
/** Every division name two living corps of one country hold. */
function heldTwice(g: Game): string[] {
  const out: string[] = [];
  for (const nat of Object.keys(g.nations)) {
    const seen = new Map<string, Corps>();
    for (const c of g.corps) if (c.nat === nat && !c.dead) for (const d of divisionsOf(g, c)) {
      const was = seen.get(d.name);
      if (was && was !== c) out.push(`${nat} ${d.name}: ${was.name} and ${c.name}`);
      seen.set(d.name, c);
    }
  }
  return out;
}

describe('the divisions the Far East sends west are their own names (pacific.json west)', () => {
  const west = (pacific as any).west as { nat: string; waves: { rows: [string, number, string][] }[] };
  const sent = west.waves.flatMap((w) => w.rows.flatMap((r) => divisionsNamed(r[2], west.nat, 'inf').map(([full]) => full)));

  it('the data names them, the 32nd Rifle Division that went to Moscow among them', () => {
    expect(west.nat).toBe('SOV');
    expect(sent.length).toBeGreaterThanOrEqual(11);
    for (const name of ['21st Rifle Division', '26th Rifle Division', '32nd Rifle Division', '92nd Rifle Division', '93rd Rifle Division', '60th Tank Division', '65th Rifle Division', '58th Tank Division', '204th Rifle Division', '205th Rifle Division', '422nd Rifle Division']) expect(sent, name).toContain(name);
  });

  it('no Soviet stand-in carries one of their numbers on the first day, and none will', () => {
    const g = far();
    const held = new Set(g.corps.filter((c) => c.nat === 'SOV' && !c.dead).flatMap((c) => names(g, c)));
    for (const name of sent) expect(held.has(name), name).toBe(false);
    for (const n of [21, 26, 32, 65, 92, 93, 204, 205, 422]) expect(numberSpokenFor(g, 'SOV', 'inf', n), `rifle ${n}`).toBe(true);
    for (const n of [58, 60]) expect(numberSpokenFor(g, 'SOV', 'arm', n), `tank ${n}`).toBe(true);
    expect(numberSpokenFor(g, 'SOV', 'inf', 22)).toBe(false);
    // A corps raised later passes over them too.
    g.flags['dn:SOV:inf'] = 91;
    const late = addCorps(g, 'SOV', 'A late corps', 'inf', 3, g.byName['Moscow'].id, {});
    expect(names(g, late)).toEqual(['94th Rifle Division', '95th Rifle Division', '96th Rifle Division']);
    expect(heldTwice(g)).toEqual([]);
  });
});

describe('a corps that passes under another flag (armies.ts numbersUnderFlag)', () => {
  it('takes its new nation\'s next numbers where its own are another division\'s name there, and keeps them where they are not', () => {
    const g = far();
    const uk = (): Set<string> => new Set(g.corps.filter((c) => c.nat === 'UK' && !c.dead).flatMap((c) => names(g, c)));
    const knil = g.corps.filter((c) => c.nat === 'NED' && !c.dead && c.kind === 'inf' && g.regions[c.reg].overseas);
    expect(knil.length).toBeGreaterThanOrEqual(8);
    const top = g.flags['dn:UK:inf'];
    // One whose number is a British division's already, and one whose number is nobody's under Britain's flag.
    const before = uk();
    const clash = knil.find((c) => c.divs === 1 && before.has(names(g, c)[0]))!;
    const clear = knil.find((c) => c.divs === 1 && !before.has(names(g, c)[0]) && !numberSpokenFor(g, 'UK', 'inf', c.dn!));
    expect(clash).toBeDefined();
    const was = names(g, clash)[0];
    clash.nat = 'UK';
    numbersUnderFlag(g, clash);
    expect(names(g, clash)[0]).not.toBe(was);
    expect(clash.dn).toBe(top + 1);
    expect(g.flags['dn:UK:inf']).toBe(top + 1);
    expect(heldTwice(g).filter((t) => t.startsWith('UK '))).toEqual([]);
    if (clear) {
      const no = clear.dn!;
      clear.nat = 'UK';
      numbersUnderFlag(g, clear);
      expect(clear.dn).toBe(no);
      // Britain's own count comes to that number later and passes over it: it is a living division's.
      g.flags['dn:UK:inf'] = Math.max(g.flags['dn:UK:inf'], no - 1);
      const late = addCorps(g, 'UK', 'A late corps', 'inf', 2, g.byName['London'].id, {});
      expect(divisionsOf(g, late).map((d) => d.no)).not.toContain(no);
      expect(heldTwice(g).filter((t) => t.startsWith('UK '))).toEqual([]);
    }
  });

  it('outside the grand campaign nothing is renumbered', () => {
    const g = createGame(2, null);
    const c = g.corps.find((x) => x.nat === 'NED' && !x.dead)!;
    const was = { dn: c.dn, nos: c.nos };
    c.nat = 'UK';
    numbersUnderFlag(g, c);
    expect({ dn: c.dn, nos: c.nos }).toEqual(was);
  });
});

describe('three years of a far war nobody plays', () => {
  it('no division name is held by two living corps of one country in any month (seed 2)', () => {
    const g = far(2);
    const twice = new Set<string>();
    while (g.turn <= 36) {
      runTurnAI(g);
      for (const t of heldTwice(g)) twice.add(t);
    }
    // (The Netherlands fell and her Indies passed into keeping; Japan made her demand of Indochina; the Far East sent its first waves west.)
    expect(g.corps.some((c) => c.raised === 'NED' && c.nat !== 'NED' && !c.dead)).toBe(true);
    expect([...twice]).toEqual([]);
  }, 1_800_000);
});
