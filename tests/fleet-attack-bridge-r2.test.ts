// The 3D game's doorway to a fleet sent straight at an enemy fleet (release 2, fleetattack.ts): the fleet in hand lists
// the enemy fleets it can be dropped on, the attack card gives the odds first, and SEND THEM IN fights it at once and
// hands back the sea it was fought in and the ships that went down, for the map to show. Off, none of it is offered.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { setFleetAttack } from '../src/campaign/fleetattack';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;

/** Germany at the table, in its first month of orders, with the fleet attack switched as asked and the fog lifted. */
function germany(on: boolean): { g: Game; battle: number; home: number } {
  const g = startOpen(createGame(3, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  setFleetAttack(g, on); // (on unless switched off since the 1.5.1 patch shipped it on)
  setFog(g, false);
  return { g, battle: g.fleets.find((f) => f.name === 'Battle Fleet')!.id, home: g.fleets.find((f) => f.name === 'Home Fleet')!.id };
}

describe('the fleet attack through the bridge', () => {
  it('off: the fleet in hand offers no attack, and the card is not there', () => {
    const { g, battle, home } = germany(false);
    TC().load(saveGame(g));
    const hand = TC().fleetInHand(battle) as { fleetAttack: boolean; attack: number[] };
    expect(hand.fleetAttack).toBe(false);
    expect(hand.attack).toEqual([]);
    const page = TC().page(`fleetattack:${battle}:${home}`) as { valid?: boolean; id?: string } | null;
    expect(page?.id === `fleetattack:${battle}:${home}`).toBe(false);
    const out = TC().act(`fleetattack:${battle}:${home}`) as { ok: boolean };
    expect(out.ok).toBe(false);
  });

  it('on: the card gives the odds first, and SEND THEM IN fights it at once', () => {
    const { g, battle, home } = germany(true);
    TC().load(saveGame(g));
    const hand = TC().fleetInHand(battle) as { fleetAttack: boolean; attack: number[] };
    expect(hand.fleetAttack).toBe(true);
    expect(hand.attack).toContain(home);
    const page = TC().page(`fleetattack:${battle}:${home}`) as { id: string; title: string; headline: string; top: { label: string; act: string; ok: boolean }[]; sections: { heading: string }[] };
    expect(page.id).toBe(`fleetattack:${battle}:${home}`);
    expect(page.title).toBe('ATTACK HOME FLEET?');
    expect(page.headline).toMatch(/to 1/);
    expect(page.top[0]).toMatchObject({ label: 'SEND THEM IN', act: `fleetattack:${battle}:${home}`, ok: true });
    expect(page.top[1].label).toBe('CALL IT OFF');
    expect(page.sections.map((x) => x.heading)).toEqual(expect.arrayContaining(['YOUR SHIPS', 'HIS SHIPS, AS FAR AS YOU KNOW', 'THE ODDS', 'IF IT IS FOUGHT']));
    // The fleet's own room lists the fleets it can attack.
    const room = TC().page(`fleet:${battle}`) as { sections: { heading: string; rows: { buttons: { act: string }[] }[] }[] };
    const targets = room.sections.find((x) => x.heading === 'ENEMY FLEETS TO ATTACK');
    expect(targets?.rows.some((r) => r.buttons.some((b) => b.act === `open:fleetattack:${battle}:${home}`))).toBe(true);
    // SEND THEM IN.
    const out = TC().act(`fleetattack:${battle}:${home}`) as { ok: boolean; note: string; seaFight: number; outcome: string; sunk: unknown[] };
    expect(out.ok).toBe(true);
    expect(out.outcome).toBe('fought');
    expect(out.seaFight).toBeGreaterThanOrEqual(0);
    expect(out.note).toMatch(/North Sea/);
    expect(Array.isArray(out.sunk)).toBe(true);
    // Once a month: the second time it says why.
    const again = TC().act(`fleetattack:${battle}:${home}`) as { ok: boolean; note: string };
    expect(again.ok).toBe(false);
  });

  it('the player can switch it on and off in a campaign under way', () => {
    const { g, battle } = germany(false);
    TC().load(saveGame(g));
    expect((TC().act('fleetattackrule:on') as { ok: boolean }).ok).toBe(true);
    expect((TC().fleetInHand(battle) as { fleetAttack: boolean }).fleetAttack).toBe(true);
    expect((TC().act('fleetattackrule:off') as { ok: boolean }).ok).toBe(true);
    expect((TC().fleetInHand(battle) as { fleetAttack: boolean }).fleetAttack).toBe(false);
    expect((TC().fleetAttack(true) as { on: boolean }).on).toBe(true);
  });

  it('ships left behind in the tray are split off as a squadron of their own, where the fleet is', () => {
    const { g, battle } = germany(false);
    TC().load(saveGame(g));
    const out = TC().act(`fleetsplitmany:${battle}:ca.2,dd.5`) as { ok: boolean; note: string; made: number };
    expect(out.ok).toBe(true);
    expect(out.made).toBeGreaterThan(0);
    expect(out.note).toMatch(/2 cruisers, 5 destroyers stay behind as/);
    const hand = TC().fleetInHand(out.made) as { cards: { cls: string; count: number }[] };
    expect(hand.cards.map((c) => [c.cls, c.count])).toEqual([['ca', 2], ['dd', 5]]);
    // A card closed without its order: the squadron left behind for it joins its fleet again, as it was.
    const back = TC().act(`fleetmerge:${out.made}:${battle}`) as { ok: boolean };
    expect(back.ok).toBe(true);
    const whole = TC().fleetInHand(battle) as { cards: { cls: string; count: number }[] };
    expect(whole.cards.map((c) => [c.cls, c.count])).toEqual([['bb', 2], ['ca', 8], ['dd', 20]]);
    // Every ship left behind: refused, with the rules' reason.
    const all = TC().act(`fleetsplitmany:${battle}:bb.2,ca.8,dd.20`) as { ok: boolean; note: string };
    expect(all.ok).toBe(false);
    expect(all.note).toMatch(/Some ships have to stay/);
  });
});
