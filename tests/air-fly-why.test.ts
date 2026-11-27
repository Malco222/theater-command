// The raid card says why FLY IT YOURSELF is not offered (a mend in his game: the design director's known-faults line of
// 27 November 2026 23:13Z, corrected 23:16Z). The 3D game asks the battle bridge TC.airLook once a pick; where it
// answers { ticket: null, why }, the card now shows that why after "This raid cannot be flown in the air battle."
// (CampaignPlayerController FlyNotOfferedWhy; pinned by Tools/fly_why/run.sh). Before, the why went only to the log.
// This is the bridge probe the line asks for: what airLook answers on a refused campaign raid, and on a raid it gives a
// ticket. And the read owed with the line (the assembler's, 23:18Z), held here: the raid's reach has one reading, so on a
// grand campaign airLook never refuses a raid by day or a fight for the sky whose card FLY THE RAID can send.
import { describe, expect, it } from 'vitest';
import { airAct } from '../src/campaign/airpages';
import { ATOM, carriersFor, safestSite, setAtom } from '../src/campaign/atom';
import { raidTargets, type RaidKind } from '../src/campaign/airwar';
import { createGame } from '../src/campaign/game';
import { setFog } from '../src/campaign/intel';
import { loadGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { grantTech } from '../src/campaign/tech';
import { beginHuman, humanUp, orderAtomStrike, previewRaid, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';
import { battleCalls } from '../unreal/TheaterCommand/Rules/battle-bridge';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

// eslint-disable-next-line @typescript-eslint/no-explicit-any
const TC = (globalThis as Record<string, unknown>).TC as any;

/** The bridge's air calls on a game of the test's own, as the 3D game reaches them. */
const bridgeOn = (g: Game) => battleCalls({ game: () => g, attack: () => null, raid: (k, tg, i, f) => airAct(g, `raid:${k}:${tg}:${i.join('.')}`, f) });

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

describe('the raid card says why FLY IT YOURSELF is not offered (known-faults mend, 27 November)', () => {
  it('a refused campaign raid: airLook answers no ticket and the whole sentence the card shows', () => {
    const g = armedUK();
    const ham = g.byName.Hamburg.id;
    const ids = carriersFor(g, 'UK', ham).map((f) => f.id);
    expect(ids.length).toBeGreaterThan(0);
    // Not yet armed for Hamburg: airLook has a ticket, so FLY IT YOURSELF is offered and the card has no line.
    const open = bridgeOn(g).airLook('bomb', ham, ids) as { ticket: unknown; why?: string };
    expect(open.ticket).toBeTruthy();
    expect(open.why).toBeUndefined();
    // Armed for Hamburg: the staff flies it, and the bridge hands the 3D game the why the card now shows.
    expect(orderAtomStrike(g, ham)).toBe(true);
    const shut = bridgeOn(g).airLook('bomb', ham, ids) as { ticket: unknown; why: string };
    expect(shut.ticket).toBeNull();
    expect(shut.why).toBe('A raid that carries the atomic bomb is flown by the staff.');
    // (the card shows it after "This raid cannot be flown in the air battle.": FlyNotOfferedLine, whose words are pinned by
    // Tools/fly_why, not here; here only that the rules' why is a whole sentence)
    expect(shut.why).toMatch(/^[A-Z].*\.$/);
    // FLY THE RAID still stands on its own check: the card's plan has no why.
    expect(previewRaid(g, 'bomb', ham, ids).why ?? null).toBeNull();
  });

  // (The UI director, 23:28Z, condition 1: "This raid cannot be flown in the air battle." says another way remains, so it
  // is said only where FLY THE RAID still flies the raid. The card draws the line only with its FLY THE RAID enabled, and
  // this holds that airLook then refuses only for the ticket's own causes, never the plan's or the assessment's: the
  // grand campaign as Germany at turn 33 (May 1942) and as Britain at turns 45 and 57, every raid by day and fight for
  // the sky the card can send, as the staff proposes it and with each group the card offers picked alone.)
  for (const seat of [{ human: 'GER', from: 33, seed: 11 }, { human: 'UK', from: 45, seed: 7 }, { human: 'UK', from: 57, seed: 5 }]) it(`the raid's reach has one reading (${seat.human}, turn ${seat.from}): airLook refuses no raid whose card FLY THE RAID can send`, () => {
    TC.start({ view: 'grand', ...seat });
    const g = loadGame(TC.save().text)!;
    let asked = 0;
    let own = 0;
    const refused: string[] = [];
    /** Asks airLook where the card's FLY THE RAID can send the raid as picked. */
    const weigh = (kind: RaidKind, target: number, ids?: number[]): boolean => {
      const p = previewRaid(g, kind, target, ids);
      if (p.why || !p.picked?.length) return false;
      const look = TC.airLook(kind, target, p.picked) as { ticket: unknown; why?: string | null };
      if (!look.ticket) refused.push(`${kind} on ${target} with ${p.picked.join('.')}: ${look.why}`);
      return true;
    };
    for (const kind of ['bomb', 'superiority'] as RaidKind[]) {
      for (const t of raidTargets(g, kind, seat.human)) {
        if (weigh(kind, t.id)) asked++;
        // (the card lets him pick his own groups: each group that can go, alone)
        for (const s of previewRaid(g, kind, t.id).seats) if (!s.why && weigh(kind, t.id, [s.id])) own++;
      }
    }
    expect(asked).toBeGreaterThan(0);
    expect(own).toBeGreaterThan(0);
    expect(refused).toEqual([]);
  });
});
