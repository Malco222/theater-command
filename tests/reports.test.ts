// Fog on armies (the grand campaign). Seeing a foreign army's flag is not knowing what stands under it. The player
// knows a foreign corps for what it is only once it is identified: by fighting it, by an agent at work with the corps
// in view, or by a reconnaissance flight over its province. The report is exact in the month it is made. After that
// it goes stale: the kind and the divisions are still known, the strength is a band that widens month by month, the
// morale and rest are not known, and after 'keep' months the corps is unidentified again. Armies in view that are not
// identified show a count and the staff's estimate, and the card that plans an attack gives the odds as a band.
// Every band holds the truth. The computer's staffs see everything, as they always did, so a war with nobody at the
// table is the same as before. Every number is in data/campaign/intel.json under 'reports'.
import { describe, expect, it } from 'vitest';
import { allied, createGame, resolveBattle } from '../src/campaign/game';
import {
  INTEL, SIGHT, armiesSeen, armyBand, corpsKnown, divisionsBand, identify, oddsBand, reconnoitre, reportSource, sightOf, sortKnown, strengthBand,
} from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import {
  armyAt, attackOutlook, beginHuman, corpsKnownAs, endHuman, humanUp, knowledgeOf, myAirGroups, orderAttack, orderFog, orderScout, previewAttack,
  previewRaid, raidTargetsFor, runTurnAI, stepAI, visibleArmies,
} from '../src/campaign/turn';
import type { Agent, Corps, Game, Region } from '../src/campaign/types';

const R = INTEL.reports;

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
/** Puts an agent straight to work in a province, with no training and no journey. */
function plant(g: Game, at: string, nat = 'GER'): Agent {
  const a: Agent = { id: g.intel.next++, nat, name: `Test ${g.intel.next}`, xp: 0, reg: g.byName[at].id, ready: g.turn, mission: 'watch', born: g.turn };
  g.intel.agents.push(a);
  return a;
}
const polesAt = (g: Game, name: string): Corps[] => g.byName[name].units.filter((c) => c.nat === 'POL' && !c.dead);
const strengthOf = (cs: Corps[]): number => cs.reduce((s, c) => s + c.divs * c.str, 0);
/** The player's field corps that stand beside a province. */
const beside = (g: Game, r: Region, nat = 'GER'): Corps[] => g.regions.filter((x) => x.nb.includes(r.id)).flatMap((x) => x.units.filter((c) => c.nat === nat && c.kind !== 'fort' && !c.dead));

describe('the data', () => {
  it('keeps a report for some months, and no band is wider than the truth allows', () => {
    expect(R.fresh).toBeGreaterThanOrEqual(0);
    expect(R.keep).toBeGreaterThan(R.fresh);
    for (const k of [R.drift, R.widest, R.contact, R.unknown]) { expect(k).toBeGreaterThan(0); expect(k).toBeLessThan(1); }
    // A report at the end of its life is no better than no report at all.
    expect(R.drift * R.keep).toBeGreaterThanOrEqual(R.unknown);
    expect(R.widest).toBeGreaterThanOrEqual(R.unknown);
  });
});

describe('what is known when the war begins', () => {
  it('is the player\'s own army and its allies\', and nothing of anyone else\'s', () => {
    const g = playing();
    expect(Object.keys(g.intel.known ?? {})).toHaveLength(0);
    for (const c of g.corps) {
      if (c.dead) continue;
      const k = knowledgeOf(g, c);
      if (allied(g, c.nat, 'GER')) expect(k, c.name).toEqual({ level: 'exact', age: 0 });
      else { expect(k.level, c.name).toBe('none'); expect(corpsKnownAs(g, c)).toBeNull(); }
    }
    const own = g.corps.find((c) => c.nat === 'GER' && !c.dead)!;
    expect(corpsKnownAs(g, own)!.known).toBe('exact');
    expect(corpsKnownAs(g, own)!.divisions.length).toBeGreaterThan(0);
  });

  it('shows an army in view as a count and the staff\'s estimate, which holds the truth', () => {
    const g = playing();
    const sight = sightOf(g);
    let inView = 0, glimpsed = 0;
    for (const r of g.regions) {
      const cs = r.units.filter((c) => c.nat === 'POL' && !c.dead);
      if (!cs.length || sight[r.id] === SIGHT.none) continue;
      const a = armyAt(g, r.id, 'POL')!;
      expect(a.corps).toHaveLength(0);
      expect(a.corpsCount).toBe(cs.length);
      expect(a.unidentified).toBe(cs.length);
      expect(a.morale).toBeNull();
      if (sight[r.id] === SIGHT.full) {
        inView++;
        expect(a.known, r.name).toBe('partial');
        expect(a.est, r.name).not.toBeNull();
        expect(a.est!.lo).toBeLessThanOrEqual(strengthOf(cs));
        expect(a.est!.hi).toBeGreaterThanOrEqual(strengthOf(cs));
        expect(a.est!.hi - a.est!.lo).toBeCloseTo(strengthOf(cs) * R.contact, 6);
        expect(a.line).toMatch(/not identified/);
      } else {
        glimpsed++;
        expect(a.known, r.name).toBe('count');
        expect(a.est).toBeNull();
      }
    }
    expect(inView).toBeGreaterThan(3);
    expect(glimpsed).toBeGreaterThan(0);
  });

  it('gives the map the same answer: no strength for an army not identified, and a band where it is in view', () => {
    const g = playing();
    const rows = visibleArmies(g);
    for (const a of rows) {
      if (a.mine) { expect(a.known).toBe('full'); expect(a.divs).not.toBeNull(); expect(a.est).toEqual({ lo: a.divs, hi: a.divs }); continue; }
      expect(a.divs).toBeNull();
      expect(a.identified).toBe(0);
      expect(a.known).toBe(a.sight === SIGHT.full ? 'partial' : 'count');
      if (a.sight === SIGHT.full) expect(a.est!.hi).toBeGreaterThan(a.est!.lo); else expect(a.est).toBeNull();
    }
    expect(rows.some((a) => !a.mine && a.sight === SIGHT.full)).toBe(true);
    // The estimate does not flicker: it is the same every time it is asked for in a month.
    expect(visibleArmies(g)).toEqual(rows);
  });
});

describe('a battle', () => {
  it('identifies the corps that were fought, wherever they fall back to', () => {
    const g = playing();
    const poz = g.byName.Poznan;
    const defs = polesAt(g, 'Poznan');
    expect(defs.length).toBeGreaterThan(0);
    expect(orderAttack(g, beside(g, poz), poz)).not.toBeNull();
    for (const c of defs) if (!c.dead) {
      expect(knowledgeOf(g, c), c.name).toEqual({ level: 'exact', age: 0 });
      const card = corpsKnownAs(g, c)!;
      expect(card.known).toBe('exact');
      expect(card.str).toBe(c.str);
      expect(card.divisions.length).toBe(c.divs);
      expect(card.mood.morale).not.toBeNull();
    }
    // The corps of the same army that were not in the fight are no better known than before.
    expect(polesAt(g, 'Warsaw').every((c) => knowledgeOf(g, c).level === 'none')).toBe(true);
  });

  it('identifies the attackers to a player who is attacked', () => {
    const g = playing();
    const poles = polesAt(g, 'Poznan');
    const target = g.regions.find((r) => r.ctrl === 'GER' && r.nb.includes(g.byName.Poznan.id) && r.units.some((c) => c.nat === 'GER'))!;
    expect(target).toBeDefined();
    resolveBattle(g, poles, target);
    expect(poles.filter((c) => !c.dead).every((c) => knowledgeOf(g, c).level === 'exact')).toBe(true);
  });

  it('fought by an ally is shared, and one fought by anyone else is not', () => {
    const g = playing();
    const a = polesAt(g, 'Warsaw'), b = polesAt(g, 'Krakow');
    identify(g, a, 'ITA');
    expect(a.every((c) => knowledgeOf(g, c).level === 'exact')).toBe(true);
    identify(g, b, 'FRA');
    identify(g, b, null);
    expect(b.every((c) => knowledgeOf(g, c).level === 'none')).toBe(true);
    // Nothing is filed on the player's own side.
    identify(g, g.corps.filter((c) => allied(g, c.nat, 'GER')), 'GER');
    expect(Object.keys(g.intel.known!).map(Number).sort()).toEqual(a.map((c) => c.id).sort());
  });
});

describe('agents and aircraft', () => {
  it('an agent at work identifies every army in its province and around it', () => {
    const g = playing();
    plant(g, 'Lublin');
    const lublin = g.byName.Lublin;
    const cover = [lublin, ...lublin.nb.map((j) => g.regions[j])];
    const there = cover.flatMap((r) => r.units.filter((c) => !c.dead && !allied(g, c.nat, 'GER')));
    expect(there.length).toBeGreaterThan(1);
    expect(armyAt(g, lublin.id, 'POL')!.known).toBe('partial');
    reconnoitre(g);
    expect(there.every((c) => knowledgeOf(g, c).level === 'exact')).toBe(true);
    const a = armyAt(g, lublin.id, 'POL')!;
    expect(a.known).toBe('full');
    expect(a.unidentified).toBe(0);
    expect(a.corps).toHaveLength(polesAt(g, 'Lublin').length);
    expect(a.est).toEqual({ lo: strengthOf(polesAt(g, 'Lublin')), hi: strengthOf(polesAt(g, 'Lublin')) });
    // Two rings out it is still only a flag.
    const far = g.regions.find((r) => !cover.includes(r) && r.units.some((c) => c.nat === 'POL') && !r.nb.some((j) => cover.includes(g.regions[j])));
    if (far) expect(far.units.filter((c) => c.nat === 'POL').every((c) => knowledgeOf(g, c).level === 'none')).toBe(true);
  });

  it('an agent still on the way sees nothing, and the month\'s first report comes when the player\'s month opens', () => {
    const g = playing();
    const a = plant(g, 'Lublin');
    a.ready = g.turn + 1;
    reconnoitre(g);
    expect(polesAt(g, 'Lublin').every((c) => knowledgeOf(g, c).level === 'none')).toBe(true);
    nextMonth(g);
    if (g.intel.agents.includes(a) && polesAt(g, 'Lublin').length) expect(polesAt(g, 'Lublin').every((c) => knowledgeOf(g, c).level === 'exact')).toBe(true);
  });

  it('a reconnaissance flight can be sent over an army in view that is not identified, and identifies it', () => {
    const g = playing();
    const poz = g.byName.Poznan;
    expect(sightOf(g)[poz.id]).toBe(SIGHT.full);
    const t = raidTargetsFor(g, 'recon').find((x) => x.id === poz.id)!;
    expect(t).toBeDefined();
    expect(t.what).toMatch(/not identified/);
    const plan = previewRaid(g, 'recon', poz.id);
    expect(plan.why).toBeFalsy();
    expect(plan.effects[0]).toMatch(/identified/);
    const before = attackOutlook(g, beside(g, poz), poz)!;
    expect(before.exact).toBe(false);
    const res = orderScout(g, plan.picked[0], poz.id);
    expect(res.ok).toBe(true);
    expect(polesAt(g, 'Poznan').every((c) => knowledgeOf(g, c).level === 'exact')).toBe(true);
    expect(armyAt(g, poz.id, 'POL')!.known).toBe('full');
    // The provinces beside it are in the photographs too.
    const ring = poz.nb.map((j) => g.regions[j]).flatMap((r) => r.units.filter((c) => c.nat === 'POL' && !c.dead));
    expect(ring.length).toBeGreaterThan(0);
    expect(ring.every((c) => knowledgeOf(g, c).level === 'exact')).toBe(true);
    // Nothing is left there for a camera, so it is no longer offered.
    expect(raidTargetsFor(g, 'recon').some((x) => x.id === poz.id)).toBe(false);
    const after = attackOutlook(g, beside(g, poz), poz)!;
    expect(after.exact).toBe(true);
    expect(after.note).toBe('');
    expect(myAirGroups(g).find((x) => x.id === plan.picked[0])!.scouted).toBe(true);
  });
});

describe('where a report came from', () => {
  it('is kept with it: a battle, an agent, or the air', () => {
    const g = playing();
    const poz = g.byName.Poznan;
    const fought = polesAt(g, 'Poznan');
    orderAttack(g, beside(g, poz), poz);
    for (const c of fought) if (!c.dead) expect(corpsKnownAs(g, c)!.source, c.name).toBe('battle');
    plant(g, 'Lublin');
    reconnoitre(g);
    for (const c of polesAt(g, 'Lublin')) expect(corpsKnownAs(g, c)!.source).toBe('agent');
    const sil = g.byName['Polish Silesia'];
    const there = polesAt(g, 'Polish Silesia');
    expect(there.length).toBeGreaterThan(0);
    const plan = previewRaid(g, 'recon', sil.id);
    expect(orderScout(g, plan.picked[0], sil.id).ok).toBe(true);
    for (const c of there) expect(corpsKnownAs(g, c)!.source).toBe('air');
    expect(armyAt(g, sil.id, 'POL')!.corps.every((c) => c.source === 'air')).toBe(true);
    // It stays with the report as it ages, and goes when the report goes.
    g.turn += 2;
    expect(corpsKnownAs(g, there[0])).toMatchObject({ known: 'report', age: 2, source: 'air' });
    g.turn += R.keep;
    reconnoitre(g);
    expect(g.intel.how![there[0].id]).toBeUndefined();
    expect(corpsKnownAs(g, there[0])).toBeNull();
  });

  it('is nothing for the player\'s own side, and nothing where there is no fog', () => {
    const g = playing();
    const own = g.corps.find((c) => c.nat === 'GER' && !c.dead)!;
    expect(corpsKnownAs(g, own)!.source).toBeNull();
    expect(armyAt(g, own.reg, 'GER')!.corps.every((c) => c.source === null)).toBe(true);
    identify(g, polesAt(g, 'Warsaw'), 'GER', 'agent');
    expect(reportSource(g, polesAt(g, 'Warsaw')[0])).toBe('agent');
    expect(reportSource(g, polesAt(g, 'Warsaw')[0], 'POL')).toBeNull();
    expect(reportSource(g, polesAt(g, 'Krakow')[0])).toBeNull();
    orderFog(g, false);
    expect(reportSource(g, polesAt(g, 'Warsaw')[0])).toBeNull();
    expect(armyAt(g, g.byName.Warsaw.id, 'POL')!.corps.every((c) => c.source === null)).toBe(true);
  });
});

describe('a report going stale', () => {
  it('is exact in its month, a report for some months after, and then nothing', () => {
    const g = playing();
    const c = polesAt(g, 'Warsaw')[0];
    identify(g, [c], 'GER');
    const from = g.turn;
    for (let age = 0; age <= R.keep + 2; age++) {
      g.turn = from + age;
      const k = knowledgeOf(g, c);
      if (age <= R.fresh) expect(k).toEqual({ level: 'exact', age });
      else if (age <= R.keep) expect(k).toEqual({ level: 'report', age });
      else expect(k.level).toBe('none');
    }
  });

  it('still names the corps and its divisions, gives its strength as a band that widens, and no longer knows its spirit', () => {
    const g = playing();
    const c = polesAt(g, 'Warsaw')[0];
    c.str = 0.8;
    identify(g, [c], 'GER');
    const from = g.turn;
    let width = 0;
    for (let age = R.fresh + 1; age <= R.keep; age++) {
      g.turn = from + age;
      const card = corpsKnownAs(g, c)!;
      expect(card.known).toBe('report');
      expect(card.age).toBe(age);
      expect(card.name).toBe(c.name);
      expect(card.kind).toBe(c.kind);
      expect(card.divs).toBe(c.divs);
      expect(card.strLo).toBeLessThanOrEqual(c.str);
      expect(card.strHi).toBeGreaterThanOrEqual(c.str);
      expect(card.str).toBeCloseTo((card.strLo + card.strHi) / 2, 9);
      expect(card.divisions).toHaveLength(0);
      expect(card.mood.morale).toBeNull();
      expect(card.mood.moraleWord).toBe('Not known');
      expect(card.mood.restWord).toBe('Not known');
      expect(card.mood.line).toMatch(age === 1 ? /last month/ : new RegExp(`${age} months ago`));
      // The same reading all month.
      expect(corpsKnownAs(g, c)).toEqual(card);
    }
    // The band at a given moment is wider for an older report, and never wider than the rules allow.
    g.turn = from;
    for (let age = 1; age <= R.keep + 3; age++) {
      const b = strengthBand(g, c, age);
      expect(b.lo).toBeLessThanOrEqual(c.str);
      expect(b.hi).toBeGreaterThanOrEqual(c.str);
      expect(b.hi - b.lo).toBeGreaterThanOrEqual(width - 1e-9);
      expect(b.hi - b.lo).toBeLessThanOrEqual(c.str * R.widest + 1e-9);
      expect(b.hi).toBeLessThanOrEqual(1);
      width = b.hi - b.lo;
    }
    expect(strengthBand(g, c, 0)).toEqual({ lo: c.str, hi: c.str });
  });

  it('shows on the army: some corps from old reports, the rest counted, and the age of the oldest', () => {
    const g = playing();
    const cs = polesAt(g, 'Warsaw');
    expect(cs.length).toBeGreaterThan(1);
    identify(g, [cs[0]], 'GER');
    let a = armyAt(g, g.byName.Warsaw.id, 'POL')!;
    expect(a.known).toBe('partial');
    expect(a.corps.map((c) => c.id)).toEqual([cs[0].id]);
    expect(a.unidentified).toBe(cs.length - 1);
    expect(a.oldest).toBe(0);
    expect(a.line).toMatch(/identified, \d+ corps not identified/);
    expect(a.est!.lo).toBeLessThanOrEqual(strengthOf(cs));
    expect(a.est!.hi).toBeGreaterThanOrEqual(strengthOf(cs));
    // Knowing one corps for certain narrows the estimate of the whole.
    expect(a.est!.hi - a.est!.lo).toBeLessThan(strengthOf(cs) * R.contact);
    identify(g, cs, 'GER');
    expect(armyAt(g, g.byName.Warsaw.id, 'POL')!.known).toBe('full');
    g.turn += 2;
    a = armyAt(g, g.byName.Warsaw.id, 'POL')!;
    expect(a.known).toBe('partial');
    expect(a.unidentified).toBe(0);
    expect(a.oldest).toBe(2);
    expect(a.corps.every((c) => c.known === 'report' && c.age === 2)).toBe(true);
    expect(a.est!.lo).toBeLessThanOrEqual(strengthOf(cs));
    expect(a.est!.hi).toBeGreaterThanOrEqual(strengthOf(cs));
    const row = armiesSeen(g).find((x) => x.reg === g.byName.Warsaw.id && x.nat === 'POL')!;
    expect(row.known).toBe('partial');
    expect(row.identified).toBe(cs.length);
    expect(row.divs).toBeNull();
  });

  it('is thrown away when it is too old, or when the corps is no more', () => {
    const g = playing();
    const [a, b] = [polesAt(g, 'Warsaw')[0], polesAt(g, 'Krakow')[0]];
    identify(g, [a, b], 'GER');
    b.dead = true;
    reconnoitre(g);
    expect(g.intel.known![a.id]).toBe(g.turn);
    expect(g.intel.known![b.id]).toBeUndefined();
    g.turn += R.keep;
    reconnoitre(g);
    expect(g.intel.known![a.id]).toBeDefined();
    g.turn += 1;
    reconnoitre(g);
    expect(g.intel.known![a.id]).toBeUndefined();
  });

  it('is renewed by another look', () => {
    const g = playing();
    const c = polesAt(g, 'Warsaw')[0];
    identify(g, [c], 'GER');
    g.turn += 3;
    expect(knowledgeOf(g, c)).toEqual({ level: 'report', age: 3 });
    identify(g, [c], 'GER');
    expect(knowledgeOf(g, c)).toEqual({ level: 'exact', age: 0 });
  });
});

describe('the bands', () => {
  it('always hold the truth, over many armies and months', () => {
    const g = playing();
    for (let m = 0; m < 6; m++) {
      for (const r of g.regions) {
        const by = new Map<string, Corps[]>();
        for (const c of r.units) if (!c.dead && !allied(g, c.nat, 'GER')) (by.get(c.nat) ?? by.set(c.nat, []).get(c.nat)!).push(c);
        for (const [nat, cs] of by) {
          const truth = strengthOf(cs);
          const d = divisionsBand(g, r.id, nat, cs);
          expect(d.lo).toBeLessThanOrEqual(truth + 1e-9);
          expect(d.hi).toBeGreaterThanOrEqual(truth - 1e-9);
          const a = armyBand(g, r.id, nat, cs, 'GER', true)!;
          expect(a.lo).toBeLessThanOrEqual(truth + 1e-9);
          expect(a.hi).toBeGreaterThanOrEqual(truth - 1e-9);
          expect(armyBand(g, r.id, nat, cs, 'GER', false)).toBeNull();
          for (const c of cs) for (let age = 1; age <= R.keep; age++) {
            const b = strengthBand(g, c, age);
            expect(b.lo).toBeLessThanOrEqual(c.str);
            expect(b.hi).toBeGreaterThanOrEqual(c.str);
          }
        }
      }
      g.turn++;
    }
    expect(divisionsBand(g, 0, 'POL', [])).toEqual({ lo: 0, hi: 0 });
  });

  it('sort an army into what is known for certain, what is known from old reports, and what is not known', () => {
    const g = playing();
    const cs = [...polesAt(g, 'Warsaw'), ...polesAt(g, 'Krakow'), ...polesAt(g, 'Lodz')];
    expect(cs.length).toBeGreaterThanOrEqual(3);
    identify(g, [cs[0]], 'GER');
    g.turn += 1;
    identify(g, [cs[1]], 'GER');
    const k = sortKnown(g, cs, 'GER');
    expect(k.exact).toEqual([cs[1]]);
    expect(k.report).toEqual([cs[0]]);
    expect(k.none).toEqual(cs.slice(2));
    // Poland's own staff, and any staff that is not the player's, knows them all.
    expect(sortKnown(g, cs, 'POL').exact).toHaveLength(cs.length);
    expect(sortKnown(g, cs, 'FRA').exact).toHaveLength(cs.length);
  });

  it('give odds that are exact when every defender is identified, and a band round the truth when not', () => {
    const g = playing();
    const poz = g.byName.Poznan;
    const defs = polesAt(g, 'Poznan');
    const dark = oddsBand(g, defs, 2.5, poz.id, 'GER');
    expect(dark.exact).toBe(false);
    expect(dark.doubt).toBeCloseTo(R.unknown, 9);
    expect(dark.lo).toBeLessThanOrEqual(2.5);
    expect(dark.hi).toBeGreaterThanOrEqual(2.5);
    expect(dark.mid).toBeGreaterThan(dark.lo);
    expect(dark.mid).toBeLessThan(dark.hi);
    identify(g, defs, 'GER');
    expect(oddsBand(g, defs, 2.5, poz.id, 'GER')).toEqual({ exact: true, doubt: 0, lo: 2.5, hi: 2.5, mid: 2.5 });
    // An old report is better than none, and worse for every month of age.
    let last = 0;
    const from = g.turn;
    for (let age = 1; age <= R.keep; age++) {
      g.turn = from + age;
      const b = oddsBand(g, defs, 2.5, poz.id, 'GER');
      expect(b.exact).toBe(false);
      expect(b.doubt).toBeCloseTo(Math.min(R.widest, R.drift * age), 9);
      expect(b.doubt).toBeGreaterThan(last);
      expect(b.lo).toBeLessThanOrEqual(2.5);
      expect(b.hi).toBeGreaterThanOrEqual(2.5);
      last = b.doubt;
    }
    expect(last).toBeGreaterThanOrEqual(R.unknown);
    // The computer plans with the true odds.
    expect(oddsBand(g, defs, 2.5, poz.id, 'POL').exact).toBe(true);
  });
});

describe('the card that plans an attack', () => {
  it('gives a band of odds and leaves out what is not known of the defence', () => {
    const g = playing();
    const poz = g.byName.Poznan;
    const mine = beside(g, poz);
    const truth = previewAttack(g, mine, poz)!;
    const look = attackOutlook(g, mine, poz)!;
    expect(look.exact).toBe(false);
    expect(look.unidentified).toBe(truth.defenders.length);
    expect(look.stale).toBe(0);
    expect(look.lo).toBeLessThanOrEqual(truth.odds);
    expect(look.hi).toBeGreaterThanOrEqual(truth.odds);
    expect(look.hi).toBeGreaterThan(look.lo);
    expect(look.odds).toBeGreaterThan(look.lo);
    expect(look.odds).toBeLessThan(look.hi);
    expect(look.chanceLo).toBeLessThanOrEqual(look.chance);
    expect(look.chanceHi).toBeGreaterThanOrEqual(look.chance);
    expect(look.note).toMatch(/identified/);
    expect(look.note).toContain('Poznan');
    expect(look.note).toMatch(/reconnaissance flight/);
    const forces = look.lines.find((l) => l.label === 'Forces')!;
    const real = truth.lines.find((l) => l.label === 'Forces')!;
    expect(forces.value).toMatch(/none of them identified/);
    expect(forces.value.split(', against ')[0]).toBe(real.value.split(', against ')[0]);
    expect(forces.value).not.toBe(real.value);
    const morale = look.lines.find((l) => l.label === 'Morale');
    if (morale) { expect(morale.value).toMatch(/not known/); expect(morale.value).not.toMatch(/defenders (shaken|steady|confident|wavering|broken)/i); }
    // The ground, the weather and the player's own side are told as they are.
    for (const l of look.lines) if (!['Forces', 'Morale', 'Armor', 'Troops'].includes(l.label)) expect(l).toEqual(truth.lines.find((x) => x.label === l.label));
    // How good the defenders are is not known either (Version 61): the line is there, and says so.
    const troops = look.lines.find((l) => l.label === 'Troops');
    if (troops) { expect(troops.value).toBe('How good the defenders are is not known'); expect(troops.effect).toMatch(/identify them first/); }
    expect(look.lines).toHaveLength(truth.lines.length);
  });

  it('is exact once the defenders are identified, and a band again when the report is old', () => {
    const g = playing();
    const poz = g.byName.Poznan;
    const mine = beside(g, poz);
    identify(g, polesAt(g, 'Poznan'), 'GER');
    const truth = previewAttack(g, mine, poz)!;
    const look = attackOutlook(g, mine, poz)!;
    expect(look).toMatchObject({ exact: true, odds: truth.odds, lo: truth.odds, hi: truth.odds, unidentified: 0, stale: 0, note: '' });
    expect(look.lines).toEqual(truth.lines);
    g.turn += 2;
    const old = attackOutlook(g, mine, poz)!;
    const now = previewAttack(g, mine, poz)!;
    expect(old.exact).toBe(false);
    expect(old.stale).toBe(now.defenders.length);
    expect(old.unidentified).toBe(0);
    expect(old.note).toMatch(/old reports?/);
    expect(old.lo).toBeLessThanOrEqual(now.odds);
    expect(old.hi).toBeGreaterThanOrEqual(now.odds);
    expect(old.lines.find((l) => l.label === 'Forces')!.value).toMatch(/known from old reports/);
  });

  it('words the note for one corps, for two, and for a mix', () => {
    const g = playing();
    const one = g.regions.find((r) => r.units.filter((c) => c.nat === 'POL').length === 1 && r.units.length === 1 && beside(g, r).length > 0)!;
    expect(attackOutlook(g, beside(g, one), one)!.note).toMatch(/^The defending corps is not identified, so/);
    const two = g.regions.find((r) => r.units.filter((c) => c.nat === 'POL').length === 2 && r.units.length === 2 && beside(g, r).length > 0)!;
    expect(attackOutlook(g, beside(g, two), two)!.note).toMatch(/^Neither of the two defending corps is identified, so/);
    identify(g, [two.units[0]], 'GER');
    expect(attackOutlook(g, beside(g, two), two)!.note).toMatch(/^One of the 2 defending corps is not identified, so/);
    g.turn += 1;
    expect(attackOutlook(g, beside(g, two), two)!.note).toMatch(/^One of the 2 defending corps is not identified, and one is known only from an old report, so/);
  });
});

describe('where the fog does not fall', () => {
  it('lifting the fog shows every army for what it is', () => {
    const g = playing();
    expect(orderFog(g, false)).toBe(true);
    for (const c of g.corps) if (!c.dead) expect(knowledgeOf(g, c).level).toBe('exact');
    const a = armyAt(g, g.byName.Warsaw.id, 'POL')!;
    expect(a.known).toBe('full');
    expect(a.corps).toHaveLength(polesAt(g, 'Warsaw').length);
    const poz = g.byName.Poznan;
    expect(attackOutlook(g, beside(g, poz), poz)!.exact).toBe(true);
    orderFog(g, true);
    expect(armyAt(g, g.byName.Warsaw.id, 'POL')!.known).toBe('partial');
  });

  it('the historical campaigns know nothing of it', () => {
    const g = createGame(3, 'GER');
    for (const c of g.corps) if (!c.dead) expect(corpsKnown(g, c).level).toBe('exact');
    identify(g, g.corps.filter((c) => c.nat === 'POL'), 'GER');
    reconnoitre(g);
    expect(g.intel?.known).toBeUndefined();
    const a = armyAt(g, g.byName.Warsaw.id, 'POL');
    if (a) expect(a.known).toBe('full');
  });

  it('a war with nobody at the table keeps no reports', () => {
    const g = startOpen(createGame(5, null));
    for (let k = 0; k < 4; k++) runTurnAI(g);
    expect(g.intel.known).toBeUndefined();
    for (const c of g.corps.slice(0, 40)) expect(corpsKnown(g, c, 'GER').level).toBe('exact');
  });

  it('the computer\'s staffs see the player\'s army for what it is', () => {
    const g = playing();
    for (const c of g.corps) if (!c.dead && c.nat === 'GER') expect(corpsKnown(g, c, 'POL').level).toBe('exact');
  });
});

describe('through a month of play, and a save', () => {
  it('reports made in battle are a month old when the next month opens', () => {
    const g = playing();
    const poz = g.byName.Poznan;
    const defs = polesAt(g, 'Poznan');
    orderAttack(g, beside(g, poz), poz);
    const fought = defs.filter((c) => !c.dead);
    expect(fought.length).toBeGreaterThan(0);
    nextMonth(g);
    for (const c of fought) {
      if (c.dead) { expect(g.intel.known![c.id]).toBeUndefined(); continue; }
      const k = knowledgeOf(g, c);
      // Fought again in the computer's months, it is known afresh; left alone, the report is a month old.
      expect(['exact', 'report']).toContain(k.level);
      if (k.level === 'report') { expect(k.age).toBe(1); expect(corpsKnownAs(g, c)!.mood.morale).toBeNull(); }
    }
  });

  it('are kept in a saved game', () => {
    const g = playing();
    const cs = polesAt(g, 'Warsaw');
    identify(g, cs, 'GER');
    const back = loadGame(saveGame(g))!;
    expect(back.intel.known).toEqual(g.intel.known);
    expect(back.intel.how).toEqual(g.intel.how);
    expect(cs.every((c) => knowledgeOf(back, back.corps.find((x) => x.id === c.id)!).level === 'exact')).toBe(true);
    expect(armyAt(back, back.byName.Warsaw.id, 'POL')).toEqual(armyAt(g, g.byName.Warsaw.id, 'POL'));
  });
});
