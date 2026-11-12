// The grand campaign's rivers, bridges and engineers. An attack across a river is harder; while the bridges are
// down nothing on wheels or tracks crosses at all and infantry goes over in boats; troops who lose a bank blow
// the bridges behind them unless they are seized first; and nothing is rebuilt, fortified, mined or demolished
// without an engineer corps on the spot. The historical scenarios use none of it.
import { describe, expect, it } from 'vitest';
import engineering from '../data/campaign/engineering.json';
import { Rng, hashString } from '../src/core/rng';
import { offers, orderOffer } from '../src/campaign/arsenal';
import { aiBridges, aiEngineers, engineersMonth, jobLine, jobsFor, minesAt, sappers } from '../src/campaign/engineers';
import { addCorps, assess, createGame, hasRoom, limitAttackers, moveCorps, moveCost, railMove, railReach } from '../src/campaign/game';
import { groundRules } from '../src/campaign/ground';
import {
  CROSSINGS, ENGINEERING, RIVERS, blocked, blowBridges, blowChance, blownOn, bridgeDown, bridgesOnCapture, crossingOf, crossingsAt,
  downBetween, farBank, noCrossing, riverBetween, riverName, riverNotes, whyBlocked, whyNotBlow, type Crossing,
} from '../src/campaign/rivers';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { engineerJobs, orderBlowBridges, orderEngineers, orderStopWork, reach, runTurnAI, whyNotBlowBridges } from '../src/campaign/turn';
import { FORT_MUL, KIND, STACK, type Corps, type Game, type Kind, type Region } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = null): Game => startOpen(createGame(seed, human));
const corps = (g: Game, nat: string, kind: Kind, at: Region, divs = 3): Corps => addCorps(g, nat, `Test ${KIND[kind].label} Corps ${g.nextCorps}`, kind, divs, at.id, {});
/** Empties a region of troops and returns it. */
const clear = (r: Region): Region => { for (const c of r.units) c.dead = true; r.units = []; return r; };
/** The crossing between two named regions. */
const between = (g: Game, a: string, b: string): Crossing => crossingOf(g.byName[a].id, g.byName[b].id)!;
/** Blows a crossing's bridges as the game records it. */
const down = (g: Game, x: Crossing): void => { g.flags[`br:${x.a}:${x.b}`] = g.turn; };
/** The same thing worked out with one number in the engineering data changed. */
const with_ = <T>(table: Record<string, number>, key: string, value: number, run: () => T): T => { const was = table[key]; table[key] = value; try { return run(); } finally { table[key] = was; } };
/** An attack's strength with the river taken away. */
const dry = (run: () => number): number => with_(ENGINEERING.river as unknown as Record<string, number>, 'attack', 1, run);
/** A month goes by and this nation's engineers finish what they were set to. */
const nextMonth = (g: Game, nat: string): void => { g.turn++; engineersMonth(g, nat); };
const engineersOf = (g: Game, nat: string): Corps[] => g.corps.filter((c) => c.nat === nat && c.kind === 'eng' && !c.dead);

describe('the river data', () => {
  it('draws real rivers and puts every bridge between two neighbouring regions', () => {
    const g = grand();
    expect(RIVERS.length).toBeGreaterThanOrEqual(30);
    for (const rv of RIVERS) {
      expect(rv.pts.length, rv.name).toBeGreaterThanOrEqual(2);
      expect(rv.ll.length, rv.name).toBe(rv.pts.length);
      for (const [lat, lon] of rv.ll) { expect(lat, rv.name).toBeGreaterThan(20); expect(lat, rv.name).toBeLessThan(72); expect(lon, rv.name).toBeGreaterThan(-12); expect(lon, rv.name).toBeLessThan(62); }
    }
    expect(new Set(RIVERS.map((r) => r.name)).size).toBe(RIVERS.length);
    expect(CROSSINGS.length).toBeGreaterThanOrEqual(90);
    const seen = new Set<string>();
    for (const x of CROSSINGS) {
      const a = g.regions[x.a], b = g.regions[x.b];
      expect(x.a).toBeLessThan(x.b);
      expect(a.nb.includes(b.id), `${a.name} and ${b.name}`).toBe(true);
      expect(RIVERS[x.river], `${a.name} and ${b.name}`).toBeDefined();
      expect(seen.has(`${x.a}:${x.b}`), `${a.name} and ${b.name} twice`).toBe(false);
      seen.add(`${x.a}:${x.b}`);
      // The bridge is drawn somewhere between the two of them.
      expect(Math.hypot(x.x - (a.x + b.x) / 2, x.y - (a.y + b.y) / 2)).toBeLessThan(Math.hypot(a.x - b.x, a.y - b.y));
      expect(crossingOf(b.id, a.id)).toBe(x);
      expect(farBank(x, a.id)).toBe(b.id);
      expect(crossingsAt(a.id)).toContain(x);
    }
  });

  it('has the crossings history was fought over', () => {
    const g = grand();
    const famous: [string, string, string][] = [
      ['Baden', 'Alsace', 'Rhine'], ['Sedan', 'Ardennes', 'Meuse'], ['Paris', 'Orleans', 'Seine'], ['Warsaw', 'Lublin', 'Vistula'], ['Lublin', 'Brest', 'Bug'],
      ['Kiev', 'Chernigov', 'Dnieper'], ['Rostov', 'Kuban', 'Don'], ['Stalingrad', 'Astrakhan', 'Volga'], ['Alexandria', 'Cairo', 'Nile'], ['Berlin', 'Neumark', 'Oder'],
    ];
    for (const [a, b, river] of famous) {
      const x = between(g, a, b);
      expect(x, `${a} and ${b}`).toBeTruthy();
      expect(riverName(x), `${a} and ${b}`).toBe(river);
    }
    // With every Rhine bridge down, the left bank is cut off from the rest of Germany.
    const left = new Set(['Rhineland', 'Eifel', 'Saar'].map((n) => g.byName[n].id));
    for (const id of left) for (const j of g.regions[id].nb) if (g.regions[j].owner === 'GER' && !left.has(j)) expect(riverName(crossingOf(id, j)!), `${g.regions[id].name} and ${g.regions[j].name}`).toBe('Rhine');
  });

  it('keeps the numbers within reason and words every job', () => {
    expect(engineering.river.attack).toBeGreaterThan(0.8); expect(engineering.river.attack).toBeLessThan(1);
    expect(engineering.blown.attack).toBeGreaterThan(0.4); expect(engineering.blown.attack).toBeLessThan(engineering.blown.withEngineers);
    expect(engineering.blown.withEngineers).toBeLessThan(engineering.river.attack);
    expect(engineering.blown.barred).toEqual(['arm', 'mot', 'art']);
    for (const p of [engineering.blow.chance, engineering.blow.unguarded, engineering.blow.engineers]) { expect(p).toBeGreaterThan(0); expect(p).toBeLessThan(1); }
    expect(engineering.blow.unguarded).toBeLessThan(engineering.blow.chance);
    expect(engineering.blow.chance).toBeLessThan(engineering.blow.engineers);
    expect(engineering.blow.fastCaptor).toBeLessThan(0);
    expect(engineering.mines.attack * engineering.mines.most).toBeLessThan(0.5);
    expect(engineering.breach.most).toBeLessThanOrEqual(0.75);
    expect(engineering.works.most).toBeLessThan(FORT_MUL.length - 1);
    for (const key of ['bridge', 'works', 'mines', 'demolish'] as const) { expect(engineering.jobs[key].label.length).toBeGreaterThan(8); expect(engineering.jobs[key].blurb.length).toBeGreaterThan(40); }
    expect(KIND.eng.mobile).toBe(false);
    expect(KIND.eng.atk).toBeLessThan(KIND.inf.atk);
  });
});

describe('a historical scenario', () => {
  it('knows nothing of rivers, bridges or engineers', () => {
    const g = createGame(3, 'GER');
    expect(groundRules(g)).toBe(false);
    const elbing = g.byName['Elbing'], pomorze = g.byName['Pomorze'];
    const x = crossingOf(elbing.id, pomorze.id)!;
    expect(riverBetween(g, elbing.id, pomorze.id)).toBeNull();
    expect(riverNotes(g, elbing.id)).toEqual([]);
    expect(engineersOf(g, 'GER')).toEqual([]);
    expect(whyNotBlow(g, 'GER', x, elbing.id)).toMatch(/no bridges to blow/);
    expect(blowBridges(g, 'GER', x, elbing.id)).toBe(false);
    expect(orderBlowBridges(g, elbing.id, pomorze.id)).toBe(false);
    // Even a bridge marked as down stops nobody, and the battle is worked out as it always was.
    const tanks = corps(g, 'GER', 'arm', elbing), inf = corps(g, 'GER', 'inf', elbing);
    const before = assess(g, [tanks, inf], pomorze);
    down(g, x);
    expect(noCrossing(g, 'arm', elbing.id, pomorze.id)).toBe(false);
    expect(blocked(g, tanks, pomorze)).toBe(false);
    expect(limitAttackers(g, [tanks, inf], pomorze)).toEqual([tanks, inf]);
    const after = assess(g, [tanks, inf], pomorze);
    expect(after.A).toBe(before.A);
    expect(after.lines.some((l) => l.label === 'River' || l.label === 'Minefields' || l.label === 'Engineers')).toBe(false);
    // Nor do mines or engineers count.
    g.flags[`mine:${pomorze.id}`] = 2;
    expect(minesAt(g, pomorze)).toBe(0);
    const sap = corps(g, 'GER', 'eng', elbing, 1);
    expect(jobsFor(g, sap)).toEqual([]);
    expect(orderEngineers(g, sap, 'works')).toBe(false);
    expect(assess(g, [tanks, inf], pomorze).A).toBe(before.A);
  });

  it('plays its opening months without a bridge blown or an engineer raised', () => {
    const g = createGame(4, null);
    for (let t = 0; t < 10; t++) runTurnAI(g);
    expect(Object.keys(g.flags).filter((k) => k.startsWith('br:') || k.startsWith('mine:'))).toEqual([]);
    expect(g.corps.some((c) => c.kind === 'eng')).toBe(false);
    expect(g.flags.sappers).toBeUndefined();
  }, 120000);
});

describe('a river with its bridges standing', () => {
  it('takes a little off the corps that attack across it, and nothing off the others', () => {
    const g = grand();
    const elbing = g.byName['Elbing'], pomorze = g.byName['Pomorze'], grenzmark = g.byName['Grenzmark'];
    expect(riverName(between(g, 'Elbing', 'Pomorze'))).toBe('Vistula');
    expect(crossingOf(grenzmark.id, pomorze.id)).toBeNull();
    const over = corps(g, 'GER', 'inf', elbing), round = corps(g, 'GER', 'inf', grenzmark);
    expect(assess(g, [over], pomorze).A / dry(() => assess(g, [over], pomorze).A)).toBeCloseTo(ENGINEERING.river.attack, 9);
    expect(assess(g, [round], pomorze).A).toBe(dry(() => assess(g, [round], pomorze).A));
    const both = assess(g, [over, round], pomorze);
    const bothDry = dry(() => assess(g, [over, round], pomorze).A);
    expect(both.A).toBeLessThan(bothDry);
    expect(both.A).toBeGreaterThan(bothDry * ENGINEERING.river.attack);
    const line = both.lines.find((l) => l.label === 'River')!;
    expect(line.value).toBe('1 of the 2 attacking corps cross the Vistula');
    expect(line.effect).toBe('Over the bridges the attack is cut to 95%');
    expect(assess(g, [over], pomorze).lines.find((l) => l.label === 'River')!.value).toBe('The attack crosses the Vistula');
    expect(assess(g, [round], pomorze).lines.some((l) => l.label === 'River')).toBe(false);
  });

  it('costs nothing to march over and stops nobody', () => {
    const g = grand();
    const berlin = g.byName['Berlin'], neumark = clear(g.byName['Neumark']), dresden = g.byName['Dresden'];
    expect(crossingOf(berlin.id, dresden.id)).toBeNull();
    for (const kind of ['inf', 'arm', 'mot', 'art', 'eng'] as Kind[]) {
      const c = corps(g, 'GER', kind, clear(berlin));
      expect(blocked(g, c, neumark), kind).toBe(false);
      expect(whyBlocked(g, c, neumark), kind).toBeNull();
      const cost = moveCost(g, c, neumark);
      expect(cost, kind).toBe(with_(ENGINEERING.blown as unknown as Record<string, number>, 'march', 5, () => moveCost(g, c, neumark)));
      expect(moveCorps(g, c, neumark), kind).toBe(true);
      clear(neumark);
    }
    expect(riverNotes(g, berlin.id).map((n) => [n.river, g.regions[n.far].name, n.down])).toEqual(expect.arrayContaining([['Oder', 'Neumark', false], ['Elbe', 'Saxony', false]]));
  });
});

describe('blown bridges', () => {
  it('are blown by order from a bank the player holds, in wartime, once', () => {
    const g = grand(3, 'GER');
    const berlin = g.byName['Berlin'], neumark = g.byName['Neumark'], dresden = g.byName['Dresden'];
    const x = between(g, 'Berlin', 'Neumark');
    expect(whyNotBlowBridges(g, berlin.id, dresden.id)).toMatch(/no river between/);
    expect(whyNotBlowBridges(g, berlin.id, neumark.id)).toBeNull();
    // Not from a bank with nobody on it, and not from somebody else's.
    const saxony = clear(g.byName['Saxony']);
    expect(whyNotBlowBridges(g, saxony.id, berlin.id)).toMatch(/no troops of yours at Saxony/);
    expect(orderBlowBridges(g, saxony.id, berlin.id)).toBe(false);
    expect(whyNotBlowBridges(g, g.byName['Warsaw'].id, g.byName['Lublin'].id)).toMatch(/not in friendly hands/);
    expect(orderBlowBridges(g, berlin.id, neumark.id)).toBe(true);
    expect(bridgeDown(g, x)).toBe(true);
    expect(blownOn(g, x)).toBe(g.turn);
    expect(downBetween(g, neumark.id, berlin.id)).toBe(true);
    expect(g.headlines.some((h) => /German troops blow the bridges over the Oder between Berlin and Neumark/.test(h.text))).toBe(true);
    expect(whyNotBlowBridges(g, berlin.id, neumark.id)).toMatch(/already down/);
    expect(orderBlowBridges(g, berlin.id, neumark.id)).toBe(false);
    expect(riverNotes(g, neumark.id).find((n) => n.far === berlin.id)).toMatchObject({ river: 'Oder', down: true, since: g.turn });
    // A nation at peace leaves its bridges alone.
    const po = CROSSINGS.find((c) => g.regions[c.a].ctrl === 'ITA' && g.regions[c.b].ctrl === 'ITA')!;
    expect(riverName(po)).toBe('Po');
    const bank = g.regions[po.a];
    corps(g, 'ITA', 'inf', bank);
    expect(g.nations.ITA.atWar).toEqual([]);
    expect(whyNotBlow(g, 'ITA', po, bank.id)).toMatch(/peacetime/);
  });

  it('stop tanks, lorries and guns by road, and infantry only for a while', () => {
    const g = grand(3, 'GER');
    const berlin = clear(g.byName['Berlin']), neumark = clear(g.byName['Neumark']);
    const x = between(g, 'Berlin', 'Neumark');
    const inf = corps(g, 'GER', 'inf', berlin), sap = corps(g, 'GER', 'eng', berlin, 1), mtn = corps(g, 'GER', 'mtn', berlin), cav = corps(g, 'GER', 'cav', berlin);
    const wheels = (['arm', 'mot', 'art'] as Kind[]).map((k) => corps(g, 'GER', k, berlin));
    const marchUp = moveCost(g, inf, neumark);
    down(g, x);
    for (const c of wheels) {
      expect(noCrossing(g, c.kind, berlin.id, neumark.id), c.kind).toBe(true);
      expect(noCrossing(g, c.kind, neumark.id, berlin.id), c.kind).toBe(true);
      expect(blocked(g, c, neumark), c.kind).toBe(true);
      expect(whyBlocked(g, c, neumark), c.kind).toMatch(/The bridges over the Oder between Berlin and Neumark are down\. .* cannot cross until engineers rebuild them\./);
      expect(moveCorps(g, c, neumark), c.kind).toBe(false);
      expect(c.reg, c.kind).toBe(berlin.id);
      // The player is not shown the far bank as a move straight across.
      expect(reach(g, c).get(neumark.id)?.prev, c.kind).not.toBe(berlin.id);
    }
    for (const c of [inf, sap, mtn, cav]) {
      expect(blocked(g, c, neumark), c.kind).toBe(false);
      expect(whyBlocked(g, c, neumark), c.kind).toBeNull();
    }
    // Infantry goes over in boats: a move more than it was.
    expect(moveCost(g, inf, neumark)).toBe(marchUp + ENGINEERING.blown.march);
    expect(reach(g, inf).get(neumark.id)).toMatchObject({ prev: berlin.id, cost: marchUp + ENGINEERING.blown.march });
    expect(moveCorps(g, inf, neumark)).toBe(true);
    // Other roads are as they were.
    expect(moveCorps(g, wheels[0], g.byName['Dresden'])).toBe(true);
  });

  it('cut the railway too', () => {
    const g = grand(3, 'GER');
    const inf = corps(g, 'GER', 'inf', g.byName['Berlin']);
    const left = ['Rhineland', 'Eifel', 'Saar'].map((n) => g.byName[n]);
    for (const r of left) expect(railReach(g, inf).has(r.id), r.name).toBe(true);
    for (const x of CROSSINGS) if (riverName(x) === 'Rhine') down(g, x);
    const cut = railReach(g, inf);
    for (const r of left) expect(cut.has(r.id), r.name).toBe(false);
    expect(cut.has(g.byName['Frankfurt'].id)).toBe(true);
    expect(railMove(g, inf, left[0])).toBe(false);
    expect(inf.reg).toBe(g.byName['Berlin'].id);
  });

  it('leave the tanks, lorries and guns out of an attack, and put the infantry in boats', () => {
    const g = grand();
    const elbing = g.byName['Elbing'], pomorze = g.byName['Pomorze'];
    const x = between(g, 'Elbing', 'Pomorze');
    const inf = corps(g, 'GER', 'inf', elbing), tanks = corps(g, 'GER', 'arm', elbing), guns = corps(g, 'GER', 'art', elbing), lorries = corps(g, 'GER', 'mot', elbing);
    const alone = dry(() => assess(g, [inf], pomorze).A);
    down(g, x);
    expect(limitAttackers(g, [tanks, inf, guns, lorries], pomorze)).toEqual([inf]);
    const boats = assess(g, [inf], pomorze);
    expect(boats.A / alone).toBeCloseTo(ENGINEERING.blown.attack, 9);
    // The wheels on the bank add nothing, not even the help tanks give infantry beside them.
    const all = assess(g, [tanks, inf, guns, lorries], pomorze);
    expect(all.A).toBeCloseTo(boats.A, 9);
    const line = all.lines.find((l) => l.label === 'River')!;
    expect(line.value).toBe('The bridges over the Vistula are down');
    expect(line.effect).toMatch(/^Infantry crossing in boats fights at 70%/);
    expect(line.effect).toMatch(/3 corps of tanks, lorries or guns cannot cross and take no part/);
    expect(all.lines.some((l) => l.label === 'Combined arms')).toBe(false);
    expect(assess(g, [tanks], pomorze).odds).toBe(0);
    // The player's tanks are not offered the attack either.
    const h = grand(3, 'GER');
    const mine = corps(h, 'GER', 'arm', h.byName['Elbing']), foot = corps(h, 'GER', 'inf', h.byName['Elbing']);
    expect(reach(h, mine).get(h.byName['Pomorze'].id)?.attack).toBe(true);
    down(h, between(h, 'Elbing', 'Pomorze'));
    expect(reach(h, mine).has(h.byName['Pomorze'].id)).toBe(false);
    expect(reach(h, foot).get(h.byName['Pomorze'].id)?.attack).toBe(true);
  });

  it('are less of an obstacle with engineers in the assault', () => {
    const g = grand();
    const elbing = g.byName['Elbing'], pomorze = g.byName['Pomorze'];
    const inf = corps(g, 'GER', 'inf', elbing), sap = corps(g, 'GER', 'eng', elbing, 1);
    const open = dry(() => assess(g, [inf, sap], pomorze).A);
    down(g, between(g, 'Elbing', 'Pomorze'));
    const a = assess(g, [inf, sap], pomorze);
    expect(a.A / open).toBeCloseTo(ENGINEERING.blown.withEngineers, 9);
    expect(a.lines.find((l) => l.label === 'River')!.effect).toMatch(/crossing in boats fights at 85%, with engineers to get it over/);
  });
});

describe('losing a river bank', () => {
  /** German troops have just taken Lublin; the Poles still hold Warsaw across the Vistula. */
  const taken = (g: Game, kind: Kind = 'inf'): { lublin: Region; x: Crossing; warsaw: Region } => {
    const lublin = clear(g.byName['Lublin']), warsaw = g.byName['Warsaw'];
    const c = corps(g, 'GER', kind, g.byName['Moravia']);
    g.byName['Moravia'].units.splice(g.byName['Moravia'].units.indexOf(c), 1);
    c.reg = lublin.id; lublin.units.push(c); lublin.ctrl = 'GER';
    return { lublin, x: between(g, 'Warsaw', 'Lublin'), warsaw };
  };

  it('sets the chance by who holds the far bank and who took this one', () => {
    const B = ENGINEERING.blow;
    const g = grand();
    const { lublin, x, warsaw } = taken(g);
    expect(warsaw.units.length).toBeGreaterThan(0);
    expect(blowChance(g, x, lublin, 'GER')).toBeCloseTo(B.chance, 9);
    corps(g, 'POL', 'eng', warsaw, 1);
    expect(blowChance(g, x, lublin, 'GER')).toBeCloseTo(B.engineers, 9);
    clear(warsaw);
    expect(blowChance(g, x, lublin, 'GER')).toBeCloseTo(B.unguarded, 9);
    // Fast troops are on the bridges before the charges are fired.
    const h = grand();
    const fast = taken(h, 'arm');
    expect(blowChance(h, fast.x, fast.lublin, 'GER')).toBeCloseTo(B.chance + B.fastCaptor, 9);
    corps(h, 'POL', 'eng', fast.warsaw, 1);
    expect(blowChance(h, fast.x, fast.lublin, 'GER')).toBeCloseTo(B.engineers + B.fastCaptor, 9);
    clear(fast.warsaw);
    expect(blowChance(h, fast.x, fast.lublin, 'GER')).toBe(0.05);
  });

  it('rolls each bridge on dice of its own, as often as the chance says', () => {
    const g = grand();
    const { lublin } = taken(g);
    const mine = crossingsAt(lublin.id).filter((x) => g.regions[farBank(x, lublin.id)].ctrl === 'POL');
    expect(mine.map(riverName).sort()).toEqual(['Bug', 'Bug', 'Vistula', 'Vistula']);
    const rolls = g.rng.state;
    let blown = 0, tried = 0;
    for (let seed = 1; seed <= 150; seed++) {
      g.seed = seed;
      for (const x of mine) delete g.flags[`br:${x.a}:${x.b}`];
      bridgesOnCapture(g, lublin, 'GER');
      for (const x of mine) {
        const roll = new Rng(hashString(`bridge:${seed}:${g.turn}:${x.a}:${x.b}`)).next();
        expect(bridgeDown(g, x), `seed ${seed}`).toBe(roll < blowChance(g, x, lublin, 'GER'));
        tried++; if (bridgeDown(g, x)) blown++;
      }
    }
    // The war's own dice were never touched.
    expect(g.rng.state).toEqual(rolls);
    const share = mine.reduce((s, x) => s + blowChance(g, x, lublin, 'GER'), 0) / mine.length;
    expect(blown / tried).toBeGreaterThan(share - 0.07);
    expect(blown / tried).toBeLessThan(share + 0.07);
    // Bridges already down stay down, and a bank whose far side is friendly keeps its bridges.
    g.seed = 3;
    for (const x of mine) down(g, x);
    bridgesOnCapture(g, lublin, 'GER');
    for (const x of mine) expect(blownOn(g, x)).toBe(g.turn);
    const k = grand();
    const kielce = clear(k.byName['Kielce']);
    kielce.ctrl = 'GER';
    k.byName['Lublin'].ctrl = 'GER'; k.byName['Przemysl'].ctrl = 'GER';
    bridgesOnCapture(k, kielce, 'GER');
    for (const x of crossingsAt(kielce.id)) expect(bridgeDown(k, x)).toBe(false);
  });

  it('happens when ground is really taken, lifts the mines there and tells the player', () => {
    let saw = { blown: 0, seized: 0 };
    for (let seed = 1; seed <= 12; seed++) {
      const g = grand(seed, 'GER');
      const poznan = clear(g.byName['Poznan']), lodz = clear(g.byName['Lodz']), mlawa = g.byName['Mlawa'];
      expect(poznan.nb.includes(lodz.id)).toBe(true);
      poznan.ctrl = 'GER';
      const x = between(g, 'Lodz', 'Mlawa');
      expect(mlawa.ctrl).toBe('POL');
      g.flags[`mine:${lodz.id}`] = 2;
      const inf = corps(g, 'GER', 'inf', poznan);
      const chance = () => blowChance(g, x, lodz, 'GER');
      expect(moveCorps(g, inf, lodz)).toBe(true);
      expect(lodz.ctrl).toBe('GER');
      expect(minesAt(g, lodz)).toBe(0);
      const roll = new Rng(hashString(`bridge:${seed}:${g.turn}:${x.a}:${x.b}`)).next();
      expect(bridgeDown(g, x)).toBe(roll < chance());
      if (bridgeDown(g, x)) {
        saw.blown++;
        expect(g.headlines.some((h) => h.text === 'Polish troops blow the bridges over the Vistula at Lodz as German forces reach the river.')).toBe(true);
      } else {
        saw.seized++;
        expect(g.headlines.some((h) => h.text === 'German troops seize the bridges over the Vistula at Lodz before they can be blown.')).toBe(true);
      }
    }
    expect(saw.blown).toBeGreaterThan(0);
    expect(saw.seized).toBeGreaterThan(0);
  });
});

describe('engineers', () => {
  it('start the grand campaign with the great powers, and are raised from the army shelf', () => {
    const g = grand(3, 'GER');
    for (const nat of ENGINEERING.start.nations) {
      const mine = engineersOf(g, nat);
      expect(mine.map((c) => c.name), nat).toEqual(['1st Engineer Corps', '2nd Engineer Corps']);
      for (const c of mine) { expect(c.divs).toBe(1); expect(g.regions[c.reg].owner, nat).toBe(nat); }
    }
    expect(engineersOf(g, 'ROM')).toEqual([]);
    expect(engineersOf(g, 'POL')).toEqual([]);
    // Starting twice gives nobody more.
    startOpen(g);
    expect(engineersOf(g, 'GER').length).toBe(2);
    g.nations.GER.pp = 100;
    const offer = offers(g, 'GER', g.byName['Berlin'].id).find((o) => o.key === 'corps:eng')!;
    expect(offer).toMatchObject({ group: 'army', label: 'Engineer corps', cost: 18, months: 3 });
    expect(offer.why).toBeNull();
    expect(orderOffer(g, 'GER', 'corps:eng', g.byName['Berlin'].id)).toBeTruthy();
    expect(g.nations.GER.pp).toBe(82);
    expect(g.nations.GER.queue.some((b) => b.what === 'corps' && b.kind === 'eng')).toBe(true);
  });

  it('are offered their jobs where they stand, with the reason a job cannot be started', () => {
    const g = grand(3, 'GER');
    const sap = engineersOf(g, 'GER')[0];
    expect(g.regions[sap.reg].name).toBe('Berlin');
    const jobs = engineerJobs(g, sap);
    expect(jobs.map((j) => j.key)).toEqual(['bridge', 'rail', 'plant', 'works', 'mines', 'demolish']);
    expect(jobs.map((j) => j.why)).toEqual(['Every bridge here is standing.', 'The railway here is working.', 'Every factory here is at work.', null, null, 'There are no fortifications here to demolish.']);
    expect(jobs[3].label).toBe('Build field works');
    // Away from the rivers there are no bridges to rebuild.
    const dresden = clear(g.regions.find((r) => r.ctrl === 'GER' && r.owner === 'GER' && !crossingsAt(r.id).length)!);
    const away = corps(g, 'GER', 'eng', dresden, 1);
    expect(engineerJobs(g, away)[0].why).toBe('No great river runs past this province.');
    // Nobody else's engineers, and no other arm.
    expect(engineerJobs(g, engineersOf(g, 'FRA')[0])).toEqual([]);
    expect(orderEngineers(g, engineersOf(g, 'FRA')[0], 'works')).toBe(false);
    expect(engineerJobs(g, corps(g, 'GER', 'inf', dresden))).toEqual([]);
  });

  it('rebuild the bridges where they stand in a month, and nothing rebuilds itself', () => {
    const g = grand(3, 'GER');
    const berlin = g.byName['Berlin'];
    const oder = between(g, 'Berlin', 'Neumark'), elbe = between(g, 'Berlin', 'Saxony'), far = between(g, 'Hamburg', 'Hanover');
    for (const x of [oder, elbe, far]) down(g, x);
    const [sap, idle] = engineersOf(g, 'GER');
    expect(engineerJobs(g, sap)[0].why).toBeNull();
    const ap = sap.ap;
    expect(orderEngineers(g, sap, 'bridge')).toBe(true);
    expect(sap.job).toMatchObject({ what: 'bridge', reg: berlin.id, eta: g.turn + 1 });
    expect(sap.ap).toBe(0);
    expect(jobLine(g, sap)).toBe('Rebuilding the bridges at Berlin: done when next month begins');
    expect(engineerJobs(g, sap).every((j) => j.why === '1st Engineer Corps already has work in hand.')).toBe(true);
    expect(orderEngineers(g, sap, 'works')).toBe(false);
    // The order can be taken back the same month.
    expect(orderStopWork(g, sap)).toBe(true);
    expect(sap.job).toBeUndefined();
    expect(sap.ap).toBe(ap);
    expect(orderEngineers(g, sap, 'bridge')).toBe(true);
    // One corps is enough for the bridges of one province.
    expect(engineerJobs(g, idle)[0].why).toBe('1st Engineer Corps is already rebuilding the bridges here.');
    expect(orderEngineers(g, idle, 'bridge')).toBe(false);
    // The other corps sat idle: months pass and the bridges it was not set to stay down.
    engineersMonth(g, 'GER');
    expect(bridgeDown(g, oder)).toBe(true);
    nextMonth(g, 'GER');
    expect(bridgeDown(g, oder)).toBe(false);
    expect(bridgeDown(g, elbe)).toBe(false);
    expect(bridgeDown(g, far)).toBe(true);
    expect(sap.job).toBeUndefined();
    expect(idle.job).toBeUndefined();
    expect(g.headlines.some((h) => /1st Engineer Corps has rebuilt the bridges at Berlin\. Tanks, lorries and guns can cross again\./.test(h.text))).toBe(true);
    for (let t = 0; t < 6; t++) nextMonth(g, 'GER');
    expect(bridgeDown(g, far)).toBe(true);
  });

  it('raise field works as far as level 2, lay two belts of mines, and pull works down', () => {
    const g = grand(3, 'GER');
    const berlin = g.byName['Berlin'];
    const sap = engineersOf(g, 'GER')[0];
    expect(berlin.fort).toBeNull();
    for (const lv of [1, 2]) {
      expect(orderEngineers(g, sap, 'works')).toBe(true);
      nextMonth(g, 'GER');
      expect(berlin.fort?.lv).toBe(lv);
      sap.ap = sap.apMax;
    }
    expect(berlin.fort!.vs).toContain('GER');
    expect(engineerJobs(g, sap).find((j) => j.key === 'works')!.why).toMatch(/level 2: as strong as field engineers can make them/);
    expect(orderEngineers(g, sap, 'works')).toBe(false);
    for (const lv of [1, 2]) {
      expect(orderEngineers(g, sap, 'mines')).toBe(true);
      nextMonth(g, 'GER');
      expect(minesAt(g, berlin)).toBe(lv);
      sap.ap = sap.apMax;
    }
    expect(engineerJobs(g, sap).find((j) => j.key === 'mines')!.why).toMatch(/as thick as they can be laid/);
    for (const lv of [1, 0]) {
      expect(orderEngineers(g, sap, 'demolish')).toBe(true);
      nextMonth(g, 'GER');
      expect(berlin.fort?.lv ?? 0).toBe(lv);
      sap.ap = sap.apMax;
    }
    expect(berlin.fort).toBeNull();
    expect(g.headlines.filter((h) => /1st Engineer Corps has (finished field works|laid minefields|demolished)/.test(h.text)).length).toBe(6);
  });

  it('can be put to work on the day they arrive, and lose the job if they leave it', () => {
    const g = grand(3, 'GER');
    const sap = engineersOf(g, 'GER')[0];
    const saxony = g.byName['Saxony'], magdeburg = clear(g.byName['Magdeburg']);
    expect(railMove(g, sap, saxony)).toBe(true);
    expect(sap.ap).toBe(0);
    expect(engineerJobs(g, sap).find((j) => j.key === 'works')!.why).toBeNull();
    expect(orderEngineers(g, sap, 'works')).toBe(true);
    // Taking the order back does not give back the moves the journey cost.
    expect(orderStopWork(g, sap)).toBe(true);
    expect(sap.ap).toBe(0);
    expect(orderEngineers(g, sap, 'works')).toBe(true);
    // Next month the corps is ordered away before the work is done: nothing is built.
    g.turn++;
    saxony.units.splice(saxony.units.indexOf(sap), 1); sap.reg = magdeburg.id; magdeburg.units.push(sap);
    engineersMonth(g, 'GER');
    expect(sap.job).toBeUndefined();
    expect(saxony.fort).toBeNull();
    expect(magdeburg.fort).toBeNull();
    // An order from an earlier month cannot be taken back.
    sap.ap = sap.apMax;
    expect(orderEngineers(g, sap, 'mines')).toBe(true);
    g.turn++;
    expect(orderStopWork(g, sap)).toBe(false);
  });

  it('work among the other troops: a full province still has room for two engineer corps', () => {
    const g = grand(3, 'GER');
    const saxony = clear(g.byName['Saxony']), berlin = g.byName['Berlin'];
    for (let k = 0; k < STACK; k++) corps(g, 'GER', 'inf', saxony);
    const extra = corps(g, 'GER', 'inf', berlin);
    const [a, b] = engineersOf(g, 'GER');
    const c = corps(g, 'GER', 'eng', berlin, 1);
    expect(hasRoom(g, saxony, extra)).toBe(false);
    expect(moveCorps(g, extra, saxony)).toBe(false);
    expect(reach(g, extra).has(saxony.id)).toBe(false);
    expect(hasRoom(g, saxony, a)).toBe(true);
    expect(reach(g, a).has(saxony.id)).toBe(true);
    expect(moveCorps(g, a, saxony)).toBe(true);
    expect(railMove(g, b, saxony)).toBe(true);
    expect(hasRoom(g, saxony, c)).toBe(false);
    expect(moveCorps(g, c, saxony)).toBe(false);
    // And engineers already there take no room from the others.
    const dresden = clear(g.byName['Dresden']);
    const d = corps(g, 'GER', 'eng', dresden, 1);
    for (let k = 0; k < STACK - 1; k++) corps(g, 'GER', 'inf', dresden);
    expect(d.reg).toBe(dresden.id);
    expect(hasRoom(g, dresden, extra)).toBe(true);
  });

  it('breach fortifications when they go in with the assault', () => {
    const g = grand();
    const elbing = g.byName['Elbing'], mlawa = g.byName['Mlawa'];
    expect(crossingOf(elbing.id, mlawa.id)).toBeNull();
    expect(mlawa.fort).toMatchObject({ lv: 1 });
    const inf = corps(g, 'GER', 'inf', elbing);
    const bare = <T>(run: () => T): T => { const was = mlawa.fort; mlawa.fort = null; try { return run(); } finally { mlawa.fort = was; } };
    const full = FORT_MUL[1];
    expect(assess(g, [inf], mlawa).A / bare(() => assess(g, [inf], mlawa).A)).toBeCloseTo(1 / full, 9);
    expect(assess(g, [inf], mlawa).lines.some((l) => l.label === 'Engineers')).toBe(false);
    const saps = [1, 2, 3].map(() => corps(g, 'GER', 'eng', elbing, 1));
    const P = ENGINEERING.breach;
    for (const n of [1, 2, 3]) {
      const force = [inf, ...saps.slice(0, n)];
      const breach = Math.min(P.most, P.perDiv * n);
      expect(sappers(force)).toBe(n);
      const a = assess(g, force, mlawa);
      expect(a.A / bare(() => assess(g, force, mlawa).A), `${n} engineer corps`).toBeCloseTo(1 / (1 + (full - 1) * (1 - breach)), 9);
      expect(a.lines.find((l) => l.label === 'Engineers')!.effect).toBe(`They breach the works: the fortifications count for ${Math.round(breach * 100)}% less`);
    }
    // Engineers worn down to half strength breach half as much.
    saps[0].str = 0.5;
    expect(sappers([inf, saps[0]])).toBe(0.5);
  });
});

describe('minefields', () => {
  it('take 12% a belt off an attack and a move off the enemy, until engineers clear lanes', () => {
    const g = grand();
    const elbing = g.byName['Elbing'], mlawa = g.byName['Mlawa'];
    const inf = corps(g, 'GER', 'inf', elbing), sap = corps(g, 'GER', 'eng', elbing, 1);
    const clean = assess(g, [inf], mlawa).A, cleanBoth = assess(g, [inf, sap], mlawa).A, march = moveCost(g, inf, mlawa);
    for (const lv of [1, 2]) {
      g.flags[`mine:${mlawa.id}`] = lv;
      expect(minesAt(g, mlawa)).toBe(lv);
      const a = assess(g, [inf], mlawa);
      expect(a.A / clean, `${lv} belts`).toBeCloseTo(1 - ENGINEERING.mines.attack * lv, 9);
      expect(a.lines.find((l) => l.label === 'Minefields')!.effect).toBe(`Attack cut to ${Math.round((1 - ENGINEERING.mines.attack * lv) * 100)}%`);
      expect(moveCost(g, inf, mlawa)).toBe(march + ENGINEERING.mines.march);
      // A full engineer division with the attack clears the lanes.
      const b = assess(g, [inf, sap], mlawa);
      expect(b.A).toBeCloseTo(cleanBoth, 9);
      expect(b.lines.find((l) => l.label === 'Minefields')!.effect).toMatch(/engineers clear lanes: the mines cost the attack nothing/);
    }
    // Half an engineer division clears half.
    sap.str = 0.5;
    const part = assess(g, [inf, sap], mlawa);
    sap.str = 1;
    const base = assess(g, [inf, sap], mlawa).A;
    expect(part.A).toBeLessThan(base);
    expect(part.A).toBeGreaterThan(base * (1 - ENGINEERING.mines.attack * 2));
    // The owner's own troops walk through their own minefield at no cost.
    const lodz = g.byName['Lodz'];
    const pole = corps(g, 'POL', 'inf', clear(lodz));
    expect(lodz.nb.includes(mlawa.id)).toBe(true);
    const free = with_(ENGINEERING.mines as unknown as Record<string, number>, 'march', 0, () => moveCost(g, pole, mlawa));
    expect(moveCost(g, pole, mlawa)).toBe(free);
  });
});

describe('a saved game', () => {
  it('keeps its blown bridges, its mines and the work in hand', () => {
    const g = grand(3, 'GER');
    const x = between(g, 'Berlin', 'Neumark');
    down(g, x);
    g.flags[`mine:${g.byName['Saxony'].id}`] = 1;
    const sap = engineersOf(g, 'GER')[0];
    expect(orderEngineers(g, sap, 'bridge')).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(back).not.toBeNull();
    expect(bridgeDown(back, x)).toBe(true);
    expect(blownOn(back, x)).toBe(g.turn);
    expect(minesAt(back, back.byName['Saxony'])).toBe(1);
    const again = back.corps.find((c) => c.id === sap.id)!;
    expect(again.kind).toBe('eng');
    expect(again.job).toEqual(sap.job);
    nextMonth(back, 'GER');
    expect(bridgeDown(back, x)).toBe(false);
  });
});

describe('the computer players', () => {
  it('blow the bridges in front of a defence that is outweighed, and keep the ones an offensive means to cross', () => {
    const g = grand();
    const elbing = g.byName['Elbing'], pomorze = g.byName['Pomorze'];
    const x = between(g, 'Elbing', 'Pomorze');
    const blow = (nat: string) => aiBridges(g, nat, (c, from) => blowBridges(g, nat, c, from));
    // Evenly matched across the river, the Poles leave the bridges up.
    clear(elbing); clear(pomorze);
    corps(g, 'POL', 'inf', pomorze); corps(g, 'GER', 'inf', elbing);
    blow('POL');
    expect(bridgeDown(g, x)).toBe(false);
    // Germany is on the offensive against Poland: however strong the Poles, it keeps its bridges.
    corps(g, 'POL', 'inf', pomorze); corps(g, 'POL', 'inf', pomorze);
    expect(g.flags['st:on:GER:POL']).toBeGreaterThan(0);
    blow('GER');
    expect(bridgeDown(g, x)).toBe(false);
    // On the defensive it would blow them.
    const on = g.flags['st:on:GER:POL'];
    delete g.flags['st:on:GER:POL'];
    blow('GER');
    expect(bridgeDown(g, x)).toBe(true);
    g.flags['st:on:GER:POL'] = on;
    delete g.flags[`br:${x.a}:${x.b}`];
    // And when the Germans mass on the bank, the Poles blow them.
    for (let k = 0; k < 4; k++) corps(g, 'GER', 'inf', elbing);
    blow('POL');
    expect(bridgeDown(g, x)).toBe(true);
    // The player's bridges are the player's to blow.
    const h = grand(3, 'POL');
    for (let k = 0; k < 4; k++) corps(h, 'GER', 'inf', h.byName['Elbing']);
    aiBridges(h, 'POL', (c, from) => blowBridges(h, 'POL', c, from));
    expect(bridgeDown(h, between(h, 'Elbing', 'Pomorze'))).toBe(false);
  });

  it('send their engineers to the bridges that are down and set them to work on arrival', () => {
    const g = grand();
    const [a, b] = engineersOf(g, 'GER');
    // Nothing is broken: the engineers stay where they are, out of the staff's hands.
    aiEngineers(g, 'GER');
    expect([a.reg, b.reg]).toEqual([g.byName['Berlin'].id, g.byName['Berlin'].id]);
    expect(a.hold && b.hold).toBe(true);
    expect(a.job ?? b.job).toBeUndefined();
    const here = between(g, 'Berlin', 'Neumark'), there = between(g, 'Bavaria', 'Franconia');
    down(g, here); down(g, there);
    aiEngineers(g, 'GER');
    const atHome = [a, b].find((c) => c.reg === g.byName['Berlin'].id)!, sent = [a, b].find((c) => c !== atHome)!;
    expect(atHome.job).toMatchObject({ what: 'bridge', reg: g.byName['Berlin'].id });
    expect([g.byName['Bavaria'].id, g.byName['Franconia'].id]).toContain(sent.reg);
    expect(sent.job).toMatchObject({ what: 'bridge', reg: sent.reg });
    nextMonth(g, 'GER');
    expect(bridgeDown(g, here)).toBe(false);
    expect(bridgeDown(g, there)).toBe(false);
    // A frontier bridge is rebuilt for an offensive, never for a defence.
    const x = between(g, 'Elbing', 'Pomorze');
    down(g, x);
    for (const c of [a, b]) c.ap = c.apMax;
    delete g.flags['st:on:GER:POL'];
    aiEngineers(g, 'GER');
    expect(a.job ?? b.job).toBeUndefined();
  });

  it('fight a two-year war in which bridges go down and are rebuilt, and the rules hold throughout', () => {
    const g = grand(5);
    const keys = new Set(CROSSINGS.map((x) => `br:${x.a}:${x.b}`));
    let blown = 0, rebuilt = 0, before = new Set<string>();
    for (let t = 0; t < 26; t++) {
      runTurnAI(g);
      const now = new Set(Object.keys(g.flags).filter((k) => k.startsWith('br:')));
      for (const k of now) { expect(keys.has(k), k).toBe(true); if (!before.has(k)) blown++; }
      for (const k of before) if (!now.has(k)) rebuilt++;
      before = now;
      for (const k of Object.keys(g.flags)) if (k.startsWith('mine:')) { expect(g.flags[k]).toBeGreaterThanOrEqual(1); expect(g.flags[k]).toBeLessThanOrEqual(ENGINEERING.mines.most); expect(g.regions[+k.slice(5)]).toBeDefined(); }
      for (const c of g.corps) {
        if (c.dead) continue;
        if (c.job) { expect(c.kind, c.name).toBe('eng'); expect(['bridge', 'rail', 'plant', 'works', 'mines', 'demolish']).toContain(c.job.what); }
      }
      for (const nat of Object.keys(g.nations)) expect(engineersOf(g, nat).length, nat).toBeLessThanOrEqual(Math.max(ENGINEERING.ai.most, ENGINEERING.start.corps));
    }
    expect(blown).toBeGreaterThan(3);
    expect(rebuilt).toBeGreaterThan(0);
    // And the war went somewhere: Poland has fallen.
    expect(g.nations.POL.alive).toBe(false);
  }, 180000);
});
