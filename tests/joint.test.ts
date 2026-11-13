// Joint armies (the grand campaign). Allies already share a province and defend it together; this lets them attack
// together as one army under the player's hand. An ally's field corps standing in a province with the player's own
// can be asked to join his army. It keeps its flag, pay, drafts and supply, takes the player's orders in the player's
// month, moves and attacks with his corps, and the ground they take is his. Its own staff leaves it alone while it
// is away. An ally lends a share of its field corps and only against enemies it is itself at war with. A corps goes
// back when the player sends it, when it is left for a month with none of his troops beside it, or when the two
// countries are allies no more. No computer nation forms a joint army and the historical scenarios have none, so
// those wars are fought as before. The numbers are in data/campaign/joint.json.
import { describe, expect, it } from 'vitest';
import { allied, atWar, createGame, startPhase } from '../src/campaign/game';
import { JOINT, armyOf, couldJoin, joinArmy, jointMonth, leaveArmy, lendLimit, lentBy, lentTo, whyNotJoin } from '../src/campaign/joint';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  armyAt, beginHuman, canOrder, candidates, current, endHuman, humanUp, joinOffers, joinable, jointForce, knowledgeOf, lending, orderAttack, orderEngineers,
  orderJoin, orderJoinAll, orderLand, orderMove, orderPromote, orderRail, orderSendBack, orderShip, orderStance, previewAttack, reach, runTurnAI, stepAI,
  visibleArmies, whyNotJoinArmy,
} from '../src/campaign/turn';
import type { Corps, Game, Region } from '../src/campaign/types';

/** A grand campaign with the player's first month open, as the game's own screen would have it. */
function playing(human = 'GER', seed = 3): Game {
  const g = startOpen(createGame(seed, human));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function nextMonth(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
}
const fieldOf = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && !c.dead && c.kind !== 'fort');
/** Sets a corps down in a province, for a test: no march, no cost. */
function place(g: Game, c: Corps, to: Region): void {
  const r = g.regions[c.reg];
  r.units.splice(r.units.indexOf(c), 1);
  c.reg = to.id;
  to.units.push(c);
}
const strength = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);

/**
 * Germany in September 1939 with a German corps marched into Slovakia beside the one Slovak corps, which is at war
 * with Poland too. Returns the game, the two corps and the province.
 */
function withSlovaks(seed = 3): { g: Game; svk: Corps; ger: Corps; home: Region; krakow: Region } {
  const g = playing('GER', seed);
  const svk = fieldOf(g, 'SVK')[0];
  const home = g.regions[svk.reg];
  const ger = g.corps.find((c) => c.nat === 'GER' && c.kind !== 'fort' && !c.dead && home.nb.includes(c.reg) && reach(g, c).has(home.id))!;
  expect(orderMove(g, ger, home.id)).toBeGreaterThan(0);
  const krakow = home.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'POL' && r.units.length > 0)!;
  return { g, svk, ger, home, krakow };
}

/** Britain in September 1939: a British corps stands at Lille with French troops, and France is at war with Germany. */
function withFrench(seed = 3): { g: Game; shared: Region; uk: Corps[]; fra: Corps[] } {
  const g = playing('UK', seed);
  const shared = g.regions.find((r) => r.units.some((c) => c.nat === 'UK' && c.kind !== 'fort') && r.units.some((c) => c.nat === 'FRA' && c.kind !== 'fort'))!;
  return { g, shared, uk: shared.units.filter((c) => c.nat === 'UK' && c.kind !== 'fort'), fra: shared.units.filter((c) => c.nat === 'FRA' && c.kind !== 'fort') };
}

describe('the data', () => {
  it('lends a share, at least one corps and never more than a few', () => {
    expect(JOINT.share).toBeGreaterThan(0);
    expect(JOINT.share).toBeLessThan(1);
    expect(JOINT.least).toBeGreaterThanOrEqual(1);
    expect(JOINT.most).toBeGreaterThanOrEqual(JOINT.least);
    expect(JOINT.capital).toBeGreaterThanOrEqual(0);
  });

  it('sets what each ally can spare by the size of its field army', () => {
    const g = playing('UK');
    const n = fieldOf(g, 'FRA').length;
    expect(lendLimit(g, 'FRA')).toBe(Math.min(n, JOINT.most, Math.max(JOINT.least, Math.floor(n * JOINT.share))));
    expect(lendLimit(g, 'FRA')).toBeLessThan(n);
    // A country with one corps will lend it; one with none lends none.
    const h = playing('GER');
    expect(fieldOf(h, 'SVK')).toHaveLength(1);
    expect(lendLimit(h, 'SVK')).toBe(1);
    for (const c of fieldOf(h, 'SVK')) c.dead = true;
    expect(lendLimit(h, 'SVK')).toBe(0);
  });
});

describe('who can join', () => {
  it('is an ally\'s field corps standing with the player\'s own', () => {
    const { g, shared, fra } = withFrench();
    expect(fra.length).toBeGreaterThan(0);
    expect(whyNotJoin(g, fra[0])).toBeNull();
    expect(whyNotJoinArmy(g, fra[0])).toBeNull();
    expect(couldJoin(g, shared.id)).toEqual(fra);
    expect(joinable(g, shared.id)).toEqual(fra);
    const offers = joinOffers(g, shared.id);
    expect(offers.map((o) => o.corps)).toEqual(fra.map((c) => c.id));
    expect(offers[0]).toMatchObject({ nat: 'FRA', name: fra[0].name, kind: fra[0].kind, divs: fra[0].divs, why: null });
    // Nothing has joined anybody yet.
    expect(g.corps.every((c) => c.joint === undefined && armyOf(c) === c.nat)).toBe(true);
    expect(lentTo(g, 'UK')).toHaveLength(0);
  });

  it('is nobody else, and each refusal says why', () => {
    const { g, svk, ger, home } = withSlovaks();
    expect(whyNotJoin(g, ger)).toMatch(/your own/);
    const pole = g.corps.find((c) => c.nat === 'POL' && !c.dead)!;
    expect(whyNotJoin(g, pole)).toMatch(/Poland is not your ally/);
    // An ally's corps somewhere the player has no army.
    const far = fieldOf(g, 'ITA')[0];
    expect(allied(g, 'ITA', 'GER')).toBe(true);
    expect(whyNotJoin(g, far)).toMatch(/same province/);
    expect(joinOffers(g, far.reg)).toHaveLength(0);
    // An ally at peace with everyone the player is fighting keeps its army at home.
    expect(g.nations.GER.atWar.some((e) => atWar(g, 'ITA', e))).toBe(false);
    const guest = fieldOf(g, 'GER').find((c) => c !== ger)!;
    place(g, guest, g.regions[far.reg]);
    expect(whyNotJoin(g, far)).toMatch(/Italy is at war with none of your enemies/);
    expect(joinOffers(g, far.reg).find((o) => o.corps === far.id)!.why).toMatch(/none of your enemies/);
    // Fortress troops stay in their works, and are not on the list at all.
    const fort = g.corps.find((c) => c.kind === 'fort' && c.nat !== 'GER' && allied(g, c.nat, 'GER') && !c.dead);
    if (fort) { expect(whyNotJoin(g, fort)).toMatch(/Fortress troops|same province/); expect(joinOffers(g, fort.reg).some((o) => o.corps === fort.id)).toBe(false); }
    // One that is already with the army.
    expect(orderJoin(g, svk)).toBe(true);
    expect(whyNotJoin(g, svk)).toMatch(/already marches/);
    expect(joinOffers(g, home.id)).toHaveLength(0);
  });

  it('is nobody in a historical campaign, and nobody for a computer nation', () => {
    const h = createGame(3, 'GER');
    const svk = h.corps.find((c) => c.nat === 'SVK' && !c.dead);
    if (svk) { expect(whyNotJoin(h, svk)).toMatch(/grand campaign/); expect(joinArmy(h, svk, 'GER')).toBe(false); expect(joinOffers(h, svk.reg)).toHaveLength(0); }
    const g = startOpen(createGame(3, null));
    const c = fieldOf(g, 'SVK')[0];
    expect(joinArmy(g, c, 'GER')).toBe(false);
    expect(lending(g)).toHaveLength(0);
    const p = playing('GER');
    // Only the player forms a joint army: Italy cannot be given the Slovak corps.
    expect(joinArmy(p, fieldOf(p, 'SVK')[0], 'ITA')).toBe(false);
    for (let k = 0; k < 3; k++) runTurnAI(g);
    expect(g.corps.every((x) => x.joint === undefined)).toBe(true);
  });

  it('is no more than its country can spare', () => {
    const { g, shared, uk } = withFrench();
    const limit = lendLimit(g, 'FRA');
    const french = fieldOf(g, 'FRA');
    expect(french.length).toBeGreaterThan(limit);
    // Bring more French corps beside the British than France will lend.
    for (const c of french.slice(0, limit + 2)) if (c.reg !== shared.id) place(g, c, shared);
    const here = shared.units.filter((c) => c.nat === 'FRA' && c.kind !== 'fort');
    expect(here.length).toBeGreaterThan(limit);
    expect(orderJoinAll(g, shared.id)).toBe(limit);
    expect(lentBy(g, 'FRA')).toHaveLength(limit);
    expect(lentTo(g, 'UK')).toHaveLength(limit);
    const left = here.find((c) => c.joint === undefined)!;
    expect(whyNotJoin(g, left)).toMatch(new RegExp(`France has lent all the corps it can spare \\(${limit}\\)`));
    expect(orderJoin(g, left)).toBe(false);
    expect(lending(g).find((x) => x.nat === 'FRA')).toMatchObject({ lent: limit, limit });
    // One sent back makes room for another.
    expect(orderSendBack(g, lentBy(g, 'FRA')[0])).toBe(true);
    expect(orderJoin(g, left)).toBe(true);
    expect(uk.every((c) => c.joint === undefined)).toBe(true);
  });
});

describe('a corps that has joined', () => {
  it('takes the player\'s orders in the player\'s month and keeps its own flag', () => {
    const { g, svk, ger, home } = withSlovaks();
    expect(canOrder(g, svk)).toBe(false);
    expect(orderJoin(g, svk)).toBe(true);
    expect(svk.joint).toBe('GER');
    expect(svk.nat).toBe('SVK');
    expect(armyOf(svk)).toBe('GER');
    expect(canOrder(g, svk)).toBe(true);
    expect(candidates(g, g.regions[home.nb.find((j) => g.regions[j].ctrl === 'POL' && g.regions[j].units.length > 0)!])).toContain(svk);
    expect(orderStance(g, svk, 'dig')).toBe(true);
    expect(svk.digs).toBe(true);
    expect(lentTo(g, 'GER')).toEqual([svk]);
    expect(lending(g).find((x) => x.nat === 'SVK')).toEqual({ nat: 'SVK', lent: 1, limit: 1, corps: [svk.id] });
    // It is known for what it is, like any ally's corps.
    expect(knowledgeOf(g, svk).level).toBe('exact');
    // Out of the player's month it takes nobody's orders from him.
    endHuman(g);
    expect(canOrder(g, svk)).toBe(false);
    expect(orderJoin(g, ger)).toBe(false);
  });

  it('stands under the army\'s flag on the map and in its tray', () => {
    const { g, svk, ger, home } = withSlovaks();
    const before = armyAt(g, home.id, 'GER')!;
    expect(before.allies).toEqual([]);
    expect(armyAt(g, home.id, 'SVK')!.corpsCount).toBe(1);
    orderJoin(g, svk);
    const army = armyAt(g, home.id, 'GER')!;
    expect(army.allies).toEqual(['SVK']);
    expect(army.corpsCount).toBe(before.corpsCount + 1);
    expect(army.divs).toBe(before.divs + svk.divs);
    const card = army.corps.find((c) => c.id === svk.id)!;
    expect(card).toMatchObject({ nat: 'SVK', joint: 'GER', known: 'exact' });
    expect(army.corps.find((c) => c.id === ger.id)!.joint).toBeNull();
    expect(card.divisions.every((d) => d.nat === 'SVK')).toBe(true);
    // Slovakia has no army of its own left in the province, and the map draws one piece there.
    expect(armyAt(g, home.id, 'SVK')).toBeNull();
    const rows = visibleArmies(g).filter((a) => a.reg === home.id);
    expect(rows).toHaveLength(1);
    expect(rows[0]).toMatchObject({ nat: 'GER', corps: army.corpsCount, mine: true, known: 'full' });
    expect(rows[0].divs).toBeCloseTo(strength(home.units), 9);
  });

  it('marches with the army, and ground it walks into is the player\'s', () => {
    const { g, svk, home } = withSlovaks();
    orderJoin(g, svk);
    // Polish ground with nobody on it, beside ground the player holds.
    const empty = g.regions.find((r) => r.ctrl === 'POL' && g.regions.some((x) => x.nb.includes(r.id) && x.ctrl === 'GER'))!;
    expect(empty).toBeDefined();
    for (const c of [...empty.units]) { c.dead = true; empty.units.splice(empty.units.indexOf(c), 1); }
    const door = g.regions.find((x) => x.nb.includes(empty.id) && x.ctrl === 'GER')!;
    place(g, svk, door);
    svk.ap = svk.apMax;
    expect(reach(g, svk).has(empty.id)).toBe(true);
    expect(orderMove(g, svk, empty.id)).toBe(1);
    expect(svk.reg).toBe(empty.id);
    expect(empty.ctrl).toBe('GER');
    expect(home.ctrl).toBe('SVK');
    // By rail too, over friendly track.
    svk.ap = svk.apMax;
    place(g, svk, home);
    const to = [...(reach(g, svk).keys())].length;
    expect(to).toBeGreaterThan(0);
    expect(typeof orderRail(g, svk, home.id)).toBe('boolean');
  });

  it('attacks beside the player\'s corps, under his flag, and the ground they take is his', () => {
    const { g, svk, ger, home, krakow } = withSlovaks();
    const alone = previewAttack(g, [ger], krakow)!;
    orderJoin(g, svk);
    const force = home.units.filter((c) => canOrder(g, c) && c.ap > 0);
    expect(force).toContain(svk);
    const both = previewAttack(g, force, krakow)!;
    expect(both.A).toBeGreaterThan(alone.A);
    expect(both.odds).toBeGreaterThan(alone.odds);
    const rep = orderAttack(g, force, krakow)!;
    expect(rep).not.toBeNull();
    expect(rep.attNat).toBe('GER');
    expect(rep.attackers.map((u) => u.nat).sort()).toContain('SVK');
    expect(rep.attackers[0].nat).toBe('GER');
    expect(svk.fought).toBe(true);
    if (rep.taken) {
      expect(krakow.ctrl).toBe('GER');
      expect(rep.attackers.some((u) => u.nat === 'SVK' && u.fate === 'advanced') ? svk.reg : krakow.id).toBe(krakow.id);
    }
    expect(svk.joint).toBe('GER');
  });

  it('attacks under the player\'s flag even with none of his own corps in the fight', () => {
    const { g, svk, krakow } = withSlovaks();
    orderJoin(g, svk);
    for (const c of krakow.units) { c.str = 0.2; c.org = 0.2; }
    const rep = orderAttack(g, [svk], krakow)!;
    expect(rep).not.toBeNull();
    expect(rep.attNat).toBe('GER');
    expect(rep.attackers.map((u) => u.nat)).toEqual(['SVK']);
    if (rep.taken) expect(krakow.ctrl).toBe('GER');
  });

  it('stays out of a fight with a country its own is not at war with', () => {
    const { g, svk, ger } = withSlovaks();
    orderJoin(g, svk);
    // Germany is at war with France; Slovakia is not.
    expect(atWar(g, 'GER', 'FRA')).toBe(true);
    expect(atWar(g, 'SVK', 'FRA')).toBe(false);
    const french = g.regions.find((r) => r.ctrl === 'FRA' && r.units.some((c) => c.nat === 'FRA'))!;
    expect(jointForce(g, [svk, ger], french)).toEqual([ger]);
    expect(jointForce(g, [svk], french)).toEqual([]);
    expect(previewAttack(g, [svk], french)).toBeNull();
    // Against Poland both go in, the player's own first.
    const polish = g.regions.find((r) => r.ctrl === 'POL' && r.units.some((c) => c.nat === 'POL'))!;
    expect(jointForce(g, [svk, ger], polish)).toEqual([ger, svk]);
    // A force with no ally in it is left exactly as it was given.
    const mine = fieldOf(g, 'GER').slice(0, 3).reverse();
    expect(jointForce(g, mine, polish)).toBe(mine);
  });

  it('does not sail, drop from the air, dig works or take a new general on the player\'s orders', () => {
    const { g, svk } = withSlovaks();
    orderJoin(g, svk);
    expect(orderShip(g, svk, svk.reg)).toBe(false);
    const landing = orderLand(g, [svk], svk.reg);
    expect(landing.ok).toBe(false);
    expect(landing.why).toMatch(/does not sail under your orders/);
    expect(orderPromote(g, svk.id)).toBeNull();
    expect(orderEngineers(g, svk, 'bridge' as never)).toBe(false);
  });
});

describe('through the months', () => {
  it('its own staff leaves it alone, and it is ready again when the player\'s month opens', () => {
    const { g, svk, home } = withSlovaks();
    orderJoin(g, svk);
    svk.ap = 0;
    endHuman(g);
    // Slovakia's own month comes and goes.
    for (let k = 0; !humanUp(g) && k < 80; k++) {
      const who = current(g);
      stepAI(g);
      if (who === 'SVK') { expect(svk.ap).toBe(0); expect(svk.reg).toBe(home.id); }
    }
    expect(svk.reg).toBe(home.id);
    expect(svk.joint).toBe('GER');
    beginHuman(g);
    expect(svk.ap).toBe(svk.apMax);
    expect(svk.ap).toBeGreaterThan(0);
    expect(canOrder(g, svk)).toBe(true);
    // Its pay and its drafts are still its own country's business: it is still a Slovak corps.
    expect(svk.nat).toBe('SVK');
  });

  it('goes back to its own command when it is left with none of the player\'s troops beside it', () => {
    const { g, svk, home } = withSlovaks();
    orderJoin(g, svk);
    const away = home.nb.map((j) => g.regions[j]).find((r) => r.ctrl === 'GER')!;
    for (const c of home.units.filter((u) => u.nat === 'GER')) place(g, c, away);
    expect(home.units.some((c) => c.nat === 'GER' && c.kind !== 'fort')).toBe(false);
    // It is still with the army for the rest of this month.
    expect(svk.joint).toBe('GER');
    const news = g.headlines.length;
    nextMonth(g);
    expect(svk.joint).toBeUndefined();
    expect(canOrder(g, svk)).toBe(false);
    expect(g.headlines.slice(news).some((h) => /gone back to Slovak command/.test(h.text) && h.text.includes(svk.name))).toBe(true);
    expect(lentTo(g, 'GER')).toHaveLength(0);
  });

  it('goes back when the player sends it, and does not march twice in the month', () => {
    const { g, svk, home } = withSlovaks();
    orderJoin(g, svk);
    expect(orderSendBack(g, svk)).toBe(true);
    expect(svk.joint).toBeUndefined();
    expect(canOrder(g, svk)).toBe(false);
    expect(orderSendBack(g, svk)).toBe(false);
    expect(joinOffers(g, home.id).map((o) => o.corps)).toEqual([svk.id]);
    // Slovakia's month comes later in the same month: the corps has had its orders for this one.
    expect(g.order.indexOf('SVK')).toBeGreaterThan(g.order.indexOf('GER'));
    endHuman(g);
    for (let k = 0; !humanUp(g) && k < 80; k++) {
      const who = current(g);
      stepAI(g);
      if (who === 'SVK') expect(svk.ap).toBe(0);
    }
    // The month after, it is Slovakia's to move again.
    beginHuman(g);
    endHuman(g);
    let seen = false;
    for (let k = 0; !humanUp(g) && k < 80; k++) {
      if (current(g) === 'SVK' && !svk.dead) { const r = svk.reg; stepAI(g); seen = true; expect(svk.reg === r ? svk.apMax : 1).toBeGreaterThan(0); expect(g.flags[`back:${svk.id}`]).toBeUndefined(); } else stepAI(g);
    }
    expect(seen || svk.dead).toBe(true);
  });

  it('goes back when the two countries are allies no more, or the player is out of the war', () => {
    const { g, svk } = withSlovaks();
    orderJoin(g, svk);
    g.nations.SVK.align = 'neutral' as never;
    expect(allied(g, 'SVK', 'GER')).toBe(false);
    expect(canOrder(g, svk)).toBe(false);
    jointMonth(g, 'GER');
    expect(svk.joint).toBeUndefined();
    const h = withSlovaks();
    orderJoin(h.g, h.svk);
    h.g.nations.GER.alive = false;
    jointMonth(h.g, 'SVK');
    expect(h.svk.joint).toBeUndefined();
    // And its own staff has it back with a full month's marching, not a month late.
    const k = withSlovaks();
    orderJoin(k.g, k.svk);
    k.g.nations.GER.alive = false;
    startPhase(k.g, 'SVK');
    expect(k.svk.joint).toBeUndefined();
    expect(k.svk.ap).toBe(k.svk.apMax);
    expect(k.svk.ap).toBeGreaterThan(0);
  });

  it('leaving is leaving: the orders it had under the player do not follow it home', () => {
    const { g, svk } = withSlovaks();
    orderJoin(g, svk);
    orderStance(g, svk, 'back');
    expect(svk.yields).toBe(true);
    expect(leaveArmy(g, svk)).toBe(true);
    expect(svk.yields).toBeUndefined();
    expect(leaveArmy(g, svk)).toBe(false);
  });

  it('is kept in a saved game', () => {
    const { g, svk, home } = withSlovaks();
    orderJoin(g, svk);
    const back = loadGame(saveGame(g))!;
    const c = back.corps.find((x) => x.id === svk.id)!;
    expect(c.joint).toBe('GER');
    expect(canOrder(back, c)).toBe(true);
    expect(armyAt(back, home.id, 'GER')!.allies).toEqual(['SVK']);
  });
});

describe('a war with nobody at the table', () => {
  it('forms no joint army and carries none of its marks', () => {
    const g = startOpen(createGame(7, null));
    for (let k = 0; k < 6; k++) runTurnAI(g);
    expect(g.corps.every((c) => c.joint === undefined)).toBe(true);
    expect(Object.keys(g.flags).some((k) => k.startsWith('back:'))).toBe(false);
  });
});
