// The fourth review of the far chart navy's rulings of 15:48Z, 16:05Z and 16:19Z, 25 November 2026 (release 2): the
// faults it found, mended.
// 1. Strikes at anchor by a nation since at peace with the fleet's no longer count, nor do the fields they flew from.
// 2. A player's striking fleet still on its gather voyage when his admirals stand down comes home all the same.
// 7. A fleet whose base falls makes for an ally's harbour open to it before it is said to have none it can reach.
// 8, 10. SCUTTLE IT on the 3D bridge is asked twice.
// 9. A fleet waiting at sea says so on its card and in its admirals' reason, not "in passage to" its fallen base.
// 12. TURN BACK is shut on a fleet waiting at sea: it has no harbour to turn back to.
import { vi } from 'vitest';
vi.hoisted(() => { (globalThis as any).TC_FAR = 1; });
import { describe, expect, it } from 'vitest';
import { setDesk } from '../src/campaign/depth';
import { signPeace } from '../src/campaign/diplomacy';
import { fleetDoing } from '../src/campaign/fleets';
import { fleetWhy, setFleetWhy } from '../src/campaign/fleetwhy';
import { allied, atWar, createGame, declareWar } from '../src/campaign/game';
import { adrift, anchorLoss, inPassage, navalAI, navalUpkeep, rebase, safeHarbour, setGate, strikeThreat, struckLine } from '../src/campaign/navy';
import { gatherMonth } from '../src/campaign/readiness';
import { saveGame } from '../src/campaign/save';
import { startOpen } from '../src/campaign/staff';
import { humanUp, stepAI } from '../src/campaign/turn';
import type { Fleet, Game, Region } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
type Button = { label: string; act: string; ok: boolean; tip: string };
const fleet = (g: Game, nat: string, name: string): Fleet => g.fleets.find((f) => f.nat === nat && f.name === name)!;
const said = (g: Game, from: number, re: RegExp): boolean => g.headlines.slice(from).some((h) => re.test(h.text));
const gateOf = (g: Game, name: string): number => g.gates.findIndex((x) => (x as { name?: string }).name === name);
const nextMonth = (g: Game, nat = 'JAP'): void => { g.turn++; navalUpkeep(g, nat); };

/** America's Pacific Fleet at anchor at Pearl Harbor, at war with Japan, struck there this month by the Mobile Fleet and from Kwajalein's field. */
function struckAtPearl(): { g: Game; f: Fleet; mf: Fleet } {
  const g = startOpen(createGame(3, null));
  if (!atWar(g, 'JAP', 'USA')) declareWar(g, 'JAP', 'USA');
  const f = fleet(g, 'USA', 'Pacific Fleet'), mf = fleet(g, 'JAP', 'Mobile Fleet');
  expect(rebase(g, f, g.byName['Pearl Harbor'], true)).toBe(true);
  const north = g.seas.findIndex((s) => s.name === 'Hawaiian Approaches'), kw = g.byName['Kwajalein'];
  expect(north).toBeGreaterThanOrEqual(0);
  kw.ctrl = 'JAP';
  f.anchor = [{ t: g.turn, at: f.base, n: 3, lost: 0.2, w: 300, from: [{ sea: north, field: -1, km: 600, nat: 'JAP', fleet: mf.id }, { sea: -1, field: kw.id, km: 1200, nat: 'JAP' }] }];
  return { g, f, mf };
}

describe('1. strikes by a nation since at peace with the fleet\'s no longer count', () => {
  it('at war, the strike is the cost of staying and its sea and field are read; at peace, neither', () => {
    const { g, f } = struckAtPearl();
    const kw = g.byName['Kwajalein'];
    expect(anchorLoss(g, f)).toBeGreaterThan(0);
    const T = strikeThreat(g, f);
    expect(T.fields.get(kw.id)).toBe(1200);
    expect(struckLine(g, f)).toMatch(/^Struck at anchor at Pearl Harbor/);
    for (const [a, b] of [['JAP', 'USA'], ['USA', 'JAP']]) g.nations[a].atWar = g.nations[a].atWar.filter((x) => x !== b);
    expect(atWar(g, 'JAP', 'USA')).toBe(false);
    expect(anchorLoss(g, f)).toBe(0);
    expect(safeHarbour(g, f)).toBeNull();
    expect(struckLine(g, f)).toBe('');
    const P = strikeThreat(g, f);
    expect(P.fields.has(kw.id)).toBe(false);
    expect(P.seas.size).toBe(0);
  });

  it('the field the strikers flew from, taken by the fleet\'s side, is not read; their carriers still are', () => {
    const { g, f } = struckAtPearl();
    const kw = g.byName['Kwajalein'];
    kw.ctrl = 'USA';
    const T = strikeThreat(g, f);
    expect(T.fields.has(kw.id)).toBe(false);
    expect(T.seas.size).toBeGreaterThan(0);
    expect(anchorLoss(g, f)).toBeGreaterThan(0);
  });

  it('a peace signed ends them the same way', () => {
    const { g, f } = struckAtPearl();
    if (signPeace(g, 'JAP', 'USA')) {
      expect(atWar(g, 'JAP', 'USA')).toBe(false);
      expect(anchorLoss(g, f)).toBe(0);
    }
  });
});

describe('2. the stand-down brings home a striking fleet still on its gather voyage', () => {
  it('gathered from far off, in passage still when the window has passed, it is ordered home from where it is', () => {
    const g = startOpen(createGame(17, 'JAP'));
    setDesk(g, 'navy', true);
    g.turn = 23;
    g.flags['pac:freeze'] = 23;
    const force = fleet(g, 'JAP', 'Mobile Fleet'), sing = g.byName['Singapore'];
    sing.ctrl = 'JAP';
    expect(rebase(g, force, sing, true)).toBe(true);
    g.turn = 25;
    gatherMonth(g, 'JAP');
    expect(g.regions[force.base].name).toBe('Etorofu');
    expect(inPassage(force)).toBe(true);
    // Still on its way when the window has passed (its passage not steamed): the admirals stand down, and it comes home.
    g.turn = 30;
    gatherMonth(g, 'JAP');
    expect(g.regions[force.base].name).toBe('Hiroshima');
    expect(force.port).toBe('Kure');
    expect(g.flags['pac:gathered']).toBe(-1);
  });
});

describe('7. an ally\'s harbour open to it is taken before a fleet is said to have none it can reach', () => {
  it('Germany\'s ports lost and the straits shut, Konigsberg held by an ally not at war with the taker: it makes for it at once', () => {
    const g = startOpen(createGame(3, null));
    if (!atWar(g, 'GER', 'SOV')) declareWar(g, 'GER', 'SOV');
    const f = fleet(g, 'GER', 'Swinemunde Fleet'), where = g.regions[f.base].name, kon = g.byName['Konigsberg'];
    Object.assign(f, { stance: 'port', zone: f.home });
    setGate(g, gateOf(g, 'the Danish straits'), true);
    for (const r of g.regions) if (r.port && r.ctrl === 'GER') r.ctrl = 'SOV';
    kon.ctrl = 'ITA';
    g.nations.ITA.align = g.nations.GER.align;
    expect(allied(g, 'GER', 'ITA')).toBe(true);
    expect(atWar(g, 'ITA', 'SOV')).toBe(false);
    const news = g.headlines.length;
    navalUpkeep(g, 'GER');
    expect(adrift(f)).toBe(false);
    expect(f.base).toBe(kon.id);
    expect(said(g, news, new RegExp(`^With ${where} lost, the Swinemunde Fleet moves its base to Konigsberg\\.$`))).toBe(true);
    expect(said(g, news, /has no harbour it can reach/)).toBe(false);
  });
});

/** The Combined Fleet in passage from Kure to Konigsberg (made Japan's) for a player's Japan, then left no harbour it can reach. */
function adriftHis(): { g: Game; f: Fleet; kon: Region } {
  const g = startOpen(createGame(3, 'JAP'));
  for (let k = 0; !humanUp(g) && k < 80; k++) stepAI(g);
  const f = fleet(g, 'JAP', 'Combined Fleet'), kon = g.byName['Konigsberg'];
  kon.ctrl = 'JAP';
  expect(rebase(g, f, kon)).toBe(true);
  for (let k = 0; k < 12 && g.turn < f.passage!.due - 1; k++) nextMonth(g);
  setGate(g, gateOf(g, 'the Danish straits'), true);
  for (const r of g.regions) if (r.port && r !== kon && allied(g, 'JAP', r.ctrl)) r.ctrl = 'USA';
  nextMonth(g);
  expect(adrift(f)).toBe(true);
  return { g, f, kon };
}

describe('8, 10, 12. the 3D bridge: SCUTTLE IT asked twice, TURN BACK shut while it waits at sea', () => {
  it('the first press asks; the second scuttles; TURN BACK says why it is shut', () => {
    const { g, f } = adriftHis();
    TC().load(saveGame(g));
    const top = (TC().page(`fleet:${f.id}`) as { top: Button[] }).top;
    const back = top.find((b) => b.label === 'TURN BACK')!, sc = top.find((b) => b.label === 'SCUTTLE IT')!;
    expect(back.ok).toBe(false);
    expect(back.tip).toMatch(/It has no harbour to turn back to: pick a harbour open to it on the map once one opens, or scuttle it\.$/);
    expect(sc.ok).toBe(true);
    const first = TC().act(sc.act) as { ok: boolean; note: string };
    expect(first.ok).toBe(false);
    expect(first.note).toMatch(/^Scuttle the Combined Fleet\? .* Press it again to scuttle it\.$/);
    expect((TC().view() as { fleets: { id: number }[] }).fleets.some((x) => x.id === f.id)).toBe(true);
    const second = TC().act(sc.act) as { ok: boolean; note: string };
    expect(second.ok).toBe(true);
    expect(second.note).toBe('The Combined Fleet is scuttled.');
    expect((TC().view() as { fleets: { id: number }[] }).fleets.some((x) => x.id === f.id)).toBe(false);
  });

  it('the question is asked again in another game, even of a fleet with the same number in the same month', () => {
    const a = adriftHis();
    TC().load(saveGame(a.g));
    expect((TC().act(`fleetscuttle:${a.f.id}`) as { ok: boolean }).ok).toBe(false);
    const b = adriftHis();
    b.g.seed = a.g.seed + 1;
    TC().load(saveGame(b.g));
    const first = TC().act(`fleetscuttle:${b.f.id}`) as { ok: boolean; note: string };
    expect(first.ok).toBe(false);
    expect(first.note).toMatch(/^Scuttle the /);
  });
});

describe('9. waiting at sea, its card and its admirals say so', () => {
  it('the card reads waiting at sea, and the admirals\' reason is not "in passage to" its fallen base', () => {
    const { g, f } = adriftHis();
    expect(fleetDoing(g, f)).toMatch(/^Waiting at sea .*, with no harbour it can reach$/);
    nextMonth(g);
    expect(fleetDoing(g, f)).toMatch(/^Waiting at sea /);
    setDesk(g, 'navy', true);
    setFleetWhy(g, true);
    delete f.ordered;
    navalAI(g, 'JAP');
    expect(fleetWhy(g, f)).toBe('Your admirals: with no harbour it can reach, it waits at sea until one opens.');
  });
});
