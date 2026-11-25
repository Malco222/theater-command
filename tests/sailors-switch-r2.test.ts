// The sailor rules' switch (release 2; the sea field's men in the water, naval-battles/sailors-r2): the flag 'sailors'
// that ticket.ts sailorsOn reads, set as the voyage log's is (voyage.ts setVoyageLog). The options page throws it with
// act('sailorsrule:on' / ':off'), TC.sailors(on) at any time; the view carries it as 'sailors'. Nothing sets it on its
// own: off, every war is the war without it, to the byte.
import { describe, expect, it } from 'vitest';
import { seaAhead } from '../src/battle/sea/handoff';
import { sailorsOn, setSailors } from '../src/battle/sea/ticket';
import { createGame } from '../src/campaign/game';
import { sail, seaBattles, zoneOf } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game } from '../src/campaign/types';
import { setVoyageLog, voyageOn } from '../src/campaign/voyage';
import { hashString } from '../src/core/rng';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (globalThis as unknown as { TC: Record<string, Call> }).TC;
const fp = (g: Game): string => hashString(saveGame(g)).toString(16);

/** A grand campaign as Germany, at the player's first month (as the voyage log's test seats it). */
function germany(): Game {
  const g = startOpen(createGame(3, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;

describe('the sailor rules\' switch', () => {
  it('is off in a new war, in the browser and through the bridge, and in a save from before it', () => {
    const g = germany();
    expect(g.flags.open).toBe(1);
    expect(g.flags.sailors).toBeUndefined();
    expect(sailorsOn(g)).toBe(false);
    TC.start({ view: 'grand', human: 'GER', seed: 7 });
    const text = (TC.save() as { text: string }).text;
    expect(JSON.parse(text).flags.sailors).toBeUndefined();
    expect((TC.view() as { sailors: boolean }).sailors).toBe(false);
    // A grand campaign saved before the switch (no flag at all) plays on with it off.
    const old = JSON.parse(text);
    delete old.flags.sailors;
    TC.load(JSON.stringify(old));
    expect((TC.view() as { sailors: boolean }).sailors).toBe(false);
    const back = loadGame(JSON.stringify(old))!;
    expect(back.flags.open).toBe(1);
    expect(sailorsOn(back)).toBe(false);
  });

  it('setSailors turns them on and off, writing only its flag; off, the save is as one that never had them', () => {
    const g = germany();
    const before = saveGame(g);
    setSailors(g, true);
    expect(g.flags.sailors).toBe(1);
    expect(sailorsOn(g)).toBe(true);
    const on = JSON.parse(saveGame(g)), was = JSON.parse(before);
    expect(on.flags).toEqual({ ...was.flags, sailors: 1 });
    expect({ ...on, flags: null }).toEqual({ ...was, flags: null });
    setSailors(g, false);
    expect(sailorsOn(g)).toBe(false);
    expect('sailors' in g.flags).toBe(false);
    expect(saveGame(g)).toBe(before);
  });

  it('outside a grand campaign it does what the voyage log\'s switch does: the flag is written, and nothing is on', () => {
    for (const nat of ['GER', null]) {
      const a = createGame(3, nat), b = createGame(3, nat);
      expect(a.flags.open).toBeUndefined();
      setSailors(a, true);
      setVoyageLog(b, true);
      expect(a.flags.sailors).toBe(1);
      expect(b.flags.voyageLog).toBe(1);
      expect(sailorsOn(a)).toBe(false);
      expect(voyageOn(b)).toBe(false);
      setSailors(a, false);
      setVoyageLog(b, false);
      expect('sailors' in a.flags).toBe(false);
      expect('voyageLog' in b.flags).toBe(false);
    }
  });

  it("act('sailorsrule:on' / ':off') switches it, says so, and the view carries it; TC.sailors(on) too", () => {
    const g = germany();
    TC.load(saveGame(g));
    expect((TC.view() as { sailors: boolean }).sailors).toBe(false);
    const on = TC.act('sailorsrule:on') as { ok: boolean; note: string; view: { sailors: boolean } };
    expect(on.ok).toBe(true);
    expect(on.note).toBe('Sailor rules are on: in fleet actions fought live on the sea field, men go into the water when a ship goes down, and the side left holding the water picks up who it can.');
    expect(on.view.sailors).toBe(true);
    expect((TC.view() as { sailors: boolean }).sailors).toBe(true);
    expect(JSON.parse((TC.save() as { text: string }).text).flags.sailors).toBe(1);
    const off = TC.act('sailorsrule:off') as { ok: boolean; note: string; view: { sailors: boolean } };
    expect(off.ok).toBe(true);
    expect(off.note).toBe('Sailor rules are off.');
    expect(off.view.sailors).toBe(false);
    expect(JSON.parse((TC.save() as { text: string }).text).flags.sailors).toBeUndefined();
    expect(TC.sailors(true)).toEqual({ on: true });
    expect((TC.view() as { sailors: boolean }).sailors).toBe(true);
    expect(TC.sailors(false)).toEqual({ on: false });
    expect((TC.view() as { sailors: boolean }).sailors).toBe(false);
    // Outside a grand campaign it says why nothing changed (the switch is not shown there: r2Switches is false).
    TC.start({ human: 'GER', seed: 7 });
    const plain = TC.act('sailorsrule:on') as { ok: boolean; note: string; view: { sailors: boolean; r2Switches: boolean } };
    expect(plain.view.r2Switches).toBe(false);
    expect(plain.view.sailors).toBe(false);
    expect(plain.note).toBe('Sailor rules work in the grand campaign only.');
  });

  it('a fleet action in a grand campaign carries men in the water on its ticket only with the switch on', () => {
    const ticketOf = (on: boolean) => {
      const g = germany();
      setSailors(g, on);
      const norw = zoneOf(g, 'norw');
      expect(sail(g, fleet(g, 'UK', 'Home Fleet'), norw, 'patrol')).toBe(true);
      expect(sail(g, fleet(g, 'GER', 'Battle Fleet'), norw, 'patrol')).toBe(true);
      const before = saveGame(g);
      const ahead = seaAhead(g, (c) => { seaBattles(c, 'UK'); });
      expect(saveGame(g)).toBe(before);
      expect(ahead.length).toBe(1);
      return ahead[0].ticket;
    };
    const off = ticketOf(false), on = ticketOf(true);
    expect('sailors' in off.place).toBe(false);
    expect(on.place.sailors).toEqual({ water: expect.stringMatching(/^(cold|temperate|warm)$/) });
    // Nothing else on the ticket moves.
    const { sailors, ...rest } = on.place;
    void sailors;
    expect(JSON.stringify({ ...on, place: rest })).toBe(JSON.stringify(off));
  });

  it('thrown on and back off, a war of a few months is the same to the byte as one never switched', () => {
    const a = germany(), b = germany();
    setSailors(b, true);
    setSailors(b, false);
    expect(fp(b)).toBe(fp(a));
    for (let k = 0; k < 4; k++) { month(a); month(b); }
    expect(a.turn).toBeGreaterThan(4);
    expect(fp(b)).toBe(fp(a));
    expect(saveGame(b)).toBe(saveGame(a));
  });
});
