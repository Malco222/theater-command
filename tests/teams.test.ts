// Support teams built in the campaign (release 2; Brian, 21 November 2026, 03:57Z: "for release 2 lets have these
// units be separate things to build outside of a normal infantry squad"): a machine-gun brigade and a mortar brigade,
// raised as artillery is, off unless the game's 'teams' flag is set. These tests hold that the switch off is the game
// as it always was, and that switched on they are built, named, priced once on the card, never go forward alone, and
// reach the field as team blocks.
import { describe, expect, it } from 'vitest';
import { offers } from '../src/campaign/arsenal';
import { cancelBuild, corpsKinds, deliver, orderCorps, specOf } from '../src/campaign/economy';
import { addCorps, aloneLine, assess, createGame, fights, limitAttackers, TEAMS_ALONE } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { TEAMS, isTeam, setTeams, teamsOn } from '../src/campaign/teams';
import { runTurnAI } from '../src/campaign/turn';
import { saveGame } from '../src/campaign/save';
import { landTicket } from '../src/battle/land/ticket';
import { LandBattle } from '../src/battle/land';
import type { Build, Game, Region } from '../src/campaign/types';

const grand = (seed = 3, human: string | null = null, on = true): Game => { const g = startOpen(createGame(seed, human)); if (on) setTeams(g, true); return g; };
const arrive = (g: Game, nat: string, b: Build | null): void => { expect(b).not.toBeNull(); b!.eta = g.turn; deliver(g, nat); };
/** A Polish province on the German border, with a Polish garrison in it and German ground next door. */
const front = (g: Game): { target: Region; from: Region } => {
  const target = g.regions.find((r) => r.ctrl === 'POL' && r.units.some((c) => c.nat === 'POL') && r.nb.some((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER'))!;
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER' && g.regions[j].owner === 'GER')!];
  return { target, from };
};

describe('switched off, the game is the one it always was', () => {
  it('no grand campaign has them unless asked: none on the build list, none can be ordered', () => {
    const g = grand(3, 'GER', false);
    expect(teamsOn(g)).toBe(false);
    expect(g.flags.teams).toBeUndefined();
    expect(corpsKinds(g)).not.toContain('mg');
    expect(specOf(g, 'mor')).toBeUndefined();
    g.nations.GER.pp = 500;
    expect(orderCorps(g, 'GER', 'mg', g.nations.GER.capital)).toBeNull();
    expect(offers(g, 'GER', g.nations.GER.capital).some((o) => o.key === 'corps:mg' || o.key === 'corps:mor')).toBe(false);
  });
  it('a historical scenario never has them, even with the flag', () => {
    const g = createGame(3, 'GER');
    setTeams(g, true);
    expect(teamsOn(g)).toBe(false);
    expect(specOf(g, 'mg')).toBeUndefined();
  });
  it('a computer war with them off saves byte for byte as one with no flag ever set', () => {
    const a = startOpen(createGame(7, null)), b = startOpen(createGame(7, null));
    setTeams(b, true); setTeams(b, false);
    for (let t = 0; t < 4; t++) { runTurnAI(a); runTurnAI(b); }
    expect(saveGame(b)).toBe(saveGame(a));
  });
});

describe('switched on: built like artillery, named apart', () => {
  it('both are on the build list and are ordered, paid for, delivered and named, the corps numbers untouched', () => {
    const g = grand(3, 'GER');
    const n = g.nations.GER;
    n.pp = 500;
    expect(corpsKinds(g)).toEqual(expect.arrayContaining(['mg', 'mor']));
    const no = n.corpsNo, pp = n.pp, at = n.capital;
    const b = orderCorps(g, 'GER', 'mg', at)!;
    expect(b.name).toBe('1st Machine-Gun Brigade');
    expect(b.cost).toBe(TEAMS.mg.cost);
    expect(n.pp).toBe(pp - TEAMS.mg.cost);
    expect(n.corpsNo).toBe(no);
    // Taken back the month it was ordered: the money and its number come back.
    expect(cancelBuild(g, 'GER', b.id)).toBe(true);
    expect(n.pp).toBe(pp);
    const again = orderCorps(g, 'GER', 'mg', at)!;
    expect(again.name).toBe('1st Machine-Gun Brigade');
    arrive(g, 'GER', again);
    const m = orderCorps(g, 'GER', 'mor', at)!;
    expect(m.name).toBe('1st Mortar Brigade');
    arrive(g, 'GER', m);
    const got = g.corps.filter((c) => c.nat === 'GER' && (c.kind === 'mg' || c.kind === 'mor'));
    expect(got.map((c) => [c.kind, c.divs])).toEqual([['mg', 1], ['mor', 1]]);
    expect(n.corpsNo).toBe(no);
  });

  it('they are no fighting corps: never forward alone, and beside the corps that fight each takes a place on the roads as any corps does', () => {
    const g = grand();
    const { target, from } = front(g);
    const mg = addCorps(g, 'GER', 'Test MG', 'mg', 1, from.id, {}), mor = addCorps(g, 'GER', 'Test Mortars', 'mor', 1, from.id, {});
    expect(fights(mg)).toBe(false);
    expect(limitAttackers(g, [mg, mor], target)).toEqual([]);
    expect(aloneLine([mg, mor])).toBe(TEAMS_ALONE);
    // Enough infantry to fill the roads: no team rides past a full frontage (the design director, 21:33Z).
    const inf = Array.from({ length: 5 }, (_, i) => addCorps(g, 'GER', `Test Infantry ${i}`, 'inf', 3, from.id, {}));
    const without = limitAttackers(g, inf, target), withTeams = limitAttackers(g, [...inf, mg, mor], target);
    expect(withTeams.map((c) => c.id)).toEqual(without.map((c) => c.id));
    expect(withTeams.some(isTeam)).toBe(false);
    // With a place to spare on the roads, a team takes it beside the infantry.
    const one = limitAttackers(g, [inf[0], mg], target);
    expect(one.map((c) => c.id)).toEqual([inf[0].id, mg.id]);
    // And it takes it as any corps would: in a full road the place is a corps' place, never one more.
    const room = without.length;
    const short = limitAttackers(g, [...inf.slice(0, room - 1), mg, mor], target);
    expect(short).toHaveLength(room);
    expect(short.filter(isTeam)).toHaveLength(1);
  });

  it('on the card: the machine guns stiffen a defence against men on foot; the mortars break up its fieldworks', () => {
    const g = grand();
    const { target, from } = front(g);
    for (const d of target.units) { d.dug = true; d.dugFor = 1; }
    const inf = addCorps(g, 'GER', 'Test Infantry', 'inf', 3, from.id, {});
    const mor = addCorps(g, 'GER', 'Test Mortars', 'mor', 1, from.id, {});
    const plain = assess(g, [inf], target), withMor = assess(g, [inf, mor], target);
    expect(withMor.faced!.worksCut).toBeGreaterThan(plain.faced!.worksCut);
    expect(withMor.A / withMor.D).toBeGreaterThan(plain.A / plain.D);
    // The defence's own: a machine-gun brigade beside the garrison.
    const before = assess(g, [inf], target);
    addCorps(g, 'POL', 'Test Polish MG', 'mg', 1, target.id, {});
    const after = assess(g, [inf], target);
    expect(after.D).toBeGreaterThan(before.D);
    expect(after.lines.some((l) => l.label === 'Machine-gun brigades')).toBe(true);
    // Losses: a team in the attack pays as the guns do.
    expect(withMor.lines.find((l) => l.label === 'Who pays')?.value ?? '').not.toMatch(/mortar/);
  });

  it('caught alone, they are overrun as guns are', () => {
    const g = grand();
    const { target, from } = front(g);
    for (const c of [...target.units]) c.dead = true;
    target.units = target.units.filter((c) => !c.dead);
    addCorps(g, 'POL', 'Test Polish MG', 'mg', 1, target.id, {});
    const inf = addCorps(g, 'GER', 'Test Infantry', 'inf', 3, from.id, {});
    const a = assess(g, [inf], target);
    expect(a.overrun).toBe(true);
  });

  it('reach the field as team blocks that fire from the rear', () => {
    const g = grand(3, 'GER');
    const { target, from } = front(g);
    const inf = addCorps(g, 'GER', 'Test Infantry', 'inf', 3, from.id, {});
    const mg = addCorps(g, 'GER', 'Test MG', 'mg', 1, from.id, {}), mor = addCorps(g, 'GER', 'Test Mortars', 'mor', 1, from.id, {});
    for (const c of [inf, mg, mor]) { c.ap = c.apMax; }
    const t = landTicket(g, [inf, mg, mor], target);
    expect(t).not.toBeNull();
    const b = new LandBattle(t!);
    const teams = b.units.filter((u) => u.team);
    expect(teams.map((u) => [u.side, u.team, u.kind]).sort()).toEqual([['att', 'mg', 'art'], ['att', 'mor', 'art']]);
  });

  it('a computer war with them on: the staffs build them, and the war runs', () => {
    const g = grand(5, null);
    for (let t = 0; t < 14; t++) runTurnAI(g);
    const teams = g.corps.filter((c) => c.kind === 'mg' || c.kind === 'mor');
    expect(teams.length).toBeGreaterThan(0);
    expect(teams.every((c) => /^\d+(st|nd|rd|th) (Machine-Gun|Mortar) Brigade$/.test(c.name))).toBe(true);
  });
});
