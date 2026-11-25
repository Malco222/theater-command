// The player's staff at work (the grand campaign): what it does with each desk he has handed it, month by month,
// and the three depths that hand over several desks at once. The switches themselves are in depth.ts.
//
// For research, building, the air war and the navy the staff is the routine a computer nation uses, called for the
// player's nation and for that one desk: one set of rules, one balance. Trade is signed as a computer nation signs
// it, at the price the player pays. Courting and the secret service have no computer routine, so the rules for
// those two are written here, and kept cautious. Everything the staff buys comes out of the share of the treasury
// the player allows it for the month.
//
// A desk's month ends in one line of news, tagged with the desk so that an interface can offer to take it back.

import { DEPTH, DESKS, capOf, capPct, deskDef, deskRan, depthRules, markRan, setCap, setDesk, staffRuns, type DeskId, type DepthDef } from './depth';
import { allied, atWar, bloc, headline, shoreRoom } from './game';
import { turnToDate } from '../core/ticket';
import { BUDGET, PRODUCTION, aiProduction, aiTeams, armyWorn, gunsWanted, income, specOf, upkeepTotal } from './economy';
import { menSpare } from './manpower';
import { ARSENAL, aiArsenal, liftView, offers, orderOffer, type Wing } from './arsenal';
import { civicAI } from './buildings';
import { keepsStores } from './resources';
import { monthsFor, spyTeamsOf, staffTeams, techById, techOf, teamsOf, workOf } from './tech';
import { TRADE, bestPartner, dealsOf, endTrade, housesFull, housesOf, signTrade, tradeRules, wealthOf } from './trade';
import { DIPLOMACY, canCourt, court, courtable, envoysWord, freeCourt, receptive, step } from './diplomacy';
import { INTEL, abroad, agentsOf, arrest, atWork, enemySends, foesFound, intelRules, mostAgents, recruit, sendAgent, setFog, setMission, threat, turnFoe, watchOdds, whyNotMission, whyNotRecruit, whyNotSend, whyNotTurn } from './intel';
import { doublesOn, watchersOn } from './secretwar';
import { cabinetOff, officeSecure, setCabinet, setCanFall } from './government';
import { airAI, airFleetsOf } from './air';
import { atSea, crossingsOf, fleetsOf, navalAI, numberWord, plannedCrossing } from './navy';
import { seaHead, seaIs, seaPlural, seaWhat, seaWhere } from './sea-words';
import { landingForce } from './staff';
import { staysOut } from './history';
import { shoreAI } from './fleets';
import { seaStaff } from './carriers';
import { gatherMonth } from './readiness';
import { STACK, type AirReport, type Build, type Game, type Nation, type Region } from './types';
import { aiRepair } from './repairs';
import { aiFieldRepair } from './fieldstate';

const listAnd = (xs: string[]): string => (xs.length <= 1 ? xs.join('') : `${xs.slice(0, -1).join(', ')} and ${xs[xs.length - 1]}`);
/** A list cut to its first few, with the rest counted. */
const some = (xs: string[], most = 3): string => (xs.length <= most ? listAnd(xs) : `${xs.slice(0, most).join(', ')} and ${xs.length - most} more`);
const the = (n: Nation): string => (/^(United|Soviet|Netherlands|Dominican Republic)/.test(n.name) ? `the ${n.name}` : n.name);
const months = (k: number): string => `${k} month${k === 1 ? '' : 's'}`;
const plural = (k: number, one: string, many = `${one}s`): string => `${k === 1 ? (/^[aeiou]/i.test(one) ? 'an' : 'a') : k} ${k === 1 ? one : many}`;

// ---- What the staff may spend -------------------------------------------------------------------------

/**
 * What is left of the month's allowance: the share of the treasury, as it stood when the staff first reached for it
 * this month, that the player lets his staff spend. A treasury in debt allows nothing.
 */
export function allowance(g: Game, nat: string): number {
  if (g.flags['dk:alwt'] !== g.turn) {
    g.flags['dk:alwt'] = g.turn;
    g.flags['dk:alw'] = Math.max(0, g.nations[nat].pp) * capOf(g);
    // The share it was drawn at: a share changed later in the month takes hold with the next allowance.
    g.flags['dk:alwc'] = capPct(g);
  }
  return g.flags['dk:alw'] ?? 0;
}

/**
 * Lets a staff routine spend, but only out of the month's allowance: while it runs the treasury it sees is the
 * allowance, and what it did not touch is put back afterwards. Returns what it spent.
 */
function fromPurse(g: Game, nat: string, run: () => void): number {
  const n = g.nations[nat];
  const purse = Math.min(allowance(g, nat), Math.max(0, n.pp));
  const held = n.pp - purse;
  n.pp = purse;
  try { run(); } finally {
    const spent = purse - n.pp;
    n.pp += held;
    g.flags['dk:alw'] = Math.max(0, (g.flags['dk:alw'] ?? 0) - spent);
  }
  return purse - (n.pp - held);
}

// ---- Research -----------------------------------------------------------------------------------------

function research(g: Game, nat: string): string | null {
  const begun = staffTeams(g, nat);
  if (!begun.length) return null;
  const names = begun.map((id) => { const t = techById(id)!; return `${t.name} (${months(Math.ceil(monthsFor(t, g.turn)))})`; });
  return `put ${begun.length === 1 ? 'a research team' : `${begun.length} research teams`} to work on ${listAnd(names)}.`;
}

// ---- Recruitment, and towns and trade ------------------------------------------------------------------

const orderWords = (g: Game, b: Build): string =>
  b.what === 'civic' ? `the ${b.name[0].toLowerCase()}${b.name.slice(1)}`
    : b.what === 'air' ? `a ${b.name[0].toLowerCase()}${b.name.slice(1)}`
    : b.what === 'work' ? `${b.item === 'aa' ? 'anti-aircraft guns' : b.item === 'lights' ? 'searchlights' : b.item ?? 'works'} at ${g.regions[b.region].name}`
      : b.what === 'ship' ? `${b.name[0].toLowerCase()}${b.name.slice(1)}`
        : `${b.name} at ${g.regions[b.region].name}`;

/** The aircraft a nation has bought over its air force's own establishment, with the wings on order, by kind. */
export function boughtAir(g: Game, nat: string): { over: Record<Wing, number>; base: Record<Wing, number>; forming: Record<Wing, number> } {
  const over = { ftr: 0, bmr: 0 }, base = { ftr: 0, bmr: 0 }, forming = { ftr: 0, bmr: 0 };
  for (const f of airFleetsOf(g, nat)) {
    if (f.ship !== undefined || f.ashore !== undefined) continue;
    for (const w of ['ftr', 'bmr'] as Wing[]) { const x = f.extra?.[w] ?? 0; over[w] += x; base[w] += Math.max(0, f.est[w] - x); }
  }
  for (const b of g.nations[nat].queue) if (b.what === 'air' && (b.item === 'ftr' || b.item === 'bmr')) { forming[b.item] += 1; over[b.item] += b.count ?? ARSENAL.air[b.item].count; }
  return { over, base, forming };
}

/** True while the armies in the field are waiting for the men a new corps would take: the staff raises none. */
const noMenFor = (g: Game, nat: string): boolean => menSpare(g, nat) < (specOf(g, 'inf')?.divs ?? 1);

/** The kinds of wing a building staff would still order: none while there are men for corps, or when it has bought its share. */
export function wingsWanted(g: Game, nat: string): Wing[] {
  const n = g.nations[nat], M = DEPTH.build?.machines;
  if (!M || !n?.alive || !n.atWar.length || !noMenFor(g, nat)) return [];
  const A = boughtAir(g, nat);
  const raid = g.raids[nat], raided = !!raid && g.turn - raid.turn <= 2;
  const fill = (w: Wing): number => A.over[w] / Math.max(1, A.base[w]);
  const kinds: Wing[] = raided || fill('ftr') <= fill('bmr') ? ['ftr', 'bmr'] : ['bmr', 'ftr'];
  return kinds.filter((w) => A.over[w] + ARSENAL.air[w].count <= M.share * A.base[w]);
}

/**
 * Money without men (depth.json, 'build.machines'). A staff that can raise no corps for want of men puts the treasury
 * into aircraft, which need pilots and not divisions: so many wings of each kind on order at a time, until what it has
 * bought stands at its share of the air force's own establishment, and nothing the treasury could not keep flying.
 */
function machines(g: Game, nat: string): void {
  const n = g.nations[nat], M = DEPTH.build?.machines;
  if (!M) return;
  let spare = income(g, nat) - upkeepTotal(g, nat);
  for (const w of wingsWanted(g, nat)) {
    if (boughtAir(g, nat).forming[w] >= M.each) continue;
    const o = offers(g, nat).find((x) => x.key === `air:${w}`);
    if (!o || o.why !== null || o.at < 0 || n.pp < o.cost + PRODUCTION.aiReserve || spare < o.upkeep * BUDGET.debt.aiMargin) continue;
    if (orderOffer(g, nat, o.key, o.at)) spare -= o.upkeep;
  }
}

/**
 * The crossing the player has named as his plan (navy.ts, `plannedCrossing`): his building staff lays down
 * troopships until the shipping can carry the force for it in one lift. The force is the corps standing on the
 * quay, never fewer than a staff sends ashore in a landing and never more than can go ashore in one place. One lot
 * a month, first call on the month's allowance, within what the yards can take (arsenal.json, 'lift').
 */
function shipping(g: Game, nat: string): void {
  const n = g.nations[nat];
  const plan = plannedCrossing(g, nat);
  if (!plan) return;
  const lift = liftView(g, nat);
  const force = plan.from.units.filter((u) => u.nat === nat && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && !u.airborne).length;
  // (An island of the Pacific takes its room and no more: no troopship is laid down for a corps that could not land. game.ts shoreRoom.)
  const want = Math.min(STACK, shoreRoom(g, plan.to, nat), Math.max(landingForce(), force));
  if (lift.room <= 0 || lift.lift + lift.building.length >= want) return;
  const o = offers(g, nat).find((x) => x.key === 'ship:tp');
  if (o && o.why === null && o.at >= 0 && n.pp >= o.cost + PRODUCTION.aiReserve) orderOffer(g, nat, o.key, o.at);
}

/** The staff's word on the crossing the player has named as his plan. */
export interface CrossingWord {
  from: Region; to: Region;
  /**
   * 'waiting': something is still wanting (a sea or a sky that bars the way, corps on the quay, ships to carry
   * them). 'warned': it could sail, but the landing card warns that the sea behind the beach can be closed, so the
   * staff waits: the order to sail under a warning is the player's own. 'ready': it can sail and the card is clear.
   */
  state: 'waiting' | 'warned' | 'ready';
  /** The same in words, for the Staff tab and the month's news. */
  text: string;
}

/**
 * What the player's staff has to say about the crossing he has named (navy.ts, `plannedCrossing`): it prepares
 * the crossing (the admirals and the air staff where he has handed them those desks, the building staff with
 * troopships), says what is still wanting, and reports when it is ready. Under the card's warning it waits and says
 * so. It never sails: the landing is always the player's own order. Null with no crossing named.
 */
export function crossingWord(g: Game, nat: string): CrossingWord | null {
  const plan = plannedCrossing(g, nat);
  if (!plan) return null;
  const x = crossingsOf(g, nat).find((c) => c.planned);
  const lift = liftView(g, nat);
  const force = plan.from.units.filter((u) => u.nat === nat && !u.dead && u.kind !== 'fort' && u.kind !== 'eng' && !u.airborne).length;
  const want = landingForce();
  const way = `the crossing from ${plan.from.name} to ${plan.to.name}`;
  // (A shore the army can now march to is no crossing at all.)
  if (!x) return { ...plan, state: 'waiting', text: `Your staff is not preparing ${way}: your army can reach that shore by land.` };
  const wanting: string[] = [];
  if (x.bar) wanting.push(`the way itself (${x.bar.replace(/\.$/, '')})`);
  if (force < want) wanting.push(`${want - force} more corps on the quay at ${plan.from.name} (a landing takes ${want}, and ${force === 0 ? 'none stands' : force === 1 ? 'one stands' : `${force} stand`} there)`);
  if (lift.lift < want) wanting.push(lift.building.length ? `shipping (the ships carry ${lift.lift} corps a month; troopships for more are on the slips, the next lot ready in ${turnToDate(lift.building[0]).label})` : `shipping (the ships carry ${lift.lift} corps a month)`);
  if (wanting.length) return { ...plan, state: 'waiting', text: `Your staff is preparing ${way}. Still wanting: ${listAnd(wanting)}.` };
  const h = x.account.hold;
  if (!h.open) return { ...plan, state: 'warned', text: h.island
    ? `Your staff has ${way} prepared, and it waits: the force can take ${plan.to.name}, but the enemy can close ${seaWhat(g.seas[h.sea].name)} behind it, and cut off it holds out for about ${numberWord(h.months)} months on what it carries. The order to sail under that warning is yours alone.`
    : `Your staff has ${way} prepared, and it waits: once the force is ashore the enemy can close ${seaWhat(g.seas[h.sea].name)} behind it. The order to sail under that warning is yours alone.` };
  const lost = Math.round(x.account.loss * 100);
  return { ...plan, state: 'ready', text: `Your staff reports ${way} ready: the way is open, ${force} corps stand on the quay and the ships can carry ${Math.min(force, lift.lift)} of them in a month${lost > 0 ? `, with about ${lost} men in a hundred likely to be lost at sea` : ''}.${h.contested ? ` ${seaHead(g.seas[h.sea].name)} behind the beach ${seaIs(g.seas[h.sea].name)} yours today, but the enemy can bring ships enough to fight you for ${seaPlural(g.seas[h.sea].name) ? 'them' : 'it'}.` : ''} The order to land is yours.` };
}

/** The month's report on the named crossing: said once in the news when it is ready, or prepared and held back by the card's warning. */
function crossingReport(g: Game, nat: string): void {
  const w = crossingWord(g, nat);
  const code = w ? (w.to.id + 1) * 4 + (w.state === 'ready' ? 1 : w.state === 'warned' ? 2 : 3) : 0;
  if (code === (g.flags['dk:cross'] ?? 0)) return;
  if (code) g.flags['dk:cross'] = code; else delete g.flags['dk:cross'];
  if (w && w.state !== 'waiting') headline(g, 'info', w.text, nat, w.to.id);
}

/**
 * The building staff's month: recruitment ('forces') and towns and trade ('towns'), the two halves of what was one
 * desk (release 2). Each step is done by the half it belongs to, in the order the one desk worked in, out of the one
 * allowance, so that with both halves handed over the month is the one desk's to the byte.
 */
function building(g: Game, nat: string, towns: boolean, forces: boolean): string | null {
  const n = g.nations[nat];
  const did: string[] = [];
  // Trade first: an agreement pays every month from now on. One that nothing gets through is given up for a better.
  if (towns && tradeRules(g) && n.capital >= 0) {
    const ctx = new Map();
    for (const d of dealsOf(g, nat)) {
      if (d.state !== 'cut') continue;
      const o = g.nations[d.a === nat ? d.b : d.a];
      if (endTrade(g, d.a, d.b)) did.push(`gave up the trade agreement with ${the(o)}, which nothing was getting through`);
    }
    const signed: string[] = [];
    for (let k = 0; k < 2 && !housesFull(g, nat) && n.pc >= TRADE.sign.pc; k++) {
      const b = bestPartner(g, nat, ctx);
      if (!b || !signTrade(g, nat, b, ctx)) break;
      signed.push(the(g.nations[b]));
    }
    if (signed.length) did.push(`signed ${signed.length === 1 ? 'a trade agreement' : 'trade agreements'} with ${listAnd(signed)}`);
  }
  // Then the month's orders: troopships for the crossing the player has named, and then as a computer nation's
  // staff places them: new corps, guns round bombed industry, building in the towns. (Each routine also asks its own
  // half's switch, depth.ts byHand, as it does for a computer nation's month.)
  const had = new Set(n.queue.map((b) => b.id));
  // (A half handed over later in a month adds what it spends to what the other half spent.)
  const before = deskRan(g, 'build') || deskRan(g, 'towns') ? g.flags['dk:spent'] ?? 0 : 0;
  let repaired: string[] = [];
  const spent = fromPurse(g, nat, () => {
    if (forces) shipping(g, nat);
    // Damaged towns first: a repair pays for itself in a few months (repairs.ts).
    if (towns) {
      repaired = aiRepair(g, nat);
      repaired.push(...aiFieldRepair(g, nat, (x) => allied(g, nat, x)).map((x) => `the airfield at ${x}`));
    }
    if (forces) { aiProduction(g, nat); aiTeams(g, nat); }
    aiArsenal(g, nat, towns, forces);
    // Guns before town halls while the staff holds recruitment too; while the player raises the forces himself the
    // towns staff cannot know his plan and does not wait (the share is his guard).
    if (towns) civicAI(g, nat, income(g, nat), keepsStores(g, nat), staffRuns(g, nat, 'build') && (gunsWanted(g, nat) || armyWorn(g, nat)));
    if (forces) machines(g, nat);
  });
  const fresh = n.queue.filter((b) => !had.has(b.id));
  if (fresh.length) did.unshift(`ordered ${some(fresh.map((b) => orderWords(g, b)), 4)}`);
  if (repaired.length) did.unshift(`repaired ${some(repaired, 4)}`);
  g.flags['dk:spent'] = before ? before + spent : spent;
  if (!did.length) return null;
  return `${listAnd(did)}.`;
}

/** Recruitment's month, with towns and trade's in it, as the one desk's was, where the staff holds that half too and it has not yet worked. */
function build(g: Game, nat: string): string | null {
  const towns = staffRuns(g, nat, 'towns') && !deskRan(g, 'towns');
  if (!towns) g.flags['dk:ran:only'] = g.turn;
  return building(g, nat, towns, true);
}

/** Towns and trade's month, alone: recruitment is the player's, or its staff has already worked this month. */
const townsDesk = (g: Game, nat: string): string | null => building(g, nat, true, false);

// ---- Diplomacy ----------------------------------------------------------------------------------------

function diplomacy(g: Game, nat: string): string | null {
  // (Ruling 223(d): while the Axis courts a neutral of the Americas, the United States courts it back at no cost.)
  // (The UI director's words: "courted Mexico back against the German envoys, at no cost.")
  const back = Object.values(g.nations).filter((T) => freeCourt(g, nat, T.id) && canCourt(g, nat, T.id).ok && court(g, nat, T.id));
  const said = envoys(g, nat);
  if (!back.length) return said;
  const backed = `courted ${listAnd(back.map((T) => the(T)))} back against the ${envoysWord(g, back.map((T) => T.id))} envoys, at no cost`;
  return said ? `${backed}, and ${said}` : `${backed}.`;
}

function envoys(g: Game, nat: string): string | null {
  const n = g.nations[nat], b = bloc(n);
  if (!b || b === 'league' || n.pc < DIPLOMACY.court.cost + DEPTH.diplomacy.keep) return null;
  const side = b === 'axis' ? 1 : -1;
  // (A player's own Axis courts as it always has, by hand or by his staff: ruling 223(e). The computer's envoys in the
  // Americas are diplomacy.ts's americasMonth.)
  let best: Nation | null = null, top = 0, book = 0;
  for (const T of Object.values(g.nations)) {
    if (!canCourt(g, nat, T.id).ok) continue;
    // The history dial: by the book the staff's envoys keep to the countries history brought in on this side. The
    // player may court any other by hand, and from then on the staff may too.
    if (staysOut(g, T.id, b)) { book++; continue; }
    // The neutral most ready to listen, the nearer to joining the better, and a rich one before a poor one.
    const v = receptive(T, b) * (2 + step(T) * side) * Math.sqrt(1 + wealthOf(g, T.id));
    if (v > top || (v === top && best !== null && T.id < best.id)) { top = v; best = T; }
  }
  if (!best) {
    // Said once: why the envoys are at home.
    if (!book || g.flags['dk:crt:book']) return null;
    g.flags['dk:crt:book'] = 1;
    return 'sent no envoys: by the book (the history dial) they court only the countries that history brought into the war on our side, and none of those is left to court. Court a country yourself and the staff will carry on with it.';
  }
  const before = step(best);
  if (!court(g, nat, best.id)) return null;
  // One line when the envoys take up a country and one when it moves: a month of the same courting is not news.
  const key = `dk:crt:${best.id}`, same = g.flags[key] === 1, now = step(best);
  g.flags[key] = 1;
  if (!same) return `began courting ${the(best)}.`;
  if (now === before) return null;
  return (now - before) * side > 0 ? `courted ${the(best)}, which has moved a step towards us.` : `courted ${the(best)}, which has moved a step away all the same.`;
}

// ---- The secret service -------------------------------------------------------------------------------

/** Enemy-held ground beside the player's field army, with the divisions of his that stand against each province. */
function frontOf(g: Game, nat: string): Map<number, number> {
  const front = new Map<number, number>();
  for (const c of g.corps) {
    if (c.nat !== nat || c.dead || c.kind === 'fort') continue;
    for (const j of g.regions[c.reg].nb) if (atWar(g, nat, g.regions[j].ctrl)) front.set(j, (front.get(j) ?? 0) + c.divs);
  }
  return front;
}

function spies(g: Game, nat: string): string | null {
  if (!intelRules(g)) return null;
  const n = g.nations[nat], S = DEPTH.spies;
  const did: string[] = [];
  const war = n.atWar.filter((e) => g.nations[e]?.alive);
  // One recruit a month while the service has room: all it can run in a war, a guard or two in peace.
  const want = war.length ? mostAgents(nat, g) : Math.min(mostAgents(nat, g), Math.max(1, S.guards));
  if (agentsOf(g, nat).length < want && whyNotRecruit(g, nat) === null && allowance(g, nat) >= INTEL.agents.cost && n.pp - INTEL.agents.cost >= PRODUCTION.aiReserve) {
    const a = recruit(g, nat);
    if (a) { g.flags['dk:alw'] = Math.max(0, allowance(g, nat) - INTEL.agents.cost); did.push(`recruited agent ${a.name}`); }
  }
  // (With the enemy's watchers, Brian, 22 November, release 2: 'Busier, mostly watchers'.) Every enemy agent that
  // has been uncovered is dealt with at once: turned into a double agent where its service has none of his yet (with
  // double agents in the campaign), arrested otherwise. With the switch off the staff leaves them alone, as before.
  if (watchersOn(g)) {
    let caught = 0;
    for (const f of foesFound(g, nat).sort((x, y) => x.id - y.id)) {
      const d = doublesOn(g) && whyNotTurn(g, nat, f.id) === null ? turnFoe(g, nat, f.id) : null;
      if (d) did.push(`turned the ${g.nations[d.of]?.adj ?? 'enemy'} agent at ${g.regions[f.reg].name} into double agent ${d.name}`);
      else if (arrest(g, nat, f.id)) caught++;
    }
    if (caught) did.push(`arrested ${plural(caught, 'enemy agent')}`);
  }
  if (!war.length && !S.peace) return did.length ? `${listAnd(did)}.` : null;
  const mine = agentsOf(g, nat).filter((a) => atWork(g, a)).sort((x, y) => x.id - y.id);
  // Where an agent is worth having: the capital of the strongest enemy, for its books, and then the ground
  // in front of the army, most divisions first, no two posts side by side (a watcher sees the provinces round it).
  const strength = (e: string): number => g.corps.reduce((s, c) => s + (c.nat === e && !c.dead ? c.divs * c.str : 0), 0);
  const capitals = war.map((e) => g.nations[e]).filter((E) => E.capital >= 0 && g.regions[E.capital].ctrl === E.id && g.regions[E.capital].owner === E.id)
    .sort((x, y) => strength(y.id) - strength(x.id) || (x.id < y.id ? -1 : 1)).slice(0, 1).map((E) => E.capital);
  const front = [...frontOf(g, nat).entries()].sort((x, y) => y[1] - x[1] || x[0] - y[0]).map(([id]) => id);
  const posts: number[] = [];
  const near = (id: number): boolean => posts.some((p) => p === id || g.regions[p].nb.includes(id));
  for (const id of [...capitals, ...front]) if (!near(id)) posts.push(id);
  // Whoever stands on a post already stays there, and whoever is on the road to one is counted as there.
  const taken = new Set<number>();
  const free: typeof mine = [];
  for (const a of agentsOf(g, nat)) if (posts.includes(a.reg) && abroad(g, a)) taken.add(a.reg);
  // The guard at home is the last to go abroad: kept back once the service has more than one agent and somebody is plotting.
  const guards = agentsOf(g, nat).length > 1 && threat(g, nat).foes.length ? S.guards : 0;
  // With the enemy's watchers, and a service big enough to keep one more back, the next agent at home hunts on the front.
  const hunters = watchersOn(g) && guards && agentsOf(g, nat).length > guards + 1 ? S.hunters ?? 0 : 0;
  const home = mine.filter((a) => !abroad(g, a));
  // Who guards and who hunts. Without watchers: the first agents at home guard. With them the guard comes first, and
  // after it the hunter: whoever hunts already goes on hunting (one on the road home to hunt counts), the guard is the
  // first of the rest, and the next one at home hunts if a hunter is still wanted. Agents in training or on the road
  // home count as coming.
  const coming = hunters ? agentsOf(g, nat).filter((a) => !atWork(g, a) && !abroad(g, a)) : [];
  const toHunt = Math.max(0, Math.min(hunters - coming.filter((a) => a.mission === 'hunt').length, home.length - guards));
  const hunting = home.filter((a) => a.mission === 'hunt').slice(0, toHunt);
  // (With watchers an agent already on guard is kept on guard before another is put there.)
  const rest = home.filter((a) => !hunting.includes(a));
  const guarding = (watchersOn(g) ? [...rest.filter((a) => a.mission === 'guard'), ...rest.filter((a) => a.mission !== 'guard')] : rest).slice(0, guards);
  hunting.push(...home.filter((a) => !hunting.includes(a) && !guarding.includes(a)).slice(0, toHunt - hunting.length));
  for (const a of mine) {
    if (abroad(g, a)) { if (!posts.includes(a.reg)) free.push(a); continue; }
    if (!guarding.includes(a) && !hunting.includes(a)) free.push(a);
  }
  // (With watchers.) A guard is on guard: a former hunter that has come to stand in its place goes back to guarding.
  if (watchersOn(g)) {
    for (const a of guarding) if (a.mission !== 'guard' && whyNotMission(g, nat, a.id, 'guard') === null && setMission(g, nat, a.id, 'guard')) did.push(`put ${a.name} back on guard at ${g.regions[a.reg].name}`);
  }
  for (const a of hunting) did.push(...hunt(g, nat, a));
  // (With watchers.) No hunter at home and none coming (its province overrun, say, with the service full): one agent
  // abroad comes home to hunt, the first with no post, else the one on the least of the posts; never one reading the
  // books or on the books post.
  if (hunting.length + coming.length < hunters) {
    const out = mine.filter((a) => abroad(g, a) && a.mission !== 'books' && !capitals.includes(a.reg));
    const back = out.find((a) => !posts.includes(a.reg)) ?? [...out].sort((x, y) => posts.indexOf(y.reg) - posts.indexOf(x.reg) || x.id - y.id)[0];
    if (back) {
      const from = back.reg, went = hunt(g, nat, back);
      if (went.length) { did.push(...went); if (free.includes(back)) free.splice(free.indexOf(back), 1); taken.delete(from); }
    }
  }
  for (const a of free) {
    const to = posts.find((p) => !taken.has(p) && whyNotSend(g, nat, a.id, p) === null);
    if (to === undefined) continue;
    if (sendAgent(g, nat, a.id, to)) { taken.add(to); did.push(`sent ${a.name} to ${g.regions[to].name}${capitals.includes(to) ? ' to read the books' : ''}`); }
  }
  // An agent in an enemy capital reads the books; anywhere else abroad it watches. It tries no sabotage.
  for (const a of mine) {
    if (!atWork(g, a) || !abroad(g, a)) continue;
    const m = whyNotMission(g, nat, a.id, 'books') === null ? 'books' : 'watch';
    if (a.mission !== m) setMission(g, nat, a.id, m);
  }
  return did.length ? `${listAnd(did)}.` : null;
}

/**
 * The service's hunter: it hunts where it stands while its search (its own province and the ones beside it) reaches
 * a province at least half as likely to hold an enemy watcher as the likeliest (watchOdds: where the services put
 * their watchers, the front where the enemy's divisions mass or, with no land front, the coast facing him).
 * Otherwise it goes to the likeliest, already ordered to hunt, so that it searches from the month it arrives. An
 * agent abroad called home to hunt goes there too. With nowhere to go it hunts where it is.
 */
function hunt(g: Game, nat: string, a: ReturnType<typeof agentsOf>[number]): string[] {
  const odds = watchOdds(g, nat);
  const busy = (r: Region): number => (r.ctrl === nat ? odds.get(r.id) ?? 0 : 0);
  let to = -1, most = 0;
  for (const r of g.regions) { const n = busy(r); if (n > most) { most = n; to = r.id; } }
  const here = g.regions[a.reg], out = abroad(g, a);
  const reach = out ? 0 : Math.max(busy(here), ...here.nb.map((j) => busy(g.regions[j])));
  if (to >= 0 && reach * 2 < most && whyNotSend(g, nat, a.id, to) === null && sendAgent(g, nat, a.id, to)) {
    // (On the road it is not at work; at home on arrival the month keeps an agent ordered to hunt at it.)
    a.mission = 'hunt';
    return [out ? `called ${a.name} home from ${here.name} to hunt enemy agents at ${g.regions[to].name}` : `sent ${a.name} to hunt enemy agents at ${g.regions[to].name}`];
  }
  if (out) return [];
  if (a.mission !== 'hunt' && whyNotMission(g, nat, a.id, 'hunt') === null && setMission(g, nat, a.id, 'hunt')) return [`set ${a.name} to hunt enemy agents at ${here.name}`];
  return [];
}

/** What the staff does with a desk, in words, for this campaign: the secret service's hunter and arrests only where the enemy's watchers are in it and its services send agents at this level. */
export function deskRuns(g: Game, id: DeskId): string {
  const d = DEPTH.desks.find((x) => x.id === id);
  if (!d) return '';
  return id === 'spies' && watchersOn(g) && enemySends(g) && d.runsMore ? `${d.runs} ${d.runsMore}` : d.runs;
}

// ---- The air war --------------------------------------------------------------------------------------

function raidWords(g: Game, r: AirReport): string {
  const at = r.region >= 0 ? g.regions[r.region] : null;
  if (!at) return `ships ${g.seas[r.zone] ? seaWhere(g.seas[r.zone].name) : `in ${r.place}`}`;
  switch (r.mission) {
    case 'bomb': return at.fac > 0 ? `the factories at ${at.name}` : `the railway at ${at.name}`;
    case 'night': return `${at.name} by night`;
    case 'ground': return `the troops at ${at.name}`;
    case 'line': return `the fieldworks at ${at.name}`;
    case 'airfield': return `the airfield at ${at.name}`;
    case 'town': return `the town of ${at.name}`;
    case 'superiority': return `the sky over ${at.name}`;
    case 'harbour': return `the ships in harbour at ${at.name}`;
    default: return at.name;
  }
}

const lossesOf = (reps: AirReport[]): { ours: number; theirs: number } => ({
  ours: reps.reduce((s, r) => s + r.att.reduce((t, x) => t + x.lostFtr + x.lostBmr, 0), 0),
  theirs: reps.reduce((s, r) => s + r.def.reduce((t, x) => t + x.lostFtr + x.lostBmr, 0), 0),
});

function air(g: Game, nat: string): string | null {
  const from = g.airReports.length;
  airAI(g, nat);
  const flown = g.airReports.slice(from).filter((r) => r.attNat === nat);
  if (!flown.length) return null;
  const L = lossesOf(flown);
  return `flew ${plural(flown.length, 'raid')}: ${some(flown.map((r) => raidWords(g, r)), 4)}. It cost ${plural(Math.round(L.ours), 'aircraft', 'aircraft')}${L.theirs >= 0.5 ? ` and shot down ${Math.round(L.theirs)}` : ''}.`;
}

// ---- The navy -----------------------------------------------------------------------------------------

function navy(g: Game, nat: string): string | null {
  const was = new Map(fleetsOf(g, nat).map((f) => [f.id, { zone: f.zone, stance: f.stance }]));
  const shore = g.shoreReports.length, strikes = g.airReports.length;
  navalAI(g, nat);
  shoreAI(g, nat);
  // The carriers go with the navy: their strikes are the admirals' to fly.
  seaStaff(g, nat, 'decks');
  const did: string[] = [];
  const out: string[] = [], back: string[] = [];
  for (const f of fleetsOf(g, nat)) {
    const w = was.get(f.id);
    if (!w || (w.zone === f.zone && w.stance === f.stance)) continue;
    // (Release 2, the far chart: one set out for a new base beyond a month's steaming.)
    if (f.stance === 'passage') out.push(`the ${f.name} on its way to ${f.port}`);
    else if (atSea(f)) out.push(`the ${f.name} into ${seaWhat(g.seas[f.zone].name)}${f.stance === 'escort' ? ' with the convoys' : f.stance === 'raid' ? ' to raid' : ''}`);
    else if (w.stance !== 'port') back.push(`the ${f.name}`);
  }
  if (out.length) did.push(`sent ${some(out)}`);
  if (back.length) did.push(`brought ${some(back)} into harbour`);
  const shelled = g.shoreReports.slice(shore).filter((r) => r.nat === nat).map((r) => g.regions[r.region].name);
  if (shelled.length) did.push(`shelled ${some([...new Set(shelled)])}`);
  const flown = g.airReports.slice(strikes).filter((r) => r.attNat === nat);
  if (flown.length) did.push(`flew ${plural(flown.length, 'carrier strike')} at ${some(flown.map((r) => raidWords(g, r)))}`);
  return did.length ? `${listAnd(did)}.` : null;
}

// ---- The staff's month --------------------------------------------------------------------------------

const WORK: Record<DeskId, (g: Game, nat: string) => string | null> = { research, build, towns: townsDesk, diplomacy, spies, air, navy };
/** The order the desks are worked in: money before the service that spends it, the fleets before the aircraft that fly from them. */
const ORDER: DeskId[] = ['research', 'build', 'towns', 'diplomacy', 'spies', 'navy', 'air'];

/**
 * True when this is word for word the line the desk gave the month before, and notes the line for the month to come.
 * The same line is not news twice: a fleet that shells the same shore every month says so once, and again when it has
 * stopped and begun anew. (Its bombardments and raids are still reported where battles are read.)
 */
export function saidLastMonth(g: Game, desk: DeskId, line: string): boolean {
  let h = 7;
  for (let i = 0; i < line.length; i++) h = (h * 31 + line.charCodeAt(i)) % 2147483647;
  const said = `dk:said:${desk}`, when = `dk:saidAt:${desk}`;
  const again = g.flags[said] === h && g.flags[when] === g.turn - 1;
  g.flags[said] = h;
  g.flags[when] = g.turn;
  return again;
}

/**
 * One desk's month, done by the staff. What the routine itself printed for the player is folded into the desk's
 * one line of news; the reports of its raids and bombardments stay where battles are read.
 */
function runDesk(g: Game, nat: string, desk: DeskId): void {
  const from = g.headlines.length;
  const line = WORK[desk](g, nat);
  const made = g.headlines.splice(from);
  for (const h of made) if (!(h.nat === nat && (h.kind === 'info' || h.kind === 'battle'))) g.headlines.push(h);
  markRan(g, desk);
  if (line && !saidLastMonth(g, desk, line)) g.headlines.push({ turn: g.turn, week: g.week, kind: 'info', text: `${deskDef(desk)!.lead} ${line}`, nat, desk });
}

/** The month's work on every desk the player has handed to his staff, as his month opens. */
export function staffMonth(g: Game): void {
  const nat = g.human;
  if (!depthRules(g) || nat === null || !g.nations[nat]?.alive) return;
  // (The far side: where no book dates Japan's strike, his admirals gather the striking fleet by the freeze, as the computer's do. readiness.ts.)
  gatherMonth(g, nat);
  for (const desk of ORDER) if (staffRuns(g, nat, desk) && !deskRan(g, desk)) runDesk(g, nat, desk);
  crossingReport(g, nat);
}

/**
 * Hands a desk to the staff or takes it back. With 'now' (the player's own month is open) a staff that is given a
 * desk goes to work on it at once, unless it has already done that desk's work this month.
 */
export function handOver(g: Game, desk: DeskId, staff: boolean, now = false): boolean {
  if (!setDesk(g, desk, staff)) return false;
  const nat = g.human;
  // Handed the navy afresh, the admirals take every fleet, those he had kept under his own orders too (1.5.1).
  if (desk === 'navy' && staff) for (const f of g.fleets) delete f.ordered;
  if (staff && now && nat !== null && g.nations[nat]?.alive && !deskRan(g, desk)) runDesk(g, nat, desk);
  return true;
}

// ---- The three depths ---------------------------------------------------------------------------------

/** Sets every switch a depth names: the desks, the cabinet, whether it can fall, and the fog of war. */
export function setDepth(g: Game, id: string, now = false): boolean {
  const d = DEPTH.depths.find((x) => x.id === id);
  if (!d || !depthRules(g)) return false;
  setCabinet(g, d.cabinet);
  setCanFall(g, d.canFall);
  setFog(g, d.fog);
  for (const k of ORDER) handOver(g, k, d.staff.includes(k), now);
  return true;
}

/** The depth the game stands at, or null where the player has made a mix of his own. */
export function depthNow(g: Game): DepthDef | null {
  if (!depthRules(g)) return null;
  for (const d of DEPTH.depths) {
    if (DESKS.some((k) => staffRuns(g, g.human, k) !== d.staff.includes(k))) continue;
    if ((g.flags.nofog !== 1) !== d.fog || cabinetOff(g) === d.cabinet) continue;
    // With no cabinet there is nobody to bring him down: the office is not asked about.
    if (d.cabinet && officeSecure(g) === d.canFall) continue;
    return d;
  }
  return null;
}

// ---- What an interface shows --------------------------------------------------------------------------

export interface DeskView {
  id: DeskId; name: string;
  /** The panel that belongs to it, or null. */
  tab: string | null;
  /** True while the staff runs it. */
  staff: boolean;
  /** What that means: the staff's way of running it, or what it asks of the player. */
  text: string;
  /** How it stands now, in a line. */
  state: string;
  /** The staff's line in this month's news, or null if it had nothing to tell. */
  news: string | null;
}

export interface DepthView {
  id: string; name: string; under: string; blurb: string;
  /** True for the depth the game stands at. */
  on: boolean;
  /** The desks on the player's own desk at this depth, and the ones the staff runs, by name. */
  yours: string[]; staff: string[];
  cabinet: boolean; canFall: boolean; fog: boolean;
}

export interface CommandView {
  /** The depth the game stands at, or null for a mix of the player's own. */
  depth: string | null;
  /** Its name, or the words for a mix. */
  name: string; under: string;
  depths: DepthView[];
  desks: DeskView[];
  /** What the staff may spend in a month, in percent of the treasury, and the steps on offer. */
  cap: number; caps: number[];
  /** What is left of this month's allowance, and what building took of it, in production points. */
  left: number; spent: number;
  /** True when the share was changed after this month's allowance was drawn: the new share takes hold next month. */
  capNext: boolean;
  cabinet: boolean; canFall: boolean; fog: boolean;
}

function stateOf(g: Game, nat: string, desk: DeskId): string {
  const n = g.nations[nat];
  switch (desk) {
    case 'research': {
      const s = techOf(g, nat), teams = teamsOf(g, nat) + spyTeamsOf(g, nat);
      if (!s) return '';
      // (Ruling 240: work on a row whose switch is off is not listed.)
      const at = workOf(s).map((w) => `${techById(w.id)?.name ?? w.id} (${months(Math.max(1, Math.ceil(w.left)))})`);
      return at.length ? `${at.length} of ${teams} team${teams === 1 ? '' : 's'} at work: ${some(at, 4)}.` : `No research team has work.`;
    }
    case 'build': {
      // (A town's building and the guns round its industry are towns and trade's: release 2.)
      const corps = n.queue.filter((b) => b.what === 'corps').length;
      const other = n.queue.filter((b) => b.what !== 'corps' && b.what !== 'civic' && !(b.what === 'work' && b.item === 'aa')).length;
      // A staff raises no corps with men the armies in the field are waiting for: say so, or a full treasury looks like idleness.
      const noMen = staffRuns(g, nat, 'build') && noMenFor(g, nat);
      // With no men to raise, the money goes into aircraft, as far as the air force can use them; what is left is a reserve.
      const air = !noMen || !DEPTH.build?.machines || !n.atWar.length ? '' : wingsWanted(g, nat).length || n.queue.some((b) => b.what === 'air')
        ? ' Your staff is putting the money into aircraft instead, which need pilots and not divisions.'
        : ' The air force has all the aircraft its pilots can fly, so what is left in the treasury is your reserve.';
      return `${corps ? `${corps} corps forming` : 'No corps forming'}${other ? `, ${other} other order${other === 1 ? '' : 's'} in hand` : ''}.${noMen ? ` There are no men to spare for new corps: the armies in the field are waiting for theirs.${air || ' Aircraft, ships and factories need few, and can be ordered by hand.'}` : ''}`;
    }
    case 'towns': {
      const up = n.queue.filter((b) => b.what === 'civic' || (b.what === 'work' && b.item === 'aa')).length;
      const deals = dealsOf(g, nat).length, houses = housesOf(g, nat);
      return `${up ? `${up} building${up === 1 ? '' : 's'} going up in your towns` : 'Nothing going up in your towns'}. ${deals} of ${houses} trading house${houses === 1 ? '' : 's'} in use.`;
    }
    case 'diplomacy': {
      const b = bloc(n);
      if (!b || b === 'league') {
        // (Out of the blocs, the backer of the Americas still courts back, at no cost, the neighbours the Axis courts: ruling 223(d).)
        // (The UI director's words, words-ruling-223.md: the envoys named as the other 223 lines name them, and who courts back.)
        const back = Object.values(g.nations).filter((T) => T.id !== nat && courtable(T) && freeCourt(g, nat, T.id));
        if (back.length) {
          const names = back.map(the), list = names.length <= 1 ? names.join('') : `${names.slice(0, -1).join(', ')} and ${names[names.length - 1]}`;
          const it = back.length > 1 ? 'them' : 'it';
          const how = staffRuns(g, nat, 'diplomacy') ? `Your staff courts ${it} back each month, at no cost.` : `Courting ${it} back costs nothing: Court back, on ${back.length > 1 ? 'their rows' : 'its row'} under Powers.`;
          return `${envoysWord(g, back.map((T) => T.id))} envoys are courting ${list}. ${how} Only a member of the Axis or the Allies courts other neutrals.`;
        }
        return 'Only a member of the Axis or the Allies courts neutrals.';
      }
      const open = Object.values(g.nations).filter((T) => T.id !== nat && courtable(T)).length;
      // Who the envoys are with, while they are at it: the news speaks only when they begin and when the country moves.
      const who = staffRuns(g, nat, 'diplomacy') ? Object.values(g.nations).filter((T) => (g.flags[`court:${nat}:${T.id}`] ?? -9) >= g.turn - 1).sort((x, y) => g.flags[`court:${nat}:${y.id}`] - g.flags[`court:${nat}:${x.id}`])[0] : undefined;
      const at = who ? ` Your envoys are courting ${the(who)}.` : '';
      return `${Math.floor(n.pc)} political capital in hand; a round of courting costs ${DIPLOMACY.court.cost}. ${open ? `${open} neutral${open === 1 ? ' is' : 's are'} open to courting.` : 'No neutral is open to courting.'}${at}`;
    }
    case 'spies': {
      const all = agentsOf(g, nat), out = all.filter((a) => abroad(g, a)).length;
      return all.length ? `${all.length} of ${mostAgents(nat, g)} agents: ${out} abroad, ${all.length - out} at home.` : `No agents yet. The service can run ${mostAgents(nat, g)}.`;
    }
    case 'air': {
      // Carrier wings are counted apart: they are aboard their fleets, at no airfield (ruling 176(b)).
      const all = airFleetsOf(g, nat), flew = all.filter((f) => f.flew === g.turn).length;
      const wings = all.filter((f) => f.ship !== undefined).length, land = all.length - wings;
      const count = [land ? `${land} air group${land === 1 ? '' : 's'} on airfields` : '', wings ? `${wings} carrier wing${wings === 1 ? '' : 's'} aboard ${wings === 1 ? 'its fleet' : 'their fleets'}` : ''].filter(Boolean).join(' and ');
      return all.length ? `${count}: ${flew} flew this month.` : 'No air groups.';
    }
    case 'navy': {
      const all = fleetsOf(g, nat), sea = all.filter((f) => atSea(f)).length;
      return all.length ? `${all.length} fleet${all.length === 1 ? '' : 's'}: ${sea} at sea, ${all.length - sea} in harbour.` : 'No fleets.';
    }
  }
}

/** Everything an interface needs to show the depths, the desks and their switches. Null outside the grand campaign or with nobody playing. */
export function commandView(g: Game): CommandView | null {
  const nat = g.human;
  if (!depthRules(g) || nat === null || !g.nations[nat]) return null;
  const now = depthNow(g);
  const nameOf = (k: DeskId): string => deskDef(k)!.name;
  return {
    depth: now?.id ?? null, name: now?.name ?? 'Your own mix', under: now?.under ?? 'Custom',
    depths: DEPTH.depths.map((d) => ({
      id: d.id, name: d.name, under: d.under, blurb: d.blurb, on: now?.id === d.id,
      yours: ['The armies and their battles', ...DESKS.filter((k) => !d.staff.includes(k)).map(nameOf)], staff: d.staff.map(nameOf),
      cabinet: d.cabinet, canFall: d.canFall, fog: d.fog,
    })),
    desks: DEPTH.desks.map((d) => {
      const staff = staffRuns(g, nat, d.id);
      const news = [...g.headlines].reverse().find((h) => h.turn === g.turn && h.desk === d.id && h.nat === nat)?.text ?? null;
      return { id: d.id, name: d.name, tab: d.tab ?? null, staff, text: staff ? deskRuns(g, d.id) : d.hand, state: stateOf(g, nat, d.id), news };
    }),
    cap: capPct(g), caps: DEPTH.cap.steps,
    left: g.flags['dk:alwt'] === g.turn ? g.flags['dk:alw'] ?? 0 : Math.max(0, g.nations[nat].pp) * capOf(g),
    spent: deskRan(g, 'build') || deskRan(g, 'towns') ? g.flags['dk:spent'] ?? 0 : 0,
    capNext: g.flags['dk:alwt'] === g.turn && g.flags['dk:alwc'] !== undefined && g.flags['dk:alwc'] !== capPct(g),
    cabinet: !cabinetOff(g), canFall: !officeSecure(g), fog: g.flags.nofog !== 1,
  };
}

/** Sets the share of the treasury the staff may spend in a month. It takes hold with the next month's allowance. */
export const setStaffCap = (g: Game, pct: number): boolean => setCap(g, pct);
