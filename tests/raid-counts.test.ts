// The defences of a target in numbers (Brian, 20 November 2026: "when running the simulations of how risky the bomb
// raid will be calculate how many flak guns they have and fighter squadrons they have that will be in range to
// intercept them"). The forecast has always weighed the guns and the fighters in reach; the raid card and the battle
// ticket now say how many there are. A count only: no loss and no chance moves by it.
import { afterEach, describe, expect, it } from 'vitest';
import { AIRWAR, assessAir, flakGuns, gunsAt, squadronsOf } from '../src/campaign/air';
import { dayOrNight, planRaid } from '../src/campaign/airwar';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { raidTargetsFor, previewRaid } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { airTicket } from '../src/battle/air/ticket';

const KEEP = JSON.parse(JSON.stringify(AIRWAR.count)) as typeof AIRWAR.count;
afterEach(() => { AIRWAR.count = JSON.parse(JSON.stringify(KEEP)); });

/** A German campaign at war with Britain and France too (tests/day-night-107.test.ts). */
function west(seed = 7): Game {
  const g = startOpen(createGame(seed, 'GER'));
  setWar(g, 'GER', 'UK');
  setWar(g, 'GER', 'FRA');
  return g;
}
const targets = (g: Game): number[] => raidTargetsFor(g, 'bomb').filter((t) => !previewRaid(g, 'bomb', t.id).why).map((t) => t.id);
/** The staff's bombers for a target with every fighter group that could go by day. */
function going(g: Game, t: number): number[] {
  const day = planRaid(g, 'bomb', t);
  return [...day.picked, ...day.seats.filter((s) => !s.why && !s.picked && s.kind === 'ftr').map((s) => s.id)];
}
const fleetsOf = (g: Game, ids: number[]) => ids.map((id) => g.airFleets.find((f) => f.id === id)!);
const line = (a: { lines: { label: string; value: string; effect?: string }[] }, label: string) => a.lines.find((l) => l.label === label);

describe('the guns and the squadrons, counted', () => {
  it('every target puts up light flak, and each level of guns built adds heavy guns', () => {
    expect(flakGuns(0)).toEqual({ light: 60, heavy: 0, all: 60 });
    expect(flakGuns(1)).toEqual({ light: 60, heavy: 120, all: 180 });
    expect(flakGuns(3)).toEqual({ light: 60, heavy: 360, all: 420 });
  });

  it('fighters are told in squadrons of their own air force\'s size, and any at all are at least one', () => {
    expect(squadronsOf('GER', 120)).toBe(10);
    expect(squadronsOf('UK', 160)).toBe(10);
    expect(squadronsOf('USA', 100)).toBe(4);
    expect(squadronsOf('FRA', 5)).toBe(1);
    expect(squadronsOf('GER', 0)).toBe(0);
  });

  it('the forecast counts the squadrons able to meet the raid, group by group, and the ticket says so', () => {
    const g = west();
    let met = 0, clear = 0;
    for (const t of targets(g)) {
      const a = assessAir(g, fleetsOf(g, going(g, t)), 'bomb', t);
      expect(a.why).toBeNull();
      expect(a.groups).toBe(a.def.length);
      expect(a.squadrons).toBe(a.def.reduce((n, x) => n + squadronsOf(x.f.nat, x.f.ftr * x.w), 0));
      const l = line(a, 'In range to intercept')!;
      if (a.groups) {
        met++;
        expect(a.squadrons).toBeGreaterThanOrEqual(a.groups);
        expect(l.value).toBe(`About ${a.squadrons.toLocaleString('en-US')} fighter squadron${a.squadrons === 1 ? '' : 's'} from ${a.groups} group${a.groups === 1 ? '' : 's'}`);
        for (const x of a.def) expect(l.effect).toContain(x.f.name);
        expect(l.effect).toContain(`about ${Math.round(a.D).toLocaleString('en-US')} fighters can meet the raids`);
      } else {
        clear++;
        expect(a.squadrons).toBe(0);
        expect(l.value).toBe('No enemy fighters');
      }
    }
    expect(met).toBeGreaterThan(0);
    expect(clear + met).toBeGreaterThan(2);
  });

  it('the forecast counts the guns round the target, and more where guns are built', () => {
    const g = west();
    const t = targets(g).find((x) => g.regions[x].fac > 0)!;
    const ids = going(g, t);
    delete g.flags[`wk:aa:${t}`];
    const bare = assessAir(g, fleetsOf(g, ids), 'bomb', t);
    expect(bare.guns).toEqual({ light: 60, heavy: 0, all: 60 });
    expect(line(bare, 'Flak')!.value).toBe(`About 60 guns round ${g.regions[t].name}`);
    expect(line(bare, 'Flak')!.effect).toBe(`Light flak only, no heavy guns built: about ${Math.round(bare.gunLoss * 100)}% of the bombers brought down by it`);
    g.flags[`wk:aa:${t}`] = 2;
    expect(gunsAt(g, t)).toBe(2);
    const built = assessAir(g, fleetsOf(g, ids), 'bomb', t);
    expect(built.guns).toEqual({ light: 60, heavy: 240, all: 300 });
    expect(line(built, 'Flak')!.value).toBe(`About 300 guns round ${g.regions[t].name}`);
    expect(line(built, 'Flak')!.effect).toBe(`240 heavy guns and 60 light: about ${Math.round(built.gunLoss * 100)}% of the bombers brought down by them`);
    expect(built.gunLoss).toBeGreaterThan(bare.gunLoss);
    // By night the same guns; a fight for the sky sends no bombers over them, so none are counted.
    expect(assessAir(g, fleetsOf(g, ids), 'night', t).guns.all).toBe(300);
    const sky = assessAir(g, fleetsOf(g, ids).filter((f) => f.ftr > 0), 'superiority', t);
    if (!sky.why) { expect(sky.guns.all).toBe(0); expect(line(sky, 'Flak')).toBeUndefined(); }
  });

  it('a count only: with other numbers for a squadron and a battery, every loss and every chance is what it was', () => {
    const g = west();
    const ts = targets(g).slice(0, 6);
    const facts = () => ts.map((t) => { const a = assessAir(g, fleetsOf(g, going(g, t)), 'bomb', t); const p = planRaid(g, 'bomb', t, going(g, t)); return [a.bomberLoss, a.gunLoss, a.D, a.Dpow, p.success, p.bomberLossN, p.fighterLossN]; });
    const was = facts();
    AIRWAR.count.squadron = { _default: 3 };
    AIRWAR.count.guns = { light: 7, heavy: 999 };
    expect(facts()).toEqual(was);
    expect(flakGuns(1).all).toBe(1006);
  });

  it('the raid card gives both counts beside the fighters in reach and the level of the guns', () => {
    const g = west();
    let some = 0;
    for (const t of targets(g).slice(0, 12)) {
      const ids = going(g, t);
      const dn = dayOrNight(g, 'bomb', t, ids)!, p = planRaid(g, 'bomb', t, ids);
      const guns = flakGuns(gunsAt(g, t));
      expect([dn.gunCount, dn.gunHeavy]).toEqual([guns.all, guns.heavy]);
      expect(dn.gunsCount).toBe(guns.heavy ? `about ${guns.all} guns, ${guns.heavy} of them heavy` : `about ${guns.all} light guns`);
      expect(dn.foeGroups).toBe(p.meets.length);
      expect(dn.foesAble).toBe(p.meets.reduce((n, x) => n + x.able, 0));
      expect(dn.foeSquadrons).toBe(p.meets.reduce((n, x) => n + squadronsOf(x.nat, x.able), 0));
      if (dn.foeGroups) { some++; expect(dn.foesCount).toMatch(/^about [0-9,]+ squadrons? of [0-9]+ groups? can intercept$/); }
      else expect(dn.foesCount).toBe('');
    }
    expect(some).toBeGreaterThan(0);
  });

  it('the battle is handed the same count of guns, for the scene to stand round the target', () => {
    const g = west();
    const t = targets(g).find((x) => g.regions[x].fac > 0)!;
    g.flags[`wk:aa:${t}`] = 1;
    const tk = airTicket(g, 'bomb', t, going(g, t), 'GER').ticket!;
    expect(tk.place.flakGuns).toEqual({ light: 60, heavy: 120 });
    expect(tk.lines.some((l) => l.label === 'Flak' && l.value.startsWith('About 180 guns round'))).toBe(true);
  });
});
