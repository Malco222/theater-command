// The 3D game's doorway to breaking a corps up (release 2, breakup.ts; Brian 24 November 2026 18:02Z): each of his corps
// says whether divisions can be split off it and what it could rejoin; 'breakup:<corps>:<slots>' splits, 'rejoin:<a>:<b>'
// puts back, 'breakuprule:on|off' switches it. Off, none of it is offered.
import { describe, expect, it } from 'vitest';
import { createGame } from '../src/campaign/game';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import { setBreakUp } from '../src/campaign/breakup';
import type { Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface ViewCorps { id: number; nat: string; name: string; divs: number; reg: number; breakWhy?: string; rejoinId?: number; rejoinName?: string; divisions?: { name: string }[] }
interface View { breakUp: boolean; corps: ViewCorps[] }

function germany(on: boolean): { g: Game; id: number } {
  const g = startOpen(createGame(3, 'GER'));
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  setBreakUp(g, on); // (on unless switched off since the 1.5.1 patch shipped it on)
  const c = g.corps.find((x) => x.nat === 'GER' && !x.dead && x.divs >= 3 && x.kind !== 'fort' && x.town === undefined && !x.joint)!;
  return { g, id: c.id };
}
const view = (): View => TC().view() as View;

describe('breaking a corps up through the bridge', () => {
  it('off: nothing is offered, and the order is refused', () => {
    const { g, id } = germany(false);
    TC().load(saveGame(g));
    const v = view();
    expect(v.breakUp).toBe(false);
    expect(v.corps.find((c) => c.id === id)!.breakWhy).toBeUndefined();
    const out = TC().act(`breakup:${id}:0`) as { ok: boolean; note: string };
    expect(out.ok).toBe(false);
    expect(out.note).toMatch(/switched on/);
  });

  it('on: SPLIT OFF makes a corps of the picked divisions, and REJOIN puts it back', () => {
    const { g, id } = germany(true);
    TC().load(saveGame(g));
    const before = view().corps.find((c) => c.id === id)!;
    expect(view().breakUp).toBe(true);
    expect(before.breakWhy).toBe('');
    expect(before.rejoinId).toBe(-1);
    const out = TC().act(`breakup:${id}:0,1`) as { ok: boolean; note: string; made: number };
    expect(out.ok).toBe(true);
    expect(out.made).toBeGreaterThan(0);
    expect(out.note).toMatch(/split off from/);
    const v = view();
    const part = v.corps.find((c) => c.id === out.made)!, rest = v.corps.find((c) => c.id === id)!;
    expect(part.divs).toBe(2);
    expect(rest.divs).toBe(before.divs - 2);
    expect(part.reg).toBe(rest.reg);
    expect(part.rejoinId).toBe(id);
    expect(part.rejoinName).toBe(rest.name);
    const back = TC().act(`rejoin:${out.made}:${id}`) as { ok: boolean; note: string; joined: number };
    expect(back.ok).toBe(true);
    expect(back.joined).toBe(id);
    expect(back.note).toMatch(/together again/);
    expect(view().corps.find((c) => c.id === id)!.divs).toBe(before.divs);
    expect(view().corps.some((c) => c.id === out.made)).toBe(false);
    // Every division picked: refused, with the rules' reason.
    const all = TC().act(`breakup:${id}:${Array.from({ length: before.divs }, (_, i) => i).join(',')}`) as { ok: boolean; note: string };
    expect(all.ok).toBe(false);
    expect(all.note).toMatch(/have to stay/);
  });

  it('the player can switch it on and off in a campaign under way', () => {
    const { g } = germany(false);
    TC().load(saveGame(g));
    expect((TC().act('breakuprule:on') as { ok: boolean }).ok).toBe(true);
    expect(view().breakUp).toBe(true);
    expect((TC().act('breakuprule:off') as { ok: boolean }).ok).toBe(true);
    expect(view().breakUp).toBe(false);
    expect((TC().breakUp(true) as { on: boolean }).on).toBe(true);
  });
});
