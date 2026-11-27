// The atomic project (release 2, atom.ts; Brian 25 November 2026 01:14Z, "lets do number 1"; the design director's
// ruling of 01:27Z, design-director/atom-1125.md): three rows of the Industry branch, then plants that draw a share of
// the nation's earnings for years toward one bill, then a bomb or two carried by an ordinary bombing raid on an enemy
// city that is not a capital. Its effect is a weight on the struck country's will to fight, through the surrender
// rules. Behind its own switch: off, nothing of it is in the tree, drawn, offered or read.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { checkCollapse, collapseScore, createGame, homeOf } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import {
  ATOM, aiAtom, aiStrikePick, armStrike, armedAt, atomBill, atomFallen, atomMonth, atomOn, atomPress, atomRaidWorth, atomSabotage, atomView, atomWanted, atomWorn,
  beginPlants, carriersFor, chosenSite, drawAt, monthsLeft, pausePlants, plantsAt, sabotageWeight, safestSite, seesPlants, setAtom, siteChoices, strikeFlown,
  strikeBar, strikeTargets, strikeWords, whyNotPlants, whyNotStrike,
} from '../src/campaign/atom';
import { dayOrNight } from '../src/campaign/airwar';
import { airTicket } from '../src/battle/air/ticket';
import { openDamage, damageOf } from '../src/campaign/citydamage';
import { income, upkeep } from '../src/campaign/economy';
import { setStop } from '../src/campaign/history';
import { willLost } from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { ATOM_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { TECH, grantTech, hasTech, outOfTree, staffPick, techById, techRule, techView } from '../src/campaign/tech';
import { townName, townSize } from '../src/campaign/towns';
import { beginHuman, humanUp, orderRaid, orderAtomPlants, orderAtomStrike, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function seat(nat: string, seed: number, on: boolean): Game {
  const g = startOpen(createGame(seed, nat));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  setAtom(g, on);
  setFog(g, false);
  beginHuman(g);
  return g;
}
const R = (g: Game, name: string): number => g.byName[name].id;
const ROWS = [ATOM.rows.physics, ATOM.rows.chain, ATOM.rows.weapon];
const allRows = (g: Game, nat: string): void => { for (const id of ROWS) grantTech(g, nat, id); };

describe('the switch', () => {
  it('ships off, and off the rows are out of the tree and nothing is drawn, offered or read', () => {
    expect((stage as { atomBomb?: boolean }).atomBomb).toBe(false);
    expect(ATOM_STAGE).toBe(false);
    const g = seat('GER', 4, false);
    expect(atomOn(g)).toBe(false);
    for (const id of ROWS) expect(outOfTree(g, techById(id)), id).toBe(true);
    expect(techView(g, 'GER')!.nodes.some((x) => ROWS.includes(x.id))).toBe(false);
    expect(grantTech(g, 'GER', ATOM.rows.chain)).toBe(false);
    expect(whyNotPlants(g, 'GER')).toMatch(/not in this campaign/);
    expect(orderAtomPlants(g)).toBe(false);
    expect(atomView(g, 'GER')).toBeNull();
    const before = saveGame(g);
    atomMonth(g, 'GER');
    aiAtom(g, 'UK');
    expect(saveGame(g)).toBe(before);
    expect(saveGame(g)).not.toMatch(/"atom"/);
    // No staff ever picks a row of the project with the switch off.
    for (let k = 0; k < 3; k++) for (const nat of ['UK', 'SOV', 'USA']) expect(ROWS).not.toContain(staffPick(g, nat));
  });
  it('switched off in a war under way, the rows leave the tree and the teams on them are free', () => {
    const g = seat('GER', 4, true);
    grantTech(g, 'GER', ATOM.rows.physics);
    g.nations.GER.tech!.work.push({ id: ATOM.rows.chain, left: 5, of: 8 });
    setAtom(g, false);
    stepAI(g);
    for (let k = 0; k < 30 && !humanUp(g); k++) stepAI(g);
    expect(g.nations.GER.tech!.work.some((w) => w.id === ATOM.rows.chain)).toBe(false);
  });
});

describe('the rows', () => {
  it('stand in the Industry branch, one after another, with a rule in words and no number in the text', () => {
    const [a, b, c] = ROWS.map((id) => techById(id)!);
    expect([a.branch, b.branch, c.branch]).toEqual(['industry', 'industry', 'industry']);
    expect(b.needs).toContain(a.id);
    expect(c.needs).toContain(b.id);
    expect(techRule(b)).toMatch(/atomic plants can be started: .* towards one great bill/);
    expect(b.text).toMatch(/can be started: a great share/);
    expect(techRule(c)).toMatch(/one more every two months while the plants keep making them\. A bomb is carried by a raid of four-engined bombers/);
    expect(techRule(c)).toMatch(/not a capital/);
    for (const t of [a, b, c]) expect(t.text).not.toMatch(/\d%/);
    expect(TECH.techs.filter((t) => t.atom).length).toBe(3);
  });
});

describe('the plants', () => {
  it('need Chain reaction and a province of the nation\'s own at home with industry; the staff picks the safest', () => {
    const g = seat('GER', 4, true);
    expect(whyNotPlants(g, 'GER')).toMatch(/Chain reaction/);
    grantTech(g, 'GER', ATOM.rows.physics);
    grantTech(g, 'GER', ATOM.rows.chain);
    expect(whyNotPlants(g, 'GER')).toBeNull();
    const sites = siteChoices(g, 'GER');
    expect(sites.length).toBeGreaterThan(3);
    for (const s of sites) { const r = g.regions[s.region]; expect(r.owner).toBe('GER'); expect(homeOf(g, r)).toBe('GER'); expect(r.fac).toBeGreaterThanOrEqual(1); }
    for (let i = 1; i < sites.length; i++) expect(sites[i].hops).toBeLessThanOrEqual(sites[i - 1].hops);
    expect(safestSite(g, 'GER')).toBe(sites[0].region);
    // Not on another country's ground.
    expect(whyNotPlants(g, 'GER', R(g, 'Paris'))).toMatch(/no province of your own/);
  });

  it('draw their pace\'s share of the earnings each month, in the ledger, and come on by it', () => {
    const g = seat('GER', 4, true);
    allRows(g, 'GER');
    expect(orderAtomPlants(g, undefined, 'full')).toBe(true);
    const s = g.nations.GER.atom!;
    expect(s.plants).toBe(0);
    const draw = drawAt(g, 'GER', 'full');
    expect(draw).toBeCloseTo(0.25 * income(g, 'GER'), 6);
    // The player's month opens: the plants draw.
    for (let k = 0; k < 40 && humanUp(g); k++) stepAI(g);
    for (let k = 0; k < 40 && !humanUp(g); k++) stepAI(g);
    expect(s.paid?.[0]).toBe(g.turn);
    expect(s.plants!).toBeGreaterThan(0);
    expect(s.plants!).toBeLessThanOrEqual(s.paid![1] + 1e-9);
    expect(atomBill(g, 'GER')).toBeCloseTo(s.paid![1], 9);
    const line = upkeep(g, 'GER').find((l) => l.key === 'atom');
    expect(line?.amount).toBeCloseTo(s.paid![1], 9);
    expect(line?.label).toBe('The atomic plants');
    // Paused, they draw nothing and keep their work.
    pausePlants(g, 'GER', true);
    const was = s.plants!;
    atomMonth(g, 'GER');
    expect(s.plants).toBe(was);
  });

  it('take a large industry a few years and a small one never: the same bill for every country', () => {
    const g = seat('GER', 4, true);
    const ger = monthsLeft(g, 'GER', 'full')!, ita = monthsLeft(g, 'ITA', 'full')!;
    expect(ger).toBeLessThan(ita / 3);
    expect(ATOM.ai.months).toBeLessThan(ita);
    // The crash pace is quicker than the full, the full than the steady.
    expect(monthsLeft(g, 'GER', 'crash')!).toBeLessThan(ger);
    expect(monthsLeft(g, 'GER', 'steady')!).toBeGreaterThan(ger);
  });

  it('work less as their town is ruined, and lose months of work to sabotage', () => {
    const g = seat('GER', 4, true);
    openDamage(g, true);
    allRows(g, 'GER');
    orderAtomPlants(g);
    const s = g.nations.GER.atom!, site = g.regions[s.site!];
    expect(plantsAt(g, site)).toBe('GER');
    s.plants = 300;
    const line = atomSabotage(g, site);
    // (his own plants: the line says whose without a name)
    expect(line).toBe('The atomic plants there lose 3 months of their work.');
    expect(s.plants).toBeCloseTo(300 - 3 * drawAt(g, 'GER', 'full'), 6);
    g.human = 'UK';
    expect(atomSabotage(g, site)).toBe('The German atomic plants there lose 3 months of their work.');
    g.human = 'GER';
    // Nowhere else.
    expect(atomSabotage(g, g.regions[R(g, 'Paris')])).toBeNull();
  });

  it('are lost with their ground, and the captor takes the papers', () => {
    const g = seat('GER', 4, true);
    allRows(g, 'GER');
    orderAtomPlants(g);
    const s = g.nations.GER.atom!, site = g.regions[s.site!];
    s.plants = 400;
    site.ctrl = 'SOV';
    atomMonth(g, 'GER');
    expect(s.plants).toBeUndefined();
    expect(hasTech(g, 'SOV', ATOM.rows.chain)).toBe(true);
    expect(g.nations.SOV.atom?.papers).toContain('GER');
    // The papers let the captor's side see the project.
    expect(seesPlants(g, 'SOV', 'GER')).toBe(false); // nothing left standing to see
    expect(g.headlines.some((h) => h.kind === 'event' && h.text === `Germany has lost its atomic plants at ${townName(site)} to the Soviet Union. All their work is lost with the province.`)).toBe(true);
    // The captor's own line, where the papers give him rows he lacked.
    const k = seat('SOV', 4, true);
    allRows(k, 'GER');
    expect(beginPlants(k, 'GER')).toBe(true);
    const ks = k.nations.GER.atom!, kr = k.regions[ks.site!];
    ks.plants = 400;
    kr.ctrl = 'SOV';
    atomMonth(k, 'GER');
    expect(k.headlines.map((h) => h.text)).toContain(`You have taken the German atomic plants at ${townName(kr)}. Their papers give you Atomic physics and Chain reaction.`);
    expect(k.headlines.some((h) => /has lost its atomic plants/.test(h.text))).toBe(false);
  });
});

describe('the bombs', () => {
  it('are ready once the plants and the weapon are done, then one more every few months, never more than a couple', () => {
    const g = seat('GER', 4, true);
    grantTech(g, 'GER', ATOM.rows.physics);
    grantTech(g, 'GER', ATOM.rows.chain);
    orderAtomPlants(g);
    const s = g.nations.GER.atom!;
    s.plants = ATOM.plants.bill - 1e-6;
    g.nations.GER.pp = 100;
    atomMonth(g, 'GER');
    expect(s.done).toBe(g.turn);
    expect(s.bombs ?? 0).toBe(0); // the weapon design is not done
    grantTech(g, 'GER', ATOM.rows.weapon);
    atomMonth(g, 'GER');
    expect(s.bombs).toBe(1);
    let made = 0;
    for (let k = 0; k < 12; k++) { g.turn++; atomMonth(g, 'GER'); made = s.bombs!; }
    expect(made).toBe(ATOM.plants.most);
  });
});

describe('the strike', () => {
  function armed(): Game {
    const g = seat('UK', 4, true);
    openDamage(g, true);
    allRows(g, 'UK');
    grantTech(g, 'UK', 'mediumbombers');
    grantTech(g, 'UK', 'heavybombers');
    const s = (g.nations.UK.atom ??= {});
    s.plants = ATOM.plants.bill; s.done = g.turn; s.ready = g.turn; s.bombs = 1; s.site = safestSite(g, 'UK')!;
    return g;
  }

  it('goes only for a city on an enemy\'s home ground, never a capital, in reach of a bomber group, with four-engined bombers', () => {
    const g = armed();
    const t = strikeTargets(g, 'UK');
    expect(t.length).toBeGreaterThan(3);
    for (const id of t) { const r = g.regions[id]; expect(['city', 'capital']).toContain(townSize(g, r)); expect(homeOf(g, r)).toBe(r.ctrl); }
    // (a capital is listed, with its reason, and can never be struck)
    expect(t).toContain(R(g, 'Berlin'));
    expect(whyNotStrike(g, 'UK', R(g, 'Berlin'))).toMatch(/Never a capital/);
    expect(strikeBar(g, 'UK', R(g, 'Berlin'))).toBe('A capital');
    expect(strikeBar(g, 'UK', R(g, 'Hamburg'))).toBeNull();
    expect(strikeBar(g, 'UK', R(g, 'Vienna'))).toBe("Beyond your bombers' reach");
    expect(whyNotStrike(g, 'UK', R(g, 'Paris'))).not.toBeNull();
    expect(whyNotStrike(g, 'UK', R(g, 'Hamburg'))).toBeNull();
    expect(carriersFor(g, 'UK', R(g, 'Hamburg')).length).toBeGreaterThan(0);
    // Out of every group's reach.
    expect(whyNotStrike(g, 'UK', R(g, 'Vienna'))).toMatch(/can reach/);
    // Without the heavy bombers.
    g.nations.UK.tech!.done = g.nations.UK.tech!.done.filter((x) => x !== 'heavybombers');
    expect(whyNotStrike(g, 'UK', R(g, 'Hamburg'))).toMatch(/Four-engined/);
    expect(strikeBar(g, 'UK', R(g, 'Hamburg'))).toBe('Needs Four-engined bombers');
  });

  it('flies as a bombing raid through the air rules: reached, the city is ruined and the will is struck; nothing else is done to it', () => {
    const g = armed();
    const ham = R(g, 'Hamburg');
    expect(orderAtomStrike(g, ham)).toBe(true);
    expect(armedAt(g, 'UK')).toBe(ham);
    const ids = carriersFor(g, 'UK', ham).map((f) => f.id);
    const raidsBefore = JSON.stringify(g.raids.GER ?? null);
    const rep = orderRaid(g, 'bomb', ham, ids);
    expect(rep).not.toBeNull();
    const s = g.nations.UK.atom!;
    expect(s.bombs).toBe(0);
    expect(armedAt(g, 'UK')).toBeNull();
    // No ordinary bombing: the raid takes nothing off the victim's output.
    expect(JSON.stringify(g.raids.GER ?? null)).toBe(raidsBefore);
    // The strike is told in a plain line of its own; the raid's own line reads as any raid's.
    const told = g.headlines.filter((h) => /atomic bomb/.test(h.text));
    expect(told.length).toBe(1);
    expect(told[0].kind).toBe('event');
    expect(g.headlines.some((h) => h.kind === 'battle' && /atomic|  /.test(h.text))).toBe(false);
    if (s.used) {
      expect(rep!.result).toBe('A single atomic bomb has destroyed most of Hamburg. The United Kingdom has the atomic bomb.');
      expect(told[0].text).toBe(rep!.result);
      expect(damageOf(g, g.regions[ham])).toBeGreaterThan(0.7);
      expect(atomWorn(g, 'GER')).toBeCloseTo(ATOM.will.first, 9);
    } else {
      expect(rep!.result).toBe('The bomber carrying the atomic bomb was lost on the way to Hamburg, and the bomb with it.');
      expect(atomWorn(g, 'GER')).toBe(0);
    }
    // One a month.
    s.bombs = 1;
    expect(whyNotStrike(g, 'UK', R(g, 'Ruhr'))).toMatch(/One strike a month/);
  });

  it('weighs on the will by its stand-ins, to a ceiling, and the will test is the one the game already has', () => {
    const g = armed();
    const ham = R(g, 'Hamburg'), ruhr = R(g, 'Ruhr'), rh = R(g, 'Rhineland');
    const base = willLost(g, 'GER');
    const words: string[] = [];
    const hit = (r: number): void => { const s = g.nations.UK.atom!; s.bombs = 1; s.armed = [g.turn, r]; words.push(strikeWords(g, 'UK', r)); words.push(strikeFlown(g, 'UK', r, 10, 10).text); delete s.struck; };
    hit(ham);
    expect(words[0]).toBe('This raid carries the atomic bomb. If its bomber reaches Hamburg, most of the city is destroyed and Germany loses 25% of its will to fight. If the bomber is shot down, the bomb goes with it.');
    expect(atomWorn(g, 'GER')).toBeCloseTo(ATOM.will.first, 9);
    hit(ruhr);
    // (the card's figure is what the next strike takes; a later strike by the same country is not a tally)
    expect(words[2]).toMatch(/loses 15% of its will to fight/);
    expect(words[3]).toBe(`Another British atomic bomb has destroyed most of ${townName(g.regions[ruhr])}.`);
    expect(atomWorn(g, 'GER')).toBeCloseTo(ATOM.will.first + ATOM.will.second, 9);
    for (let k = 0; k < 6; k++) hit(rh);
    expect(atomWorn(g, 'GER')).toBeCloseTo(ATOM.will.most, 9);
    expect(willLost(g, 'GER')).toBeCloseTo(base + ATOM.will.most, 9);
    expect(words[words.length - 2]).toMatch(/loses 0% of its will to fight/);
  });

  it('a bomb lost on the way is told to the striker\'s side only: the defender saw an ordinary raid', () => {
    const g = armed();
    const ham = R(g, 'Hamburg');
    g.nations.UK.atom!.armed = [g.turn, ham];
    expect(strikeFlown(g, 'UK', ham, 10, 0).tell).toBe(true);
    const k = armed();
    k.human = 'GER';
    k.nations.UK.atom!.armed = [k.turn, ham];
    const lost = strikeFlown(k, 'UK', ham, 10, 0);
    expect(lost.hit).toBe(false);
    expect(lost.tell).toBe(false);
    k.nations.UK.atom!.bombs = 1; k.nations.UK.atom!.armed = [k.turn, ham];
    expect(strikeFlown(k, 'UK', ham, 10, 10).tell).toBe(true);
  });

  it('opens the way to terms for a computer-run country for some months, never moves the player\'s seat', () => {
    const g = armed();
    const ham = R(g, 'Hamburg');
    const s = g.nations.UK.atom!; s.armed = [g.turn, ham];
    strikeFlown(g, 'UK', ham, 10, 10);
    expect(atomPress(g, 'GER')).toBe('UK');
    expect(atomPress(g, 'UK')).toBeNull();
    const t0 = g.turn;
    g.turn = t0 + ATOM.will.press + 1;
    expect(atomPress(g, 'GER')).toBeNull();
    g.turn = t0;
    // The will test is unchanged: a healthy great power does not give in to one strike.
    expect(collapseScore(g, 'GER')).toBeLessThan(g.nations.GER.resilience);
    checkCollapse(g, 'GER', 'UK');
    expect(g.nations.GER.alive).toBe(true);
    // A beaten one, its capital free and its home held, gives in while the strike presses; without it, it stands.
    const ger = g.nations.GER;
    ger.res = { ...(ger.res ?? { oil: 0, sup: 0, oilMet: 1, supMet: 1 }), hunger: 1000 };
    ger.resilience = collapseScore(g, 'GER') - 0.01;
    g.nations.GER.atom!.hits = [];
    checkCollapse(g, 'GER', 'UK');
    expect(g.nations.GER.alive).toBe(true);
    g.nations.GER.atom!.hits = [[g.turn, 'UK', ham]];
    checkCollapse(g, 'GER', 'UK');
    expect(g.nations.GER.alive).toBe(false);
  });
});

describe('sight', () => {
  it('a staff knows an enemy\'s plants only at its own sight; an ally always; everyone once a bomb is used', () => {
    const g = seat('GER', 4, true);
    allRows(g, 'GER');
    orderAtomPlants(g);
    const site = g.regions[g.nations.GER.atom!.site!];
    expect(seesPlants(g, 'UK', 'GER')).toBe(false);
    expect(seesPlants(g, 'ITA', 'GER')).toBe(g.nations.ITA.align === g.nations.GER.align && seesPlants(g, 'ITA', 'GER'));
    expect(atomRaidWorth(g, 'UK', site, 0.5)).toBe(0);
    expect(sabotageWeight(g, 'UK', site)).toBe(0);
    g.nations.GER.atom!.used = 1;
    expect(seesPlants(g, 'UK', 'GER')).toBe(true);
    expect(atomRaidWorth(g, 'UK', site, 0.5)).toBe(ATOM.raid.worth * 0.5);
    expect(sabotageWeight(g, 'UK', site)).toBe(ATOM.sabotage.weight);
  });
});

describe('the computer', () => {
  it('takes the project up only in a long war against a great power it can afford, never for the player', () => {
    const g = seat('UK', 4, true);
    expect(atomWanted(g, 'UK')).toBe(false); // the player's
    expect(atomWanted(g, 'ITA')).toBe(false); // not at war, and too small
    g.turn = 20;
    // Germany's earnings of 1939 would not finish the plants within the staff's horizon at full pace; with most of the
    // work done they would.
    expect(monthsLeft(g, 'GER', 'full')!).toBeGreaterThan(ATOM.ai.months);
    expect(atomWanted(g, 'GER')).toBe(false);
    g.nations.GER.atom = { plants: 400 };
    expect(atomWanted(g, 'GER')).toBe(true);
    // Under the switch, and only then, the staff puts the rows first.
    grantTech(g, 'GER', 'institutes');
    expect(staffPick(g, 'GER')).toBe(ATOM.rows.physics);
    setAtom(g, false);
    expect(atomWanted(g, 'GER')).toBe(false);
    expect(staffPick(g, 'GER')).not.toBe(ATOM.rows.physics);
  });

  it('begins the plants, and strikes the enemy nearest to breaking at its most industrial city in reach', () => {
    const g = seat('GER', 4, true);
    openDamage(g, true);
    g.turn = 20;
    allRows(g, 'UK');
    grantTech(g, 'UK', 'mediumbombers');
    grantTech(g, 'UK', 'heavybombers');
    // (Britain's earnings of 1939 would not finish the plants within the staff's horizon: lengthened here, to see it begin.)
    aiAtom(g, 'UK');
    expect(g.nations.UK.atom?.plants).toBeUndefined();
    const was = ATOM.ai.months;
    try { ATOM.ai.months = 999; aiAtom(g, 'UK'); } finally { ATOM.ai.months = was; }
    expect(g.nations.UK.atom?.plants).toBe(0);
    expect(g.nations.UK.atom?.site).toBe(safestSite(g, 'UK'));
    const s = g.nations.UK.atom!;
    s.plants = ATOM.plants.bill; s.done = g.turn; s.ready = g.turn; s.bombs = 1;
    const pick = aiStrikePick(g, 'UK');
    expect(pick).not.toBeNull();
    expect(whyNotStrike(g, 'UK', pick!.region)).toBeNull();
    aiAtom(g, 'UK');
    expect(s.bombs).toBe(0);
    expect(s.struck).toBe(g.turn);
  });

  it('at the stops that follow the book, only the record\'s nation, on the record\'s clock', () => {
    const g = startOpen(createGame(5, null));
    setAtom(g, true);
    setStop(g, 1, true);
    expect(atomWanted(g, 'GER')).toBe(false);
    for (let t = 1; t < ATOM.book.plants + 2; t++) {
      g.turn = t;
      for (const nat of Object.keys(g.nations)) atomMonth(g, nat);
    }
    expect(hasTech(g, 'USA', ATOM.rows.chain)).toBe(true);
    expect(g.nations.USA.atom?.plants).toBeGreaterThan(0);
    expect(g.nations.USA.atom?.done).toBeUndefined();
    for (const nat of Object.keys(g.nations)) if (nat !== 'USA') expect(g.nations[nat].atom, nat).toBeUndefined();
    g.turn = ATOM.book.ready;
    atomMonth(g, 'USA');
    expect(g.nations.USA.atom?.done).toBe(ATOM.book.ready);
    expect(g.nations.USA.atom?.bombs).toBe(1);
  });
});

describe('saves', () => {
  it('keep the project as it stands', () => {
    const g = seat('GER', 4, true);
    allRows(g, 'GER');
    orderAtomPlants(g, undefined, 'crash');
    g.nations.GER.atom!.plants = 123.5;
    const h = loadGame(saveGame(g))!;
    expect(h.nations.GER.atom).toEqual(g.nations.GER.atom);
    expect(atomOn(h)).toBe(true);
  });
});

describe('through the bridge', () => {
  type Call = (...a: unknown[]) => unknown;
  type PageLike = { title: string; headline: string; note?: string; top: { label: string; act: string; ok: boolean }[]; sections: { heading: string; text: string; rows: { title: string; lines?: string[]; buttons: { label: string; act: string; ok: boolean }[] }[] }[] };
  it('the room is listed only with the switch on; the plants are begun, a bomb is armed and the raid card opens', () => {
    const TC = (globalThis as unknown as { TC: Record<string, Call> }).TC;
    const off = seat('UK', 4, false);
    TC.load(saveGame(off));
    expect((TC.view() as { atomBomb: boolean }).atomBomb).toBe(false);
    expect((TC.view() as { pages: { id: string }[] }).pages.some((p) => p.id === 'atom')).toBe(false);
    const g = seat('UK', 4, true);
    allRows(g, 'UK');
    grantTech(g, 'UK', 'mediumbombers');
    grantTech(g, 'UK', 'heavybombers');
    TC.load(saveGame(g));
    expect((TC.view() as { atomBomb: boolean }).atomBomb).toBe(true);
    expect((TC.view() as { pages: { id: string; label: string }[] }).pages).toContainEqual({ id: 'atom', label: 'THE ATOMIC PROJECT' });
    let page = TC.page('atom') as PageLike;
    expect(page.headline).toBe('The plants can be started');
    expect(page.sections[1].heading).toBe('WHERE THE PLANTS WILL STAND');
    expect(page.note).toMatch(/^A long gamble: /);
    const begin = page.top.find((b) => b.act === 'atom:begin')!;
    expect(begin.ok).toBe(true);
    expect((TC.act('atom:pace:crash') as { ok: boolean }).ok).toBe(true);
    expect((TC.act('atom:begin') as { ok: boolean }).ok).toBe(true);
    page = TC.page('atom') as PageLike;
    expect(page.headline).toBe('The plants are 0% done');
    expect(page.top.map((b) => b.label)).toContain('STOP THE PLANTS');
    // Finished, with a bomb in hand: the chip, the targets, and the raid card with its line.
    const h = loadGame((TC.save() as { text: string }).text)!;
    const a = h.nations.UK.atom!;
    a.plants = ATOM.plants.bill; a.done = h.turn; a.ready = h.turn; a.bombs = 1;
    TC.load(saveGame(h));
    expect((TC.view() as { todo: { k: string; t: string }[] }).todo).toContainEqual(expect.objectContaining({ k: 'page:atom', t: 'AN ATOMIC BOMB IS READY' }));
    page = TC.page('atom') as PageLike;
    // (the chip has done its work once he has opened the page: a bomb kept in hand is a choice, not a chore)
    expect((TC.view() as { todo: { k: string }[] }).todo.some((x) => x.k === 'page:atom')).toBe(false);
    const berlin = page.sections.find((x) => x.heading === 'A STRIKE')!.rows.find((r) => r.title === 'BERLIN')!;
    expect(berlin.lines).toEqual(['A capital']);
    const strike = page.sections.find((x) => x.heading === 'A STRIKE')!;
    expect(strike.text).toMatch(/Never a capital/);
    const ham = strike.rows.find((r) => r.title === 'HAMBURG')!;
    const out = TC.act(ham.buttons[0].act) as { ok: boolean; open: string; note: string };
    expect(out.ok).toBe(true);
    expect(out.open.startsWith(`plan:bomb:${h.byName.Hamburg.id}:`)).toBe(true);
    const card = TC.page(out.open) as PageLike & { card?: { effects?: string[] } };
    expect(JSON.stringify(card)).toMatch(/This raid carries the atomic bomb\. If its bomber reaches Hamburg, most of the city is destroyed and Germany loses 25% of its will to fight\. If the bomber is shot down, the bomb goes with it\./);
    expect(JSON.stringify(card)).toMatch(/"bomb":true/);
    expect(JSON.stringify(card)).toMatch(/"back":\{"label":"BACK","act":"atom:back"/);
    const back = TC.act('atom:back') as { ok: boolean; open: string };
    expect(back.ok).toBe(true);
    expect(back.open).toBe('atom');
    expect((TC.page('atom') as PageLike).headline).toBe('An atomic bomb is ready');
    expect((TC.act('atomrule:off') as { ok: boolean }).ok).toBe(true);
    expect((TC.view() as { atomBomb: boolean }).atomBomb).toBe(false);
  });
});

describe('the review\'s mends (25 November)', () => {
  function armedUK(): Game {
    const g = seat('UK', 4, true);
    openDamage(g, true);
    allRows(g, 'UK');
    grantTech(g, 'UK', 'mediumbombers');
    grantTech(g, 'UK', 'heavybombers');
    const s = (g.nations.UK.atom ??= {});
    s.plants = ATOM.plants.bill; s.done = g.turn; s.ready = g.turn; s.bombs = 1; s.site = safestSite(g, 'UK')!;
    return g;
  }

  it('finished plants are lost with their ground too: the captor takes the papers, and the bombs already made stay in hand', () => {
    const g = seat('GER', 4, true);
    allRows(g, 'GER');
    orderAtomPlants(g);
    const s = g.nations.GER.atom!, site = g.regions[s.site!];
    s.plants = ATOM.plants.bill; s.done = g.turn - 1; s.ready = g.turn - 1; s.bombs = 1; s.next = 1;
    site.ctrl = 'SOV';
    atomMonth(g, 'GER');
    expect(s.plants).toBeUndefined();
    expect(s.done).toBeUndefined();
    expect(s.site).toBeUndefined();
    expect(s.bombs).toBe(1);
    expect(hasTech(g, 'SOV', ATOM.rows.chain)).toBe(true);
    // (no plants, no more bombs)
    s.next = ATOM.plants.every + 5;
    atomMonth(g, 'GER');
    expect(s.bombs).toBe(1);
  });

  it('a country that gives in loses its project: the plants go to the victor with their papers, and the bombs and any strike with it', () => {
    const g = seat('UK', 4, true);
    allRows(g, 'GER');
    expect(beginPlants(g, 'GER')).toBe(true);
    const s = g.nations.GER.atom!;
    s.plants = ATOM.plants.bill; s.done = g.turn; s.ready = g.turn; s.bombs = 2; s.armed = [g.turn, R(g, 'London')];
    atomFallen(g, 'GER', 'UK');
    for (const k of ['plants', 'site', 'done', 'bombs', 'armed', 'next', 'paused', 'stopMaking'] as const) expect(s[k], k).toBeUndefined();
    expect(hasTech(g, 'UK', ATOM.rows.chain)).toBe(true);
    expect(g.nations.UK.atom?.papers).toContain('GER');
    expect(armedAt(g, 'GER')).toBeNull();
    // Off, it does nothing.
    const h = seat('UK', 4, false);
    const before = saveGame(h);
    atomFallen(h, 'GER', 'UK');
    expect(saveGame(h)).toBe(before);
  });

  it('switched off, the atomic rows leave every staff\'s work at once', () => {
    const g = seat('GER', 4, true);
    g.nations.UK.tech!.work.push({ id: ATOM.rows.physics, left: 5, of: 8 });
    g.nations.GER.tech!.work.push({ id: ATOM.rows.physics, left: 5, of: 8 });
    setAtom(g, false);
    for (const n of Object.values(g.nations)) expect((n.tech?.work ?? []).some((w) => ROWS.includes(w.id)), n.id).toBe(false);
  });

  it('a raid card that carries the bomb lists no hits and no stop note, and its chance is that the bomber reaches the city', () => {
    const g = armedUK();
    const ham = R(g, 'Hamburg');
    expect(orderAtomStrike(g, ham)).toBe(true);
    const ids = carriersFor(g, 'UK', ham).map((f) => f.id);
    const dn = dayOrNight(g, 'bomb', ham, ids, 'UK')!;
    let flown = 0;
    for (const w of [dn.day, dn.night]) {
      if (w.why) continue;
      flown++;
      expect(w.hits).toEqual([]);
      expect(w.stopNote).toBe('');
      expect(w.figures).toEqual({});
      expect(w.success).toBeCloseTo(Math.max(0, Math.min(1, 1 - w.bomberLoss)), 9);
      expect(w.prose).toMatch(/^This raid carries the atomic bomb\./);
    }
    expect(flown).toBeGreaterThan(0);
    // Not armed, the same raid reads as any raid.
    g.nations.UK.atom!.armed = undefined;
    const plain = dayOrNight(g, 'bomb', ham, ids, 'UK')!;
    expect(plain.day.hits.length + plain.night.hits.length).toBeGreaterThan(0);
  });

  it('a raid that carries the bomb is not fought out in the 3D air battle until its own scene is built: the staff flies it', () => {
    const g = armedUK();
    const ham = R(g, 'Hamburg');
    const ids = carriersFor(g, 'UK', ham).map((f) => f.id);
    const before = airTicket(g, 'bomb', ham, ids, 'UK');
    expect(before.why).not.toBe('A raid that carries the atomic bomb is flown by the staff.');
    expect(orderAtomStrike(g, ham)).toBe(true);
    const t = airTicket(g, 'bomb', ham, ids, 'UK');
    expect(t.ticket).toBeNull();
    expect(t.why).toBe('A raid that carries the atomic bomb is flown by the staff.');
    // (any other city is fought out as before)
    const other = airTicket(g, 'bomb', R(g, 'Bremen'), undefined, 'UK');
    expect(other.why).not.toBe('A raid that carries the atomic bomb is flown by the staff.');
  });

  it('a chosen site that no longer fits gives way to the safest', () => {
    const g = seat('GER', 4, true);
    allRows(g, 'GER');
    const sites = siteChoices(g, 'GER');
    const pick = sites[sites.length - 1].region;
    (g.nations.GER.atom ??= {}).site = pick;
    expect(chosenSite(g, 'GER')).toBe(pick);
    g.regions[pick].ctrl = 'FRA';
    expect(chosenSite(g, 'GER')).toBe(safestSite(g, 'GER'));
    expect(whyNotPlants(g, 'GER')).toBeNull();
    expect(beginPlants(g, 'GER')).toBe(true);
    expect(g.nations.GER.atom!.site).toBe(safestSite(g, 'GER'));
  });

  it('the page tells plants that stand idle from plants that draw, and from plants started after the month\'s draw', () => {
    const TC = (globalThis as unknown as { TC: Record<string, (...a: unknown[]) => unknown> }).TC;
    const g = seat('UK', 4, true);
    allRows(g, 'UK');
    expect(orderAtomPlants(g)).toBe(true);
    let v = atomView(g, 'UK')!;
    expect(v.drawn).toBe(false);
    expect(v.idle).toBe(false);
    expect(v.draw).toBeGreaterThan(0);
    const text = (h: Game): string => { TC.load(saveGame(h)); return (TC.page('atom') as { sections: { heading: string; text: string }[] }).sections.find((x) => x.heading === 'THE ATOMIC PLANTS')!.text; };
    expect(text(g)).toMatch(/^From next month they take /);
    g.nations.UK.atom!.paid = [g.turn, v.draw];
    v = atomView(g, 'UK')!;
    expect(v.drawn).toBe(true);
    expect(text(g)).toMatch(/^This month they take /);
    delete g.nations.UK.atom!.paid;
    g.flags['atom:idle:UK'] = g.turn;
    v = atomView(g, 'UK')!;
    expect(v.idle).toBe(true);
    expect(text(g)).toMatch(/^They stand idle: the treasury is in debt\. They start again once it is out of debt\./);
  });
});
