// The 3D game's look at a neutral's bargains and the volunteer legions (release 2; build contract
// production/r2-bargains-legions/contract.md, part B): TC.bargainsLook(), TC.bargainBid() and TC.bargainSell(), from
// Rules/bargains-look.ts, added to the bridge's TC. Both switches are off in release 1, and then the map's one question
// is answered exactly {"on":false} and the map's picture is the one it always was.
import { describe, expect, it } from 'vitest';
import stage from '../data/campaign/stage.json';
import { BARGAINS, bargainsView, bidSteps, startBargains } from '../src/campaign/bargains';
import { legionsMonth, legionsView, startLegions } from '../src/campaign/legions';
import { BUDGET } from '../src/campaign/economy';
import { atWar, createGame } from '../src/campaign/game';
import { setStop, stopOf } from '../src/campaign/history';
import { loadGame, saveGame } from '../src/campaign/save';
import { BARGAINS_STAGE, LEGIONS_STAGE } from '../src/campaign/stage';
import { startOpen } from '../src/campaign/staff';
import { beginHuman, humanUp, stepAI } from '../src/campaign/turn';
import { MEN_PER_DIV, type Game } from '../src/campaign/types';
import '../unreal/TheaterCommand/Rules/campaign-bridge';
import { bargainsLookOf, type LookRow } from '../unreal/TheaterCommand/Rules/bargains-look';

type Call = (...a: unknown[]) => unknown;
const TC = (): Record<string, Call> => (globalThis as unknown as { TC: Record<string, Call> }).TC;
const saved = (): string => (TC().save() as { text: string }).text;
const look = (): Record<string, unknown> => TC().bargainsLook() as Record<string, unknown>;

/** A grand campaign with this country at the table, at its first month of orders, with the switches as asked. */
function playing(nat: string, on: { bargains?: boolean; legions?: boolean } = { bargains: true, legions: true }, stop = 1, seed = 1): Game {
  const g = startOpen(createGame(seed, nat));
  if (stop !== stopOf(g)) setStop(g, stop, true);
  if (on.bargains) startBargains(g);
  if (on.legions) startLegions(g);
  let guard = 0;
  while (!humanUp(g) && guard++ < 200) stepAI(g);
  beginHuman(g);
  return g;
}

const war = (g: Game, a: string, b: string): void => { if (!atWar(g, a, b)) { g.nations[a].atWar.push(b); g.nations[b].atWar.push(a); } };

describe('switched off (release 1)', () => {
  it('the map asks one question and is answered exactly {"on":false}; its picture carries nothing new', () => {
    expect((stage as { bargains?: boolean; legions?: boolean }).bargains).toBe(false);
    expect((stage as { bargains?: boolean; legions?: boolean }).legions).toBe(false);
    if (BARGAINS_STAGE || LEGIONS_STAGE) return; // (a run with TC_BARGAINS or TC_LEGIONS set)
    TC().start({ human: 'UK', view: 'grand', seed: 1939 });
    const before = saved();
    expect(JSON.stringify(TC().bargainsLook())).toBe('{"on":false}');
    expect(saved()).toBe(before);
    // (The view's corps entries gain 'vol' only for a legion's corps, and with the legions off there is none: the
    // view's JSON is byte for byte what it was before the bridge's line.)
    const g = loadGame(before)!;
    expect(g.corps.some((c) => c.vol)).toBe(false);
    const view = JSON.stringify(TC().view());
    expect(view).not.toContain('"vol"');
    expect(JSON.stringify(TC().view())).toBe(view);
    expect(saved()).toBe(before);
  });

  it('the orders refuse, and change nothing', () => {
    if (BARGAINS_STAGE || LEGIONS_STAGE) return;
    TC().start({ human: 'UK', view: 'grand', seed: 1939 });
    const before = saved();
    const bid = TC().bargainBid('swe-ore', 1.5) as { ok: boolean; note: string; view: unknown };
    expect(bid.ok).toBe(false);
    expect(bid.note).not.toBe('');
    expect(bid.view).toBeTruthy();
    const sell = TC().bargainSell('swe-ore', null) as { ok: boolean; note: string };
    expect(sell.ok).toBe(false);
    expect(saved()).toBe(before);
  });

  it('the bridge adds the three calls to TC and replaces none', () => {
    for (const k of ['bargainsLook', 'bargainBid', 'bargainSell']) expect(typeof TC()[k]).toBe('function');
    for (const k of ['view', 'start', 'load', 'save', 'court', 'endMonth']) expect(typeof TC()[k]).toBe('function');
  });
});

describe('switched on', () => {
  it('reads the rules\' rows and adds the map\'s fields, changing nothing of the campaign', () => {
    const g = playing('UK');
    TC().load(saveGame(g));
    const before = saved();
    const L = look() as { on: boolean; viewer: string; turn: number; month: string; term: number; round: number; scale: number; nations: Record<string, { name: string; the: string; adj: string; bloc: string }>; rows: LookRow[]; legions: unknown[] };
    expect(saved()).toBe(before);
    expect(L.on).toBe(true);
    expect(L.viewer).toBe('UK');
    expect(L.turn).toBe(g.turn);
    expect(`${L.month} ${1939 + Math.floor((8 + g.turn - 1) / 12)}`).toMatch(/^[A-Z][a-z]+ \d{4}$/);
    expect(L.term).toBe(BARGAINS.term);
    expect(L.round).toBe(BARGAINS.round);
    expect(L.scale).toBe(BUDGET.scale); // (the card shows bids as the game shows money: 1.5 points read 150)
    // The rules' own rows, unchanged, in the rules' order.
    const rules = bargainsView(loadGame(before)!, 'UK');
    expect(L.rows.length).toBe(rules.length);
    expect(L.rows.length).toBeGreaterThan(0);
    L.rows.forEach((r, i) => {
      const { generic, capitalName, rel, agentSeller, agentBuyer, path, loss, cut, ...v } = r;
      expect(v).toEqual(rules[i]);
      expect(typeof generic).toBe('boolean');
      expect(capitalName).toBe(g.regions[r.capital].name);
      expect(['none', 'self', 'ally', 'enemy', 'other']).toContain(rel);
      if (!r.buyer) expect(rel).toBe('none');
      if (r.buyer === 'UK') expect(rel).toBe('self');
      if (r.buyer && atWar(g, 'UK', r.buyer)) expect(rel).toBe('enemy');
      expect(agentSeller).toBe(false);
      expect(agentBuyer).toBe(false);
      expect(path.length % 2).toBe(0);
      if (r.base >= 0 || !r.buyer || r.way === 'none') expect(path.length === 0 || !!cut).toBe(true);
      if (loss) expect(loss.sea).toBe(r.lossAt);
    });
    // A lane for every row that ships something, by land or by sea.
    expect(L.rows.filter((r) => r.buyer && r.base < 0 && r.way !== 'none').every((r) => r.path.length >= 4)).toBe(true);
    // Every country named in a row has its names and side.
    for (const r of L.rows) for (const t of [r.seller, r.buyer, ...(r.round?.bids ?? []).map((b) => b.nat)]) if (t) expect(L.nations[t]).toBeDefined();
    expect(L.nations.UK).toEqual({ name: 'Britain', the: 'Britain', adj: 'British', bloc: 'allies' });
    expect(L.nations.GER.bloc).toBe('axis');
    if (L.nations.USA) expect(L.nations.USA.the).toBe('the United States');
  });

  it('a bid is always one of the round\'s steps, and only the bidder\'s own figure changes', () => {
    // (A round opened by hand this month, as the rules' own tests open one: bargains-r2.test.ts.)
    const g = playing('UK', undefined, 3);
    const h = g.bargains!.rows['tur-chrome'] ?? (g.bargains!.rows['tur-chrome'] = { buyer: null, price: 0, since: g.turn, until: g.turn, way: 'none', seas: [], lost: 0, share: 0 });
    h.round = { opened: g.turn, bids: {} };
    TC().load(saveGame(g));
    const open = (look().rows as LookRow[]).find((r) => r.id === 'tur-chrome' && r.round && r.round.canBid === null && r.round.steps.length >= 2);
    expect(open).toBeDefined();
    expect(open!.round!.mine).toBeNull();
    const steps = bidSteps(loadGame(saved())!, 'UK');
    const out = TC().bargainBid(open!.id, steps[1] + 0.2) as { ok: boolean; note: string; view: { corps?: unknown[] } };
    expect(out.ok).toBe(true);
    expect(out.note).toContain('Your bid stands');
    expect(Array.isArray(out.view.corps)).toBe(true);
    const after = (look().rows as LookRow[]).find((r) => r.id === open!.id)!;
    expect(after.round!.mine).toBe(steps[1]);
    expect(loadGame(saved())!.bargains!.rows[open!.id].round!.bids.UK).toBe(steps[1]);
    expect((TC().bargainBid(open!.id, Number.NaN) as { ok: boolean }).ok).toBe(false);
    expect((TC().bargainBid('no-such-row', steps[0]) as { ok: boolean }).ok).toBe(false);
  });

  it('a seller in the player\'s seat sells to nobody, or is told why not', () => {
    const g = playing('SWE', undefined, 3);
    war(g, 'ITA', 'FRA');
    g.bargains!.rows['swe-ore'].round = { opened: g.turn - 1, bids: { GER: 2, ITA: 1.5 } };
    TC().load(saveGame(g));
    const mine = (look().rows as LookRow[]).filter((r) => r.seller === 'SWE' && r.round);
    expect(mine.length).toBeGreaterThan(0);
    const row = mine.find((r) => r.id === 'swe-ore')!;
    expect(row.round!.accept).toBe(true);
    expect(row.round!.bids).toEqual([{ nat: 'GER', offer: 2 }, { nat: 'ITA', offer: 1.5 }]);
    expect(row.agentSeller).toBe(false);
    const bad = TC().bargainSell(row.id, 'XXX') as { ok: boolean; note: string };
    expect(bad.ok).toBe(false);
    expect(bad.note).toBe('That country has made no bid.');
    const sold = TC().bargainSell(row.id, null) as { ok: boolean; note: string };
    expect(sold.ok).toBe(true);
    expect(sold.note).toContain('nobody');
    const h = loadGame(saved())!.bargains!.rows[row.id];
    expect(h.buyer).toBeNull();
    expect(h.round).toBeUndefined();
  });

  it('marks the agents the rules count for seeing a bid', () => {
    const g = playing('UK');
    const swe = g.nations.SWE.capital;
    g.intel!.agents.push({ id: 9901, nat: 'UK', name: 'Test', xp: 0, reg: swe, ready: g.turn, mission: 'watch', born: g.turn - 2 });
    const L = bargainsLookOf(g, 'UK');
    if (!L.on) throw new Error('off');
    for (const r of L.rows) {
      expect(r.agentSeller).toBe(r.seller === 'SWE');
      expect(r.agentBuyer).toBe(r.buyer === 'SWE');
    }
    // (an agent still on the road counts for nothing)
    g.intel!.agents[g.intel!.agents.length - 1].ready = g.turn + 1;
    const M = bargainsLookOf(g, 'UK');
    if (!M.on) throw new Error('off');
    expect(M.rows.some((r) => r.agentSeller)).toBe(false);
  });

  it('remembers a sea lane, and draws it cut at its sea of loss with the month it was first seen cut', () => {
    const g = playing('UK');
    const first = bargainsLookOf(g, 'UK');
    if (!first.on) throw new Error('off');
    const lane = first.rows.find((r) => r.way === 'sea' && r.seas.length >= 2 && r.buyer && r.path.length >= 4);
    expect(lane).toBeDefined();
    const h = g.bargains!.rows[lane!.id];
    // The rules cut it: no way, no seas, nothing arrives (bargains.ts refresh, with the way shut).
    h.way = 'none'; h.seas = []; h.lost = 1; h.share = 0;
    const cut = bargainsLookOf(g, 'UK');
    if (!cut.on) throw new Error('off');
    const row = cut.rows.find((r) => r.id === lane!.id)!;
    expect(row.state).toBe('cut');
    expect(row.path).toEqual(lane!.path);
    const mid = lane!.lossAt >= 0 ? lane!.lossAt : lane!.seas[Math.floor(lane!.seas.length / 2)];
    expect(row.cut).toEqual({ sea: mid, name: g.seas[mid].name, x: Math.round(g.seas[mid].x * 10) / 10, y: Math.round(g.seas[mid].y * 10) / 10, month: cut.month });
    expect(row.loss).toBeNull();
    // A month later it is still the month it was first seen cut.
    const month = cut.month;
    g.turn += 1;
    const later = bargainsLookOf(g, 'UK');
    if (!later.on) throw new Error('off');
    expect(later.rows.find((r) => r.id === lane!.id)!.cut!.month).toBe(month);
    g.turn -= 1;
    // Another buyer's cut lane is not this one: nothing is drawn.
    h.buyer = h.buyer === 'CAN' ? 'USA' : 'CAN';
    const other = bargainsLookOf(g, 'UK');
    if (!other.on) throw new Error('off');
    const o = other.rows.find((r) => r.id === lane!.id);
    if (o) { expect(o.path).toEqual([]); expect(o.cut).toBeNull(); }
    // A game loaded afresh has no memory: a cut lane is not drawn.
    const fresh = loadGame(saveGame(g))!;
    h.buyer = lane!.buyer;
    const f = bargainsLookOf(fresh, 'UK');
    if (!f.on) throw new Error('off');
    for (const r of f.rows) if (r.state === 'cut') { expect(r.path).toEqual([]); expect(r.cut).toBeNull(); }
  });

  it('the legions: home capital, place, strength, tour; and the legion\'s corps carries its home in the view', () => {
    const g = playing('UK', { legions: true });
    g.turn = 5;
    war(g, 'SOV', 'FIN');
    legionsMonth(g);
    g.turn = 6;
    legionsMonth(g);
    const L = bargainsLookOf(g, 'UK');
    if (!L.on) throw new Error('off');
    expect(L.rows).toEqual([]);
    const raw = legionsView(g).find((l) => l.home === 'SWE')!;
    const leg = L.legions.find((l) => l.home === 'SWE')!;
    expect(raw).toBeDefined();
    const { homeCapital, place, pct, tour, tourAt, ...rest } = leg;
    expect(rest).toEqual(raw);
    expect(homeCapital).toBe(g.nations.SWE.capital);
    expect(place).toBe(raw.region >= 0 ? g.regions[raw.region].name : '');
    expect(pct).toBe(Math.round((100 * raw.men) / (raw.est * MEN_PER_DIV)));
    expect(tour).toBe(raw.until - raw.since + 1);
    expect(tourAt).toBe(Math.max(1, Math.min(tour, tour - raw.monthsLeft + 1)));
    for (const t of ['SWE', 'FIN', 'SOV']) expect(L.nations[t]).toBeDefined();
    // The view's corps entry: 'vol' on a legion's corps, and on no other.
    const mine = g.corps.find((c) => c.nat === 'UK' && !c.dead && c.kind !== 'fort')!;
    mine.vol = 'IRL';
    TC().load(saveGame(g));
    const corps = (TC().view() as { corps: { id: number; vol?: string }[] }).corps;
    expect(corps.find((c) => c.id === mine.id)!.vol).toBe('IRL');
    expect(corps.filter((c) => c.vol !== undefined).every((c) => g.corps.find((x) => x.id === c.id)?.vol === c.vol)).toBe(true);
  });
});
