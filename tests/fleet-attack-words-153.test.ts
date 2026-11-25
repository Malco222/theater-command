// Release 1.5.3: the fleet attack card's words (the 1.5.1 playtest, findings 25, 26 and 33). Words only, and the sea
// report's strength line: no rule reads any of them, and the card stays a forecast that throws no die.
//   25  a fleet the player's side has only reported keeps its name and its port to itself, in every refusal and on the card;
import { describe, expect, it } from 'vitest';
import { fleetKnown } from '../src/campaign/carriers';
import { createGame } from '../src/campaign/game';
import { detach } from '../src/campaign/fleets';
import { orderFleetAttack, planFleetAttack, whyNotFleetAttack } from '../src/campaign/fleetattack';
import { loadGame } from '../src/campaign/save';
import { sail, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;

interface Row { title: string; lines: string[]; buttons: { tip?: string }[] }
interface Page { id: string; title: string; headline: string; note: string; top: { label: string; ok: boolean; tip: string }[]; sections: { heading: string; text: string; rows: Row[] }[] }
/** Every word a page shows: its title, headline, note, buttons' tips, and each section's text and rows. */
const words = (p: Page): string[] => [p.title, p.headline, p.note, ...p.top.map((b) => b.tip),
  ...p.sections.flatMap((s) => [s.heading, s.text, ...s.rows.flatMap((r) => [r.title, ...r.lines, ...r.buttons.map((b) => b.tip ?? '')])])];

/** Germany at the table in its own first month of the grand campaign, with the fog as the game sets it. */
function germany(seed: number): Game {
  const g = startOpen(createGame(seed, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 50) stepAI(g);
  expect(humanUp(g)).toBe(true);
  return g;
}

describe('25: a fleet only reported stays unnamed', () => {
  // The playtest's case: GER from 1, seed 11. The view shows "British ships reported" and "Canadian ships reported", with no port.
  for (const world of [false, true]) {
    it(`no card names it or its port, and the out-of-reach refusal says "those ... ships" (${world ? 'the world' : 'Europe'})`, () => {
      TC.start({ human: 'GER', from: 1, seed: 11, view: 'grand', depth: 'theatre', world });
      const g = loadGame(TC.save().text)!;
      const v = TC.view() as { fleets: { id: number; nat: string; name: string; port: string; report: boolean }[] };
      const mine = v.fleets.filter((f) => f.nat === 'GER');
      const reported = v.fleets.filter((f) => f.report);
      expect(reported.length).toBeGreaterThan(0);
      let refused = 0;
      for (const t of reported) {
        const real = g.fleets.find((f) => f.id === t.id)!;
        expect(real.port).not.toBe('');
        expect(t.name).toMatch(/ ships reported$/);
        expect(t.port).toBe('');
        for (const f of mine) {
          const p = TC.page(`fleetattack:${f.id}:${t.id}`) as Page | null;
          if (!p) continue;
          for (const w of words(p)) {
            expect(w, p.id).not.toContain(real.name);
            expect(w, p.id).not.toContain(real.port);
          }
          if (/out of reach/.test(p.headline)) {
            refused++;
            expect(p.headline).toMatch(new RegExp(`^Those ${g.nations[real.nat].adj} ships are (in the [A-Z][a-z]+|in harbour)`));
            expect(p.top[0]).toMatchObject({ label: 'SEND THEM IN', ok: false, tip: p.headline });
            if (real.stance === 'port') expect(p.headline).toMatch(/ are in harbour, out of reach of the /);
          }
          // The act refused says the same, and names nothing either.
          if (!p.top[0].ok) {
            const out = TC.act(`fleetattack:${f.id}:${t.id}`) as { ok: boolean; note: string };
            expect(out.ok).toBe(false);
            expect(out.note).not.toContain(real.name);
            expect(out.note).not.toContain(real.port);
          }
        }
        // The fleet's own room lists it, in harbour or at sea, without its port.
        for (const f of mine) {
          const room = TC.page(`fleet:${f.id}`) as Page | null;
          for (const w of room ? room.sections.filter((s) => s.heading === 'ENEMY FLEETS TO ATTACK').flatMap((s) => s.rows.filter((r) => r.title === t.name).flatMap((r) => r.lines)) : []) {
            expect(w).not.toContain(real.port);
          }
        }
      }
      expect(refused).toBeGreaterThan(0);
      if (world) {
        // The Canadian ships at Halifax, out of the German fleets' reach: in harbour, and no more.
        const canada = reported.find((t) => t.nat === 'CAN');
        if (canada) {
          const p = TC.page(`fleetattack:${mine.find((f) => f.name === 'Swinemunde Fleet')!.id}:${canada.id}`) as Page;
          expect(p.headline).toBe('Those Canadian ships are in harbour, out of reach of the Swinemunde Fleet from Swinemunde.');
          expect(p.note).toMatch(/against Canadian ships reported, in harbour\. /);
        }
      }
    });
  }

  it('the rules say it the same way: out of reach, not patrolling, not found, and making for port', () => {
    let missed = 0, declined = 0;
    for (let seed = 1; seed <= 12; seed++) {
      const g = germany(seed);
      const battle = fleet(g, 'Battle Fleet'), home = fleet(g, 'Home Fleet');
      // The Home Fleet raiding in the North Sea, last met a month ago: reported, not identified. A squadron off it,
      // patrolling the same sea, in contact this month: identified, so the card has figures and its notes.
      sail(g, home, zoneOf(g, 'nsea'), 'raid');
      home.seen = { GER: g.turn - 1 };
      const sq = detach(g, home, { dd: 2 })!;
      sail(g, sq, zoneOf(g, 'nsea'), 'patrol');
      sq.seen = { GER: g.turn };
      if (fleetKnown(g, home, 'GER') !== 'report') continue;
      const plan = planFleetAttack(g, battle.id, home.id)!;
      expect(plan.theirs.fleets.find((x) => x.id === home.id)!.name).toBe('British ships reported');
      expect(plan.notes.join(' ')).toContain('Those British ships are not patrolling, so they have to be found first: about ');
      for (const w of [...plan.notes, ...plan.lines.flatMap((l) => [l.value, l.effect ?? ''])]) expect(w).not.toContain('Home Fleet');
      const out = orderFleetAttack(g, battle.id, home.id)!;
      // (Fought, the two sides are in contact and know each other by name: the battle's news names them.)
      if (out.outcome === 'missed') {
        missed++;
        expect(out.text).toBe('The Battle Fleet searched in the North Sea for those British ships and did not find them.');
      }
    }
    expect(missed).toBeGreaterThan(0);
    for (let seed = 1; seed <= 20; seed++) {
      const g = germany(seed);
      const battle = fleet(g, 'Battle Fleet'), home = fleet(g, 'Home Fleet');
      // A small British squadron patrolling the North Sea, last met a month ago: outweighed, it may make for port.
      const sq = detach(g, home, { dd: 2 })!;
      sail(g, home, home.home, 'port');
      sail(g, sq, zoneOf(g, 'nsea'), 'patrol');
      sq.seen = { GER: g.turn - 1 };
      if (fleetKnown(g, sq, 'GER') !== 'report') continue;
      const out = orderFleetAttack(g, battle.id, sq.id)!;
      if (out.outcome === 'declined') {
        declined++;
        expect(out.text).toBe('Those British ships decline battle in the North Sea and make for port.');
        expect(g.headlines.at(-1)!.text).toBe(out.text);
      }
    }
    expect(declined).toBeGreaterThan(0);
    // Identified, it is named as before.
    const g = germany(3);
    const swine = fleet(g, 'Swinemunde Fleet'), home = fleet(g, 'Home Fleet');
    sail(g, home, zoneOf(g, 'wapp'), 'patrol');
    home.seen = { GER: g.turn };
    expect(whyNotFleetAttack(g, swine.id, home.id)).toBe('The Home Fleet is in the Western Approaches, out of reach of the Swinemunde Fleet from Swinemunde.');
  });
});
