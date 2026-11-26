// What an order buys on the battle card for its price, at the staff's own sight (release 2; the design director's
// rulings of 25 November 2026 on the support teams' quota, 19:48Z: "a staff orders a brigade only when, at its own
// sight, it buys more on the card for its price than the corps or refit the same money would, using the build pick's
// one value test for every order. The quota goes; the price is not touched"; 20:08Z: "one reckoning for every order,
// not only one test ... the card's own strength with the order and without it where it would stand"; and 21:33Z:
// "one card, not a sum. Weigh every order on the card that the front where it would stand will fight under the
// staff's plan now: the attack card where the plan attacks there, the defence card where it holds or where the plan
// names nothing ... the front decides the card, not the order's kind").
//
// One reckoning for every order: the battle card itself (game.ts assess), read with the order standing where it would
// stand and without it, per point of its price. So whatever the card counts for a formation counts for every order
// alike: its own weight, the ground, its arm's suits and the arms beside it, the works it digs in with, the brigades'
// and the guns' lifts, the day's luck against a ready line. The card's loss bill is not weighed, for any order (the
// design director's 20:08Z 2, on the known list).
//
// Each front sector the staff holds is read on one card, the one its plan this month fights there (plannow.ts, set by
// the campaign as it reads the month's plan):
//   - The attack card where the plan attacks there: the card's odds (A over D, times the run of the day's luck) of an
//     attack from the sector into a province next door the staff can see and the plan attacks (any, where it names
//     none), with the order going in and without it, read as attack strength: what the attack would have to gain on
//     its own to move the odds as far.
//   - The defence card where the plan holds, or names nothing there: the card's defence (D) of the sector against the
//     attack the enemy it can see next door could bring, with the order there and without it.
// A player who runs his own army has his own orders for a plan: the fronts next to a province his corps attacked this
// month or the month just ended read the attack card (the design director, 21:44Z; plannow.ts hisOrders).
// The order is weighed at the sector where it adds most and there is room for it (as teamsUp and gunsUp send them),
// joining the works of the line it stands in (the fewest months of them). Orders of the same kind already on order
// stand there too, so a want is not bought twice. A new formation is full, at a new corps' readiness, training and
// heart (economy.ts FRESH, morale.ts raiseMorale). A refit is the drafts the staff's corps at its fronts are waiting
// for this month, each sector's read on its own card; it is what the money would buy only where the purse, with the
// order paid, would not cover them.
//
// Only the support teams' rule asks it (economy.ts aiTeams); nothing reads it while they are off.

import { armySize, assess, ensureSupply, fights, friendlyUnits, hostileUnits, type Assessment } from './game';
import { FRESH, PRODUCTION, corpsCost, specOf } from './economy';
import { SIGHT, sightOf } from './intel';
import { raiseMorale } from './morale';
import { planAt } from './plannow';
import { raisedXp } from './stories';
import { roomLeft } from './islands';
import { TEAMS, isTeam } from './teams';
import { type Corps, type Game, type Kind, type Region } from './types';

/** The card a front sector is read on: its plan's attack, or its defence. */
export type Card = 'atk' | 'def';

/** The number a stand-in formation carries while the card is read: no corps has it. */
const STAND_IN = -424242;

/**
 * A front sector the staff holds: its own corps there, the enemy it can see next door, the provinces they stand in,
 * and the card it is read on (on the attack card, the targets are the provinces the plan attacks).
 */
interface Front { r: Region; mine: Corps[]; foes: Corps[]; targets: Region[]; card: Card }

const standing = (cs: Corps[]): Corps[] => cs.filter((u) => !u.dead && u.town === undefined);
const odds = (a: Assessment): number => (a.D > 0 ? (a.A / a.D) * ((a.dice[0] + a.dice[1]) / 2) : 0);

/** A staff's reckoning for one month's orders: the card read with and without each order, the readings without kept. */
export class Reckoning {
  private fronts: Front[];
  private defBase = new Map<number, number>();
  private atkBase = new Map<string, Assessment>();
  constructor(private g: Game, private nat: string, sight: Uint8Array = sightOf(g, nat)) {
    // Supply is drawn before any stand-in is set down, so the card never draws it with one on the map.
    ensureSupply(g);
    const out: Front[] = [];
    for (const r of g.regions) {
      const mine = standing(r.units.filter((u) => u.nat === nat && u.kind !== 'fort'));
      if (!mine.some(fights)) continue;
      const foes: Corps[] = [], targets: Region[] = [];
      for (const j of r.nb) {
        if (sight[j] === SIGHT.none) continue;
        const there = standing(hostileUnits(g, g.regions[j], nat));
        if (!there.length) continue;
        targets.push(g.regions[j]);
        foes.push(...there.filter((u) => u.kind !== 'fort'));
      }
      if (!targets.length) continue;
      const plan = planAt(g, nat, r.id);
      const aims = plan?.attack ? targets.filter((t) => !plan.only || plan.only.has(t.name)) : [];
      out.push(aims.length ? { r, mine, foes, targets: aims, card: 'atk' } : { r, mine, foes, targets, card: 'def' });
    }
    this.fronts = out;
  }

  /** A fresh formation of this kind standing in r, dug into the line's works. */
  private standIn(kind: Kind, divs: number, f: Front): Corps {
    const g = this.g, nat = this.nat;
    const c = { id: STAND_IN, nat, name: '', kind, divs, str: 1, org: FRESH.org, xp: nat === g.human ? raisedXp(g, nat) : FRESH.xp, reg: f.r.id, ap: 4, apMax: 4, general: null, hold: false, dyle: null, supplied: true, fought: false, dead: false } as Corps;
    const dug = f.mine.filter((u) => fights(u) && u.dug);
    if (dug.length) { c.dug = true; c.dugFor = Math.min(...dug.map((u) => u.dugFor ?? 1)); }
    raiseMorale(g, c);
    return c;
  }

  /** The card's defence at the front with these formations standing in it too. */
  private defence(f: Front, extra: Corps[]): number {
    if (!f.foes.length) return 0;
    if (!extra.length) {
      let d = this.defBase.get(f.r.id);
      if (d === undefined) this.defBase.set(f.r.id, (d = assess(this.g, f.foes, f.r).D));
      return d;
    }
    const units = f.r.units, n = units.length;
    units.push(...extra);
    try { return assess(this.g, f.foes, f.r).D; } finally { units.length = n; }
  }

  /** The card's attack from the front into t with these formations going in too. */
  private attack(f: Front, t: Region, extra: Corps[]): Assessment {
    if (!extra.length) {
      const key = `${f.r.id}:${t.id}`;
      let a = this.atkBase.get(key);
      if (!a) this.atkBase.set(key, (a = assess(this.g, f.mine, t)));
      return a;
    }
    return assess(this.g, [...f.mine, ...extra], t);
  }

  /** What these changes at a front add to its card: on the defence card D, on the attack card the attack strength that would move the odds as far, at the target where most. */
  private gain(f: Front, before: Corps[], after: Corps[]): number {
    if (f.card === 'def') return this.defence(f, after) - this.defence(f, before);
    let best = 0;
    for (const t of f.targets) {
      const a0 = this.attack(f, t, before), a1 = this.attack(f, t, after), o0 = odds(a0);
      if (o0 > 0) best = Math.max(best, a0.A * (odds(a1) / o0 - 1));
    }
    return best;
  }

  /** What a new formation of this kind adds to the card where it adds most and there is room for it. */
  private formation(kind: Kind, divs: number): number {
    const g = this.g, nat = this.nat;
    const queued = g.nations[nat].queue.filter((b) => b.what === 'corps' && b.kind === kind && b.divs).map((b) => b.divs!);
    let best = 0;
    for (const f of this.fronts) {
      // (Room as every rule counts it: five corps, or an island's room in men. islands.ts `roomLeft`.)
      if (roomLeft(g, f.r, nat, friendlyUnits(g, f.r, nat).length) < 1) continue;
      if (isTeam({ kind }) && f.mine.filter(isTeam).length >= TEAMS.frontage) continue;
      const before = queued.map((d) => this.standIn(kind, d, f));
      best = Math.max(best, this.gain(f, before, [...before, this.standIn(kind, divs, f)]));
    }
    return best;
  }

  /**
   * What the drafts the staff's corps at its fronts are waiting for this month add to the card, per point, where the
   * purse, with 'spend' paid out of it, would not cover them; 0 where it would (the money is then not theirs).
   */
  private refit(spend: number): number {
    const g = this.g, nat = this.nat, n = g.nations[nat];
    const gap = (c: Corps): number => (c.supplied && !c.vol && c.str > 0 ? Math.min(Math.max(0, (c.full ?? 1) - c.str) * c.divs, 0.2 * c.divs) : 0);
    let want = 0;
    for (const c of g.corps) if (c.nat === nat && !c.dead) want += gap(c);
    // The depots send out no more in a month than their share of the army's full size.
    const most = PRODUCTION.manpower * Math.max(n.armyPeak, armySize(g, nat));
    const share = want > most ? most / want : 1;
    const bill = want * share * PRODUCTION.replacementCost;
    if (bill <= 0 || n.pp - spend >= bill) return 0;
    let worth = 0;
    for (const f of this.fronts) {
      const wanting = f.mine.filter((c) => gap(c) > 0);
      if (!wanting.length || (f.card === 'def' && !f.foes.length)) continue;
      // The sector's card without the drafts (kept), then read again with them in.
      const d0 = f.card === 'def' ? this.defence(f, []) : 0;
      const a0 = f.card === 'atk' ? f.targets.map((t) => this.attack(f, t, [])) : [];
      const was = wanting.map((c) => c.str);
      wanting.forEach((c) => { c.str += (gap(c) * share) / c.divs; });
      try {
        if (f.card === 'def') worth += assess(g, f.foes, f.r).D - d0;
        else {
          let best = 0;
          f.targets.forEach((t, k) => { const o0 = odds(a0[k]); if (o0 > 0) best = Math.max(best, a0[k].A * (odds(assess(g, f.mine, t)) / o0 - 1)); });
          worth += best;
        }
      } finally { wanting.forEach((c, k) => { c.str = was[k]; }); }
    }
    return worth / bill;
  }

  /**
   * What an order adds to the card per point of its price, each front on the card its plan fights there: a corps of a
   * kind, or 'refit' (the drafts the money would otherwise pay, 'spend' being the price of the order it is weighed
   * against). 0 for an order with no price in points (a corps paid in men alone).
   */
  worth(order: Kind | 'refit', spend = 0): number {
    if (order === 'refit') return this.refit(spend);
    const spec = specOf(this.g, order), price = corpsCost(this.g, this.nat, order);
    if (!spec || price <= 0) return 0;
    return this.formation(order, spec.divs) / price;
  }

  /** The card each front sector is read on, by province name (for the tests and the gate's readers). */
  cards(): Map<string, Card> {
    return new Map(this.fronts.map((f) => [f.r.name, f.card]));
  }
}
