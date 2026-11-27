// Double agents (Brian, 22 November 2026, release 2, yes (3): double agents, with a telltale sign so a false report is
// never unfair), behind the switch secret.doubles ('sw:double'). Both ways: the player turns an enemy agent his
// hunters uncovered, and the enemy turns one of his agents abroad, which then gives itself away by reports too good
// to be true, sabotage that leaves no new damage, books that show a small army, and losses among the agents beside it.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame, fieldArmy, setWar } from '../src/campaign/game';
import {
  INTEL, TURNED_HELP, armyKnown, arrest, booksOf, caughtChance, doublesOf, foesFound, guardChance, intelTick, knowsBooks, riskNow, spyRoom, turnFoe, whyNotTurn,
} from '../src/campaign/intel';
import { fieldOrder } from '../src/campaign/air';
import { onNet, railCut, ruinedAt, workingFactories } from '../src/campaign/rails';
import { doublesOn, setSecretWar } from '../src/campaign/secretwar';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, orderTurn, stepAI, whyNotTurnAgent } from '../src/campaign/turn';
import { loadGame, saveGame } from '../src/campaign/save';
import type { Agent, Game, Mission } from '../src/campaign/types';

const free = (seed = 7, human: string | null = 'UK'): Game => startOpen(createGame(seed, human));
const keep = structuredClone({ enemy: INTEL.enemy, missions: INTEL.missions, secret: INTEL.secret });
afterEach(() => {
  Object.assign(INTEL.enemy, structuredClone(keep.enemy));
  Object.assign(INTEL.missions, structuredClone(keep.missions));
  Object.assign(INTEL.secret, structuredClone(keep.secret));
});
/** The double agents' numbers, as they stand now (the hook above puts back a fresh copy after each test). */
const D = new Proxy({} as typeof INTEL.secret.doubles, { get: (_, k) => (INTEL.secret.doubles as any)[k], set: (_, k, v) => ((INTEL.secret.doubles as any)[k] = v, true) });
const plant = (g: Game, at: string | number, mission: Mission, nat = 'UK'): Agent => {
  const a: Agent = { id: g.intel.next++, nat, name: `Test ${g.intel.next}`, xp: 0, reg: typeof at === 'number' ? at : g.byName[at].id, ready: g.turn, mission, born: g.turn };
  g.intel.agents.push(a);
  return a;
};
/** Turned by the given service a month ago, as the enemy would have done it. */
const turned = (a: Agent, by = 'GER', g?: Game): Agent => { a.turned = (g?.turn ?? 1) - 1; a.turnedFor = by; return a; };
/** Britain's player at war with Germany, the enemy's services quiet unless a test wants them. */
function scene(seed = 7): Game {
  const g = free(seed);
  setWar(g, 'UK', 'GER');
  INTEL.enemy.chance = 0;
  // (And no watchers, release 2, unless a test wants them: tests/watchers.test.ts.)
  INTEL.secret.watchers.chance = 0;
  return g;
}
/** Every month the other side's services plot, at once. */
const busy = (g: Game): void => { INTEL.enemy.chance = 1; INTEL.enemy.quiet = 0; g.flags.spyfrom = 0; };
/** A German province with a working factory and a railway, not the capital. */
const target = (g: Game) => g.regions.find((r) => r.ctrl === 'GER' && r.owner === 'GER' && !r.cap && workingFactories(g, r) > 0 && onNet(r.id) && !railCut(g, r.id))!;

describe('the switch', () => {
  it('is on for a new grand campaign and keeps with the save; a campaign begun with it off has none', () => {
    const g = free();
    expect(doublesOn(g)).toBe(true);
    expect(doublesOn(loadGame(saveGame(g))!)).toBe(true);
    D.on = false;
    expect(doublesOn(free())).toBe(false);
    // A historical scenario never has it.
    const h = createGame(7, 'UK');
    setSecretWar(h, 'double', true);
    expect(doublesOn(h)).toBe(false);
  });
});

describe('you turn theirs', () => {
  it('an uncovered enemy agent can be turned instead of arrested, and no more than "most" in one service', () => {
    const g = scene();
    const London = g.byName.London.id;
    g.intel.foes = [
      { id: 1, nat: 'GER', reg: London, kind: 'factory', strike: g.turn + 2, found: g.turn },
      { id: 2, nat: 'GER', reg: London, kind: 'rail', strike: g.turn + 2, found: g.turn },
      { id: 3, nat: 'GER', reg: London, kind: 'rail', strike: g.turn + 2 },
    ];
    expect(whyNotTurn(g, 'UK', 3)).toBe('Nobody knows where that agent is.');
    expect(whyNotTurn(g, 'UK', 1)).toBeNull();
    const d = turnFoe(g, 'UK', 1)!;
    expect(d).toMatchObject({ of: 'GER', by: 'UK', since: g.turn });
    expect(INTEL.names).toContain(d.name);
    expect(g.intel.foes.map((f) => f.id)).toEqual([2, 3]);
    expect(doublesOf(g).map((x) => x.name)).toEqual([d.name]);
    expect(g.headlines.at(-1)!.text).toBe(`The German agent at London is turned. As double agent ${d.name} they go back to the German service and work for you.`);
    // One a service: the next can only be arrested.
    expect(whyNotTurn(g, 'UK', 2)).toBe('You already run a double agent in the German service. Arrest this one.');
    expect(turnFoe(g, 'UK', 2)).toBeNull();
    D.most = 2;
    expect(whyNotTurn(g, 'UK', 2)).toBeNull();
    D.most = 1;
    expect(arrest(g, 'UK', 2)).toBe(true);
    // The room lists it: name, which service, since when.
    const room = spyRoom(g)!;
    expect(room.doubles).toEqual([{ id: d.id, name: d.name, of: 'GER', since: g.turn, text: expect.stringMatching(new RegExp(`^Double agent ${d.name}, in the German service since `)) }]);
  });

  it('the order goes through the player\'s own month (TURN in the browser and the 3D game)', () => {
    const g = free(7, 'UK');
    for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
    beginHuman(g);
    setWar(g, 'UK', 'GER');
    g.intel.foes = [{ id: 1, nat: 'GER', reg: g.byName.London.id, kind: 'factory', strike: g.turn + 2, found: g.turn }];
    expect(whyNotTurnAgent(g, 1)).toBeNull();
    expect(orderTurn(g, 1)).toBe(true);
    expect(g.intel.doubles!.length).toBe(1);
    expect(orderTurn(g, 1)).toBe(false);
  });

  it("a double warns of every new agent its service sends, the month it arrives, and opens that country's books", () => {
    const g = scene();
    setWar(g, 'UK', 'HUN');
    D.blown = 0;
    expect(knowsBooks(g, 'GER', 'UK')).toBe(false);
    g.intel.doubles = [{ id: 1, name: 'Nightjar', of: 'GER', by: 'UK', since: g.turn }];
    expect(knowsBooks(g, 'GER', 'UK')).toBe(true);
    const b = booksOf(g, 'GER', 'UK')!;
    expect(b.agent).toBe('Nightjar');
    expect(b.double).toBe(true);
    expect(b.divisions).toBe(fieldArmy(g, 'GER'));
    expect(armyKnown(g, 'GER', 'UK')).toEqual({ divs: fieldArmy(g, 'GER'), exact: true });
    expect(spyRoom(g)!.books).toContain('GER');
    // Only the books of the service it is in.
    expect(knowsBooks(g, 'HUN', 'UK')).toBe(false);
    busy(g);
    INTEL.secret.everyone.smaller = 1;
    const before = g.headlines.length;
    intelTick(g, 'UK');
    const ger = g.intel.foes!.filter((f) => f.nat === 'GER'), hun = g.intel.foes!.filter((f) => f.nat === 'HUN');
    expect(ger.length).toBe(1);
    expect(ger[0].found).toBe(g.turn);
    expect(foesFound(g).map((f) => f.id)).toContain(ger[0].id);
    expect(g.headlines.slice(before).map((h) => h.text)).toContain(`Double agent Nightjar warns of a new German agent at ${g.regions[ger[0].reg].name}.`);
    // A service with no double of his in it stays hidden.
    expect(hun.length).toBe(1);
    expect(hun[0].found).toBeUndefined();
  });

  it('a double is blown with chance "blown", and ends quietly at peace; nothing more is shown', () => {
    const g = scene();
    g.intel.doubles = [{ id: 1, name: 'Nightjar', of: 'GER', by: 'UK', since: g.turn }];
    D.blown = 0;
    intelTick(g, 'UK');
    expect(doublesOf(g).length).toBe(1);
    D.blown = 1;
    const before = g.headlines.length;
    intelTick(g, 'UK');
    expect(doublesOf(g)).toEqual([]);
    expect(g.headlines.slice(before).map((h) => h.text)).toEqual(['German intelligence has stopped trusting double agent Nightjar. The agent is of no more use.']);
    expect(knowsBooks(g, 'GER', 'UK')).toBe(false);
    // At peace it ends without a word.
    const p = scene();
    p.intel.doubles = [{ id: 1, name: 'Nightjar', of: 'GER', by: 'UK', since: p.turn }];
    D.blown = 0;
    p.nations.UK.atWar = p.nations.UK.atWar.filter((x) => x !== 'GER');
    p.nations.GER.atWar = p.nations.GER.atWar.filter((x) => x !== 'UK');
    expect(knowsBooks(p, 'GER', 'UK')).toBe(false);
    // From the moment of peace, before the month's end, the room no longer lists it or says it warns of anything.
    expect(doublesOf(p)).toEqual([]);
    expect(spyRoom(p)!.doubles).toEqual([]);
    // Likewise when its service's country is gone.
    const q = scene();
    q.intel.doubles = [{ id: 1, name: 'Nightjar', of: 'GER', by: 'UK', since: q.turn }];
    expect(doublesOf(q).length).toBe(1);
    q.nations.GER.alive = false;
    expect(doublesOf(q)).toEqual([]);
    expect(spyRoom(q)!.doubles).toEqual([]);
    const quiet = p.headlines.length;
    intelTick(p, 'UK');
    expect(p.intel.doubles).toBeUndefined();
    expect(p.headlines.slice(quiet).some((h) => /Nightjar/.test(h.text))).toBe(false);
  });
});

describe('they turn yours, with a tell', () => {
  it('an agent abroad that would be caught by a service at war with him is turned instead, with chance "share"; nothing is said', () => {
    INTEL.missions.watch.caught = 1;
    D.share = 1;
    let found = 0;
    for (let seed = 1; seed <= 12; seed++) {
      const g = scene(seed);
      const a = plant(g, 'Ruhr', 'watch');
      const notes = g.intel.notes.length, news = g.headlines.length;
      intelTick(g, 'UK');
      if (!a.turned) continue;
      found++;
      expect(g.intel.agents).toContain(a);
      expect(a.turnedFor).toBe('GER');
      expect(a.turned).toBe(g.turn);
      expect(g.intel.notes.slice(notes).some((n) => n.agent === a.name)).toBe(false);
      expect(g.headlines.slice(news).some((h) => h.text.includes(a.name))).toBe(false);
      // Never caught while it works for them.
      for (let k = 0; k < 6; k++) { g.turn++; intelTick(g, 'UK'); }
      expect(g.intel.agents).toContain(a);
      // With share 0 the same draw takes it, as before.
      const h = scene(seed);
      const b = plant(h, 'Ruhr', 'watch');
      D.share = 0;
      intelTick(h, 'UK');
      expect(h.intel.agents).not.toContain(b);
      expect(h.headlines.at(-1)!.text).toBe(`Agent ${b.name} is caught at the Ruhr by German counter-intelligence. Nothing more is heard.`);
      D.share = 1;
    }
    expect(found).toBeGreaterThan(5);
    // A country at peace with him does not turn his agent: it catches it.
    let lost = 0;
    for (let seed = 1; seed <= 12; seed++) {
      const g = scene(seed);
      const s = g.regions.find((r) => r.cap && r.ctrl === 'SWE')!;
      const a = plant(g, s.id, 'watch');
      intelTick(g, 'UK');
      expect(a.turned).toBeUndefined();
      if (!g.intel.agents.includes(a)) lost++;
    }
    expect(lost).toBeGreaterThan(0);
  });

  it("a turned agent's books show the army 'lie' smaller, unless a loyal agent reads them too", () => {
    const g = scene();
    const berlin = g.nations.GER.capital;
    const a = turned(plant(g, berlin, 'books'), 'GER', g);
    const truth = fieldArmy(g, 'GER');
    expect(knowsBooks(g, 'GER', 'UK')).toBe(true);
    expect(armyKnown(g, 'GER', 'UK')).toEqual({ divs: truth * (1 - D.lie), exact: true });
    expect(booksOf(g, 'GER', 'UK')!.divisions).toBeCloseTo(truth * (1 - D.lie), 9);
    expect(booksOf(g, 'GER', 'UK')!.agent).toBe(a.name);
    // A loyal reader's true figures win.
    plant(g, berlin, 'books');
    expect(armyKnown(g, 'GER', 'UK')).toEqual({ divs: truth, exact: true });
    expect(booksOf(g, 'GER', 'UK')!.divisions).toBe(truth);
  });

  it("the lie starts the month the tell does: in the month it is turned, its books still show the truth", () => {
    INTEL.missions.books.caught = 1;
    D.share = 1;
    let found = 0;
    for (let seed = 1; seed <= 12; seed++) {
      const g = scene(seed);
      const a = plant(g, g.nations.GER.capital, 'books');
      intelTick(g, 'UK');
      if (a.turned === undefined) continue;
      found++;
      expect(a.turned).toBe(g.turn);
      const truth = fieldArmy(g, 'GER');
      // No tell yet, and no lie.
      expect(g.intel.notes.some((n) => n.what === 'quiet')).toBe(false);
      expect(armyKnown(g, 'GER', 'UK')).toEqual({ divs: truth, exact: true });
      expect(booksOf(g, 'GER', 'UK')!.divisions).toBe(truth);
      // The next month the tell is in the file, and the books lie.
      g.turn++;
      intelTick(g, 'UK');
      expect(g.intel.notes.some((n) => n.what === 'quiet' && n.agent === a.name && n.turn === g.turn)).toBe(true);
      const now = fieldArmy(g, 'GER');
      expect(armyKnown(g, 'GER', 'UK').divs).toBeLessThan(now);
      expect(booksOf(g, 'GER', 'UK')!.divisions).toBeCloseTo(now * (1 - D.lie), 9);
    }
    expect(found).toBeGreaterThan(3);
  });

  it("its wreck of an airfield says 'both' for two groups, and with no group on the field it claims nothing", () => {
    INTEL.missions.airfield.success = 1;
    const at = (g: Game, n: number): number => {
      const bremen = g.byName.Bremen.id;
      for (const f of g.airFleets) if (f.base === bremen) f.base = g.byName.Hanover.id;
      g.airFleets.filter((f) => f.nat === 'GER' && f.ship === undefined && f.ftr + f.bmr > 0).slice(0, n).forEach((f) => { f.base = bremen; });
      return bremen;
    };
    let both = 0;
    for (let seed = 1; seed <= 12; seed++) {
      const g = scene(seed);
      const bremen = at(g, 2);
      const a = turned(plant(g, bremen, 'airfield'), 'GER', g);
      intelTick(g, 'UK');
      const said = g.headlines.find((h) => h.text.startsWith(`Agent ${a.name}'s saboteurs wreck the field at Bremen.`));
      if (!said) continue;
      both++;
      expect(said.text).toBe(`Agent ${a.name}'s saboteurs wreck the field at Bremen. Both German air groups based there are grounded this month.`);
    }
    expect(both).toBeGreaterThan(5);
    for (let seed = 1; seed <= 12; seed++) {
      const g = scene(seed);
      const bremen = at(g, 0);
      const a = turned(plant(g, bremen, 'airfield'), 'GER', g);
      intelTick(g, 'UK');
      expect(g.headlines.some((h) => /wreck the field/.test(h.text))).toBe(false);
      // (The month's check stands it down to watching before it tries: an empty field is never claimed wrecked.)
      expect(a.mission).toBe('watch');
      expect(g.intel.notes.some((n) => n.agent === a.name && n.what === 'sabotage')).toBe(false);
    }
  });

  it('its sabotage is reported in the usual words and wrecks nothing: the map shows no damage', () => {
    INTEL.missions.factory.success = 1;
    INTEL.missions.rail.success = 1;
    for (const m of ['factory', 'rail'] as Mission[]) {
      const g = scene();
      const r = target(g);
      const works = workingFactories(g, r);
      const a = turned(plant(g, r.id, m), 'GER', g);
      intelTick(g, 'UK');
      const said = m === 'factory' ? `Agent ${a.name} wrecks a factory at ${r.name}.` : `Agent ${a.name} blows the railway at ${r.name}.`;
      expect(g.headlines.some((h) => h.text.startsWith(said))).toBe(true);
      expect(ruinedAt(g, r)).toBe(0);
      expect(workingFactories(g, r)).toBe(works);
      expect(railCut(g, r.id)).toBe(false);
      expect(g.intel.cuts?.[r.id]).toBeUndefined();
      // A loyal agent's does.
      INTEL.missions[m].caught = 0;
      const h = scene();
      const t = target(h);
      plant(h, t.id, m);
      intelTick(h, 'UK');
      expect(m === 'factory' ? ruinedAt(h, t) : +railCut(h, t.id)).toBe(1);
    }
  });

  it('it never finds an order: a stolen order is never false', () => {
    const g = scene();
    g.turn = 12;
    const field = g.byName.Ruhr.id;
    const b = g.airFleets.filter((f) => f.nat === 'GER' && f.bmr > 0 && f.ship === undefined).sort((x, y) => y.bmr - x.bmr)[0];
    b.base = field; b.bmr = Math.max(b.bmr, 300); b.ready = 1;
    INTEL.missions.orders.success = 1;
    const a = turned(plant(g, 'Ruhr', 'orders'), 'GER', g);
    const could = fieldOrder(g, 'GER', field);
    intelTick(g, 'UK');
    expect(g.intel.orders ?? []).toEqual([]);
    expect(g.headlines.map((h) => h.text)).toContain(`Agent ${a.name}, the Ruhr: no German raid is ordered from the field for next month.`);
    // (The scene would have given a loyal agent an order where the staff had one to give.)
    if (could) expect(could.target).toBeGreaterThanOrEqual(0);
  });

  it("betrayal: his other agents in its masters' country are caught 'betray' more often; the chance he is shown does not change", () => {
    const g = scene();
    setWar(g, 'UK', 'ITA');
    const sold = turned(plant(g, 'Ruhr', 'watch'), 'GER', g);
    const other = plant(g, 'Hanover', 'factory');
    const far = plant(g, g.nations.ITA.capital, 'watch');
    expect(riskNow(g, sold)).toBe(0);
    expect(riskNow(g, other)).toBeCloseTo(Math.min(0.95, caughtChance(g, other) * (1 + D.betray)), 12);
    expect(riskNow(g, far)).toBe(caughtChance(g, far));
    // The room shows the usual chance for all three, the turned one too.
    const room = spyRoom(g)!;
    for (const a of [sold, other, far]) expect(room.agents.find((x) => x.id === a.id)!.caught).toBe(caughtChance(g, a));
    expect(room.agents.find((x) => x.id === sold.id)!.caught).toBeGreaterThan(0);
    // Without the turned agent, the true chance is the one shown.
    sold.turnedFor = 'ITA';
    expect(riskNow(g, other)).toBe(caughtChance(g, other));
  });

  it('the tell: each month a turned agent files a quiet report too good to be true; a loyal one does not', () => {
    INTEL.missions.watch.caught = 0;
    const g = scene();
    const a = turned(plant(g, 'Ruhr', 'watch'), 'GER', g);
    const b = plant(g, 'Hanover', 'watch');
    for (let k = 0; k < 3; k++) { intelTick(g, 'UK'); g.turn++; }
    const quiet = g.intel.notes.filter((n) => n.what === 'quiet');
    expect(quiet.length).toBe(3);
    expect(quiet.every((n) => n.agent === a.name && n.text === `Agent ${a.name}, Ruhr: all quiet, and the German forces there are weaker than we feared.`)).toBe(true);
    expect(g.intel.notes.some((n) => n.agent === b.name)).toBe(false);
    // The room tells him, in plain words, that an agent can be turned and how to know it.
    expect(spyRoom(g)!.turnedHelp).toBe(TURNED_HELP);
    expect(TURNED_HELP).toMatch(/can be turned/);
    expect(TURNED_HELP).toMatch(/too good to be true/);
    // A pattern to weigh, not a certain sign: a loyal agent's wreck can be rebuilt the same month.
    expect(TURNED_HELP).toMatch(/sabotage that leaves no new damage on the map, month after month/);
    expect(TURNED_HELP).not.toMatch(/no mark/);
  });

  it('brought home, a turned agent is found out with chance "home" and arrested; meanwhile it guards nothing', () => {
    const g = scene();
    const a = turned(plant(g, 'London', 'guard'), 'GER', g);
    D.home = 0;
    expect(guardChance(g, 'UK', a.reg)).toEqual({ chance: 0, by: null });
    intelTick(g, 'UK');
    expect(g.intel.agents).toContain(a);
    D.home = 1;
    g.turn++;
    intelTick(g, 'UK');
    expect(g.intel.agents).not.toContain(a);
    expect(g.headlines.map((h) => h.text)).toContain(`Agent ${a.name} has been working for Germany. The agent is arrested.`);
    // The words say no more than that.
    const said = g.intel.notes.filter((n) => n.agent === a.name).map((n) => n.text).join(' ');
    expect(said).not.toMatch(/interrogat|execut|shot|hanged|tortur/i);
  });

  it('nothing turned is visible in the room: a turned agent looks exactly like a loyal one', () => {
    const g = scene();
    const a = plant(g, 'Ruhr', 'watch');
    plant(g, 'London', 'guard');
    const loyal = JSON.stringify(spyRoom(g));
    turned(a, 'GER', g);
    const sold = spyRoom(g)!;
    expect(JSON.stringify(sold)).toBe(loyal);
    expect(JSON.stringify(sold)).not.toMatch(/"turned"|"turnedFor"/);
  });
});

describe('with the switch off', () => {
  it('nothing can be turned, no agent of his is turned, and the month is as before', () => {
    const g = scene();
    setSecretWar(g, 'double', false);
    g.intel.foes = [{ id: 1, nat: 'GER', reg: g.byName.London.id, kind: 'factory', strike: g.turn + 2, found: g.turn }];
    expect(whyNotTurn(g, 'UK', 1)).toBe('Not in this campaign.');
    expect(turnFoe(g, 'UK', 1)).toBeNull();
    const room = spyRoom(g)!;
    expect(room.doubles).toBeUndefined();
    expect(room.turnedHelp).toBeUndefined();
    // A double left in the record by hand opens nothing.
    g.intel.doubles = [{ id: 1, name: 'Nightjar', of: 'GER', by: 'UK', since: g.turn }];
    expect(knowsBooks(g, 'GER', 'UK')).toBe(false);
    delete g.intel.doubles;
    // Caught is caught.
    INTEL.missions.watch.caught = 1;
    D.share = 1;
    for (let seed = 1; seed <= 8; seed++) {
      const h = scene(seed);
      setSecretWar(h, 'double', false);
      const a = plant(h, 'Ruhr', 'watch');
      intelTick(h, 'UK');
      expect(a.turned).toBeUndefined();
    }
    // With no agent turned and 'share' 0, a month with the switch on is the same as one with it off.
    INTEL.missions.watch.caught = 0.3;
    D.share = 0;
    D.blown = 0;
    for (let seed = 1; seed <= 6; seed++) {
      const on = scene(seed), off = scene(seed);
      setSecretWar(off, 'double', false);
      for (const x of [on, off]) { plant(x, 'Ruhr', 'watch'); plant(x, 'Hanover', 'watch'); plant(x, 'London', 'guard'); busy(x); }
      for (let k = 0; k < 4; k++) { intelTick(on, 'UK'); intelTick(off, 'UK'); on.turn++; off.turn++; }
      expect(JSON.stringify(on.intel)).toBe(JSON.stringify(off.intel));
      expect(on.headlines.map((h) => h.text)).toEqual(off.headlines.map((h) => h.text));
    }
  });
});
