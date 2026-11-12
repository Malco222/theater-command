// The grand campaign's ground rules: every arm has ground that suits it and ground that does not, the steepest
// mountains are closed to tanks altogether, and tanks and infantry are worth more together than apart.
// The historical scenarios use none of it.
import { describe, expect, it } from 'vitest';
import ground from '../data/campaign/ground.json';
import { canRaiseKind } from '../src/campaign/economy';
import { addCorps, assess, createGame, limitAttackers, moveCorps, moveCost, railMove, railReach } from '../src/campaign/game';
import { GROUND, barred, combinedArms, groundAttack, groundDefence, groundEffects, groundMarch, groundNotes, groundRules, groundSays, steep, whyBarred } from '../src/campaign/ground';
import { whyNotShip } from '../src/campaign/navy';
import { startOpen } from '../src/campaign/staff';
import { reach, runTurnAI } from '../src/campaign/turn';
import { KIND, TERRAIN_DEF, type Corps, type Game, type Kind, type Region, type Terrain } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = null): Game => startOpen(createGame(seed, human));
/** A Polish province with Polish troops in it, next to German home ground. */
const front = (g: Game): { target: Region; from: Region } => {
  const target = g.regions.find((r) => r.ctrl === 'POL' && !r.fort && r.units.some((c) => c.nat === 'POL') && r.nb.some((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER'))!;
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER')!];
  return { target, from };
};
const corps = (g: Game, nat: string, kind: Kind, at: Region, divs = 3): Corps => addCorps(g, nat, `Test ${KIND[kind].label} Corps ${g.nextCorps}`, kind, divs, at.id, {});
/** The same battle on another kind of ground. */
const on = <T>(r: Region, terrain: Terrain, run: () => T): T => { const was = r.terrain; r.terrain = terrain; try { return run(); } finally { r.terrain = was; } };
/** Empties a region of troops and returns it. */
const clear = (r: Region): Region => { for (const c of r.units) c.dead = true; r.units = []; return r; };

describe('the ground data', () => {
  it('names real mountain regions as steep, none of them a capital or a dead end', () => {
    const g = grand();
    expect(ground.steep.regions.length).toBeGreaterThanOrEqual(6);
    for (const name of ground.steep.regions) {
      const r = g.byName[name];
      expect(r, name).toBeDefined();
      expect(r.terrain, name).toBe('mountain');
      expect(r.cap, name).toBe(false);
      expect(steep(g, r), name).toBe(true);
      // There is always a way round for the tanks: a neighbour that is not steep.
      expect(r.nb.some((j) => !steep(g, g.regions[j])), name).toBe(true);
    }
    // Most mountain country is hard going, not closed.
    const mountains = g.regions.filter((r) => r.terrain === 'mountain');
    expect(mountains.filter((r) => steep(g, r)).length).toBeLessThan(mountains.length / 3);
  });

  it('keeps every multiplier within reason, and says something about every kind of ground', () => {
    for (const table of [ground.attack, ground.defence]) for (const row of Object.values(table)) for (const m of Object.values(row)) { expect(m).toBeGreaterThanOrEqual(0.5); expect(m).toBeLessThanOrEqual(1.5); }
    for (const row of Object.values(ground.march)) for (const m of Object.values(row)) expect(m).toBe(1);
    for (const t of Object.keys(TERRAIN_DEF)) expect((ground.says as Record<string, string>)[t].length, t).toBeGreaterThan(15);
    expect(ground.says.steep.length).toBeGreaterThan(15);
    expect(ground.steep.barred).toEqual(['arm']);
    // Mountain troops are the only arm the mountains help.
    expect(ground.attack.mtn.mountain).toBeGreaterThan(1);
    expect(ground.defence.mtn.mountain).toBeGreaterThan(1);
    expect(ground.defence.arm.mountain).toBeLessThan(1);
  });
});

describe('a historical scenario', () => {
  it('knows nothing of the ground rules', () => {
    const g = createGame(3, 'GER');
    expect(groundRules(g)).toBe(false);
    const tyrol = g.byName['Tyrol'];
    expect(steep(g, tyrol)).toBe(false);
    expect(groundNotes(g, tyrol)).toEqual([]);
    expect(groundSays(g, tyrol)).toBe('');
    expect(groundEffects(g, tyrol)).toEqual([]);
    for (const kind of Object.keys(KIND) as Kind[]) {
      expect(groundAttack(g, kind, tyrol)).toBe(1);
      expect(groundDefence(g, kind, tyrol)).toBe(1);
      expect(groundMarch(g, kind, tyrol)).toBe(0);
    }
  });

  it('lets tanks into the Alps and settles battles by the classic rules alone', () => {
    const g = createGame(3, 'GER');
    const tanks = corps(g, 'GER', 'arm', clear(g.byName['Bavaria']));
    expect(barred(g, tanks, g.byName['Tyrol'])).toBe(false);
    expect(whyBarred(g, tanks, g.byName['Tyrol'])).toBeNull();
    expect(moveCorps(g, tanks, clear(g.byName['Tyrol']))).toBe(true);
    const { target, from } = front(g);
    const att = [corps(g, 'GER', 'inf', from), corps(g, 'GER', 'arm', from)];
    for (const t of ['forest', 'mountain', 'urban'] as Terrain[]) {
      const a = on(target, t, () => assess(g, att, target));
      expect(a.lines.some((l) => l.label === 'Ground' || l.label === 'Combined arms'), t).toBe(false);
    }
    // Tanks and infantry attacking together are worth exactly the sum of the two.
    const both = assess(g, att, target), foot = assess(g, [att[0]], target), tank = assess(g, [att[1]], target);
    expect(both.A).toBeCloseTo(foot.A + tank.A, 9);
    // And tanks pay no more than they ever did to push into a forest.
    expect(on(target, 'forest', () => moveCost(g, att[1], target))).toBe(on(target, 'plains', () => moveCost(g, att[1], target)));
  });
});

describe('ground by arm', () => {
  it('multiplies an attack by what the ground does to that arm', () => {
    const g = grand();
    const { target, from } = front(g);
    for (const kind of ['inf', 'mtn', 'mot', 'art', 'cav'] as Kind[]) {
      const c = corps(g, 'GER', kind, from);
      const open = on(target, 'plains', () => assess(g, [c], target).A);
      for (const t of ['forest', 'hills', 'mountain', 'marsh', 'urban'] as Terrain[]) {
        const there = on(target, t, () => assess(g, [c], target).A);
        expect(there / open, `${kind} in ${t}`).toBeCloseTo(GROUND.attack[kind]?.[t] ?? 1, 9);
      }
      c.dead = true; from.units.splice(from.units.indexOf(c), 1);
    }
  });

  it('makes mountain troops the masters of the mountains, in attack and defence', () => {
    const g = grand();
    const { target, from } = front(g);
    const jager = corps(g, 'GER', 'mtn', from), inf = corps(g, 'GER', 'inf', from);
    on(target, 'mountain', () => {
      // Division for division the infantry has the better attack on paper; in the mountains the mountain troops out-fight it.
      expect(assess(g, [jager], target).A).toBeGreaterThan(assess(g, [inf], target).A);
      const line = assess(g, [jager], target).lines.find((l) => l.label === 'Ground')!;
      expect(line.value).toBe(GROUND.says.mountain);
      expect(line.effect).toContain('30%');
    });
    clear(target);
    const holder = corps(g, 'POL', 'mtn', target);
    const flat = on(target, 'plains', () => assess(g, [inf], target).D), high = on(target, 'mountain', () => assess(g, [inf], target).D);
    expect(high / flat).toBeCloseTo(TERRAIN_DEF.mountain * GROUND.defence.mtn!.mountain!, 9);
    expect(holder.kind).toBe('mtn');
  });

  it('leaves tanks weak in close country, in attack and defence', () => {
    const g = grand();
    const { target, from } = front(g);
    const tanks = corps(g, 'GER', 'arm', from), inf = corps(g, 'GER', 'inf', from, 9);
    // With infantry enough to cover them, the ground is all that tells against the tanks.
    const lift = 1 + GROUND.arms.armour;
    const alone = (t: Terrain): number => on(target, t, () => assess(g, [tanks, inf], target).A - assess(g, [inf], target).A * (1 + GROUND.arms.infantry * combinedArms(g, [tanks, inf], target, false)!.share));
    const open = alone('plains');
    expect(alone('forest') / open).toBeCloseTo(GROUND.attack.arm!.forest!, 9);
    expect(alone('hills') / open).toBeCloseTo(GROUND.attack.arm!.hills!, 9);
    expect(alone('mountain') / open).toBeCloseTo(0.6 / 1.25, 9);
    expect(lift).toBeGreaterThan(1);
    // In defence a mountain is worth less to a tank corps than to anyone else.
    clear(target);
    corps(g, 'POL', 'arm', target);
    const flat = on(target, 'plains', () => assess(g, [inf], target).D), high = on(target, 'mountain', () => assess(g, [inf], target).D);
    expect(high / flat).toBeCloseTo(TERRAIN_DEF.mountain * GROUND.defence.arm!.mountain!, 9);
  });

  it('slows tanks, lorries and guns off the roads, and nobody on them', () => {
    const g = grand();
    const { target, from } = front(g);
    const tanks = corps(g, 'GER', 'arm', from), mot = corps(g, 'GER', 'mot', from), guns = corps(g, 'GER', 'art', from), inf = corps(g, 'GER', 'inf', from), jager = corps(g, 'GER', 'mtn', from);
    const cost = (c: Corps, t: Terrain, r = target): number => on(r, t, () => moveCost(g, c, r));
    expect(cost(tanks, 'forest')).toBe(cost(tanks, 'plains') + 1);
    expect(cost(tanks, 'marsh')).toBe(cost(tanks, 'plains') + 2); // the marsh slows everyone a move, tanks two
    expect(cost(tanks, 'mountain')).toBe(cost(tanks, 'plains') + 2);
    expect(cost(mot, 'mountain')).toBe(cost(mot, 'plains') + 2);
    expect(cost(mot, 'forest')).toBe(cost(mot, 'plains'));
    expect(cost(guns, 'mountain')).toBe(cost(guns, 'plains') + 2);
    expect(cost(inf, 'forest')).toBe(cost(inf, 'plains'));
    expect(cost(inf, 'mountain')).toBe(cost(inf, 'plains') + 1);
    expect(cost(jager, 'mountain')).toBe(cost(jager, 'plains'));
    // Along roads through ground held since before the month began, a forest costs a tank no more than a field.
    const home = g.regions[from.nb.find((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER')!];
    expect(cost(tanks, 'forest', home)).toBe(cost(tanks, 'plains', home));
  });

  it('tells the player what the ground does, province by province', () => {
    const g = grand();
    const notes = (name: string): string => groundNotes(g, g.byName[name]).join(' ');
    const mountain = g.regions.find((r) => r.terrain === 'mountain' && !steep(g, r))!;
    expect(groundSays(g, mountain)).toBe(GROUND.says.mountain);
    expect(groundNotes(g, mountain).join(' ')).toMatch(/Tanks: attack down 40%, defence down 25%, one more move/);
    expect(groundNotes(g, mountain).join(' ')).toMatch(/Mountain troops: attack up 30%, defence up 25%/);
    expect(groundSays(g, g.byName['Tyrol'])).toBe(GROUND.says.steep);
    expect(notes('Tyrol')).toMatch(/Tanks cannot enter/);
    const city = g.regions.find((r) => r.terrain === 'urban')!;
    expect(groundNotes(g, city).join(' ')).toMatch(/without infantry/);
    const plain = g.regions.find((r) => r.terrain === 'plains')!;
    expect(groundNotes(g, plain)).toEqual([]);
    expect(groundSays(g, plain)).toBe(GROUND.says.plains);
    expect(groundEffects(g, g.byName['Tyrol']).find((e) => e.kind === 'arm')!.barred).toBe(true);
  });
});

describe('ground too steep for tanks', () => {
  it('cannot be entered by an armoured corps by road, rail or sea, while infantry walks in', () => {
    const g = grand();
    const bavaria = clear(g.byName['Bavaria']), tyrol = clear(g.byName['Tyrol']);
    const tanks = corps(g, 'GER', 'arm', bavaria), inf = corps(g, 'GER', 'inf', bavaria), jager = corps(g, 'GER', 'mtn', bavaria);
    expect(barred(g, tanks, tyrol)).toBe(true);
    expect(whyBarred(g, tanks, tyrol)).toMatch(/Tyrol is too steep for tanks/);
    expect(whyBarred(g, inf, tyrol)).toBeNull();
    expect(moveCorps(g, tanks, tyrol)).toBe(false);
    expect(tanks.reg).toBe(bavaria.id);
    expect(railReach(g, tanks).has(tyrol.id)).toBe(false);
    expect(railMove(g, tanks, tyrol)).toBe(false);
    // The trains still run through: the country beyond the pass is in reach.
    expect(railReach(g, inf).has(tyrol.id)).toBe(true);
    expect(moveCorps(g, inf, tyrol)).toBe(true);
    expect(moveCorps(g, jager, tyrol)).toBe(true);
    // No tanks are raised there either, however much industry the valley is given.
    tyrol.fac = 3;
    expect(canRaiseKind(g, 'GER', tyrol, 'arm')).toBe(false);
    expect(canRaiseKind(g, 'GER', tyrol, 'mot')).toBe(true);
    // Nor shipped to a steep harbour.
    const narvik = g.byName['Narvik'];
    expect(whyNotShip(g, tanks, narvik)).not.toBeNull();
  });

  it('is left out of the reach the player is shown', () => {
    const g = grand(3, 'GER');
    const bavaria = clear(g.byName['Bavaria']);
    clear(g.byName['Tyrol']);
    const tanks = corps(g, 'GER', 'arm', bavaria), inf = corps(g, 'GER', 'inf', bavaria);
    expect(reach(g, inf).has(g.byName['Tyrol'].id)).toBe(true);
    expect(reach(g, tanks).has(g.byName['Tyrol'].id)).toBe(false);
    expect(reach(g, tanks).has(g.byName['Upper Austria'].id)).toBe(true);
  });

  it('cannot be attacked by tanks: they are left out of the battle', () => {
    const g = grand();
    // A real steep region held by an enemy: Graubunden, with Germany at war with Switzerland.
    const swiss = g.byName['Graubunden'], tyrol = clear(g.byName['Tyrol']);
    g.nations.GER.atWar.push('SUI'); g.nations.SUI.atWar.push('GER');
    const mtnInf = corps(g, 'GER', 'inf', tyrol);
    const strays = corps(g, 'GER', 'arm', g.byName['Bavaria']);
    strays.reg = tyrol.id; tyrol.units.push(strays); // tanks that were already there when the rule came in
    if (!swiss.units.length) corps(g, 'SUI', 'inf', swiss, 1);
    expect(limitAttackers(g, [strays, mtnInf], swiss)).toEqual([mtnInf]);
    const withTanks = assess(g, [strays, mtnInf], swiss), without = assess(g, [mtnInf], swiss);
    expect(withTanks.A).toBeCloseTo(without.A, 9);
    expect(withTanks.lines.find((l) => l.label === 'Ground')!.effect).toMatch(/cannot climb here/);
    expect(assess(g, [strays], swiss).odds).toBe(0);
  });
});

describe('combined arms', () => {
  it('makes infantry and tanks each worth more beside the other, as far as the tanks go round', () => {
    const g = grand();
    const { target, from } = front(g);
    const inf = corps(g, 'GER', 'inf', from, 6), tanks = corps(g, 'GER', 'arm', from, 2);
    on(target, 'plains', () => {
      const both = assess(g, [inf, tanks], target), foot = assess(g, [inf], target), tank = assess(g, [tanks], target);
      const arms = combinedArms(g, [inf, tanks], target, false)!;
      expect(arms.share).toBeCloseTo(1, 9);   // two armoured divisions support six of infantry
      expect(arms.cover).toBe(1);
      expect(both.A).toBeCloseTo(foot.A * (1 + GROUND.arms.infantry) + tank.A * (1 + GROUND.arms.armour), 6);
      const line = both.lines.find((l) => l.label === 'Combined arms')!;
      expect(line.value).toBe('Tanks and infantry attack together');
      expect(line.effect).toBe('Infantry attack up 15%, tank attack up 10%');
      // Infantry on its own gets no such line, and tanks on their own are told what they are missing.
      expect(foot.lines.some((l) => l.label === 'Combined arms')).toBe(false);
      expect(tank.lines.find((l) => l.label === 'Combined arms')!.value).toBe('Tanks attacking without infantry');
    });
  });

  it('spreads thin when there are few tanks among much infantry', () => {
    const g = grand();
    const { target, from } = front(g);
    const a = corps(g, 'GER', 'inf', from, 6), b = corps(g, 'GER', 'inf', from, 6), tanks = corps(g, 'GER', 'arm', from, 1);
    on(target, 'plains', () => {
      const arms = combinedArms(g, [a, b, tanks], target, false)!;
      expect(arms.share).toBeCloseTo(3 / 12, 9);
      expect(arms.infantry).toBeCloseTo(1 + GROUND.arms.infantry * 0.25, 9);
      const all = assess(g, [a, b, tanks], target);
      expect(all.A).toBeCloseTo((assess(g, [a], target).A + assess(g, [b], target).A) * arms.infantry + assess(g, [tanks], target).A * (1 + GROUND.arms.armour), 6);
      expect(all.lines.find((l) => l.label === 'Combined arms')!.value).toMatch(/part of the infantry/);
    });
  });

  it('punishes tanks sent into a city or a forest alone, and forgives them with infantry beside them', () => {
    const g = grand();
    const { target, from } = front(g);
    const tanks = corps(g, 'GER', 'arm', from, 3), inf = corps(g, 'GER', 'inf', from, 3);
    const open = on(target, 'plains', () => assess(g, [tanks], target).A);
    on(target, 'urban', () => {
      const lone = assess(g, [tanks], target);
      // The classic rule cuts tanks to 60% in a city and takes their shock away; alone, they lose a quarter more.
      expect(lone.A / open).toBeCloseTo((0.6 / 1.25) * GROUND.arms.alone.urban!, 9);
      expect(lone.lines.find((l) => l.label === 'Combined arms')!.value).toMatch(/city streets with no infantry/);
      const arms = combinedArms(g, [tanks, inf], target, false)!;
      expect(arms.alone).toBe(1);
      expect(arms.armour).toBeCloseTo(1 + GROUND.arms.armour, 9);
    });
    on(target, 'forest', () => {
      expect(assess(g, [tanks], target).A / open).toBeCloseTo(GROUND.attack.arm!.forest! * GROUND.arms.alone.forest!, 9);
    });
  });

  it('helps a defence that keeps tanks in reserve behind its infantry', () => {
    const g = grand();
    const { target, from } = front(g);
    const att = [corps(g, 'GER', 'inf', from)];
    clear(target);
    corps(g, 'POL', 'inf', target, 3);
    on(target, 'plains', () => {
      const before = assess(g, att, target).D;
      const reserve = corps(g, 'POL', 'arm', target, 1);
      const after = assess(g, att, target);
      const own = (() => { const keep = target.units; target.units = [reserve]; const d = assess(g, att, target).D; target.units = keep; return d; })();
      expect(after.D).toBeCloseTo(before * (1 + GROUND.arms.defence.infantry) + own, 6);
      expect(after.lines.find((l) => l.label === 'Combined arms')!.effect).toBe('Infantry defence up 8%');
    });
  });
});

describe('the computer players', () => {
  it('fight a two-year war without a tank ever climbing where it cannot go', () => {
    const g = grand(5);
    const high = g.regions.filter((r) => steep(g, r));
    for (let t = 0; t < 26; t++) {
      runTurnAI(g);
      for (const r of high) expect(r.units.filter((c) => c.kind === 'arm' && !c.dead).map((c) => `${c.name} in ${r.name}, ${g.turn}`)).toEqual([]);
    }
    // And the war went somewhere: Poland has fallen.
    expect(g.nations.POL.alive).toBe(false);
  }, 120000);
});
