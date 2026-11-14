// The air war as its own game (the grand campaign): airfields on the map, air groups of one kind, standing orders,
// raids planned from the target with the odds shown first, bombers meeting up, escorts, enemy fighters rising to
// meet every raid, reconnaissance, and the pages the 3D game reads. The historical scenarios keep the air war they had.
import { describe, expect, it } from 'vitest';
import airwar from '../data/campaign/airwar.json';
import { AIR, AIRWAR, airFleetsOf, airUpkeep, airWar, airfieldsHold, crowding, hasAirfield, whyNotBase } from '../src/campaign/air';
import { airPage, airAct } from '../src/campaign/airpages';
import { airfieldCard, airfields, airGroups, fieldWord, groupJobs, groupRow, planRaid, raidKinds, raidTargets, setGroupJob } from '../src/campaign/airwar';
import { engineersMonth } from '../src/campaign/engineers';
import { addCorps, allied, createGame } from '../src/campaign/game';
import { SIGHT, sightOf } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { airfieldAt, airfieldsSeen, beginHuman, endHuman, engineerJobs, flownRaids, humanUp, myAirGroups, orderAirBase, orderEngineers, orderGroupJob, orderRaid, orderScout, previewRaid, raidMenu, raidTargetsFor, stepAI } from '../src/campaign/turn';
import type { AirFleet, Game } from '../src/campaign/types';

const grand = (human: string | null = 'GER', seed = 7): Game => startOpen(createGame(seed, human));
const group = (g: Game, id: number): AirFleet => g.airFleets.find((f) => f.id === id)!;
const mine = (g: Game, kind: 'ftr' | 'bmr', nat = g.human!): AirFleet[] => airFleetsOf(g, nat).filter((f) => f.kind === kind && f.ftr + f.bmr > 0);
const sum = (fs: AirFleet[], k: 'ftr' | 'bmr'): number => fs.reduce((s, f) => s + f[k], 0);
/** A province the enemy's fighters can reach, for the kind of raid asked for, and the bombers and fighters that can fly it. */
function contested(g: Game, kind: 'bomb' | 'ground' = 'bomb'): { target: number; bombers: number[]; fighters: number[] } {
  for (const t of raidTargetsFor(g, kind)) {
    const p = previewRaid(g, kind, t.id);
    const bombers = p.seats.filter((s) => s.kind === 'bmr' && !s.why).map((s) => s.id);
    const fighters = p.seats.filter((s) => s.kind === 'ftr' && !s.why).map((s) => s.id);
    if (p.meets.length && bombers.length >= 2 && fighters.length) return { target: t.id, bombers, fighters };
  }
  throw new Error('no contested target');
}

describe('the air war data', () => {
  it('names nine kinds of raid, sizes airfields in steps and numbers the groups of each nation', () => {
    expect(airwar.raids.kinds.map((k) => k.key)).toEqual(['bomb', 'night', 'ground', 'line', 'town', 'superiority', 'strike', 'fleet', 'harbour', 'recon']);
    for (const k of airwar.raids.kinds) { expect(k.label.length).toBeGreaterThan(5); expect(k.blurb.length).toBeGreaterThan(30); expect(['bmr', 'any']).toContain(k.needs); }
    const S = airwar.fields.sizes;
    expect(S[0]).toBe(0);
    for (let i = 1; i < S.length; i++) expect(S[i]).toBeGreaterThan(S[i - 1]);
    expect(airwar.fields.least).toBeLessThan(S[1]);
    for (const row of Object.values(airwar.groups.names) as { ftr: string; bmr: string }[]) { expect(row.ftr).toMatch(/\{n(th)?\}/); expect(row.bmr).toMatch(/\{n(th)?\}/); expect(row.ftr).not.toBe(row.bmr); }
    expect(fieldWord(500).word).toBe('Landing ground');
    expect(fieldWord(1000).word).toBe('Airfield');
    expect(fieldWord(1500).word).toBe('Air base');
    expect(fieldWord(2500).word).toBe('Great air base');
    expect(fieldWord(2500).size).toBeGreaterThan(fieldWord(1500).size);
  });
});

describe('a historical scenario', () => {
  it('keeps the air war it always had: whole air fleets, no airfields to build, no raid planner', () => {
    const g = createGame(3, 'GER');
    expect(airWar(g)).toBe(false);
    expect(g.airFleets.every((f) => f.kind === undefined && f.parent === undefined)).toBe(true);
    expect(airfields(g, 'GER')).toEqual([]);
    expect(airfieldCard(g, g.nations.GER.capital, 'GER')).toBeNull();
    expect(raidKinds(g, 'GER').every((k) => k.targets === 0)).toBe(true);
    expect(raidTargets(g, 'bomb', 'GER')).toEqual([]);
    expect(planRaid(g, 'bomb', g.byName.Warsaw.id, undefined, 'GER').why).toBeTruthy();
    expect(airPage(g, 'airfield', g.nations.GER.capital)).toBeNull();
    expect(airAct(g, 'job:1:rest')).toBeNull();
    // Any region can base an air fleet, as before.
    expect(hasAirfield(g, g.byName.Hesse.id)).toBe(true);
  });
});

describe('airfields', () => {
  it('begin the war at every capital, city and town, and wherever history bases an air fleet', () => {
    const g = grand();
    expect(airWar(g)).toBe(true);
    const all = airfields(g, null);
    expect(all.length).toBeGreaterThan(100);
    for (const r of g.regions) if (r.cap) { expect(hasAirfield(g, r.id), r.name).toBe(true); expect(airfieldsHold(g, r.id), r.name).toBeGreaterThanOrEqual(1500); }
    // Every group on land has an airfield under it (a carrier's wing has its decks).
    for (const f of g.airFleets) if (f.ship === undefined) expect(hasAirfield(g, f.base), `${f.name} at ${g.regions[f.base].name}`).toBe(true);
    // Open country has none.
    expect(g.regions.some((r) => !hasAirfield(g, r.id))).toBe(true);
    expect(all.some((a) => a.size === 1)).toBe(true);
    expect(all.some((a) => a.size === 2)).toBe(true);
    for (const a of all) { expect(a.holds).toBeGreaterThanOrEqual(500); expect(a.word).toBe(fieldWord(a.holds).word); }
  });

  it('are seen by everyone, but what flies from an enemy one is hidden by the fog of war', () => {
    const g = grand('GER');
    const seen = airfieldsSeen(g);
    const own = seen.filter((a) => a.side === 'own');
    expect(own.length).toBeGreaterThan(10);
    for (const a of own) expect(a.groups).toBeGreaterThanOrEqual(0);
    const foe = seen.filter((a) => a.side === 'foe');
    expect(foe.length).toBeGreaterThan(5);
    const dark = foe.filter((a) => a.groups < 0);
    expect(dark.length).toBeGreaterThan(0);
    for (const a of dark) { expect(a.aircraft).toBe(-1); const card = airfieldAt(g, a.region)!; expect(card.hidden).toBe(true); expect(card.rows).toEqual([]); expect(card.note).toMatch(/not known/); }
    // With nobody at the controls everything is in the open.
    expect(airfields(grand(null), null).every((a) => a.groups >= 0)).toBe(true);
  });

  it('are the only ground aircraft can be based on, and a crowded one flies fewer', () => {
    const g = grand('GER');
    const f = mine(g, 'bmr')[0];
    const bare = g.regions.find((r) => r.ctrl === 'GER' && !hasAirfield(g, r.id))!;
    expect(whyNotBase(g, f, bare)).toMatch(/no airfield/i);
    expect(orderAirBase(g, f.id, bare.id)).toBe(false);
    const card = airfieldAt(g, bare.id)!;
    expect(card.field).toBeNull();
    expect(card.note).toMatch(/no airfield/i);
    expect(card.build).toMatch(/engineer/i);
    // Too many aircraft for the field: only a share of each group can fly.
    expect(crowding(g, f)).toBe(1);
    const holds = airfieldsHold(g, f.base);
    f.bmr = holds * 2; f.est.bmr = f.bmr;
    expect(crowding(g, f)).toBeLessThan(0.75);
    expect(airfieldAt(g, f.base)!.field!.crowded).toBe(true);
    expect(myAirGroups(g).find((x) => x.id === f.id)!.crowd).toBeLessThan(0.75);
  });

  it('can be laid out by engineers, a level a month as far as field engineers can build', () => {
    const g = grand('GER');
    const bare = g.regions.find((r) => r.ctrl === 'GER' && r.owner === 'GER' && !hasAirfield(g, r.id))!;
    const sap = addCorps(g, 'GER', 'Test Engineer Corps', 'eng', 2, bare.id, {});
    const job = engineerJobs(g, sap).find((j) => j.key === 'airfield')!;
    expect(job.label).toBe('Lay out an airfield');
    expect(job.why).toBeNull();
    expect(orderEngineers(g, sap, 'airfield')).toBe(true);
    expect(hasAirfield(g, bare.id)).toBe(false);
    g.turn++; engineersMonth(g, 'GER');
    expect(hasAirfield(g, bare.id)).toBe(true);
    expect(airfieldsHold(g, bare.id)).toBe(500);
    expect(airfieldAt(g, bare.id)!.field!.word).toBe('Landing ground');
    expect(whyNotBase(g, mine(g, 'ftr')[0], bare) ?? '').not.toMatch(/no airfield/i);
    // A second month enlarges it, and then the engineers have done all they can.
    expect(orderEngineers(g, sap, 'airfield')).toBe(true);
    g.turn++; engineersMonth(g, 'GER');
    expect(airfieldsHold(g, bare.id)).toBe(1000);
    expect(AIRWAR.fields.engineers).toBe(2);
    expect(engineerJobs(g, sap).find((j) => j.key === 'airfield')!.why).toBeTruthy();
    expect(orderEngineers(g, sap, 'airfield')).toBe(false);
  });
});

describe('air groups', () => {
  it('split every air fleet into groups of one kind, with every aircraft kept', () => {
    const whole = createGame(7, 'GER');
    const g = grand('GER');
    for (const nat of new Set(whole.airFleets.map((f) => f.nat))) {
      // The carriers' own wings are new aircraft, not a share of history's air fleets.
      const before = whole.airFleets.filter((f) => f.nat === nat), after = g.airFleets.filter((f) => f.nat === nat && f.ship === undefined);
      expect(sum(after, 'ftr'), nat).toBe(sum(before, 'ftr'));
      expect(sum(after, 'bmr'), nat).toBe(sum(before, 'bmr'));
      expect(after.reduce((s, f) => s + f.est.ftr + f.est.bmr, 0), nat).toBe(before.reduce((s, f) => s + f.est.ftr + f.est.bmr, 0));
      expect(after.length, nat).toBeGreaterThanOrEqual(before.length);
    }
    expect(new Set(g.airFleets.map((f) => f.id)).size).toBe(g.airFleets.length);
    for (const f of g.airFleets) {
      expect(f.kind === 'ftr' || f.kind === 'bmr', f.name).toBe(true);
      expect(f.kind === 'ftr' ? f.bmr : f.ftr, f.name).toBe(0);
      // A carrier's wing is as large as its decks, and was never part of a land command.
      if (f.ship !== undefined) continue;
      expect(f.ftr + f.bmr, f.name).toBeLessThanOrEqual(AIRWAR.groups.most);
      expect(f.parent, f.name).toBeTruthy();
      expect(f.share, f.name).toBeGreaterThan(0);
    }
    // No two groups of a nation share a name.
    for (const nat of new Set(g.airFleets.map((f) => f.nat))) { const names = g.airFleets.filter((f) => f.nat === nat).map((f) => f.name); expect(new Set(names).size, nat).toBe(names.length); }
  });

  it('are numbered the way each air force numbered its own, and a small command keeps its name', () => {
    const g = grand('GER');
    expect(mine(g, 'ftr').every((f) => /^Jagdgeschwader \d+$/.test(f.name))).toBe(true);
    expect(mine(g, 'bmr').every((f) => /^Kampfgeschwader \d+$/.test(f.name))).toBe(true);
    expect(g.airFleets.some((f) => f.nat === 'FRA' && /^Groupe de Chasse \d+$/.test(f.name))).toBe(true);
    const uk = g.airFleets.filter((f) => f.nat === 'UK').map((f) => f.name);
    expect(uk).toContain('No. 11 Group');
    expect(uk.some((n) => /^No\. \d+ Bomber Group$/.test(n))).toBe(true);
    // The first group of a fleet keeps its commander.
    const led = whole_leaders(createGame(7, 'GER'));
    for (const name of led) expect(g.airFleets.filter((f) => f.leader?.name === name).length, name).toBe(1);
    const row = myAirGroups(g).find((x) => x.kind === 'bmr')!;
    expect(row.command).toMatch(/Luftflotte/);
    expect(row.range).toBe(900);
    expect(row.job).toBe('patrol');
  });

  it('are each kept at their share of their command month after month', () => {
    const g = grand('GER');
    const before = new Map(mine(g, 'bmr').concat(mine(g, 'ftr')).map((f) => [f.id, f.est.ftr + f.est.bmr]));
    for (let m = 0; m < 3; m++) { g.turn++; airUpkeep(g, 'GER'); }
    for (const f of airFleetsOf(g, 'GER')) {
      const row = AIR.fleets.find((x) => x.nat === 'GER' && x.name === f.parent)!;
      expect(row, f.name).toBeDefined();
      expect(f.kind === 'ftr' ? f.est.bmr : f.est.ftr, f.name).toBe(0);
      // A command's strength may change with the months, but a group never jumps to the whole of it.
      expect(f.est.ftr + f.est.bmr, f.name).toBeLessThanOrEqual(Math.max(AIRWAR.groups.most, (before.get(f.id) ?? 0) * 1.6));
      expect(f.est.ftr + f.est.bmr, f.name).toBeGreaterThan(0);
    }
  });

  it('survive a save, and a grand campaign saved before the air war is brought into it on loading', () => {
    const g = grand('GER');
    const f = mine(g, 'ftr')[0], b = mine(g, 'bmr').find((x) => x.base === f.base)!;
    expect(orderGroupJob(g, f.id, `escort:${b.id}`)).toBe(true);
    const back = loadGame(saveGame(g))!;
    expect(airWar(back)).toBe(true);
    expect(back.airFleets.length).toBe(g.airFleets.length);
    expect(group(back, f.id).escorts).toBe(b.id);
    expect(group(back, f.id).kind).toBe('ftr');
    expect(airfields(back, 'GER').length).toBe(airfields(g, 'GER').length);
    // An older save: free play, whole fleets, no airfields marked.
    const old = createGame(7, 'GER');
    old.flags.open = 1;
    expect(airWar(old)).toBe(false);
    const up = loadGame(saveGame(old))!;
    expect(airWar(up)).toBe(true);
    expect(up.airFleets.every((x) => x.kind !== undefined)).toBe(true);
    const ashore = up.airFleets.filter((x) => x.ship === undefined);
    expect(sum(ashore, 'ftr')).toBe(sum(old.airFleets, 'ftr'));
    expect(ashore.every((x) => hasAirfield(up, x.base))).toBe(true);
    // And its carriers are given their wings.
    expect(up.airFleets.some((x) => x.ship !== undefined)).toBe(true);
  });
});

/** The names of the commanders of a game's air fleets. */
function whole_leaders(g: Game): string[] {
  return g.airFleets.filter((f) => f.leader).map((f) => f.leader!.name);
}

describe('standing orders', () => {
  it('give fighters three jobs, each with what it would mean: patrol, escort a named bomber group, rest', () => {
    const g = grand('GER');
    const f = mine(g, 'ftr')[0];
    const jobs = groupJobs(g, f.id);
    expect(jobs[0].key).toBe('patrol');
    expect(jobs[0].label).toBe('Patrol the sky');
    expect(jobs[0].on).toBe(true);
    expect(jobs[0].forecast).toMatch(/Meets any raid within \d+ km/);
    expect(jobs[jobs.length - 1].key).toBe('rest');
    const escorts = jobs.filter((j) => j.key.startsWith('escort:'));
    expect(escorts.length).toBeGreaterThan(0);
    for (const j of escorts) { expect(j.why).toBeNull(); expect(j.label).toMatch(/^Escort the Kampfgeschwader \d+$/); expect(j.forecast).toMatch(/Sticks with/); expect(group(g, Number(j.key.split(':')[1])).kind).toBe('bmr'); }
    // Bombers have no escort job: they stand ready, or rest.
    expect(groupJobs(g, mine(g, 'bmr')[0].id).map((j) => j.key)).toEqual(['patrol', 'rest']);
    expect(groupJobs(g, mine(g, 'bmr')[0].id)[0].label).toBe('Support the armies');
    // Nobody else's groups take the player's orders.
    const fra = g.airFleets.find((x) => x.nat === 'FRA')!;
    expect(groupJobs(g, fra.id)).toEqual([]);
    expect(setGroupJob(g, fra.id, 'rest')).toBe(false);
    expect(orderGroupJob(g, fra.id, 'rest')).toBe(false);
  });

  it('an escort sticks with its bombers: it is on any day raid of theirs it can reach, and patrols until then', () => {
    const g = grand('GER');
    const { target, bombers, fighters } = contested(g);
    const b = bombers[0];
    // A fighter group near enough to be given the job.
    const f = fighters.find((id) => groupJobs(g, id).some((j) => j.key === `escort:${b}` && !j.why));
    expect(f, 'a fighter group within reach of the bombers').toBeDefined();
    expect(orderGroupJob(g, f!, `escort:${b}`)).toBe(true);
    expect(group(g, f!).escorts).toBe(b);
    expect(group(g, f!).mission).toBe('support');
    const row = groupRow(g, group(g, f!));
    expect(row.job).toBe('escort');
    expect(row.escorts).toBe(b);
    expect(groupJobs(g, f!).find((j) => j.key === `escort:${b}`)!.on).toBe(true);
    expect(groupJobs(g, f!)[0].on).toBe(false);
    // Planning a raid for the bombers alone brings the escort with them, and it cannot be left out.
    const p = previewRaid(g, 'bomb', target, [b]);
    expect(p.picked).toContain(f);
    const seat = p.seats.find((s) => s.id === f)!;
    expect(seat.locked).toBe(true);
    expect(p.fighters).toBeGreaterThan(0);
    // By night nobody escorts.
    expect(previewRaid(g, 'night', target, [b]).picked).toEqual([b]);
    // Back on patrol, it is the planner's to add or leave.
    expect(orderGroupJob(g, f!, 'patrol')).toBe(true);
    expect(group(g, f!).escorts).toBeUndefined();
    expect(previewRaid(g, 'bomb', target, [b]).picked).toEqual([b]);
  });

  it('a group at rest does not fly and does not meet raids', () => {
    const g = grand('GER');
    const { target, bombers } = contested(g);
    const meets = previewRaid(g, 'bomb', target, [bombers[0]]).meets;
    expect(meets.length).toBeGreaterThan(0);
    const foe = g.airFleets.find((x) => x.name === meets[0].name && x.nat === meets[0].nat)!;
    foe.mission = 'rest';
    expect(previewRaid(g, 'bomb', target, [bombers[0]]).meets.some((m) => m.name === foe.name)).toBe(false);
    const f = mine(g, 'ftr')[0];
    expect(orderGroupJob(g, f.id, 'rest')).toBe(true);
    expect(f.mission).toBe('rest');
    expect(groupRow(g, f).job).toBe('rest');
    const seat = previewRaid(g, 'superiority', target).seats.find((s) => s.id === f.id)!;
    expect(seat.why).toBeTruthy();
  });
});

describe('the raid planner', () => {
  it('offers every kind of raid with its targets, nearest first, and nothing to a nation at peace', () => {
    const g = grand('GER');
    const menu = raidMenu(g);
    expect(menu.map((k) => k.key)).toEqual(['bomb', 'night', 'ground', 'line', 'town', 'superiority', 'strike', 'fleet', 'harbour', 'recon']);
    // A strike on a fleet needs a fleet found at sea, and bombing the line needs troops seen dug in: there may be none in reach in the first month.
    for (const k of menu) if (k.key !== 'fleet' && k.key !== 'harbour' && k.key !== 'line') { expect(k.targets, k.key).toBeGreaterThan(0); expect(k.why, k.key).toBeNull(); }
    for (const kind of ['bomb', 'ground', 'superiority', 'strike'] as const) {
      const ts = raidTargetsFor(g, kind);
      for (let i = 1; i < ts.length; i++) expect(ts[i].km).toBeGreaterThanOrEqual(ts[i - 1].km);
      for (const t of ts) { expect(t.groups).toBeGreaterThan(0); expect(t.sea).toBe(kind === 'strike'); }
    }
    // Troops are raided only where enemy troops are seen to stand.
    for (const t of raidTargetsFor(g, 'ground')) expect(g.regions[t.id].units.some((u) => !u.dead && g.nations.GER.atWar.includes(u.nat)), t.name).toBe(true);
    // Led by one group, the list is what that group reaches, measured from its own airfield.
    const f = mine(g, 'bmr')[0];
    const led = raidTargetsFor(g, 'bomb', f.id);
    expect(led.length).toBeGreaterThan(0);
    expect(led.length).toBeLessThanOrEqual(raidTargetsFor(g, 'bomb').length);
    for (const t of led) expect(t.km).toBeLessThanOrEqual(900);
    // Italy is at peace in September 1939.
    const it_ = grand('ITA');
    expect(raidMenu(it_).every((k) => k.targets === 0 && !!k.why)).toBe(true);
    expect(raidTargetsFor(it_, 'bomb')).toEqual([]);
  });

  it('proposes the bombers of the nearest airfield, and says what it expects before anything flies', () => {
    const g = grand('GER');
    const { target } = contested(g);
    const p = previewRaid(g, 'bomb', target);
    expect(p.why).toBeNull();
    expect(p.title).toMatch(/^Day bombing: /);
    expect(p.picked.length).toBeGreaterThan(0);
    const going = p.seats.filter((s) => s.picked);
    expect(going.every((s) => s.kind === 'bmr')).toBe(true);
    expect(new Set(going.map((s) => s.base)).size).toBe(1);
    expect(p.bombers).toBe(going.reduce((s, x) => s + x.aircraft, 0));
    expect(p.success).toBeGreaterThanOrEqual(0);
    expect(p.success).toBeLessThanOrEqual(1);
    expect(p.verdict.length).toBeGreaterThan(5);
    expect(p.bomberLoss).toBeGreaterThan(0);
    expect(p.bomberLossN).toBe(Math.round(p.bombers * p.bomberLoss));
    expect(p.effects.length).toBeGreaterThan(0);
    expect(p.lines.length).toBeGreaterThan(2);
    // Seats: those that can go first, and every one that cannot says why.
    const firstCannot = p.seats.findIndex((s) => s.why);
    if (firstCannot >= 0) expect(p.seats.slice(firstCannot).every((s) => !!s.why)).toBe(true);
    // Fighters alone cannot bomb.
    const ftr = p.seats.find((s) => s.kind === 'ftr' && !s.why);
    if (ftr) expect(previewRaid(g, 'bomb', target, [ftr.id]).why).toBeTruthy();
    // Nothing has been ordered by looking.
    expect(g.airReports.length).toBe(0);
    expect(airFleetsOf(g, 'GER').every((f) => f.mission === 'support' && f.flew === undefined)).toBe(true);
  });

  it('enemy fighters in reach rise to meet the bombers: an escort saves bombers, and with no patrol up almost all come home', () => {
    const g = grand('GER');
    const { target, bombers, fighters } = contested(g);
    const alone = previewRaid(g, 'bomb', target, [bombers[0]]);
    expect(alone.meets.length).toBeGreaterThan(0);
    for (const m of alone.meets) { expect(m.able).toBeGreaterThan(0); expect(m.able).toBeLessThanOrEqual(m.ftr); expect(g.nations.GER.atWar).toContain(m.nat); }
    expect(alone.bomberLoss).toBeGreaterThan(0.1);
    expect(alone.bomberLoss).toBeLessThanOrEqual(AIRWAR.losses.most);
    expect(alone.foeLossN).toBe(0);
    const escorted = previewRaid(g, 'bomb', target, [bombers[0], ...fighters.slice(0, 2)]);
    expect(escorted.fighters).toBeGreaterThan(0);
    expect(escorted.bomberLoss).toBeLessThan(alone.bomberLoss);
    expect(escorted.foeLossN).toBeGreaterThan(0);
    expect(escorted.fighterLossN).toBeGreaterThan(0);
    expect(escorted.success).toBeGreaterThanOrEqual(alone.success);
    expect(escorted.offPatrol).toMatch(/patrol/);
    // The defenders stand down: nobody comes up.
    for (const f of g.airFleets) if (g.nations.GER.atWar.includes(f.nat)) f.mission = 'rest';
    const free = previewRaid(g, 'bomb', target, [bombers[0]]);
    expect(free.meets).toEqual([]);
    expect(free.bomberLoss).toBeLessThan(0.06);
    expect(free.bomberLoss).toBeLessThan(alone.bomberLoss / 4);
    expect(free.success).toBeGreaterThan(alone.success);
  });

  it('bombers that meet up from several airfields lose a smaller share than a group sent alone', () => {
    const g = grand('GER');
    const { target, bombers } = contested(g);
    const alone = previewRaid(g, 'bomb', target, [bombers[0]]);
    const many = previewRaid(g, 'bomb', target, bombers);
    expect(new Set(many.seats.filter((s) => s.picked).map((s) => s.base)).size).toBeGreaterThan(1);
    expect(many.bombers).toBeGreaterThan(alone.bombers);
    expect(many.bomberLoss).toBeLessThan(alone.bomberLoss);
    expect(many.massed).toMatch(/bomber groups/);
    expect(alone.massed).toBe('');
  });
});

describe('flying a raid', () => {
  it('is flown the moment it is ordered, and the groups that went are away for the month', () => {
    const g = grand('GER');
    const { target, bombers, fighters } = contested(g);
    const ids = [bombers[0], bombers[1], fighters[0]];
    const plan = previewRaid(g, 'bomb', target, ids);
    const h0 = g.headlines.length;
    const rep = orderRaid(g, 'bomb', target, plan.picked)!;
    expect(rep).not.toBeNull();
    expect(rep.mission).toBe('bomb');
    expect(rep.region).toBe(target);
    expect(rep.attNat).toBe('GER');
    expect(rep.att.length).toBe(plan.picked.length);
    expect(rep.def.length).toBe(plan.meets.length);
    // The report says who flew from where, and who rose from where, so the flight can be drawn again.
    expect(rep.att.map((x) => x.id).sort()).toEqual([...plan.picked].sort());
    for (const x of rep.att) { expect(x.base).toBe(group(g, x.id!).base); expect(x.fleet).toBe(group(g, x.id!).name); expect(x.lostFtr + x.lostBmr).toBeLessThanOrEqual(x.ftr + x.bmr); }
    for (const x of rep.def) { expect(group(g, x.id!).nat).toBe(x.nat); expect(x.base).toBe(group(g, x.id!).base); }
    const [replay] = flownRaids(g);
    expect(flownRaids(g).length).toBe(1);
    expect(replay).toMatchObject({ id: rep.id, kind: 'bomb', region: target, zone: -1, attNat: 'GER', x: g.regions[target].x, y: g.regions[target].y, won: rep.won });
    expect(replay.attackers.map((a) => a.id).sort()).toEqual([...plan.picked].sort());
    for (const a of replay.attackers) { expect(a.x).toBe(g.regions[a.base].x); expect(a.baseName).toBe(g.regions[a.base].name); expect(a.lostFtr + a.lostBmr).toBeLessThanOrEqual(a.ftr + a.bmr); }
    expect(replay.defenders.length).toBe(rep.def.length);
    expect(replay.defenders.every((d) => d.base >= 0 && g.regions[d.base].ctrl !== 'GER')).toBe(true);
    expect(flownRaids(g, g.turn + 1)).toEqual([]);
    expect(g.airReports[g.airReports.length - 1]).toBe(rep);
    expect(g.headlines.length).toBeGreaterThan(h0);
    // The losses come out near what the planner said: the luck of the day is 15% either way.
    const lost = rep.att.reduce((s, x) => s + x.lostBmr, 0);
    expect(Math.abs(lost - plan.bomberLossN)).toBeLessThanOrEqual(plan.bomberLossN * 0.3 + 6);
    for (const id of plan.picked) { expect(group(g, id).flew).toBe(g.turn); expect(group(g, id).mission).toBe('bomb'); }
    // They fly no second raid, take no new standing order, and cannot be picked.
    expect(orderRaid(g, 'bomb', target, [bombers[0]])).toBeNull();
    expect(previewRaid(g, 'bomb', target, [bombers[0]]).why).toBeTruthy();
    expect(previewRaid(g, 'bomb', target).seats.find((s) => s.id === bombers[0])!.why).toMatch(/flown/);
    expect(groupJobs(g, bombers[0])[0].why).toMatch(/flown its raid/);
    expect(myAirGroups(g).find((x) => x.id === bombers[0])!.flew).toBe(true);
    expect(myAirGroups(g).find((x) => x.id === bombers[0])!.job).toBe('raid');
    // The others can still go.
    if (bombers[2] !== undefined) expect(previewRaid(g, 'bomb', target, [bombers[2]]).why).toBeNull();
    // Next month they are back at their airfields, on patrol as before.
    g.turn++; airUpkeep(g, 'GER');
    for (const id of plan.picked) expect(group(g, id).mission).toBe('support');
    expect(myAirGroups(g).find((x) => x.id === bombers[0])!.flew).toBe(false);
    expect(previewRaid(g, 'bomb', target, [bombers[0]]).why).toBeNull();
  });

  it('a raid on troops cuts their organisation for the battle that follows, and costs them men', () => {
    const g = grand('GER');
    const { target, bombers, fighters } = contested(g, 'ground');
    const foes = g.regions[target].units.filter((u) => !u.dead && g.nations.GER.atWar.includes(u.nat));
    expect(foes.length).toBeGreaterThan(0);
    const before = foes.map((c) => ({ org: c.org, str: c.str }));
    const plan = previewRaid(g, 'ground', target, [...bombers.slice(0, 2), fighters[0]]);
    expect(plan.title).toMatch(/^Raid on troops: /);
    expect(plan.effects.join(' ')).toMatch(/organisation/);
    const rep = orderRaid(g, 'ground', target, plan.picked)!;
    expect(rep.mission).toBe('ground');
    expect(rep.result).toMatch(/organisation/);
    foes.forEach((c, i) => { expect(c.org, c.name).toBeLessThan(before[i].org); expect(c.str, c.name).toBeLessThan(before[i].str); expect(c.org, c.name).toBeGreaterThanOrEqual(before[i].org - AIRWAR.ground.max - 1e-9); });
    // Fighters alone can raid troops.
    const f2 = fighters.find((id) => group(g, id).flew !== g.turn);
    if (f2 !== undefined) { const p = previewRaid(g, 'ground', target, [f2]); if (!p.seats.find((s) => s.id === f2)!.why) expect(p.why).toBeNull(); }
  });

  it('is refused while it is not the player\'s month, and the war goes on round it', () => {
    const g = grand('UK');
    let guard = 0;
    if (!humanUp(g)) {
      const b = mine(g, 'bmr')[0];
      const t = raidTargets(g, 'night', 'UK')[0];
      expect(t).toBeDefined();
      expect(orderRaid(g, 'night', t.id, [b.id])).toBeNull();
      expect(b.flew).toBeUndefined();
    }
    // Three months of war with the player flying a raid each month: nothing breaks, and the groups keep coming home.
    for (let m = 0; m < 3; m++) {
      while (!humanUp(g) && guard++ < 200) stepAI(g);
      expect(humanUp(g)).toBe(true);
      beginHuman(g);
      expect(mine(g, 'bmr').every((f) => f.mission === 'support')).toBe(true);
      const t = raidTargetsFor(g, 'night')[0];
      if (t) { const p = previewRaid(g, 'night', t.id); if (!p.why) expect(orderRaid(g, 'night', t.id, p.picked)).not.toBeNull(); }
      endHuman(g);
    }
    expect(g.airReports.some((r) => r.attNat === 'UK' && r.mission === 'night')).toBe(true);
    expect(g.airFleets.every((f) => Number.isFinite(f.ftr) && Number.isFinite(f.bmr) && f.ftr >= 0 && f.bmr >= 0)).toBe(true);
  });
});

describe('reconnaissance', () => {
  it('lifts the fog over one province for the month, once a month for each group', () => {
    const g = grand('GER');
    const b = mine(g, 'bmr')[0];
    const ts = raidTargetsFor(g, 'recon', b.id).filter((x) => sightOf(g)[x.id] !== SIGHT.full);
    expect(ts.length).toBeGreaterThan(0);
    const t = ts[0];
    expect(sightOf(g)[t.id]).not.toBe(SIGHT.full);
    const aircraft = b.bmr;
    const res = orderScout(g, b.id, t.id);
    expect(res.ok).toBe(true);
    expect(res.text).toMatch(/photographed/);
    expect(res.lost).toBeLessThanOrEqual(AIRWAR.recon.aircraft);
    expect(b.bmr).toBe(aircraft - res.lost);
    expect(sightOf(g)[t.id]).toBe(SIGHT.full);
    expect(b.scouted).toBe(g.turn);
    // It keeps its patrol and can still fly its raid, but not a second reconnaissance.
    expect(b.mission).toBe('support');
    expect(b.flew).toBeUndefined();
    const again = orderScout(g, b.id, ts[1]?.id ?? t.id);
    expect(again.ok).toBe(false);
    expect(myAirGroups(g).find((x) => x.id === b.id)!.scouted).toBe(true);
    // The photographs are a month old next month.
    g.turn++;
    expect(sightOf(g)[t.id]).not.toBe(SIGHT.full);
    expect(myAirGroups(g).find((x) => x.id === b.id)!.scouted).toBe(false);
  });

  it('is flown only over ground that is hidden or holds an army not identified, and only by the player\'s own groups', () => {
    const g = grand('GER');
    const full = sightOf(g);
    // Ground in full view is worth a flight only for the armies there that are not yet identified (tests/reports.test.ts).
    for (const t of raidTargetsFor(g, 'recon')) if (full[t.id] === SIGHT.full) expect(g.regions[t.id].units.some((u) => !allied(g, u.nat, 'GER')), t.name).toBe(true);
    expect(raidTargetsFor(g, 'recon').some((t) => full[t.id] !== SIGHT.full)).toBe(true);
    const fra = g.airFleets.find((x) => x.nat === 'FRA')!;
    expect(orderScout(g, fra.id, raidTargetsFor(g, 'recon')[0].id).ok).toBe(false);
  });
});

describe('the pages the 3D game reads', () => {
  it('lead from an airfield to a group, a kind of raid, a target, a plan and the order, by the buttons alone', () => {
    const g = grand('GER');
    const b = mine(g, 'bmr')[0];
    const field = airPage(g, 'airfield', b.base)!;
    expect(field.title).toMatch(/^AIR BASE AT /);
    expect(field.top[0].act).toBe('open:raids');
    const row = field.sections[0].rows.find((r) => r.title === b.name)!;
    expect(row.buttons[0].act).toBe(`open:group:${b.id}`);
    const page = airPage(g, `group:${b.id}`)!;
    expect(page.sections.map((s) => s.heading)).toEqual(['STANDING ORDERS', 'RAIDS']);
    expect(page.sections[0].rows[0].buttons[0].act).toBe(`job:${b.id}:patrol`);
    expect(page.sections[0].rows[0].buttons[0].ok).toBe(false);
    const pick = page.sections[1].rows.find((r) => r.buttons[0].act === `open:raid:bomb:${b.id}`)!;
    expect(pick.buttons[0].ok).toBe(true);
    const targets = airPage(g, `raid:bomb:${b.id}`)!;
    expect(targets.sections[0].rows.length).toBeGreaterThan(0);
    const open = targets.sections[0].rows[0].buttons[0].act;
    expect(open).toMatch(new RegExp(`^open:plan:bomb:\\d+:${b.id}(\\.\\d+)*$`));
    const plan = airPage(g, open.replace(/^open:/, ''))!;
    expect(plan.headline).toMatch(/\d+%$/);
    expect(plan.note).toMatch(/Chance of success \d+%/);
    expect(plan.sections[0].heading).toBe('FLYING THE RAID');
    expect(plan.sections.some((s) => s.heading === 'THE BATTLE TICKET')).toBe(true);
    // Adding a group is another page; flying it is an action.
    const join = plan.sections.find((s) => s.heading === 'COULD JOIN IT');
    if (join) { const next = airPage(g, join.rows[0].buttons[0].act.replace(/^open:/, ''))!; expect(next.sections[0].rows.length).toBe(plan.sections[0].rows.length + 1); }
    const fly = plan.top[0];
    expect(fly.label).toBe('FLY THE RAID');
    expect(fly.ok).toBe(true);
    const res = airAct(g, fly.act)!;
    expect(res.ok).toBe(true);
    expect(res.report).toBe(g.airReports[g.airReports.length - 1].id);
    expect(res.fly!.from).toBe(b.base);
    expect(res.fly!.bombers).toBe(true);
    expect(res.open).toBe('airfield');
    expect(b.flew).toBe(g.turn);
    // It cannot be flown twice.
    expect(airAct(g, fly.act)!.ok).toBe(false);
    expect(airPage(g, `group:${b.id}`)!.sections[1].rows.every((r) => !r.buttons[0].ok)).toBe(true);
  });

  it('take standing orders, reconnaissance and moves as actions, and leave anything else alone', () => {
    const g = grand('GER');
    const f = mine(g, 'ftr')[0], b = mine(g, 'bmr')[0];
    expect(airPage(g, 'raids')!.sections[0].rows.length).toBe(10);
    expect(airAct(g, `job:${f.id}:rest`)).toMatchObject({ ok: true, open: `group:${f.id}` });
    expect(f.mission).toBe('rest');
    const esc = airPage(g, `group:${f.id}`)!.sections[0].rows.find((r) => r.buttons[0].act.startsWith(`job:${f.id}:escort:`))!;
    expect(airAct(g, esc.buttons[0].act)!.ok).toBe(true);
    expect(f.escorts).toBeGreaterThan(0);
    const scout = airPage(g, `scout:${b.id}`)!;
    expect(scout.sections[0].rows.length).toBeGreaterThan(0);
    expect(airAct(g, scout.sections[0].rows[0].buttons[0].act)!.ok).toBe(true);
    expect(b.scouted).toBe(g.turn);
    const move = airPage(g, `rebase:${b.id}`)!;
    const to = move.sections[0].rows.find((r) => r.buttons[0].ok)!;
    expect(airAct(g, to.buttons[0].act)!.ok).toBe(true);
    expect(g.regions[b.base].name).toBe(to.title);
    expect(airPage(g, 'ledger')).toBeNull();
    expect(airPage(g, 'group:99999')).toBeNull();
    expect(airAct(g, 'buy:tank')).toBeNull();
    // With nobody at the controls there is nothing to order.
    expect(airPage(grand(null), 'raids')).toBeNull();
  });
});

describe('the list of a nation\'s groups', () => {
  it('reads the same from the airfield and from the air force', () => {
    const g = grand('GER');
    const rows = airGroups(g, 'GER');
    expect(rows.length).toBe(airFleetsOf(g, 'GER').length);
    for (const x of rows) {
      const card = airfieldCard(g, x.base, 'GER')!;
      expect(card.rows.find((r) => r.id === x.id), x.name).toEqual(x);
      expect(x.mine).toBe(true);
      expect(x.aircraft).toBe(x.ftr + x.bmr);
      expect(x.baseName).toBe(g.regions[x.base].name);
      expect(x.doing.length).toBeGreaterThan(8);
    }
  });
});
