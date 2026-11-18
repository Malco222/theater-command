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
import { allied, atWar, capture, createGame } from '../src/campaign/game';
import { INTEL, MISSIONS, MISSION_TEXT, intelTick, successChance, whyNotMission } from '../src/campaign/intel';
import { PLANS, buildsOwn, givesPlans, grantPlans, onPlans, planRules, plansFrom, plansOf, whyNoPlans } from '../src/campaign/plans';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { locked } from '../src/campaign/tech';
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
      expect(offer(g, 'ESP', k).why, k).toMatch(/built only by the great powers/);
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
    expect(plansOf(g, 'POL')).toEqual([{ hull: 'bb', from: 'GER', how: 'taken', turn: g.turn }]);
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
    expect(v.ESP).toEqual({ bb: 'UK', cv: null });
    expect(v.UK).toEqual({ bb: 'UK', cv: 'UK' });
    expect(v.CAN).toEqual({ bb: 'UK', cv: 'UK' });
    expect(v.TUR).toEqual({ bb: null, cv: null });
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
    expect(whyNotMission(g, 'ESP', plain.id, 'bbPlans')).toMatch(/no great naval yard/);
    expect(whyNotMission(g, 'ESP', berlin.id, 'bbPlans')).toBeNull();
    expect(whyNotMission(g, 'ESP', berlin.id, 'cvPlans')).toMatch(/builds no carriers yet/);
    expect(whyNotMission(g, 'ESP', warsaw.id, 'bbPlans')).toMatch(/kept by the great powers/);
    // Once held, there is nothing to steal; and a great power steals nothing.
    grantPlans(g, 'ESP', 'bb', 'GER', 'taken');
    expect(whyNotMission(g, 'ESP', london.id, 'bbPlans')).toMatch(/already hold/);
    const h = grand('GER');
    const spy = plant(h, 'London', 'watch', 'GER');
    expect(whyNotMission(h, 'GER', spy.id, 'bbPlans')).toMatch(/own designs/);
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
      if (stole) expect(plansOf(g, 'ESP')).toEqual([{ hull: 'bb', from: 'UK', how: 'stolen', turn: g.turn }]);
      if (a) { expect(a.mission).toBe('watch'); expect(a.rest).toBe(g.turn + INTEL.risk.lieLow); if (stole) expect(a.xp).toBe(INTEL.missions.bbPlans.gain); }
      expect(g.headlines.some((h) => /Agent/.test(h.text) && /plans|drawing offices/.test(h.text))).toBe(true);
      if (atWar(g, 'UK', 'ESP')) {
        expect(a).toBeUndefined();
        expect(g.headlines.some((h) => /declares war on Spain for the theft of its plans/.test(h.text))).toBe(true);
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
