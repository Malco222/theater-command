// The raid card weighs FLY IT YOURSELF afresh each time it is read from the rules (a second mend in his game, 27 November
// 2026: the design director's yes of 23:54Z, after the mend of her known-faults line of 23:13Z, the card says why FLY IT
// YOURSELF is not offered). The 3D game asks the bridge TC.airLook for the raid as picked and keeps the answer; it kept
// it for the pick alone, never asked again when the card was read again, not at a month's end, so the card could offer a
// raid the rules now refuse, or give a why no longer true. It now keeps it for the card read as well (CampaignPlayer-
// Controller CanFlyLive, FlyOfferRevision; pinned by Tools/fly_why/run.sh). This holds why, through the bridge's own
// calls as the 3D game makes them (TC.load, TC.page, TC.act, TC.endMonth, TC.airLook): the rules' answer for one and the
// same card, its fly act the same string on both reads, changes with the war, within a month and across one, both ways.
// (These hold the fault's why. The memo itself is the 3D game's, which no test here can run without the engine: its keys
// are held by the source pins of Tools/fly_why, section 6.)
import { describe, expect, it } from 'vitest';
import { ATOM, armedAt, safestSite, setAtom } from '../src/campaign/atom';
import { createGame } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import { beginHuman, humanUp, previewRaid, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;
type Look = { ticket: unknown; why?: string };
type Done = { ok: boolean; note: string; view: { turn: number } };

/** A British seat whose atomic bomb is ready this month (as atom-r2.test.ts's review mends build it). */
function armedUK(): Game {
  const g = startOpen(createGame(4, 'UK'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  setAtom(g, true);
  setFog(g, false);
  beginHuman(g);
  for (const id of [ATOM.rows.physics, ATOM.rows.chain, ATOM.rows.weapon, 'mediumbombers', 'heavybombers']) grantTech(g, 'UK', id);
  const s = (g.nations.UK.atom ??= {});
  s.plants = ATOM.plants.bill; s.done = g.turn; s.ready = g.turn; s.bombs = 1; s.site = safestSite(g, 'UK')!;
  return g;
}

type Card = { id: string; card: { bomb?: boolean; fly: { act: string; ok: boolean }; back: { act: string } | null } };

/**
 * That war, loaded into the bridge as the 3D game's LOAD does, and Hamburg's raid card read as the 3D game reads it
 * (TC.page): the staff's pick of groups, the very card THE ATOMIC PROJECT's CARRY THE BOMB HERE opens. The memo the mend
 * keys is that card's own fly act (Page.Card.Fly.Act), so each case reads the card again and holds that the fly act is
 * the same string while the rules' answer for it changes.
 */
function loaded(): { ham: number; id: string; read: () => Card; look: (c: Card) => Look } {
  const g = armedUK();
  const ham = g.byName.Hamburg.id;
  expect(TC.load(saveGame(g))).toBeTruthy();
  const pick = previewRaid(loadGame(TC.save().text)!, 'bomb', ham).picked;
  expect(pick.length).toBeGreaterThan(0);
  const id = `plan:bomb:${ham}:${pick.join('.')}`;
  const read = (): Card => TC.page(id, ham) as Card;
  /** TC.airLook for the card's own fly act ("raid:bomb:<target>:<ids>"), as CanFlyLive asks it. */
  const look = (c: Card): Look => {
    const [, kind, target, ids] = c.card.fly.act.split(':');
    return TC.airLook(kind, Number(target), ids.split('.').map(Number)) as Look;
  };
  return { ham, id, read, look };
}

const BOMB = 'A raid that carries the atomic bomb is flown by the staff.';

describe('one pick, two answers: why the raid card weighs FLY IT YOURSELF afresh each time it is read (second mend, 27 November)', () => {
  it('an offer the rules now refuse: Hamburg\'s card is given a ticket, then the bomb is armed for it; the same card is refused', () => {
    const { ham, id, read, look } = loaded();
    const first = read();
    expect(first.card.fly.ok).toBe(true);
    const open = look(first);
    expect(open.ticket).toBeTruthy();
    expect(open.why).toBeUndefined();
    // (THE ATOMIC PROJECT's CARRY THE BOMB HERE, as the 3D page sends it: it opens this very card again)
    const armed = TC.act(`atom:arm:${ham}`, ham) as Done & { open: string };
    expect(armed.ok).toBe(true);
    expect(armed.open).toBe(id);
    const again = read();
    // (the same fly act, so a memo kept for the pick would still offer FLY IT YOURSELF; FLY THE RAID still sends it)
    expect(again.card.fly.act).toBe(first.card.fly.act);
    expect(again.card.fly.ok).toBe(true);
    expect(again.card.bomb).toBe(true);
    const shut = look(again);
    expect(shut.ticket).toBeNull();
    expect(shut.why).toBe(BOMB);
  });

  it('within a month: the bomb armed for Hamburg, then taken back by the card\'s BACK ("The bomb stays in hand."); the same card is refused, then given a ticket', () => {
    const { ham, read, look } = loaded();
    expect((TC.act(`atom:arm:${ham}`, ham) as Done).ok).toBe(true);
    const armed = read();
    expect(armed.card.fly.ok).toBe(true);
    expect(look(armed).why).toBe(BOMB);
    // (the bomb card's BACK, the safe answer Enter and Esc give)
    expect(armed.card.back?.act).toBe('atom:back');
    const back = TC.act(armed.card.back!.act, -1) as Done;
    expect(back.ok).toBe(true);
    expect(back.note).toBe('The bomb stays in hand.');
    expect(armedAt(loadGame(TC.save().text)!, 'UK')).toBeNull();
    // (the same month, the same card read again: an answer kept for the pick would still say the why above)
    const again = read();
    expect(again.card.fly.act).toBe(armed.card.fly.act);
    expect(again.card.fly.ok).toBe(true);
    const open = look(again);
    expect(open.ticket).toBeTruthy();
    expect(open.why).toBeUndefined();
  });

  it('across a month: FLY THE RAID drops the bomb on Hamburg; the next month the same card is given a ticket', () => {
    const { ham, read, look } = loaded();
    expect((TC.act(`atom:arm:${ham}`, ham) as Done).ok).toBe(true);
    const armed = read();
    expect(look(armed).why).toBe(BOMB);
    // (FLY THE RAID: the card's own fly act, as the 3D card sends it)
    const flown = TC.act(armed.card.fly.act, ham) as Done;
    expect(flown.ok).toBe(true);
    const was = loadGame(TC.save().text)!;
    expect(was.nations.UK.atom?.bombs).toBe(0);
    expect(armedAt(was, 'UK')).toBeNull();
    // (END THE MONTH, as the 3D game's button sends it: the others' months, and his next one begun)
    TC.endMonth();
    const now = loadGame(TC.save().text)!;
    expect(now.turn).toBe(was.turn + 1);
    expect(humanUp(now)).toBe(true);
    const next = read();
    expect(next.card.fly.act).toBe(armed.card.fly.act);
    expect(next.card.fly.ok).toBe(true);
    const then = look(next);
    expect(then.ticket).toBeTruthy();
    expect(then.why).toBeUndefined();
  });
});
