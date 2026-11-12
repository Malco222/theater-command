// Small countries and the League (the grand campaign). A small country in the player's hands chooses its own path:
// join the Axis, join the Allies, stand alone, or found or join the League, a third alliance of smaller nations that
// belongs to neither bloc. The League is a pact of mutual defence: war on one member is war on all. For computer-run
// countries it is rare: a circle of friends founds it in the month after one of their friends is attacked, or once in
// a great while unprovoked. The historical scenarios know none of it.
import { describe, expect, it } from 'vitest';
import grandData from '../data/campaign/grand.json';
import { rival, setLevel } from '../src/campaign/difficulty';
import { DIPLOMACY, canCourt, courtable, momentum, monthlyDiplomacy, sneakOn, tradePartners } from '../src/campaign/diplomacy';
import { allied, atWar, bloc, createGame, declareWar } from '../src/campaign/game';
import { LEAGUE, deterred, founders, invites, leagueFounded, leagueMembers, leagueMonth, ownPath, pathOptions, small, standsWith, takePath, threatened, willing } from '../src/campaign/league';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { declarable, humanUp, leagueInvites, leaguePartners, orderDeclare, orderLeagueInvite, orderPath, passMonth, paths, previewDeclare, runTurnAI, whyNotLeagueInvite } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
/** A campaign with the player's small country up and its first month open. */
const mine = (nat: string, seed = 3): Game => { const g = grand(nat, seed); if (!humanUp(g)) expect(passMonth(g)).toBe(true); return g; };
/** Puts countries into the League without waiting for the dice. */
const enrol = (g: Game, ...ids: string[]): void => {
  g.flags['league:born'] ??= g.turn;
  for (const id of ids) { const n = g.nations[id]; n.league = true; n.align = 'independent'; n.opinion = n.home = 0; g.flags[`league:${id}`] = g.turn; }
  g.supplyDirty = true;
};
const ids = (ns: { id: string }[]): string[] => ns.map((n) => n.id).sort();
const told = (g: Game, re: RegExp, from = 0): boolean => g.headlines.slice(from).some((h) => re.test(h.text));
/** Runs fn with the League's odds changed, and puts them back. */
function withOdds<T>(found: Partial<typeof LEAGUE.found>, join: Partial<typeof LEAGUE.join>, fn: () => T): T {
  const f = { ...LEAGUE.found }, j = { ...LEAGUE.join };
  Object.assign(LEAGUE.found, found); Object.assign(LEAGUE.join, join);
  try { return fn(); } finally { Object.assign(LEAGUE.found, f); Object.assign(LEAGUE.join, j); }
}

describe('the data', () => {
  it('keeps the great powers out and makes the League rare', () => {
    for (const id of ['GER', 'ITA', 'UK', 'FRA', 'SOV', 'USA']) { expect(LEAGUE.never).toContain(id); expect(small(id)).toBe(false); }
    for (const id of DIPLOMACY.majors) expect(small(id)).toBe(false);
    for (const id of ['SWE', 'NOR', 'BEL', 'ROM', 'TUR', 'IRL']) expect(small(id)).toBe(true);
    expect(LEAGUE.found.shock).toBeGreaterThan(0);
    expect(LEAGUE.found.shock).toBeLessThanOrEqual(0.3);
    expect(LEAGUE.found.month).toBeLessThan(0.005);
    expect(LEAGUE.found.least).toBeGreaterThanOrEqual(2);
    expect(LEAGUE.join.friend).toBeGreaterThan(LEAGUE.join.threat);
    expect(LEAGUE.join.most).toBeGreaterThanOrEqual(4);
    expect(LEAGUE.invite.cost).toBeGreaterThan(0);
    expect(LEAGUE.color).toMatch(/^#[0-9a-f]{6}$/i);
  });

  it('offers sixteen small countries in the grand campaign, from September 1939', () => {
    const g = grand();
    const list = (grandData as unknown as { nations: { id: string; small?: boolean; years: Record<string, string> }[] }).nations;
    const smalls = list.filter((n) => n.small);
    expect(smalls.length).toBe(16);
    for (const n of smalls) {
      const N = g.nations[n.id];
      expect(N, n.id).toBeDefined();
      expect(small(n.id), n.id).toBe(true);
      expect(N.alive && !N.dormant, n.id).toBe(true);
      expect(bloc(N), n.id).toBeNull();
      expect(Object.keys(n.years)).toEqual(['1939']);
      expect(n.years['1939'].length).toBeGreaterThan(40);
    }
    for (const n of list.filter((x) => !x.small)) expect(['GER', 'UK', 'FRA', 'SOV', 'ITA', 'POL']).toContain(n.id);
    expect(new Set(list.map((n) => n.id)).size).toBe(list.length);
  });
});

describe('history is untouched', () => {
  it('founds no League, offers no path and keeps its treaties', () => {
    const g = createGame(3, 'ROM');
    expect(ownPath(g, 'ROM')).toBe(false);
    expect(pathOptions(g, 'ROM')).toEqual([]);
    expect(paths(g)).toEqual([]);
    expect(leaguePartners(g)).toEqual([]);
    expect(takePath(g, 'ROM', 'allies')).toBe(false);
    withOdds({ shock: 1, month: 1 }, { friend: 1, threat: 1 }, () => { g.turn = 12; leagueMonth(g); monthlyDiplomacy(g); });
    expect(leagueFounded(g)).toBe(false);
    expect(Object.values(g.nations).some((n) => n.league)).toBe(false);
    expect(Object.keys(g.flags).some((k) => k.startsWith('league:') || k.startsWith('shock:'))).toBe(false);
    // A neutral that is attacked still joins the other side, whoever plays it.
    const h = createGame(3, 'DEN');
    declareWar(h, 'GER', 'DEN');
    expect(h.nations['DEN'].align).toBe('allies');
    expect(Object.keys(h.flags).some((k) => k.startsWith('shock:'))).toBe(false);
  });

  it('plays a year with every nation on the computer and nobody in a League', () => {
    const g = createGame(5, null);
    withOdds({ shock: 1, month: 1 }, { friend: 1, threat: 1 }, () => { for (let k = 0; k < 12; k++) runTurnAI(g); });
    expect(Object.values(g.nations).some((n) => n.league)).toBe(false);
    expect(leagueFounded(g)).toBe(false);
  });
});

describe('the League as an alliance', () => {
  it('is a third bloc: its members are allies of one another and of nobody else', () => {
    const g = grand();
    enrol(g, 'SWE', 'FIN', 'NOR');
    for (const id of ['SWE', 'FIN', 'NOR']) expect(bloc(g.nations[id])).toBe('league');
    expect(ids(leagueMembers(g))).toEqual(['FIN', 'NOR', 'SWE']);
    expect(allied(g, 'SWE', 'FIN')).toBe(true);
    expect(allied(g, 'NOR', 'FIN')).toBe(true);
    for (const o of ['GER', 'UK', 'DEN', 'SOV']) expect(allied(g, 'SWE', o), o).toBe(false);
    // Off the track between the blocs: nobody courts a member, it trades with neither side, and holds no weight in who is winning.
    expect(courtable(g.nations['SWE'])).toBe(false);
    expect(canCourt(g, 'GER', 'SWE').ok).toBe(false);
    expect(tradePartners(g, 'GER').map((n) => n.id)).not.toContain('SWE');
    expect(tradePartners(g, 'SWE')).toEqual([]);
    const m = momentum(g);
    g.byName['Jutland'].ctrl = 'SWE';
    expect(momentum(g)).toBe(m);
    g.byName['Jutland'].ctrl = 'DEN';
  });

  it('stands together: war on one member is war on them all', () => {
    const g = grand();
    enrol(g, 'SWE', 'FIN', 'NOR');
    const news = g.headlines.length;
    declareWar(g, 'GER', 'NOR');
    for (const id of ['NOR', 'SWE', 'FIN']) expect(atWar(g, 'GER', id), id).toBe(true);
    expect(told(g, /The League stands by Norway: .*Sweden.* at war with Germany too/, news)).toBe(true);
    // The member attacked stays in the League: it does not run to the other bloc.
    expect(g.nations['NOR'].align).toBe('independent');
    expect(bloc(g.nations['NOR'])).toBe('league');
    expect(allied(g, 'NOR', 'UK')).toBe(false);
    // Germany's ally is not at war with the League, and the League is not at war with the Allies' enemies in general.
    expect(atWar(g, 'ITA', 'SWE')).toBe(false);
    // The surprise is on the country struck, not on those who come to its aid.
    expect(sneakOn(g, 'GER', 'NOR')).toBe(true);
    expect(sneakOn(g, 'GER', 'SWE')).toBe(false);
    // A member that had already fallen stays out of it.
    const h = grand();
    enrol(h, 'SWE', 'FIN', 'NOR');
    h.nations['FIN'].alive = false;
    declareWar(h, 'GER', 'SWE');
    expect(atWar(h, 'GER', 'NOR')).toBe(true);
    expect(atWar(h, 'GER', 'FIN')).toBe(false);
  });

  it('is a pact of defence: a member that goes to war of its own accord goes alone', () => {
    const g = grand();
    enrol(g, 'SWE', 'FIN');
    declareWar(g, 'SWE', 'NOR');
    expect(atWar(g, 'SWE', 'NOR')).toBe(true);
    expect(atWar(g, 'FIN', 'NOR')).toBe(false);
    // The country a member attacks does not run to a bloc for it: the League is neither side's enemy.
    expect(g.nations['NOR'].align).toBe('independent');
    // And members never fight each other.
    declareWar(g, 'SWE', 'FIN');
    expect(atWar(g, 'SWE', 'FIN')).toBe(false);
  });

  it('tells the player what attacking a member would bring down on him', () => {
    const g = grand('GER');
    enrol(g, 'SWE', 'FIN', 'DEN');
    expect(standsWith(g, 'GER', 'DEN').sort()).toEqual(['FIN', 'SWE']);
    expect(standsWith(g, 'GER', 'NOR')).toEqual([]);
    expect(standsWith(g, 'SWE', 'DEN')).toEqual([]);
    expect(declarable(g)).toContain('DEN');
    const f = previewDeclare(g, 'DEN')!;
    expect(f.league.sort()).toEqual(['FIN', 'SWE']);
    expect(f.joins).toBeNull();
    expect(f.alarm).toEqual([]); // the neutrals on the track take no side in a quarrel with the League
    expect(f.sneak).toBeGreaterThan(1);
    expect(previewDeclare(g, 'NED')!.league).toEqual([]);
    expect(orderDeclare(g, 'DEN')).toBe(true);
    for (const id of ['DEN', 'SWE', 'FIN']) expect(atWar(g, 'GER', id), id).toBe(true);
    expect(bloc(g.nations['DEN'])).toBe('league');
  });

  it('makes a staff think twice before attacking a member', () => {
    const g = grand();
    expect(LEAGUE.deter).toBeGreaterThan(0);
    expect(LEAGUE.deter).toBeLessThan(1);
    expect(deterred(g, 'GER', ['DEN', 'NOR'])).toBe(1); // no League yet
    enrol(g, 'SWE', 'FIN', 'NOR');
    expect(deterred(g, 'GER', ['DEN', 'NOR'])).toBe(LEAGUE.deter);
    expect(deterred(g, 'GER', ['NED', 'BEL', 'LUX'])).toBe(1);
    expect(deterred(g, 'GER', [])).toBe(1);
    expect(deterred(g, 'SWE', ['NOR'])).toBe(1); // a member's staff has no such worry
    // A member standing alone, its partners fallen, deters nobody.
    g.nations['SWE'].alive = false; g.nations['FIN'].alive = false;
    expect(deterred(g, 'GER', ['NOR'])).toBe(1);
    // History has no League to be deterred by.
    const h = createGame(3, null);
    enrol(h, 'SWE', 'NOR');
    expect(deterred(h, 'GER', ['NOR'])).toBe(1);
  });

  it('cannot be annexed by treaty, and survives a save', () => {
    const g = grand();
    enrol(g, 'EST', 'LAT');
    g.nations['POL'].alive = false;
    g.turn = 11;
    monthlyDiplomacy(g);
    // Moscow takes the one Baltic state that stood alone, and leaves the two that stand together.
    expect(g.nations['LIT'].alive).toBe(false);
    expect(g.nations['EST'].alive).toBe(true);
    expect(g.nations['LAT'].alive).toBe(true);
    expect(g.byName['Estonia'].ctrl).toBe('EST');
    const back = loadGame(saveGame(g))!;
    expect(ids(leagueMembers(back))).toEqual(['EST', 'LAT']);
    expect(bloc(back.nations['EST'])).toBe('league');
    expect(leagueFounded(back)).toBe(true);
    expect(allied(back, 'EST', 'LAT')).toBe(true);
  });
});

describe('computer-run countries', () => {
  it('found the League when a friend is struck down, if the dice allow', () => {
    const g = grand();
    declareWar(g, 'GER', 'DEN');
    expect(g.flags['shock:DEN']).toBe(g.turn);
    g.turn++;
    // With the odds at nothing, nothing happens.
    withOdds({ shock: 0, month: 0 }, {}, () => leagueMonth(g));
    expect(leagueFounded(g)).toBe(false);
    const news = g.headlines.length;
    withOdds({ shock: 1 }, { friend: 0, threat: 0 }, () => leagueMonth(g));
    expect(leagueFounded(g)).toBe(true);
    expect(g.flags['league:born']).toBe(g.turn);
    // Denmark's Nordic friends, still at peace and leaning to neither side. Denmark itself is at war and no founder.
    expect(ids(leagueMembers(g))).toEqual(['FIN', 'NOR', 'SWE']);
    expect(told(g, /found the League of Free Nations as the war reaches their neighbours/, news)).toBe(true);
    // There is only ever one League.
    withOdds({ shock: 1, month: 1 }, { friend: 0, threat: 0 }, () => { g.turn++; leagueMonth(g); });
    expect(ids(leagueMembers(g))).toEqual(['FIN', 'NOR', 'SWE']);
  });

  it('need a shock last month, two founders, and nobody leaning to a side', () => {
    const g = grand();
    declareWar(g, 'GER', 'DEN');
    g.turn += 2; // the fright has passed
    withOdds({ shock: 1, month: 0 }, {}, () => leagueMonth(g));
    expect(leagueFounded(g)).toBe(false);
    // Only one Nordic country left free: no League.
    const h = grand();
    h.nations['NOR'].align = 'leaning_allies';
    h.nations['FIN'].align = 'leaning_axis';
    declareWar(h, 'GER', 'DEN');
    h.turn++;
    withOdds({ shock: 1, month: 0 }, {}, () => leagueMonth(h));
    expect(leagueFounded(h)).toBe(false);
    // A great power's war on another great power frightens nobody into a League.
    const k = grand();
    declareWar(k, 'GER', 'SOV');
    expect(k.flags['shock:SOV']).toBe(k.turn); // the Soviet Union is outside both blocs, but it has no circle of small friends
    k.turn++;
    withOdds({ shock: 1, month: 0 }, {}, () => leagueMonth(k));
    expect(leagueFounded(k)).toBe(false);
  });

  it('may found it unprovoked once in a great while, but never in the first months', () => {
    const g = grand();
    g.turn = LEAGUE.found.after;
    withOdds({ shock: 0, month: 1 }, {}, () => leagueMonth(g));
    expect(leagueFounded(g)).toBe(false);
    g.turn = LEAGUE.found.after + 1;
    withOdds({ shock: 0, month: 1 }, { friend: 0, threat: 0 }, () => leagueMonth(g));
    expect(leagueFounded(g)).toBe(true);
    const members = leagueMembers(g);
    expect(members.length).toBeGreaterThanOrEqual(LEAGUE.found.least);
    for (const n of members) { expect(small(n.id)).toBe(true); expect(n.atWar.length).toBe(0); }
  });

  it('join it later: friends of a member sooner, others only with the war at their border', () => {
    const g = grand();
    enrol(g, 'SWE', 'FIN');
    expect(willing(g, 'NOR', ['SWE', 'FIN'])).toMatchObject({ ok: true, friend: true });
    expect(willing(g, 'DEN', ['SWE', 'FIN'])).toMatchObject({ ok: true, friend: true });
    // Belgium has Germany, at war, on its border. Portugal has nobody at war near it.
    expect(threatened(g, 'BEL')).toBe(true);
    expect(threatened(g, 'POR')).toBe(false);
    expect(willing(g, 'BEL', ['SWE', 'FIN'])).toMatchObject({ ok: true, friend: false });
    expect(willing(g, 'POR', ['SWE', 'FIN']).ok).toBe(false);
    // Leaning countries, countries at war and great powers refuse.
    expect(willing(g, 'HUN', ['SWE', 'FIN']).why).toMatch(/Leans toward the Axis/);
    expect(willing(g, 'POL', ['SWE', 'FIN']).ok).toBe(false);
    expect(willing(g, 'SOV', ['SWE', 'FIN']).why).toMatch(/great power/);
    g.turn++;
    withOdds({}, { friend: 1, threat: 0 }, () => leagueMonth(g));
    expect(ids(leagueMembers(g))).toEqual(['DEN', 'FIN', 'NOR', 'SWE']);
    expect(told(g, /joins the League/)).toBe(true);
    g.turn++;
    withOdds({}, { friend: 1, threat: 1, most: 6 }, () => leagueMonth(g));
    const members = leagueMembers(g);
    expect(members.length).toBe(6); // no more than it can hold together
    expect(members.every((n) => small(n.id) && !n.atWar.length)).toBe(true);
    expect(ids(members)).not.toContain('POR');
    expect(ids(members)).not.toContain('HUN');
  });

  it('never enrol the player\'s country for him', () => {
    const g = mine('NOR');
    declareWar(g, 'GER', 'DEN');
    g.turn++;
    withOdds({ shock: 1 }, { friend: 1, threat: 1 }, () => { leagueMonth(g); g.turn++; leagueMonth(g); });
    expect(leagueFounded(g)).toBe(true);
    expect(g.nations['NOR'].league).toBeFalsy();
    expect(ids(leagueMembers(g))).toEqual(expect.arrayContaining(['FIN', 'SWE']));
  });
});

describe('a small country in the player\'s hands', () => {
  it('has four paths, and stands alone until it chooses', () => {
    const g = mine('SWE');
    expect(ownPath(g, 'SWE')).toBe(true);
    const opts = paths(g);
    expect(opts.map((o) => o.id)).toEqual(['axis', 'allies', 'alone', 'found']);
    expect(opts.find((o) => o.id === 'alone')).toMatchObject({ now: true, ok: false });
    expect(opts.find((o) => o.id === 'axis')).toMatchObject({ now: false, ok: true });
    expect(opts.find((o) => o.id === 'allies')).toMatchObject({ now: false, ok: true });
    expect(opts.find((o) => o.id === 'found')).toMatchObject({ now: false, ok: true });
    // The great powers have no such card, nor has a watcher.
    expect(paths(grand('GER'))).toEqual([]);
    expect(paths(grand('SOV'))).toEqual([]);
    expect(paths(grand(null))).toEqual([]);
  });

  it('can join the Axis, for good', () => {
    const g = mine('SWE');
    const news = g.headlines.length;
    expect(orderPath(g, 'axis')).toBe(true);
    expect(g.nations['SWE'].align).toBe('axis');
    expect(bloc(g.nations['SWE'])).toBe('axis');
    expect(allied(g, 'SWE', 'GER')).toBe(true);
    expect(g.nations['SWE'].atWar).toEqual([]); // at war with nobody until someone declares it
    expect(told(g, /Sweden joins the Axis/, news)).toBe(true);
    const opts = paths(g);
    expect(opts.find((o) => o.id === 'axis')).toMatchObject({ now: true, ok: false });
    expect(opts.filter((o) => o.ok)).toEqual([]);
    expect(orderPath(g, 'allies')).toBe(false);
    expect(orderPath(g, 'alone')).toBe(false);
    expect(orderPath(g, 'found', 'NOR')).toBe(false);
    // As an ally of Germany it can now reach, and declare war on, Germany's enemies.
    expect(declarable(g)).toEqual(expect.arrayContaining(['FRA', 'UK']));
  });

  it('can join the Allies, unless the Allies have no leader left', () => {
    const g = mine('BEL');
    expect(orderPath(g, 'allies')).toBe(true);
    expect(bloc(g.nations['BEL'])).toBe('allies');
    expect(allied(g, 'BEL', 'FRA')).toBe(true);
    const h = mine('BEL');
    h.nations['UK'].alive = false;
    expect(paths(h).find((o) => o.id === 'allies')!.ok).toBe(false);
    expect(paths(h).find((o) => o.id === 'allies')!.why).toMatch(/no leader/);
    expect(orderPath(h, 'allies')).toBe(false);
    expect(paths(h).find((o) => o.id === 'axis')!.ok).toBe(true);
  });

  it('can step back from a bloc it leans toward', () => {
    const g = mine('HUN');
    expect(g.nations['HUN'].align).toBe('leaning_axis');
    expect(tradePartners(g, 'GER').map((n) => n.id)).toContain('HUN');
    expect(paths(g).find((o) => o.id === 'alone')).toMatchObject({ now: false, ok: true });
    expect(orderPath(g, 'alone')).toBe(true);
    expect(g.nations['HUN'].align).toBe('independent');
    expect(tradePartners(g, 'GER').map((n) => n.id)).not.toContain('HUN');
    expect(paths(g).find((o) => o.id === 'alone')).toMatchObject({ now: true, ok: false });
  });

  it('is left where its player puts it: no drift, no courting, no treaty over its head', () => {
    const g = mine('ROM');
    // The summer of 1940 as history had it: Poland fallen, Germany winning, Moscow and Budapest with claims.
    g.nations['POL'].alive = false;
    g.nations['ROM'].opinion = 80;
    expect(canCourt(g, 'GER', 'ROM').ok).toBe(false);
    for (const t of [11, 13, 16, 20]) { g.turn = t; monthlyDiplomacy(g); }
    expect(g.nations['ROM'].align).toBe('independent');
    expect(g.nations['ROM'].opinion).toBe(80);
    expect(bloc(g.nations['ROM'])).toBeNull();
    for (const nm of ['Bessarabia', 'Bukovina', 'Transylvania']) { expect(g.byName[nm].owner, nm).toBe('ROM'); expect(g.byName[nm].ctrl, nm).toBe('ROM'); }
    expect(told(g, /demands Bessarabia and Bukovina\. Romania gives up nothing without a fight/)).toBe(true);
    // The same months with Romania on the computer: the ultimatum is served and the land goes.
    const h = grand('GER');
    h.nations['POL'].alive = false;
    h.turn = 11;
    monthlyDiplomacy(h);
    expect(h.byName['Bessarabia'].owner).toBe('SOV');
  });

  it('is not annexed by treaty', () => {
    const g = mine('LIT');
    g.nations['POL'].alive = false;
    g.turn = 11;
    monthlyDiplomacy(g);
    expect(g.nations['LIT'].alive).toBe(true);
    expect(g.byName['Lithuania'].ctrl).toBe('LIT');
    expect(g.nations['EST'].alive).toBe(false);
    expect(g.nations['LAT'].alive).toBe(false);
  });

  it('stays its own master when attacked, and can then take a side', () => {
    const g = mine('DEN');
    declareWar(g, 'GER', 'DEN');
    expect(g.nations['DEN'].align).toBe('independent');
    expect(bloc(g.nations['DEN'])).toBeNull();
    expect(allied(g, 'DEN', 'UK')).toBe(false);
    const opts = paths(g);
    expect(opts.find((o) => o.id === 'axis')).toMatchObject({ ok: false });
    expect(opts.find((o) => o.id === 'axis')!.why).toMatch(/at war with the Axis/);
    expect(opts.find((o) => o.id === 'allies')!.ok).toBe(true);
    // The League is founded in peace: a country already fighting finds no partners.
    expect(opts.find((o) => o.id === 'found')!.ok).toBe(false);
    expect(orderPath(g, 'allies')).toBe(true);
    expect(allied(g, 'DEN', 'UK')).toBe(true);
    expect(atWar(g, 'GER', 'DEN')).toBe(true);
    // The same blow against Denmark on the computer sends it to the Allies at once, as before.
    const h = grand('GER');
    declareWar(h, 'GER', 'DEN');
    expect(h.nations['DEN'].align).toBe('allies');
  });

  it('gives orders only in its own month', () => {
    const g = grand('SWE');
    expect(humanUp(g)).toBe(false); // Sweden moves late in the month
    expect(paths(g).find((o) => o.id === 'axis')!.ok).toBe(true); // the card can be read at any time
    expect(orderPath(g, 'axis')).toBe(false);
    expect(g.nations['SWE'].align).toBe('independent');
    expect(whyNotLeagueInvite(g, 'NOR')).toMatch(/Not in your month/);
  });
});

describe('the player\'s League', () => {
  it('is founded with a willing partner, friends first on the list', () => {
    const g = mine('SWE');
    const list = leaguePartners(g);
    expect(list).toEqual(founders(g, 'SWE'));
    const yes = list.filter((x) => x.ok).map((x) => x.nat);
    expect(yes.slice(0, 3).sort()).toEqual(['DEN', 'FIN', 'NOR']);
    expect(yes).toContain('BEL');
    expect(list.find((x) => x.nat === 'HUN')).toMatchObject({ ok: false });
    expect(list.find((x) => x.nat === 'POR')).toMatchObject({ ok: false });
    expect(list.some((x) => ['GER', 'UK', 'SOV', 'POL', 'SWE'].includes(x.nat))).toBe(false);
    // Unwilling ones come after every willing one.
    const order = list.map((x) => Number(x.ok));
    expect(order).toEqual([...order].sort((a, b) => b - a));
    expect(orderPath(g, 'found')).toBe(false); // it takes two
    expect(orderPath(g, 'found', 'HUN')).toBe(false);
    expect(orderPath(g, 'found', 'GER')).toBe(false);
    expect(leagueFounded(g)).toBe(false);
    const news = g.headlines.length;
    expect(orderPath(g, 'found', 'FIN')).toBe(true);
    expect(leagueFounded(g)).toBe(true);
    expect(ids(leagueMembers(g))).toEqual(['FIN', 'SWE']);
    expect(allied(g, 'SWE', 'FIN')).toBe(true);
    expect(told(g, /Sweden and Finland found the League of Free Nations/, news)).toBe(true);
    const opts = paths(g);
    expect(opts.map((o) => o.id)).toEqual(['axis', 'allies', 'alone', 'join']);
    expect(opts.find((o) => o.id === 'join')).toMatchObject({ now: true, ok: false });
    expect(opts.filter((o) => o.ok)).toEqual([]);
  });

  it('grows by invitation, for political capital', () => {
    const g = mine('SWE');
    expect(leagueInvites(g)).toEqual([]); // not a member yet
    expect(whyNotLeagueInvite(g, 'NOR')).toMatch(/Only a member/);
    expect(orderPath(g, 'found', 'FIN')).toBe(true);
    const list = leagueInvites(g);
    expect(list).toEqual(invites(g, 'SWE'));
    expect(list.find((x) => x.nat === 'NOR')).toMatchObject({ ok: true, friend: true });
    expect(list.some((x) => x.nat === 'FIN' || x.nat === 'SWE')).toBe(false);
    g.nations['SWE'].pc = LEAGUE.invite.cost - 1;
    expect(whyNotLeagueInvite(g, 'NOR')).toMatch(/political capital/);
    expect(orderLeagueInvite(g, 'NOR')).toBe(false);
    g.nations['SWE'].pc = LEAGUE.invite.cost + 2;
    expect(whyNotLeagueInvite(g, 'NOR')).toBeNull();
    expect(orderLeagueInvite(g, 'NOR')).toBe(true);
    expect(g.nations['SWE'].pc).toBe(2);
    expect(ids(leagueMembers(g))).toEqual(['FIN', 'NOR', 'SWE']);
    expect(told(g, /Norway joins the League at Sweden's invitation/)).toBe(true);
    // Those who would refuse cannot be bought.
    g.nations['SWE'].pc = 30;
    expect(whyNotLeagueInvite(g, 'HUN')).toMatch(/Leans toward the Axis/);
    expect(orderLeagueInvite(g, 'HUN')).toBe(false);
    expect(orderLeagueInvite(g, 'GER')).toBe(false);
    expect(orderLeagueInvite(g, 'NOR')).toBe(false); // already in
    expect(g.nations['SWE'].pc).toBe(30);
  });

  it('defends the player, and the player defends it', () => {
    const g = mine('SWE');
    expect(orderPath(g, 'found', 'FIN')).toBe(true);
    declareWar(g, 'SOV', 'FIN');
    expect(atWar(g, 'SOV', 'SWE')).toBe(true);
    expect(told(g, /The League stands by Finland: Sweden is at war with the Soviet Union too/)).toBe(true);
    const h = mine('SWE');
    expect(orderPath(h, 'found', 'FIN')).toBe(true);
    declareWar(h, 'GER', 'SWE');
    expect(atWar(h, 'GER', 'FIN')).toBe(true);
    expect(bloc(h.nations['SWE'])).toBe('league');
  });

  it('can be joined once it exists, and its members are no rivals at any level', () => {
    const g = mine('SWE');
    enrol(g, 'NOR', 'DEN');
    const opts = paths(g);
    expect(opts.map((o) => o.id)).toEqual(['axis', 'allies', 'alone', 'join']);
    expect(opts.find((o) => o.id === 'join')).toMatchObject({ now: false, ok: true });
    setLevel(g, 'general');
    expect(rival(g, 'NOR')).toBe(true); // not yet on the player's side
    expect(orderPath(g, 'join')).toBe(true);
    expect(ids(leagueMembers(g))).toEqual(['DEN', 'NOR', 'SWE']);
    expect(told(g, /Sweden joins the League/)).toBe(true);
    expect(rival(g, 'NOR')).toBe(false);
    expect(rival(g, 'GER')).toBe(true);
    // A League whose members have all fallen takes in nobody.
    const h = mine('SWE');
    enrol(h, 'NOR');
    h.nations['NOR'].alive = false;
    expect(paths(h).find((o) => o.id === 'join')).toMatchObject({ ok: false });
    expect(orderPath(h, 'join')).toBe(false);
  });
});
