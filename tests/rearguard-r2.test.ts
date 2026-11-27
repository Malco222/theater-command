// Rearguards (release 2, rearguard.ts; Brian 24 November 2026 18:24Z, "i like these"): a small corps holds where it
// stands until its side's next month; an enemy that attacks it there loses time by the odds (the design director's
// 19:14Z), and under Units decide it slows the pursuit of the corps falling back behind it, by time only. The computer
// staffs order them by the same rule. Behind its own switch.
import { describe, expect, it } from 'vitest';
import { breakUp, setBreakUp } from '../src/campaign/breakup';
import { allied, atWar, createGame, resolveBattle } from '../src/campaign/game';
import { REARGUARD, isRearguard, orderRearguard, rearguardOn, rearguardWeeks, screenWeeks, setRearguard, staffRearguard, weeksWords, whyNotRearguard } from '../src/campaign/rearguard';
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

/** Whether the attackers carried the field (the rearguard was broken). */
const rHeldWin = (r: { level: string }): boolean => / victory$/.test(r.level);

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
    const rg = held.corps.find((x) => x.id === c.id)!;
    // (one division: a week where it held, half that where it was broken)
    const weeks = rearguardWeeks([rg], rHeldWin(rHeld));
    expect(weeks).toBe(Math.min(REARGUARD.weeks, REARGUARD.weeksPerDiv) * (rHeldWin(rHeld) ? REARGUARD.broken : 1));
    expect(rHeld.rearguard).toBe(weeks);
    expect(rOpen.rearguard).toBeUndefined();
    expect(rHeld.lines.some((l) => l.label === 'Rearguard')).toBe(true);
    // (the fight itself is the same fight: who carried the ground does not change)
    expect(rHeld.taken).toBe(rOpen.taken);
    aHeld.forEach((x, i) => expect(x.ap).toBe(Math.max(0, aOpen[i].ap - weeks)));
    expect(before.length).toBeGreaterThan(0);
    expect(held.headlines.some((h) => /rearguard at .* held the enemy up/.test(h.text))).toBe(true);
    expect(held.headlines.some((h) => /rearguard at .* (was broken, but )?held the enemy up/.test(h.text))).toBe(true);
    expect(held.headlines.some((h) => /enemy rearguard at .* held our attack up/.test(h.text))).toBe(true);
    // (the rearguard fought in it, as any corps)
    expect(rHeld.defenders.some((u) => u.name === rg.name)).toBe(true);
  });

  it('the attackers share one fight\'s delay: a week a division where it held, half where it broke, two weeks at most', () => {
    const two = { divs: 2 } as Corps, one = { divs: 1 } as Corps;
    expect(rearguardWeeks([two], false)).toBe(2);
    expect(rearguardWeeks([two], true)).toBe(1);
    expect(rearguardWeeks([one], false)).toBe(1);
    expect(rearguardWeeks([one], true)).toBe(0.5);
    expect(rearguardWeeks([two, two], false)).toBe(REARGUARD.weeks);
    expect(rearguardWeeks([], false)).toBe(0);
    expect(weeksWords(2)).toBe('2 weeks');
    expect(weeksWords(1)).toBe('1 week');
    expect(weeksWords(0.5)).toBe('4 days');
    expect(weeksWords(0.01)).toBe('less than a day');
  });

  it('driven out of its province, it is a rearguard no longer', () => {
    const g = germany(true);
    const { c } = detachment(g);
    orderHoldRear(g, c.id);
    expect(isRearguard(g, c)).toBe(true);
    const to = g.regions[c.reg].nb.map((j) => g.regions[j]).find((r) => allied(g, r.ctrl, 'GER'))!;
    const from = g.regions[c.reg];
    from.units.splice(from.units.indexOf(c), 1); to.units.push(c); c.reg = to.id;
    expect(isRearguard(g, c)).toBe(false);
  });

  it('the computer staffs order rearguards by the same rule', () => {
    const g = germany(true);
    let seen = false;
    for (let m = 0; m < 6 && !seen; m++) {
      endHuman(g);
      for (let k = 0; !humanUp(g) && k < 200; k++) {
        stepAI(g);
        if (g.corps.some((x) => x.rear && x.nat !== 'GER' && !x.dead)) seen = true;
      }
      beginHuman(g);
    }
    expect(seen).toBe(true);
    for (const x of g.corps) if (x.rear && x.nat !== 'GER') expect(x.divs).toBeLessThanOrEqual(REARGUARD.most);
    // (the player's corps are never ordered by a staff)
    const mine = g.corps.find((x) => x.nat === 'GER' && !x.dead)!;
    expect(staffRearguard(g, mine)).toBe(false);
  });

  it('it screens its own side from the pursuit, and stands down at the start of his next month', () => {
    const g = germany(true);
    const { c } = detachment(g);
    orderHoldRear(g, c.id);
    const friend = (nat: string): boolean => nat === 'GER' || allied(g, nat, 'GER');
    const other = g.corps.find((x) => x.nat === 'GER' && !x.dead && x !== c)!;
    // (time only, by the odds against the pursuers: never a blanket exemption)
    expect(screenWeeks(g, other, g.regions[c.reg], friend)).toBe(rearguardWeeks([c], false));
    expect(screenWeeks(g, other, g.regions[c.reg], () => false)).toBe(0);
    // (it does not slow its own pursuers: beaten, it is chased as any corps)
    expect(screenWeeks(g, c, g.regions[c.reg], friend)).toBe(0);
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
