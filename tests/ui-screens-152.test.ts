// The campaign screens' faults the playtester found in 1.5.1 (25 November 2026, results-r15/151-playtest, all old):
// the province panel's raise buttons (#29), the cabinet's SHOW ON THE MAP (#31), the month's report after a load
// (#32), fleet strength called fleets (#20, #35), "1 infantry divisions" (#36), a shut fleet order that does not say
// it is the order now (#37), a hyphen for a minus and stray spaces (#38), and "Only the 0 men" (#48).
import { describe, expect, it } from 'vitest';
import { menWords } from '../src/campaign/manpower';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

interface Btn { label: string; act: string; ok: boolean; tip: string }
interface Row { title: string; lines: string[]; buttons: Btn[] }
interface Page { ok?: boolean; id: string; title: string; sections: { heading: string; text: string; rows: Row[] }[] }
interface Raise { kind: string; cost: number; ok: boolean; why: string }
interface Bridge {
  start(o: Record<string, unknown>): unknown; save(): { text: string }; load(text: string): unknown;
  endMonth(): unknown; page(id: string): Page | null; act(a: string, r?: number): { ok: boolean };
  province(id: number): { raise: Raise[] } | null; raise(kind: string, region: number): { ok: boolean };
  view(): { pages: { id: string }[] };
}
const TC = (globalThis as Record<string, unknown>).TC as Bridge;

describe('the province panel raises what the rules raise (#29)', () => {
  it('every button shown open is accepted, and every one shut says why', () => {
    TC.start({ view: 'grand', human: 'USA', from: 29, seed: 5, world: true });
    for (const id of [577, 375, 914]) {
      const p = TC.province(id);
      if (!p) continue;
      for (const r of p.raise) {
        if (!r.ok) { expect(r.why, `${id} ${r.kind}`).not.toBe(''); continue; }
        const text = TC.save().text;
        expect(TC.raise(r.kind, id).ok, `${id} ${r.kind}`).toBe(true);
        TC.load(text);
      }
    }
  });

  it("Richmond's tanks are shut with the reason and the place that can raise them", () => {
    TC.start({ view: 'grand', human: 'USA', from: 29, seed: 5, world: true });
    const arm = TC.province(577)!.raise.find((r) => r.kind === 'arm')!;
    expect(arm.ok).toBe(false);
    expect(arm.why).toMatch(/^.+ (has not the \d+ factor(y|ies) it needs|is too steep for tanks to form up)\. Raise it at .+\.$/);
  });

  it('the price is the nation\'s own, and a corps the treasury can pay for is open', () => {
    TC.start({ view: 'grand', human: 'UK', from: 17, seed: 6, world: true });
    const mot = TC.province(101)!.raise.find((r) => r.kind === 'mot')!;
    const text = TC.save().text;
    const took = TC.raise('mot', 101).ok;
    TC.load(text);
    expect(mot.ok).toBe(took);
  });
});

describe("the cabinet's SHOW ON THE MAP goes to the place (#31)", () => {
  it('every map button on the cabinet page names a region in three parts', () => {
    TC.start({ view: 'grand', human: 'GER', from: 1, seed: 3, world: true });
    for (let i = 0; i < 6; i++) TC.endMonth();
    const p = TC.page('cabinet')!;
    const gotos = p.sections.flatMap((x) => x.rows.flatMap((r) => r.buttons)).filter((b) => b.act.startsWith('goto:'));
    expect(gotos.length).toBeGreaterThan(0);
    for (const b of gotos) expect(b.act, b.label).toMatch(/^goto:[a-z]+:\d+$/);
  });
});

describe("the month's report survives a save and a load (#32)", () => {
  it('the report is there after loading, and a second save gives the same text', () => {
    TC.start({ view: 'grand', human: 'GER', from: 1, seed: 3, world: true });
    TC.endMonth(); TC.endMonth();
    const before = TC.page('reports');
    expect(before?.ok).toBe(true);
    const text = TC.save().text;
    TC.start({ view: 'grand', human: 'UK', from: 17, seed: 6, world: true });
    TC.load(text);
    expect(TC.view().pages.some((p) => p.id === 'reports')).toBe(true);
    expect(TC.page('reports')).toEqual(before);
    expect(TC.save().text).toBe(text);
    // The save carries it beside the rules' flags.
    expect(JSON.parse(TC.save().text).flags.monthReport).toBeDefined();
  });

  it('a save made before the first month ends has none, and loads as before', () => {
    TC.start({ view: 'grand', human: 'GER', from: 1, seed: 3, world: true });
    const text = TC.save().text;
    expect(JSON.parse(text).flags.monthReport).toBeUndefined();
    TC.load(text);
    expect(TC.page('reports')?.ok ?? false).toBe(false);
  });
});

describe('words (#35, #36, #37, #38, #48)', () => {
  it('fleet strength on the crossings page is called ships', () => {
    TC.start({ view: 'grand', human: 'GER', from: 1, seed: 3, world: true });
    const p = TC.page('crossings');
    const text = JSON.stringify(p);
    expect(text).not.toMatch(/Enemy fleets (that|your)/);
  });

  it('the forces page counts each kind of division with its own noun', () => {
    TC.start({ view: 'grand', human: 'GER', from: 1, seed: 3, world: true });
    const lines = TC.page('forces')!.sections.flatMap((x) => x.rows.flatMap((r) => r.lines));
    expect(lines.some((l) => /: \d+ /.test(l))).toBe(true);
    for (const l of lines) {
      expect(l).not.toMatch(/\b1 [a-z]+ divisions\b/);
      expect(l).not.toMatch(/\b(engineers|armor) divisions?\b/);
    }
  });

  it("a fleet order shut because it is the order now says so", () => {
    TC.start({ view: 'grand', human: 'GER', from: 1, seed: 3, world: true });
    let seen = 0;
    for (const id of [1, 2, 3, 4, 5, 6, 7, 8]) {
      const p = TC.page(`fleet:${id}`);
      if (!p?.sections) continue;
      for (const b of p.sections.flatMap((x) => x.rows.flatMap((r) => r.buttons))) {
        if (!/^fleet:\d+:\d+:(patrol|raid|port)$/.test(b.act) || b.ok) continue;
        seen++;
        expect(b.tip, b.act).toMatch(/^It is (doing so now|in harbour now)\. /);
      }
    }
    expect(seen).toBeGreaterThan(0);
  });

  it('the cabinet minutes use a minus sign, and the report rows are trimmed', () => {
    TC.start({ view: 'grand', human: 'GER', from: 1, seed: 3, world: true });
    for (let i = 0; i < 6; i++) TC.endMonth();
    TC.act('cabinet:tab:minutes');
    const cab = JSON.stringify(TC.page('cabinet'));
    expect(cab).not.toMatch(/ -\d/);
    const rows = TC.page('reports')!.sections.flatMap((x) => x.rows.flatMap((r) => r.lines));
    for (const l of rows) expect(l).toBe(l.trim());
  });

  it('a few hundred men are given whole', () => {
    expect(menWords(235)).toBe('235');
    expect(menWords(999.6)).toBe('1,000');
    expect(menWords(36000)).toBe('36,000');
    expect(menWords(-5)).toBe('0');
  });
});
