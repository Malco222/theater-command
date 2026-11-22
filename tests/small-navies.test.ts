// Ruling 235 (Brian, 18 November): "they can get some ships but not carriers or battleships like the major nations.
// they only get this technology if they capture one of the major players cities, factories, territory"; "spys from
// smaller nations can also try and steal technology plans for more powerful weapons that the major countries have
// like battleships ect"; "being caught could have that nation declare war on your"; "keep british ships".
// The seven great powers lay down battleships and carriers as before; the dominions build on Britain's plans and
// Vichy and Free France on France's. Every other country lays one down only on a great power's plans, taken with
// ground that power owns, in a war with it, or stolen by an agent, who may bring war on his country if caught.
import { describe, expect, it } from 'vitest';
import arsenal from '../data/campaign/arsenal.json';
import { offers, orderOffer, yardLevel } from '../src/campaign/arsenal';
import { addCorps, allied, atWar, capture, createGame } from '../src/campaign/game';
import { JOBS, jobsFor, orderJob } from '../src/campaign/engineers';
import { INTEL, MISSIONS, MISSION_TEXT, THEFT, caughtChance, guardChance, intelTick, mostAgents, successChance, whyNotMission } from '../src/campaign/intel';
import { PLANS, PLAN_ITEMS, buildsOwn, givesPlans, grantPlans, onPlans, planRules, plansFrom, plansOf, whyNoPlans, whyNoTechPlans } from '../src/campaign/plans';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { inOrAt } from '../src/campaign/towns';
import { TECH, hasTech, locked, spyOf, spyTeamsOf, staffPick, startResearch, staffTeams, teamsOf, techOf, techTick, techView, whyNotResearch } from '../src/campaign/tech';
import { capitalPlans } from '../src/campaign/turn';
import type { Agent, Game, Mission } from '../src/campaign/types';

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const offer = (g: Game, nat: string, key: string) => offers(g, nat).find((o) => o.key === key)!;
/** Every home port of the nation made a great yard, and money to spend: what stands between it and a battleship is then the plans alone. */
function ready(g: Game, nat: string): void {
  g.nations[nat].pp = 5000;
  for (const r of g.regions) if (r.owner === nat && r.ctrl === nat && r.port) g.flags[`wk:yard:${r.id}`] = 3;
}
function plant(g: Game, at: string, mission: Mission, nat: string, xp = 0): Agent {
  const a: Agent = { id: g.intel.next++, nat, name: `Test ${g.intel.next}`, xp, reg: g.byName[at].id, ready: g.turn, mission, born: g.turn };
  g.intel.agents.push(a);
  return a;
}

describe('the data', () => {
  it('names the seven great powers and the kin of two of them', () => {
    expect(PLANS.great).toEqual(['GER', 'UK', 'USA', 'SOV', 'FRA', 'ITA', 'JAP']);
    expect(PLANS.hulls).toEqual(['bb', 'cv']);
    expect(PLANS.techs).toEqual(['heavybombers', 'jets', 'rockets', 'guided']);
    for (const [kin, of] of Object.entries(PLANS.kin)) expect(PLANS.great, kin).toContain(of);
    expect(PLANS.kin.CAN).toBe('UK');
    expect(PLANS.kin.VIC).toBe('FRA');
    expect((arsenal as any).plans.on).toBe(true);
  });
});

describe('who builds battleships and carriers', () => {
  it('a great power and its kin as before; a smaller nation not without plans', () => {
    const g = grand();
    expect(planRules(g)).toBe(true);
    for (const nat of ['GER', 'UK', 'SOV', 'CAN', 'VIC']) expect(buildsOwn(nat), nat).toBe(true);
    for (const nat of ['POL', 'ESP', 'TUR', 'ROM']) expect(buildsOwn(nat), nat).toBe(false);
    expect(plansFrom(g, 'UK', 'bb')).toBe('UK');
    expect(plansFrom(g, 'CAN', 'cv')).toBe('UK');
    expect(plansFrom(g, 'VIC', 'bb')).toBe('FRA');
    expect(plansFrom(g, 'ESP', 'bb')).toBeNull();
    ready(g, 'GER'); ready(g, 'ESP');
    // Germany lays down battleships; its carriers wait on the tech tree, as before.
    expect(offer(g, 'GER', 'ship:bb').why).toBeNull();
    expect(offer(g, 'GER', 'ship:cv').why).toMatch(/Carrier aviation/);
    // Spain, with a great yard, builds destroyers, submarines and cruisers, and no capital ship.
    for (const k of ['ship:dd', 'ship:ss', 'ship:ca']) expect(offer(g, 'ESP', k).why, k).toBeNull();
    for (const k of ['ship:bb', 'ship:cv']) {
      expect(offer(g, 'ESP', k).why, k).toMatch(/^Your shipyards hold no plans of (battleships|aircraft carriers): only the great powers draw them\. Take ground that belongs to one that builds them, in a war with it, or send an agent to steal its plans\.$/);
      expect(offer(g, 'ESP', k).tech, k).toBeUndefined();
      expect(orderOffer(g, 'ESP', k), k).toBeNull();
    }
    expect(whyNoPlans(g, 'ESP', 'dd')).toBeNull();
  });

  it('ships a smaller navy starts with stay', () => {
    const g = grand();
    const had = g.fleets.filter((f) => !buildsOwn(f.nat) && f.ships.bb > 0).map((f) => `${f.nat}:${f.ships.bb}`);
    const h = grand();
    expect(h.fleets.filter((f) => !buildsOwn(f.nat) && f.ships.bb > 0).map((f) => `${f.nat}:${f.ships.bb}`)).toEqual(had);
  });

  it('is the grand campaign\'s alone', () => {
    const h = createGame(3, null);
    expect(planRules(h)).toBe(false);
    expect(whyNoPlans(h, 'ESP', 'bb')).toBeNull();
  });
});

describe('plans taken with a great power\'s ground', () => {
  it('a smaller nation at war with a great power takes its plans with any ground it owns', () => {
    const g = grand();
    expect(atWar(g, 'POL', 'GER')).toBe(true);
    const r = g.byName['Grenzmark'];
    expect(r.owner).toBe('GER');
    capture(g, r, 'POL');
    expect(r.ctrl).toBe('POL');
    // Germany's battleship plans; it has no carriers to give.
    expect(locked(g, 'GER', 'ship:cv')).not.toBeNull();
    expect(plansOf(g, 'POL')).toEqual([{ what: 'bb', from: 'GER', how: 'taken', turn: g.turn }]);
    expect(plansFrom(g, 'POL', 'bb')).toBe('GER');
    expect(plansFrom(g, 'POL', 'cv')).toBeNull();
    // Taken again, nothing new.
    capture(g, g.byName['Neumark'], 'POL');
    expect(plansOf(g, 'POL')).toHaveLength(1);
    ready(g, 'POL');
    expect(onPlans(g, 'POL', 'bb')).toBe(true);
    expect(offer(g, 'POL', 'ship:bb').why).toBeNull();
    const b = orderOffer(g, 'POL', 'ship:bb');
    expect(b?.item).toBe('bb');
    // Kept through a save.
    const h = loadGame(saveGame(g))!;
    expect(plansFrom(h, 'POL', 'bb')).toBe('GER');
  });

  it('the player is told what was found', () => {
    const g = grand('POL');
    capture(g, g.byName['Grenzmark'], 'POL');
    expect(g.headlines.filter((h) => /plans/.test(h.text)).map((h) => h.text)).toEqual(['German battleship plans are found at the Grenzmark. Your shipyards can now lay down battleships on German designs.']);
  });

  it('owned, not occupied: retaking a smaller nation\'s ground gives nobody plans, nor does ground taken in peace or from a dominion', () => {
    const g = grand();
    const warsaw = g.byName['Warsaw'];
    expect(warsaw.owner).toBe('POL');
    warsaw.ctrl = 'GER';
    capture(g, warsaw, 'POL');
    expect(plansOf(g, 'POL')).toEqual([]);
    // A province of a great power changing hands in peace.
    const it = g.regions.find((r) => r.owner === 'ITA' && r.ctrl === 'ITA')!;
    expect(atWar(g, 'YUG', 'ITA')).toBe(false);
    capture(g, it, 'YUG');
    expect(plansOf(g, 'YUG')).toEqual([]);
    // A great power's own taking gives it nothing: it has its own.
    capture(g, g.regions.find((r) => r.owner === 'FRA' && r.ctrl === 'FRA')!, 'GER');
    expect(plansOf(g, 'GER')).toEqual([]);
    // Only the seven give plans.
    expect(givesPlans(g, 'CAN', 'bb')).toBe(false);
    expect(givesPlans(g, 'POL', 'bb')).toBe(false);
  });

  it('carrier plans open carriers without the country\'s own research', () => {
    const g = grand();
    ready(g, 'ESP');
    expect(locked(g, 'ESP', 'ship:cv')).not.toBeNull();
    expect(givesPlans(g, 'UK', 'cv')).toBe(true);
    expect(grantPlans(g, 'ESP', 'cv', 'UK', 'stolen')).toBe(true);
    expect(offer(g, 'ESP', 'ship:cv').why).toBeNull();
    expect(offer(g, 'ESP', 'ship:bb').why).toMatch(/great powers/);
    // A great power gains no plans; a power without carriers gives none.
    expect(grantPlans(g, 'GER', 'cv', 'UK', 'stolen')).toBe(false);
    expect(grantPlans(g, 'ESP', 'cv', 'GER', 'stolen')).toBe(false);
  });

  it('the 3D game reads each country\'s source of plans, the latest gained', () => {
    const g = grand();
    grantPlans(g, 'ESP', 'bb', 'GER', 'taken');
    grantPlans(g, 'ESP', 'bb', 'UK', 'stolen');
    const v = capitalPlans(g);
    const none = { heavybombers: null, jets: null, rockets: null, guided: null };
    expect(v.ESP).toEqual({ bb: 'UK', cv: null, ...none });
    expect(v.UK).toEqual({ bb: 'UK', cv: 'UK', heavybombers: 'UK', jets: 'UK', rockets: 'UK', guided: 'UK' });
    expect(v.CAN).toEqual({ bb: 'UK', cv: 'UK', ...none });
    expect(v.TUR).toEqual({ bb: null, cv: null, ...none });
    // A dominion's bombers are Britain's once Britain has them; a smaller country's those it took or stole.
    techOf(g, 'UK')!.done.push('heavybombers');
    techOf(g, 'GER')!.done.push('jets');
    grantPlans(g, 'ESP', 'jets', 'GER', 'stolen');
    expect(capitalPlans(g).CAN.heavybombers).toBe('UK');
    expect(capitalPlans(g).ESP).toEqual({ bb: 'UK', cv: null, ...none, jets: 'GER' });
  });
});

describe('plans stolen by an agent', () => {
  it('two missions, in a great power\'s capital or great yard, at peace as at war', () => {
    expect(MISSIONS).toContain('bbPlans');
    expect(MISSIONS).toContain('cvPlans');
    expect(MISSION_TEXT.bbPlans.label).toBe('Steal the battleship plans');
    const g = grand('ESP');
    const london = plant(g, 'London', 'watch', 'ESP');
    const clyde = plant(g, 'Clydeside', 'watch', 'ESP');
    const kent = g.regions.find((r) => r.owner === 'UK' && r.ctrl === 'UK' && !r.cap && yardLevel(g, r) < 3 && r.name !== 'London')!;
    const plain = plant(g, kent.name, 'watch', 'ESP');
    const berlin = plant(g, 'Berlin', 'watch', 'ESP');
    const warsaw = plant(g, 'Warsaw', 'watch', 'ESP');
    expect(atWar(g, 'ESP', 'UK')).toBe(false);
    expect(whyNotMission(g, 'ESP', london.id, 'bbPlans')).toBeNull();
    expect(whyNotMission(g, 'ESP', london.id, 'cvPlans')).toBeNull();
    expect(whyNotMission(g, 'ESP', clyde.id, 'bbPlans')).toBeNull();
    expect(whyNotMission(g, 'ESP', plain.id, 'bbPlans')).toBe(`There is no great naval yard ${inOrAt(kent)}: the United Kingdom keeps its plans at London and at its great naval yards.`);
    expect(whyNotMission(g, 'ESP', berlin.id, 'bbPlans')).toBeNull();
    expect(whyNotMission(g, 'ESP', berlin.id, 'cvPlans')).toBe('Germany builds no aircraft carriers yet: there are no plans to steal.');
    expect(whyNotMission(g, 'ESP', warsaw.id, 'bbPlans')).toBe('Plans of battleships are kept by the great powers, in their capitals and great naval yards. Send the agent to one.');
    // Ground a great power has taken is not where it keeps its plans.
    g.byName['Warsaw'].ctrl = 'GER';
    expect(whyNotMission(g, 'ESP', warsaw.id, 'bbPlans')).toBe('Germany keeps its plans at home, in its capital and great naval yards: Warsaw is ground it has taken.');
    // Once held, there is nothing to steal; and a great power steals nothing.
    grantPlans(g, 'ESP', 'bb', 'GER', 'taken');
    expect(whyNotMission(g, 'ESP', london.id, 'bbPlans')).toBe('Your shipyards already lay down battleships on German designs.');
    const h = grand('GER');
    const spy = plant(h, 'London', 'watch', 'GER');
    expect(whyNotMission(h, 'GER', spy.id, 'bbPlans')).toBe('Germany lays down battleships on its own designs.');
    const c = grand('CAN');
    const cs = plant(c, 'Berlin', 'watch', 'CAN');
    expect(whyNotMission(c, 'CAN', cs.id, 'bbPlans')).toBe('Your shipyards already lay down battleships on British designs.');
  });

  /** One attempt on Britain's battleship plans in each of many games, by a Spanish agent: plans got, agents lost, wars. */
  function thefts(n: number): { got: number; lost: number; wars: number; games: Game[] } {
    let got = 0, lost = 0, wars = 0;
    const games: Game[] = [];
    for (let seed = 1; seed <= n; seed++) {
      const g = grand('ESP', seed);
      const a = plant(g, 'London', 'bbPlans', 'ESP');
      intelTick(g, 'ESP');
      if (plansFrom(g, 'ESP', 'bb') === 'UK') got++;
      if (!g.intel.agents.includes(a)) lost++;
      if (atWar(g, 'UK', 'ESP')) wars++;
      games.push(g);
    }
    return { got, lost, wars, games };
  }

  it('comes off about as often as the odds say; a caught agent brings war four times in ten', () => {
    const N = 150;
    const { got, lost, wars, games } = thefts(N);
    expect(got / N).toBeGreaterThan(INTEL.missions.bbPlans.success! - 0.12);
    expect(got / N).toBeLessThan(INTEL.missions.bbPlans.success! + 0.12);
    const risk = INTEL.missions.bbPlans.caught * INTEL.risk.capital;
    expect(lost / N).toBeGreaterThan(risk - 0.12);
    expect(lost / N).toBeLessThan(risk + 0.12);
    expect(wars).toBeGreaterThan(0);
    expect(wars).toBeLessThan(lost);
    expect(wars / lost).toBeGreaterThan(INTEL.caught.war! - 0.25);
    expect(wars / lost).toBeLessThan(INTEL.caught.war! + 0.25);
    for (const g of games) {
      const a = g.intel.agents[0];
      const stole = plansFrom(g, 'ESP', 'bb') === 'UK';
      if (stole) expect(plansOf(g, 'ESP')).toEqual([{ what: 'bb', from: 'UK', how: 'stolen', turn: g.turn }]);
      if (a) { expect(a.mission).toBe('watch'); expect(a.rest).toBe(g.turn + INTEL.risk.lieLow); if (stole) expect(a.xp).toBe(INTEL.missions.bbPlans.gain); }
      expect(g.headlines.some((h) => /Agent/.test(h.text) && /plans|drawing offices/.test(h.text))).toBe(true);
      if (atWar(g, 'UK', 'ESP')) {
        expect(a).toBeUndefined();
        expect(g.headlines.some((h) => / is caught at London by British counter-intelligence\. Nothing more is heard\. The United Kingdom declares war on Spain over the attempt on its plans\.$/.test(h.text))).toBe(true);
        expect(g.flags['provoked:UK:ESP']).toBe(g.turn);
        expect(g.flags['war:UK:ESP']).toBe(g.turn);
      }
    }
    // The odds shown are the ones played.
    expect(successChance(games[0].intel.agents[0] ?? { xp: 0 } as Agent, 'bbPlans')).toBeCloseTo(INTEL.missions.bbPlans.success!, 9);
  });

  it('no war from a power already at war with the thief, nor from its ally', () => {
    let lostAtWar = 0;
    for (let seed = 1; seed <= 40; seed++) {
      const g = grand('POL', seed);
      expect(atWar(g, 'POL', 'GER')).toBe(true);
      const a = plant(g, 'Berlin', 'bbPlans', 'POL');
      const wars = g.nations.POL.atWar.length;
      intelTick(g, 'POL');
      if (!g.intel.agents.includes(a)) lostAtWar++;
      expect(g.nations.POL.atWar.length).toBe(wars);
      expect(g.flags['provoked:GER:POL']).toBeUndefined();
    }
    expect(lostAtWar).toBeGreaterThan(0);
    const g = grand('POL');
    expect(allied(g, 'POL', 'UK')).toBe(true);
    for (let k = 0; k < 30; k++) plant(g, 'London', 'bbPlans', 'POL');
    intelTick(g, 'POL');
    expect(atWar(g, 'UK', 'POL')).toBe(false);
  });
});

// Rider b542 on ruling 235 (Brian, 18 November): "heavy bombers, jets and rockets should all be treated like the
// battleships"; a smaller nation's "tech tree is more around spying efforts".
describe('rider b542: four-engined bombers, jets, rocket artillery and guided weapons, as the battleships', () => {
  it('a smaller nation cannot research the four; a great power can; its kin have them once it does', () => {
    const g = grand();
    for (const nat of ['ESP', 'CAN', 'GER']) techOf(g, nat)!.done.push(...['mediumbombers', 'monoplanes', 'radar', 'tanks', 'motor'].filter((x) => !techOf(g, nat)!.done.includes(x)));
    for (const id of PLANS.techs) {
      expect(whyNoTechPlans(g, 'GER', id), id).toBeNull();
      expect(whyNoTechPlans(g, 'ESP', id), id).toMatch(/^Only the great powers develop .+\. Take ground that belongs to one that has (them|it), in a war with it, or send an agent to steal its plans\.$/);
      expect(whyNotResearch(g, 'ESP', id), id).toMatch(/^Only the great powers develop/);
      expect(whyNoTechPlans(g, 'CAN', id), id).toMatch(/on British designs once .*United Kingdom has (them|it)\.$/);
    }
    expect(whyNoTechPlans(g, 'ESP', 'radar')).toBeNull();
    // (The UI director: rocket artillery takes "it".)
    expect(whyNoTechPlans(g, 'ESP', 'rockets')).toBe('Only the great powers develop rocket artillery. Take ground that belongs to one that has it, in a war with it, or send an agent to steal its plans.');
    expect(whyNoTechPlans(g, 'CAN', 'rockets')).toBe('Canada has rocket artillery on British designs once the United Kingdom has it.');
    expect(whyNoTechPlans(g, 'CAN', 'jets')).toBe('Canada has jet fighters on British designs once the United Kingdom has them.');
    // No staff of a smaller nation or a dominion ever picks one, in any year.
    for (const nat of ['ESP', 'CAN', 'TUR', 'SWE']) {
      const s = techOf(g, nat);
      if (!s) continue;
      for (const t of TECH.techs) if (t.branch !== 'intel' && !PLANS.techs.includes(t.id as never)) s.done.includes(t.id) || s.done.push(t.id);
      expect(staffPick(g, nat, 60), nat).toBeNull();
    }
    // Britain researches four-engined bombers: Canada has them at its next month.
    techOf(g, 'UK')!.done.push('heavybombers');
    expect(hasTech(g, 'CAN', 'heavybombers')).toBe(false);
    techTick(g, 'CAN');
    expect(hasTech(g, 'CAN', 'heavybombers')).toBe(true);
    expect(hasTech(g, 'CAN', 'jets')).toBe(false);
  });

  it('plans taken with ground or stolen put the weapon in service, as if researched', () => {
    const g = grand('POL');
    techOf(g, 'GER')!.done.push('heavybombers', 'jets');
    capture(g, g.byName['Grenzmark'], 'POL');
    expect(plansOf(g, 'POL').map((p) => p.what)).toEqual(['bb', 'heavybombers', 'jets']);
    expect(hasTech(g, 'POL', 'heavybombers')).toBe(true);
    expect(hasTech(g, 'POL', 'jets')).toBe(true);
    expect(hasTech(g, 'POL', 'rockets')).toBe(false);
    expect(g.headlines.filter((h) => /plans are found/.test(h.text)).map((h) => h.text)).toEqual([
      'German battleship, four-engined bomber and jet fighter plans are found at the Grenzmark. Your shipyards can now lay down battleships on German designs. Your air force can now fly four-engined bombers and jet fighters on German designs.',
    ]);
    // A technology a great power has not got gives no plans.
    expect(grantPlans(g, 'POL', 'rockets', 'GER', 'stolen')).toBe(false);
    // A team at work on one the plans bring in is freed.
    const h = grand();
    techOf(h, 'UK')!.done.push('rockets');
    techOf(h, 'ESP')!.work.push({ id: 'rockets', left: 5, of: 8 });
    expect(grantPlans(h, 'ESP', 'rockets', 'UK', 'stolen')).toBe(true);
    expect(techOf(h, 'ESP')!.work.some((w) => w.id === 'rockets')).toBe(false);
    expect(hasTech(h, 'ESP', 'rockets')).toBe(true);
  });

  it('one mission for each weapon, one place for each: great aircraft works for aircraft, the capital for rockets and guided weapons', () => {
    expect(PLAN_ITEMS.map((x) => Object.keys(THEFT).find((m) => THEFT[m as Mission] === x))).toEqual(['bbPlans', 'cvPlans', 'bomberPlans', 'jetPlans', 'rocketPlans', 'guidedPlans']);
    for (const m of Object.keys(THEFT) as Mission[]) expect(MISSIONS, m).toContain(m);
    expect(MISSION_TEXT.bbPlans.text).toBe('A great power\'s battleship plans are copied and got out of its capital or one of its great naval yards, and your shipyards can lay down battleships on its designs. Tried once, next month, and then the agent must lie low. If the agent is caught, a great power at peace with you, and not your ally, declares war 4 times in 10.');
    expect(MISSION_TEXT.cvPlans.text).toMatch(/^A great power's carrier plans .* your shipyards can lay down aircraft carriers on its designs\./);
    expect(MISSION_TEXT.jetPlans.label).toBe('Steal the jet fighter plans');
    expect(MISSION_TEXT.jetPlans.text).toMatch(/out of its capital or one of its great aircraft works, and your air force can fly jet fighters on its designs\./);
    expect(MISSION_TEXT.rocketPlans.text).toMatch(/out of its capital, and your army can fire rocket artillery on its designs\./);
    expect(MISSION_TEXT.guidedPlans.text).toMatch(/out of its capital, and you can launch guided weapons on its designs\./);
    const g = grand('ESP');
    techOf(g, 'GER')!.done.push('heavybombers', 'jets', 'rockets');
    const ger = (pred: (r: Game['regions'][number]) => boolean) => g.regions.find((r) => r.owner === 'GER' && r.ctrl === 'GER' && !r.cap && pred(r))!;
    const works3 = ger((r) => r.fac >= 3), works2 = ger((r) => r.fac === 2), none = ger((r) => r.fac === 0);
    const at = (r: { name: string }) => plant(g, r.name, 'watch', 'ESP');
    const berlin = at(g.byName['Berlin']), big = at(works3), mid = at(works2), bare = at(none);
    for (const m of ['bomberPlans', 'jetPlans', 'rocketPlans'] as Mission[]) expect(whyNotMission(g, 'ESP', berlin.id, m), m).toBeNull();
    expect(whyNotMission(g, 'ESP', berlin.id, 'guidedPlans')).toBe('Germany has no guided weapons yet: there are no plans to steal.');
    expect(whyNotMission(g, 'ESP', big.id, 'bomberPlans')).toBeNull();
    expect(whyNotMission(g, 'ESP', big.id, 'jetPlans')).toBeNull();
    expect(whyNotMission(g, 'ESP', mid.id, 'jetPlans')).toBeNull();
    expect(whyNotMission(g, 'ESP', mid.id, 'bomberPlans')).toBe(`There are no great aircraft works ${inOrAt(works2)}: Germany keeps its plans at Berlin and at its great aircraft works.`);
    expect(whyNotMission(g, 'ESP', bare.id, 'jetPlans')).toMatch(/^There are no great aircraft works (at|in) /);
    expect(whyNotMission(g, 'ESP', big.id, 'rocketPlans')).toBe('Germany keeps its rocket artillery plans at Berlin, its capital.');
    // A theft in Berlin comes off sooner or later, and the jets are Spain's.
    let tries = 0;
    while (!hasTech(g, 'ESP', 'jets') && tries++ < 40) {
      const a = plant(g, 'Berlin', 'jetPlans', 'ESP');
      g.intel.heat = {};
      intelTick(g, 'ESP');
      g.turn++;
      void a;
    }
    expect(hasTech(g, 'ESP', 'jets')).toBe(true);
    expect(plansFrom(g, 'ESP', 'jets')).toBe('GER');
    expect(g.headlines.some((h) => /copies the German jet fighter plans at Berlin and gets them out\. Your air force can now fly jet fighters on German designs\.$/.test(h.text))).toBe(true);
    expect(whyNotMission(g, 'ESP', big.id, 'jetPlans')).toBe('Your air force already flies jet fighters on German designs.');
  });

  it('every nation but the great powers has a spy team that works the Intelligence branch in order', () => {
    const g = grand();
    expect(TECH.spyTeams).toBe(1);
    expect(spyTeamsOf(g, 'ESP')).toBe(1);
    expect(spyTeamsOf(g, 'CAN')).toBe(1);
    expect(spyTeamsOf(g, 'GER')).toBe(0);
    expect(spyTeamsOf(createGame(3, null), 'ESP')).toBe(0);
    // A computer staff: the ordinary teams never take the branch; the spy team takes nothing else, 1 to 5.
    const order: string[] = [];
    const s = techOf(g, 'ESP')!;
    expect(s.work.filter((w) => w.spy).map((w) => w.id)).toEqual(['agents']);
    expect(s.work.filter((w) => !w.spy).every((w) => !w.id.match(/^(agents|cover|microfilm|couriers|counterintel)$/))).toBe(true);
    s.work = [];
    for (let k = 0; k < 5; k++) {
      const id = staffPick(g, 'ESP', 40, true)!;
      order.push(id);
      s.done.push(id);
    }
    expect(order).toEqual(['agents', 'cover', 'microfilm', 'couriers', 'counterintel']);
    expect(staffPick(g, 'ESP', 40, true)).toBeNull();
    // No computer great power takes it up.
    const ger = techOf(g, 'GER')!;
    expect(ger.work.every((w) => !w.spy)).toBe(true);
    for (let k = 0; k < 40; k++) { const id = staffPick(g, 'GER', 72); if (!id) break; expect(id).not.toMatch(/^(agents|cover|microfilm|couriers|counterintel)$/); ger.done.push(id); }
    // The research page counts the team.
    const v = techView(g, 'ESP')!;
    expect(v.spyTeams).toBe(1);
    expect(v.teams).toBe(teamsOf(g, 'ESP') + 1);
    expect(v.spyFree).toBe(1);
    expect(v.free).toBe(Math.max(0, teamsOf(g, 'ESP') - s.work.filter((w) => !w.spy).length));
  });

  it('a player\'s small nation puts its spy team on the branch, and its other teams too if it likes; a player\'s great power with its own teams', () => {
    const g = grand('ESP');
    const s = techOf(g, 'ESP')!;
    s.work = [];
    expect(startResearch(g, 'ESP', 'agents')).toBe(true);
    expect(s.work).toEqual([expect.objectContaining({ id: 'agents', spy: true })]);
    // The ordinary teams are all free still.
    staffTeams(g, 'ESP');
    expect(s.work.filter((w) => !w.spy)).toHaveLength(teamsOf(g, 'ESP'));
    const h = grand('UK');
    const u = techOf(h, 'UK')!;
    u.work = [];
    expect(startResearch(h, 'UK', 'agents')).toBe(true);
    expect(u.work).toEqual([expect.objectContaining({ id: 'agents' })]);
    expect(u.work[0].spy).toBeUndefined();
  });

  it('the five technologies, as data, in the secret service', () => {
    const g = grand('ESP');
    const s = techOf(g, 'ESP')!;
    const a = plant(g, 'London', 'bbPlans', 'ESP');
    const home = plant(g, 'Madrid', 'guard', 'ESP');
    const before = { caught: caughtChance(g, a), success: successChance(a, 'bbPlans', g), most: mostAgents('ESP', g), guard: guardChance(g, 'ESP', home.reg).chance };
    expect(spyOf(g, 'ESP')).toEqual({ caught: 1, rest: 0, theft: 0, agents: 0, guard: 0 });
    s.done.push('agents', 'cover', 'microfilm', 'couriers', 'counterintel');
    expect(spyOf(g, 'ESP')).toEqual({ caught: 0.85, rest: 1, theft: 0.1, agents: 1, guard: 0.1 });
    expect(caughtChance(g, a)).toBeCloseTo(before.caught * 0.85, 9);
    expect(successChance(a, 'bbPlans', g)).toBeCloseTo(before.success + 0.1, 9);
    expect(successChance(a, 'factory', g)).toBeCloseTo(successChance(a, 'factory'), 9);
    expect(mostAgents('ESP', g)).toBe(before.most + 1);
    expect(guardChance(g, 'ESP', home.reg).chance).toBeCloseTo(Math.min(INTEL.enemy.most, before.guard + 0.1), 9);
    // Cover identities: after the attempt the agent lies low a month less.
    intelTick(g, 'ESP');
    if (g.intel.agents.includes(a)) expect(a.rest).toBe(g.turn + Math.max(1, INTEL.risk.lieLow - 1));
  });
});

// Brian, 18 November: "small nations can still build all the pill boxes and defenses with their eigineers". The lock
// is on capital ships and the four technologies alone: a smaller nation's engineers and its treasury build every
// work a great power's do.
describe('fortifications are never locked', () => {
  it('a smaller nation\'s engineers and treasury build every defence a great power\'s do', () => {
    const g = grand('ESP');
    const madrid = g.byName['Madrid'];
    g.nations.ESP.pp = 5000;
    const works = offers(g, 'ESP', madrid.id).filter((o) => o.group === 'works');
    expect(works.map((o) => o.key)).toContain('work:fort');
    for (const o of works) expect(o.why ?? '', o.key).not.toMatch(/plans|great powers/);
    expect(orderOffer(g, 'ESP', 'work:fort', madrid.id), 'fortifications').not.toBeNull();
    const eng = addCorps(g, 'ESP', 'Test Sappers', 'eng', 1, madrid.id, { fresh: true });
    const jobs = jobsFor(g, eng);
    expect(jobs.map((j) => j.key)).toEqual(JOBS);
    for (const j of jobs) expect(j.why ?? '', j.key).not.toMatch(/plans|great powers/);
    expect(jobs.find((j) => j.key === 'works')!.why).toBeNull();
    expect(jobs.find((j) => j.key === 'mines')!.why).toBeNull();
    expect(orderJob(g, eng, 'works')).toBe(true);
    // No technology the plans hold back gates any order at all.
    for (const id of PLANS.techs) expect(TECH.techs.find((t) => t.id === id)!.unlock ?? [], id).toEqual([]);
  });
});
