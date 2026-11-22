// The month's submarine scene's record (release 2; ruling 241 D1, the production lead's form section D1): written
// only while the scene's viewing setting is on, read by no rule. Off, every save is release 1's; on, the same once
// the record is stripped.
import { afterEach, describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { navalUpkeep, subScene } from '../src/campaign/navy';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { SUBSCENE_ENV } from '../src/campaign/stage';
import type { Game } from '../src/campaign/types';

afterEach(() => { subScene.on = false; });

const twin = (g: Game): Game => loadGame(saveGame(g))!;
const stripped = (g: Game): string => { const d = JSON.parse(saveGame(g)); delete d.subLook; return JSON.stringify(d); };

describe('the submarine scene\'s record', () => {
  it('is off unless the setting is on: nothing written, the save is release 1\'s', () => {
    expect(SUBSCENE_ENV).toBe(false);
    const g = startOpen(createGame(1, null));
    navalUpkeep(g, 'UK');
    expect(g.subLook).toBeUndefined();
    expect('subLook' in JSON.parse(saveGame(g))).toBe(false);
  });

  it('on, writes the month\'s hunts and convoys, and moves nothing else', () => {
    const a = startOpen(createGame(1, null)), b = twin(a);
    navalUpkeep(a, 'UK');
    subScene.on = true;
    navalUpkeep(b, 'UK');
    subScene.on = false;
    expect(stripped(b)).toBe(saveGame(a));
    const l = b.subLook!.find((x) => x.nat === 'UK' && x.turn === b.turn)!;
    const hunt = l.hunts.find((h) => h.quarry.some((q) => q.nat === 'GER'))!;
    expect(hunt).toBeDefined();
    expect(hunt.hunters.length + (hunt.air > 0 ? 1 : 0)).toBeGreaterThan(0);
    expect(hunt.sunk).toBe(a.fleets.filter((f) => hunt.quarry.some((q) => q.fleet === f.id)).reduce((s, f) => s + f.lost.ss, 0));
    const c = l.convoys.find((x) => x.route === 'atlantic')!;
    expect(c.tons).toBe(b.convoys.atlantic.tons);
    const raiders = c.seas.flatMap((s) => s.raiders);
    expect(raiders.some((r) => r.nat === 'GER' && r.n > 0)).toBe(true);
    expect(c.seas.flatMap((s) => s.escorts).some((e) => e.nat === 'UK')).toBe(true);
    expect(Math.abs(c.seas.reduce((s, x) => s + x.tons, 0) - c.tons)).toBeLessThanOrEqual(c.seas.length * 1000);
  });

  it('names a torpedoing\'s possible attackers and picks one on its own key, without moving the torpedo\'s roll', () => {
    let seen = 0;
    for (let t = 1; t <= 60 && !seen; t++) {
      const a = startOpen(createGame(1, null));
      const home = a.fleets.find((f) => f.nat === 'UK' && f.ships.bb > 0)!;
      const boats = a.fleets.find((f) => f.nat === 'GER' && f.ships.ss > 0)!;
      home.ships.dd = 0;
      boats.zone = home.zone; boats.stance = 'raid';
      a.turn = t;
      const b = twin(a);
      navalUpkeep(a, 'UK');
      subScene.on = true;
      navalUpkeep(b, 'UK');
      subScene.on = false;
      expect(stripped(b)).toBe(saveGame(a));
      const tp = b.subLook?.flatMap((l) => l.torpedoes) ?? [];
      if (!tp.length) continue;
      seen++;
      expect(tp[0].fleet).toBe(home.id);
      expect(tp[0].could).toContain(boats.id);
      expect(tp[0].could).toContain(tp[0].by);
    }
    expect(seen).toBe(1);
  });

  it('keeps this month and last, and goes through a save', () => {
    const g = startOpen(createGame(1, null));
    subScene.on = true;
    for (let i = 0; i < 3; i++) { navalUpkeep(g, 'UK'); g.turn++; }
    g.turn--;
    const turns = [...new Set(g.subLook!.map((l) => l.turn))].sort();
    expect(turns).toEqual([g.turn - 1, g.turn]);
    expect(twin(g).subLook).toEqual(g.subLook);
  });
});
