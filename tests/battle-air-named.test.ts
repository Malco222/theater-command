// Bombers by name in the air battle (Brian, 20 November 2026: "i would like the game to keep track of real bombers
// names and how many missions they have completed and fighters shot down"; "i still want yellow painted bombs for each
// bombing mission they did and then the symbols showing how many aircraft they shot down"). The battle's ticket carries
// each bomber of the raid as the campaign knows it (campaign/bombers.ts seatsOf), for the 3D game to paint; the battle
// says which of them each loss is, and whose gunners brought a fighter down; and what it hands back has the campaign
// strike off the very bombers the player saw fall, and paint the kills on the bombers seen to make them.
//
// Shown, never counted (ruling 237): with the names or without them the same aircraft fall at the same moments.
import { describe, expect, it } from 'vitest';
import { planRaid, raidTargets, type RaidKind } from '../src/campaign/airwar';
import { bomberCard, styleOf, tourOf } from '../src/campaign/bombers';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import type { BomberRow, Game } from '../src/campaign/types';
import type { FieldEvent, FieldTicket } from '../src/battle/core/types';
import { SKY } from '../src/battle/air/book';
import { airBattle, AirSim } from '../src/battle/air/sim';
import { airHandBack, airTicket, foughtAir, raidGroups } from '../src/battle/air/ticket';
import type { AirOutcome, AirPlace, BomberTold } from '../src/battle/air/types';

type Ticket = FieldTicket<AirPlace>;
const WARS: Record<string, string[]> = { GER: ['UK', 'FRA'], UK: ['GER'], FRA: ['GER'] };
const RAIDS: [string, RaidKind, string][] = [['GER', 'bomb', 'Alsace'], ['GER', 'bomb', 'Kent'], ['FRA', 'bomb', 'Rhineland'], ['UK', 'bomb', 'Ruhr']];
const RUNS = 6;

/**
 * The raid on a named province as the raid card flies it (raidGroups: the staff's bombers from their airfields and every
 * fighter group that can go), and the game it is flown in. The bomber groups have 20 missions behind them, so their
 * rosters muster with old hands and new crews both.
 */
function raid(nat: string, kind: RaidKind, name: string, seed = 7): { g: Game; tk: Ticket; target: number; ids: number[] } {
  const g: Game = startOpen(createGame(seed, nat));
  for (const w of WARS[nat]) setWar(g, nat, w);
  for (const f of g.airFleets) if (f.nat === nat && f.bmr > 0) f.rec = { ...(f.rec ?? {}), missions: 20 };
  // The player's nation is the one giving its orders.
  g.phase = g.order.indexOf(nat);
  const t = raidTargets(g, kind, nat).find((x) => planRaid(g, kind, x.id, undefined, nat).place === name);
  if (!t) throw new Error(`no ${kind} raid on ${name} for ${nat}`);
  const ids = raidGroups(g, kind, t.id, nat);
  return { g, tk: airTicket(g, kind, t.id, ids, nat).ticket!, target: t.id, ids };
}

function fight(tk: Ticket, k: number, show = true): AirSim {
  const s = airBattle({ ...tk, seed: tk.seed + k }, { show });
  s.ai.att = true; s.ai.def = true;
  while (!s.over) s.step();
  return s;
}

const made = RAIDS.map(([nat, kind, name]) => raid(nat, kind, name));
const bare = (tk: Ticket): Ticket => { const { bombers: _, ...place } = tk.place; return { ...tk, place: place as AirPlace }; };
const lostOf = (h: FieldEvent[]) => h.filter((e) => e.what === 'air:lost');
const bomberOf = (e: FieldEvent): BomberTold | undefined => (e.extra as { bomber?: BomberTold } | undefined)?.bomber;
const gunnerOf = (e: FieldEvent): (BomberTold & { box: number }) | undefined => (e.extra as { gunner?: BomberTold & { box: number } } | undefined)?.gunner;
/** A battle's dispatches without the one line only the names add. */
const strip = (h: FieldEvent[]) => h.filter((e) => e.what !== 'air:named').map((e) => ({ t: e.t, what: e.what, unit: e.unit, text: e.text, cue: e.cue }));
/** An outcome without what the names add to it. */
const counts = (o: AirOutcome) => ({ ...o, groups: o.groups.map(({ fell: _f, shot: _s, ...gr }) => gr) });
const key = (b: BomberRow): string => `${b[0]}:${b[1]}`;

describe('the ticket carries the raid\'s bombers one by one', () => {
  it('every bomber that flies has a seat, with what its nation paints on it; the old hands come first', () => {
    for (const { tk } of made) {
      const boxes = tk.att.forces.filter((f) => f.kind === 'bmr');
      expect(boxes.length).toBeGreaterThan(0);
      for (const f of boxes) {
        const B = tk.place.bombers![f.corps];
        expect(B, `${f.nat} ${f.name}`).toBeDefined();
        expect(B.seats.length).toBe(f.men);
        expect(B.style).toBe(styleOf(f.nat));
        expect(B.flown).toBe(f.nat === 'UK' ? 'operation' : 'mission');
        for (let i = 1; i < B.seats.length; i++) expect(B.seats[i - 1].missions).toBeGreaterThanOrEqual(B.seats[i].missions);
        for (const s of B.seats) {
          // Nose art is America's alone: every other nation's bomber has its marking and no painting.
          if (B.style !== 'nose') { expect(s.art).toBe(0); expect(s.mark).not.toBe(''); expect(s.name).not.toBe(''); }
          expect(s.risk).toBeGreaterThan(0);
          expect(s.row.slice(2, 4)).toEqual([s.missions, s.kills]);
        }
        // No two bombers of a group carry the same marking (two of its squadrons may each have a 'G for George').
        const marks = B.seats.map((s) => s.mark).filter(Boolean);
        expect(new Set(marks).size).toBe(marks.length);
      }
    }
    // The markings are each nation's own.
    const seat = (i: number) => Object.values(made[i].tk.place.bombers!)[0].seats[0];
    expect(seat(0).mark).toMatch(/^[0-9A-Z]{2}\+[A-Z]{2}$/);
    expect(seat(3).name).toMatch(/^[A-Z] for [A-Z][a-z]+$/);
    expect(seat(3).mark).toMatch(/^[A-Z]{2}-[A-Z]$/);
    expect(seat(2).name).toMatch(/^No\. \d+$/);
  });

  it('making the ticket writes nothing to the game, and the same raid has the same seats', () => {
    const [nat, kind, name] = RAIDS[3];
    const { g, tk, target, ids } = raid(nat, kind, name);
    const before = JSON.stringify(g);
    const again = airTicket(g, kind, target, ids, nat).ticket!;
    expect(JSON.stringify(g)).toBe(before);
    expect(again.place.bombers).toEqual(tk.place.bombers);
  });
});

describe('shown, never counted (ruling 237)', () => {
  it('with the names and without them every battle ends the same: the counts, the bombs, the verdict, every dispatch at its moment', () => {
    for (const { tk } of made) for (let k = 0; k < RUNS; k++) {
      const on = fight(tk, k), off = fight(bare(tk), k);
      expect(counts(on.outcome())).toEqual(off.outcome());
      expect(on.over!.level).toBe(off.over!.level);
      expect(on.over!.lines).toEqual(off.over!.lines);
      expect(strip(on.history())).toEqual(strip(off.history()));
      // Without the names nothing is named.
      expect(off.history().some((e) => e.what === 'air:named' || bomberOf(e) || gunnerOf(e))).toBe(false);
      expect(off.outcome().groups.some((gr) => gr.fell || gr.shot)).toBe(false);
    }
  });

  it('shown or not, the same bombers fall and the same gunners score', () => {
    for (const { tk } of made) for (let k = 0; k < RUNS; k++) {
      const on = fight(tk, k, true), off = fight(tk, k, false);
      expect(on.outcome()).toEqual(off.outcome());
      expect(lostOf(on.history()).map((e) => [bomberOf(e), gunnerOf(e)])).toEqual(lostOf(off.history()).map((e) => [bomberOf(e), gunnerOf(e)]));
      // The line that names an old hand is a shown battle's only.
      expect(off.history().some((e) => e.what === 'air:named')).toBe(false);
    }
  });
});

describe('each loss is a bomber by name', () => {
  it('every bomber lost names its seat, no seat falls twice, and the rows handed back are those seats\' in the order they fell', () => {
    let fell = 0;
    for (const { tk } of made) for (let k = 0; k < RUNS; k++) {
      const s = fight(tk, k);
      const out = s.outcome();
      for (const u of s.units.filter((x) => x.air === 'bmr' && x.side === 'att')) {
        const B = tk.place.bombers![u.corps];
        const told = lostOf(s.history()).filter((e) => e.unit === u.id).map((e) => bomberOf(e)!);
        expect(told.every(Boolean)).toBe(true);
        expect(told.length).toBe(u.n0 - u.n);
        expect(new Set(told.map((b) => b.seat)).size).toBe(told.length);
        for (const b of told) expect(b).toEqual({ seat: b.seat, name: B.seats[b.seat].name, mark: B.seats[b.seat].mark, art: B.seats[b.seat].art, missions: B.seats[b.seat].missions, kills: B.seats[b.seat].kills });
        const gr = out.groups.find((x) => x.id === u.corps)!;
        expect(gr.fell).toEqual(told.map((b) => B.seats[b.seat].row));
        expect(gr.fell!.length).toBe(gr.lostBmr);
        fell += told.length;
      }
    }
    expect(fell).toBeGreaterThan(40);
  });

  it('a bomber that falls out of formation keeps its seat: when the straggler goes down, it is that bomber', () => {
    let seen = 0;
    for (const { tk } of made) for (let k = 0; k < RUNS; k++) {
      const h = fight(tk, k).history();
      const out = new Map(h.filter((e) => e.what === 'air:straggle').map((e) => [(e.extra as { id: number }).id, bomberOf(e)!]));
      for (const b of out.values()) expect(b).toBeDefined();
      // Two stragglers of a box at once are never the same bomber.
      for (const e of lostOf(h)) {
        const id = (e.extra as { strag?: number }).strag;
        if (id === undefined) continue;
        expect(bomberOf(e)).toEqual(out.get(id));
        seen++;
      }
    }
    expect(seen).toBeGreaterThan(10);
  });

  it('a fighter that falls to a named box\'s gunners names the bomber that got it, one of the other side\'s still flying; the kills handed back are those', () => {
    let scored = 0, fighters = 0;
    for (const { tk } of made) for (let k = 0; k < RUNS; k++) {
      const s = fight(tk, k);
      const gone = new Map<number, Set<number>>();
      const tally = new Map<string, number>();
      for (const e of lostOf(s.history())) {
        const b = bomberOf(e);
        if (b) { (gone.get(e.unit!) ?? gone.set(e.unit!, new Set()).get(e.unit!)!).add(b.seat); expect(gunnerOf(e)).toBeUndefined(); continue; }
        fighters++;
        const gun = gunnerOf(e);
        if (!gun) continue;
        const box = s.unit(gun.box)!, victim = s.unit(e.unit!)!;
        expect(box.air).toBe('bmr');
        expect(box.side).not.toBe(victim.side);
        expect(gone.get(box.id)?.has(gun.seat) ?? false).toBe(false);
        expect(gun.name).toBe(tk.place.bombers![box.corps].seats[gun.seat].name);
        // The loss is booked to the box whose gunners got it (ruling 227).
        const by = (e.extra as { by?: number | 'flak' | null }).by;
        expect(by).toBe(box.id);
        const at = `${box.corps}:${gun.seat}:${victim.nat}`;
        tally.set(at, (tally.get(at) ?? 0) + 1);
        scored++;
      }
      const back = new Map<string, number>();
      for (const gr of s.outcome().groups) for (const x of gr.shot ?? []) {
        const seat = tk.place.bombers![gr.id].seats.findIndex((q) => q.row === x.row);
        for (const [foe, n] of Object.entries(x.by)) back.set(`${gr.id}:${seat}:${foe}`, n);
      }
      expect(back).toEqual(tally);
    }
    // Some of the defenders' losses are the gunners', and most are the escort's.
    expect(scored).toBeGreaterThan(10);
    expect(scored).toBeLessThan(fighters / 2);
  });

  it('one of a group\'s longest-serving bombers is named as it goes, in a line of its own; the rest of its losses are not', () => {
    let told = 0, most = 0;
    for (const { tk } of made) for (let k = 0; k < RUNS; k++) {
      const s = fight(tk, k), h = s.history();
      const lines = h.filter((e) => e.what === 'air:named');
      const old = lostOf(h).filter((e) => { const b = bomberOf(e); return b && b.seat < SKY.named.lines && b.name && b.missions >= tk.place.bombers![s.unit(e.unit!)!.corps].told; });
      expect(lines.map((e) => bomberOf(e))).toEqual(old.map((e) => bomberOf(e)));
      for (const e of lines) {
        const b = bomberOf(e)!;
        expect(e.text).toBe(`That was ${b.name}, with ${b.missions} ${tk.att.nat === 'UK' ? 'operation' : 'mission'}s flown.`);
        told++;
      }
      // Never a strip full of names: at most a couple for each group, however many it loses.
      const boxes = s.units.filter((u) => u.air === 'bmr' && u.side === 'att').length;
      expect(lines.length).toBeLessThanOrEqual(SKY.named.lines * boxes);
      most = Math.max(most, lostOf(h).filter((e) => bomberOf(e)).length);
    }
    expect(told).toBeGreaterThan(0);
    expect(most).toBeGreaterThan(20);
  });
});

describe('what the battle hands back is what the campaign books', () => {
  it('the bombers seen to fall are the ones struck off their group, and the gunners seen to score have the kills painted on', () => {
    let struck = 0, painted = 0;
    for (const [nat, kind, name] of RAIDS) for (let k = 0; k < 3; k++) {
      const { g, tk, target, ids } = raid(nat, kind, name);
      const s = fight(tk, k);
      const out = s.outcome();
      const fought = foughtAir(out);
      const rep = airHandBack(g, kind, target, ids, s.over!);
      expect(rep, `${nat} ${name} ${k}`).not.toBeNull();
      for (const gr of out.groups.filter((x) => x.fell)) {
        const f = g.airFleets.find((x) => x.id === gr.id)!;
        const B = tk.place.bombers![gr.id];
        const on = f.bombers!.on;
        // The group's count is the battle's, and its roster is as long as the group.
        expect(on.length).toBe(f.bmr);
        // None of those that fell is on the roster (a marking is painted again on the aircraft that takes the place, as another of that marking).
        const now = new Set(on.map(key));
        for (const b of fought.fell![gr.id]) { expect(now.has(key(b)), `${nat} ${name} ${k}: ${key(b)} fell`).toBe(false); struck++; }
        // Every bomber that flew and did not fall is still there, unless its crew finished a tour and flew it home.
        const fell = new Set(fought.fell![gr.id].map(key));
        for (const seat of B.seats) {
          if (fell.has(key(seat.row)) || seat.crew + 5 >= tourOf(f.nat)) continue;
          expect(now.has(key(seat.row)), `${nat} ${name} ${k}: ${seat.name} came back`).toBe(true);
        }
        // The kills the battle saw are on the bombers seen to make them, by the victim's nation, in the order seen and
        // up to the group's share of the raid's kills (stories.ts storyAir: the defenders lost, shared among the groups that flew).
        let left = Math.round(rep!.def.reduce((t, x) => t + x.lostFtr, 0) / rep!.att.length);
        for (const x of fought.shot![gr.id] ?? []) {
          const b = on.find((q) => key(q) === key(x.row));
          if (fell.has(key(x.row)) || !b) continue;
          const seen = Math.min(left, Object.values(x.by).reduce((a, n) => a + n, 0));
          left -= seen;
          expect(b[3], `${nat} ${name} ${k}: ${key(b)}`).toBeGreaterThanOrEqual(x.row[3] + seen);
          expect(Object.values(b[6] ?? {}).reduce((a, n) => a + n, 0)).toBeGreaterThanOrEqual(seen);
          painted += seen;
        }
      }
    }
    expect(struck).toBeGreaterThan(20);
    expect(painted).toBeGreaterThan(5);
  });

  it('the record card after the raid lists no bomber the player saw go down, by what it is called', () => {
    const [nat, kind, name] = RAIDS[0];
    const { g, tk, target, ids } = raid(nat, kind, name);
    const s = fight(tk, 1);
    const downed = lostOf(s.history()).map((e) => bomberOf(e)).filter((b): b is BomberTold => !!b && b.missions > 0);
    expect(downed.length).toBeGreaterThan(0);
    airHandBack(g, kind, target, ids, s.over!);
    for (const gr of s.outcome().groups.filter((x) => x.fell)) {
      const f = g.airFleets.find((x) => x.id === gr.id)!;
      const card = bomberCard(g, f);
      const B = tk.place.bombers![gr.id];
      for (const b of downed.filter((d) => B.seats[d.seat]?.name === d.name)) {
        // The marking flies again on a new aircraft with no missions painted: the old one's tally is gone with it.
        const again = card.find((c) => c.name === b.name);
        if (again) expect(again.missions).toBeLessThanOrEqual(5);
      }
    }
  });
});
