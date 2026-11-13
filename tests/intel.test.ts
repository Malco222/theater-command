// Spies and the fog of war (the grand campaign). The player sees a foreign army whole only where his own side can
// see it, a flag and a count a province further off, and nothing beyond. Agents are pieces he recruits and sends
// abroad: they see for him, read a government's books in its capital, and try acts of sabotage, and every month
// abroad they may be caught. The computer's staffs see everything and keep no agents: their services strike at the
// player's home ground by a monthly chance, which his guards can stop. Every number is in data/campaign/intel.json.
// The historical scenarios know none of it.
import { describe, expect, it } from 'vitest';
import { DIFFICULTY, setLevel } from '../src/campaign/difficulty';
import { ledger } from '../src/campaign/economy';
import { allied, assess, createGame, fieldArmy, startPhase } from '../src/campaign/game';
import {
  INTEL, MISSIONS, MISSION_TEXT, SIGHT, abroad, agentAt, agentCost, agentsOf, armiesSeen, armyKnown, atWork, booksOf, caughtChance, cityReport, fogOn,
  guardChance, homeBase, intelAttack, intelRules, intelTick, knowsBooks, mostAgents, rankOf, recruit, retire, sendAgent, setFog, setMission, sightOf,
  skillOf, spyRoom, successChance, threat, whyNotMission, whyNotRecruit, whyNotSend,
} from '../src/campaign/intel';
import { onNet, railCut, ruinedAt, workingFactories } from '../src/campaign/rails';
import { fieldOut, fieldYield } from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  agentReport, beginHuman, endHuman, foreignBooks, humanUp, orderAgentMission, orderFog, orderRecruitAgent, orderRetireAgent, orderSendAgent, runTurnAI,
  secretService, sight, stepAI, visibleArmies, whyNotRecruitAgent, whyNotSendAgent,
} from '../src/campaign/turn';
import type { Agent, Game, Mission } from '../src/campaign/types';

const grand = (human: string | null = 'GER', seed = 3): Game => startOpen(createGame(seed, human));
const history = (human: string | null = 'GER', seed = 3): Game => createGame(seed, human);

function war(g: Game, a: string, b: string): void {
  if (!g.nations[a].atWar.includes(b)) g.nations[a].atWar.push(b);
  if (!g.nations[b].atWar.includes(a)) g.nations[b].atWar.push(a);
}
function peace(g: Game, a: string, b: string): void {
  g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b);
  g.nations[b].atWar = g.nations[b].atWar.filter((x) => x !== a);
}
/** Puts an agent straight to work in a province, with no training and no journey. */
function plant(g: Game, at: string, mission: Mission = 'watch', nat = 'GER', xp = 0): Agent {
  const a: Agent = { id: g.intel.next++, nat, name: `Test ${g.intel.next}`, xp, reg: g.byName[at].id, ready: g.turn, mission, born: g.turn };
  g.intel.agents.push(a);
  return a;
}
/** The player's next month, with every other nation's month played by the computer in between. */
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
}
/** A grand campaign with the player's first month open, as the game's own screen would have it. */
function playing(human = 'GER', seed = 3): Game {
  const g = grand(human, seed);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
const share = (hits: number, of: number): number => hits / of;

describe('the data', () => {
  it('names missions, ranks, levels and services that exist', () => {
    const g = grand();
    for (const m of MISSIONS) {
      expect(INTEL.missions[m], m).toBeTruthy();
      expect(INTEL.missions[m].caught, m).toBeGreaterThanOrEqual(0);
      expect(INTEL.missions[m].caught, m).toBeLessThan(0.5);
      expect(MISSION_TEXT[m].label.length, m).toBeGreaterThan(5);
      expect(MISSION_TEXT[m].text.length, m).toBeGreaterThan(30);
    }
    for (const m of ['factory', 'rail', 'oil'] as Mission[]) {
      expect(INTEL.missions[m].success, m).toBeGreaterThan(0.2);
      expect(INTEL.missions[m].success, m).toBeLessThan(0.9);
    }
    expect(INTEL.agents.ranks).toHaveLength(INTEL.agents.skillAt.length + 1);
    expect(INTEL.enemy.level).toHaveLength(DIFFICULTY.levels.length);
    // The easiest level has no saboteurs at all, and each harder one has more.
    expect(INTEL.enemy.level[0]).toBe(0);
    for (let k = 1; k < INTEL.enemy.level.length; k++) expect(INTEL.enemy.level[k]).toBeGreaterThan(INTEL.enemy.level[k - 1]);
    // Every service on today's map is a nation, and the great powers run more agents than a small country.
    expect(INTEL.enemy.services.filter((s) => g.nations[s]).length).toBeGreaterThanOrEqual(6);
    for (const s of INTEL.enemy.services) expect(mostAgents(s), s).toBeGreaterThan(mostAgents('SWE'));
    expect(mostAgents('SWE')).toBe(INTEL.agents.most._default);
    expect(new Set(INTEL.names).size).toBe(INTEL.names.length);
    expect(INTEL.names.length).toBeGreaterThanOrEqual(30);
    expect(INTEL.sight.glimpse).toBeGreaterThan(INTEL.sight.full);
  });
});

describe('a historical scenario', () => {
  it('has no fog, no agents and no secret war', () => {
    const g = history();
    expect(intelRules(g)).toBe(false);
    expect(fogOn(g)).toBe(false);
    expect([...sightOf(g)].every((s) => s === SIGHT.full)).toBe(true);
    expect(spyRoom(g)).toBeNull();
    expect(secretService(g)).toBeNull();
    expect(whyNotRecruit(g, 'GER')).toMatch(/grand campaign/);
    expect(recruit(g, 'GER')).toBeNull();
    expect(setFog(g, false)).toBe(false);
    expect(agentsOf(g, 'GER')).toEqual([]);
    expect(knowsBooks(g, 'FRA')).toBe(true);
    expect(armyKnown(g, 'FRA')).toEqual({ divs: fieldArmy(g, 'FRA'), exact: true });
    expect(intelAttack(g, 'GER', g.byName['Lodz'])).toBeNull();
    expect(threat(g, 'GER').chance).toBe(0);
    const flags = JSON.stringify(g.flags);
    intelTick(g, 'GER');
    expect(JSON.stringify(g.flags)).toBe(flags);
    expect(g.intel.notes).toEqual([]);
  });

  it('is saved exactly as it always was', () => {
    const g = history();
    for (let k = 0; k < 2; k++) runTurnAI(g);
    expect(Object.keys(JSON.parse(saveGame(g)))).not.toContain('intel');
    expect(armiesSeen(g).length).toBe(new Set(g.corps.filter((c) => !c.dead).map((c) => `${c.reg}:${c.nat}`)).size);
  });
});

describe('the fog of war', () => {
  it('shows the player its own side whole, a flag a province further off, and nothing beyond', () => {
    const g = grand();
    expect(fogOn(g)).toBe(true);
    const s = sightOf(g);
    const own = g.regions.filter((r) => allied(g, r.ctrl, 'GER'));
    for (const r of own) expect(s[r.id], r.name).toBe(SIGHT.full);
    // Every province beside the player's own side is seen whole, the next ring is glimpsed, and Moscow is dark.
    const near = new Set<number>();
    for (const r of g.regions) if (allied(g, r.ctrl, 'GER') || r.units.some((u) => allied(g, u.nat, 'GER'))) { near.add(r.id); for (const j of r.nb) near.add(j); }
    for (const id of near) expect(s[id], g.regions[id].name).toBe(SIGHT.full);
    const ring = g.regions.filter((r) => !near.has(r.id) && r.nb.some((j) => near.has(j)));
    expect(ring.length).toBeGreaterThan(10);
    for (const r of ring) expect(s[r.id], r.name).toBe(SIGHT.glimpse);
    expect(s[g.byName['Moscow'].id]).toBe(SIGHT.none);
    expect(s[g.byName['London'].id]).toBe(SIGHT.none);
    expect([...s].filter((x) => x === SIGHT.none).length).toBeGreaterThan(100);
    expect([...sight(g)]).toEqual([...s]);
  });

  it('lists only the armies the player knows of, and counts a glimpsed one without weighing it', () => {
    const g = grand();
    const s = sightOf(g);
    const seen = armiesSeen(g);
    expect(visibleArmies(g)).toEqual(seen);
    const all = new Set(g.corps.filter((c) => !c.dead).map((c) => `${c.reg}:${c.nat}`));
    expect(seen.length).toBeLessThan(all.size);
    for (const a of seen) {
      expect(all.has(`${a.reg}:${a.nat}`)).toBe(true);
      if (a.mine) { expect(a.sight).toBe(SIGHT.full); expect(a.divs).toBeGreaterThan(0); continue; }
      expect(s[a.reg]).toBeGreaterThan(SIGHT.none);
      if (s[a.reg] === SIGHT.glimpse) { expect(a.divs).toBeNull(); expect(a.corps).toBeGreaterThan(0); }
      else expect(a.divs).toBeGreaterThan(0);
    }
    expect(seen.some((a) => a.divs === null)).toBe(true);
    // Every one of the player's own armies is on the list, wherever it stands.
    const mine = new Set(g.corps.filter((c) => c.nat === 'GER' && !c.dead).map((c) => c.reg));
    for (const reg of mine) expect(seen.some((a) => a.reg === reg && a.nat === 'GER'), g.regions[reg].name).toBe(true);
    // No Soviet army deep in Russia is on it.
    expect(seen.some((a) => a.reg === g.byName['Moscow'].id)).toBe(false);
  });

  it('is not there with nobody at the table, for a fallen player, or once it is lifted', () => {
    const watch = grand(null);
    expect(fogOn(watch)).toBe(false);
    expect([...sightOf(watch)].every((x) => x === SIGHT.full)).toBe(true);
    expect(armiesSeen(watch).every((a) => a.divs !== null)).toBe(true);
    const g = grand();
    expect(orderFog(g, false)).toBe(true);
    expect(fogOn(g)).toBe(false);
    expect(g.flags.nofog).toBe(1);
    expect([...sightOf(g)].every((x) => x === SIGHT.full)).toBe(true);
    expect(armyKnown(g, 'SOV').exact).toBe(true);
    expect(orderFog(g, true)).toBe(true);
    expect(g.flags.nofog).toBeUndefined();
    expect(sightOf(g)[g.byName['Moscow'].id]).toBe(SIGHT.none);
    g.nations.GER.alive = false;
    expect(fogOn(g)).toBe(false);
  });

  it('gives a foreign army to the nearest ten divisions until its books are read', () => {
    const g = grand();
    const k = INTEL.estimate.round;
    const sov = armyKnown(g, 'SOV');
    expect(sov.exact).toBe(false);
    expect(sov.divs % k).toBe(0);
    expect(Math.abs(sov.divs - fieldArmy(g, 'SOV'))).toBeLessThanOrEqual(k / 2);
    expect(armyKnown(g, 'GER')).toEqual({ divs: fieldArmy(g, 'GER'), exact: true });
    const friend = Object.values(g.nations).find((n) => n.id !== 'GER' && n.alive && allied(g, n.id, 'GER'));
    if (friend) expect(armyKnown(g, friend.id).exact).toBe(true);
    // A small army is never rounded away to nothing.
    const small = Object.values(g.nations).filter((n) => n.alive && !allied(g, n.id, 'GER') && fieldArmy(g, n.id) > 0 && fieldArmy(g, n.id) < k / 2);
    for (const n of small) expect(armyKnown(g, n.id).divs, n.id).toBe(k);
  });

  it('is pushed back by an agent at work, and not by one still on the road', () => {
    const g = grand();
    const moscow = g.byName['Moscow'];
    const a = plant(g, 'Moscow');
    a.ready = g.turn + 1;
    expect(sightOf(g)[moscow.id]).toBe(SIGHT.none);
    a.ready = g.turn;
    const s = sightOf(g);
    expect(s[moscow.id]).toBe(SIGHT.full);
    for (const j of moscow.nb) expect(s[j], g.regions[j].name).toBe(SIGHT.full);
    for (const j of moscow.nb) for (const k of g.regions[j].nb) expect(s[k], g.regions[k].name).toBeGreaterThanOrEqual(SIGHT.glimpse);
    expect(armiesSeen(g).filter((x) => x.reg === moscow.id).every((x) => x.divs !== null)).toBe(true);
    // Another nation's agent is no use to the player.
    g.intel.agents.length = 0;
    plant(g, 'Moscow', 'watch', 'FRA');
    expect(sightOf(g)[moscow.id]).toBe(SIGHT.none);
  });
});

describe('recruiting', () => {
  it('costs money, takes a month of training and gives each agent a codename of its own', () => {
    const g = playing();
    const n = g.nations.GER;
    n.pp = 100;
    expect(whyNotRecruitAgent(g)).toBeNull();
    const a = orderRecruitAgent(g)!;
    expect(a).toBeTruthy();
    expect(n.pp).toBeCloseTo(100 - INTEL.agents.cost, 9);
    expect(agentCost()).toBe(Math.round(INTEL.agents.cost * 100));
    expect(INTEL.names).toContain(a.name);
    expect(a.reg).toBe(n.capital);
    expect(homeBase(g, 'GER')).toBe(n.capital);
    expect(a.ready).toBe(g.turn + INTEL.agents.train);
    expect(atWork(g, a)).toBe(false);
    expect(abroad(g, a)).toBe(false);
    expect(skillOf(a)).toBe(1);
    expect(rankOf(a)).toBe(INTEL.agents.ranks[0]);
    // In training it can be given no orders.
    expect(whyNotSendAgent(g, a.id, g.byName['Paris'].id)).toMatch(/in training/);
    expect(orderSendAgent(g, a.id, g.byName['Paris'].id)).toBe(false);
    expect(whyNotMission(g, 'GER', a.id, 'guard')).toMatch(/not yet at work/);
    const b = orderRecruitAgent(g)!;
    expect(b.name).not.toBe(a.name);
    expect(g.intel.notes.filter((x) => x.what === 'recruit')).toHaveLength(2);
    const room = secretService(g)!;
    expect(room.agents.map((x) => x.name)).toEqual([a.name, b.name]);
    expect(room.agents[0].doing).toMatch(/In training/);
    expect(room.agents[0].atWork).toBe(false);
    expect(room.most).toBe(mostAgents('GER'));
    expect(room.cost).toBe(agentCost());
  });

  it('stops at the size of the service and at an empty treasury', () => {
    const g = playing();
    const n = g.nations.GER;
    n.pp = 1000;
    for (let k = 0; k < mostAgents('GER'); k++) expect(orderRecruitAgent(g), `agent ${k}`).toBeTruthy();
    expect(whyNotRecruitAgent(g)).toMatch(/at once/);
    expect(orderRecruitAgent(g)).toBeNull();
    const first = agentsOf(g, 'GER')[0];
    expect(orderRetireAgent(g, first.id)).toBe(true);
    expect(agentsOf(g, 'GER')).toHaveLength(mostAgents('GER') - 1);
    n.pp = INTEL.agents.cost - 0.5;
    expect(whyNotRecruitAgent(g)).toMatch(/treasury/);
    n.pp = INTEL.agents.cost;
    expect(whyNotRecruitAgent(g)).toBeNull();
    expect(retire(g, 'GER', 9999)).toBe(false);
  });

  it('takes the player\'s orders only in the player\'s own month', () => {
    const g = grand();
    g.nations.GER.pp = 100;
    // Before the player's month has opened, another nation is up.
    g.phase = (g.order.indexOf('GER') + 1) % g.order.length;
    expect(humanUp(g)).toBe(false);
    expect(whyNotRecruitAgent(g)).toMatch(/own month/);
    expect(orderRecruitAgent(g)).toBeNull();
    const a = plant(g, 'Berlin', 'guard');
    expect(orderSendAgent(g, a.id, g.byName['Paris'].id)).toBe(false);
    expect(orderAgentMission(g, a.id, 'guard')).toBe(false);
    expect(orderRetireAgent(g, a.id)).toBe(false);
    const watch = grand(null);
    expect(whyNotRecruitAgent(watch)).toMatch(/Nobody/);
    expect(secretService(watch)).toBeNull();
  });

  it('trains a country with no capital of its own on an ally\'s ground, or nowhere', () => {
    const g = grand();
    const n = g.nations.GER;
    const cap = g.regions[n.capital];
    cap.ctrl = 'FRA';
    const home = homeBase(g, 'GER');
    expect(home).not.toBe(n.capital);
    expect(g.regions[home].ctrl).toBe('GER');
    for (const r of g.regions) if (r.ctrl === 'GER') r.ctrl = 'FRA';
    const last = homeBase(g, 'GER');
    if (last >= 0) expect(allied(g, g.regions[last].ctrl, 'GER')).toBe(true);
    else { n.pp = 100; expect(whyNotRecruit(g, 'GER')).toMatch(/nowhere/); }
  });
});

describe('sending an agent', () => {
  it('takes a month on the road, then it watches abroad or guards at home', () => {
    const g = playing();
    g.nations.GER.pp = 100;
    const a = orderRecruitAgent(g)!;
    nextMonth(g);
    expect(atWork(g, a)).toBe(true);
    expect(a.mission).toBe('guard');
    expect(whyNotSendAgent(g, a.id, a.reg)).toMatch(/already/);
    expect(whyNotSendAgent(g, a.id, 99999)).toMatch(/Pick/);
    const paris = g.byName['Paris'].id;
    expect(orderSendAgent(g, a.id, paris)).toBe(true);
    expect(a.reg).toBe(paris);
    expect(a.ready).toBe(g.turn + INTEL.agents.travel);
    expect(a.mission).toBe('watch');
    expect(atWork(g, a)).toBe(false);
    expect(abroad(g, a)).toBe(true);
    // On the road it sees nothing yet and can be given no new orders.
    expect(agentAt(g, paris)).toBeNull();
    expect(whyNotSendAgent(g, a.id, g.byName['Moscow'].id)).toMatch(/on the road/);
    expect(secretService(g)!.agents[0].doing).toMatch(/On the road to Paris/);
    nextMonth(g);
    if (!agentsOf(g, 'GER').length) return; // caught in its first month: the odds allow it
    expect(atWork(g, a)).toBe(true);
    expect(agentAt(g, paris)?.id).toBe(a.id);
    expect(sightOf(g)[paris]).toBe(SIGHT.full);
    // Recalled home it stands guard again.
    expect(orderSendAgent(g, a.id, g.nations.GER.capital)).toBe(true);
    expect(a.mission).toBe('guard');
    expect(abroad(g, a)).toBe(false);
  });
});

describe('an agent\'s work', () => {
  it('is offered by where the agent stands', () => {
    const g = playing();
    war(g, 'GER', 'FRA');
    peace(g, 'GER', 'SOV');
    const paris = plant(g, 'Paris');
    const moscow = plant(g, 'Moscow');
    const home = plant(g, 'Berlin', 'guard');
    // At home the only work is guarding.
    expect(whyNotMission(g, 'GER', home.id, 'guard')).toBeNull();
    for (const m of ['watch', 'books', 'factory', 'rail', 'oil'] as Mission[]) expect(whyNotMission(g, 'GER', home.id, m), m).toMatch(/friendly ground/);
    expect(whyNotMission(g, 'GER', paris.id, 'guard')).toMatch(/friendly ground/);
    // In an enemy capital: watch, read the books, wreck a factory, cut the railway; no oilfield.
    expect(whyNotMission(g, 'GER', paris.id, 'watch')).toBeNull();
    expect(whyNotMission(g, 'GER', paris.id, 'books')).toBeNull();
    expect(whyNotMission(g, 'GER', paris.id, 'factory')).toBeNull();
    expect(onNet(g.byName['Paris'].id)).toBe(true);
    expect(whyNotMission(g, 'GER', paris.id, 'rail')).toBeNull();
    expect(whyNotMission(g, 'GER', paris.id, 'oil')).toMatch(/no oilfield/);
    // In a country at peace: watching and reading only.
    expect(whyNotMission(g, 'GER', moscow.id, 'watch')).toBeNull();
    expect(whyNotMission(g, 'GER', moscow.id, 'books')).toBeNull();
    for (const m of ['factory', 'rail', 'oil'] as Mission[]) expect(whyNotMission(g, 'GER', moscow.id, m), m).toMatch(/at peace/);
    expect(setMission(g, 'GER', moscow.id, 'factory')).toBe(false);
    // The books are kept in a capital and nowhere else.
    const lyon = plant(g, 'Bordeaux');
    expect(whyNotMission(g, 'GER', lyon.id, 'books')).toMatch(/capital/);
    expect(whyNotMission(g, 'GER', 9999, 'watch')).toMatch(/no such agent/);
    // The room shows the same answers.
    const view = secretService(g)!.agents.find((x) => x.id === paris.id)!;
    expect(view.missions.map((m) => m.id)).toEqual(['watch', 'books', 'factory', 'rail', 'oil']);
    expect(view.missions.find((m) => m.id === 'oil')!.ok).toBe(false);
    expect(view.missions.find((m) => m.id === 'watch')!.now).toBe(true);
    expect(view.missions.find((m) => m.id === 'factory')!.success).toBeCloseTo(INTEL.missions.factory.success!, 9);
    expect(view.missions.find((m) => m.id === 'watch')!.success).toBeNull();
    expect(secretService(g)!.agents.find((x) => x.id === home.id)!.missions.map((m) => m.id)).toEqual(['guard']);
  });

  it('needs something to wreck: industry, a working line, a field that is not burning', () => {
    const g = playing();
    war(g, 'GER', 'FRA'); war(g, 'GER', 'ROM');
    const bare = g.regions.find((r) => r.ctrl !== 'GER' && g.nations[r.ctrl]?.alive && !allied(g, 'GER', r.ctrl) && r.fac === 0 && !onNet(r.id) && !r.oil)!;
    expect(bare, 'a foreign province with nothing in it').toBeTruthy();
    war(g, 'GER', bare.ctrl);
    const a = plant(g, bare.name);
    expect(whyNotMission(g, 'GER', a.id, 'factory')).toMatch(/no industry/);
    expect(whyNotMission(g, 'GER', a.id, 'rail')).toMatch(/No railway/);
    expect(whyNotMission(g, 'GER', a.id, 'oil')).toMatch(/no oilfield/);
    const oil = plant(g, 'Ploiesti');
    expect(whyNotMission(g, 'GER', oil.id, 'oil')).toBeNull();
    g.flags[`oilout:${g.byName['Ploiesti'].id}`] = g.turn + 2;
    g.flags[`oilcut:${g.byName['Ploiesti'].id}`] = 50;
    expect(whyNotMission(g, 'GER', oil.id, 'oil')).toMatch(/already burning/);
    const paris = plant(g, 'Paris');
    g.flags[`ruin:${g.byName['Paris'].id}`] = g.byName['Paris'].fac;
    expect(whyNotMission(g, 'GER', paris.id, 'factory')).toMatch(/left to wreck/);
    g.flags[`cut:${g.byName['Paris'].id}`] = g.turn;
    if (railCut(g, g.byName['Paris'].id)) expect(whyNotMission(g, 'GER', paris.id, 'rail')).toMatch(/already cut/);
    paris.rest = g.turn + 2;
    delete g.flags[`ruin:${g.byName['Paris'].id}`];
    expect(whyNotMission(g, 'GER', paris.id, 'factory')).toMatch(/lying low for 2 more months/);
    expect(whyNotMission(g, 'GER', paris.id, 'watch')).toBeNull();
    expect(whyNotMission(g, 'GER', paris.id, 'books')).toBeNull();
  });

  it('is riskier in a capital, in a country at war, on alert, and safer for a master', () => {
    const g = playing();
    peace(g, 'GER', 'FRA');
    const base = INTEL.missions.watch.caught;
    const a = plant(g, 'Bordeaux');
    expect(caughtChance(g, a)).toBeCloseTo(base, 9);
    const cap = plant(g, 'Paris');
    expect(caughtChance(g, cap)).toBeCloseTo(base * INTEL.risk.capital, 9);
    // Reading the books is priced as capital work already.
    expect(caughtChance(g, cap, 'books')).toBeCloseTo(INTEL.missions.books.caught, 9);
    war(g, 'GER', 'FRA');
    expect(caughtChance(g, a)).toBeCloseTo(base * INTEL.risk.war, 9);
    g.intel.heat[a.reg] = g.turn + 2;
    expect(caughtChance(g, a)).toBeCloseTo(base * INTEL.risk.war * INTEL.risk.heat, 9);
    g.intel.heat[a.reg] = g.turn;
    expect(caughtChance(g, a)).toBeCloseTo(base * INTEL.risk.war, 9);
    a.xp = INTEL.agents.skillAt[0];
    expect(skillOf(a)).toBe(2);
    expect(rankOf(a)).toBe(INTEL.agents.ranks[1]);
    expect(caughtChance(g, a)).toBeCloseTo(base * INTEL.risk.war * (1 - INTEL.skill.caught), 9);
    a.xp = INTEL.agents.skillAt[1];
    expect(skillOf(a)).toBe(3);
    expect(caughtChance(g, a)).toBeCloseTo(base * INTEL.risk.war * (1 - 2 * INTEL.skill.caught), 9);
    expect(successChance(a, 'factory')).toBeCloseTo(INTEL.missions.factory.success! + 2 * INTEL.skill.success, 9);
    expect(successChance(a, 'watch')).toBeLessThanOrEqual(2 * INTEL.skill.success + 1e-9);
    // At home nobody is hunting it.
    expect(caughtChance(g, plant(g, 'Berlin', 'guard'))).toBe(0);
  });
});

describe('sabotage', () => {
  /** One attempt on the factories of Paris in each of many wars: how many came off, how many agents were lost. */
  function attempts(mission: Mission, at: string, n: number, xp = 0): { done: number; lost: number; games: Game[] } {
    let done = 0, lost = 0;
    const games: Game[] = [];
    for (let seed = 1; seed <= n; seed++) {
      const g = grand('GER', seed);
      war(g, 'GER', g.byName[at].ctrl);
      const a = plant(g, at, mission, 'GER', xp);
      intelTick(g, 'GER');
      if (g.intel.notes.some((x) => x.what === 'sabotage')) done++;
      if (!g.intel.agents.includes(a)) lost++;
      games.push(g);
    }
    return { done, lost, games };
  }

  it('wrecks a factory about as often as the odds say, and the agent is sometimes lost', () => {
    const N = 120;
    const { done, lost, games } = attempts('factory', 'Paris', N);
    expect(share(done, N)).toBeGreaterThan(INTEL.missions.factory.success! - 0.14);
    expect(share(done, N)).toBeLessThan(INTEL.missions.factory.success! + 0.14);
    const risk = INTEL.missions.factory.caught * INTEL.risk.capital * INTEL.risk.war;
    expect(share(lost, N)).toBeGreaterThan(risk - 0.13);
    expect(share(lost, N)).toBeLessThan(risk + 0.13);
    for (const g of games) {
      const paris = g.byName['Paris'];
      const hit = g.intel.notes.some((x) => x.what === 'sabotage');
      expect(ruinedAt(g, paris)).toBe(hit ? 1 : 0);
      expect(workingFactories(g, paris)).toBe(paris.fac - (hit ? 1 : 0));
      expect(g.intel.heat[paris.id]).toBe(g.turn + INTEL.risk.heatMonths);
      expect(g.headlines.some((h) => h.region === paris.id && /Agent/.test(h.text))).toBe(true);
      const a = g.intel.agents[0];
      if (a) { expect(a.mission).toBe('watch'); expect(a.rest).toBe(g.turn + INTEL.risk.lieLow); if (hit) expect(a.xp).toBe(INTEL.missions.factory.gain); }
    }
    // The enemy's income falls with its factory.
    const hit = games.find((g) => g.intel.notes.some((x) => x.what === 'sabotage'))!;
    const fresh = grand('GER', hit.seed);
    expect(ledger(hit, 'FRA').gross).toBeLessThan(ledger(fresh, 'FRA').gross);
  });

  it('is surer in a master\'s hands', () => {
    const N = 120;
    const green = attempts('factory', 'Paris', N).done;
    const master = attempts('factory', 'Paris', N, INTEL.agents.skillAt[1]).done;
    expect(master).toBeGreaterThan(green);
    expect(share(master, N)).toBeGreaterThan(INTEL.missions.factory.success! + 2 * INTEL.skill.success - 0.14);
  });

  it('cuts a railway until engineers relay it', () => {
    const N = 60;
    const { done, games } = attempts('rail', 'Paris', N);
    expect(share(done, N)).toBeGreaterThan(INTEL.missions.rail.success! - 0.2);
    expect(share(done, N)).toBeLessThan(INTEL.missions.rail.success! + 0.2);
    for (const g of games) expect(railCut(g, g.byName['Paris'].id)).toBe(g.intel.notes.some((x) => x.what === 'sabotage'));
  });

  it('sets an oilfield alight for a few months', () => {
    const N = 60;
    const { done, games } = attempts('oil', 'Ploiesti', N);
    expect(done).toBeGreaterThan(N * 0.25);
    expect(done).toBeLessThan(N * 0.7);
    const g = games.find((x) => x.intel.notes.some((n) => n.what === 'sabotage'))!;
    const field = g.byName['Ploiesti'];
    const full = fieldYield(grand('GER', g.seed), grand('GER', g.seed).byName['Ploiesti']);
    expect(fieldOut(g, field)).toBeCloseTo(INTEL.missions.oil.cut!, 9);
    expect(fieldYield(g, field)).toBeCloseTo(full * (1 - INTEL.missions.oil.cut!), 9);
    g.turn += INTEL.missions.oil.months! - 1;
    expect(fieldOut(g, field)).toBeGreaterThan(0);
    g.turn += 1;
    expect(fieldOut(g, field)).toBe(0);
    expect(fieldYield(g, field)).toBeCloseTo(full, 9);
  });

  it('plays out the same way every time for the same war', () => {
    const run = (): string => {
      const g = grand('GER', 11);
      war(g, 'GER', 'FRA');
      plant(g, 'Paris', 'factory'); plant(g, 'Bordeaux', 'rail'); plant(g, 'Moscow');
      for (let k = 0; k < 6; k++) { intelTick(g, 'GER'); g.turn++; }
      return JSON.stringify([g.intel, g.flags, g.headlines.map((h) => h.text)]);
    };
    expect(run()).toBe(run());
  });

  it('is tried once and then the agent lies low', () => {
    const g = grand('GER', 4);
    war(g, 'GER', 'FRA');
    const a = plant(g, 'Bordeaux', 'factory');
    intelTick(g, 'GER');
    const tries = (): number => g.intel.notes.filter((x) => x.what === 'sabotage' || x.what === 'failed').length;
    expect(tries()).toBe(1);
    if (!g.intel.agents.includes(a)) return;
    expect(a.mission).toBe('watch');
    expect(setMission(g, 'GER', a.id, 'factory')).toBe(false);
    g.turn++;
    intelTick(g, 'GER');
    expect(tries()).toBe(1);
    if (!g.intel.agents.includes(a)) return;
    g.turn = a.rest!;
    expect(whyNotMission(g, 'GER', a.id, 'factory')).toBeNull();
  });
});

describe('the month\'s secret war', () => {
  it('catches watchers about as often as the odds say, and a neutral thinks the worse of the sender', () => {
    const N = 400;
    let lost = 0, protests = 0;
    for (let seed = 1; seed <= N; seed++) {
      const g = grand('GER', seed);
      peace(g, 'GER', 'SOV');
      const was = g.nations.SOV.opinion;
      const a = plant(g, 'Moscow', 'books');
      intelTick(g, 'GER');
      if (g.intel.agents.includes(a)) { expect(g.nations.SOV.opinion).toBe(was); continue; }
      lost++;
      expect(g.intel.notes.at(-1)!.what).toBe('caught');
      expect(g.headlines.at(-1)!.text).toMatch(/caught at Moscow by Soviet counter-intelligence/);
      if (g.nations.SOV.opinion !== was) { protests++; expect(g.nations.SOV.opinion).toBe(Math.max(-100, was - INTEL.caught.opinion)); expect(g.headlines.at(-1)!.text).toMatch(/protests/); }
    }
    expect(share(lost, N)).toBeGreaterThan(INTEL.missions.books.caught - 0.04);
    expect(share(lost, N)).toBeLessThan(INTEL.missions.books.caught + 0.04);
    // The Soviet Union is at peace and outside both blocs when the war opens: it protests every time.
    expect(protests).toBe(lost);
  });

  it('teaches an agent that stays alive, and announces the new rank', () => {
    const g = grand('GER', 2);
    peace(g, 'GER', 'SOV');
    const a = plant(g, 'Kiev');
    a.months = INTEL.agents.watch * INTEL.agents.skillAt[0] - 1;
    a.xp = INTEL.agents.skillAt[0] - 1;
    // Find a month in which the agent is not caught: the dice are the war's own.
    for (let k = 0; k < 12 && g.intel.agents.includes(a) && skillOf(a) === 1; k++) { a.months = INTEL.agents.watch * INTEL.agents.skillAt[0] - 1; intelTick(g, 'GER'); g.turn++; }
    if (!g.intel.agents.includes(a)) return;
    expect(skillOf(a)).toBe(2);
    expect(g.intel.notes.some((x) => x.what === 'rank' && x.agent === a.name)).toBe(true);
    expect(g.headlines.some((h) => /has earned the rank of Operative/.test(h.text))).toBe(true);
  });

  it('turns a guard whose ground is lost into a watcher, and a watcher whose ground is won into a guard', () => {
    const g = grand('GER', 5);
    war(g, 'GER', 'FRA');
    const a = plant(g, 'Berlin', 'guard');
    g.byName['Berlin'].ctrl = 'FRA';
    intelTick(g, 'GER');
    if (g.intel.agents.includes(a)) expect(a.mission).toBe('watch');
    const b = plant(g, 'Paris', 'books');
    g.byName['Paris'].ctrl = 'GER';
    intelTick(g, 'GER');
    expect(g.intel.agents.includes(b)).toBe(true);
    expect(b.mission).toBe('guard');
    // A reader whose capital has fallen to someone else goes back to watching.
    const c = plant(g, 'Warsaw', 'books');
    g.byName['Warsaw'].ctrl = 'SOV';
    intelTick(g, 'GER');
    if (g.intel.agents.includes(c)) expect(c.mission).toBe('watch');
  });

  it('does nothing for a nation the computer runs, or in a war nobody is playing', () => {
    const g = grand('GER', 6);
    plant(g, 'Paris', 'factory', 'FRA');
    const before = JSON.stringify([g.intel, g.flags]);
    intelTick(g, 'FRA');
    intelTick(g, 'UK');
    expect(JSON.stringify([g.intel, g.flags])).toBe(before);
    const watch = grand(null, 6);
    for (let k = 0; k < 14; k++) runTurnAI(watch);
    expect(watch.intel).toEqual({ agents: [], next: 1, notes: [], heat: {} });
    expect(watch.flags.spyfrom).toBeUndefined();
    expect(Object.keys(watch.flags).some((k) => k.startsWith('oilout:'))).toBe(false);
  });

  it('runs when the player\'s month opens', () => {
    const g = grand('GER', 7);
    war(g, 'GER', 'FRA');
    plant(g, 'Bordeaux', 'factory');
    startPhase(g, 'GER');
    expect(g.flags.spyfrom).toBe(g.turn);
    expect(g.intel.notes.some((x) => x.what === 'sabotage' || x.what === 'failed')).toBe(true);
  });
});

describe('the other side\'s saboteurs', () => {
  it('leave the player alone at the start, on the easiest level, and at peace', () => {
    const g = grand('GER');
    war(g, 'GER', 'FRA'); war(g, 'GER', 'UK');
    expect(threat(g, 'GER').foes).toEqual(expect.arrayContaining(['FRA', 'UK']));
    expect(threat(g, 'GER').each).toBeCloseTo(INTEL.enemy.chance, 9);
    expect(threat(g, 'GER').chance).toBeCloseTo(1 - Math.pow(1 - INTEL.enemy.chance, threat(g, 'GER').foes.length), 9);
    // The first months are quiet.
    g.flags.spyfrom = g.turn;
    expect(threat(g, 'GER').chance).toBe(0);
    g.turn += INTEL.enemy.quiet;
    expect(threat(g, 'GER').chance).toBeGreaterThan(0);
    setLevel(g, 0);
    expect(threat(g, 'GER').chance).toBe(0);
    setLevel(g, DIFFICULTY.levels.length - 1);
    expect(threat(g, 'GER').each).toBeCloseTo(INTEL.enemy.chance * INTEL.enemy.level.at(-1)!, 9);
    setLevel(g, DIFFICULTY.start);
    for (const f of [...g.nations.GER.atWar]) peace(g, 'GER', f);
    expect(threat(g, 'GER')).toEqual({ chance: 0, each: INTEL.enemy.chance, foes: [] });
    // A small country at war with the player has no service to send.
    war(g, 'GER', 'POL');
    expect(threat(g, 'GER').foes).toEqual([]);
  });

  /** A hundred and fifty months of the secret war against Germany, with these guards posted. */
  function siege(guards: string[], seed: number): { struck: number; foiled: number; g: Game } {
    const g = grand('GER', seed);
    war(g, 'GER', 'FRA'); war(g, 'GER', 'UK');
    for (const at of guards) plant(g, at, 'guard');
    g.flags.spyfrom = -99;
    for (let k = 0; k < 150; k++) { intelTick(g, 'GER'); g.turn++; for (const key of Object.keys(g.flags)) if (key.startsWith('ruin:') || key.startsWith('cut:')) delete g.flags[key]; }
    return { struck: g.intel.notes.filter((x) => x.what === 'struck').length, foiled: g.intel.notes.filter((x) => x.what === 'foiled').length, g };
  }

  it('wreck factories and cut railways on home ground about as often as the odds say', () => {
    let struck = 0, months = 0;
    for (let seed = 1; seed <= 4; seed++) {
      const g = grand('GER', seed);
      war(g, 'GER', 'FRA'); war(g, 'GER', 'UK');
      g.flags.spyfrom = -99;
      const foes = threat(g, 'GER').foes.length;
      for (let k = 0; k < 120; k++) {
        const flags = Object.keys(g.flags).length;
        intelTick(g, 'GER');
        months += foes;
        const last = g.intel.notes.at(-1);
        if (last && last.turn === g.turn && last.what === 'struck') {
          const r = g.regions[last.reg];
          expect(r.owner).toBe('GER');
          expect(r.ctrl).toBe('GER');
          expect(ruinedAt(g, r) > 0 || railCut(g, r.id)).toBe(true);
          expect(Object.keys(g.flags).length).toBeGreaterThanOrEqual(flags);
        }
        g.turn++;
        for (const key of Object.keys(g.flags)) if (key.startsWith('ruin:') || key.startsWith('cut:')) delete g.flags[key];
      }
      struck += g.intel.notes.filter((x) => x.what === 'struck').length + g.headlines.filter((h) => /^Saboteurs/.test(h.text)).length * 0;
      // The file keeps only the latest notes, so the headlines are the full count.
      struck += Math.max(0, g.headlines.filter((h) => /^Saboteurs/.test(h.text)).length - g.intel.notes.filter((x) => x.what === 'struck').length);
    }
    expect(share(struck, months)).toBeGreaterThan(INTEL.enemy.chance - 0.02);
    expect(share(struck, months)).toBeLessThan(INTEL.enemy.chance + 0.02);
  });

  it('are mostly stopped by a guard in or beside the province, and a little by one elsewhere', () => {
    const g = grand('GER');
    expect(guardChance(g, 'GER', g.byName['Berlin'].id)).toEqual({ chance: 0, by: null });
    const a = plant(g, 'Berlin', 'guard');
    expect(guardChance(g, 'GER', g.byName['Berlin'].id).chance).toBeCloseTo(INTEL.enemy.near, 9);
    expect(guardChance(g, 'GER', g.byName['Berlin'].id).by).toBe(a);
    const beside = g.byName['Berlin'].nb[0];
    expect(guardChance(g, 'GER', beside).chance).toBeCloseTo(INTEL.enemy.near, 9);
    const far = g.regions.find((r) => r.ctrl === 'GER' && r.id !== a.reg && !g.byName['Berlin'].nb.includes(r.id))!;
    expect(guardChance(g, 'GER', far.id).chance).toBeCloseTo(INTEL.enemy.far, 9);
    a.xp = INTEL.agents.skillAt[1];
    expect(guardChance(g, 'GER', beside).chance).toBeCloseTo(INTEL.enemy.near + 2 * INTEL.skill.guard, 9);
    plant(g, far.name, 'guard');
    expect(guardChance(g, 'GER', beside).chance).toBeCloseTo(Math.min(INTEL.enemy.most, INTEL.enemy.near + 2 * INTEL.skill.guard + INTEL.enemy.far), 9);
    // An agent abroad, in training or busy elsewhere guards nothing.
    const h = grand('GER');
    plant(h, 'Paris'); plant(h, 'Berlin', 'guard').ready = h.turn + 1;
    expect(guardChance(h, 'GER', h.byName['Berlin'].id).chance).toBe(0);
    expect(spyRoom(h)!.threat.guards).toBe(0);
  });

  it('strike far less often against a guarded country', () => {
    let open = 0, held = 0, foiled = 0;
    for (let seed = 1; seed <= 3; seed++) {
      open += siege([], seed).struck;
      const g = grand('GER', seed);
      const homes = g.regions.filter((r) => r.ctrl === 'GER' && r.owner === 'GER' && r.fac > 0).slice(0, 4).map((r) => r.name);
      const s = siege(homes, seed);
      held += s.struck; foiled += s.foiled;
    }
    expect(open).toBeGreaterThan(5);
    expect(foiled).toBeGreaterThan(0);
    expect(held).toBeLessThan(open);
  });
});

describe('what an agent brings home', () => {
  it('opens a government\'s books from its capital, and only while the agent is reading them', () => {
    const g = playing();
    expect(knowsBooks(g, 'GER')).toBe(true);
    expect(knowsBooks(g, 'FRA')).toBe(false);
    expect(booksOf(g, 'FRA')).toBeNull();
    expect(foreignBooks(g, 'FRA')).toBeNull();
    const a = plant(g, 'Paris');
    expect(knowsBooks(g, 'FRA')).toBe(false);
    expect(orderAgentMission(g, a.id, 'books')).toBe(true);
    expect(knowsBooks(g, 'FRA')).toBe(true);
    const b = foreignBooks(g, 'FRA')!;
    const L = ledger(g, 'FRA');
    expect(b.agent).toBe(a.name);
    expect(b.treasury).toBe(L.treasury);
    expect(b.gross).toBe(L.gross);
    expect(b.upkeep).toBe(L.upkeepTotal);
    expect(b.net).toBe(L.net);
    expect(b.divisions).toBe(fieldArmy(g, 'FRA'));
    expect(b.corps).toBe(g.corps.filter((c) => c.nat === 'FRA' && !c.dead).length);
    expect(b.queue).toHaveLength(g.nations.FRA.queue.length);
    expect(b.research).toHaveLength(g.nations.FRA.tech?.work.length ?? 0);
    expect(b.stores).not.toBeNull();
    expect(armyKnown(g, 'FRA')).toEqual({ divs: fieldArmy(g, 'FRA'), exact: true });
    expect(secretService(g)!.books).toEqual(['FRA']);
    // Another country's books stay shut, and so do these once the agent stops reading or is on the road.
    expect(knowsBooks(g, 'UK')).toBe(false);
    a.mission = 'watch';
    expect(knowsBooks(g, 'FRA')).toBe(false);
    a.mission = 'books'; a.ready = g.turn + 1;
    expect(knowsBooks(g, 'FRA')).toBe(false);
    a.ready = g.turn;
    g.byName['Paris'].ctrl = 'UK';
    expect(knowsBooks(g, 'FRA')).toBe(false);
    // An ally's are always open.
    const friend = Object.values(g.nations).find((n) => n.id !== 'GER' && n.alive && allied(g, n.id, 'GER'));
    if (friend) { expect(knowsBooks(g, friend.id)).toBe(true); expect(booksOf(g, friend.id)!.agent).toBeNull(); }
  });

  it('reports what is on order and on the airfields of the province it stands in', () => {
    const g = playing();
    const paris = g.byName['Paris'];
    expect(agentReport(g, paris.id)).toBeNull();
    const a = plant(g, 'Paris');
    g.nations.FRA.queue.push({ id: 900, what: 'corps', kind: 'inf', divs: 2, name: 'Test Corps', region: paris.id, ordered: g.turn, eta: g.turn + 3, cost: 10 });
    g.nations.FRA.queue.push({ id: 901, what: 'corps', kind: 'inf', divs: 2, name: 'Elsewhere Corps', region: g.byName['Bordeaux'].id, ordered: g.turn, eta: g.turn + 2, cost: 10 });
    const rep = agentReport(g, paris.id)!;
    expect(rep.agent).toBe(a.name);
    expect(rep.orders).toEqual([{ name: 'Test Corps', eta: g.turn + 3 }]);
    expect(rep.factories).toBe(paris.fac);
    expect(rep.ruined).toBe(0);
    expect(rep.air).toHaveLength(g.airFleets.filter((f) => f.base === paris.id).length);
    // Next door the armies are in sight, but the city's secrets are not.
    expect(cityReport(g, paris.nb[0])).toBeNull();
    const view = secretService(g)!.agents.find((x) => x.id === a.id)!;
    const there = new Set(g.corps.filter((c) => !c.dead && !allied(g, c.nat, 'GER') && (c.reg === paris.id || paris.nb.includes(c.reg))).map((c) => `${c.reg}:${c.nat}`));
    expect(view.sees).toHaveLength(there.size);
    for (const s of view.sees) expect(s.divs).toBeGreaterThan(0);
  });

  it('makes the player\'s attack on its province stronger, and says so on the battle ticket', () => {
    const g = playing();
    const lodz = g.byName['Lodz'];
    const corps = g.corps.filter((c) => c.nat === 'GER' && !c.dead && g.regions[c.reg].nb.includes(lodz.id)).slice(0, 2);
    expect(corps.length).toBeGreaterThan(0);
    const before = assess(g, corps, lodz);
    expect(before.lines.some((l) => l.label === 'Intelligence')).toBe(false);
    const a = plant(g, 'Lodz');
    expect(intelAttack(g, 'GER', lodz)).toEqual({ mul: 1 + INTEL.battle.attack, agent: a.name });
    const after = assess(g, corps, lodz);
    expect(after.A / before.A).toBeCloseTo(1 + INTEL.battle.attack, 6);
    expect(after.D).toBeCloseTo(before.D, 9);
    const line = after.lines.find((l) => l.label === 'Intelligence')!;
    expect(line.value).toContain(a.name);
    expect(line.effect).toContain(`${Math.round(INTEL.battle.attack * 100)}%`);
    // An agent on the road has mapped nothing, and the enemy gains nothing from the player's agent.
    a.ready = g.turn + 1;
    expect(assess(g, corps, lodz).A).toBeCloseTo(before.A, 9);
    a.ready = g.turn;
    expect(intelAttack(g, 'FRA', lodz)).toBeNull();
  });
});

describe('the secret service\'s room', () => {
  it('shows every agent, the risk at home and the file', () => {
    const g = playing();
    war(g, 'GER', 'FRA');
    g.nations.GER.pp = 100;
    const a = plant(g, 'Paris', 'books');
    const b = plant(g, 'Berlin', 'guard');
    orderRecruitAgent(g);
    // The quiet first months are over.
    g.flags.spyfrom = g.turn - INTEL.enemy.quiet;
    const room = secretService(g)!;
    expect(room.nat).toBe('GER');
    expect(room.fog).toBe(true);
    expect(room.agents).toHaveLength(3);
    const pa = room.agents.find((x) => x.id === a.id)!, pb = room.agents.find((x) => x.id === b.id)!;
    expect(pa.abroad).toBe(true);
    expect(pa.host).toBe('FRA');
    expect(pa.where).toBe('Paris');
    expect(pa.doing).toBe('Reading the books at Paris');
    expect(pa.caught).toBeCloseTo(caughtChance(g, a), 9);
    expect(pa.next).toBe(INTEL.agents.skillAt[0]);
    expect(pb.abroad).toBe(false);
    expect(pb.doing).toBe('On guard at Berlin');
    expect(pb.caught).toBe(0);
    expect(pb.sees).toEqual([]);
    expect(room.books).toEqual(['FRA']);
    expect(room.threat.guards).toBe(1);
    expect(room.threat.foes).toContain('FRA');
    expect(room.notes[0].what).toBe('recruit');
    expect(room.whyNotRecruit).toBeNull();
    // A master has no further rank to earn.
    a.xp = INTEL.agents.skillAt[1];
    expect(secretService(g)!.agents.find((x) => x.id === a.id)!.next).toBeNull();
    expect(secretService(g)!.agents.find((x) => x.id === a.id)!.rank).toBe(INTEL.agents.ranks[2]);
  });
});

describe('saved games', () => {
  it('keep the service, and a save from before spies opens with nobody on its books', () => {
    const g = playing();
    war(g, 'GER', 'FRA');
    g.nations.GER.pp = 100;
    orderRecruitAgent(g);
    const a = plant(g, 'Paris', 'books');
    a.rest = g.turn + 2; a.months = 3;
    g.intel.heat[a.reg] = g.turn + 3;
    g.flags.nofog = 1;
    const back = loadGame(saveGame(g))!;
    expect(back.intel).toEqual(g.intel);
    expect(back.flags.nofog).toBe(1);
    expect(fogOn(back)).toBe(false);
    expect(knowsBooks(back, 'FRA')).toBe(true);
    const old = JSON.parse(saveGame(g));
    delete old.intel;
    const fresh = loadGame(JSON.stringify(old))!;
    expect(fresh.intel).toEqual({ agents: [], next: 1, notes: [], heat: {} });
    expect(secretService(fresh)!.agents).toEqual([]);
  });
});

describe('a played campaign', () => {
  it('runs two years with agents at work and nothing out of order', () => {
    const g = playing('GER', 9);
    g.nations.GER.pp += 60;
    for (let k = 0; k < 24; k++) {
      const room = secretService(g)!;
      if (!room.whyNotRecruit && room.agents.length < 3) orderRecruitAgent(g);
      for (const v of secretService(g)!.agents) {
        if (!v.atWork) continue;
        if (!v.abroad) {
          const foe = g.regions.filter((r) => g.nations.GER.atWar.includes(r.ctrl) && r.fac > 0);
          if (foe.length && v.id % 2 === 0) orderSendAgent(g, v.id, foe[(g.turn * 7 + v.id) % foe.length].id);
        } else if (v.rest === 0) {
          for (const m of ['factory', 'oil', 'rail'] as Mission[]) if (orderAgentMission(g, v.id, m)) break;
        }
      }
      nextMonth(g);
      if (!g.nations.GER.alive) break;
      for (const a of g.intel.agents) { expect(a.nat).toBe('GER'); expect(g.regions[a.reg]).toBeTruthy(); expect(a.xp).toBeGreaterThanOrEqual(0); }
      expect(g.intel.agents.length).toBeLessThanOrEqual(mostAgents('GER'));
      expect(g.intel.notes.length).toBeLessThanOrEqual(60);
      const s = sight(g);
      for (const r of g.regions) if (r.ctrl === 'GER') expect(s[r.id]).toBe(SIGHT.full);
    }
    expect(g.intel.notes.length).toBeGreaterThan(3);
    expect(new Set(g.intel.agents.map((a) => a.name)).size).toBe(g.intel.agents.length);
  });
});
