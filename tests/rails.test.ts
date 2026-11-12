// The grand campaign's railways. Troops go by rail only along the lines on the map; a province's line is working
// or cut, and a cut line carries nothing until engineers relay it. Lines are cut by troops driven out of a
// province, by order and by bombing. An army lives off its railhead and fights weaker the further by road it
// stands from it. Heavy bombing wrecks factories until engineers rebuild them. The historical scenarios use none of it.
import { describe, expect, it } from 'vitest';
import { Rng, hashString } from '../src/core/rng';
import { airBattles, airFleetsOf, forecastAir, previewAir, setMission, whyNotMission } from '../src/campaign/air';
import { income, provinceYield } from '../src/campaign/economy';
import { aiEngineers, engineersMonth, jobLine, jobsFor, orderJob } from '../src/campaign/engineers';
import { EASTERN, addCorps, allied, assess, capture, computeSupply, createGame, ensureSupply, moveCorps, railMove, railReach } from '../src/campaign/game';
import { groundRules } from '../src/campaign/ground';
import {
  RAILWAYS, RAIL_LINKS, bombFactories, bombRail, computeRailheads, cutOn, cutRail, mendRail, onNet, railBombChance, railCut, railNote, railOnCapture,
  railOpen, railheadAt, railheads, railsAt, rebuildFactory, ruinChance, ruinMost, ruinedAt, tearUp, whyNotCutRail, workingFactories, wreckChance,
} from '../src/campaign/rails';
import { CROSSINGS, crossingOf } from '../src/campaign/rivers';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { orderCutRailway, runTurnAI, whyNotCutRailway } from '../src/campaign/turn';
import { KIND, type Corps, type Game, type Kind, type Region } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = null): Game => startOpen(createGame(seed, human));
const corps = (g: Game, nat: string, kind: Kind, at: Region, divs = 3): Corps => addCorps(g, nat, `Test ${KIND[kind].label} Corps ${g.nextCorps}`, kind, divs, at.id, {});
/** Empties a region of troops and returns it. */
const clear = (r: Region): Region => { for (const c of r.units) c.dead = true; r.units = []; return r; };
/** Cuts a province's line as the game records it, without redrawing the supply picture. */
const sever = (g: Game, r: Region): void => { g.flags[`rail:${r.id}`] = g.turn; };
/** The same thing worked out with one number in the railway data changed. */
const with_ = <T>(table: Record<string, number>, key: string, value: number, run: () => T): T => { const was = table[key]; table[key] = value; try { return run(); } finally { table[key] = was; } };
/** How many provinces of road lie between each province and a start, through ground the test allows. */
function hops(g: Game, from: number, pass: (r: Region) => boolean): Int16Array {
  const d = new Int16Array(g.regions.length).fill(-1);
  d[from] = 0;
  const q = [from];
  for (let i = 0; i < q.length; i++) for (const j of g.regions[q[i]].nb) if (d[j] < 0 && pass(g.regions[j])) { d[j] = d[q[i]] + 1; q.push(j); }
  return d;
}
/** Everything a train of this nation can reach from a province along working, friendly track. */
function byTrain(g: Game, nat: string, from: number): Set<number> {
  const seen = new Set<number>([from]);
  const q = [from];
  for (let i = 0; i < q.length; i++) for (const j of railsAt(q[i])) if (!seen.has(j) && railOpen(g, nat, q[i], j)) { seen.add(j); q.push(j); }
  seen.delete(from);
  return seen;
}
const nextMonth = (g: Game, nat: string): void => { g.turn++; engineersMonth(g, nat); };
const engineersOf = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && c.kind === 'eng' && !c.dead);
const line = (g: Game, attackers: Corps[], target: Region, label: string): string[] => assess(g, attackers, target).lines.filter((l) => l.label === label).map((l) => `${l.value}. ${l.effect ?? ''}`);

describe('the railway net', () => {
  it('lays every line between two neighbouring provinces and crosses the border between them', () => {
    const g = grand();
    expect(RAIL_LINKS.length).toBeGreaterThan(400);
    const seen = new Set<string>();
    for (const l of RAIL_LINKS) {
      const a = g.regions[l.a], b = g.regions[l.b];
      expect(l.a).toBeLessThan(l.b);
      expect(a.nb.includes(b.id), `${a.name} and ${b.name}`).toBe(true);
      expect(a.terrain === 'desert' || b.terrain === 'desert', `${a.name} and ${b.name}`).toBe(false);
      expect(seen.has(`${l.a}:${l.b}`), `${a.name} and ${b.name} twice`).toBe(false);
      seen.add(`${l.a}:${l.b}`);
      expect(Math.hypot(l.x - (a.x + b.x) / 2, l.y - (a.y + b.y) / 2)).toBeLessThan(Math.hypot(a.x - b.x, a.y - b.y));
      expect(railsAt(a.id)).toContain(b.id);
      expect(railsAt(b.id)).toContain(a.id);
      // Where a river runs between the two, the line goes over the bridge.
      const x = crossingOf(a.id, b.id);
      if (x) { expect(l.x).toBe(x.x); expect(l.y).toBe(x.y); }
    }
    expect(RAIL_LINKS.some((l) => CROSSINGS.some((x) => x.a === l.a && x.b === l.b))).toBe(true);
  });

  it('reaches every province outside the desert, and the main lines history built are there', () => {
    const g = grand();
    const off = g.regions.filter((r) => !onNet(r.id)).map((r) => r.name);
    // Nothing on the mainland of Europe is off the net: only the desert and a few islands.
    for (const name of off) { const r = g.byName[name]; expect(r.terrain === 'desert' || r.overseas || r.nb.length <= 1, name).toBe(true); }
    const trunks: string[][] = [
      ['Berlin', 'Neumark', 'Poznan', 'Lodz', 'Warsaw', 'Brest', 'Polesie', 'Minsk', 'Mogilev', 'Smolensk', 'Vyazma', 'Moscow'],
      ['Moscow', 'Kalinin', 'Valdai', 'Novgorod', 'Leningrad'],
      ['Paris', 'Aube', 'Burgundy', 'Lyon', 'Marseille'],
      ['Moscow', 'Tula', 'Orel', 'Kursk', 'Kharkov'],
    ];
    for (const t of trunks) for (let i = 0; i < t.length - 1; i++) expect(railsAt(g.byName[t[i]].id), `${t[i]} to ${t[i + 1]}`).toContain(g.byName[t[i + 1]].id);
    // Narvik's only line is the ore railway to Sweden.
    expect(railsAt(g.byName['Narvik'].id).every((j) => g.regions[j].owner === 'SWE')).toBe(true);
  });

  it("joins up each country's own net, so that none depends on a neighbour's line", () => {
    const g = grand();
    for (const [nat, capital, far] of [['FRA', 'Paris', 'Lorraine'], ['POL', 'Warsaw', 'Wilno'], ['ROM', 'Bucharest', 'Bessarabia'], ['YUG', 'Belgrade', 'Slovenia'], ['ESP', 'Madrid', 'Galicia']] as const) {
      const from = g.byName[capital], to = g.byName[far];
      if (!from || !to) continue;
      const seen = new Set<number>([from.id]);
      const q = [from.id];
      for (let i = 0; i < q.length; i++) for (const j of railsAt(q[i])) if (!seen.has(j) && g.regions[j].owner === nat) { seen.add(j); q.push(j); }
      expect(seen.has(to.id), `${capital} to ${far}`).toBe(true);
    }
  });
});

describe('history is untouched', () => {
  it('has no lines to cut, no railheads and no ruins in a historical scenario', () => {
    const g = createGame(3, 'GER');
    expect(groundRules(g)).toBe(false);
    const berlin = g.byName['Berlin'];
    sever(g, berlin);
    g.flags[`ruin:${berlin.id}`] = 2;
    expect(railCut(g, berlin.id)).toBe(false);
    expect(cutOn(g, berlin.id)).toBeUndefined();
    expect(cutRail(g, g.byName['Hamburg'].id)).toBe(false);
    expect(whyNotCutRail(g, 'GER', berlin.id)).toMatch(/no railways to cut/);
    expect(tearUp(g, 'GER', berlin.id)).toBe(false);
    expect(ruinedAt(g, berlin)).toBe(0);
    expect(workingFactories(g, berlin)).toBe(berlin.fac);
    expect(railNote(g, berlin.id)).toBeNull();
    expect(railBombChance(g, berlin.id, 2000)).toBe(0);
    expect(ruinChance(g, berlin, 0.3)).toBe(0);
    computeSupply(g);
    // The supply picture marks no depots, and nobody is counted as far from a railhead.
    for (const t of Object.values(g.supply)) expect(t.includes(2)).toBe(false);
    expect(g.railhead).toEqual({});
    expect(railheads(g, 'GER')).toBeNull();
    expect(railheadAt(g, 'GER', g.byName['Vienna'].id)).toEqual({ road: 0, attack: 1, defence: 1 });
  });

  it('lets the trains run wherever friendly ground is joined, as they always did', () => {
    const g = createGame(3, 'GER');
    const c = corps(g, 'GER', 'inf', g.byName['Berlin']);
    const reach = railReach(g, c);
    // Stettin and Neumark have no line between them on the map, and Neumark's only German line runs through Berlin.
    expect(railsAt(g.byName['Stettin'].id)).not.toContain(g.byName['Neumark'].id);
    sever(g, g.byName['Berlin']);
    expect(railReach(g, c)).toEqual(reach);
    expect(reach.size).toBe(hops(g, c.reg, (r) => allied(g, 'GER', r.ctrl) && r.terrain !== 'desert').filter((d) => d > 0).length);
  });

  it('bombs only industry there', () => {
    const g = createGame(3, 'GER');
    const f = airFleetsOf(g, 'GER').find((x) => x.bmr > 0)!;
    expect(whyNotMission(g, f, 'bomb', g.byName['Poznan'].id)).toBe('There is no industry there to bomb.');
    const fc = forecastAir(g, previewAir(g, f, 'bomb', g.byName['Warsaw'].id), 'bomb', g.byName['Warsaw'].id);
    expect(fc.rail).toBeUndefined();
    expect(fc.ruin).toBeUndefined();
  });
});

describe('going by rail', () => {
  it('keeps to the lines on the map', () => {
    const g = grand();
    const c = corps(g, 'GER', 'inf', g.byName['Berlin']);
    const reach = railReach(g, c);
    expect(reach).toEqual(byTrain(g, 'GER', c.reg));
    expect(reach.has(g.byName['Vienna'].id)).toBe(true);
    // Fewer places than friendly ground alone would allow: East Prussia lies beyond Polish track.
    expect(reach.has(g.byName['Konigsberg'].id)).toBe(false);
  });

  it('runs no train into, out of or through a province whose line is cut', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    const c = corps(g, 'GER', 'inf', berlin);
    const before = railReach(g, c);
    const saxony = g.byName['Saxony'];
    expect(before.has(saxony.id)).toBe(true);
    expect(cutRail(g, saxony.id)).toBe(true);
    expect(cutOn(g, saxony.id)).toBe(g.turn);
    expect(cutRail(g, saxony.id)).toBe(false);
    const after = railReach(g, c);
    expect(after.has(saxony.id)).toBe(false);
    expect(after).toEqual(byTrain(g, 'GER', berlin.id));
    expect(railMove(g, c, saxony)).toBe(false);
    // With every line out of Berlin cut, nothing leaves by train.
    for (const j of railsAt(berlin.id)) cutRail(g, j);
    expect(railReach(g, c).size).toBe(0);
    // And a corps standing in a cut province cannot entrain at all.
    for (const j of railsAt(berlin.id)) mendRail(g, j);
    cutRail(g, berlin.id);
    expect(railReach(g, c).size).toBe(0);
    expect(railMove(g, c, g.byName['Hamburg'])).toBe(false);
    expect(mendRail(g, berlin.id)).toBe(true);
    expect(railReach(g, c)).toEqual(before);
  });

  it('carries engineers alone up to the break', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    const eng = engineersOf(g, 'GER').find((c) => c.reg === berlin.id)!;
    const inf = corps(g, 'GER', 'inf', berlin);
    const friendly = railsAt(berlin.id).filter((j) => allied(g, 'GER', g.regions[j].ctrl));
    for (const j of friendly) cutRail(g, j);
    expect(railReach(g, inf).size).toBe(0);
    expect([...railReach(g, eng)].sort()).toEqual([...friendly].sort());
    expect(railMove(g, eng, g.regions[friendly[0]])).toBe(true);
    expect(eng.reg).toBe(friendly[0]);
  });

  it('crosses no river whose bridges are down', () => {
    const g = grand();
    const berlin = g.byName['Berlin'], neumark = g.byName['Neumark'];
    expect(railOpen(g, 'GER', berlin.id, neumark.id)).toBe(true);
    const x = crossingOf(berlin.id, neumark.id)!;
    g.flags[`br:${x.a}:${x.b}`] = g.turn;
    expect(railOpen(g, 'GER', berlin.id, neumark.id)).toBe(false);
    expect(railReach(g, corps(g, 'GER', 'inf', berlin)).has(neumark.id)).toBe(false);
    // No train runs onto an enemy's track either.
    expect(railOpen(g, 'GER', neumark.id, g.byName['Poznan'].id)).toBe(false);
  });
});

describe('cutting the line', () => {
  it('is done by order, by troops on the spot, in wartime', () => {
    const g = grand(3, 'GER');
    const berlin = g.byName['Berlin'], hesse = clear(g.byName['Hesse']);
    expect(whyNotCutRailway(g, hesse.id)).toMatch(/no troops of yours/);
    expect(whyNotCutRailway(g, g.byName['Warsaw'].id)).toMatch(/not in friendly hands/);
    const desert = g.regions.find((r) => !onNet(r.id) && r.terrain === 'desert')!;
    expect(whyNotCutRail(g, 'GER', desert.id)).toMatch(/No railway runs through/);
    expect(whyNotCutRail(g, 'SOV', g.byName['Moscow'].id)).toMatch(/peacetime/);
    expect(whyNotCutRailway(g, berlin.id)).toBeNull();
    g.supplyDirty = false;
    expect(orderCutRailway(g, berlin.id)).toBe(true);
    expect(railCut(g, berlin.id)).toBe(true);
    expect(g.supplyDirty).toBe(true);
    expect(g.headlines[g.headlines.length - 1].text).toMatch(/tear up the railway at Berlin/);
    expect(whyNotCutRailway(g, berlin.id)).toMatch(/already cut/);
    expect(orderCutRailway(g, berlin.id)).toBe(false);
    // Nobody gives the order for another nation, and nobody at all in a game without a player.
    expect(orderCutRailway(g, g.byName['Paris'].id)).toBe(false);
    expect(orderCutRailway(grand(), berlin.id)).toBe(false);
  });

  it('is likelier when the loser still has troops next door, and less likely when fast troops arrive first', () => {
    const g = grand();
    const W = RAILWAYS.wreck;
    const lodz = clear(g.byName['Lodz']);
    for (const j of lodz.nb) clear(g.regions[j]);
    expect(wreckChance(g, lodz, 'GER')).toBeCloseTo(W.unguarded);
    corps(g, 'POL', 'inf', g.byName['Warsaw']);
    expect(wreckChance(g, lodz, 'GER')).toBeCloseTo(W.chance);
    corps(g, 'GER', 'inf', lodz);
    expect(wreckChance(g, lodz, 'GER')).toBeCloseTo(W.chance);
    corps(g, 'GER', 'arm', lodz);
    expect(wreckChance(g, lodz, 'GER')).toBeCloseTo(W.chance + W.fastCaptor);
    expect(W.fastCaptor).toBeLessThan(0);
    expect(W.unguarded).toBeLessThan(W.chance);
  });

  it('happens on the dice of the province when it is lost, and shifts nothing else', () => {
    const g = grand();
    const polish = g.regions.filter((r) => r.ctrl === 'POL' && onNet(r.id));
    let torn = 0, whole = 0;
    const dice = g.rng.state;
    for (const r of polish) {
      const want = new Rng(hashString(`rail:${g.seed}:${g.turn}:${r.id}`)).next() < wreckChance(g, r, 'GER');
      railOnCapture(g, r, 'GER', 'POL');
      expect(railCut(g, r.id), r.name).toBe(want);
      if (want) torn++; else whole++;
    }
    expect(torn).toBeGreaterThan(0);
    expect(whole).toBeGreaterThan(0);
    expect(g.rng.state).toBe(dice);
    // A line already cut is not cut again, and stays cut whoever holds the ground.
    const cut = polish.find((r) => railCut(g, r.id))!;
    const since = cutOn(g, cut.id);
    g.turn += 2;
    railOnCapture(g, cut, 'GER', 'POL');
    expect(cutOn(g, cut.id)).toBe(since);
  });

  it('follows the capture of ground in war, and only in war', () => {
    const g = grand(3, 'GER');
    const always = (run: () => void): void => with_(RAILWAYS.wreck as unknown as Record<string, number>, 'chance', 9, () => with_(RAILWAYS.wreck as unknown as Record<string, number>, 'unguarded', 9, run));
    // Find a Polish province the dice will wreck at the highest chance there can be, and take it.
    const taken = g.regions.find((r) => r.ctrl === 'POL' && onNet(r.id) && new Rng(hashString(`rail:${g.seed}:${g.turn}:${r.id}`)).next() < 0.95)!;
    always(() => capture(g, taken, 'GER'));
    expect(taken.ctrl).toBe('GER');
    expect(railCut(g, taken.id)).toBe(true);
    expect(g.headlines.some((h) => /Polish troops tear up the railway/.test(h.text) && h.text.includes(taken.name))).toBe(true);
    // Ground that changes hands between nations not at war keeps its line.
    const neutral = g.regions.filter((r) => r.ctrl === 'DEN' && onNet(r.id));
    expect(neutral.length).toBeGreaterThan(0);
    always(() => { for (const r of neutral) capture(g, r, 'GER'); });
    for (const r of neutral) expect(railCut(g, r.id), r.name).toBe(false);
  });

  it('cuts the line behind an army that marches in', () => {
    const g = grand();
    const from = g.byName['Neumark'], to = clear(g.byName['Poznan']);
    const c = corps(g, 'GER', 'inf', from);
    const want = new Rng(hashString(`rail:${g.seed}:${g.turn}:${to.id}`)).next() < wreckChance(g, to, 'GER');
    expect(moveCorps(g, c, to)).toBe(true);
    // The captor is in the province by the time the dice are thrown, and infantry is not fast enough to matter.
    expect(railCut(g, to.id)).toBe(want);
  });
});

describe('the railhead', () => {
  /** Germany with Berlin its only depot and Berlin's own line cut: everything is counted by road from Berlin. */
  function fromBerlin(): { g: Game; d: Int16Array } {
    const g = grand();
    ensureSupply(g);
    const ok = g.supply.GER, berlin = g.byName['Berlin'];
    for (let i = 0; i < ok.length; i++) if (ok[i] === 2) ok[i] = 1;
    ok[berlin.id] = 2;
    sever(g, berlin);
    computeRailheads(g);
    return { g, d: hops(g, berlin.id, (r) => !!ok[r.id]) };
  }

  it('is everywhere at the start: every home province stands on working track', () => {
    const g = grand();
    ensureSupply(g);
    for (const nat of ['GER', 'FRA', 'POL', 'SOV', 'UK', 'ITA']) {
      const t = railheads(g, nat)!;
      expect(t, nat).toBeDefined();
      for (const r of g.regions) {
        if (r.owner !== nat || r.ctrl !== nat || r.terrain === 'desert' || r.overseas || !g.supply[nat][r.id]) continue;
        expect(t[r.id], `${nat} at ${r.name}`).toBe(0);
        expect(railheadAt(g, nat, r.id)).toEqual({ road: 0, attack: 1, defence: 1 });
      }
    }
    // Where a nation draws no supply at all, there is no railhead to count from.
    expect(railheads(g, 'GER')![g.byName['Moscow'].id]).toBe(-1);
  });

  it('counts a step for every province of road beyond the working line', () => {
    const { g, d } = fromBerlin();
    const t = railheads(g, 'GER')!;
    let far = 0;
    for (const r of g.regions) {
      if (!g.supply.GER[r.id]) { expect(t[r.id]).toBe(-1); continue; }
      expect(t[r.id], r.name).toBe(d[r.id]);
      far = Math.max(far, d[r.id]);
    }
    expect(far).toBeGreaterThanOrEqual(5);
    // Relay Berlin's line and the whole net is fed again.
    mendRail(g, g.byName['Berlin'].id);
    computeRailheads(g);
    expect(railheads(g, 'GER')![g.byName['Vienna'].id]).toBe(0);
  });

  it('costs nothing for two provinces, then weakens an army the further it goes', () => {
    const { g, d } = fromBerlin();
    const bands = RAILWAYS.railhead;
    expect(bands[0]).toEqual([2, 1, 1]);
    const at = (n: number): Region => g.regions.find((r) => d[r.id] === n && r.terrain !== 'desert' && !r.overseas && r.owner !== 'SOV')!;
    expect(railheadAt(g, 'GER', g.byName['Berlin'].id)).toEqual({ road: 0, attack: 1, defence: 1 });
    expect(railheadAt(g, 'GER', at(1).id)).toEqual({ road: 1, attack: 1, defence: 1 });
    expect(railheadAt(g, 'GER', at(2).id)).toEqual({ road: 2, attack: 1, defence: 1 });
    expect(railheadAt(g, 'GER', at(3).id)).toEqual({ road: 3, attack: bands[1][1], defence: bands[1][2] });
    expect(railheadAt(g, 'GER', at(4).id)).toEqual({ road: 4, attack: bands[1][1], defence: bands[1][2] });
    expect(railheadAt(g, 'GER', at(5).id)).toEqual({ road: 5, attack: bands[2][1], defence: bands[2][2] });
    expect(bands[2][1]).toBeLessThan(bands[1][1]);
    expect(bands[1][1]).toBeLessThan(1);
    // Defence suffers less than attack.
    expect(bands[1][2]).toBeGreaterThan(bands[1][1]);
  });

  it('shows on the Battle Ticket for the attack and for the defence', () => {
    const { g, d } = fromBerlin();
    // A German province three or four roads from Berlin with Polish ground next to it.
    const from = g.regions.find((r) => r.ctrl === 'GER' && d[r.id] >= 3 && d[r.id] <= 4 && r.nb.some((j) => g.regions[j].ctrl === 'POL'))!;
    expect(from, 'a frontier province far from Berlin').toBeDefined();
    const target = g.regions[from.nb.find((j) => g.regions[j].ctrl === 'POL')!];
    clear(from); clear(target);
    const a = corps(g, 'GER', 'inf', from), p = corps(g, 'POL', 'inf', target);
    const far = assess(g, [a], target).A;
    expect(line(g, [a], target, 'Railhead')).toEqual([`Attackers as far as ${d[from.id]} provinces by road beyond their nearest working railway. Attack cut to as little as ${Math.round(RAILWAYS.railhead[1][1] * 100)}% until engineers bring the line forward`]);
    // The same German corps defending there is weaker too, by less.
    const thin = assess(g, [p], from).D;
    expect(line(g, [p], from, 'Railhead')).toEqual([`Defenders as far as ${d[from.id]} provinces by road beyond their nearest working railway. Defence cut to as little as ${Math.round(RAILWAYS.railhead[1][2] * 100)}% until their line is relaid`]);
    // Relay the line and both are whole again.
    delete g.flags[`rail:${g.byName['Berlin'].id}`];
    computeRailheads(g);
    expect(line(g, [a], target, 'Railhead')).toEqual([]);
    expect(far / assess(g, [a], target).A).toBeCloseTo(RAILWAYS.railhead[1][1], 6);
    expect(thin / assess(g, [p], from).D).toBeCloseTo(RAILWAYS.railhead[1][2], 6);
  });

  it('is redrawn whenever a line is cut or relaid or a bridge goes down', () => {
    const g = grand();
    ensureSupply(g);
    // Neumark's only German line runs over the Oder from Berlin.
    const neumark = g.byName['Neumark'], berlin = g.byName['Berlin'];
    expect(railheads(g, 'GER')![neumark.id]).toBe(0);
    cutRail(g, neumark.id);
    expect(g.supplyDirty).toBe(true);
    ensureSupply(g);
    expect(railheads(g, 'GER')![neumark.id]).toBe(1);
    mendRail(g, neumark.id);
    ensureSupply(g);
    expect(railheads(g, 'GER')![neumark.id]).toBe(0);
    const x = crossingOf(berlin.id, neumark.id)!;
    g.flags[`br:${x.a}:${x.b}`] = g.turn;
    g.supplyDirty = true;
    ensureSupply(g);
    expect(railheads(g, 'GER')![neumark.id]).toBe(1);
  });

  it("leaves an invader's line inside the Soviet Union to the rule that was already there, and keeps a torn-up line unconverted", () => {
    const g = grand();
    const minsk = clear(g.byName['Minsk']);
    minsk.ctrl = 'POL';
    minsk.since = g.turn - EASTERN.convertAfter - 1;
    g.supplyDirty = true;
    ensureSupply(g);
    expect(g.supply.POL[minsk.id]).toBeTruthy();
    const relaid = g.reach.POL[minsk.id];
    cutRail(g, minsk.id);
    ensureSupply(g);
    const torn = g.reach.POL[minsk.id];
    expect(torn / relaid).toBeCloseTo(EASTERN.stepFresh / EASTERN.stepConverted, 5);
    // The railhead rule does not count the same line twice.
    expect(railheads(g, 'POL')![minsk.id]).toBeGreaterThan(0);
    expect(railheadAt(g, 'POL', minsk.id)).toEqual({ road: 0, attack: 1, defence: 1 });
    // Soviet troops on their own ground are counted like anyone else.
    expect(railheadAt(g, 'SOV', g.byName['Moscow'].id)).toEqual({ road: 0, attack: 1, defence: 1 });
  });
});

describe('bombing', () => {
  it('can cut the line of the province it falls on', () => {
    const g = grand();
    const B = RAILWAYS.bomb;
    const poznan = g.byName['Poznan'];
    expect(railBombChance(g, poznan.id, B.scale / 2)).toBeCloseTo(0.5);
    expect(railBombChance(g, poznan.id, B.scale / 2, 0.3)).toBeCloseTo(0.35);
    expect(railBombChance(g, poznan.id, B.scale * 10)).toBeCloseTo(B.most);
    expect(railBombChance(g, poznan.id, 0)).toBe(0);
    expect(railBombChance(g, g.regions.find((r) => !onNet(r.id))!.id, B.scale)).toBe(0);
    const want = new Rng(hashString(`railbomb:${g.seed}:${g.turn}:${poznan.id}:GER`)).next() < railBombChance(g, poznan.id, 600);
    const dice = g.rng.state;
    expect(bombRail(g, 'GER', poznan.id, 600)).toBe(want);
    expect(railCut(g, poznan.id)).toBe(want);
    expect(g.rng.state).toBe(dice);
    cutRail(g, poznan.id);
    expect(railBombChance(g, poznan.id, B.scale)).toBe(0);
    expect(bombRail(g, 'GER', poznan.id, 5000)).toBe(false);
  });

  it('makes a province with a railway and no industry a target', () => {
    const g = grand(3, 'GER');
    const f = airFleetsOf(g, 'GER').find((x) => x.bmr > 0 && g.regions[x.base].name === 'Berlin')!;
    const poznan = g.byName['Poznan'], warsaw = g.byName['Warsaw'];
    expect(poznan.fac).toBe(0);
    expect(whyNotMission(g, f, 'bomb', poznan.id)).toBeNull();
    expect(whyNotMission(g, f, 'night', poznan.id)).toBeNull();
    const fc = forecastAir(g, previewAir(g, f, 'bomb', poznan.id), 'bomb', poznan.id);
    expect(fc.cut).toBe(0);
    expect(fc.nation).toBe(0);
    expect(fc.rail).toBeGreaterThan(0.2);
    expect(fc.ruin).toBe(0);
    const city = forecastAir(g, previewAir(g, f, 'bomb', warsaw.id), 'bomb', warsaw.id);
    expect(city.cut).toBeGreaterThan(0);
    expect(city.rail).toBeGreaterThan(0);
    cutRail(g, poznan.id);
    expect(whyNotMission(g, f, 'bomb', poznan.id)).toBe('There is no industry there, and its railway is already cut.');
    expect(whyNotMission(g, f, 'bomb', warsaw.id)).toBeNull();
  });

  it('says in the report what the raid did to the railway', () => {
    const g = grand(3, 'GER');
    const f = airFleetsOf(g, 'GER').find((x) => x.bmr > 0 && g.regions[x.base].name === 'Berlin')!;
    const poznan = g.byName['Poznan'];
    expect(setMission(g, f, 'bomb', poznan.id)).toBe(true);
    // Bombers enough to make it certain.
    const B = RAILWAYS.bomb as unknown as Record<string, number>;
    const reps = with_(B, 'scale', 1, () => with_(B, 'most', 1, () => airBattles(g, 'GER')));
    expect(reps).toHaveLength(1);
    expect(railCut(g, poznan.id)).toBe(true);
    expect(reps[0].result).toMatch(/The railway through Poznan is cut: no train runs until engineers relay it\./);
    expect(reps[0].result).not.toMatch(/output/);
  });

  it('wrecks factories outright when it is heavy enough, as far as half of them', () => {
    const g = grand();
    const U = RAILWAYS.ruin;
    const paris = g.byName['Paris'];
    expect(paris.fac).toBe(4);
    expect(ruinMost(paris)).toBe(2);
    expect(ruinChance(g, paris, U.over)).toBe(0);
    expect(ruinChance(g, paris, 0.2)).toBeCloseTo(0.2 * U.chance);
    const pays = provinceYield(g, paris).fac, earned = income(g, 'FRA');
    const want = new Rng(hashString(`ruin:${g.seed}:${g.turn}:${paris.id}:GER`)).next() < ruinChance(g, paris, 0.3);
    expect(bombFactories(g, 'GER', paris, 0.3)).toBe(want);
    // Month after month of it, until half the factories lie in ruins and no more.
    for (let k = 0; k < 60 && ruinedAt(g, paris) < 2; k++) { g.turn++; bombFactories(g, 'GER', paris, 0.3); }
    expect(ruinedAt(g, paris)).toBe(2);
    expect(ruinChance(g, paris, 0.3)).toBe(0);
    expect(bombFactories(g, 'GER', paris, 0.3)).toBe(false);
    expect(workingFactories(g, paris)).toBe(2);
    expect(provinceYield(g, paris).fac).toBeCloseTo(pays / 2);
    expect(income(g, 'FRA')).toBeLessThan(earned);
    expect(railNote(g, paris.id)).toMatchObject({ ruined: 2, factories: 4 });
    expect(rebuildFactory(g, paris)).toBe(true);
    expect(workingFactories(g, paris)).toBe(3);
    expect(rebuildFactory(g, paris)).toBe(true);
    expect(rebuildFactory(g, paris)).toBe(false);
    expect(income(g, 'FRA')).toBeCloseTo(earned);
    expect(g.flags[`ruin:${paris.id}`]).toBeUndefined();
  });
});

describe('engineers', () => {
  it('relay a cut line in a month, and nothing else does', () => {
    const g = grand(3, 'GER');
    const berlin = g.byName['Berlin'];
    const [first, second] = engineersOf(g, 'GER');
    const offer = (c: Corps, key: string) => jobsFor(g, c).find((o) => o.key === key)!;
    expect(offer(first, 'rail').why).toBe('The railway here is working.');
    cutRail(g, berlin.id);
    expect(offer(first, 'rail').why).toBeNull();
    expect(orderJob(g, first, 'rail')).toBe(true);
    expect(jobLine(g, first)).toBe('Relaying the railway at Berlin: done when next month begins');
    // Two corps cannot relay the same track.
    expect(offer(second, 'rail').why).toBe(`${first.name} is already relaying the railway here.`);
    expect(orderJob(g, second, 'rail')).toBe(false);
    for (let k = 0; k < 3; k++) { g.turn++; engineersMonth(g, 'FRA'); }
    expect(railCut(g, berlin.id)).toBe(true);
    engineersMonth(g, 'GER');
    expect(railCut(g, berlin.id)).toBe(false);
    expect(first.job).toBeUndefined();
    expect(g.headlines.some((h) => /has relaid the railway at Berlin/.test(h.text))).toBe(true);
    // Where no line runs there is nothing to relay.
    const island = g.regions.find((r) => !onNet(r.id) && r.terrain !== 'desert')!;
    const far = corps(g, 'GER', 'eng', island, 1);
    island.ctrl = 'GER';
    expect(offer(far, 'rail').why).toBe('No railway runs through this province.');
  });

  it('leave a cut line cut for as long as none of them comes', () => {
    const g = grand();
    const saxony = g.byName['Saxony'];
    cutRail(g, saxony.id);
    const since = cutOn(g, saxony.id);
    for (let k = 0; k < 12; k++) nextMonth(g, 'GER');
    expect(cutOn(g, saxony.id)).toBe(since);
  });

  it('put a bombed factory back to work, one a month', () => {
    const g = grand(3, 'GER');
    const berlin = g.byName['Berlin'];
    const [first, second] = engineersOf(g, 'GER');
    const offer = (c: Corps, key: string) => jobsFor(g, c).find((o) => o.key === key)!;
    expect(offer(first, 'plant').why).toBe('Every factory here is at work.');
    g.flags[`ruin:${berlin.id}`] = 2;
    const low = income(g, 'GER');
    expect(orderJob(g, first, 'plant')).toBe(true);
    // With two in ruins there is work for two corps.
    expect(offer(second, 'plant').why).toBeNull();
    nextMonth(g, 'GER');
    expect(ruinedAt(g, berlin)).toBe(1);
    expect(income(g, 'GER')).toBeGreaterThan(low);
    expect(g.headlines.some((h) => /has put a bombed factory at Berlin back to work: 1 still lies in ruins/.test(h.text))).toBe(true);
    expect(orderJob(g, first, 'plant')).toBe(true);
    // With one left it is one corps' work.
    expect(offer(second, 'plant').why).toBe(`${first.name} is already rebuilding a factory here.`);
    nextMonth(g, 'GER');
    expect(ruinedAt(g, berlin)).toBe(0);
  });
});

describe('the computer', () => {
  it('sends its engineers by works train to a cut line at the end of its net and relays it', () => {
    const g = grand();
    const neumark = g.byName['Neumark'];
    cutRail(g, neumark.id);
    aiEngineers(g, 'GER');
    const at = engineersOf(g, 'GER').filter((c) => c.job?.what === 'rail');
    expect(at).toHaveLength(1);
    expect(at[0].reg).toBe(neumark.id);
    nextMonth(g, 'GER');
    expect(railCut(g, neumark.id)).toBe(false);
  });

  it('leaves a cut line beyond the end of its net until the one before it is relaid', () => {
    const g = grand();
    // Vienna's lines all run through provinces that are cut: Vienna itself waits its turn.
    const vienna = g.byName['Vienna'];
    const ring = railsAt(vienna.id).filter((j) => allied(g, 'GER', g.regions[j].ctrl));
    cutRail(g, vienna.id);
    for (const j of ring) cutRail(g, j);
    aiEngineers(g, 'GER');
    const jobs = engineersOf(g, 'GER').filter((c) => c.job?.what === 'rail').map((c) => c.reg);
    expect(jobs.length).toBeGreaterThan(0);
    expect(jobs).not.toContain(vienna.id);
    for (const reg of jobs) expect(ring).toContain(reg);
  });

  it('rebuilds its bombed factories when nothing more urgent waits', () => {
    const g = grand();
    const berlin = g.byName['Berlin'];
    g.flags[`ruin:${berlin.id}`] = 1;
    aiEngineers(g, 'GER');
    expect(engineersOf(g, 'GER').filter((c) => c.job?.what === 'plant' && c.reg === berlin.id)).toHaveLength(1);
    nextMonth(g, 'GER');
    expect(ruinedAt(g, berlin)).toBe(0);
  });

  it('fights two years of war with lines cut behind the armies and relaid by engineers', () => {
    const g = grand(5);
    let made = 0, mended = 0, before = new Set<number>();
    for (let t = 0; t < 24; t++) {
      runTurnAI(g);
      const now = new Set(g.regions.filter((r) => railCut(g, r.id)).map((r) => r.id));
      for (const id of now) if (!before.has(id)) made++;
      for (const id of before) if (!now.has(id)) mended++;
      before = now;
      // Engineers only ever work in friendly ground.
      for (const c of g.corps) if (c.job && !c.dead) expect(allied(g, c.nat, g.regions[c.job.reg].ctrl), `${c.name} at ${g.regions[c.job.reg].name}`).toBe(true);
    }
    expect(made).toBeGreaterThan(3);
    expect(mended).toBeGreaterThan(0);
  }, 60000);
});

describe('saving', () => {
  it('keeps cut lines, ruins and the railheads through a save', () => {
    const g = grand(3, 'GER');
    cutRail(g, g.byName['Saxony'].id);
    cutRail(g, g.byName['Neumark'].id);
    g.flags[`ruin:${g.byName['Berlin'].id}`] = 1;
    ensureSupply(g);
    const h = loadGame(saveGame(g))!;
    expect(railCut(h, h.byName['Saxony'].id)).toBe(true);
    expect(cutOn(h, h.byName['Neumark'].id)).toBe(g.turn);
    expect(ruinedAt(h, h.byName['Berlin'])).toBe(1);
    expect(h.supplyDirty).toBe(false);
    for (const nat of Object.keys(g.supply)) expect(Array.from(railheads(h, nat)!), nat).toEqual(Array.from(railheads(g, nat)!));
    expect(railheads(h, 'GER')![h.byName['Neumark'].id]).toBe(1);
    expect(saveGame(h)).toBe(saveGame(g));
  });

  it('draws the picture afresh for a grand campaign saved before the railways were laid', () => {
    const g = grand(3, 'GER');
    ensureSupply(g);
    const old = JSON.parse(saveGame(g));
    for (const k of Object.keys(old.supply)) old.supply[k] = old.supply[k].replace(/2/g, '1');
    const h = loadGame(JSON.stringify(old))!;
    expect(h.supplyDirty).toBe(true);
    ensureSupply(h);
    expect(railheads(h, 'GER')![h.byName['Berlin'].id]).toBe(0);
    expect(Object.values(h.supply).some((t) => t.includes(2))).toBe(true);
  });
});

describe('the province panel', () => {
  it('is told where the line runs, whether it is cut and how far the railhead is', () => {
    const g = grand(3, 'GER');
    ensureSupply(g);
    const neumark = g.byName['Neumark'];
    expect(railNote(g, neumark.id)).toMatchObject({ line: true, cut: false, road: 0, attack: 1, defence: 1, ruined: 0 });
    expect(railNote(g, neumark.id)!.to.map((j) => g.regions[j].name).sort()).toEqual(['Berlin', 'Poznan']);
    cutRail(g, neumark.id);
    ensureSupply(g);
    expect(railNote(g, neumark.id)).toMatchObject({ line: true, cut: true, since: g.turn, road: 1 });
    // Ground the player draws no supply in has no railhead of theirs to count from.
    expect(railNote(g, g.byName['Moscow'].id)).toMatchObject({ line: true, road: -1 });
    const desert = g.regions.find((r) => r.terrain === 'desert' && !onNet(r.id))!;
    expect(railNote(g, desert.id)).toMatchObject({ line: false, to: [] });
  });
});
