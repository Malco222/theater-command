// Release 1.5.3: the fleet attack card's words (the 1.5.1 playtest, findings 25, 26 and 33). Words only, and the sea
// report's strength line: no rule reads any of them, and the card stays a forecast that throws no die.
//   25  a fleet the player's side has only reported keeps its name, its port and what it is about to itself, in every
//       refusal and on the card;
//   26  "1 ship", not "1 ships", on the card and in the month's sea forecast; the losses listed with "and" before the
//       last, and their range said plainly; a battleship or carrier the likely count rounds away, though it may well go
//       down, named on that line; and in the sea report, "The Battle Fleet was caught at anchor" (not "The the"), and each
//       side's strength summed before the sinkings; and the fleet's own name in the refusals, "Force H", not "The Force H";
//   33  odds whose two ends read the same are said once: "More than 10 to 1 all day", on the card, in the month's sea
//       forecast and in the browser's fleet orders; and odds that round to nothing are "less than 0.1", not "0.0".
import { describe, expect, it } from 'vitest';
import { fleetKnown } from '../src/campaign/carriers';
import { atWar, createGame, declareWar } from '../src/campaign/game';
import { detach } from '../src/campaign/fleets';
import { HEAVY_RISK, orderFleetAttack, planFleetAttack, whyNotFleetAttack, type FleetAttackPlan } from '../src/campaign/fleetattack';
import { setFog } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { NAVY, cutOff, fleetName, sail, zoneOf } from '../src/campaign/navy';
import { oddsSwing, oddsWord, planSeaBattle } from '../src/campaign/seaplan';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, fleetOptions, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
// (the browser client's source, read as text: its fleet orders cannot be drawn here)
import browser from '../src/campaign/ui/main.ts?raw';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
const fleet = (g: Game, name: string): Fleet => g.fleets.find((f) => f.name === name)!;
const Cap = (s: string): string => s[0].toUpperCase() + s.slice(1);

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
        const canada = reported.find((t) => t.nat === 'CAN')!;
        expect(canada).toBeDefined();
        const p = TC.page(`fleetattack:${mine.find((f) => f.name === 'Swinemunde Fleet')!.id}:${canada.id}`) as Page;
        expect(p.headline).toBe('Those Canadian ships are in harbour, out of reach of the Swinemunde Fleet from Swinemunde.');
        expect(p.note).toMatch(/against Canadian ships reported, in harbour\. /);
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
      expect(plan.notes.join(' ')).toContain('Those British ships may not be patrolling. If not, they have to be found first: about ');
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

  it('a fleet only reported at sea may not be patrolling, the card says, whatever it is about', () => {
    // (The 1.5.3 review) The map draws every fleet only reported at sea as patrolling, so the card does not say which it is.
    const maybe = `Those British ships may not be patrolling. If not, they have to be found first: about ${Math.round(NAVY.battle.catchRaider * 100)}% of the time. Not found, your fleet patrols in the North Sea.`;
    let checked = 0;
    for (let seed = 1; seed <= 6; seed++) {
      const seen: Record<string, { notes: string[]; worth: string; stance: string }> = {};
      for (const stance of ['raid', 'patrol'] as const) {
        const g = germany(seed);
        const battle = fleet(g, 'Battle Fleet'), home = fleet(g, 'Home Fleet');
        sail(g, home, zoneOf(g, 'nsea'), stance);
        home.seen = { GER: g.turn - 1 };
        const sq = detach(g, home, { dd: 2 })!;
        sail(g, sq, zoneOf(g, 'nsea'), 'patrol');
        sq.seen = { GER: g.turn };
        if (fleetKnown(g, home, 'GER') !== 'report') continue;
        // Identified, it is said as it is: not patrolling, or nothing.
        expect(planFleetAttack(g, battle.id, home.id, null)!.notes.filter((n) => / patrolling/.test(n))).toEqual(stance === 'raid'
          ? [`The Home Fleet is not patrolling, so it has to be found first: about ${Math.round(NAVY.battle.catchRaider * 100)}% of the time. Not found, your fleet patrols in the North Sea.`] : []);
        TC.load(saveGame(g));
        const p = TC.page(`fleetattack:${battle.id}:${home.id}`) as Page;
        seen[stance] = {
          notes: planFleetAttack(g, battle.id, home.id)!.notes,
          worth: p.sections.find((x) => x.heading === 'WORTH KNOWING')!.text,
          stance: (TC.view() as { fleets: { id: number; stance: string }[] }).fleets.find((x) => x.id === home.id)!.stance,
        };
      }
      if (!seen.raid || !seen.patrol) continue;
      checked++;
      expect(seen.raid.notes).toContain(maybe);
      expect(seen.raid.worth).toContain(maybe);
      expect(seen.patrol).toEqual(seen.raid);
      expect(seen.raid.stance).toBe('patrol');
    }
    expect(checked).toBeGreaterThan(0);
  });

  it('in harbour beside a fleet identified, the odds name the fleet identified and not the harbour', () => {
    // (The 1.5.3 review) A fleet in harbour is identified the month it is met, while another in the same port stays reported.
    let checked = 0;
    for (let seed = 1; seed <= 6; seed++) {
      const g = germany(seed);
      const battle = fleet(g, 'Battle Fleet'), home = fleet(g, 'Home Fleet');
      sail(g, home, home.home, 'port');
      g.regions[home.base].fort = { lv: 2, vs: [] };
      const sq = detach(g, home, { dd: 2 })!;
      sq.seen = { GER: g.turn };
      if (fleetKnown(g, home, 'GER') !== 'report' || fleetKnown(g, sq, 'GER') !== 'exact') continue;
      checked++;
      const row = (p: FleetAttackPlan, label: string): string => p.lines.find((l) => l.label === label)!.value;
      const plan = planFleetAttack(g, battle.id, home.id)!;
      expect(row(plan, 'In harbour')).toBe(`${Cap(fleetName(sq))} at anchor in harbour`);
      expect(row(plan, 'Shore batteries')).toBe('The harbour\'s guns');
      TC.load(saveGame(g));
      const p = TC.page(`fleetattack:${battle.id}:${home.id}`) as Page;
      for (const w of words(p)) {
        expect(w, p.id).not.toContain(home.port);
        expect(w, p.id).not.toContain(home.name);
      }
      // Sent at the squadron it has identified, the harbour is named as before; and so it is with the fog lifted.
      const known = planFleetAttack(g, battle.id, sq.id)!;
      expect(row(known, 'In harbour')).toBe(`${Cap(fleetName(sq))} at anchor at ${home.port}`);
      expect(row(known, 'Shore batteries')).toBe(`The guns of ${home.port}`);
      expect(row(planFleetAttack(g, battle.id, home.id, null)!, 'In harbour')).toBe(`The Home Fleet and ${fleetName(sq)} at anchor at ${home.port}`);
    }
    expect(checked).toBeGreaterThan(0);
  });

  it('the fleet\'s room lists a fleet only reported, in harbour and in reach, as "In harbour." and no more', () => {
    // UK from 17, seed 5: the German ships reported in harbour (the Battle Fleet at Wilhelmshaven) are in the Home Fleet's reach.
    TC.start({ human: 'UK', view: 'grand', from: 17, seed: 5 });
    const g = loadGame(TC.save().text)!;
    const v = TC.view() as { fleets: { id: number; nat: string; name: string; report: boolean }[] };
    const german = v.fleets.find((f) => f.report && f.nat === 'GER')!;
    const real = g.fleets.find((f) => f.id === german.id)!;
    expect(real.stance).toBe('port');
    const home = v.fleets.find((f) => f.nat === 'UK' && f.name === 'Home Fleet')!;
    const room = TC.page(`fleet:${home.id}`) as Page;
    const rows = room.sections.find((s) => s.heading === 'ENEMY FLEETS TO ATTACK')!.rows.filter((r) => r.buttons.some((b) => (b as { act?: string }).act === `open:fleetattack:${home.id}:${german.id}`));
    expect(rows.length).toBe(1);
    expect(rows[0].title).toBe('German ships reported');
    expect(rows[0].lines[0]).toBe('In harbour.');
    for (const w of words(room)) {
      expect(w).not.toContain(real.port);
      expect(w).not.toContain(real.name);
    }
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
  it('a fleet of one ship is "1 ship", on its row, in the forces line, in its room and in the sea report', () => {
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
    // (The 1.5.3 review) The fleet's room on the way to the card, and the Home Fleet's row to join it, say so too.
    expect((TC.page(`fleet:${made.made}`) as Page).note).toMatch(/^1 ship, readiness \d+%\. /);
    const join = (TC.page(`fleet:${home.id}`) as Page).sections.find((s) => s.heading === 'FLEETS TO JOIN')!.rows.find((r) => r.title === '1st Escort Fleet')!;
    expect(join.lines[0]).toMatch(/^1 ship\. [A-Z]/);
    expect((TC.page(`fleet:${home.id}`) as Page).note).toMatch(/\b\d+ ships, readiness /);
    // The forces line, where one ship goes in alone; and the sea report of the fight.
    const g = germany(3);
    setFog(g, false);
    const sq = detach(g, fleet(g, 'Battle Fleet'), { dd: 1 })!;
    const plan = planFleetAttack(g, sq.id, fleet(g, 'Home Fleet').id)!;
    expect(plan.lines.find((l) => l.label === 'Forces')!.value).toMatch(/^1 ship against \d+$/);
    const them = fleet(g, 'Home Fleet');
    sail(g, them, them.home, 'port');
    const out = orderFleetAttack(g, sq.id, them.id)!;
    expect(out.outcome).toBe('fought');
    expect(out.report!.lines.find((l) => l.label === 'Forces')!.value).toMatch(/^1 ship against \d+$/);
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

describe('26: the sea report of an attack in harbour', () => {
  it('"The Home Fleet was caught at anchor", and the strength before the battle is the strength the card gave', () => {
    for (let seed = 1; seed <= 6; seed++) {
      const g = germany(seed);
      setFog(g, false);
      const battle = fleet(g, 'Battle Fleet'), home = fleet(g, 'Home Fleet');
      sail(g, home, home.home, 'port');
      const plan = planFleetAttack(g, battle.id, home.id, null)!;
      const forecast = plan.lines.find((l) => l.label === 'Forces')!.effect!;
      const out = orderFleetAttack(g, battle.id, home.id)!;
      expect(out.outcome).toBe('fought');
      const lines = out.report!.lines;
      expect(lines.find((l) => l.label === 'In harbour')!.value).toBe(`The Home Fleet was caught at anchor at ${home.port}`);
      expect(lines.find((l) => l.label === 'Forces')!.effect).toBe(`${forecast} before the battle`);
      for (const l of lines) expect(`${l.value} ${l.effect ?? ''}`).not.toMatch(/\bthe the\b/i);
    }
    // A name with no article of its own reads bare: "Force H was caught at anchor" (the Home Fleet under that name).
    const g = germany(3);
    setFog(g, false);
    const battle = fleet(g, 'Battle Fleet'), home = fleet(g, 'Home Fleet');
    sail(g, home, home.home, 'port');
    home.name = 'Force H';
    const out = orderFleetAttack(g, battle.id, home.id)!;
    expect(out.report!.lines.find((l) => l.label === 'In harbour')!.value).toBe(`Force H was caught at anchor at ${home.port}`);
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

describe('26: the fleet\'s own name in the refusals', () => {
  it('"Force H has already been sent", "The Home Fleet has already been sent": each as the rest of the card names it', () => {
    // UK from 17, seed 5: Force H at Gibraltar, a name with no article of its own.
    TC.start({ human: 'UK', view: 'grand', from: 17, seed: 5 });
    const g = loadGame(TC.save().text)!;
    const h = fleet(g, 'Force H'), home = fleet(g, 'Home Fleet');
    const t = g.fleets.find((x) => x.nat === 'GER')!;
    h.attacked = g.turn;
    home.attacked = g.turn;
    expect(whyNotFleetAttack(g, h.id, t.id)).toBe('Force H has already been sent at the enemy this month.');
    expect(whyNotFleetAttack(g, home.id, t.id)).toBe('The Home Fleet has already been sent at the enemy this month.');
    expect(fleetOptions(g, h.id)!.held).toBe('Force H was sent at the enemy this month: it stays where the battle left it until next month.');
    expect(fleetOptions(g, home.id)!.held).toBe('The Home Fleet was sent at the enemy this month: it stays where the battle left it until next month.');
    TC.load(saveGame(g));
    const p = TC.page(`fleetattack:${h.id}:${t.id}`) as Page;
    expect(p.headline).toBe('Force H has already been sent at the enemy this month.');
    expect(p.top[0]).toMatchObject({ ok: false, tip: p.headline });
    // A fleet with no ships left.
    for (const c of ['bb', 'cv', 'ca', 'dd', 'ss'] as const) h.ships[c] = 0;
    expect(whyNotFleetAttack(g, h.id, t.id)).toBe('Force H has no ships.');
    for (const w of [...words(p), whyNotFleetAttack(g, h.id, t.id)!]) expect(w).not.toMatch(/\bThe Force H\b/i);
  });

  it('"The 2nd Fleet is cut off", and under a name of its own, "Force H is cut off"', () => {
    // Italy's 2nd Fleet past Gibraltar when Italy goes to war with Britain (the 1.5.2 case, tests/fleet-cut-off-152).
    const g = startOpen(createGame(3, 'ITA'));
    for (let k = 0; !humanUp(g) && k < 60; k++) stepAI(g);
    const f = g.fleets.find((x) => x.nat === 'ITA' && x.name === '2nd Fleet')!;
    expect(sail(g, f, zoneOf(g, 'iberia'), 'patrol')).toBe(true);
    if (!atWar(g, 'ITA', 'UK')) declareWar(g, 'ITA', 'UK');
    expect(cutOff(g, f)).toBe(true);
    const t = fleet(g, 'Home Fleet');
    expect(whyNotFleetAttack(g, f.id, t.id)).toBe('The 2nd Fleet is cut off beyond a shut strait: send it to harbour on its side first.');
    f.name = 'Force H';
    expect(whyNotFleetAttack(g, f.id, t.id)).toBe('Force H is cut off beyond a shut strait: send it to harbour on its side first.');
  });
});

describe('26: the month\'s sea forecast counts its ships as the card does', () => {
  it('"1 ship against 30", and "45 ships against 30"', () => {
    const g = startOpen(createGame(3, 'UK'));
    let guard = 0;
    while (!humanUp(g) && guard++ < 50) stepAI(g);
    setFog(g, false);
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    for (const f of g.fleets.filter((x) => x.nat === 'UK' && x.id !== home.id)) sail(g, f, f.home, 'port');
    const z = zoneOf(g, 'nsea');
    sail(g, battle, z, 'patrol');
    const forces = (id: number): string => planSeaBattle(g, id, z, null)!.lines.find((l) => l.label === 'Forces')!.value;
    expect(forces(home.id)).toMatch(/^\d+ ships against \d+$/);
    const sq = detach(g, home, { dd: 1 })!;
    sail(g, home, home.home, 'port');
    expect(forces(sq.id)).toMatch(/^1 ship against \d+$/);
  });
});

describe('33: the month\'s sea forecast says its odds once, as the card does', () => {
  it('"More than 10 to 1 all day", and the two ends where they differ', () => {
    const g = startOpen(createGame(3, 'UK'));
    let guard = 0;
    while (!humanUp(g) && guard++ < 50) stepAI(g);
    setFog(g, false);
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    for (const f of g.fleets.filter((x) => x.nat === 'UK' && x.id !== home.id)) sail(g, f, f.home, 'port');
    const z = zoneOf(g, 'nsea');
    sail(g, battle, z, 'patrol');
    const odds = (): { value: string; effect?: string } => planSeaBattle(g, home.id, z, null)!.lines.find((l) => l.label === 'Odds')!;
    expect(odds().effect).toMatch(/^Between \d+\.\d and \d+\.\d to 1 as the day goes$/);
    // Two German destroyers left patrolling there, the rest in harbour: the Home Fleet outweighs them past 10 to 1 all day.
    const sq = detach(g, battle, { dd: 2 })!;
    sail(g, battle, battle.home, 'port');
    sail(g, sq, z, 'patrol');
    expect(planSeaBattle(g, home.id, z, null)!.odds!.low).toBeGreaterThanOrEqual(9.95);
    expect(odds()).toEqual({ label: 'Odds', value: 'more than 10 to 1', effect: 'More than 10 to 1 all day' });
  });
});

describe('33: the browser\'s fleet orders say the forecast\'s odds as the card does', () => {
  it('"odds more than 10 to 1 all day", not "(over 10 to over 10)", and "less than 0.1", not "0.0"', () => {
    // (The 1.5.3 review) The month's sea forecast reaches the player in one line under each sea, on patrol (ui/main.ts seaOddsLine).
    const line = browser.slice(browser.indexOf('function seaOddsLine('), browser.indexOf('function fleetOrders('));
    expect(line).toContain('const swing = oddsSwing(p.odds.low, p.odds.high);');
    expect(line).toContain("swing.endsWith(' all day') ? lowerFirst(swing) : `${oddsWord(p.odds.mid)} to 1 (${oddsWord(p.odds.low)} to ${oddsWord(p.odds.high)})`");
    expect(line).not.toContain("'over 10'");
    expect(line).not.toContain('toFixed');
    // The band is said once exactly where oddsSwing ends "all day"; where its ends differ, both are given.
    for (const [lo, hi] of [[0.01, 0.02], [0.04, 0.06], [0.2, 0.24], [12, 18]]) expect(oddsSwing(lo, hi)).toMatch(/^[A-Z][a-z ]+[\d.]+ to 1 all day$/);
    for (const [lo, hi] of [[0.12, 0.18], [9, 13], [1.6, 2.4]]) expect(oddsSwing(lo, hi)).toMatch(/^Between .+ to 1 as the day goes$/);
  });
});

describe('33: odds that round to nothing read "less than 0.1", not "0.0"', () => {
  /** One British destroyer, alone, against the Battle Fleet made `times` its size and patrolling the North Sea. */
  function hopeless(times: number): { g: Game; sq: Fleet; battle: Fleet; z: number } {
    const g = startOpen(createGame(3, 'UK'));
    let guard = 0;
    while (!humanUp(g) && guard++ < 50) stepAI(g);
    beginHuman(g);
    setFog(g, false);
    const home = fleet(g, 'Home Fleet'), battle = fleet(g, 'Battle Fleet');
    const sq = detach(g, home, { dd: 1 })!;
    for (const f of g.fleets.filter((x) => x.nat === 'UK' && x.id !== sq.id)) sail(g, f, f.home, 'port');
    for (const c of ['bb', 'ca', 'dd'] as const) battle.ships[c] *= times;
    const z = zoneOf(g, 'nsea');
    sail(g, battle, z, 'patrol');
    return { g, sq, battle, z };
  }
  const odds = (lines: { label: string; value: string; effect?: string }[]): { value: string; effect?: string } => lines.find((l) => l.label === 'Odds')!;

  it('the card, its headline, the fleet\'s room and the month\'s sea forecast: "Less than 0.1 to 1 all day"', () => {
    const { g, sq, battle, z } = hopeless(10);
    const plan = planFleetAttack(g, sq.id, battle.id)!;
    expect(plan.why).toBeNull();
    expect(plan.odds!.high).toBeLessThan(0.05);
    expect(odds(plan.lines)).toMatchObject({ value: 'less than 0.1 to 1', effect: 'Less than 0.1 to 1 all day' });
    expect(plan.notes).toContain(`Your admiral would not offer battle at these odds (he wants ${NAVY.battle.seek} to 1 and has less than 0.1): it is your order, and it will be carried out.`);
    TC.load(saveGame(g));
    const p = TC.page(`fleetattack:${sq.id}:${battle.id}`) as Page;
    expect(p.headline).toMatch(/^Very likely to lose: less than 0\.1 to 1, about \d+% to win$/);
    expect(oddsRow(p).lines).toEqual(['less than 0.1 to 1', 'Less than 0.1 to 1 all day']);
    const room = TC.page(`fleet:${sq.id}`) as Page;
    const row = room.sections.find((s) => s.heading === 'ENEMY FLEETS TO ATTACK')!.rows.find((r) => r.buttons.some((b) => (b as { act?: string }).act === `open:fleetattack:${sq.id}:${battle.id}`))!;
    expect(row.lines).toEqual(['At sea, in the North Sea.', 'Very likely to lose: less than 0.1 to 1.']);
    // The month's sea forecast, for the destroyer on patrol there.
    sail(g, sq, z, 'patrol');
    const sea = planSeaBattle(g, sq.id, z, null)!;
    expect(sea.odds!.high).toBeLessThan(0.05);
    expect(odds(sea.lines)).toMatchObject({ value: 'less than 0.1 to 1', effect: 'Less than 0.1 to 1 all day' });
    expect(sea.notes[0]).toBe(`At these odds your admiral will not offer battle: he wants ${NAVY.battle.seek} to 1 and has less than 0.1. An enemy who is patrolling and thinks himself strong enough will come to you.`);
    for (const w of [...words(p), ...words(room), ...[plan, sea].flatMap((x) => [...x.notes, ...x.lines.flatMap((l) => [l.value, l.effect ?? ''])])]) {
      expect(w).not.toMatch(/\b0\.0\b/);
      expect(w).not.toMatch(/\b(about|between) less\b/i);
    }
  });

  it('where only the worse end rounds to nothing: "No better than 0.1 to 1 all day"', () => {
    const { g, sq, battle, z } = hopeless(3);
    const plan = planFleetAttack(g, sq.id, battle.id)!;
    expect(plan.odds!.low).toBeLessThan(0.05);
    expect(plan.odds!.high).toBeGreaterThanOrEqual(0.05);
    expect(odds(plan.lines).effect).toBe('No better than 0.1 to 1 all day');
    sail(g, sq, z, 'patrol');
    expect(odds(planSeaBattle(g, sq.id, z, null)!.lines).effect).toBe('No better than 0.1 to 1 all day');
  });

  it('the words for the odds, at every turn of the scale', () => {
    expect([0.01, 0.0499, 0.05, 0.24, 9.94, 9.95, 40].map(oddsWord)).toEqual(['less than 0.1', 'less than 0.1', '0.1', '0.2', '9.9', 'more than 10', 'more than 10']);
    expect(oddsSwing(0.01, 0.02)).toBe('Less than 0.1 to 1 all day');
    expect(oddsSwing(0.04, 0.06)).toBe('No better than 0.1 to 1 all day');
    expect(oddsSwing(0.2, 0.24)).toBe('About 0.2 to 1 all day');
    expect(oddsSwing(0.12, 0.18)).toBe('Between 0.1 and 0.2 to 1 as the day goes');
    expect(oddsSwing(9, 13)).toBe('Between 9.0 and more than 10 to 1 as the day goes');
    expect(oddsSwing(12, 18)).toBe('More than 10 to 1 all day');
  });
});
