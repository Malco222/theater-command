// Rearguards (release 2, rearguard.ts; Brian 24 November 2026 18:24Z, "i like these"): a small corps of the player's
// holds where it stands until his next month; an enemy that attacks it there loses weeks of its month, and under Units
// decide it screens the corps falling back behind it from the pursuit. Behind its own switch.
import { describe, expect, it } from 'vitest';
import { breakUp, setBreakUp } from '../src/campaign/breakup';
import { allied, atWar, createGame, resolveBattle } from '../src/campaign/game';
import { REARGUARD, isRearguard, orderRearguard, rearguardOn, rearguardWeeks, screened, setRearguard, whyNotRearguard } from '../src/campaign/rearguard';
import { loadGame, saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, endHuman, humanUp, orderHoldRear, orderStandDown, stepAI } from '../src/campaign/turn';
import type { Corps, Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

function germany(on: boolean): Game {
  const g = startOpen(createGame(3, 'GER'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  if (on) setRearguard(g, true);
  setBreakUp(g, true);
  beginHuman(g);
  return g;
}
function month(g: Game): void {
  endHuman(g);
  for (let k = 0; !humanUp(g) && k < 200; k++) stepAI(g);
  beginHuman(g);
}
/** A German corps in the field split down to one division, standing next to an enemy army: the rearguard to be. */
function detachment(g: Game): { c: Corps; foes: Corps[] } {
  for (const c of g.corps) {
    if (c.nat !== 'GER' || c.dead || c.kind === 'fort' || c.town !== undefined || c.joint !== undefined || c.divs < 2) continue;
    const r = g.regions[c.reg];
    const foes = r.nb.flatMap((j) => g.regions[j].units.filter((u) => !u.dead && u.kind !== 'fort' && u.town === undefined && atWar(g, u.nat, 'GER') && u.ap > 0));
    if (!foes.length) continue;
    const part = breakUp(g, c, [0]);
    if (!part) continue;
    // (the rest of the corps marches off with the army: the detachment is left alone in the province)
    c.dead = true; r.units.splice(r.units.indexOf(c), 1);
    return { c: part, foes: foes.slice(0, 1) };
  }
  throw new Error('no German corps beside the enemy');
}

describe('rearguards', () => {
  it('off: none can be ordered', () => {
    const g = germany(false);
    expect(rearguardOn(g)).toBe(false);
    const { c } = detachment(g);
    expect(whyNotRearguard(g, c)).toMatch(/switched on/);
    expect(orderHoldRear(g, c.id)).toBe(false);
  });

  it('a small corps holds: no marching left this month; stood down the same month, it has its marching back', () => {
    const g = germany(true);
    const { c } = detachment(g);
    const ap = c.ap;
    expect(ap).toBeGreaterThan(0);
    expect(orderHoldRear(g, c.id)).toBe(true);
    expect(isRearguard(g, c)).toBe(true);
    expect(c.ap).toBe(0);
    expect(whyNotRearguard(g, c)).toMatch(/already/);
    expect(orderStandDown(g, c.id)).toBe(true);
    expect(c.ap).toBe(ap);
    expect(isRearguard(g, c)).toBe(false);
  });

  it('a big corps is told to split some off first', () => {
    const g = germany(true);
    const big = g.corps.find((c) => c.nat === 'GER' && !c.dead && c.kind !== 'fort' && c.town === undefined && c.joint === undefined && c.divs > REARGUARD.most)!;
    expect(whyNotRearguard(g, big)).toMatch(/Split some off first/);
  });

  it('an enemy that attacks it loses weeks of his month, win or lose, and the report says so', () => {
    const g = germany(true);
    const { c, foes } = detachment(g);
    const target = g.regions[c.reg];
    const save = saveGame(g);
    // The same battle twice, from the same save: with the rearguard ordered, and without.
    const held = loadGame(save)!, open = loadGame(save)!;
    expect(orderRearguard(held, held.corps.find((x) => x.id === c.id)!)).toBe(true);
    const att = (h: Game): Corps[] => foes.map((f) => h.corps.find((x) => x.id === f.id)!);
    const aHeld = att(held), aOpen = att(open);
    for (const x of [...aHeld, ...aOpen]) x.ap = Math.max(x.ap, 4);
    const before = aHeld.map((x) => x.ap);
    const rHeld = resolveBattle(held, aHeld, held.regions[target.id]);
    const rOpen = resolveBattle(open, aOpen, open.regions[target.id]);
    const weeks = rearguardWeeks([held.corps.find((x) => x.id === c.id)!]);
    expect(weeks).toBe(Math.min(REARGUARD.weeks, REARGUARD.weeksPerDiv));
    expect(rHeld.rearguard).toBe(weeks);
    expect(rOpen.rearguard).toBeUndefined();
    expect(rHeld.lines.some((l) => l.label === 'Rearguard')).toBe(true);
    // (the fight itself is the same fight: who carried the ground does not change)
    expect(rHeld.taken).toBe(rOpen.taken);
    aHeld.forEach((x, i) => expect(x.ap).toBe(Math.max(0, aOpen[i].ap - weeks)));
    expect(before.length).toBeGreaterThan(0);
    expect(held.headlines.some((h) => /rearguard at .* held the enemy up/.test(h.text))).toBe(true);
  });

  it('it screens its own side from the pursuit, and stands down at the start of his next month', () => {
    const g = germany(true);
    const { c } = detachment(g);
    orderHoldRear(g, c.id);
    const friend = (nat: string): boolean => nat === 'GER' || allied(g, nat, 'GER');
    const other = g.corps.find((x) => x.nat === 'GER' && !x.dead && x !== c)!;
    expect(screened(g, other, g.regions[c.reg], friend)).toBe(true);
    expect(screened(g, other, g.regions[c.reg], () => false)).toBe(false);
    month(g);
    if (!c.dead) expect(c.rear).toBeUndefined();
  });

  it('switched off, every rearguard stands down', () => {
    const g = germany(true);
    const { c } = detachment(g);
    orderHoldRear(g, c.id);
    setRearguard(g, false);
    expect(c.rear).toBeUndefined();
    expect(g.corps.some((x) => x.rear)).toBe(false);
  });
});

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
interface ViewCorps { id: number; rear?: boolean; rearWhy?: string }

describe('rearguards through the bridge', () => {
  it('the switch, the card fields and the order', () => {
    const g = germany(false);
    const { c } = detachment(g);
    TC().load(saveGame(g));
    const card = (): ViewCorps => (TC().view() as { corps: ViewCorps[] }).corps.find((x) => x.id === c.id)!;
    expect((TC().view() as { rearguard: boolean }).rearguard).toBe(false);
    expect(card().rearWhy).toBeUndefined();
    expect((TC().act('rearguardrule:on') as { ok: boolean }).ok).toBe(true);
    expect(card().rearWhy).toBe('');
    expect(card().rear).toBe(false);
    const out = TC().act(`rearguard:${c.id}:on`) as { ok: boolean; note: string };
    expect(out.ok).toBe(true);
    expect(out.note).toMatch(/holds as a rearguard/);
    expect(card().rear).toBe(true);
    expect((TC().act(`rearguard:${c.id}:off`) as { ok: boolean }).ok).toBe(true);
    expect(card().rear).toBe(false);
  });
});
