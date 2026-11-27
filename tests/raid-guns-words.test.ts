// Whose guns the raid card says the bombers are lost to (the UI director's rider 3 table and rider 4, 22 November
// 09:01Z and 12:53Z, in words-ship-air-defence.md). Release 1 called a fleet's own guns "light flak". Each share now
// goes by its own name, for everyone and with every switch: over land as release 1 had it, at sea "the ships' guns",
// and in a harbour the ships' guns beside the guns ashore. The words are screen text only: nothing saved moves.
import { describe, expect, it } from 'vitest';
import page from '../src/campaign/ui/main.ts?raw';
import { gunsOfThem, gunsTo, type RaidKind, type RaidPlan } from '../src/campaign/airwar';
import bridge from '../unreal/TheaterCommand/Rules/campaign-bridge.ts?raw';
import { createGame, setWar } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { previewRaid, raidTargetsFor } from '../src/campaign/turn';
import type { Game } from '../src/campaign/types';

/** A German campaign at war with Britain and France too (tests/raid-figures-78.test.ts). */
function west(seed = 7): Game {
  const g = startOpen(createGame(seed, 'GER'));
  setWar(g, 'GER', 'UK');
  setWar(g, 'GER', 'FRA');
  return g;
}
/** Britain at war with Germany and Italy: its bombers can fly a harbour raid (Bremen) and a strike at a fleet at sea. */
function britain(seed = 7): Game {
  const g = startOpen(createGame(seed, 'UK'));
  setWar(g, 'UK', 'GER');
  setWar(g, 'UK', 'ITA');
  return g;
}
/** Every raid of a kind that can be flown this month, weighed up (at most `most` of them). */
function plans(g: Game, kind: RaidKind, most = 12): RaidPlan[] {
  const out: RaidPlan[] = [];
  for (const t of raidTargetsFor(g, kind)) {
    const p = previewRaid(g, kind, t.id);
    if (!p.why) out.push(p);
    if (out.length >= most) break;
  }
  return out;
}
const guns = (level: number, ships?: 'sea' | 'harbour') => ({ level, loss: 0.03, lossN: 3, stop: 0, ...(ships ? { ships } : {}) });

describe("the raid card's guns, each by its own name", () => {
  it('says each form the UI director ruled', () => {
    expect(gunsTo(guns(0))).toBe('light flak');
    expect(gunsTo(guns(2))).toBe('flak and level 2 guns');
    expect(gunsTo(guns(0, 'sea'))).toBe("the ships' guns");
    expect(gunsTo(guns(0, 'harbour'))).toBe("the ships' guns and light flak");
    expect(gunsTo(guns(2, 'harbour'))).toBe("the ships' guns, flak and level 2 guns");
    // The 3D game's raid card, in the forecast's "of them to ..." form (rider 3's table).
    expect(gunsOfThem(guns(0))).toBe('of them to light flak');
    expect(gunsOfThem(guns(2))).toBe('of them to flak and guns');
    expect(gunsOfThem(guns(0, 'sea'))).toBe("of them to the ships' guns");
    expect(gunsOfThem(guns(0, 'harbour'))).toBe("of them to the ships' guns and light flak");
    expect(gunsOfThem(guns(2, 'harbour'))).toBe("of them to the ships' guns, flak and guns");
  });

  it("names the ships' guns on a strike at a fleet and a raid on a harbour, and nowhere else; the game is untouched", () => {
    let fleets = 0, ports = 0;
    // Germany's bombers find fleets at sea; Britain's find a harbour (Bremen) too.
    for (const g of [west(), britain()]) {
      const before = saveGame(g);
      const fleet = plans(g, 'fleet'), port = plans(g, 'harbour');
      fleets += fleet.length;
      ports += port.length;
      for (const p of fleet) {
        expect(p.guns.ships, p.title).toBe('sea');
        expect(gunsTo(p.guns), p.title).toBe("the ships' guns");
      }
      for (const p of port) {
        expect(p.guns.ships, p.title).toBe('harbour');
        expect(gunsTo(p.guns), p.title).toBe(p.guns.level ? `the ships' guns, flak and level ${p.guns.level} guns` : "the ships' guns and light flak");
      }
      // Bombing by day and by night, and the older strike at shipping (no fleet in it): release 1's words.
      for (const kind of ['bomb', 'night', 'strike'] as RaidKind[]) {
        for (const p of plans(g, kind, 4)) {
          expect(p.guns.ships, `${kind} ${p.title}`).toBeUndefined();
          expect(gunsTo(p.guns)).toBe(p.guns.level ? `flak and level ${p.guns.level} guns` : 'light flak');
        }
      }
      expect(saveGame(g)).toBe(before);
    }
    expect(fleets).toBeGreaterThan(0);
    expect(ports).toBeGreaterThan(0);
  });

  it("is what the raid card's bombers line says, in the browser and in the 3D game", () => {
    expect(page).toContain('` · about ${num(p.guns.lossN)} to ${gunsTo(p.guns)}`');
    expect(page).not.toContain("'light flak'");
    expect(bridge).toContain("gunsWords: p.why || p.bombers <= 0 ? '' : gunsOfThem(p.guns),");
  });
});
