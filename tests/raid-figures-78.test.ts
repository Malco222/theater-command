// Version 78: what a raid should do, as numbers. The raid planner has always said it in sentences (`effects`); the 3D
// game's raid card was reading its figures back out of those sentences by their wording. `figures` gives the same
// numbers straight off the same forecast, so a card never depends on how a sentence is phrased. Display only.
import { describe, expect, it } from 'vitest';
import { planRaid, type RaidFigures, type RaidKind, type RaidPlan } from '../src/campaign/airwar';
import { createGame, setWar, worksOf } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { previewRaid, raidTargetsFor } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const pct = (x: number): string => `${Math.round(x * 100)}%`;
/** A German campaign at war with Britain and France too, so every kind of raid has something to aim at. */
function west(seed = 7): Game {
  const g = startOpen(createGame(seed, 'GER'));
  setWar(g, 'GER', 'UK');
  setWar(g, 'GER', 'FRA');
  return g;
}
/** Every raid of a kind that can be flown this month, weighed up (at most `most` of them). */
function plans(g: Game, kind: RaidKind, most = 12): RaidPlan[] {
  const out: RaidPlan[] = [];
  for (const t of raidTargetsFor(g, kind)) {
    const p = previewRaid(g, kind, t.id);
    if (!p.why) out.push(p);
    if (out.length >= most) break;
  }
  return out;
}
const KEYS: Record<RaidKind, (keyof RaidFigures)[]> = {
  bomb: ['output', 'national', 'rail', 'factory'], night: ['output', 'national', 'rail', 'factory'],
  superiority: ['odds', 'readiness'], strike: ['ships'], fleet: ['hits', 'sunk'], harbour: ['hits', 'sunk'],
  line: ['works', 'worksLeft'], ground: ['organisation'], town: ['town'], recon: [], airfield: ['odds', 'field'],
};

describe('a raid plan gives its figures as numbers (Version 78)', () => {
  it('carries only the figures of its own kind, every one a plain number', () => {
    const g = west();
    let seen = 0;
    for (const kind of Object.keys(KEYS) as RaidKind[]) for (const p of plans(g, kind, 6)) {
      seen++;
      for (const k of Object.keys(p.figures) as (keyof RaidFigures)[]) expect(KEYS[kind], `${kind}: ${k}`).toContain(k);
      for (const [k, v] of Object.entries(p.figures)) if (typeof v === 'number') { expect(Number.isFinite(v), `${kind}: ${k}`).toBe(true); expect(v, `${kind}: ${k}`).toBeGreaterThanOrEqual(0); }
      // Plain data: it survives a trip through JSON unchanged, so the 3D game can read it over the wire.
      expect(JSON.parse(JSON.stringify(p.figures))).toEqual(p.figures);
    }
    expect(seen).toBeGreaterThan(5);
  });

  it('gives a bombing raid the output stopped, the share of national production, and the chances on the railway and a factory', () => {
    const g = west();
    const all = [...plans(g, 'bomb'), ...plans(g, 'night')];
    const works = all.filter((p) => g.regions[p.target].fac > 0);
    expect(works.length).toBeGreaterThan(0);
    for (const p of works) {
      const f = p.figures, words = p.effects.join(' ');
      expect(f.output).toBeGreaterThanOrEqual(0);
      expect(f.output).toBeLessThanOrEqual(1);
      expect(f.national).toBeLessThanOrEqual(f.output! + 1e-9);
      expect(words).toContain(`Should stop about ${pct(f.output!)} of`);
      expect(words).toContain(`${(f.national! * 100).toFixed(1)}% of`);
      // The railway and the factory: a figure where the sentence is there, and none where it is not.
      expect(f.rail !== undefined).toBe(/cutting the railway/.test(words));
      if (f.rail !== undefined) expect(words).toContain(`About a ${pct(f.rail)} chance of cutting the railway`);
      expect(f.factory !== undefined).toBe(/wrecking a factory/.test(words));
      if (f.factory !== undefined) expect(words).toContain(`About a ${pct(f.factory)} chance of wrecking a factory`);
    }
    // A place with no industry: no output figure, and the sentence says so.
    for (const p of all.filter((x) => g.regions[x.target].fac <= 0)) { expect(p.figures.output).toBeUndefined(); expect(p.effects[0]).toMatch(/no industry/); }
  });

  it('gives a raid on troops the organisation lost, and a fight for the sky its fighter odds', () => {
    const g = west();
    const ground = plans(g, 'ground');
    expect(ground.length).toBeGreaterThan(0);
    for (const p of ground) {
      expect(p.figures.organisation).toBeGreaterThanOrEqual(0);
      expect(p.effects[0]).toContain(`should lose about ${pct(p.figures.organisation!)} of their organisation`);
    }
    const sky = plans(g, 'superiority');
    expect(sky.length).toBeGreaterThan(0);
    for (const p of sky) {
      const o = p.figures.odds!;
      expect(o).toBeGreaterThanOrEqual(0);
      expect(p.effects[0]).toContain(o >= 9 ? 'Fighter odds overwhelming' : `Fighter odds ${o.toFixed(1)} to 1`);
      expect(p.figures.readiness !== undefined).toBe(p.effects.length > 1);
      if (p.figures.readiness !== undefined) expect(p.effects[1]).toContain(`about ${pct(p.figures.readiness)} of their readiness`);
    }
  });

  it('gives a raid on the line the months taken off the works and the months left, and a raid on a town what the siege rules expect', () => {
    const g = west();
    // The French in front of the Rhine have had three months to dig.
    const dug = plans(g, 'ground').map((p) => g.regions[p.target]).flatMap((r) => r.units.filter((u) => !u.dead && u.nat === 'FRA'));
    expect(dug.length).toBeGreaterThan(0);
    for (const c of dug) { c.dug = true; c.dugFor = 3; }
    const line = plans(g, 'line');
    expect(line.length).toBeGreaterThan(0);
    for (const p of line) {
      const f = p.figures;
      expect(f.works).toBeGreaterThanOrEqual(0);
      expect(f.worksLeft).toBeGreaterThanOrEqual(0);
      // What it takes is never more than the raid's whole wear, and what is left is the rest of the deepest works there.
      expect(f.works!).toBeLessThanOrEqual((p.wear ?? 0) + 1e-9);
      expect(/Too few bombers/.test(p.effects[0])).toBe(f.works! < 0.05);
      const deepest = Math.max(0, ...g.regions[p.target].units.filter((u) => !u.dead && g.nations.GER.atWar.includes(u.nat)).map(worksOf));
      expect(deepest).toBeGreaterThan(0);
      expect(f.works! + f.worksLeft!).toBeCloseTo(deepest, 6);
    }
    for (const p of plans(g, 'town')) {
      if (!p.figures.town) { expect(p.effects).toHaveLength(0); continue; }
      const t = p.figures.town;
      expect(t.works).toBeGreaterThanOrEqual(0);
      expect(t.bonusAfter).toBeLessThanOrEqual(t.bonus + 1e-9);
      if (t.works >= 0.5) expect(p.effects[0]).toContain(`Should break down about ${Math.round(t.works)}% of the works`);
    }
  });

  it('gives a strike on ships the bombs that should hit and what they should sink, class by class', () => {
    const g = west();
    const sea = [...plans(g, 'harbour'), ...plans(g, 'fleet')];
    for (const p of sea) {
      const f = p.figures;
      expect(f.hits).toBeGreaterThanOrEqual(0);
      expect(p.effects[0]).toContain(`About ${f.hits! >= 10 ? Math.round(f.hits!).toLocaleString('en-US') : f.hits!.toFixed(1)} bombs and torpedoes should hit.`);
      if (f.sunk) for (const n of Object.values(f.sunk)) expect(n).toBeGreaterThanOrEqual(0);
      const sure = f.sunk ? Object.values(f.sunk).some((n) => n >= 1) : false;
      expect(/That should sink/.test(p.effects[0])).toBe(sure);
    }
    for (const p of plans(g, 'strike')) {
      expect(p.figures.ships).toBeGreaterThanOrEqual(0);
      expect(p.effects[0]).toContain(`could lose up to ${pct(p.figures.ships!)} of their number`);
    }
  });

  it('has no figures for a scouting flight or a raid that cannot fly, and changes nothing in the game', () => {
    const g = west();
    const before = saveGame(g);
    const scout = plans(g, 'recon', 3);
    for (const p of scout) expect(p.figures).toEqual({});
    // A raid that cannot be flown says why and carries no figures.
    const far = planRaid(g, 'bomb', g.byName.Warsaw.id, [], 'GER');
    expect(far.why).toBeTruthy();
    expect(far.figures).toEqual({});
    for (const kind of ['bomb', 'ground', 'superiority', 'harbour'] as RaidKind[]) plans(g, kind, 3);
    expect(saveGame(g)).toBe(before);
  });
});
