// Fog on raids (release 2, behind g.flags.rfog, off as shipped): the day and night card says "fighters in range not
// known" where its side never saw the fighters and counts none. The UI director, 25 Nov 2026 21:22Z, case b, her code
// note: the knowledge lines now say "not known", so the card reads the sighting itself (RaidFog.fightersSeen < 0),
// never the words of a line, or that line goes silent.
import { describe, expect, it } from 'vitest';
import { setRaidFog } from '../src/campaign/airfog';
import { createGame, setWar } from '../src/campaign/game';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, previewDayNight, raidTargetsFor, stepAI } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** The player's Britain at war with Germany in September 1939, fog on raids on, at the opening of his month. */
function britain(): Game {
  const g = startOpen(createGame(7, 'UK'));
  setWar(g, 'UK', 'GER');
  setRaidFog(g, true);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
  return g;
}

describe('fog on raids: the fighters never seen', () => {
  it('a target whose fighters were never seen, with none counted, says "fighters in range not known"; one seen with none says nothing', () => {
    const g = britain();
    // Nothing remembered anywhere: no place near any target was ever seen, so the high end counts no unseen fighters.
    delete (g.intel as unknown as { airSeen?: unknown }).airSeen;
    let unseen = 0, seenNone = 0;
    for (const x of raidTargetsFor(g, 'bomb')) {
      const dn = previewDayNight(g, 'bomb', x.id);
      if (!dn || !dn.fog || dn.fog.live || dn.foeSquadrons > 0 || dn.day.why) continue;
      expect(dn.fog.fightersSeen).toBe(-1);
      expect(dn.fog.fighters).toBe('Fighters: not known');
      expect(dn.foesCount).toBe('fighters in range not known');
      unseen++;
      // The same place last seen a month ago with no fighters in reach: the card has nothing to say of them.
      (g.intel as unknown as { airSeen?: Record<string, Record<number, number[]>> }).airSeen = { UK: { [x.id]: [g.turn - 1, 0, 0, 0, 0] } };
      const seen = previewDayNight(g, 'bomb', x.id)!;
      if (seen.fog && !seen.fog.live && seen.foeSquadrons === 0) {
        expect(seen.fog.fightersSeen).toBe(0);
        expect(seen.fog.fighters).toBe('Fighters: none in reach, seen a month ago');
        expect(seen.foesCount).toBe('');
        seenNone++;
      }
      delete (g.intel as unknown as { airSeen?: unknown }).airSeen;
    }
    expect(unseen).toBeGreaterThan(0);
    expect(seenNone).toBeGreaterThan(0);
  });
});
