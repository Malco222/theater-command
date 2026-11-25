// Carrier strikes on a harbour by the player's own hand (release 2, harbourstrike.ts; Brian 24 November 2026 21:45Z,
// "Resume the Pacific for release 2"; the 19 November Pacific playtest's first finding). Played on the whole-world
// chart as Japan, whose Mobile Fleet lies at Kure, out of its aircraft's reach of Pearl Harbor: its page names the
// base from which they reach it, says war must be declared first, and in the month war is declared one order sails
// it into range and sends its wings in, which go in without warning. Behind its own switch.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { beforeAll, describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { setDesk } from '../src/campaign/depth';
import { createGame } from '../src/campaign/game';
import { carrierStrikeOn, setCarrierStrike, strikeHarbours, whyNotCarrierStrike } from '../src/campaign/harbourstrike';
import { atSea, baseTargets, inPassage, keptByHand, rebase, shipCount, voyageMonths } from '../src/campaign/navy';
import { monthCard, monthMark } from '../src/campaign/monthcard';
import { loadGame, saveGame } from '../src/campaign/save';
import { CARRIERSTRIKE_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, orderCarrierStrikeFleet, orderDeclare, runTurnAI, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type TCApi = { load: (t: string) => unknown; act: (a: string) => { ok: boolean; note: string }; page: (id: string) => { sections: { heading: string; rows: { title: string; lines: string[]; buttons: { label: string; act: string; ok: boolean; tip: string }[] }[] }[] } | null };
const TC = (globalThis as unknown as { TC: TCApi }).TC;

function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  beginHuman(g);
}
/** Japan at the player's month of November 1941 (month 27), running its navy and air force by hand. */
function japan(on = true, staff = false): Game {
  const g = startOpen(createGame(17, 'JAP'));
  for (let k = 0; !humanUp(g) && k < 400; k++) stepAI(g);
  setDesk(g, 'navy', staff);
  setDesk(g, 'air', staff);
  setCarrierStrike(g, on);
  beginHuman(g);
  while (g.turn < 27) month(g);
  return g;
}
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const atPearl = (g: Game): number => g.fleets.filter((f) => f.base === g.byName['Pearl Harbor'].id && !atSea(f)).reduce((n, f) => n + shipCount(f.ships) - f.ships.ss, 0);

let base: string;
beforeAll(() => { base = saveGame(japan()); }, 300000);
const copy = (): Game => loadGame(base)!;

describe('the switch', () => {
  it('ships off, and off nothing is offered', () => {
    expect((stage as { carrierStrike?: boolean }).carrierStrike).toBe(false);
    expect(CARRIERSTRIKE_STAGE).toBe(false);
    const g = copy();
    setCarrierStrike(g, false);
    expect(carrierStrikeOn(g)).toBe(false);
    const mf = fleet(g, 'JAP', 'Mobile Fleet');
    expect(strikeHarbours(g, mf.id, () => true)).toEqual([]);
    expect(whyNotCarrierStrike(g, mf.id, g.byName['Pearl Harbor'].id)).toMatch(/switched on/);
    expect(orderCarrierStrikeFleet(g, mf.id, g.byName['Pearl Harbor'].id)).toBeNull();
  });
  it('is saved with the campaign, and switched from the options', () => {
    const g = copy();
    expect(loadGame(saveGame(g))!.flags.carrierStrike).toBe(1);
    TC.load(saveGame(g));
    expect(TC.act('carrierstrikerule:off').ok).toBe(true);
    expect(TC.act('carrierstrikerule:on').note).toMatch(/Carrier strikes on harbours are on/);
  });
});

describe('Pearl Harbor from Kure, at peace', () => {
  it('names the base its aircraft would reach it from, and says to declare war first', () => {
    const g = copy();
    const mf = fleet(g, 'JAP', 'Mobile Fleet'), pearl = g.byName['Pearl Harbor'];
    expect(g.regions[mf.base].name).toBe('Hiroshima');
    expect(strikeHarbours(g, mf.id)).toEqual([]); // (nobody he may declare on is offered unless the page says so)
    const h = strikeHarbours(g, mf.id, (nat) => nat === 'USA').find((x) => x.region === pearl.id)!;
    expect(h).toBeTruthy();
    expect([h.atWar, h.launch]).toEqual([false, -1]);
    expect(h.forward).toBeGreaterThanOrEqual(0);
    expect(h.fleets.join(' ')).toMatch(/Pacific Fleet/);
    expect(h.why).toMatch(/at peace with the United States: declare war first/);
  });
  it('on the fleet page: the harbour, what lies there, and the order to move its base forward', () => {
    const g = copy();
    TC.load(saveGame(g));
    const mf = fleet(g, 'JAP', 'Mobile Fleet');
    const sec = TC.page(`fleet:${mf.id}`)!.sections.find((x) => x.heading === 'ENEMY HARBOURS TO STRIKE')!;
    const row = sec.rows.find((r) => r.title.startsWith('Pearl Harbor'))!;
    expect(row.lines.join(' ')).toMatch(/At anchor: the Pacific Fleet/);
    expect(row.lines.join(' ')).toMatch(/Declare war first/);
    expect(row.buttons[0].label).toMatch(/^MOVE BASE TO /);
    expect(TC.act(row.buttons[0].act).ok).toBe(true);
  });
  it('in passage to a new base, the move forward is barred, with the reason (release 2, the far chart)', () => {
    const g = copy();
    const mf = fleet(g, 'JAP', 'Mobile Fleet');
    const far = baseTargets(g, mf).find((r) => voyageMonths(g, mf, r) > 0)!;
    expect(rebase(g, mf, far)).toBe(true);
    expect(inPassage(mf)).toBe(true);
    TC.load(saveGame(g));
    const moves = TC.page(`fleet:${mf.id}`)!.sections.flatMap((x) => x.rows.flatMap((r) => r.buttons)).filter((b) => /^MOVE BASE TO /.test(b.label));
    expect(moves.length).toBeGreaterThan(0);
    for (const b of moves) {
      expect(b.ok).toBe(false);
      expect(b.tip).toMatch(/is in passage to /);
    }
  });
});

describe('the strike, in the month war is declared', () => {
  let after: string, sent: Game;
  beforeAll(() => {
    const g = copy();
    const mf = fleet(g, 'JAP', 'Mobile Fleet'), pearl = g.byName['Pearl Harbor'];
    const h = strikeHarbours(g, mf.id, (nat) => nat === 'USA').find((x) => x.region === pearl.id)!;
    TC.load(saveGame(g));
    expect(TC.act(`fleetstrikebase:${mf.id}:${h.forward}`).ok).toBe(true);
    const at = loadGame((TC as unknown as { save: () => { text: string } }).save().text)!;
    month(at);
    expect(orderDeclare(at, 'USA')).toBe(true);
    sent = at;
    after = saveGame(at);
  }, 300000);
  it('is offered once at war and in reach, and one order sails the fleet and sends its wings in', () => {
    const g = loadGame(after)!;
    const mf = fleet(g, 'JAP', 'Mobile Fleet'), pearl = g.byName['Pearl Harbor'];
    expect(whyNotCarrierStrike(g, mf.id, pearl.id)).toBeNull();
    const done = orderCarrierStrikeFleet(g, mf.id, pearl.id)!;
    expect(done).toBeTruthy();
    expect([atSea(mf), mf.zone]).toEqual([true, done.zone]);
    expect(keptByHand(g, mf)).toBe(true);
    expect(g.airFleets.filter((w) => done.wings.includes(w.id)).every((w) => w.mission === 'harbour' && w.target === pearl.id)).toBe(true);
  });
  it('goes in without warning with the month\'s air war, and sinks ships at anchor', () => {
    const g = loadGame(after)!;
    const mf = fleet(g, 'JAP', 'Mobile Fleet'), pearl = g.byName['Pearl Harbor'];
    const before = atPearl(g);
    const done = orderCarrierStrikeFleet(g, mf.id, pearl.id)!;
    expect(done).toBeTruthy();
    const strikers = done.wings.filter((id) => (g.airFleets.find((w) => w.id === id)?.bmr ?? 0) > 0).length;
    const mark = monthMark(g);
    month(g);
    expect(g.headlines.some((x) => /raid Pearl Harbor without warning/.test(x.text))).toBe(true);
    expect(atPearl(g)).toBeLessThan(before);
    // The strike is booked on the fleets struck at anchor, for their staff's weighing of staying against moving (the
    // design director's ruling of 04:33Z, 25 November 2026, 2): from the decks it was flown from.
    const hit = g.fleets.filter((f) => f.nat === 'USA' && f.anchor?.some((e) => e.at === pearl.id && e.t === g.turn - 1));
    expect(hit.length).toBeGreaterThan(0);
    expect(hit.every((f) => f.anchor!.every((e) => e.from.length > 0 && e.from.every((o) => o.sea >= 0 && o.km > 0)))).toBe(true);
    // Only the groups that carried bombs: their fighters reach no further than the bombers they guard.
    expect(hit.every((f) => f.anchor!.every((e) => e.from.length <= strikers))).toBe(true);
    // The month card names the place once (the playtest's #49): "Pearl Harbor", never "Pearl Harbor harbour".
    const card = JSON.stringify(monthCard(g, mark));
    expect(card).toMatch(/Pearl Harbor/);
    expect(card).not.toMatch(/Harbor harbour/);
  }, 300000);
  it('once at war, the fleets at anchor are named, or counted where only reported: never the same words twice', () => {
    const g = loadGame(after)!;
    const mf = fleet(g, 'JAP', 'Mobile Fleet'), pearl = g.byName['Pearl Harbor'];
    const h = strikeHarbours(g, mf.id).find((x) => x.region === pearl.id)!;
    expect(h.fleets.length).toBeGreaterThan(0);
    expect(new Set(h.fleets).size).toBe(h.fleets.length);
    for (const f of h.fleets) expect(f).toMatch(/^(the |[A-Z][a-z]+ ships reported$|(two|three|four|five) [A-Z][a-z]+ fleets reported$)/);
  });
  it('the page offers STRIKE FROM THE AIR, and its act gives the order', () => {
    TC.load(after);
    const mf = fleet(sent, 'JAP', 'Mobile Fleet');
    const sec = TC.page(`fleet:${mf.id}`)!.sections.find((x) => x.heading === 'ENEMY HARBOURS TO STRIKE')!;
    const row = sec.rows.find((r) => r.title.startsWith('Pearl Harbor'))!;
    expect(row.buttons[0]).toMatchObject({ label: 'STRIKE FROM THE AIR', ok: true });
    const r = TC.act(row.buttons[0].act);
    expect(r.ok).toBe(true);
    expect(r.note).toMatch(/its aircraft will strike Pearl Harbor/);
    expect(TC.act(row.buttons[0].act).ok).toBe(true); // (the same order again changes nothing: it is already given)
  });
  it('refuses a fleet with no carriers, and the computer\'s fleets', () => {
    const g = loadGame(after)!;
    const pearl = g.byName['Pearl Harbor'];
    expect(strikeHarbours(g, fleet(g, 'JAP', '2nd Fleet').id, () => true)).toEqual([]);
    expect(whyNotCarrierStrike(g, fleet(g, 'JAP', '2nd Fleet').id, pearl.id)).toMatch(/no carriers/);
    expect(whyNotCarrierStrike(g, fleet(g, 'USA', 'Pacific Fleet').id, pearl.id)).toMatch(/Only your own fleet/);
  });
});

describe('the computer\'s wars', () => {
  it('are the same with it on: no staff reads it', () => {
    const a = startOpen(createGame(5, null)), b = startOpen(createGame(5, null));
    setCarrierStrike(b, true);
    for (let m = 0; m < 3; m++) { runTurnAI(a); runTurnAI(b); }
    delete b.flags.carrierStrike;
    expect(saveGame(b)).toBe(saveGame(a));
  }, 300000);
});
