// The defences of a target in numbers (Brian, 20 November 2026: "when running the simulations of how risky the bomb
// raid will be calculate how many flak guns they have and fighter squadrons they have that will be in range to
// intercept them"). The forecast has always weighed the guns and the fighters in reach; the raid card and the battle
// ticket now say how many there are. A count only: no loss and no chance moves by it. The numbers are the historian's
// (historian/raid-card-defence-count.md) and the words the UI director's (ui-director/words-bombers-flak-valour.md).
import { afterEach, describe, expect, it } from 'vitest';
import { AIRWAR, assessAir, flakGuns, gunsAt, squadronSize, squadronsOf, standingGuns } from '../src/campaign/air';
import { theUnit } from '../src/campaign/bombers';
import { turnToDate } from '../src/core/ticket';
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
  it('every target puts up light flak, and each level of guns built is heavy guns with two light ones to each', () => {
    expect([flakGuns(0), standingGuns()]).toEqual([{ light: 0, heavy: 0, all: 0 }, 60]);
    expect(flakGuns(1)).toEqual({ light: 240, heavy: 120, all: 360 });
    expect(flakGuns(3)).toEqual({ light: 720, heavy: 360, all: 1080 });
    // More light guns than heavy wherever guns are built, as the record had it.
    for (const level of [1, 2, 3]) expect(flakGuns(level).light).toBeGreaterThan(flakGuns(level).heavy);
  });

  it('fighters are told in squadrons of their own air force\'s size in that year, and any at all are at least one', () => {
    expect(squadronsOf('GER', 120, 1940)).toBe(10);
    expect(squadronsOf('UK', 160, 1940)).toBe(10);
    expect(squadronsOf('FRA', 5, 1940)).toBe(1);
    expect(squadronsOf('GER', 0, 1940)).toBe(0);
    // An American squadron was about 16 aircraft in 1942-43 and 25 from 1944; a German Staffel went from 12 to 16 in
    // 1944; a Japanese chutai was three flights of three.
    expect([squadronSize('USA', 1943), squadronSize('USA', 1944)]).toEqual([16, 25]);
    expect([squadronSize('GER', 1943), squadronSize('GER', 1944), squadronSize('GER', 1945)]).toEqual([12, 16, 16]);
    expect([squadronSize('JAP', 1942), squadronSize('JPN', 1942), squadronSize('UK', 1944), squadronSize('SOV', 1944), squadronSize('ITA', 1941)]).toEqual([9, 9, 16, 12, 12]);
    expect(squadronsOf('USA', 100, 1943)).toBe(6);
    expect(squadronsOf('USA', 100, 1944)).toBe(4);
  });

  it('the forecast counts the squadrons able to meet the raid, group by group, and the ticket says so', () => {
    const g = west();
    let met = 0, clear = 0;
    for (const t of targets(g)) {
      const a = assessAir(g, fleetsOf(g, going(g, t)), 'bomb', t);
      expect(a.why).toBeNull();
      expect(a.groups).toBe(a.def.length);
      expect(a.squadrons).toBe(a.def.reduce((n, x) => n + squadronsOf(x.f.nat, x.f.ftr * x.w, turnToDate(g.turn).year), 0));
      const l = line(a, 'In range to intercept')!;
      if (a.groups) {
        met++;
        expect(a.squadrons).toBeGreaterThanOrEqual(a.groups);
        // In this game a group is one air group, so the line counts squadrons only; the detail names who they are.
        expect(l.value).toBe(`About ${a.squadrons.toLocaleString('en-US')} fighter squadron${a.squadrons === 1 ? '' : 's'}`);
        expect(l.effect).toMatch(/^From .+: about [0-9,]+ fighters can meet the raid\.$/);
        for (const x of a.def) expect(l.effect).toContain(theUnit(x.f.name));
        expect(l.effect).toContain(`about ${Math.round(a.D).toLocaleString('en-US')} fighters can meet the raid.`);
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
    expect([bare.guns, bare.standing]).toEqual([{ light: 0, heavy: 0, all: 0 }, 60]);
    expect(line(bare, 'Flak')!.value).toBe('Only the 60 light guns every target has.');
    const share = (x: number) => (Math.round(x * 100) >= 1 ? `about ${Math.round(x * 100)}%` : 'almost none');
    expect(line(bare, 'Flak')!.effect).toBe(`They may bring down ${share(bare.gunLoss)} of the bombers.`);
    g.flags[`wk:aa:${t}`] = 2;
    expect(gunsAt(g, t)).toBe(2);
    const built = assessAir(g, fleetsOf(g, ids), 'bomb', t);
    expect(built.guns).toEqual({ light: 480, heavy: 240, all: 720 });
    expect(line(built, 'Flak')!.value).toBe('About 720 guns built here, 480 light and 240 heavy, and the 60 light guns every target has.');
    expect(line(built, 'Flak')!.effect).toBe(`They may bring down ${share(built.gunLoss)} of the bombers.`);
    expect(built.gunLoss).toBeGreaterThan(bare.gunLoss);
    // By night the same guns; a fight for the sky sends no bombers over them, so none are counted.
    expect(assessAir(g, fleetsOf(g, ids), 'night', t).guns.all).toBe(720);
    const sky = assessAir(g, fleetsOf(g, ids).filter((f) => f.ftr > 0), 'superiority', t);
    if (!sky.why) { expect(sky.guns.all).toBe(0); expect(line(sky, 'Flak')).toBeUndefined(); }
  });

  it('a count only: with other numbers for a squadron and a battery, every loss and every chance is what it was', () => {
    const g = west();
    const ts = targets(g).slice(0, 6);
    const facts = () => ts.map((t) => { const a = assessAir(g, fleetsOf(g, going(g, t)), 'bomb', t); const p = planRaid(g, 'bomb', t, going(g, t)); return [a.bomberLoss, a.gunLoss, a.D, a.Dpow, p.success, p.bomberLossN, p.fighterLossN]; });
    const was = facts();
    AIRWAR.count.squadron = { _default: 3 };
    AIRWAR.count.later = {};
    AIRWAR.count.guns = { base: 7, light: 7, heavy: 999 };
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
      expect(dn.gunsCount).toBe(guns.all ? `About ${guns.all.toLocaleString('en-US')} guns built here, ${guns.heavy.toLocaleString('en-US')} of them heavy, and the 60 light guns every target has.` : 'Only the 60 light guns every target has.');
      expect(dn.foeGroups).toBe(p.meets.length);
      expect(dn.foesAble).toBe(p.meets.reduce((n, x) => n + x.able, 0));
      expect(dn.foeSquadrons).toBe(p.meets.reduce((n, x) => n + squadronsOf(x.nat, x.able, turnToDate(g.turn).year), 0));
      if (dn.foeGroups) { some++; expect(dn.foesCount).toMatch(/^about [0-9,]+ fighter squadrons? in range$/); }
      else expect(dn.foesCount).toBe('');
    }
    expect(some).toBeGreaterThan(0);
  });

  it('the battle is handed the same count of guns, for the scene to stand round the target', () => {
    const g = west();
    const t = targets(g).find((x) => g.regions[x].fac > 0)!;
    g.flags[`wk:aa:${t}`] = 1;
    const tk = airTicket(g, 'bomb', t, going(g, t), 'GER').ticket!;
    expect(tk.place.flakGuns).toEqual({ light: 240, heavy: 120 });
    expect(tk.lines.some((l) => l.label === 'Flak' && l.value.startsWith('About 360 guns built here'))).toBe(true);
  });
});
