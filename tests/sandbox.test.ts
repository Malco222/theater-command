// The sandbox's what-if planes and tanks (classes.ts, data/campaign/classes.json; Brian, 19 November 2026: "i want
// this to be a sandbox to play and make your own history, not just play out the real one"). Machines history never
// built are technologies on the research tree; once researched they are ordered from the arsenal like any corps or
// wing, at their own price and months, and with a steel bill drawn from the supplies. All of it behind one switch,
// g.flags.sandbox, in the grand campaign only: off, the game is exactly what it was.
import { describe, expect, it } from 'vitest';
import { airRange, assessAir } from '../src/campaign/air';
import { offers, orderOffer } from '../src/campaign/arsenal';
import {
  CLASSES, classById, classCount, classRule, classesFor, pickClass, sandbox, steelMonth, steelOnBooks, steelShare, unlockWords, whyNoClass,
} from '../src/campaign/classes';
import { cancelBuild, corpsCost, deliver, orderCorps, orderPara } from '../src/campaign/economy';
import { addCorps, createGame, worksFor } from '../src/campaign/game';
import { setStop, whatifShare } from '../src/campaign/history';
import { needOf, storesOf } from '../src/campaign/resources';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { TECH, grantTech, hasTech, staffPick, startResearch, techById, techOf, techRule, techTick, techView, whyNotResearch } from '../src/campaign/tech';
import { traitAttack, traitDefence, traitNames } from '../src/campaign/traits';
import { runTurnAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';

const grand = (human: string | null = null, seed = 3): Game => startOpen(createGame(seed, human));
const sand = (human: string | null = null, seed = 3): Game => { const g = grand(human, seed); g.flags.sandbox = 1; return g; };
const WHATIF = TECH.techs.filter((t) => t.whatif);
const PLANES = ['escortftr', 'nightftr', 'tankbuster', 'gunship', 'seahunter'];
const TANKS = ['breaker', 'flametank', 'amphtank', 'snowtank', 'tankhunter', 'glidertank'];

/** Gives a nation the technology that opens a class, and what it needs before it. */
function open(g: Game, nat: string, cls: string): void {
  const t = WHATIF.find((x) => x.unlock?.includes(`class:${cls}`))!;
  const all = (id: string): void => { for (const x of techById(id)!.needs) all(x); grantTech(g, nat, id); };
  all(t.id);
  expect(hasTech(g, nat, t.id), t.id).toBe(true);
}

describe('the data', () => {
  it('has eleven original machines, each opened by one what-if technology of the tree', () => {
    expect(CLASSES.classes.map((r) => r.id).sort()).toEqual([...PLANES, ...TANKS].sort());
    expect(WHATIF).toHaveLength(11);
    const ids = new Set(CLASSES.classes.map((r) => r.id));
    expect(ids.size).toBe(CLASSES.classes.length);
    for (const r of CLASSES.classes) {
      expect(r.whatif, r.id).toBe(true);
      const t = techById(r.tech!)!;
      expect(t, r.id).toBeDefined();
      expect(t.whatif, r.id).toBe(true);
      expect(t.unlock, r.id).toEqual([`class:${r.id}`]);
      expect(['arm', 'mot', 'para', 'ftr', 'bmr'], r.id).toContain(r.of);
      expect(r.label.length, r.id).toBeGreaterThan(5);
      expect(r.blurb!.length, r.id).toBeGreaterThan(40);
      // The words a player reads carry no numbers of their own: those come from the rule.
      expect(r.blurb, r.id).not.toMatch(/\d%/);
      expect(r.model, r.id).toMatch(r.of === 'ftr' || r.of === 'bmr' ? /^air\/WhatIf_[A-Za-z]+$/ : /^land\/WhatIf_[A-Za-z]+$/);
      if (r.of !== 'ftr' && r.of !== 'bmr') expect(r.word, r.id).toMatch(/^[A-Z][a-z]+( [A-Z][a-z]+)*$/);
      expect(classRule(r).length, r.id).toBeGreaterThan(20);
      // Each has a price and a time of its own, and every number of its rule is sane.
      expect(r.cost, r.id).toBeGreaterThan(0);
      expect(r.months, r.id).toBeGreaterThanOrEqual(3);
      for (const e of r.edges ?? []) for (const m of [e.attack, e.defence]) if (m !== undefined) { expect(m, r.id).toBeGreaterThanOrEqual(0.8); expect(m, r.id).toBeLessThanOrEqual(1.6); }
      for (const k of ['fight', 'reach', 'close', 'bomb', 'sea', 'oil'] as const) if (r[k] !== undefined) { expect(r[k], `${r.id} ${k}`).toBeGreaterThan(0); expect(r[k], `${r.id} ${k}`).toBeLessThanOrEqual(2.5); }
    }
    // A what-if technology comes after the real ones it builds on, and never before 1941.
    for (const t of WHATIF) {
      expect(t.year, t.id).toBeGreaterThanOrEqual(1941);
      for (const n of t.needs) expect(techById(n)!.whatif, `${t.id} needs ${n}`).toBeFalsy();
    }
  });

  it('says what each machine does in plain words worked out from its numbers', () => {
    expect(classRule(classById(sand(), 'GER', 'flametank')!)).toBe('Attacks 60% harder in towns and cities. Attacks 30% harder in woods. Attacks 25% harder against fortified ground. Defends 15% weaker. Burns 20% more fuel than its kind.');
    expect(classRule(classById(sand(), 'GER', 'nightftr')!)).toBe('Each aircraft is worth 15% less in a fight in the air. By night 70% of its fighters find the raid. Burns 40% more fuel than its kind.');
    expect(unlockWords('class:breaker')).toMatch(/^Breakthrough tank corps can be ordered from the Arsenal\. Attacks 40% harder against fortified ground\./);
    expect(techRule(techById('nightfighters')!)).toMatch(/^Night fighter wings can be ordered from the Arsenal\./);
  });
});

describe('with the sandbox off', () => {
  it('is the game as it was: no what-if technology, no class to order, no steel bill', () => {
    const g = grand('GER');
    expect(sandbox(g)).toBe(false);
    expect(whatifShare(g)).toBe(0);
    const v = techView(g, 'GER')!;
    expect(v.nodes.some((n) => techById(n.id)!.whatif)).toBe(false);
    expect(v.total).toBe(TECH.techs.length - WHATIF.length);
    for (const t of WHATIF) expect(whyNotResearch(g, 'GER', t.id), t.id).toBe('There is no such technology.');
    expect(startResearch(g, 'GER', 'escortfighters')).toBe(false);
    // Even a nation that somehow has the technology is offered nothing.
    open(g, 'GER', 'flametank');
    expect(classesFor(g, 'GER', 'arm')).toEqual([]);
    expect(offers(g, 'GER').some((o) => o.cls)).toBe(false);
    expect(pickClass(g, 'GER', 'arm')).toBeNull();
    const cap = g.nations.GER.capital;
    g.nations.GER.pp = 500;
    expect(orderOffer(g, 'GER', 'corps:arm:flametank', cap)).toBeNull();
    expect(orderCorps(g, 'GER', 'arm', cap, undefined, 'flametank')).toBeNull();
    const b = orderOffer(g, 'GER', 'corps:arm', cap)!;
    expect(b.steel).toBeUndefined();
    expect(b.cls).toBeUndefined();
    expect(steelMonth(g, 'GER')).toEqual([]);
  });

  it('keeps the computer staffs on history\'s machines through a year of war', () => {
    const g = grand(null, 5);
    for (const t of WHATIF) for (const nat of ['GER', 'UK', 'FRA']) for (const x of t.needs) grantTech(g, nat, x);
    while (g.turn <= 14) runTurnAI(g);
    for (const n of Object.values(g.nations)) {
      expect(n.tech?.done.some((id) => techById(id)?.whatif) ?? false, n.id).toBe(false);
      expect(n.tech?.work.some((w) => techById(w.id)?.whatif) ?? false, n.id).toBe(false);
      expect(n.queue.some((b) => b.cls || b.steel), n.id).toBe(false);
    }
    expect(g.corps.some((c) => c.cls)).toBe(false);
    expect(g.airFleets.some((f) => f.cls)).toBe(false);
  });
});

describe('with the sandbox on', () => {
  it('puts the what-if technologies on the tree, in their branches', () => {
    const g = sand('GER');
    const v = techView(g, 'GER')!;
    expect(v.total).toBe(TECH.techs.length);
    const land = v.nodes.filter((n) => n.branch === 'land' && techById(n.id)!.whatif).map((n) => n.id);
    const air = v.nodes.filter((n) => n.branch === 'air' && techById(n.id)!.whatif).map((n) => n.id);
    expect(land.sort()).toEqual(['amphibioustanks', 'breakthroughtanks', 'flametanks', 'snowtanks', 'tankhunters']);
    expect(air.sort()).toEqual(['assaultgliders', 'escortfighters', 'gunships', 'nightfighters', 'seahunters', 'tankbusters']);
    // Germany begins the war with the monoplanes, so the escort fighter is open to a team at once; the night
    // fighter waits on the radar warning chain.
    expect(whyNotResearch(g, 'GER', 'escortfighters')).toBeNull();
    expect(whyNotResearch(g, 'GER', 'nightfighters')).toMatch(/^Needs .*first\.$/);
  });

  it('lists a class in the arsenal before it is researched, saying what it waits on, and opens it once it is', () => {
    const g = sand('GER');
    const cap = g.nations.GER.capital;
    g.nations.GER.pp = 500;
    const before = offers(g, 'GER', cap).find((o) => o.key === 'air:ftr:escortftr')!;
    expect(before).toMatchObject({ group: 'air', cls: 'escortftr', model: 'air/WhatIf_EscortFighter', tech: 'escortfighters', why: 'Needs the technology Escort fighters. Open Research to begin it.' });
    expect(orderOffer(g, 'GER', 'air:ftr:escortftr', cap)).toBeNull();
    expect(startResearch(g, 'GER', 'escortfighters')).toBe(true);
    const months = techOf(g, 'GER')!.work.find((w) => w.id === 'escortfighters')!.left;
    for (let k = 0; k < months; k++) techTick(g, 'GER');
    expect(hasTech(g, 'GER', 'escortfighters')).toBe(true);
    const now = offers(g, 'GER', cap).find((o) => o.key === 'air:ftr:escortftr')!;
    expect(now.why).toBeNull();
    expect(now.tech).toBeUndefined();
    expect(now.cost).toBe(16);
    expect(now.months).toBe(4);
    expect(now.size).toBe('100 fighters');
  });

  it('builds a tank corps of a class at its own price and months, with its name and a steel bill', () => {
    const g = sand('GER');
    open(g, 'GER', 'flametank');
    const n = g.nations.GER, cap = n.capital;
    n.pp = 500;
    const row = classById(g, 'GER', 'flametank')!;
    const o = offers(g, 'GER', cap).find((x) => x.key === 'corps:arm:flametank')!;
    expect(o.why).toBeNull();
    expect(o.cost).toBe(corpsCost(g, 'GER', 'arm', row));
    const no = n.corpsNo;
    const b = orderOffer(g, 'GER', 'corps:arm:flametank', cap)!;
    expect(b).toMatchObject({ what: 'corps', kind: 'arm', cls: 'flametank', eta: g.turn + 8, cost: o.cost });
    expect(b.name).toMatch(/ Flame Tank Corps$/);
    expect(n.corpsNo).toBe(no + 1);
    // The steel: its column's share of the money cost, spread evenly over its months.
    expect(b.steel).toBeCloseTo((steelShare('land') * b.cost) / 8, 2);
    // Taken back in the month it was ordered, the number is free again.
    expect(cancelBuild(g, 'GER', b.id)).toBe(true);
    expect(n.corpsNo).toBe(no);
    const again = orderOffer(g, 'GER', 'corps:arm:flametank', cap)!;
    g.turn = again.eta;
    deliver(g, 'GER');
    const c = g.corps.find((x) => x.name === again.name)!;
    expect(c).toMatchObject({ kind: 'arm', cls: 'flametank' });
    // It counts as a tank corps everywhere a tank corps is counted, and it is still one after a save and a load.
    expect(classCount(g, 'GER', 'flametank')).toBe(1);
    const back = loadGame(saveGame(g))!;
    expect(back.corps.find((x) => x.id === c.id)!.cls).toBe('flametank');
  });

  it('keeps a class the nation can hold only so many of to that number', () => {
    const g = sand('GER');
    open(g, 'GER', 'breaker');
    const n = g.nations.GER, cap = n.capital;
    n.pp = 2000;
    for (let k = 0; k < 3; k++) expect(orderOffer(g, 'GER', 'corps:arm:breaker', cap), `order ${k + 1}`).not.toBeNull();
    const row = classById(g, 'GER', 'breaker')!;
    expect(whyNoClass(g, 'GER', row)).toBe('The nation can keep 3 of these at once, in the field and on order.');
    expect(offers(g, 'GER', cap).find((o) => o.key === 'corps:arm:breaker')!.why).toBe(whyNoClass(g, 'GER', row));
    expect(orderOffer(g, 'GER', 'corps:arm:breaker', cap)).toBeNull();
    // A class of one kind is never ordered as another.
    expect(orderCorps(g, 'GER', 'mot', cap, undefined, 'flametank')).toBeNull();
  });

  it('gives a class corps its edges in battle, by the ground it fights on, and names it on the battle ticket', () => {
    const g = sand('GER');
    const plain = (cs: string): Corps => {
      const r = g.regions.find((x) => x.ctrl === 'GER' && x.units.length < 4)!;
      const c = addCorps(g, 'GER', `Test ${cs}`, 'arm', 3, r.id, {});
      if (cs) c.cls = cs;
      return c;
    };
    const tank = plain(''), flame = plain('flametank'), breaker = plain('breaker');
    const town = g.regions.find((r) => r.terrain === 'urban' && r.ctrl !== 'GER' && !worksFor(g, r, 'GER'))!;
    const open_ = g.regions.find((r) => r.terrain === 'plains' && r.ctrl !== 'GER' && !worksFor(g, r, 'GER'))!;
    expect(traitAttack(g, flame, town) / traitAttack(g, tank, town)).toBeCloseTo(1.6, 9);
    expect(traitAttack(g, flame, open_) / traitAttack(g, tank, open_)).toBeCloseTo(1, 9);
    expect(traitDefence(g, flame, open_) / traitDefence(g, tank, open_)).toBeCloseTo(0.85, 9);
    expect(traitAttack(g, breaker, open_) / traitAttack(g, tank, open_)).toBeCloseTo(1.1, 9);
    expect(traitDefence(g, breaker, open_) / traitDefence(g, tank, open_)).toBeCloseTo(1.2, 9);
    expect(traitNames(g, [flame], town, 'attack')).toContain('Flame tank corps');
    // An amphibious tank corps comes into its own in a landing from the sea.
    const amph = plain('amphtank');
    expect(traitAttack(g, amph, open_, true) / traitAttack(g, tank, open_, true)).toBeCloseTo(1.4, 9);
    expect(traitAttack(g, amph, open_, false) / traitAttack(g, tank, open_, false)).toBeCloseTo(1, 9);
  });

  it('makes a heavy class burn more fuel than its kind', () => {
    const g = sand('GER');
    const r = g.regions.find((x) => x.ctrl === 'GER' && x.units.length < 4)!;
    const before = needOf(g, 'GER', 'oil', true);
    const c = addCorps(g, 'GER', 'Test', 'arm', 3, r.id, {});
    const plainCost = needOf(g, 'GER', 'oil', true) - before;
    c.cls = 'breaker';
    expect(needOf(g, 'GER', 'oil', true) - before).toBeCloseTo(plainCost * 1.6, 6);
  });

  it('raises a glider tank corps as an airborne corps of its own', () => {
    const g = sand('GER');
    open(g, 'GER', 'glidertank');
    const n = g.nations.GER, cap = n.capital;
    n.pp = 500;
    const o = offers(g, 'GER', cap).find((x) => x.key === 'para:glidertank')!;
    expect(o).toMatchObject({ why: null, cls: 'glidertank', kind: 'inf' });
    const b = orderOffer(g, 'GER', 'para:glidertank', cap)!;
    expect(b).toMatchObject({ what: 'corps', kind: 'inf', airborne: true, cls: 'glidertank', name: 'I Glider Tank Corps', cost: o.cost });
    expect(orderPara(g, 'GER', cap, 'breaker')).toBeNull();
    g.turn = b.eta;
    deliver(g, 'GER');
    expect(g.corps.find((c) => c.name === 'I Glider Tank Corps')).toMatchObject({ airborne: true, cls: 'glidertank' });
  });

  it('flies a wing of a class as an air group of its own, named for it', () => {
    const g = sand('GER');
    open(g, 'GER', 'nightftr');
    const n = g.nations.GER, cap = n.capital;
    n.pp = 500;
    const b = orderOffer(g, 'GER', 'air:ftr:nightftr', cap)!;
    expect(b).toMatchObject({ what: 'air', item: 'ftr', cls: 'nightftr', count: 100 });
    expect(b.steel).toBeCloseTo((steelShare('air') * b.cost) / 4, 2);
    const plain = orderOffer(g, 'GER', 'air:ftr', cap)!;
    g.turn = b.eta;
    deliver(g, 'GER');
    const grp = g.airFleets.find((f) => f.nat === 'GER' && f.cls === 'nightftr')!;
    expect(grp).toBeDefined();
    expect(grp.name).toBe('1st Night Fighter Group');
    expect(grp.kind).toBe('ftr');
    expect(grp.ftr).toBe(100);
    // The plain wing joined a group of no class, never the night fighters.
    expect(plain.cls).toBeUndefined();
    expect(grp.ftr).toBe(100);
    // A second wing of the class joins the first.
    const again = orderOffer(g, 'GER', 'air:ftr:nightftr', grp.base)!;
    g.turn = again.eta;
    deliver(g, 'GER');
    expect(g.airFleets.filter((f) => f.nat === 'GER' && f.cls === 'nightftr')).toHaveLength(1);
    expect(grp.ftr).toBe(200);
  });

  it('gives an air group of a class its reach, its escort and its weight of bombs', () => {
    const g = sand();
    const warsaw = g.byName['Warsaw'];
    for (const x of g.airFleets) if (x.nat === 'POL') x.ftr = 0;
    const f = g.airFleets.filter((x) => x.nat === 'GER' && x.bmr > 0).sort((a, b) => assessAir(g, [b], 'bomb', warsaw.id).B - assessAir(g, [a], 'bomb', warsaw.id).B)[0];
    const reach = airRange(f, g);
    const bare = assessAir(g, [f], 'bomb', warsaw.id);
    expect(bare.why).toBeNull();
    expect(bare.load).toBeUndefined();
    f.cls = 'seahunter';
    expect(airRange(f, g).bmr).toBeCloseTo(reach.bmr * 1.6, 9);
    expect(assessAir(g, [f], 'bomb', warsaw.id).load).toBeCloseTo(0.4, 9);
    f.cls = 'gunship';
    const gun = assessAir(g, [f], 'bomb', warsaw.id);
    expect(gun.load).toBeCloseTo(0.1, 9);
    expect(gun.Epow).toBeGreaterThan(bare.Epow);
    expect(gun.B).toBe(bare.B);
  });

  it('lets a night fighter class find a raid by night that other fighters never see', () => {
    const g = sand();
    const london = g.byName['London'];
    const raiders = g.airFleets.filter((x) => x.nat === 'GER' && x.bmr > 0);
    const uk = g.airFleets.filter((x) => x.nat === 'UK' && x.ftr > 0);
    // (At war, for the defenders to come up.)
    if (!g.nations.GER.atWar.includes('UK')) { g.nations.GER.atWar.push('UK'); g.nations.UK.atWar.push('GER'); }
    const before = assessAir(g, raiders, 'night', london.id);
    expect(before.why).toBeNull();
    expect(before.def.length).toBeGreaterThan(0);
    const D = before.Dpow;
    for (const x of uk) x.cls = 'nightftr';
    expect(assessAir(g, raiders, 'night', london.id).Dpow).toBeGreaterThan(D * 5);
  });
});

describe('the steel bill', () => {
  it('draws each order\'s steel from the supplies, and makes every order slip a month the depots cannot cover', () => {
    const g = sand('GER');
    const n = g.nations.GER, cap = n.capital, st = storesOf(g, 'GER')!;
    n.pp = 500; n.queue = [];
    const b = orderOffer(g, 'GER', 'corps:inf', cap)!;
    const w = orderOffer(g, 'GER', 'air:ftr', cap)!;
    expect(b.steel).toBeGreaterThan(0);
    expect(w.steel).toBeGreaterThan(0);
    // Nothing is drawn in the month an order is placed.
    expect(steelOnBooks(g, 'GER')).toBe(0);
    g.turn++;
    const bill = steelOnBooks(g, 'GER');
    expect(bill).toBeCloseTo(b.steel! + w.steel!, 6);
    st.sup = 1000;
    expect(steelMonth(g, 'GER')).toEqual([]);
    expect(st.sup).toBeCloseTo(1000 - bill, 6);
    // A month the depots are short: nothing is drawn and both orders slip.
    const eta = [b.eta, w.eta];
    st.sup = bill / 2;
    expect(steelMonth(g, 'GER').map((x) => x.id).sort()).toEqual([b.id, w.id].sort());
    expect([b.eta, w.eta]).toEqual([eta[0] + 1, eta[1] + 1]);
    expect(st.sup).toBeCloseTo(bill / 2, 6);
    expect(g.headlines.some((h) => /^Short of steel/.test(h.text))).toBe(true);
  });

  it('is paid in money by a country that keeps no stocks', () => {
    const g = sand();
    const nat = Object.keys(g.nations).find((k) => { const n = g.nations[k]; if (!n.alive || n.capital < 0 || storesOf(g, k)) return false; n.pp = 300; return offers(g, k).some((o) => o.key === 'corps:inf' && o.why === null); })!;
    expect(nat).toBeDefined();
    const n = g.nations[nat];
    n.queue = [];
    const b = orderOffer(g, nat, 'corps:inf', n.capital)!;
    g.turn++;
    const pp = n.pp;
    expect(steelMonth(g, nat)).toEqual([]);
    expect(n.pp).toBeCloseTo(pp - b.steel!, 6);
  });

  it('comes to about a tenth of a great power\'s monthly supplies at its usual pace of building', () => {
    const g = sand(null, 5);
    while (g.turn <= 12) runTurnAI(g);
    for (const nat of ['GER', 'UK', 'SOV', 'USA']) {
      if (!g.nations[nat]?.alive) continue;
      const share = steelOnBooks(g, nat) / Math.max(1, needOf(g, nat, 'sup', true));
      expect(share, nat).toBeLessThan(0.35);
    }
  });
});

describe('the history dial', () => {
  it('lets the computer staffs take no what-if class at the stops nearest history, and every one at the loosest', () => {
    const g = sand();
    for (const c of ['flametank', 'breaker', 'nightftr', 'gunship']) open(g, 'GER', c);
    for (const stop of [1, 2]) {
      setStop(g, stop);
      expect(whatifShare(g), `stop ${stop}`).toBe(0);
      for (let k = 0; k < 40; k++) expect(pickClass(g, 'GER', 'arm', k), `stop ${stop}`).toBeNull();
    }
    setStop(g, 3);
    expect(whatifShare(g)).toBe(0.1);
    setStop(g, 5);
    expect(whatifShare(g)).toBe(1);
    const got = new Set<string>();
    for (let k = 0; k < 40; k++) got.add(pickClass(g, 'GER', 'arm', k)!.id);
    expect([...got].sort()).toEqual(['breaker', 'flametank']);
    expect(pickClass(g, 'GER', 'ftr', 1)!.id).toBe('nightftr');
    // A class the nation has not researched is never drawn.
    expect(pickClass(g, 'UK', 'arm', 1)).toBeNull();
  });

  it('keeps a computer staff\'s research off the what-if branch where the dial gives it no use', () => {
    const g = sand();
    setStop(g, 1);
    const s = techOf(g, 'GER')!;
    // Everything else done but one ordinary technology of the top step, years ahead of its time: the staff takes that,
    // and never a what-if one it would build nothing from.
    for (const t of TECH.techs) if (!t.whatif && t.id !== 'jets' && t.branch !== 'intel') grantTech(g, 'GER', t.id);
    s.work = [];
    const left = TECH.techs.filter((t) => !t.whatif && !s.done.includes(t.id) && t.branch !== 'intel');
    expect(left.map((t) => t.id)).toEqual(['jets']);
    expect(staffPick(g, 'GER')).toBe('jets');
    grantTech(g, 'GER', 'jets');
    expect(staffPick(g, 'GER')).toBeNull();
    // At the stop that lets the staffs build some what-if classes, the what-if technologies come late, after the real ones of their step;
    // at the loosest they are weighed like any other.
    s.done = s.done.filter((id) => id !== 'jets');
    setStop(g, 3);
    expect(staffPick(g, 'GER', 60)).toBe('jets');
    setStop(g, 5);
    expect(techById(staffPick(g, 'GER')!)!.whatif).toBe(true);
  });

  it('has the computer build what-if tanks and wings at the loosest stop once it has researched them', () => {
    const g = sand(null, 5);
    setStop(g, 5, true);
    for (const c of ['flametank', 'breaker', 'snowtank', 'escortftr', 'nightftr', 'gunship']) open(g, 'GER', c);
    while (g.turn <= 14) runTurnAI(g);
    const tanks = g.corps.filter((c) => c.nat === 'GER' && c.cls).length + g.nations.GER.queue.filter((b) => b.what === 'corps' && b.cls).length;
    const wings = g.airFleets.filter((f) => f.nat === 'GER' && f.cls).length + g.nations.GER.queue.filter((b) => b.what === 'air' && b.cls).length;
    expect(tanks + wings).toBeGreaterThan(0);
  });
});
