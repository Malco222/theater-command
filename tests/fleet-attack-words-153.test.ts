// Release 1.5.3: the fleet attack card's words (the 1.5.1 playtest, findings 25, 26 and 33). Words only, and the sea
// report's strength line: no rule reads any of them, and the card stays a forecast that throws no die.
//   25  a fleet the player's side has only reported keeps its name and its port to itself, in every refusal and on the card;
//   26  "1 ship", not "1 ships"; the losses listed with "and" before the last, and their range said plainly; a battleship
//       or carrier the likely count rounds away, though it may well go down, named on that line;
//   33  odds whose two ends read the same are said once: "More than 10 to 1 all day".
import { describe, expect, it } from 'vitest';
import { fleetKnown } from '../src/campaign/carriers';
import { createGame } from '../src/campaign/game';
import { detach } from '../src/campaign/fleets';
import { HEAVY_RISK, orderFleetAttack, planFleetAttack, whyNotFleetAttack, type FleetAttackPlan } from '../src/campaign/fleetattack';
import { setFog } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { sail, zoneOf } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
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

/** A list of ships as the card says it: "nothing", "1 destroyer", or "1 battleship, 2 cruisers and 3 destroyers". */
const LIST = String.raw`(?:nothing|\d+ [a-z]+(?:(?:, \d+ [a-z]+)* and \d+ [a-z]+)?)`;
const lossRows = (p: Page): Row[] => p.sections.find((s) => s.heading === 'IF IT IS FOUGHT')!.rows;
const oddsRow = (p: Page): Row => p.sections.find((s) => s.heading === 'THE ODDS')!.rows.find((r) => r.title === 'Odds')!;
/** The page of one of the player's fleets sent at an enemy fleet, both found by name in the view. */
const cardFor = (mine: string, theirs: string): Page => {
  const v = TC.view() as { human: string; fleets: { id: number; nat: string; name: string }[] };
  const a = v.fleets.find((f) => f.nat === v.human && f.name === mine)!, b = v.fleets.find((f) => f.nat !== v.human && f.name === theirs)!;
  return TC.page(`fleetattack:${a.id}:${b.id}`) as Page;
};

describe('26: the card counts and lists its ships in plain words', () => {
  it('a fleet of one ship is "1 ship", on its row and in the forces line', () => {
    // The playtest's case: UK from 17, seed 5, one destroyer split off the Home Fleet.
    TC.start({ human: 'UK', view: 'grand', from: 17, seed: 5 });
    const v = TC.view() as { fleets: { id: number; nat: string; name: string }[] };
    const home = v.fleets.find((f) => f.name === 'Home Fleet' && f.nat === 'UK')!;
    const made = TC.act(`fleetsplit:${home.id}:dd:1`) as { ok: boolean; made: number };
    expect(made.ok).toBe(true);
    const foe = v.fleets.find((f) => f.nat === 'GER')!;
    const p = TC.page(`fleetattack:${made.made}:${foe.id}`) as Page;
    const titles = p.sections.find((s) => s.heading === 'YOUR SHIPS')!.rows.map((r) => r.title);
    expect(titles).toContain('1st Escort Fleet  \u00B7  1 ship');
    expect(titles.some((t) => /^Home Fleet  \u00B7  \d+ ships$/.test(t))).toBe(true);
    for (const t of titles) expect(t).not.toMatch(/\b1 ships\b/);
    // The forces line, where one ship goes in alone.
    const g = germany(3);
    setFog(g, false);
    const sq = detach(g, fleet(g, 'Battle Fleet'), { dd: 1 })!;
    const plan = planFleetAttack(g, sq.id, fleet(g, 'Home Fleet').id)!;
    expect(plan.lines.find((l) => l.label === 'Forces')!.value).toMatch(/^1 ship against \d+$/);
  });

  it('the losses read as a list, and their range as "at best ... at worst ..."', () => {
    // The playtest's case: UK from 1, seed 11, the Home Fleet sent at the Battle Fleet.
    TC.start({ human: 'UK', view: 'grand', from: 1, seed: 11, depth: 'theatre' });
    const p = cardFor('Home Fleet', 'Battle Fleet');
    const [ours, theirs] = lossRows(p);
    expect(ours.title).toBe('You are likely to lose');
    expect(theirs.title).toBe('He is likely to lose');
    // (a heavy ship at real risk is named after the list: below)
    expect(ours.lines[0]).toMatch(new RegExp(`^${LIST}(, and .+ may well go down)?$`));
    expect(theirs.lines[0]).toMatch(new RegExp(`^${LIST}(, and .+ may well go down)?$`));
    expect(ours.lines[1]).toMatch(new RegExp(`^(At best you lose ${LIST}; at worst ${LIST}|You lose ${LIST} however the day goes)\\.$`));
    expect(theirs.lines[1]).toMatch(new RegExp(`^(At the least he loses ${LIST}; at the most ${LIST}|He loses ${LIST} however the day goes)\\.$`));
    for (const w of words(p)) {
      expect(w).not.toMatch(/as the day goes\.$/);
      expect(w).not.toMatch(/^Between .* and .* as the day goes\.$/);
    }
    // At more than 10 to 1 the best is nothing, and the line says so.
    TC.start({ human: 'GER', view: 'grand', from: 1, seed: 3, world: true });
    const q = cardFor('Battle Fleet', 'Polish Navy');
    expect(lossRows(q)[0].lines[1]).toMatch(new RegExp(`^At best you lose nothing; at worst ${LIST}\\.$`));
  });
});

describe('26: a heavy ship at real risk is named on the likely line', () => {
  /** The heavy classes the plan names: a battleship or carrier the likely count rounds to none, expected at HEAVY_RISK or more. */
  const atRisk = (p: FleetAttackPlan, side: 'ours' | 'theirs'): string[] => (['bb', 'cv'] as const).filter((c) => p.likely[side][c] === 0 && p.expected[side][c] >= HEAVY_RISK);

  it('the playtest\'s case: the Home Fleet at the Battle Fleet may well lose a battleship, and the card says so', () => {
    // UK from 1, seed 11: the line said "1 cruiser, 3 destroyers", and a battleship went down in 20 to 45% of the fights.
    TC.start({ human: 'UK', view: 'grand', from: 1, seed: 11, depth: 'theatre' });
    const before = TC.save().text as string;
    const g = loadGame(before)!;
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    const plan = planFleetAttack(g, home.id, battle.id)!;
    expect(plan.risk.ours).toEqual(atRisk(plan, 'ours'));
    expect(plan.risk.theirs).toEqual(atRisk(plan, 'theirs'));
    expect(plan.risk.ours).toContain('bb');
    expect(plan.likely.ours.bb).toBe(0);
    expect(plan.expected.ours.bb).toBeGreaterThanOrEqual(HEAVY_RISK);
    const p = TC.page(`fleetattack:${home.id}:${battle.id}`) as Page;
    const [ours, theirs] = lossRows(p);
    expect(ours.lines[0]).toMatch(new RegExp(`^${LIST}, and a battleship${plan.risk.ours.includes('cv') ? ' and a carrier' : ''} may well go down$`));
    if (!plan.risk.theirs.length) expect(theirs.lines[0]).toMatch(new RegExp(`^${LIST}$`));
    else expect(theirs.lines[0]).toMatch(/, and a (battleship|carrier)( and a carrier)? may well go down$/);
    // A forecast still: the card throws no die and changes nothing.
    expect(TC.page(`fleetattack:${home.id}:${battle.id}`)).toEqual(p);
    expect(TC.save().text).toBe(before);
    const dice = g.rng.state, saved = saveGame(g);
    planFleetAttack(g, home.id, battle.id);
    expect(g.rng.state).toBe(dice);
    expect(saveGame(g)).toBe(saved);
  });

  it('where nothing else is likely to go down: "Probably nothing, but a battleship and a carrier may well go down"', () => {
    const g = startOpen(createGame(11, 'UK'));
    let guard = 0;
    while (!humanUp(g) && guard++ < 50) stepAI(g);
    beginHuman(g);
    setFog(g, false);
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    for (const f of g.fleets.filter((x) => x.nat === 'UK' && x.id !== home.id)) sail(g, f, f.home, 'port');
    const sq = detach(g, home, { bb: 1, cv: 1 })!;
    const plan = planFleetAttack(g, sq.id, battle.id)!;
    expect(Object.values(plan.likely.ours).every((n) => n === 0)).toBe(true);
    expect(plan.risk.ours).toEqual(['bb', 'cv']);
    TC.load(saveGame(g));
    const p = TC.page(`fleetattack:${sq.id}:${battle.id}`) as Page;
    expect(lossRows(p)[0].lines[0]).toBe('Probably nothing, but a battleship and a carrier may well go down');
    // Below the mark, nothing is added: a battleship expected at less than HEAVY_RISK is not named.
    for (const side of ['ours', 'theirs'] as const) for (const c of ['bb', 'cv'] as const) {
      if (plan.expected[side][c] < HEAVY_RISK) expect(plan.risk[side]).not.toContain(c);
    }
  });
});

describe('33: the odds said once where both ends read the same', () => {
  it('"More than 10 to 1 all day", and the two ends where they differ', () => {
    // The playtest's case: GER from 1, seed 3, the world: the Battle Fleet sent at the Polish Navy.
    TC.start({ human: 'GER', view: 'grand', from: 1, seed: 3, world: true });
    const q = cardFor('Battle Fleet', 'Polish Navy');
    expect(q.headline).toMatch(/: more than 10 to 1, about \d+% to win$/);
    expect(oddsRow(q).lines).toEqual(['more than 10 to 1', 'More than 10 to 1 all day']);
    TC.start({ human: 'UK', view: 'grand', from: 1, seed: 11, depth: 'theatre' });
    expect(oddsRow(cardFor('Home Fleet', 'Battle Fleet')).lines[1]).toMatch(/^Between \d+\.\d and \d+\.\d to 1 as the day goes$/);
    // Hopeless odds read the same at both ends too: one British destroyer sent at the Battle Fleet in its harbour.
    const g = startOpen(createGame(3, 'UK'));
    let guard = 0;
    while (!humanUp(g) && guard++ < 50) stepAI(g);
    setFog(g, false);
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    const sq = detach(g, home, { dd: 1 })!;
    // (no other British fleet joins it: every one of them is sent to harbour)
    for (const f of g.fleets.filter((x) => x.nat === 'UK' && x.id !== sq.id)) sail(g, f, f.home, 'port');
    sail(g, battle, battle.home, 'port');
    const plan = planFleetAttack(g, sq.id, battle.id)!;
    const odds = plan.lines.find((l) => l.label === 'Odds')!;
    expect(plan.odds!.low.toFixed(1)).toBe(plan.odds!.high.toFixed(1));
    expect(odds.value).toBe(`${plan.odds!.mid.toFixed(1)} to 1`);
    expect(odds.effect).toBe(`About ${plan.odds!.mid.toFixed(1)} to 1 all day`);
  });
});
