// The battle card's support teams, told through the bridge (release 2; src/campaign/teamfire.ts, TC.teamFire).
//
// One row of the 3D game's Battle rules card has two readers. The field brings every infantry force its own machine-gun
// and mortar teams when the rows `teams`, `facing` and `stand` are all on (land-bridge.ts landStartTicket; the design
// director, 25 November 2026 21:54Z and 26 November 01:02Z), and the campaign's
// battle card must price those teams exactly then, or the card and the field part. The 3D game tells the rules how the
// two rows stand when a war opens, when one is read in and when a row is clicked: TC.teamFire(on)
// (FCampaignSession::SyncBattleRules). These tests hold that call by the design director's rule that a switch off is the
// same war in the WHOLE save (25 November 2026, 16:42Z), and his three checks of 21:47Z: the brigades' switch alone and
// the card's key alone each leave the other untouched, and with both rows on the card's term and the field's teams are
// both there and the ticket carries the term.
import { describe, expect, it } from 'vitest';
import { isOwnTeam, ownTeamsLook } from '../src/battle/land/own-teams';
import { liveTicket } from '../src/battle/land/live';
import type { FieldTicket } from '../src/battle/core/types';
import type { LandPlace } from '../src/battle/land/field';
import { addCorps, createGame, relocate } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
type Ticket = FieldTicket<LandPlace>;
type Look = { ticket: Ticket; live?: { seen?: number[] } };
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
/** The whole war as the 3D game keeps it (TC.save's text): land, sea and air. */
const saved = (): string => (TC().save() as { text: string }).text;
const flags = (): Record<string, unknown> => (JSON.parse(saved()) as { flags: Record<string, unknown> }).flags;

/** A grand war in the player's first month, a Polish line of foot round Lodz and German foot in front of it, as a save. */
function war(): { save: string; ids: number[]; to: number } {
  const g: Game = startOpen(createGame(3, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  const target = g.byName['Lodz'];
  const from = g.regions[target.nb.find((j) => g.regions[j].ctrl === 'GER')!];
  for (const u of [...target.units]) relocate(g, u, g.byName['Lublin']);
  const def = [1, 2].map((i) => addCorps(g, 'POL', `Line ${i}`, 'inf', 3, target.id, {}));
  const att = [1, 2].map((i) => addCorps(g, 'GER', `Storm ${i}`, 'inf', 3, from.id, {}));
  for (const c of [...def, ...att]) { c.supplied = true; c.ap = c.apMax; }
  g.nations.GER.offStart.POL = g.turn - 6;
  g.nations.GER.lastFight.POL = g.turn - 1;
  return { save: saveGame(g), ids: att.map((c) => c.id), to: target.id };
}

describe('TC.teamFire: the card\'s key, told as the Battle rules card stands', () => {
  it('told off, as every start and load tells it with either row off, the whole save is the save it was', () => {
    const w = war();
    TC().load(w.save);
    const before = saved();
    expect((TC().teamFire(false) as { on: boolean }).on).toBe(false);
    expect(saved()).toBe(before);
    // told twice, and told anything that is not true: still off, still the same save
    TC().teamFire(false);
    TC().teamFire(undefined);
    expect(saved()).toBe(before);
    expect(flags().teamfire).toBeUndefined();
  });

  it('told on, the whole save differs by the one key; told off again, it is the save it was, byte for byte', () => {
    const w = war();
    TC().load(w.save);
    const before = saved();
    expect((TC().teamFire(true) as { on: boolean }).on).toBe(true);
    const on = JSON.parse(saved()) as { flags: Record<string, unknown> };
    expect(on.flags.teamfire).toBe(1);
    delete on.flags.teamfire;
    expect(on).toEqual(JSON.parse(before));
    expect((TC().teamFire(false) as { on: boolean }).on).toBe(false);
    expect(saved()).toBe(before);
  });

  it('a war saved with it on and read in with a row off is the war it would have been without it', () => {
    const w = war();
    TC().load(w.save);
    const before = saved();
    TC().teamFire(true);
    const kept = saved();
    // (the 3D game: Load, then the rows as they stand now, one of them off)
    TC().load(kept);
    expect(flags().teamfire).toBe(1);
    TC().teamFire(false);
    expect(saved()).toBe(before);
  });
});

describe('two switches, two keys: the brigades\' and the card\'s', () => {
  it('the brigades\' switch alone leaves the card\'s key untouched in the whole save', () => {
    const w = war();
    TC().load(w.save);
    expect((TC().act('teamsrule:on') as { ok: boolean }).ok).toBe(true);
    expect(flags().teams).toBe(1);
    expect(flags().teamfire).toBeUndefined();
    expect((TC().battleLook(w.ids, w.to) as Look).ticket.reckoning.teamFire).toBeUndefined();
    // and the card's key told on and off beside it leaves the brigades' save as it stood
    const brigades = saved();
    TC().teamFire(true);
    expect(flags().teams).toBe(1);
    TC().teamFire(false);
    expect(saved()).toBe(brigades);
  });

  it('the card\'s key alone leaves the brigades\' switch untouched in the whole save', () => {
    const w = war();
    TC().load(w.save);
    TC().teamFire(true);
    expect(flags().teamfire).toBe(1);
    expect(flags().teams).toBeUndefined();
    expect((TC().view() as { supportTeams: boolean }).supportTeams).toBe(false);
    // the brigades' switch turned on and off beside it leaves the card's key standing
    const card = saved();
    TC().act('teamsrule:on');
    expect(flags().teamfire).toBe(1);
    TC().act('teamsrule:off');
    expect(flags().teamfire).toBe(1);
    expect((JSON.parse(saved()) as { flags: Record<string, unknown> }).flags.teams).toBe((JSON.parse(card) as { flags: Record<string, unknown> }).flags.teams);
    // the ship list's brigades are another list's: taking them does not move the card's key either way
    TC().teamFire(false);
    TC().r2Ship(['teamsrule']);
    expect(flags().teamfire).toBeUndefined();
  });
});

describe('both rows on: the card and the field carry the same teams', () => {
  it('the card prices them, the ticket carries the term, and the field lays the teams at the power before it', () => {
    const w = war();
    TC().load(w.save);
    const plain = TC().battleLook(w.ids, w.to) as Look;
    expect(plain.ticket.reckoning.teamFire).toBeUndefined();
    TC().teamFire(true);
    const look = TC().battleLook(w.ids, w.to) as Look;
    const k = look.ticket.reckoning.teamFire!;
    expect(k).toBeDefined();
    for (const x of [k.att, k.def]) { expect(x).toBeGreaterThan(0); expect(x).toBeLessThanOrEqual(1); }
    // the card's own figures: each side by its term, and nothing else on the ticket's reckoning moved but the odds they make
    expect(look.ticket.reckoning.A).toBeCloseTo(plain.ticket.reckoning.A * k.att, 6);
    expect(look.ticket.reckoning.D).toBeCloseTo(plain.ticket.reckoning.D * k.def, 6);
    expect(look.ticket.seed).toBe(plain.ticket.seed);
    // the field, as the 3D game's land rules lay it with all three rows on (land-bridge.ts landStartTicket): the teams are
    // there, the three rules are on, and the blocks share out the power before the term, so it is never taken off twice
    const laid = ownTeamsLook(look) as Look;
    expect(laid.ticket.att.forces.some(isOwnTeam)).toBe(true);
    expect(laid.ticket.def.forces.some(isOwnTeam)).toBe(true);
    const on = liveTicket(laid.ticket, { me: null, teams: true, facing: true, stand: true }).sim;
    expect(on.teamsRule).toBe(true);
    expect(on.faceRule).toBe(true);
    expect(on.standRule).toBe(true);
    const power = (s: unknown): { att: number; def: number } => (s as { power: { att: number; def: number } }).power;
    const bare = liveTicket((ownTeamsLook(plain) as Look).ticket, { me: null, teams: true, facing: true, stand: true }).sim;
    expect(power(on).att).toBeCloseTo(power(bare).att, 6);
    expect(power(on).def).toBeCloseTo(power(bare).def, 6);
  });

  it('with either row off the 3D game tells it off: no term on the ticket and no team on the field', () => {
    const w = war();
    TC().load(w.save);
    TC().teamFire(false);
    const look = TC().battleLook(w.ids, w.to) as Look;
    expect(look.ticket.reckoning.teamFire).toBeUndefined();
    // `teams` alone lays no team block and turns on neither rule (the design director, 25 November 21:54Z)
    const alone = liveTicket(look.ticket, { me: null, teams: true }).sim;
    expect(alone.teamsRule).toBe(false);
    expect(alone.faceRule).toBe(false);
    // and with `facing` beside it but no `stand`, neither (the design director, 26 November 01:02Z)
    const two = liveTicket(look.ticket, { me: null, teams: true, facing: true }).sim;
    expect(two.teamsRule).toBe(false);
    expect(two.faceRule).toBe(false);
    expect(look.ticket.att.forces.some(isOwnTeam)).toBe(false);
  });
});
